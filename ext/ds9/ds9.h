#ifndef DS9_H
#define DS9_H

#include <ruby.h>
#include <ruby/io.h>
#include <nghttp2/nghttp2.h>
#include <ds9_option.h>

static const rb_data_type_t ds9_session_type = {
    "DS9/session",
    {0, (void (*)(void *))nghttp2_session_del, 0,},
    0, 0,
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    RUBY_TYPED_FREE_IMMEDIATELY,
#endif
};

static const rb_data_type_t ds9_callbacks_type = {
    "DS9/callbacks",
    {0, (void (*)(void *))nghttp2_session_callbacks_del, 0,},
    0, 0,
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    RUBY_TYPED_FREE_IMMEDIATELY,
#endif
};

void Init_ds9_client(VALUE mDS9, VALUE cDS9Session);
void Init_ds9_frames(VALUE mDS9);
VALUE WrapDS9Frame(const nghttp2_frame *frame);
VALUE WrapDS9FrameHeader(const nghttp2_frame_hd *hd);

#endif
