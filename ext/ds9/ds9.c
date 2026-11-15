#include <ds9.h>
#include <assert.h>

VALUE mDS9;
VALUE cDS9Session;
VALUE cDS9Client;
VALUE cDS9Server;
VALUE cDS9Callbacks;
VALUE eDS9Exception;
VALUE eDS9UninitializedException;

ID id_on_header,
   id_on_begin_headers,
   id_on_frame_not_send,
   id_on_begin_frame,
   id_on_invalid_frame_recv,
   id_send_event,
   id_recv_event,
   id_on_frame_send,
   id_on_frame_recv,
   id_on_stream_close,
   id_on_data_chunk_recv,
   id_on_data_source_read,
   id_before_frame_send,
   id_eof_p;

/*
 * nghttp2 hands this back to every callback as user_data. It, rather than
 * the Ruby object, is what nghttp2 holds on to, so that GC compaction can
 * move the object and update `self` here.
 */
typedef struct {
    nghttp2_session *session;
    VALUE self;
    /* Request bodies being sent, by stream id */
    VALUE bodies;
    /* How a callback's call into Ruby jumped out, to carry on with */
    int jump_state;
    /*
     * The fatal error nghttp2 returned, if it has. nghttp2 doesn't allow the
     * session to be used after one.
     */
    int fatal_error;
    /* Whether the session has its own on_stream_close */
    int has_on_stream_close;
} ds9_session_t;

static void ds9_session_mark(void *ptr)
{
    ds9_session_t *s = ptr;
    rb_gc_mark_movable(s->bodies);
}

static void ds9_session_free(void *ptr)
{
    ds9_session_t *s = ptr;
    nghttp2_session_del(s->session);
    xfree(s);
}

static void ds9_session_compact(void *ptr)
{
    ds9_session_t *s = ptr;
    s->self = rb_gc_location(s->self);
    s->bodies = rb_gc_location(s->bodies);
}

static const rb_data_type_t ds9_session_type = {
    "DS9/session",
    {ds9_session_mark, ds9_session_free, 0, ds9_session_compact,},
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY,
};

static ds9_session_t *get_session_struct(VALUE self)
{
    ds9_session_t *s;
    TypedData_Get_Struct(self, ds9_session_t, &ds9_session_type, s);
    return s;
}

static VALUE explode(int code);
static void ds9_resume_jump(ds9_session_t *s);

static nghttp2_session *get_session(VALUE self)
{
    ds9_session_t *s = get_session_struct(self);
    if (NULL == s->session)
	rb_raise(eDS9UninitializedException, "not initialized, call `super` from `initialize`");
    if (s->fatal_error)
	explode(s->fatal_error);
    return s->session;
}

/*
 * Call after each nghttp2 call that can run callbacks, with what it
 * returned.
 */
static void ds9_check_result(ds9_session_t *s, long rv)
{
    if (rv < 0 && nghttp2_is_fatal((int)rv))
	s->fatal_error = (int)rv;
    ds9_resume_jump(s);
}

static void * wrap_xmalloc(size_t size, void *mem_user_data)
{
    return xmalloc(size);
}

static void wrap_xfree(void *ptr, void *mem_user_data)
{
    xfree(ptr);
}

static void *wrap_xcalloc(size_t nmemb, size_t size, void *mem_user_data)
{
    return xcalloc(nmemb, size);
}

static void *wrap_xrealloc(void *ptr, size_t size, void *mem_user_data)
{
    return xrealloc(ptr, size);
}

static VALUE explode(int code) {
    return rb_funcall(eDS9Exception, rb_intern("abort"), 1, INT2NUM(code));
}

/*
 * Ruby code mustn't unwind through nghttp2's frames, so callbacks call into
 * Ruby under rb_protect. When that call raises (or throws, or is killed),
 * the callback records how it jumped and fails, and the jump is resumed
 * with ds9_resume_jump once nghttp2 has returned. Callbacks after a failed
 * one fail straight away, so nothing touches errinfo in between.
 */
static int ds9_protect(ds9_session_t *s, VALUE (*func)(VALUE), VALUE arg, VALUE *ret)
{
    int state;

    if (s->jump_state) return 0;

    *ret = rb_protect(func, arg, &state);
    if (state) {
	s->jump_state = state;
	return 0;
    }
    return 1;
}

static void ds9_resume_jump(ds9_session_t *s)
{
    int state = s->jump_state;

    if (state) {
	s->jump_state = 0;
	rb_jump_tag(state);
    }
}

struct ds9_funcall {
    VALUE recv;
    ID mid;
    int argc;
    const VALUE *argv;
};

static VALUE ds9_funcall_body(VALUE arg)
{
    struct ds9_funcall *c = (struct ds9_funcall *)arg;
    return rb_funcallv(c->recv, c->mid, c->argc, c->argv);
}

/*
 * Calls the session's `mid` for a callback that fails when it returns
 * false. Returns what nghttp2 expects from the callback.
 */
