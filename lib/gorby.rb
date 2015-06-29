require 'gorby.so'

module DS9
  VERSION = '1.0.0'

  module Frames
    class Frame
      Header = Struct.new :length, :stream_id, :type, :flags

      def length
        header.length
      end

      def settings?; false; end
      def headers?;  false; end
      def data?;     false; end
    end

    class Data
      def data?;     true; end
    end

    class Settings
      def settings?; true; end
    end

    class Headers
      def headers?; true; end
    end
  end

  class Session
    def initialize
      cbs = make_callbacks callbacks
      init_internals cbs
    end

    private

    def callbacks
      Session.private_instance_methods(false).grep(/^on_|event$/)
    end

    def send_event string
      raise NotImplementedError
    end

    def on_data_source_read stream_id, length
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
      raise NotImplementedError
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
end
