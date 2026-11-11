require 'helper'

class TestHeaders < Minitest::Test
  include DS9::InMemory

  def exchange headers
    client = Client.new
    server = Server.new
    client.submit_settings []
    server.submit_settings []
    client.request headers
    pump client, server
    server.requests[1][:headers]
  end

  def test_names_are_us_ascii_and_values_binary
    name, value = exchange(REQUEST + [["x-name", "value"]]).last
    assert_equal ["x-name", "value"], [name, value]
    assert_equal Encoding::US_ASCII, name.encoding
    assert_equal Encoding::BINARY, value.encoding
  end

  def test_non_ascii_header_value
    _, value = exchange(REQUEST + [["x-name", "café".b]]).last
    assert_equal "café".b, value
    assert_equal Encoding::BINARY, value.encoding
    assert_predicate value, :valid_encoding?
  end
end