static int ds9_callback(void *user_data, ID mid, int argc, const VALUE *argv)
{
    ds9_session_t *s = user_data;
    struct ds9_funcall c = { s->self, mid, argc, argv };
    VALUE ret;

    if (!ds9_protect(s, ds9_funcall_body, (VALUE)&c, &ret))
	return NGHTTP2_ERR_CALLBACK_FAILURE;

    if (ret == Qfalse) return 1;

    return 0;
}

static int before_frame_send_callback(nghttp2_session *session,
				      const nghttp2_frame *frame,
				      void *user_data)
{
    VALUE argv[] = { WrapDS9Frame(frame) };
    return ds9_callback(user_data, id_before_frame_send, 1, argv);
}

struct ds9_send_args {
    VALUE self;
    const uint8_t *data;
    size_t length;
};

static VALUE send_body(VALUE arg)
{
    struct ds9_send_args *a = (struct ds9_send_args *)arg;
    VALUE ret = rb_funcall(a->self, id_send_event, 1, rb_str_new((const char *)a->data, a->length));

    return LONG2NUM(NUM2LONG(ret));
}

static ssize_t send_callback(nghttp2_session * session,
			     const uint8_t *data,
			     size_t length,
			     int flags, void *user_data)
{
    ds9_session_t *s = user_data;
    struct ds9_send_args a = { s->self, data, length };
    VALUE ret;

    if (!ds9_protect(s, send_body, (VALUE)&a, &ret))
	return NGHTTP2_ERR_CALLBACK_FAILURE;

    return NUM2LONG(ret);
}

static int on_frame_recv_callback(nghttp2_session *session,
				  const nghttp2_frame *frame,
				  void *user_data)
{
    VALUE argv[] = { WrapDS9Frame(frame) };
    return ds9_callback(user_data, id_on_frame_recv, 1, argv);
}

static int on_stream_close_callback(nghttp2_session *session,
				    int32_t stream_id,
				    uint32_t error_code,
				    void *user_data)
{
    ds9_session_t *s = user_data;
    VALUE argv[] = { INT2NUM(stream_id), UINT2NUM(error_code) };

    /* The stream may have closed before all of its body was sent */
    if (RHASH_SIZE(s->bodies) > 0)
	rb_hash_delete(s->bodies, argv[0]);

    if (!s->has_on_stream_close) return 0;

    return ds9_callback(user_data, id_on_stream_close, 2, argv);
}

static int on_header_callback(nghttp2_session *session,
			      const nghttp2_frame *frame,
			      const uint8_t *name, size_t namelen,
			      const uint8_t *value, size_t valuelen,
			      uint8_t flags, void *user_data)
{
    VALUE argv[] = {
	rb_usascii_str_new((const char *)name, namelen),
	rb_str_new((const char *)value, valuelen),
	WrapDS9Frame(frame),
	INT2NUM(flags),
    };
    return ds9_callback(user_data, id_on_header, 4, argv);
}

static int on_begin_headers_callback(nghttp2_session *session,
			             const nghttp2_frame *frame,
			             void *user_data)
{
    VALUE argv[] = { WrapDS9Frame(frame) };
    return ds9_callback(user_data, id_on_begin_headers, 1, argv);
}

/* Copies what a callback returned into nghttp2's buffer of `length` */
static long copy_to_buffer(VALUE str, uint8_t *buf, size_t length, const char *callback)
{
    long len;

    Check_Type(str, T_STRING);
    len = RSTRING_LEN(str);
    if ((size_t)len > length)
	rb_raise(rb_eArgError, "%s returned %ld bytes, more than the %"PRIuSIZE" asked for",
		callback, len, length);

    memcpy(buf, RSTRING_PTR(str), len);
    return len;
}

struct ds9_recv_args {
    VALUE self;
    uint8_t *buf;
    size_t length;
};

static VALUE recv_body(VALUE arg)
{
    struct ds9_recv_args *a = (struct ds9_recv_args *)arg;
    VALUE ret = rb_funcall(a->self, id_recv_event, 1, ULONG2NUM(a->length));

    if (FIXNUM_P(ret)) return ret;

    return LONG2NUM(copy_to_buffer(ret, a->buf, a->length, "recv_event"));
}

static ssize_t recv_callback(nghttp2_session *session, uint8_t *buf,
			     size_t length, int flags,
			     void *user_data)
{
    ds9_session_t *s = user_data;
    struct ds9_recv_args a = { s->self, buf, length };
    VALUE ret;

    if (!ds9_protect(s, recv_body, (VALUE)&a, &ret))
	return NGHTTP2_ERR_CALLBACK_FAILURE;

    return FIX2LONG(ret);
}

static int on_begin_frame_callback(nghttp2_session *session,
				   const nghttp2_frame_hd *hd,
				   void *user_data)
{
    VALUE argv[] = { WrapDS9FrameHeader(hd) };
    return ds9_callback(user_data, id_on_begin_frame, 1, argv);
}

