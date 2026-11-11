require 'helper'

class TestGC < Minitest::Test
  include DS9::InMemory

  def setup
    skip "GC compaction isn't supported" unless GC.respond_to?(:verify_compaction_references)
  end

  def test_callbacks_reach_the_session_after_compaction
    sent = []
    sessions = 100.times.map do
      Class.new(DS9::Server) {
        define_method(:send_event) { |string| sent << self; string.bytesize }
      }.new.tap { |s| s.submit_settings [] }
    end

    GC.verify_compaction_references(expand_heap: true, toward: :empty)

    sessions.each(&:send)
    assert_equal sessions.map(&:object_id), sent.map(&:object_id)
  end

  def test_request_body_survives_compaction
    client = Client.new
    server = Server.new
    client.submit_settings []
    server.submit_settings []

    body = StringIO.new("x" * 100)
    client.request [[':method', 'POST'], *REQUEST.drop(1)], body
    body = nil

    GC.verify_compaction_references(expand_heap: true, toward: :empty)

    pump client, server
    assert_equal "x" * 100, server.requests[1][:body]
  end
end
