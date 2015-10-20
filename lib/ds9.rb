require 'ds9.so'

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
      def push_promise?; false; end
      def priority?; false; end
      def rst_stream?; false; end
      def ping?; false; end
      def goaway?; false; end
      def window_update?; false; end
      def continuation?; false; end
    end

    class Continuation
      def continuation?; true; end
    end
    class WindowUpdate
      def window_update?; true; end
    end

    class Goaway
      def goaway?; true; end
    end

    class Ping
      def ping?; true; end
    end

    class Priority
      def priority?; true; end
    end

    class RstStream
      def rst_stream?; true; end
    end

    class PushPromise
      def push_promise?; true; end
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
      CALLBACKS
    end

    def send_event string
      raise NotImplementedError
    end

    def on_data_source_read stream_id, length
      raise NotImplementedError
    end

    def recv_event length
      raise NotImplementedError
    end

    CALLBACKS = private_instance_methods(false).grep(/^(on_|before)|event$/)
  end

  class Exception < StandardError
    def self.abort code
      raise new(to_string(code), code)
    end

    attr_reader :code

    def initialize str, code
      @code = code
      super(str)
    end
  end
end
