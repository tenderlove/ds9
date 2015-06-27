require 'minitest/autorun'
require 'gorby'

class TestClient < Minitest::Test
  attr_reader :session, :target

  class Target
    attr_reader :recording

    def initialize
      @recording = []
    end

    def send_callback string
      @recording << [__method__, string]
    end
  end

  def setup
    @queue = log
    super
  end

  class Request < Struct.new(:headers); end

  def test_request
    session = Class.new(Gorby::Client) {
      attr_reader :queue, :data

      def initialize queue, tc
        @queue = queue
        @tc = tc
        @data = []
        super()
      end

      def on_data_chunk_recv id, data, flags
        @data << data
      end

      def on_stream_close id, err
        terminate_session Gorby::NO_ERROR
      end

      def send_event string
        event = queue.shift
        @tc.assert_equal :WRITE, event.keys.first
        super
      end

      def recv_event length
        @tc.assert_equal :READ, queue.first.keys.first
        event = queue.shift
        event[:READ]
      end
    }.new(log, self)

    session.submit_settings []
    id = session.submit_request Request.new([
      [':method', 'GET'],
      [':path', '/'],
      [':scheme', 'https'],
      [':authority', ['localhost', '8080'].join(':')],
      ['accept', '*/*'],
      ['user-agent', 'test'],
    ])

    assert_equal 1, id
    while session.want_read? || session.want_write?
      session.receive if session.want_read?
      session.send if session.want_write?
    end

    assert_predicate session.queue, :empty?
    assert_match(/Not Found/, session.data.first)
  end

  def log
    [
      {:READ=>"\x00\x00\x06\x04\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00d"},
      {:READ=>-504},
      {:WRITE=>"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"},
      {:WRITE=>"\x00\x00\x00\x04\x00\x00\x00\x00\x00"},
      {:WRITE=>"\x00\x00\x00\x04\x01\x00\x00\x00\x00"},
      {:WRITE=>"\x00\x00\x19\x01\x05\x00\x00\x00\x01\x82\x84\x87A\x8A\xA0\xE4\x1D\x13\x9D\t\xB8\xF0\x1E\aS\x03*/*z\x83IP\x9F"},
      {:READ=>"\x00\x00\x00\x04\x01\x00\x00\x00\x00"},
      {:READ=>-504},
      {:READ=>-504},
      {:READ=>"\x00\x00>\x01\x04\x00\x00\x00\x01\x8Dv\x8F\xAAi\xD2\x9A\xE4R\xA9\xA7Jk\x13\x01\\\v\x8Ba\x96\xDC4\xFD('T\xCBmJ\b\x01m@\x02\xE0]\xB8\x00)\x8BF\xFF_\x92I|\xA5\x89\xD3M\x1Fj\x12q\xD8\x82\xA6\x0E\e\xF0\xAC\xF7\x00\x00\x92\x00\x01\x00\x00\x00\x01<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><hr><address>nghttpd nghttp2/1.0.2 at port 8080</address></body></html>"},
      {:READ=>-504},
      {:WRITE=>"\x00\x00\b\a\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"},
    ]
  end
end
