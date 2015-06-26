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

  class Session

    private

    def send_callback string
      # not sure what to do with this yet
      string.length
    end

    def on_frame_send frame
    end

    #def recv_callback length
    #  raise NotImplementedError
    #end
  end
end