static int on_data_chunk_recv_callback(nghttp2_session *session,
				       uint8_t flags,
				       int32_t stream_id,
				       const uint8_t *data,
				       size_t len, void *user_data)
{
    VALUE argv[] = {
	INT2NUM(stream_id),
	rb_str_new((const char *)data, len),
	INT2NUM(flags),
    };
    return ds9_callback(user_data, id_on_data_chunk_recv, 3, argv);
}

static int on_invalid_frame_recv_callback(nghttp2_session *session,
					  const nghttp2_frame *frame, int lib_error_code,
					  void *user_data)
{
    VALUE argv[] = { WrapDS9Frame(frame), INT2NUM(lib_error_code) };
    return ds9_callback(user_data, id_on_invalid_frame_recv, 2, argv);
}

static int on_frame_send_callback(nghttp2_session *session,
				  const nghttp2_frame *frame,
				  void *user_data)
{
    VALUE argv[] = { WrapDS9Frame(frame) };
    return ds9_callback(user_data, id_on_frame_send, 1, argv);
}

static int on_frame_not_send_callback(nghttp2_session *session,
				      const nghttp2_frame *frame,
				      int lib_error_code,
				      void *user_data)
{
    VALUE argv[] = { WrapDS9Frame(frame), rb_str_new2(nghttp2_strerror(lib_error_code)) };
    return ds9_callback(user_data, id_on_frame_not_send, 2, argv);
}

struct ds9_read_args {
    ds9_session_t *s;
    int32_t stream_id;
    uint8_t *buf;
    size_t length;
    uint32_t *data_flags;
};

static VALUE data_read_body(VALUE arg)
{
    struct ds9_read_args *a = (struct ds9_read_args *)arg;
    VALUE ret;

    ret = rb_funcall(a->s->self, id_on_data_source_read, 2, INT2NUM(a->stream_id),
	    ULONG2NUM(a->length));

    if (NIL_P(ret)) {
	*a->data_flags |= NGHTTP2_DATA_FLAG_EOF;
	return INT2FIX(0);
    }

    if (ret == Qfalse) {
	*a->data_flags |= NGHTTP2_DATA_FLAG_EOF;
	*a->data_flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
	return INT2FIX(0);
    }

    if (FIXNUM_P(ret) && FIX2LONG(ret) == NGHTTP2_ERR_DEFERRED) {
	return ret;
    }

    return LONG2NUM(copy_to_buffer(ret, a->buf, a->length, "on_data_source_read"));
}

static ssize_t rb_data_read_callback(nghttp2_session *session,
				     int32_t stream_id, uint8_t *buf,
				     size_t length, uint32_t *data_flags,
				     nghttp2_data_source *source, void *user_data)
{
    struct ds9_read_args a = { user_data, stream_id, buf, length, data_flags };
    VALUE ret;

    if (!ds9_protect(user_data, data_read_body, (VALUE)&a, &ret))
	return NGHTTP2_ERR_CALLBACK_FAILURE;

    return FIX2LONG(ret);
}

static int header_strings_i(VALUE name, VALUE value, VALUE strs)
{
    StringValue(name);
    StringValue(value);
    rb_ary_push(strs, name);
    rb_ary_push(strs, value);
    return ST_CONTINUE;
}

/* An Array of [name, value] pairs, or a Hash, as names and values */
static VALUE header_strings(VALUE headers)
{
    VALUE strs;
    long i;

    switch(TYPE(headers))
    {
	case T_ARRAY:
	    strs = rb_ary_new_capa(RARRAY_LEN(headers) * 2);
	    for (i = 0; i < RARRAY_LEN(headers); i++) {
		VALUE tuple = rb_ary_entry(headers, i);
		header_strings_i(rb_ary_entry(tuple, 0), rb_ary_entry(tuple, 1), strs);
	    }
	    return strs;
	case T_HASH:
	    strs = rb_ary_new_capa(RHASH_SIZE(headers) * 2);
	    rb_hash_foreach(headers, header_strings_i, strs);
	    return strs;
	default:
	    Check_Type(headers, T_ARRAY);
	    UNREACHABLE_RETURN(Qnil);
    }
}

/*
 * Converts an Array of [name, value] pairs, or a Hash, of headers to the
 * nghttp2_nv array nghttp2 takes. The names and values are copied in after
 * it, rather than pointed to where they are in their Strings: nghttp2
 * allocates before copying them, which can run GC, and compaction can move
 * a String's contents. Free it with ALLOCV_END(*buf).
 */
