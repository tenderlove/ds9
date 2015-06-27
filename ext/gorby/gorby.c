#include <gorby.h>
#include <assert.h>

VALUE mGorby;
VALUE cGorbySession;
VALUE cGorbyClient;
VALUE cGorbyServer;
VALUE cGorbyCallbacks;
VALUE eGorbyException;

static ssize_t send_callback(nghttp2_session * session,
			     const uint8_t *data,
			     size_t length,
			     int flags, void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("send_event"), 1, rb_str_new(data, length));

    return NUM2INT(ret);
}

static int on_frame_recv_callback(nghttp2_session *session,
				  const nghttp2_frame *frame,
				  void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("on_frame_recv"), 1, WrapGorbyFrame(frame));

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
	    rb_str_new(name, namelen),
	    rb_str_new(value, valuelen),
	    WrapGorbyFrame(frame),
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
	    WrapGorbyFrame(frame));

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

    /*
    printf("BUF DEBUG: ");
      int i;
for (i = 0; i < len; i++)
{
    if (i > 0) printf(":");
    printf("%02X", buf[i]);
}
printf(", %d\n", len);
*/


    return len;
}

static int on_begin_frame_callback(nghttp2_session *session,
				   const nghttp2_frame_hd *hd,
				   void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("on_begin_frame"), 1, WrapGorbyFrameHeader(hd));

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
	    rb_str_new(data, len),
	    INT2NUM(flags));

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static int on_invalid_frame_recv_callback(nghttp2_session *session,
					  const nghttp2_frame *frame, uint32_t lib_error_code,
					  void *user_data)
{
    VALUE self = (VALUE)user_data;
    VALUE ret;

    ret = rb_funcall(self, rb_intern("on_invalid_frame_recv"), 2,
	    WrapGorbyFrame(frame),
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

    ret = rb_funcall(self, rb_intern("on_frame_send"), 1, WrapGorbyFrame(frame));

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

    ret = rb_funcall(self, rb_intern("on_frame_not_send"), 2, WrapGorbyFrame(frame), reason);

    if (ret == Qfalse) {
	return 1;
    }

    return 0;
}

static VALUE allocate_session(VALUE klass)
{
    return TypedData_Wrap_Struct(klass, &gorby_session_type, 0);
}

static VALUE client_init_internals(VALUE self, VALUE cb)
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_session *session;

    TypedData_Get_Struct(cb, nghttp2_session_callbacks, &gorby_callbacks_type, callbacks);

    nghttp2_session_client_new(&session, callbacks, (void *)self);
    DATA_PTR(self) = session;

    return self;
}

static VALUE server_init_internals(VALUE self, VALUE cb)
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_session *session;

    TypedData_Get_Struct(cb, nghttp2_session_callbacks, &gorby_callbacks_type, callbacks);

    nghttp2_session_server_new(&session, callbacks, (void *)self);
    DATA_PTR(self) = session;

    return self;
}

static VALUE session_want_write_p(VALUE self)
{
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

    if (nghttp2_session_want_write(session) == 0) {
	return Qfalse;
    }

    return Qtrue;
}

static VALUE session_want_read_p(VALUE self)
{
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

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

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

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
	rb_raise(eGorbyException, "Error: %s", nghttp2_strerror(rv));
    }

    return self;
}

static VALUE session_send(VALUE self)
{
    int rv;
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

    rv = nghttp2_session_send(session);
    if (rv != 0) {
	rb_raise(eGorbyException, "Error SEESSION SEND: %s", nghttp2_strerror(rv));
    }

    return self;
}

static VALUE session_receive(VALUE self)
{
    int rv;
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

    assert(session);
    rv = nghttp2_session_recv(session);
    if (rv != 0) {
	rb_raise(eGorbyException, "Error: %s", nghttp2_strerror(rv));
    }

    return self;
}

static VALUE session_mem_receive(VALUE self, VALUE buf)
{
    int rv;
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

    rv = nghttp2_session_mem_recv(session, StringValuePtr(buf), RSTRING_LEN(buf));
    if (rv < 0) {
	rb_raise(eGorbyException, "Error: %s", nghttp2_strerror(rv));
    }

    return INT2NUM(rv);
}

static VALUE session_mem_send(VALUE self)
{
    ssize_t rv;
    uint8_t *data;
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

    rv = nghttp2_session_mem_send(session, &data);

    if (rv == 0) {
	return Qfalse;
    }

    if (rv < 0) {
	rb_raise(eGorbyException, "Error: %s", nghttp2_strerror(rv));
    }

    return rb_str_new(data, rv);
}

static VALUE session_outbound_queue_size(VALUE self)
{
    nghttp2_session *session;

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

    return INT2NUM(nghttp2_session_get_outbound_queue_size(session));
}

