require 'gorby.so'

module Gorby
  VERSION = '1.0.0'

  module Frames
    class Frame
      Header = Struct.new :length, :stream_id, :type, :flags

      def length
        header.length
      end

      def settings?; false; end
      def headers?;  false; end
    end

    class Settings
      def settings?; true; end
    end

    class Headers
      def headers?; true; end
    end
  end

  class Events
    def send_event string
      string.length
    end

    def on_frame_recv frame
    end

    def on_stream_close id, error_code
    end

    def on_header name, value, frame, flags
    end

    def on_begin_headers frame
    end

    def recv_event length
    end

    def on_begin_frame frame
    end

    def on_data_chunk_recv id, data, flags
    end

    def on_invalid_frame_recv frame, error_code
    end

    def on_frame_send frame
    end

    def on_frame_not_send frame, reason
    end
  end

  class Session
    attr_reader :target

    def initialize target
      @target = target
      cbs = make_callbacks callbacks
      init_internals cbs
    end

    private

    def callbacks
      CALLBACKS
    end

    def send_event string
      @target.send_event string
    end

    def on_frame_recv frame
      @target.on_frame_recv frame
    end

    def on_stream_close id, error_code
      @target.on_stream_close id, error_code
    end

    def on_header name, value, frame, flags
      @target.on_header name, value, frame, flags
    end

    def on_begin_headers frame
      @target.on_begin_headers frame
    end

    def recv_event length
      @target.recv_event length
    end

    def on_begin_frame frame
      @target.on_begin_frame frame
    end

    def on_data_chunk_recv id, data, flags
      @target.on_data_chunk_recv id, data, flags
    end

    def on_invalid_frame_recv frame, error_code
      @target.on_invalid_frame_recv frame, error_code
    end

    def on_frame_send frame
      @target.on_frame_send frame
    end

    def on_frame_not_send frame, reason
      @target.on_frame_not_send frame, reason
    end

    CALLBACKS = private_instance_methods(false).find_all { |m|
      m =~ /^on_|event$/
    }

  end
end
