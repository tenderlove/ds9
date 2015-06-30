require 'helper'

class TestClient < DS9::TestCase
  def test_request
    body = 'omglolwut'

    rd, wr, server = make_server do |req, res|
      res.submit_response [[":status", '200'],
                           ["server", 'test server'],
                           ["date", 'Sat, 27 Jun 2015 17:29:21 GMT'],
                           ["X-Whatever", "blah"]]
      res.finish body
    end

    client = make_client(rd, wr) do |session|
      session.submit_request [
        [':method', 'GET'],
        [':path', '/'],
        [':scheme', 'https'],
        [':authority', ['localhost', '8080'].join(':')],
        ['accept', '*/*'],
        ['user-agent', 'test'],
      ]
    end

    _, _body = run_loop server, client
    assert_equal body, _body.join

    assert_finish server, client
  end
end