static nghttp2_nv *headers_to_nv(VALUE headers, size_t *niv, VALUE *buf)
{
    VALUE strs = header_strings(headers);
    long nstrs = RARRAY_LEN(strs), i;
    size_t n = nstrs / 2, total = 0;
    nghttp2_nv *nva;
    uint8_t *bytes;

    for (i = 0; i < nstrs; i++)
	total += RSTRING_LEN(RARRAY_AREF(strs, i));

    /* Allocated before any pointers into the Strings are taken */
    nva = rb_alloc_tmp_buffer(buf, n * sizeof(nghttp2_nv) + total);
    bytes = (uint8_t *)(nva + n);

    for (i = 0; i < (long)n; i++) {
	VALUE name = RARRAY_AREF(strs, i * 2);
	VALUE value = RARRAY_AREF(strs, i * 2 + 1);
	nghttp2_nv *nv = &nva[i];

	nv->name = bytes;
	nv->namelen = RSTRING_LEN(name);
	memcpy(bytes, RSTRING_PTR(name), nv->namelen);
	bytes += nv->namelen;

	nv->value = bytes;
	nv->valuelen = RSTRING_LEN(value);
	memcpy(bytes, RSTRING_PTR(value), nv->valuelen);
	bytes += nv->valuelen;

	nv->flags = NGHTTP2_NV_FLAG_NONE;
    }

    RB_GC_GUARD(strs);
    *niv = n;
    return nva;
}

static VALUE allocate_session(VALUE klass)
{
    ds9_session_t *s;
    VALUE obj = TypedData_Make_Struct(klass, ds9_session_t, &ds9_session_type, s);
    s->self = obj;
    s->bodies = rb_hash_new();
    return obj;
}

static VALUE client_init_internals(VALUE self, VALUE cb, VALUE opt)
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_session *session;
    nghttp2_option *option;
    nghttp2_mem mem = {
	NULL,
	wrap_xmalloc,
	wrap_xfree,
	wrap_xcalloc,
	wrap_xrealloc
    };

    TypedData_Get_Struct(cb, nghttp2_session_callbacks, &ds9_callbacks_type, callbacks);
    option = UnwrapDS9Option(opt);
    nghttp2_session_client_new3(&session, callbacks, get_session_struct(self), option, &mem);
    get_session_struct(self)->session = session;

    return self;
}

static VALUE server_init_internals(VALUE self, VALUE cb, VALUE opt)
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_session *session;
    nghttp2_option *option;
    nghttp2_mem mem = {
	NULL,
	wrap_xmalloc,
	wrap_xfree,
	wrap_xcalloc,
	wrap_xrealloc
    };

    TypedData_Get_Struct(cb, nghttp2_session_callbacks, &ds9_callbacks_type, callbacks);
    option = UnwrapDS9Option(opt);
    nghttp2_session_server_new3(&session, callbacks, get_session_struct(self), option, &mem);
    get_session_struct(self)->session = session;

    return self;
}

static VALUE session_want_write_p(VALUE self)
{
    nghttp2_session *session;

    /* A session that has failed is finished with */
    if (get_session_struct(self)->fatal_error) return Qfalse;
    session = get_session(self);

    if (nghttp2_session_want_write(session) == 0) {
	return Qfalse;
    }

    return Qtrue;
}

static VALUE session_want_read_p(VALUE self)
{
    nghttp2_session *session;

    /* A session that has failed is finished with */
    if (get_session_struct(self)->fatal_error) return Qfalse;
    session = get_session(self);

    if (nghttp2_session_want_read(session) == 0) {
	return Qfalse;
    }

    return Qtrue;
}

static VALUE session_submit_ping(VALUE self)
{
    nghttp2_session *session;

    session = get_session(self);

    if (nghttp2_submit_ping(session, NGHTTP2_FLAG_NONE, NULL) == 0)
	return Qtrue;

    return Qfalse;
}

static VALUE session_submit_settings(VALUE self, VALUE settings)
{
    size_t niv, i;
    nghttp2_settings_entry *iv, *head;
    VALUE iv_buf;
    nghttp2_session *session;
    int rv;

    session = get_session(self);

    Check_Type(settings, T_ARRAY);
    niv = RARRAY_LEN(settings);
    iv = ALLOCV_N(nghttp2_settings_entry, iv_buf, niv);
    head = iv;

    for(i = 0; i < niv; i++, head++) {
	VALUE tuple = rb_ary_entry(settings, (long)i);
	head->settings_id = NUM2INT(rb_ary_entry(tuple, 0));
	head->value = NUM2UINT(rb_ary_entry(tuple, 1));
    }

    rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, niv);

    ALLOCV_END(iv_buf);

    if (0 != rv) {
	explode(rv);
    }

    return self;
}

static VALUE session_submit_trailer(VALUE self, VALUE stream_id, VALUE trailers)
{
    size_t niv;
    nghttp2_nv *nva;
    nghttp2_session *session;
    int rv;
    int32_t s_id;
    VALUE nva_buf;

    session = get_session(self);

    s_id = NUM2INT(stream_id);

    nva = headers_to_nv(trailers, &niv, &nva_buf);

    rv = nghttp2_submit_trailer(session, s_id, nva, niv);

    ALLOCV_END(nva_buf);

    if (0 != rv) {
	explode(rv);
    }

    return self;
}

