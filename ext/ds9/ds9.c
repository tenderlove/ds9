#include <ds9.h>
#include <assert.h>

VALUE mDS9;
VALUE cDS9Session;
VALUE cDS9Client;
VALUE cDS9Server;
VALUE cDS9Callbacks;
VALUE eDS9Exception;

#define CheckSelf(ptr) \
    if (NULL == (ptr)) \
      rb_raise(eDS9Exception, "not initialized, call `super` from `initialize`");

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

static int before_frame_send_callback(nghttp2_session *session,
				      const nghttp2_frame *frame,
				      void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("before_frame_send"), 1, WrapDS9Frame(frame));

    if (ret == Qfalse) return 1;

    return 0;
}

static ssize_t send_callback(nghttp2_session * session,
			     const uint8_t *data,
			     size_t length,
			     int flags, void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("send_event"), 1, rb_str_new((const char *)data, length));

    return NUM2INT(ret);
}

static int on_frame_recv_callback(nghttp2_session *session,
				  const nghttp2_frame *frame,
				  void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("on_frame_recv"), 1, WrapDS9Frame(frame));

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static int on_stream_close_callback(nghttp2_session *session,
				    int32_t stream_id,
				    uint32_t error_code,
				    void *user_data)
{
    VALUE self = (VALUE)user_data;

    VALUE ret = rb_funcall(self, rb_intern("on_stream_close"), 2, INT2NUM(stream_id), INT2NUM(error_code));

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static int on_header_callback(nghttp2_session *session,
			      const nghttp2_frame *frame,
			      const uint8_t *name, size_t namelen,
			      const uint8_t *value, size_t valuelen,
			      uint8_t flags, void *user_data)
{
    VALUE self = (VALUE)user_data;

    VALUE ret = rb_funcall(self, rb_intern("on_header"), 4,
	    rb_usascii_str_new((const char *)name, namelen),
	    rb_usascii_str_new((const char *)value, valuelen),
	    WrapDS9Frame(frame),
	    INT2NUM(flags));

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static int on_begin_headers_callback(nghttp2_session *session,
			             const nghttp2_frame *frame,
			             void *user_data)
{
    VALUE self = (VALUE)user_data;

    VALUE ret = rb_funcall(self, rb_intern("on_begin_headers"), 1,
	    WrapDS9Frame(frame));

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static ssize_t recv_callback(nghttp2_session *session, uint8_t *buf,
			     size_t length, int flags,
			     void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;
    ssize_t len;

    ret = rb_funcall(self, rb_intern("recv_event"), 1, INT2NUM(length));

    if (FIXNUM_P(ret)) {
	return NUM2INT(ret);
    }

    Check_Type(ret, T_STRING);
    len = RSTRING_LEN(ret);

    memcpy(buf, StringValuePtr(ret), len);

    return len;
}

static int on_begin_frame_callback(nghttp2_session *session,
				   const nghttp2_frame_hd *hd,
				   void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("on_begin_frame"), 1, WrapDS9FrameHeader(hd));

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static int on_data_chunk_recv_callback(nghttp2_session *session,
				       uint8_t flags,
				       int32_t stream_id,
				       const uint8_t *data,
				       size_t len, void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("on_data_chunk_recv"), 3,
	    INT2NUM(stream_id),
	    rb_str_new((const char *)data, len),
	    INT2NUM(flags));

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static int on_invalid_frame_recv_callback(nghttp2_session *session,
					  const nghttp2_frame *frame, int lib_error_code,
					  void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("on_invalid_frame_recv"), 2,
	    WrapDS9Frame(frame),
	    INT2NUM(lib_error_code));

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static int on_frame_send_callback(nghttp2_session *session,
				  const nghttp2_frame *frame,
				  void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("on_frame_send"), 1, WrapDS9Frame(frame));

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static int on_frame_not_send_callback(nghttp2_session *session,
				      const nghttp2_frame *frame,
				      int lib_error_code,
				      void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE reason = rb_str_new2(nghttp2_strerror(lib_error_code));
    VALUE ret;

    ret = rb_funcall(self, rb_intern("on_frame_not_send"), 2, WrapDS9Frame(frame), reason);

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static ssize_t rb_data_read_callback(nghttp2_session *session,
				     int32_t stream_id, uint8_t *buf,
				     size_t length, uint32_t *data_flags,
				     nghttp2_data_source *source, void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;
    ssize_t len;

    ret = rb_funcall(self, rb_intern("on_data_source_read"), 2, INT2NUM(stream_id),
	    INT2NUM(length));

    if (NIL_P(ret)) {
	*data_flags |= NGHTTP2_DATA_FLAG_EOF;
	return 0;
    }

    Check_Type(ret, T_STRING);
    len = RSTRING_LEN(ret);
    memcpy(buf, StringValuePtr(ret), len);

    return len;
}

static void copy_list_to_nv(VALUE list, nghttp2_nv * head, size_t niv)
{
    size_t i;

    for(i = 0; i < niv; i++, head++) {
	VALUE tuple = rb_ary_entry(list, (long)i);
	VALUE name = rb_ary_entry(tuple, 0);
	VALUE value = rb_ary_entry(tuple, 1);

	head->name = (uint8_t *)StringValuePtr(name);
	head->namelen = RSTRING_LEN(name);

	head->value = (uint8_t *)StringValuePtr(value);
	head->valuelen = RSTRING_LEN(value);
	head->flags = NGHTTP2_NV_FLAG_NONE;
    }
}

static VALUE allocate_session(VALUE klass)
{
    return TypedData_Wrap_Struct(klass, &ds9_session_type, 0);
}

static VALUE client_init_internals(VALUE self, VALUE cb)
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_session *session;
    nghttp2_mem mem = {
	NULL,
	wrap_xmalloc,
	wrap_xfree,
	wrap_xcalloc,
	wrap_xrealloc
    };

    TypedData_Get_Struct(cb, nghttp2_session_callbacks, &ds9_callbacks_type, callbacks);

    nghttp2_session_client_new3(&session, callbacks, (void *)self, NULL, &mem);
    DATA_PTR(self) = session;

    return self;
}

static VALUE server_init_internals(VALUE self, VALUE cb)
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_session *session;
    nghttp2_mem mem = {
	NULL,
	wrap_xmalloc,
	wrap_xfree,
	wrap_xcalloc,
	wrap_xrealloc
    };

    TypedData_Get_Struct(cb, nghttp2_session_callbacks, &ds9_callbacks_type, callbacks);

    nghttp2_session_server_new3(&session, callbacks, (void *)self, NULL, &mem);
    DATA_PTR(self) = session;

    return self;
}

static VALUE session_want_write_p(VALUE self)
{
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    if (nghttp2_session_want_write(session) == 0) {
	return Qfalse;
    }

    return Qtrue;
}

static VALUE session_want_read_p(VALUE self)
{
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    if (nghttp2_session_want_read(session) == 0) {
	return Qfalse;
    }

    return Qtrue;
}

static VALUE session_submit_settings(VALUE self, VALUE settings)
{
    size_t niv, i;
    nghttp2_settings_entry *iv, *head;
    nghttp2_session *session;
    int rv;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    niv = RARRAY_LEN(settings);
    iv = xcalloc(niv, sizeof(nghttp2_settings_entry));
    head = iv;

    for(i = 0; i < niv; i++, head++) {
	VALUE tuple = rb_ary_entry(settings, (long)i);
	head->settings_id = NUM2INT(rb_ary_entry(tuple, 0));
	head->value = NUM2INT(rb_ary_entry(tuple, 1));
    }

    rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, niv);

    xfree(iv);

    if (0 != rv) {
	rb_raise(eDS9Exception, "Error: %s", nghttp2_strerror(rv));
    }

    return self;
}

static VALUE session_send(VALUE self)
{
    int rv;
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    rv = nghttp2_session_send(session);
    if (rv != 0) {
	rb_raise(eDS9Exception, "Error SEESSION SEND: %s", nghttp2_strerror(rv));
    }

    return self;
}

static VALUE session_receive(VALUE self)
{
    int rv;
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    assert(session);
    rv = nghttp2_session_recv(session);
    if (rv != 0) {
	rb_raise(eDS9Exception, "Error: %s", nghttp2_strerror(rv));
    }

    return self;
}

static VALUE session_mem_receive(VALUE self, VALUE buf)
{
    int rv;
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    rv = nghttp2_session_mem_recv(session, (const uint8_t *)StringValuePtr(buf), RSTRING_LEN(buf));
    if (rv < 0) {
	rb_raise(eDS9Exception, "Error: %s", nghttp2_strerror(rv));
    }

    return INT2NUM(rv);
}

static VALUE session_mem_send(VALUE self)
{
    ssize_t rv;
    const uint8_t *data;
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    rv = nghttp2_session_mem_send(session, &data);

    if (rv == 0) {
	return Qfalse;
    }

    if (rv < 0) {
	rb_raise(eDS9Exception, "Error: %s", nghttp2_strerror(rv));
    }

    return rb_str_new((const char *)data, rv);
}

static VALUE session_outbound_queue_size(VALUE self)
{
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    return INT2NUM(nghttp2_session_get_outbound_queue_size(session));
}

static VALUE session_submit_request(VALUE self, VALUE settings)
{
    size_t niv, i;
    nghttp2_nv *nva, *head;
    nghttp2_session *session;
    int rv;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    Check_Type(settings, T_ARRAY);
    niv = RARRAY_LEN(settings);
    nva = xcalloc(niv, sizeof(nghttp2_nv));

    copy_list_to_nv(settings, nva, niv);

    rv = nghttp2_submit_request(session, NULL, nva, niv, NULL, NULL);

    xfree(nva);

    if (rv < 0) {
	rb_raise(rb_eStandardError, "Error: %s", nghttp2_strerror(rv));
    }

    return INT2NUM(rv);
}

static VALUE session_terminate_session(VALUE self, VALUE err)
{
    int rv;
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    rv = nghttp2_session_terminate_session(session, NUM2INT(err));

    if (rv != 0) {
	rb_raise(rb_eStandardError, "Error: %s", nghttp2_strerror(rv));
    }

    return self;
}

static VALUE server_submit_response(VALUE self, VALUE stream_id, VALUE headers)
{
    nghttp2_session *session;
    size_t niv;
    nghttp2_nv *nva, *head;
    nghttp2_data_provider provider;
    int rv;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    niv = RARRAY_LEN(headers);
    nva = xcalloc(niv, sizeof(nghttp2_nv));

    copy_list_to_nv(headers, nva, niv);

    provider.read_callback = rb_data_read_callback;

    rv = nghttp2_submit_response(session, NUM2INT(stream_id), nva, niv, &provider);

    xfree(nva);

    if (0 != rv) {
	rb_raise(eDS9Exception, "Error: %s", nghttp2_strerror(rv));
    }

    return self;
}

static VALUE make_callbacks(VALUE self, VALUE callback_list)
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_session_callbacks_new(&callbacks);

    nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
    nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, on_begin_headers_callback);
    nghttp2_session_callbacks_set_on_frame_not_send_callback(callbacks, on_frame_not_send_callback);
    nghttp2_session_callbacks_set_on_begin_frame_callback(callbacks, on_begin_frame_callback);
    nghttp2_session_callbacks_set_on_invalid_frame_recv_callback(callbacks, on_invalid_frame_recv_callback);

    nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
    nghttp2_session_callbacks_set_recv_callback(callbacks, recv_callback);
    nghttp2_session_callbacks_set_on_frame_send_callback(callbacks, on_frame_send_callback);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_callback);
    nghttp2_session_callbacks_set_before_frame_send_callback(callbacks, before_frame_send_callback);

    return TypedData_Wrap_Struct(cDS9Callbacks, &ds9_callbacks_type, callbacks);
}

