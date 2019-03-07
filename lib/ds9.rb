require 'ds9.so'
require 'stringio'
require 'ds9/version'

module DS9
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

      def end_stream?
        flags & Flags::END_STREAM > 0
      end
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

      def ping_ack?
        (flags & Flags::ACK) > 0
      end
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
      def request?;       category == REQUEST; end
      def response?;      category == RESPONSE; end
      def push_response?; category == PUSH_RESPONSE; end
    end
  end

  class Session
    # @param option [DS9::Option]
    def initialize(option: nil)
      @post_buffers = {}
      cbs = make_callbacks
      init_internals(cbs, option)
    end

    private

    def save_post_buffer id, stream
      @post_buffers[id] = stream
    end

    def remove_post_buffer id
      @post_buffers.delete id
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

  class Client
    def submit_request headers, body = nil
      case body
      when String
        body = StringIO.new body
      end
      super(headers, body)
    end
  end
end