static VALUE session_send(VALUE self)
{
    int rv;
    nghttp2_session *session;

    session = get_session(self);

    rv = nghttp2_session_send(session);
    ds9_check_result(get_session_struct(self), rv);
    if (rv != 0) {
	explode(rv);
    }

    return self;
}

static VALUE session_receive(VALUE self)
{
    int rv;
    nghttp2_session *session;

    session = get_session(self);

    assert(session);
    rv = nghttp2_session_recv(session);
    ds9_check_result(get_session_struct(self), rv);
    if (rv != 0) {
	explode(rv);
    }

    return self;
}

static VALUE session_mem_receive(VALUE self, VALUE buf)
{
    ssize_t rv;
    nghttp2_session *session;

    session = get_session(self);

    StringValue(buf);

    /* nghttp2 reads straight from buf, so callbacks mustn't change it */
    rb_str_locktmp(buf);
    rv = nghttp2_session_mem_recv(session, (const uint8_t *)RSTRING_PTR(buf), RSTRING_LEN(buf));
    rb_str_unlocktmp(buf);
    ds9_check_result(get_session_struct(self), rv);
    if (rv < 0) {
	explode((int)rv);
    }

    return ULONG2NUM(rv);
}

static VALUE session_mem_send(VALUE self)
{
    ssize_t rv;
    const uint8_t *data;
    nghttp2_session *session;

    session = get_session(self);

    rv = nghttp2_session_mem_send(session, &data);
    ds9_check_result(get_session_struct(self), rv);

    if (rv == 0) {
	return Qfalse;
    }

    if (rv < 0) {
	explode((int)rv);
    }

    return rb_str_new((const char *)data, rv);
}

static VALUE session_outbound_queue_size(VALUE self)
{
    nghttp2_session *session;

    session = get_session(self);

    return ULONG2NUM(nghttp2_session_get_outbound_queue_size(session));
}

static VALUE ruby_read_body(VALUE arg)
{
    struct ds9_read_args *a = (struct ds9_read_args *)arg;
    VALUE body = rb_hash_aref(a->s->bodies, INT2NUM(a->stream_id));
    VALUE ret = rb_funcall(body, rb_intern("read"), 1, ULONG2NUM(a->length));

    if (FIXNUM_P(ret) && FIX2LONG(ret) == NGHTTP2_ERR_DEFERRED) {
	return ret;
    }

    if (NIL_P(ret) || (rb_respond_to(body, id_eof_p) && RTEST(rb_funcall(body, id_eof_p, 0)))) {
	rb_hash_delete(a->s->bodies, INT2NUM(a->stream_id));
	*a->data_flags |= NGHTTP2_DATA_FLAG_EOF;
    }

    if (NIL_P(ret)) {
	return INT2FIX(0);
    }

    return LONG2NUM(copy_to_buffer(ret, a->buf, a->length, "request body's read"));
}

static ssize_t
ruby_read(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length,
    uint32_t *data_flags, nghttp2_data_source *source, void *user_data)
{
    struct ds9_read_args a = { user_data, stream_id, buf, length, data_flags };
    VALUE ret;

    if (!ds9_protect(user_data, ruby_read_body, (VALUE)&a, &ret))
	return NGHTTP2_ERR_CALLBACK_FAILURE;

    return FIX2LONG(ret);
}

static VALUE file_read_body(VALUE arg)
{
    struct ds9_read_args *a = (struct ds9_read_args *)arg;
    ssize_t nread;
    VALUE io = rb_hash_aref(a->s->bodies, INT2NUM(a->stream_id));
    int fd;
#ifdef HAVE_RB_IO_DESCRIPTOR
    fd = rb_io_descriptor(io);
#else
    rb_io_t *fptr;
    GetOpenFile(io, fptr);
    rb_io_check_readable(fptr);
    fd = fptr->fd;
#endif

    nread = read(fd, a->buf, a->length);

    if (nread == -1) {
	return INT2FIX(NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE);
    }

    if (nread == 0) {
	*a->data_flags |= NGHTTP2_DATA_FLAG_EOF;
	rb_hash_delete(a->s->bodies, INT2NUM(a->stream_id));
    }
    return LONG2NUM(nread);
}

static ssize_t
file_read(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length,
    uint32_t *data_flags, nghttp2_data_source *source, void *user_data)
{
    struct ds9_read_args a = { user_data, stream_id, buf, length, data_flags };
    VALUE ret;

    if (!ds9_protect(user_data, file_read_body, (VALUE)&a, &ret))
	return NGHTTP2_ERR_CALLBACK_FAILURE;

    return FIX2LONG(ret);
}

