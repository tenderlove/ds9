require 'helper'

class TestRequestBody < Minitest::Test
  include DS9::InMemory

  # RST_STREAM(CANCEL), as a server would send for a request it won't read
  def rst_stream stream_id
    [0, 4, 3, 0, stream_id, 8].pack("nCCCNN")
  end

  def post_and_reset client, server, bodies
    body = StringIO.new("x" * 1_000_000)
    bodies[bodies.size] = body
    stream_id = client.request [[':method', 'POST'], *REQUEST.drop(1)], body
    # The body is too big to send before the flow control window fills
    while (data = client.mem_send)
      server.mem_receive data
    end
    client.mem_receive rst_stream(stream_id)
    nil
  end

  def test_body_released_when_stream_is_reset
    client = Client.new
    server = Server.new
    client.submit_settings []
    server.submit_settings []
    pump client, server

    bodies = ObjectSpace::WeakMap.new
    10.times { post_and_reset client, server, bodies }
    GC.start

    # Allow for one still being found on the stack
    assert_operator bodies.keys.size, :<=, 1
  end
end
