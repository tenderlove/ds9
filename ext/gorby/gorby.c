#include <gorby.h>

VALUE mGorby;

void Init_gorby(void)
{
    mGorby = rb_define_module("Gorby");

    rb_define_const(mGorby, "NGHTTP2_PROTO_VERSION_ID", rb_str_new(NGHTTP2_PROTO_VERSION_ID, NGHTTP2_PROTO_VERSION_ID_LEN));
}

/* vim: set noet sws=4 sw=4: */