static VALUE session_submit_request(VALUE self, VALUE settings, VALUE body)
{
    size_t niv;
    nghttp2_nv *nva;
    nghttp2_session *session;
    nghttp2_data_provider provider;
    int rv;
    VALUE nva_buf;

    session = get_session(self);

    nva = headers_to_nv(settings, &niv, &nva_buf);

    if (NIL_P(body)) {
	rv = nghttp2_submit_request(session, NULL, nva, niv, NULL, NULL);
    } else {
	/* The callbacks find the body in the session's bodies by stream id */
	provider.source.ptr = NULL;
	if (TYPE(body) == T_FILE) {
	    /* Treat as a file descriptor */
	    provider.read_callback = file_read;
	} else {
	    provider.read_callback = ruby_read;
	}

	rv = nghttp2_submit_request(session, NULL, nva, niv, &provider, NULL);
    }

    ALLOCV_END(nva_buf);

    if (rv < 0) {
	explode(rv);
    }

    if(!NIL_P(body)) {
	rb_hash_aset(get_session_struct(self)->bodies, INT2NUM(rv), body);
    }

    return INT2NUM(rv);
}

static VALUE session_submit_shutdown(VALUE self)
{
    int rv;
    nghttp2_session *session;

    session = get_session(self);

    rv = nghttp2_submit_shutdown_notice(session);

    if (rv == 0)
	return Qtrue;

    return explode(rv);
}

static VALUE session_get_last_proc_stream_id(VALUE self)
{
    int rv;
    nghttp2_session *session;

    session = get_session(self);

    rv = nghttp2_session_get_last_proc_stream_id(session);

    return INT2NUM(rv);
}

static VALUE session_submit_goaway(VALUE self, VALUE last_stream_id, VALUE err)
{
    int rv;
    nghttp2_session *session;

    session = get_session(self);

    rv = nghttp2_submit_goaway(session, NGHTTP2_FLAG_NONE,
	    NUM2INT(last_stream_id), NUM2INT(err), NULL, 0);

    if (rv == 0)
	return Qtrue;

    return explode(rv);
}

static VALUE session_stream_local_closed_p(VALUE self, VALUE streamid)
{
    nghttp2_session *session;
    int stream_id;

    session = get_session(self);

    stream_id = NUM2INT(streamid);

    switch(nghttp2_session_get_stream_local_close(session, stream_id))
    {
	case 1:
	    return Qtrue;
	    break;
	case 0:
	    return Qfalse;
	    break;
	default:
	    rb_raise(rb_eStandardError, "no such stream: %d", stream_id);
    }

    return Qfalse;
}

static VALUE session_stream_remote_closed_p(VALUE self, VALUE streamid)
{
    nghttp2_session *session;
    int stream_id;

    session = get_session(self);

    stream_id = NUM2INT(streamid);

    switch(nghttp2_session_get_stream_remote_close(session, stream_id))
    {
	case 1:
	    return Qtrue;
	    break;
	case 0:
	    return Qfalse;
	    break;
	default:
	    rb_raise(rb_eStandardError, "no such stream: %d", stream_id);
    }

    return Qfalse;
}

static VALUE session_resume_data(VALUE self, VALUE streamid)
{
    nghttp2_session *session;
    int stream_id;
    int rv;

    session = get_session(self);

    stream_id = NUM2INT(streamid);
    rv = nghttp2_session_resume_data(session, stream_id);

    if (rv == 0)
      return Qtrue;

    return explode(rv);
}

static VALUE session_terminate_session(VALUE self, VALUE err)
{
    int rv;
    nghttp2_session *session;

    session = get_session(self);

    rv = nghttp2_session_terminate_session(session, NUM2INT(err));

    if (rv != 0) {
	explode(rv);
    }

    return self;
}

static VALUE server_submit_response(VALUE self, VALUE stream_id, VALUE headers)
{
    nghttp2_session *session;
    size_t niv;
    nghttp2_nv *nva;
    nghttp2_data_provider provider;
    int rv;
    VALUE nva_buf;

    session = get_session(self);

    nva = headers_to_nv(headers, &niv, &nva_buf);

    provider.read_callback = rb_data_read_callback;

    rv = nghttp2_submit_response(session, NUM2INT(stream_id), nva, niv, &provider);

    ALLOCV_END(nva_buf);

    if (0 != rv) {
	explode(rv);
    }

    return self;
}

static VALUE server_submit_headers(VALUE self, VALUE stream_id, VALUE headers) {
    nghttp2_session *session;
    size_t niv;
    nghttp2_nv *nva;
    int rv;
    VALUE nva_buf;

    session = get_session(self);

    nva = headers_to_nv(headers, &niv, &nva_buf);

    /* nghttp2_submit_headers is too low level API for ds9 */
    rv = nghttp2_submit_response(session, NUM2INT(stream_id), nva, niv, NULL);

    ALLOCV_END(nva_buf);

    if (0 != rv) {
	explode(rv);
    }

    return self;
}

