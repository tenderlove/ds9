#include <ds9.h>

VALUE mDS9Frames;

VALUE cDS9FramesFrame;
VALUE mDS9FramesFlags;
VALUE cDS9FramesData;
VALUE cDS9FramesHeaders;
VALUE cDS9FramesPriority;
VALUE cDS9FramesRstStream;
VALUE cDS9FramesSettings;
VALUE cDS9FramesPushPromise;
VALUE cDS9FramesPing;
VALUE cDS9FramesGoaway;
VALUE cDS9FramesWindowUpdate;
VALUE cDS9FramesContinuation;

static const rb_data_type_t ds9_frame_type = {
    "DS9/frame",
    {0, xfree, 0,},
    0, 0,
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    RUBY_TYPED_FREE_IMMEDIATELY,
#endif
};

static VALUE frame_type_class(nghttp2_frame *frame)
{
    switch(frame->hd.type) {
	case NGHTTP2_DATA: return cDS9FramesData;
	case NGHTTP2_HEADERS: return cDS9FramesHeaders;
	case NGHTTP2_PRIORITY: return cDS9FramesPriority;
	case NGHTTP2_RST_STREAM: return cDS9FramesRstStream;
	case NGHTTP2_SETTINGS: return cDS9FramesSettings;
	case NGHTTP2_PUSH_PROMISE: return cDS9FramesPushPromise;
	case NGHTTP2_PING: return cDS9FramesPing;
	case NGHTTP2_GOAWAY: return cDS9FramesGoaway;
	case NGHTTP2_WINDOW_UPDATE: return cDS9FramesWindowUpdate;
	case NGHTTP2_CONTINUATION: return cDS9FramesContinuation;
	default: return cDS9FramesFrame;
    }
}

VALUE WrapDS9FrameHeader(const nghttp2_frame_hd *hd)
{
    VALUE klass = rb_const_get(cDS9FramesFrame, rb_intern("Header"));
    return rb_funcall(klass, rb_intern("new"), 4,
	    INT2NUM(hd->length),
	    INT2NUM(hd->stream_id),
	    INT2NUM(hd->type),
	    INT2NUM(hd->flags));
}

VALUE WrapDS9Frame(const nghttp2_frame *frame)
{
    /* dup the frame so Ruby can manage the struct's memory */
    nghttp2_frame *dup = xmalloc(sizeof(nghttp2_frame));
    memcpy(dup, frame, sizeof(nghttp2_frame));

    return TypedData_Wrap_Struct(frame_type_class(dup), &ds9_frame_type, dup);
}

static VALUE frame_stream_id(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &ds9_frame_type, frame);

    return INT2NUM(frame->hd.stream_id);
}

static VALUE frame_type(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &ds9_frame_type, frame);

    return INT2NUM(frame->hd.type);
}

static VALUE frame_flags(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &ds9_frame_type, frame);

    return INT2NUM(frame->hd.flags);
}

static VALUE promised_stream_id(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &ds9_frame_type, frame);

    return INT2NUM(frame->push_promise.promised_stream_id);
}

static VALUE frame_header(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &ds9_frame_type, frame);

    return WrapDS9FrameHeader((nghttp2_frame_hd *)frame);
}

static VALUE header_category(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &ds9_frame_type, frame);

    return INT2NUM(frame->headers.cat);
}

static VALUE goaway_last_stream_id(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &ds9_frame_type, frame);

    return INT2NUM(frame->goaway.last_stream_id);
}

static VALUE goaway_error_code(VALUE self)
{
    nghttp2_frame *frame;
    TypedData_Get_Struct(self, nghttp2_frame, &ds9_frame_type, frame);

    return INT2NUM(frame->goaway.error_code);
}