static VALUE session_submit_request(VALUE self, VALUE request)
{
    VALUE settings;
    size_t niv, i;
    nghttp2_nv *nva, *head;
    nghttp2_session *session;
    int rv;

    settings = rb_funcall(request, rb_intern("headers"), 0);

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

    niv = RARRAY_LEN(settings);
    nva = xcalloc(nva, sizeof(nghttp2_nv));
    head = nva;

    for(i = 0; i < niv; i++, head++) {
	VALUE tuple = rb_ary_entry(settings, (long)i);
	VALUE name = rb_ary_entry(tuple, 0);
	VALUE value = rb_ary_entry(tuple, 1);

	head->name = StringValuePtr(name);
	head->namelen = RSTRING_LEN(name);

	head->value = StringValuePtr(value);
	head->valuelen = RSTRING_LEN(value);
	head->flags = NGHTTP2_NV_FLAG_NONE;
    }

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

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

    rv = nghttp2_session_terminate_session(session, NUM2INT(err));

    if (rv != 0) {
	rb_raise(rb_eStandardError, "Error: %s", nghttp2_strerror(rv));
    }

    return self;
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

static VALUE server_submit_response(VALUE self, VALUE stream_id, VALUE headers)
{
    nghttp2_session *session;
    size_t niv, i;
    nghttp2_nv *nva, *head;
    nghttp2_data_provider provider;
    int rv;

    TypedData_Get_Struct(self, nghttp2_session, &gorby_session_type, session);

    niv = RARRAY_LEN(headers);
    nva = xcalloc(nva, sizeof(nghttp2_nv));
    head = nva;

    for(i = 0; i < niv; i++, head++) {
	VALUE tuple = rb_ary_entry(headers, (long)i);
	VALUE name = rb_ary_entry(tuple, 0);
	VALUE value = rb_ary_entry(tuple, 1);

	head->name = StringValuePtr(name);
	head->namelen = RSTRING_LEN(name);

	head->value = StringValuePtr(value);
	head->valuelen = RSTRING_LEN(value);
	head->flags = NGHTTP2_NV_FLAG_NONE;
    }

    provider.read_callback = rb_data_read_callback;

    rv = nghttp2_submit_response(session, NUM2INT(stream_id), nva, niv, &provider);

    xfree(nva);

    if (0 != rv) {
	rb_raise(eGorbyException, "Error: %s", nghttp2_strerror(rv));
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

    return TypedData_Wrap_Struct(cGorbyCallbacks, &gorby_callbacks_type, callbacks);
}

void Init_gorby(void)
{
    mGorby = rb_define_module("Gorby");

    Init_gorby_frames(mGorby);

    rb_define_const(mGorby, "NGHTTP2_PROTO_VERSION_ID", rb_str_new(NGHTTP2_PROTO_VERSION_ID, NGHTTP2_PROTO_VERSION_ID_LEN));

    eGorbyException = rb_define_class_under(mGorby, "Exception", rb_eStandardError);

    VALUE mGorbySettings = rb_define_module_under(mGorby, "Settings");
    rb_define_const(mGorbySettings, "MAX_CONCURRENT_STREAMS", INT2NUM(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS));
    rb_define_const(mGorbySettings, "INITIAL_WINDOW_SIZE", INT2NUM(NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE));
    rb_define_const(mGorby, "ERR_WOULDBLOCK", INT2NUM(NGHTTP2_ERR_WOULDBLOCK));
    rb_define_const(mGorby, "ERR_EOF", INT2NUM(NGHTTP2_ERR_EOF));
    rb_define_const(mGorby, "NO_ERROR", INT2NUM(NGHTTP2_NO_ERROR));
    rb_define_const(mGorby, "INITIAL_WINDOW_SIZE", INT2NUM(NGHTTP2_INITIAL_WINDOW_SIZE));

    cGorbyCallbacks = rb_define_class_under(mGorby, "Callbacks", rb_cData);

    cGorbySession = rb_define_class_under(mGorby, "Session", rb_cObject);
    cGorbyClient = rb_define_class_under(mGorby, "Client", cGorbySession);
    cGorbyServer = rb_define_class_under(mGorby, "Server", cGorbySession);

    rb_define_alloc_func(cGorbySession, allocate_session);

    rb_define_method(cGorbySession, "want_write?", session_want_write_p, 0);
    rb_define_method(cGorbySession, "want_read?", session_want_read_p, 0);
    rb_define_method(cGorbySession, "submit_settings", session_submit_settings, 1);
    rb_define_method(cGorbySession, "send", session_send, 0);
    rb_define_method(cGorbySession, "receive", session_receive, 0);
    rb_define_method(cGorbySession, "mem_receive", session_mem_receive, 1);
    rb_define_method(cGorbySession, "mem_send", session_mem_send, 0);
    rb_define_method(cGorbySession, "outbound_queue_size", session_outbound_queue_size, 0);
    rb_define_method(cGorbySession, "terminate_session", session_terminate_session, 1);

    rb_define_method(cGorbySession, "submit_request", session_submit_request, 1);
    rb_define_private_method(cGorbySession, "make_callbacks", make_callbacks, 1);
    rb_define_private_method(cGorbyClient, "init_internals", client_init_internals, 1);
    rb_define_private_method(cGorbyServer, "init_internals", server_init_internals, 1);

    rb_define_method(cGorbyServer, "submit_response", server_submit_response, 2);
}

/* vim: set noet sws=4 sw=4: */
