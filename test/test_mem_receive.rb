require 'helper'

class TestMemReceive < Minitest::Test
  include DS9::InMemory

  def test_callbacks_cant_change_the_buffer_being_read
    client = Client.new
    server = Server.new
    client.submit_settings []
    server.submit_settings []
    pump client, server

    client.request REQUEST
    buf = client.mem_send
    server.define_singleton_method(:on_header) { |*| buf.replace("x" * 100_000) }

    ex = assert_raises(RuntimeError) { server.mem_receive buf }
    assert_match(/temporarily locked/, ex.message)

    # It's unlocked again afterwards
    buf.clear
  end
end