void Init_ds9_frames(VALUE mDS9)
{
    mDS9Frames = rb_define_module_under(mDS9, "Frames");
    cDS9FramesFrame = rb_define_class_under(mDS9Frames, "Frame", rb_cData);

    mDS9FramesFlags = rb_define_module_under(cDS9FramesFrame, "Flags");

    cDS9FramesData = rb_define_class_under(mDS9Frames, "Data", cDS9FramesFrame);
    cDS9FramesHeaders = rb_define_class_under(mDS9Frames, "Headers", cDS9FramesFrame);
    cDS9FramesPriority = rb_define_class_under(mDS9Frames, "Priority", cDS9FramesFrame);
    cDS9FramesRstStream = rb_define_class_under(mDS9Frames, "RstStream", cDS9FramesFrame);
    cDS9FramesSettings = rb_define_class_under(mDS9Frames, "Settings", cDS9FramesFrame);
    cDS9FramesPushPromise = rb_define_class_under(mDS9Frames, "PushPromise", cDS9FramesFrame);
    cDS9FramesPing = rb_define_class_under(mDS9Frames, "Ping", cDS9FramesFrame);
    cDS9FramesGoaway = rb_define_class_under(mDS9Frames, "Goaway", cDS9FramesFrame);
    cDS9FramesWindowUpdate = rb_define_class_under(mDS9Frames, "WindowUpdate", cDS9FramesFrame);
    cDS9FramesContinuation = rb_define_class_under(mDS9Frames, "Continuation", cDS9FramesFrame);

    rb_define_method(cDS9FramesFrame, "stream_id", frame_stream_id, 0);
    rb_define_method(cDS9FramesFrame, "type", frame_type, 0);
    rb_define_method(cDS9FramesFrame, "flags", frame_flags, 0);
    rb_define_method(cDS9FramesFrame, "header", frame_header, 0);
    rb_define_method(cDS9FramesPushPromise, "promised_stream_id", promised_stream_id, 0);
    rb_define_method(cDS9FramesHeaders, "category", header_category, 0);

    rb_define_const(mDS9FramesFlags, "NONE", INT2NUM(NGHTTP2_FLAG_NONE));
    rb_define_const(mDS9FramesFlags, "END_STREAM", INT2NUM(NGHTTP2_FLAG_END_STREAM));
    rb_define_const(mDS9FramesFlags, "END_HEADERS", INT2NUM(NGHTTP2_FLAG_END_HEADERS));
    rb_define_const(mDS9FramesFlags, "ACK", INT2NUM(NGHTTP2_FLAG_ACK));
    rb_define_const(mDS9FramesFlags, "PADDED", INT2NUM(NGHTTP2_FLAG_PADDED));
    rb_define_const(mDS9FramesFlags, "PRIORITY", INT2NUM(NGHTTP2_FLAG_PRIORITY));

    rb_define_method(cDS9FramesGoaway, "last_stream_id", goaway_last_stream_id, 0);
    rb_define_method(cDS9FramesGoaway, "error_code", goaway_error_code, 0);
    rb_define_const(cDS9FramesGoaway, "NO_ERROR", INT2NUM(NGHTTP2_NO_ERROR));
    rb_define_const(cDS9FramesGoaway, "PROTOCOL_ERROR", INT2NUM(NGHTTP2_PROTOCOL_ERROR));
    rb_define_const(cDS9FramesGoaway, "INTERNAL_ERROR", INT2NUM(NGHTTP2_INTERNAL_ERROR));
    rb_define_const(cDS9FramesGoaway, "FLOW_CONTROL_ERROR", INT2NUM(NGHTTP2_FLOW_CONTROL_ERROR));
    rb_define_const(cDS9FramesGoaway, "SETTINGS_TIMEOUT", INT2NUM(NGHTTP2_SETTINGS_TIMEOUT));
    rb_define_const(cDS9FramesGoaway, "STREAM_CLOSED", INT2NUM(NGHTTP2_STREAM_CLOSED));
    rb_define_const(cDS9FramesGoaway, "FRAME_SIZE_ERROR", INT2NUM(NGHTTP2_FRAME_SIZE_ERROR));
    rb_define_const(cDS9FramesGoaway, "REFUSED_STREAM", INT2NUM(NGHTTP2_REFUSED_STREAM));
    rb_define_const(cDS9FramesGoaway, "CANCEL", INT2NUM(NGHTTP2_CANCEL));
    rb_define_const(cDS9FramesGoaway, "COMPRESSION_ERROR", INT2NUM(NGHTTP2_COMPRESSION_ERROR));
    rb_define_const(cDS9FramesGoaway, "CONNECT_ERROR", INT2NUM(NGHTTP2_CONNECT_ERROR));
    rb_define_const(cDS9FramesGoaway, "ENHANCE_YOUR_CALM", INT2NUM(NGHTTP2_ENHANCE_YOUR_CALM));
    rb_define_const(cDS9FramesGoaway, "INADEQUATE_SECURITY", INT2NUM(NGHTTP2_INADEQUATE_SECURITY));
    rb_define_const(cDS9FramesGoaway, "HTTP_1_1_REQUIRED", INT2NUM(NGHTTP2_HTTP_1_1_REQUIRED));

    rb_define_const(cDS9FramesHeaders, "REQUEST", INT2NUM(NGHTTP2_HCAT_REQUEST));
    rb_define_const(cDS9FramesHeaders, "RESPONSE", INT2NUM(NGHTTP2_HCAT_RESPONSE));
    rb_define_const(cDS9FramesHeaders, "PUSH_RESPONSE", INT2NUM(NGHTTP2_HCAT_PUSH_RESPONSE));
    rb_define_const(cDS9FramesHeaders, "HEADERS", INT2NUM(NGHTTP2_HCAT_HEADERS));
}

/* vim: set noet sws=4 sw=4: */
