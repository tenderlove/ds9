require 'helper'

class TestClient < DS9::TestCase
  def test_blow_up
    rd, wr, server = make_server { |req, res| }
    client = make_client rd, wr
    wr.close

    ex = assert_raises(DS9::Exception) do
      server.receive
    end
    assert ex.code
  end

  def test_submit_shutdown
    rd, wr, server = make_server { |req, res| }
    client = make_client rd, wr

    server.submit_shutdown
    server.send
    client.receive
    assert_predicate client.frames.last, :goaway?
  end

  def test_submit_goaway
    rd, wr, server = make_server { |req, res| }
    client = make_client rd, wr

    server.submit_goaway 0, 0
    server.send
    client.receive
    assert_predicate client.frames.last, :goaway?
  end

  def test_stream_remote_closed?
    _, _, server = make_server { |req, res| }
    refute server.stream_remote_closed? 0
  end

  def test_stream_local_closed?
    _, _, server = make_server { |req, res| }
    refute server.stream_local_closed? 0
  end

  def test_ping
    rd, wr, server = make_server { |req, res| }
    client = make_client rd, wr

    server.submit_ping
    server.send
    client.receive
    assert_predicate client.frames.last, :ping?
  end

  def test_push
    body = 'omglolwut'

    rd, wr, server = make_server do |req, res|
      res.push([[':method', 'GET'],
                [':path', '/omglol'],
                [':scheme', 'http'],
                [':authority', 'localhost:8080']]) do |stream|
                  stream.submit_response [[":status", '200'],
                                       ["server", 'test server'],
                                       ["date", 'Sat, 27 Jun 2015 17:29:21 GMT'],
                                       ["X-Whatever", "blah"]]
                  stream.finish 'lololol'
                end

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

    _, _body = run_loop server, client
    assert_equal 'lololol', _body.join

    assert_finish server, client
  end

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