static VALUE make_callbacks(VALUE self)
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_session_callbacks_new(&callbacks);

    if (rb_obj_respond_to(self, id_on_header, 1))
	nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);

    if (rb_obj_respond_to(self, id_on_begin_headers, 1))
	nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, on_begin_headers_callback);

    if (rb_obj_respond_to(self, id_on_frame_not_send, 1))
	nghttp2_session_callbacks_set_on_frame_not_send_callback(callbacks, on_frame_not_send_callback);

    if (rb_obj_respond_to(self, id_on_begin_frame, 1))
	nghttp2_session_callbacks_set_on_begin_frame_callback(callbacks, on_begin_frame_callback);

    if (rb_obj_respond_to(self, id_on_invalid_frame_recv, 1))
	nghttp2_session_callbacks_set_on_invalid_frame_recv_callback(callbacks, on_invalid_frame_recv_callback);

    if (rb_obj_respond_to(self, id_send_event, 1))
	nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);

    if (rb_obj_respond_to(self, id_recv_event, 1))
	nghttp2_session_callbacks_set_recv_callback(callbacks, recv_callback);

    if (rb_obj_respond_to(self, id_on_frame_send, 1))
	nghttp2_session_callbacks_set_on_frame_send_callback(callbacks, on_frame_send_callback);

    if (rb_obj_respond_to(self, id_on_frame_recv, 1))
	nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);

    /* Always set, to let go of request bodies */
    get_session_struct(self)->has_on_stream_close = rb_obj_respond_to(self, id_on_stream_close, 1);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);

    if (rb_obj_respond_to(self, id_on_data_chunk_recv, 1))
	nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_callback);

    if (rb_obj_respond_to(self, id_before_frame_send, 1))
	nghttp2_session_callbacks_set_before_frame_send_callback(callbacks, before_frame_send_callback);

    return TypedData_Wrap_Struct(cDS9Callbacks, &ds9_callbacks_type, callbacks);
}

static VALUE server_submit_push_promise(VALUE self, VALUE stream_id, VALUE headers)
{
    nghttp2_session *session;
    nghttp2_nv *nva;
    VALUE nva_buf;
    size_t niv;
    int rv;

    session = get_session(self);

    Check_Type(headers, T_ARRAY);
    nva = headers_to_nv(headers, &niv, &nva_buf);

    rv = nghttp2_submit_push_promise(session, 0, NUM2INT(stream_id), nva, niv, NULL);

    ALLOCV_END(nva_buf);

    switch(rv) {
	case NGHTTP2_ERR_NOMEM:
	case NGHTTP2_ERR_PROTO:
	case NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE:
	case NGHTTP2_ERR_INVALID_ARGUMENT:
	    return explode(rv);
	    break;
	default:
	    return INT2NUM(rv);
    }
}

static VALUE rb_nghttp_version(VALUE klass)
{
    nghttp2_info * info = nghttp2_version(0);

    return rb_usascii_str_new2(info->version_str);
}

static VALUE errors_to_string(VALUE mod, VALUE err)
{
    return rb_usascii_str_new2(nghttp2_strerror(NUM2INT(err)));
}

