#ifndef GORBY_H
#define GORBY_H

#include <ruby.h>
#include <nghttp2/nghttp2.h>

static const rb_data_type_t gorby_session_type = {
    "Gorby/session",
    {0, nghttp2_session_del, 0,},
    0, 0,
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    RUBY_TYPED_FREE_IMMEDIATELY,
#endif
};

static const rb_data_type_t gorby_callbacks_type = {
    "Gorby/callbacks",
    {0, nghttp2_session_callbacks_del, 0,},
    0, 0,
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    RUBY_TYPED_FREE_IMMEDIATELY,
#endif
};

void Init_gorby_client(VALUE mGorby, VALUE cGorbySession);
void Init_gorby_frames(VALUE mGorby);
VALUE WrapGorbyFrame(const nghttp2_frame *frame);
VALUE WrapGorbyFrameHeader(const nghttp2_frame_hd *hd);

#endif
