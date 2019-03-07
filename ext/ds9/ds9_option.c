#include <ds9_option.h>

VALUE cDS9Option;

static VALUE allocate_option(VALUE klass)
{
    nghttp2_option *option;

    nghttp2_option_new(&option);

    return TypedData_Wrap_Struct(klass, &ds9_option_type, option);
}

static VALUE option_set_no_auto_ping_ack(VALUE self)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_no_auto_ping_ack(option, 1);

    return self;
}

static VALUE option_set_no_auto_window_update(VALUE self)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_no_auto_window_update(option, 1);

    return self;
}

static VALUE option_set_no_closed_streams(VALUE self)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_no_closed_streams(option, 1);

    return self;
}

static VALUE option_set_no_http_messaging(VALUE self)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_no_http_messaging(option, 1);

    return self;
}

static VALUE option_set_no_recv_client_magic(VALUE self)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_no_recv_client_magic(option, 1);

    return self;
}


static VALUE option_set_builtin_recv_extension_type(VALUE self, VALUE size)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_builtin_recv_extension_type(option, NUM2INT(size));

    return self;
}

static VALUE option_set_max_deflate_dynamic_table_size(VALUE self, VALUE size)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_max_deflate_dynamic_table_size(option, NUM2INT(size));

    return self;
}

static VALUE option_set_max_send_header_block_length(VALUE self, VALUE size)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_max_send_header_block_length(option, NUM2INT(size));

    return self;
}

static VALUE option_set_max_reserved_remote_streams(VALUE self, VALUE size)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_max_reserved_remote_streams(option, NUM2INT(size));

    return self;
}

static VALUE option_set_peer_max_concurrent_streams(VALUE self, VALUE size)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_peer_max_concurrent_streams(option, NUM2INT(size));

    return self;
}

static VALUE option_set_user_recv_extension_type(VALUE self, VALUE size)
{
    nghttp2_option *option;
    TypedData_Get_Struct(self, nghttp2_option, &ds9_option_type, option);

    nghttp2_option_set_user_recv_extension_type(option, NUM2INT(size));

    return self;
}

nghttp2_option* UnwrapDS9Option(VALUE opt)
{
    nghttp2_option* option = NULL;

    if (!NIL_P(opt)) {
	TypedData_Get_Struct(opt, nghttp2_option, &ds9_option_type, option);
    }

    return option;
}

void Init_ds9_option(VALUE mDS9)
{
    cDS9Option = rb_define_class_under(mDS9, "Option", rb_cData);
    rb_define_alloc_func(cDS9Option, allocate_option);

    rb_define_method(cDS9Option, "set_no_auto_ping_ack", option_set_no_auto_ping_ack, 0);
    rb_define_method(cDS9Option, "set_no_auto_window_update", option_set_no_auto_window_update, 0);
    rb_define_method(cDS9Option, "set_no_closed_streams", option_set_no_closed_streams, 0);
    rb_define_method(cDS9Option, "set_no_http_messaging", option_set_no_http_messaging, 0);
    rb_define_method(cDS9Option, "set_no_recv_client_magic", option_set_no_recv_client_magic, 0);

    rb_define_method(cDS9Option, "set_builtin_recv_extension_type", option_set_builtin_recv_extension_type, 1);
    rb_define_method(cDS9Option, "set_max_deflate_dynamic_table_size", option_set_max_deflate_dynamic_table_size, 1);
    rb_define_method(cDS9Option, "set_max_send_header_block_length", option_set_max_send_header_block_length, 1);
    rb_define_method(cDS9Option, "set_max_reserved_remote_streams", option_set_max_reserved_remote_streams, 1);
    rb_define_method(cDS9Option, "set_peer_max_concurrent_streams", option_set_peer_max_concurrent_streams, 1);
    rb_define_method(cDS9Option, "set_user_recv_extension_type", option_set_user_recv_extension_type, 1);
}
