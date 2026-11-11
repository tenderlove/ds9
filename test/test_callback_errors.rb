require 'helper'

class TestCallbackErrors < Minitest::Test
  include DS9::InMemory

  class Boom < StandardError; end

  def connected
    client = Client.new
    server = Server.new
    client.submit_settings []
    server.submit_settings []
    pump client, server
    [client, server]
  end

  def test_exception_from_callback_reaches_the_caller
    client, server = connected
    server.define_singleton_method(:on_header) { |*| raise Boom, "from on_header" }

    client.request REQUEST
    data = client.mem_send
    ex = assert_raises(Boom) { server.mem_receive data }
    assert_equal "from on_header", ex.message
  end

  def test_throw_from_callback
    client, server = connected
    server.define_singleton_method(:on_header) { |*| throw :out, :thrown }

    client.request REQUEST
    data = client.mem_send
    assert_equal :thrown, catch(:out) { server.mem_receive data }
  end

  def test_session_fails_after_a_callback_raises
    client, server = connected
    raised = false
    server.define_singleton_method(:on_header) do |*args|
      unless raised
        raised = true
        raise Boom
      end
      super(*args)
    end

    client.request REQUEST
    assert_raises(Boom) { pump client, server }

    refute_predicate server, :want_read?
    refute_predicate server, :want_write?

    client.request REQUEST
    data = client.mem_send
    ex = assert_raises(DS9::Exception) { server.mem_receive data }
    assert_equal "The user callback function failed", ex.message
  end

  class RefusingClient < Client
    def before_frame_send frame
      !frame.headers?
    end
  end

  def test_session_fails_after_before_frame_send_fails
    client = RefusingClient.new
    server = Server.new
    client.submit_settings []
    server.submit_settings []
    pump client, server

    client.request REQUEST
    assert_raises(DS9::Exception) { pump client, server }

    # Carrying on used to crash when the session was freed
    3.times do
      assert_raises(DS9::Exception) do
        client.request REQUEST
        pump client, server
      end
    end
    client = nil
    GC.start
  end
end
