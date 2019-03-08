require 'helper'

class TestMeme < Minitest::Test
  def setup
    @option = DS9::Option.new
  end

  def test_set_no_auto_ping_ack
    assert @option.set_no_auto_ping_ack
  end

  def test_set_no_auto_window_update
    assert @option.set_no_auto_window_update
  end

  def test_set_no_closed_streams
    assert @option.set_no_closed_streams
  end

  def test_set_no_http_messaging
    assert @option.set_no_http_messaging
  end

  def test_set_no_recv_client_magic
    assert @option.set_no_recv_client_magic
  end

  def test_set_builtin_recv_extension_type
    assert @option.set_builtin_recv_extension_type(1)
  end

  def test_set_builtin_recv_extension_type_with_not_num
    assert_raises TypeError do
      assert @option.set_builtin_recv_extension_type('invalid')
    end
  end

  def test_set_max_deflate_dynamic_table_size
    assert @option.set_max_deflate_dynamic_table_size(1)
  end

  def test_set_max_deflate_dynamic_table_size_with_not_num
    assert_raises TypeError do
      @option.set_max_deflate_dynamic_table_size('invalid')
    end
  end

  def test_set_max_send_header_block_length
    assert @option.set_max_send_header_block_length(1)
  end

  def test_set_max_send_header_block_length_with_not_num
    assert_raises TypeError do
      assert @option.set_max_send_header_block_length('invalid')
    end
  end

  def test_set_max_reserved_remote_streams
    assert @option.set_max_reserved_remote_streams(100)
  end

  def test_set_max_reserved_remote_streams_not_num
    assert_raises TypeError do
      @option.set_max_reserved_remote_streams('invalid')
    end
  end

  def test_set_peer_max_concurrent_streams
    assert @option.set_peer_max_concurrent_streams(1)
  end

  def test_set_peer_max_concurrent_stream_with_not_num
    assert_raises TypeError do
      assert @option.set_peer_max_concurrent_streams('invalid')
    end
  end

  def test_set_user_recv_extension_type
    assert @option.set_user_recv_extension_type(1)
  end

  def test_set_user_recv_extension_type_with_not_num
    assert_raises TypeError do
      assert @option.set_user_recv_extension_type('invalid')
    end
  end
end
