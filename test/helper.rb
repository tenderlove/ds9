require 'minitest/autorun'
require 'gorby'

module Gorby
  class TestCase < Minitest::Test
    def make_server settings = [], &block
      rd1, wr1 = IO.pipe
      rd2, wr2 = IO.pipe

      server = Server.new rd1, wr2, block
      server.submit_settings settings
      [rd2, wr1, server]
    end

    def make_client rd, wr, settings = []
      client = Client.new rd, wr, []
      client.submit_settings settings
      yield client
      client
    end

    def run_loop server, client
      while server.want_read? || server.want_write?
        break client.responses.shift if client.responses.any?

        client.send
        client.receive
        server.send
        server.receive
      end
    end

    def assert_finish server, client
      client.terminate_session Gorby::NO_ERROR

      while server.want_read? || server.want_write?
        client.send
        client.receive
        server.send
        server.receive
      end
    end

    module IOEvents
      attr_reader :reader, :writer

      def initialize reader, writer
        @reader = reader
        @writer = writer
        super()
      end

      def send_event string
        writer.write_nonblock string
      end

      def recv_event length
        case data = reader.read_nonblock(length, nil, exception: false)
        when :wait_readable
          Gorby::ERR_WOULDBLOCK
        when nil
          Gorby::ERR_EOF
        else
          data
        end
      end
    end

    class Client < Gorby::Client
      include IOEvents

      attr_reader :responses, :response_queue

      def initialize read, write, response_queue
        @response_streams = {}
        @responses        = response_queue
        super(read, write)
      end

      def on_begin_headers frame
        @response_streams[frame.stream_id] = [[], []]
      end

      def on_header name, value, frame, flags
        @response_streams[frame.stream_id].first << [name, value]
      end

      def on_stream_close id, err
        @responses << @response_streams.delete(id)
      end

      def on_data_chunk_recv stream_id, data, flags
        @response_streams[stream_id].last << data
      end
    end

    class Server < Gorby::Server
      include IOEvents

      def initialize read, write, rack_app
        @rack_app      = rack_app
        @read_streams  = {}
        @write_streams = {}
        super(read, write)
      end

      def on_begin_headers frame
        @read_streams[frame.stream_id] = []
      end

      def on_data_source_read stream_id, length
        @write_streams[stream_id].body.shift
      end

      def on_stream_close id, error_code
        @read_streams.delete id
        @write_streams.delete id
      end

      def on_header name, value, frame, flags
        @read_streams[frame.stream_id] << [name, value]
      end

      class Response < Struct.new :stream, :stream_id, :body
        def submit_response headers
          stream.submit_response stream_id, headers
        end

        def finish str
          body << str
          body << nil
        end
      end

      def on_frame_recv frame
        return unless frame.headers?

        req_headers = @read_streams[frame.stream_id]

        response = Response.new(self, frame.stream_id, [])

        @rack_app.call req_headers, response

        @write_streams[frame.stream_id] = response
      end
    end
  end
end
