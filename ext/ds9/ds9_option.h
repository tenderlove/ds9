#ifndef DS9_OPTION_H
#define DS9_OPTION_H

#include <ruby.h>
#include <nghttp2/nghttp2.h>

static const rb_data_type_t ds9_option_type = {
    "DS9/option",
    {0, (void (*)(void *))nghttp2_option_del, 0,},
    0, 0,
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    RUBY_TYPED_FREE_IMMEDIATELY,
#endif
};

void Init_ds9_option(VALUE mDS9);
nghttp2_option* UnwrapDS9Option(VALUE opt);

#endif
