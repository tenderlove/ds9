#include <gorby.h>

VALUE mGorby;
VALUE cGorbySession;

static const rb_data_type_t gorby_session_type = {
    "Gorby/session",
    {0, nghttp2_session_del, 0,},
    0, 0,
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    RUBY_TYPED_FREE_IMMEDIATELY,
#endif
};

static ssize_t send_callback(nghttp2_session * session,
			     const uint8_t *data,
			     size_t length,
			     int flags, void *user_data)
{
    printf("send_callback\n");
    return 0;
}

static int on_frame_recv_callback(nghttp2_session *session,
				  const nghttp2_frame *frame,
				  void *user_data)
{
    printf("on_frame_recv_callback\n");
    return 0;
}

static int on_stream_close_callback(nghttp2_session *session,
				    int32_t stream_id,
				    uint32_t error_code,
				    void *user_data)
{
    printf("on_stream_close_callback\n");
    return 0;
}

static int on_header_callback(nghttp2_session *session,
			      const nghttp2_frame *frame,
			      const uint8_t *name, size_t namelen,
			      const uint8_t *value, size_t valuelen,
			      uint8_t flags, void *user_data)
{
    printf("on_header_callback\n");
    return 0;
}

static int on_begin_headers_callback(nghttp2_session *session,
			             const nghttp2_frame *frame,
			             void *user_data)
{
    printf("on_begin_headers_callback\n");
    return 0;
}

static VALUE allocate_session(VALUE klass)
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_session *session;
    VALUE rb_session;

    nghttp2_session_callbacks_new(&callbacks);
    nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
    nghttp2_session_callbacks_set_on_stream_close_callback( callbacks, on_stream_close_callback);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
    nghttp2_session_callbacks_set_on_begin_headers_callback( callbacks, on_begin_headers_callback);

    rb_session = TypedData_Wrap_Struct(klass, &gorby_session_type, 0);

    nghttp2_session_server_new(&session, callbacks, (void *)rb_session);
    DATA_PTR(rb_session) = session;

    nghttp2_session_callbacks_del(callbacks);

    return rb_session;
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
	rb_raise(rb_eRuntimeError, "Error: %s", nghttp2_strerror(rv));
    }

    return self;
}

void Init_gorby(void)
{
    mGorby = rb_define_module("Gorby");
    rb_define_const(mGorby, "NGHTTP2_PROTO_VERSION_ID", rb_str_new(NGHTTP2_PROTO_VERSION_ID, NGHTTP2_PROTO_VERSION_ID_LEN));

    VALUE mGorbySettings = rb_define_module_under(mGorby, "Settings");
    rb_define_const(mGorbySettings, "MAX_CONCURRENT_STREAMS", INT2NUM(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS));

    cGorbySession = rb_define_class_under(mGorby, "Session", rb_cObject);
    rb_define_alloc_func(cGorbySession, allocate_session);
    rb_define_method(cGorbySession, "submit_settings", session_submit_settings, 1);

}

/* vim: set noet sws=4 sw=4: */
