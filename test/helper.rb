require 'minitest/autorun'
require 'ds9'
require 'io/wait'
require 'thread'
require 'stringio'

Thread.abort_on_exception = true

trap("INFO") {
  Thread.list.each do |k|
    puts "#" * 90
    puts k.backtrace
    puts "#" * 90
  end
}

module DS9
  class TestCase < Minitest::Test
    def pipe &block
      rd1, wr1 = IO.pipe
      rd2, wr2 = IO.pipe

      server = Server.new rd1, wr2, block
      server.submit_settings [
        [DS9::Settings::MAX_CONCURRENT_STREAMS, 100],
      ]

      client = Client.new rd2, wr1, Queue.new
      client.submit_settings [
        [DS9::Settings::MAX_CONCURRENT_STREAMS, 100],
        [DS9::Settings::INITIAL_WINDOW_SIZE, 65535],
      ]
      [server, client]
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
          DS9::ERR_WOULDBLOCK
        when nil
          DS9::ERR_EOF
        else
          data
        end
      rescue IOError
        DS9::ERR_EOF
      end

      def run
        while want_read? || want_write?
          if want_write?
            @writer.wait_writable 0.1
            send
          end

          if want_read?
            @reader.wait_readable 0.1
            receive
          end
        end
      end
    end

    class Client < DS9::Client
      include IOEvents

      class Response
        attr_reader :stream_id, :body

        def initialize stream_id
          @stream_id = stream_id
          @headers   = [{}]
          @body      = StringIO.new
        end

        def headers
          @headers[0]
        end

        def trailers
          @headers[1]
        end

        def [] k; @headers.last[k]; end
        def []= k, v; @headers.last[k] = v; end

        def bump
          @headers << {}
        end
      end

      attr_reader :responses, :response_queue, :frames

      def initialize read, write, response_queue
        @response_streams = {}
        @responses        = response_queue
        @frames           = []
        super(read, write)
      end

      def on_frame_recv frame
        @frames << frame
      end

      def on_begin_headers frame
        if @response_streams[frame.stream_id]
          @response_streams[frame.stream_id].bump
        else
          @response_streams[frame.stream_id] = Response.new(frame.stream_id)
        end
      end

      def on_header name, value, frame, flags
        @response_streams[frame.stream_id][name] = value
      end

      def on_stream_close id, err
        @responses << @response_streams.delete(id)
      end

      def on_data_chunk_recv stream_id, data, flags
        @response_streams[stream_id].body << data
      end

      def terminate_session err
        super
        @responses << nil
      end
    end

    class Server < DS9::Server
      include IOEvents
      attr_reader :frames

      def initialize read, write, app
        @app           = app
        @read_streams  = {}
        @read_post_streams  = {}
        @write_streams = {}
        @frames        = []
        super(read, write)
      end

      def before_frame_send frame
      end

      def on_begin_headers frame
        @read_streams[frame.stream_id] = []
      end

      def on_data_source_read stream_id, length
        chunk = @write_streams[stream_id].body.shift
        if chunk.nil? && @write_streams[stream_id].trailers
          submit_trailer stream_id, @write_streams[stream_id].trailers
          false
        else
          chunk
        end
      end

      def on_stream_close id, error_code
        @read_streams.delete id
        @write_streams.delete id
      end

      def submit_push_promise stream_id, headers
        response = Response.new(self, super(stream_id, headers), [])
        request = Request.new(self, stream_id, Hash[headers])
        @app.call request, response
        @write_streams[response.stream_id] = response
      end

      def on_header name, value, frame, flags
        if name == ":method" && value == "POST"
          @read_post_streams[frame.stream_id] = []
        end
        @read_streams[frame.stream_id] << [name, value]
      end

      class Request < Struct.new :stream, :stream_id, :headers, :body
        def path
          headers[':path']
        end
      end

      class Response < Struct.new :stream, :stream_id, :body, :trailers
        def push headers
          stream.submit_push_promise stream_id, headers
        end

        def submit_response headers
          stream.submit_response stream_id, headers
        end

        def submit_headers headers
          stream.submit_headers stream_id, headers
        end

        def finish str
          body << str
          body << nil
        end
      end

      def on_data_chunk_recv id, data, flags
        @read_post_streams[id] << data
      end

      def on_frame_recv frame
        @frames << frame
        return unless (frame.data? || frame.headers?) && frame.end_stream?

        req_headers = @read_streams[frame.stream_id]

        response = Response.new(self, frame.stream_id, [])
        request = Request.new(self, frame.stream_id, Hash[req_headers])
        if @read_post_streams[frame.stream_id]
          request.body = @read_post_streams[frame.stream_id].join
        end

        @app.call request, response

        @write_streams[frame.stream_id] = response
      end

    end
  end
end
