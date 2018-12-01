# DS9

* https://github.com/tenderlove/ds9

## DESCRIPTION:

This library allows you to write HTTP/2 clients and servers.  It is a wrapper
around nghttp2.

## INSTALLING

Add this line to your application's Gemfile:

```ruby
gem ds9
```

And then execute:

```shell
$ bundle install
```

Or install it yourself as:

```shell
$ gem install ds9
```

To install this gem without nghttp2:

Use `DS9_USE_SYSTEM_LIBRARIES` ENV var:

```shell
$ DS9_USE_SYSTEM_LIBRARIES=1 bundle install
```

or, the `--use-system-libraries` option:

## SYNOPSIS:

Here is a full client that supports server pushes:

```ruby
##
# ruby client.rb https://nghttp2.org/
# ruby client.rb https://www.google.com/

require 'ds9'
require 'socket'
require 'openssl'
require 'stringio'
require 'uri'
require 'thread'

class MySession < DS9::Client
  def initialize sock
    @sock      = sock
    @responses = Queue.new
    @requests  = []
    @in_flight = {}
    super()
  end

  def on_stream_close id, errcode
    @responses << @in_flight.delete(id)
    puts "FINISHED READING STREAM: #{id}"
    if @requests.any?
      submit_request @requests.pop
    else
      @responses << nil
      @thread.join
      terminate_session DS9::NO_ERROR
    end
  end

  def on_begin_headers frame
    @in_flight[frame.stream_id] = StringIO.new if frame.headers?
    @requests << [] if frame.push_promise?
  end

  def on_header name, value, frame, flags
    if frame.push_promise?
      @requests.last << [name, value]
    else
      puts "HEADER: #{name}: #{value}"
    end
  end

  def on_data_chunk_recv id, data, flags
    @in_flight[id] << data
  end

  def send_event string
    @sock.write_nonblock string
  end

  def recv_event length
    case data = @sock.read_nonblock(length, nil, exception: false)
    when :wait_readable then DS9::ERR_WOULDBLOCK
    when nil            then DS9::ERR_EOF
    else
      data
    end
  end

  def run
    @thread = Thread.new do
      while response = @responses.pop
        yield response
      end
    end

    while want_read? || want_write?
      rd, wr, _ = IO.select([@sock], [@sock])
      receive
      send
    end
  end
end

uri = URI.parse ARGV[0]
socket = TCPSocket.new uri.host, uri.port

ctx               = OpenSSL::SSL::SSLContext.new
ctx.npn_protocols = [DS9::PROTO_VERSION_ID]
ctx.npn_select_cb = lambda do |protocols|
  if protocols.include?(DS9::PROTO_VERSION_ID)
    puts "The negotiated protocol: " + DS9::PROTO_VERSION_ID
    DS9::PROTO_VERSION_ID
  end
end

socket            = OpenSSL::SSL::SSLSocket.new socket, ctx
socket.hostname = uri.hostname
socket.connect
socket.sync_close = true

session = MySession.new socket
session.submit_settings []

path = uri.path == '' ? '/' : uri.path
session.submit_request [
  [':method', 'GET'],
  [':path', path],
  [':scheme', uri.scheme],
  [':authority', [uri.host, uri.port].join(':')],
  ['accept', '*/*'],
  ['user-agent', 'test'],
]

session.run do |res|
  p res
end
```

## HACKING:

```
$ bundle install
$ bundle exec rake compile test
```

## LICENSE:

(The MIT License)

Copyright (c) 2015 Aaron Patterson

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
