require 'minitest/autorun'
require 'gorby'

class TestGorby < Minitest::Test
  def test_sanity
    assert Gorby::NGHTTP2_PROTO_VERSION_ID
  end

  def test_session_allocation
    assert Gorby::Session.new
  end

  def test_session_submit_settings
    session = Gorby::Session.new
    session.submit_settings [[Gorby::Settings::MAX_CONCURRENT_STREAMS, 100]]
  end
end