static VALUE server_submit_push_promise(VALUE self, VALUE stream_id, VALUE headers)
{
    nghttp2_session *session;
    nghttp2_nv *nva, *head;
    size_t niv, i;
    int rv;

    TypedData_Get_Struct(self, nghttp2_session, &ds9_session_type, session);
    CheckSelf(session);

    Check_Type(headers, T_ARRAY);
    niv = RARRAY_LEN(headers);
    nva = xcalloc(niv, sizeof(nghttp2_nv));
    head = nva;

    for(i = 0; i < niv; i++, head++) {
	VALUE tuple = rb_ary_entry(headers, (long)i);
	VALUE name = rb_ary_entry(tuple, 0);
	VALUE value = rb_ary_entry(tuple, 1);

	head->name = (uint8_t *)StringValuePtr(name);
	head->namelen = RSTRING_LEN(name);

	head->value = (uint8_t *)StringValuePtr(value);
	head->valuelen = RSTRING_LEN(value);
	head->flags = NGHTTP2_NV_FLAG_NONE;
    }

    rv = nghttp2_submit_push_promise(session, 0, NUM2INT(stream_id), nva, niv, NULL);

    xfree(nva);

    switch(rv) {
	case NGHTTP2_ERR_NOMEM:
	case NGHTTP2_ERR_PROTO:
	case NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE:
	case NGHTTP2_ERR_INVALID_ARGUMENT:
	    rb_raise(eDS9Exception, "Error: %s", nghttp2_strerror(rv));
	    break;
	default:
	    return INT2NUM(rv);
    }
}

