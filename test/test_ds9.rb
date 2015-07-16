require 'helper'

class TestDS9 < Minitest::Test
  def test_sanity
    assert DS9::PROTO_VERSION_ID
  end

  def test_session_allocation
    assert DS9::Server.new
  end

  def test_session_submit_settings
    session = DS9::Server.new
    session.submit_settings [[DS9::Settings::MAX_CONCURRENT_STREAMS, 100]]
  end

  def test_send
    called = false
    session = Class.new(DS9::Server) {
      define_method :send_event do |string|
        called = string
        string.length
      end
    }.new

    session.submit_settings [[DS9::Settings::MAX_CONCURRENT_STREAMS, 100]]
    session.send
    assert called
  end

  def test_want_read?
    session = DS9::Server.new
    session.submit_settings [[DS9::Settings::MAX_CONCURRENT_STREAMS, 100]]
    assert_predicate session, :want_write?
  end

  def test_want_write?
    session = DS9::Server.new
    session.submit_settings [[DS9::Settings::MAX_CONCURRENT_STREAMS, 100]]
    assert_predicate session, :want_write?

    session = DS9::Server.new
    refute_predicate session, :want_write?
  end
end
