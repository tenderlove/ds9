require 'helper'

class TestServer < DS9::TestCase
  def test_submit_response
    server, client = pipe do |req, res|
      refute req.stream.stream_remote_closed?(req.stream_id)
      refute req.stream.stream_local_closed?(req.stream_id)
      res.submit_response([[':status', '200'],
                           ['server', 'test server'],
                           ['date', 'Sat, 27 Jun 2015 17:29:21 GMT']])
      res.finish('omglol')
    end

    s = Thread.new { server.run }
    c = Thread.new { client.run }

    client.submit_request([[':method', 'GET'],
                           [':path', '/'],
                           [':scheme', 'https'],
                           [':authority', ['localhost', '8080'].join(':')],
                           ['accept', '*/*'],
                           ['user-agent', 'test']])

    responses = []
    while (response = client.responses.pop)
      responses << response
      if responses.length == 1
        client.terminate_session DS9::NO_ERROR
      end
    end

    s.join
    c.join
    assert_equal ['omglol'], responses.map(&:body).map(&:string)
  end

  def test_submit_headers
    server, client = pipe do |req, res|
      refute req.stream.stream_remote_closed?(req.stream_id)
      refute req.stream.stream_local_closed?(req.stream_id)
      res.submit_headers([[':status', '200'],
                          ['server', 'test server'],
                          ['date', 'Sat, 27 Jun 2015 17:29:21 GMT']])
    end

    s = Thread.new { server.run }
    c = Thread.new { client.run }

    client.submit_request([[':method', 'GET'],
                           [':path', '/'],
                           [':scheme', 'https'],
                           [':authority', ['localhost', '8080'].join(':')],
                           ['accept', '*/*'],
                           ['user-agent', 'test']])

    responses = []
    while (response = client.responses.pop)
      responses << response
      if responses.length == 1
        client.terminate_session DS9::NO_ERROR
      end
    end

    s.join
    c.join
    assert_equal [''], responses.map(&:body).map(&:string)
  end
end
