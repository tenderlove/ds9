#include <gorby.h>

VALUE mGorbyFrames;

VALUE cGorbyFramesFrame;
VALUE cGorbyFramesData;
VALUE cGorbyFramesHeaders;
VALUE cGorbyFramesPriority;
VALUE cGorbyFramesRstStream;
VALUE cGorbyFramesSettings;
VALUE cGorbyFramesPushPromise;
VALUE cGorbyFramesPing;
VALUE cGorbyFramesGoaway;
VALUE cGorbyFramesWindowUpdate;
VALUE cGorbyFramesContinuation;

static const rb_data_type_t gorby_frame_type = {
    "Gorby/frame",
    {0, xfree, 0,},
    0, 0,
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    RUBY_TYPED_FREE_IMMEDIATELY,
#endif
};

static VALUE frame_type_class(nghttp2_frame *frame)
{
    switch(frame->hd.type) {
	case NGHTTP2_DATA: return cGorbyFramesData;
	case NGHTTP2_HEADERS: return cGorbyFramesHeaders;
	case NGHTTP2_PRIORITY: return cGorbyFramesPriority;
	case NGHTTP2_RST_STREAM: return cGorbyFramesRstStream;
	case NGHTTP2_SETTINGS: return cGorbyFramesSettings;
	case NGHTTP2_PUSH_PROMISE: return cGorbyFramesPushPromise;
	case NGHTTP2_PING: return cGorbyFramesPing;
	case NGHTTP2_GOAWAY: return cGorbyFramesGoaway;
	case NGHTTP2_WINDOW_UPDATE: return cGorbyFramesWindowUpdate;
	case NGHTTP2_CONTINUATION: return cGorbyFramesContinuation;
	default: return cGorbyFramesFrame;
    }
}

VALUE WrapGorbyFrameHeader(const nghttp2_frame_hd *hd)
{
    VALUE klass = rb_const_get(cGorbyFramesFrame, rb_intern("Header"));
    return rb_funcall(klass, rb_intern("new"), 4,
	    INT2NUM(hd->length),
	    INT2NUM(hd->stream_id),
	    INT2NUM(hd->type),
	    INT2NUM(hd->flags));
}

VALUE WrapGorbyFrame(const nghttp2_frame *frame)
{
    /* dup the frame so Ruby can manage the struct's memory */
    nghttp2_frame *dup = xmalloc(sizeof(nghttp2_frame));
    memcpy(dup, frame, sizeof(nghttp2_frame));

    return TypedData_Wrap_Struct(frame_type_class(dup), &gorby_frame_type, dup);
}

static VALUE frame_stream_id(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &gorby_frame_type, frame);

    return INT2NUM(frame->hd.stream_id);
}

static VALUE frame_type(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &gorby_frame_type, frame);

    return INT2NUM(frame->hd.type);
}

static VALUE frame_flags(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &gorby_frame_type, frame);

    return INT2NUM(frame->hd.flags);
}

static VALUE frame_header(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &gorby_frame_type, frame);

    return WrapGorbyFrameHeader((nghttp2_frame_hd *)frame);
}

void Init_gorby_frames(VALUE mGorby)
{
    mGorbyFrames = rb_define_module_under(mGorby, "Frames");
    cGorbyFramesFrame = rb_define_class_under(mGorbyFrames, "Frame", rb_cData);

    cGorbyFramesData = rb_define_class_under(mGorbyFrames, "Data", cGorbyFramesFrame);
    cGorbyFramesHeaders = rb_define_class_under(mGorbyFrames, "Headers", cGorbyFramesFrame);
    cGorbyFramesPriority = rb_define_class_under(mGorbyFrames, "Priority", cGorbyFramesFrame);
    cGorbyFramesRstStream = rb_define_class_under(mGorbyFrames, "RstStream", cGorbyFramesFrame);
    cGorbyFramesSettings = rb_define_class_under(mGorbyFrames, "Settings", cGorbyFramesFrame);
    cGorbyFramesPushPromise = rb_define_class_under(mGorbyFrames, "PushPromise", cGorbyFramesFrame);
    cGorbyFramesPing = rb_define_class_under(mGorbyFrames, "Ping", cGorbyFramesFrame);
    cGorbyFramesGoaway = rb_define_class_under(mGorbyFrames, "Goaway", cGorbyFramesFrame);
    cGorbyFramesWindowUpdate = rb_define_class_under(mGorbyFrames, "WindowUpdate", cGorbyFramesFrame);
    cGorbyFramesContinuation = rb_define_class_under(mGorbyFrames, "Continuation", cGorbyFramesFrame);

    rb_define_method(cGorbyFramesFrame, "stream_id", frame_stream_id, 0);
    rb_define_method(cGorbyFramesFrame, "type", frame_type, 0);
    rb_define_method(cGorbyFramesFrame, "flags", frame_flags, 0);
    rb_define_method(cGorbyFramesFrame, "header", frame_header, 0);
}

/* vim: set noet sws=4 sw=4: */
