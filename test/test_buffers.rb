require 'helper'

class TestBuffers < Minitest::Test
  include DS9::InMemory

  def test_recv_event_returning_too_much
    server = Class.new(DS9::Server) {
      def recv_event(length) = DS9::CLIENT_MAGIC + ("\0" * length * 4)
      def send_event(string) = string.bytesize
    }.new

    ex = assert_raises(ArgumentError) { server.receive }
    assert_match(/recv_event returned \d+ bytes, more than the \d+ asked for/, ex.message)
  end

  class LongResponseServer < Server
    def on_frame_recv frame
      if frame.headers? && frame.end_stream?
        submit_response frame.stream_id, [[":status", "200"]]
      end
    end

    def on_data_source_read stream_id, length
      "x" * (length * 4)
    end
  end

  def test_on_data_source_read_returning_too_much
    client = Client.new
    server = LongResponseServer.new
    client.submit_settings []
    server.submit_settings []
    client.request REQUEST

    ex = assert_raises(ArgumentError) { pump client, server }
    assert_match(/on_data_source_read returned \d+ bytes/, ex.message)
  end

  def test_request_body_returning_too_much
    client = Client.new
    server = Server.new
    client.submit_settings []
    server.submit_settings []

    body = Object.new
    def body.read(length) = "x" * (length * 4)
    client.request [[':method', 'POST'], *REQUEST.drop(1)], body

    ex = assert_raises(ArgumentError) { pump client, server }
    assert_match(/request body's read returned \d+ bytes/, ex.message)
  end
end