void Init_ds9(void)
{
    mDS9 = rb_define_module("DS9");

    Init_ds9_frames(mDS9);
    Init_ds9_option(mDS9);

    rb_define_const(mDS9, "PROTO_VERSION_ID", rb_str_new(NGHTTP2_PROTO_VERSION_ID, NGHTTP2_PROTO_VERSION_ID_LEN));

    eDS9Exception = rb_define_class_under(mDS9, "Exception", rb_eStandardError);
    eDS9UninitializedException = rb_define_class_under(mDS9, "UninitializedException", rb_eStandardError);

    VALUE mDS9Settings = rb_define_module_under(mDS9, "Settings");
    rb_define_const(mDS9Settings, "MAX_CONCURRENT_STREAMS", INT2NUM(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS));
    rb_define_const(mDS9Settings, "INITIAL_WINDOW_SIZE", INT2NUM(NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE));
    rb_define_const(mDS9Settings, "HEADER_TABLE_SIZE", INT2NUM(NGHTTP2_SETTINGS_HEADER_TABLE_SIZE));
    rb_define_const(mDS9Settings, "ENABLE_PUSH", INT2NUM(NGHTTP2_SETTINGS_ENABLE_PUSH));
    rb_define_const(mDS9Settings, "MAX_FRAME_SIZE", INT2NUM(NGHTTP2_SETTINGS_MAX_FRAME_SIZE));
    rb_define_const(mDS9Settings, "MAX_HEADER_LIST_SIZE", INT2NUM(NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE));

    rb_define_const(mDS9, "ERR_WOULDBLOCK", INT2NUM(NGHTTP2_ERR_WOULDBLOCK));
    rb_define_const(mDS9, "ERR_EOF", INT2NUM(NGHTTP2_ERR_EOF));
    rb_define_const(mDS9, "ERR_DEFERRED", INT2NUM(NGHTTP2_ERR_DEFERRED));
    rb_define_const(mDS9, "ERR_BAD_CLIENT_MAGIC", INT2NUM(NGHTTP2_ERR_BAD_CLIENT_MAGIC));
    rb_define_const(mDS9, "NO_ERROR", INT2NUM(NGHTTP2_NO_ERROR));
    rb_define_const(mDS9, "DEFAULT_WEIGHT", INT2NUM(NGHTTP2_DEFAULT_WEIGHT));
    rb_define_const(mDS9, "MAX_WEIGHT", INT2NUM(NGHTTP2_MAX_WEIGHT));
    rb_define_const(mDS9, "MIN_WEIGHT", INT2NUM(NGHTTP2_MIN_WEIGHT));
    rb_define_const(mDS9, "MAX_WINDOW_SIZE", INT2NUM(NGHTTP2_MAX_WINDOW_SIZE));
    rb_define_const(mDS9, "INITIAL_WINDOW_SIZE", INT2NUM(NGHTTP2_INITIAL_WINDOW_SIZE));
    rb_define_const(mDS9, "INITIAL_CONNECTION_WINDOW_SIZE", INT2NUM(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE));
    rb_define_const(mDS9, "DEFAULT_HEADER_TABLE_SIZE", INT2NUM(NGHTTP2_DEFAULT_HEADER_TABLE_SIZE));
    rb_define_const(mDS9, "CLIENT_MAGIC", rb_str_new(NGHTTP2_CLIENT_MAGIC, NGHTTP2_CLIENT_MAGIC_LEN));

    rb_define_singleton_method(mDS9, "nghttp_version", rb_nghttp_version, 0);

    cDS9Callbacks = rb_define_class_under(mDS9, "Callbacks", rb_cObject);
    rb_undef_alloc_func(cDS9Callbacks);

    cDS9Session = rb_define_class_under(mDS9, "Session", rb_cObject);
    cDS9Client = rb_define_class_under(mDS9, "Client", cDS9Session);
    cDS9Server = rb_define_class_under(mDS9, "Server", cDS9Session);

    rb_define_alloc_func(cDS9Session, allocate_session);

    rb_define_method(cDS9Session, "want_write?", session_want_write_p, 0);
    rb_define_method(cDS9Session, "want_read?", session_want_read_p, 0);
    rb_define_method(cDS9Session, "submit_settings", session_submit_settings, 1);
    rb_define_method(cDS9Session, "submit_ping", session_submit_ping, 0);
    rb_define_method(cDS9Session, "submit_goaway", session_submit_goaway, 2);
    rb_define_method(cDS9Session, "send", session_send, 0);
    rb_define_method(cDS9Session, "receive", session_receive, 0);
    rb_define_method(cDS9Session, "mem_receive", session_mem_receive, 1);
    rb_define_method(cDS9Session, "mem_send", session_mem_send, 0);
    rb_define_method(cDS9Session, "outbound_queue_size", session_outbound_queue_size, 0);
    rb_define_method(cDS9Session, "terminate_session", session_terminate_session, 1);
    rb_define_method(cDS9Session, "stream_local_closed?", session_stream_local_closed_p, 1);
    rb_define_method(cDS9Session, "stream_remote_closed?", session_stream_remote_closed_p, 1);
    rb_define_method(cDS9Session, "resume_data", session_resume_data, 1);
    rb_define_method(cDS9Session, "last_proc_stream_id", session_get_last_proc_stream_id, 0);

    rb_define_private_method(cDS9Session, "submit_request", session_submit_request, 2);
    rb_define_private_method(cDS9Session, "make_callbacks", make_callbacks, 0);
    rb_define_private_method(cDS9Client, "init_internals", client_init_internals, 2);
    rb_define_private_method(cDS9Server, "init_internals", server_init_internals, 2);

    rb_define_method(cDS9Server, "submit_response", server_submit_response, 2);
    rb_define_method(cDS9Server, "submit_push_promise", server_submit_push_promise, 2);
    rb_define_method(cDS9Server, "submit_shutdown", session_submit_shutdown, 0);
    rb_define_method(cDS9Server, "submit_trailer", session_submit_trailer, 2);
    rb_define_method(cDS9Server, "submit_headers", server_submit_headers, 2);

    rb_define_singleton_method(eDS9Exception, "to_string", errors_to_string, 1);

    id_on_header             = rb_intern("on_header");
    id_on_begin_headers      = rb_intern("on_begin_headers");
    id_on_frame_not_send     = rb_intern("on_frame_not_send");
    id_on_begin_frame        = rb_intern("on_begin_frame");
    id_on_invalid_frame_recv = rb_intern("on_invalid_frame_recv");
    id_send_event            = rb_intern("send_event");
    id_recv_event            = rb_intern("recv_event");
    id_on_frame_send         = rb_intern("on_frame_send");
    id_on_frame_recv         = rb_intern("on_frame_recv");
    id_on_stream_close       = rb_intern("on_stream_close");
    id_on_data_chunk_recv    = rb_intern("on_data_chunk_recv");
    id_before_frame_send     = rb_intern("before_frame_send");
    id_on_data_source_read   = rb_intern("on_data_source_read");
    id_eof_p                 = rb_intern("eof?");
}

/* vim: set noet sws=4 sw=4: */
