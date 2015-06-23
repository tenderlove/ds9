require 'minitest/autorun'
require 'gorby'

class TestGorby < Minitest::Test
  def test_sanity
    assert Gorby::NGHTTP2_PROTO_VERSION_ID
  end

  def test_session_allocation
    assert Gorby::Session.new
  end
end
