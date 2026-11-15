require 'helper'

class TestErrorCodes < Minitest::Test
  include DS9::InMemory

  class ErrorClient < Client
    attr_reader :closed, :goaway

    def on_stream_close stream_id, error_code
      @closed = error_code
    end

    def on_frame_recv frame
      @goaway = frame if frame.goaway?
    end
  end

  # Error codes are 32-bit unsigned, and a peer can send any of them
  CODE = 0xFFFFFFFF

  def connected
    client = ErrorClient.new
    server = Server.new
    client.submit_settings []
    server.submit_settings []
    pump client, server
    client
  end

  def test_rst_stream_error_code
    client = connected
    stream_id = client.request REQUEST
    while (data = client.mem_send); end
    client.mem_receive [0, 4, 3, 0, stream_id, CODE].pack("nCCCNN")
    assert_equal CODE, client.closed
  end

  def test_goaway_error_code
    client = connected
    client.mem_receive [0, 8, 7, 0, 0, 0, CODE].pack("nCCCNNN")
    assert_equal CODE, client.goaway.error_code
  end
end
