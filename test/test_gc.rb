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

class TestGCHeaders < Minitest::Test
  include DS9::InMemory

  def setup
    skip "GC compaction isn't supported" unless GC.respond_to?(:verify_compaction_references)
  end

  # Converting it compacts the heap, moving the Strings of the headers
  # converted before it.
  class Compacting
    def to_str
      GC.verify_compaction_references(expand_heap: true, toward: :empty)
      "late"
    end
  end

  def test_headers_survive_compaction_while_submitting
    client = Client.new
    server = Server.new
    client.submit_settings []
    server.submit_settings []

    headers = [
      [":method", "GET"],
      [":path", "/#{"p" * 30}"],
      [":scheme", "http"],
      [":authority", "localhost"],
      ["x-early", "early #{"e" * 20}"],
    ]
    client.request headers + [["x-late", Compacting.new]]
    pump client, server

    assert_equal headers + [["x-late", "late"]], server.requests[1][:headers]
  end
end
