require 'helper'

class TestClient < DS9::TestCase
  def test_outbound_queue_size
    _, _, server = make_server

    assert_equal 1, server.outbound_queue_size
    server.submit_shutdown
    assert_operator server.outbound_queue_size, :>, 1
  end

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
    rd, wr, server = make_server { |req, res|
      refute req.stream.stream_remote_closed? req.stream_id
      refute req.stream.stream_local_closed? req.stream_id
      res.submit_response [[":status", '200'],
                           ["server", 'test server'],
                           ["date", 'Sat, 27 Jun 2015 17:29:21 GMT'],
                           ["X-Whatever", "blah"]]
      res.finish 'omglol'
    }
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
      case req.path
      when '/'
        res.push([[':method', 'GET'],
                  [':path', '/omglol'],
                  [':scheme', 'http'],
                  [':authority', 'localhost:8080']])

        res.submit_response [[":status", '200'],
                             ["server", 'test server'],
                             ["date", 'Sat, 27 Jun 2015 17:29:21 GMT'],
                             ["X-Whatever", "blah"]]
      when '/omglol'
        res.submit_response [[":status", '200'],
                             ["server", 'test server'],
                             ["date", 'Sat, 27 Jun 2015 17:29:21 GMT'],
                             ["X-Whatever", "blah"]]
        res.finish 'lololol'
      end

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
