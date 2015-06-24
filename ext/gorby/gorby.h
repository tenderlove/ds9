#ifndef GORBY_H
#define GORBY_H

#include <ruby.h>
#include <nghttp2/nghttp2.h>

void Init_gorby_frames(VALUE mGorby);
VALUE WrapGorbyFrame(const nghttp2_frame *frame);

#endif
