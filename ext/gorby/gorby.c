#include <gorby.h>

VALUE mGorby;
VALUE cGorbySession;

static const rb_data_type_t gorby_session_type = {
    "Gorby/session",
    {0, NULL, 0,}, /* FIXME: add dealloc */
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
    return 0;
}

static int on_frame_recv_callback(nghttp2_session *session,
				  const nghttp2_frame *frame,
				  void *user_data)
{
    return 0;
}

static int on_stream_close_callback(nghttp2_session *session,
				    int32_t stream_id,
				    uint32_t error_code,
				    void *user_data)
{
    return 0;
}

static int on_header_callback(nghttp2_session *session,
			      const nghttp2_frame *frame,
			      const uint8_t *name, size_t namelen,
			      const uint8_t *value, size_t valuelen,
			      uint8_t flags, void *user_data)
{
    return 0;
}

static int on_begin_headers_callback(nghttp2_session *session,
			             const nghttp2_frame *frame,
			             void *user_data)
{
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

    rb_session = TypedData_Wrap_Struct(klass, &gorby_session_type, session);

    nghttp2_session_server_new(&session, callbacks, (void *)rb_session);
    nghttp2_session_callbacks_del(callbacks);

    return rb_session;
}

void Init_gorby(void)
{
    mGorby = rb_define_module("Gorby");
    cGorbySession = rb_define_class_under(mGorby, "Session", rb_cObject);

    rb_define_alloc_func(cGorbySession, allocate_session);

    rb_define_const(mGorby, "NGHTTP2_PROTO_VERSION_ID", rb_str_new(NGHTTP2_PROTO_VERSION_ID, NGHTTP2_PROTO_VERSION_ID_LEN));
}

/* vim: set noet sws=4 sw=4: */
