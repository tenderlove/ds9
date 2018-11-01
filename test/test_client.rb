require 'helper'

class TestClient < DS9::TestCase
  def test_outbound_queue_size
    server ,= pipe { |req, res| }

    assert_equal 1, server.outbound_queue_size
    server.submit_shutdown
    assert_operator server.outbound_queue_size, :>, 1
  end

  def test_blow_up
    server, client = pipe { |req, res| }
    client.writer.close

    ex = assert_raises(DS9::Exception) do
      server.receive
    end
    assert ex.code
  end

  def test_submit_shutdown
    server, client = pipe { |req, res| }

    server.submit_shutdown
    server.send
    client.receive
    assert_predicate client.frames.last, :goaway?
  end

  def test_submit_goaway
    server, client = pipe { |req, res| }
    server.submit_goaway 0, 0
    server.send
    client.receive
    assert_predicate client.frames.last, :goaway?
  end

  def test_stream_remote_closed?
    server, client = pipe do |req, res|
      refute req.stream.stream_remote_closed? req.stream_id
      refute req.stream.stream_local_closed? req.stream_id
      res.submit_response [[":status", '200'],
                           ["server", 'test server'],
                           ["date", 'Sat, 27 Jun 2015 17:29:21 GMT'],
                           ["X-Whatever", "blah"]]
      res.finish 'omglol'
    end

    s = Thread.new { server.run }
    c = Thread.new { client.run }

    client.submit_request [
      [':method', 'GET'],
      [':path', '/'],
      [':scheme', 'https'],
      [':authority', ['localhost', '8080'].join(':')],
      ['accept', '*/*'],
      ['user-agent', 'test'],
    ]

    responses = []
    while response = client.responses.pop
      responses << response
      if responses.length == 1
        client.terminate_session DS9::NO_ERROR
      end
    end

    s.join
    c.join
  end

  def test_ping
    server, client = pipe { |req, res| }

    server.submit_ping
    server.send
    client.receive
    assert_predicate client.frames.last, :ping?
    client.send
    server.receive
    assert_predicate server.frames.last, :ping?
    assert_predicate server.frames.last, :ping_ack?
  end

  def test_push
    body = 'omglolwut'

    server, client = pipe do |req, res|
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

    s = Thread.new { server.run }
    c = Thread.new { client.run }

    client.submit_request [
      [':method', 'GET'],
      [':path', '/'],
      [':scheme', 'https'],
      [':authority', ['localhost', '8080'].join(':')],
      ['accept', '*/*'],
      ['user-agent', 'test'],
    ]

    responses = []
    while response = client.responses.pop
      responses << response
      if responses.length == 2
        client.terminate_session DS9::NO_ERROR
      end
    end

    s.join
    c.join
    assert_equal ["omglolwut", "lololol"], responses.map(&:body).map(&:string)
  end

  def test_post
    body = 'omglolwut'

    server, client = pipe do |req, res|
      case req.path
      when '/'
        res.submit_response [[":status", '200'],
                             ["server", 'test server'],
                             ["date", 'Sat, 27 Jun 2015 17:29:21 GMT'],
                             ["X-Whatever", "blah"]]
        res.finish req.body
      end
    end

    s = Thread.new { server.run }
    c = Thread.new { client.run }

    client.submit_request [
      [':method', 'POST'],
      [':path', '/'],
      [':scheme', 'https'],
      [':authority', ['localhost', '8080'].join(':')],
      ['accept', '*/*'],
      ['user-agent', 'test'],
    ], body

    responses = []
    while response = client.responses.pop
      responses << response
      if responses.length == 1
        client.terminate_session DS9::NO_ERROR
      end
    end

    s.join
    c.join
    assert_equal [body], responses.map(&:body).map(&:string)
  end

  def test_post_file
    server, client = pipe do |req, res|
      case req.path
      when '/'
        res.submit_response [[":status", '200'],
                             ["server", 'test server'],
                             ["date", 'Sat, 27 Jun 2015 17:29:21 GMT'],
                             ["X-Whatever", "blah"]]
        res.finish req.body
      end
    end

    s = Thread.new { server.run }
    c = Thread.new { client.run }

    client.submit_request [
      [':method', 'POST'],
      [':path', '/'],
      [':scheme', 'https'],
      [':authority', ['localhost', '8080'].join(':')],
      ['accept', '*/*'],
      ['user-agent', 'test'],
    ], File.open(__FILE__, "r")

    responses = []
    while response = client.responses.pop
      responses << response
      if responses.length == 1
        client.terminate_session DS9::NO_ERROR
      end
    end

    s.join
    c.join
    assert_equal File.read(__FILE__), responses.map(&:body).map(&:string).first
  end

  def test_request
    body = 'omglolwut'

    server, client = pipe do |req, res|
      res.submit_response [[":status", '200'],
                           ["server", 'test server'],
                           ["date", 'Sat, 27 Jun 2015 17:29:21 GMT'],
                           ["X-Whatever", "blah"]]
      res.finish body
    end

    client.submit_request [
      [':method', 'GET'],
      [':path', '/'],
      [':scheme', 'https'],
      [':authority', ['localhost', '8080'].join(':')],
      ['accept', '*/*'],
      ['user-agent', 'test'],
    ]

    s = Thread.new { server.run }
    c = Thread.new { client.run }

    responses = []
    while response = client.responses.pop
      responses << response
      if responses.length == 1
        client.terminate_session DS9::NO_ERROR
      end
    end

    s.join
    c.join
    assert_equal ["omglolwut"], responses.map(&:body).map(&:string)
  end

  def test_request_response_with_hashes
    body = 'omglolwut'

    req_hash = {
      ':method'    => 'GET',
      ':path'      => '/',
      ':scheme'    => 'https',
      ':authority' => ['localhost', '8080'].join(':'),
      'accept'     => '*/*',
      'user-agent' => 'test',
    }

    res_hash = {
      ":status"    => '200',
      "server"     => 'test server',
      "date"       => 'Sat, 27 Jun 2015 17:29:21 GMT',
      "x-whatever" => "blah"
    }

    server, client = pipe do |req, res|
      assert_equal req_hash, req.headers

      res.submit_response res_hash
      res.finish body
    end

    client.submit_request req_hash

    s = Thread.new { server.run }
    c = Thread.new { client.run }

    responses = []
    while response = client.responses.pop
      responses << response
      if responses.length == 1
        client.terminate_session DS9::NO_ERROR
      end
    end

    s.join
    c.join
    assert_equal res_hash, responses.first.headers
    assert_equal ["omglolwut"], responses.map(&:body).map(&:string)
  end

  def test_trailers
    body = 'omglolwut'

    req_hash = {
      ':method'    => 'GET',
      ':path'      => '/',
      ':scheme'    => 'https',
      ':authority' => ['localhost', '8080'].join(':'),
      'accept'     => '*/*',
      'user-agent' => 'test',
    }

    res_hash = {
      ":status"    => '200',
      "server"     => 'test server',
      "date"       => 'Sat, 27 Jun 2015 17:29:21 GMT',
      "x-whatever" => "blah"
    }

    trailers = { 'x-foo' => 'bar' }

    server, client = pipe do |req, res|
      assert_equal req_hash, req.headers

      res.submit_response res_hash
      res.trailers = trailers
      res.finish body
    end

    client.submit_request req_hash

    s = Thread.new { server.run }
    c = Thread.new { client.run }

    responses = []
    while response = client.responses.pop
      responses << response
      if responses.length == 1
        client.terminate_session DS9::NO_ERROR
      end
    end

    s.join
    c.join
    assert_equal res_hash, responses.first.headers
    assert_equal trailers, responses.first.trailers
    assert_equal ["omglolwut"], responses.map(&:body).map(&:string)
  end
end
