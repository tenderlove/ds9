# DS9

* https://github.com/tenderlove/ds9

## DESCRIPTION:

This library allows you to write HTTP/2 clients and servers.  It is a wrapper
around nghttp2.

## FEATURES/PROBLEMS:

* Needs nghttp2

## SYNOPSIS:

Here is a full client that supports server pushes:

```ruby
require 'ds9'
require 'socket'
require 'openssl'
require 'uri'

class MySession < DS9::Client
  def initialize sock
    @sock      = sock
    @complete  = []
    @in_flight = {}
    @requests  = []
    super()
  end

  def submit_request headers
    @requests << headers
    super
  end

  def on_stream_close id, errcode
    @complete << [@requests.shift, @in_flight.delete(id)]
    terminate_session DS9::NO_ERROR if @requests.empty?
  end

  def on_begin_headers frame
    @in_flight[frame.stream_id] = [[], []] if frame.headers?
    @requests << []                        if frame.push_promise?
  end

  def on_header name, value, frame, flags
    @in_flight[frame.stream_id].first << [name, value] if frame.headers?
    @requests.last << [name, value]                    if frame.push_promise?
  end

  def on_data_chunk_recv id, data, flags
    @in_flight[id].last << data
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
    while want_read? || want_write?
      rd, wr, _ = IO.select([@sock], [@sock])
      receive
      send
      yield @complete.pop if @complete.any?
    end
  end
end

uri = URI.parse ARGV[0]
socket = TCPSocket.new uri.host, uri.port
socket.setsockopt Socket::IPPROTO_TCP, Socket::TCP_NODELAY, 1

ctx               = OpenSSL::SSL::SSLContext.new
ctx.npn_protocols = [DS9::NGHTTP2_PROTO_VERSION_ID]
ctx.npn_select_cb = lambda do |protocols|
  DS9::NGHTTP2_PROTO_VERSION_ID if protocols.include?(DS9::NGHTTP2_PROTO_VERSION_ID)
end

socket            = OpenSSL::SSL::SSLSocket.new socket, ctx
socket.hostname = uri.hostname
socket.connect
socket.sync_close = true

session = MySession.new socket
session.submit_settings []

session.submit_request [
  [':method', 'GET'],
  [':path', '/'],
  [':scheme', 'https'],
  [':authority', [uri.host, uri.port].join(':')],
  ['accept', '*/*'],
  ['user-agent', 'test'],
]

session.run do |req, res|
  p req => res
end
```

## HACKING:

On OS X:

```
$ brew install nghttp2
$ gem install hoe rake-compiler
$ rake compile test
```

## REQUIREMENTS:

* Needs nghttp2

## INSTALL:

* brew install nghttp2
* gem install ds9

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