void Init_ds9(void)
{
    mDS9 = rb_define_module("DS9");

    Init_ds9_frames(mDS9);

    rb_define_const(mDS9, "NGHTTP2_PROTO_VERSION_ID", rb_str_new(NGHTTP2_PROTO_VERSION_ID, NGHTTP2_PROTO_VERSION_ID_LEN));

    eDS9Exception = rb_define_class_under(mDS9, "Exception", rb_eStandardError);

    VALUE mDS9Settings = rb_define_module_under(mDS9, "Settings");
    rb_define_const(mDS9Settings, "MAX_CONCURRENT_STREAMS", INT2NUM(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS));
    rb_define_const(mDS9Settings, "INITIAL_WINDOW_SIZE", INT2NUM(NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE));
    rb_define_const(mDS9, "ERR_WOULDBLOCK", INT2NUM(NGHTTP2_ERR_WOULDBLOCK));
    rb_define_const(mDS9, "ERR_EOF", INT2NUM(NGHTTP2_ERR_EOF));
    rb_define_const(mDS9, "NO_ERROR", INT2NUM(NGHTTP2_NO_ERROR));
    rb_define_const(mDS9, "INITIAL_WINDOW_SIZE", INT2NUM(NGHTTP2_INITIAL_WINDOW_SIZE));

    cDS9Callbacks = rb_define_class_under(mDS9, "Callbacks", rb_cData);

    cDS9Session = rb_define_class_under(mDS9, "Session", rb_cObject);
    cDS9Client = rb_define_class_under(mDS9, "Client", cDS9Session);
    cDS9Server = rb_define_class_under(mDS9, "Server", cDS9Session);

    rb_define_alloc_func(cDS9Session, allocate_session);

    rb_define_method(cDS9Session, "want_write?", session_want_write_p, 0);
    rb_define_method(cDS9Session, "want_read?", session_want_read_p, 0);
    rb_define_method(cDS9Session, "submit_settings", session_submit_settings, 1);
    rb_define_method(cDS9Session, "send", session_send, 0);
    rb_define_method(cDS9Session, "receive", session_receive, 0);
    rb_define_method(cDS9Session, "mem_receive", session_mem_receive, 1);
    rb_define_method(cDS9Session, "mem_send", session_mem_send, 0);
    rb_define_method(cDS9Session, "outbound_queue_size", session_outbound_queue_size, 0);
    rb_define_method(cDS9Session, "terminate_session", session_terminate_session, 1);

    rb_define_method(cDS9Session, "submit_request", session_submit_request, 1);
    rb_define_private_method(cDS9Session, "make_callbacks", make_callbacks, 1);
    rb_define_private_method(cDS9Client, "init_internals", client_init_internals, 1);
    rb_define_private_method(cDS9Server, "init_internals", server_init_internals, 1);

    rb_define_method(cDS9Server, "submit_response", server_submit_response, 2);
    rb_define_method(cDS9Server, "submit_push_promise", server_submit_push_promise, 2);
}

/* vim: set noet sws=4 sw=4: */
