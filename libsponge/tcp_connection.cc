#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

//! 收到一个数据的操作
void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;

    //  当前处于关闭状态，且收到了一个来自对方的链接，
    // 结束之后自己的rec和sen都应该就绪了
    if (_cur_state == stat::CLOSED && seg.header().syn) {
        _cur_state = stat::SYN_RECEIVED;
        _receiver.segment_received(seg);  // 将接受到的传递给rec，设置好rec的各个属性
        // 假装接受了一个ack字段，仅仅用来设置窗口，而且其内部也会调用fill_window方法，发送SYN + ACK数据。
        _sender.ack_received(WrappingInt32{0}, seg.header().win);
        tmp_sent(); // 将sen发的内容放入connection的输出队列中
    } else if (_cur_state == stat::SYN_SENT && seg.header().syn) { // 客户端收到了来自服务器的同步确认字段
        _cur_state = stat::ESTABLISHED;
        _sender.ack_received(seg.header().ackno, seg.header().win); // 将服务器发来的SYN + ACK传递给自己的send，设置好窗口
        _receiver.segment_received(seg);  // 将受到的传给自己的rec。
        tmp_sent();
    } else if (_cur_state == stat::SYN_RECEIVED) { // 服务器端收到了第三次握手的数据
        if (seg.header().fin) {
            _cur_state = stat::FIN_WAIT1;
            _sender.ack_received(seg.header().ackno, seg.header().win);
        } else if (seg.header().ack) {
            _cur_state = stat::ESTABLISHED;
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }
        _receiver.segment_received(seg);
    } else if (_cur_state == stat::ESTABLISHED) {
        if (seg.header().fin) {
            _cur_state = stat::FIN_WAIT1;
        }
        if (seg.header().ack) _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
    }
//    if (seg.header().ack) {
//        _sender.ack_received(seg.header().ackno, seg.header().win);
//        tmp_sent();
//    }
//    _receiver.segment_received(seg);

}

bool TCPConnection::active() const { return false; }

//! 写数据的操作
size_t TCPConnection::write(const string &data) {
    size_t write_succ = _sender.stream_in().write(data);
    _sender.fill_window();
    tmp_sent();
    return write_succ;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    tmp_sent();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    tmp_sent();
}

void TCPConnection::connect() {
    //! 跳转到SYN_SENT状态
    if (_cur_state != stat::CLOSED) return;
    _cur_state = stat::SYN_SENT;
    _sender.fill_window();
    tmp_sent();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

//! 查看sender的队列中是否有数据，
//! 如果有的话则发送出去
//! 同时需要封装ack标志位和ackno
void TCPConnection::tmp_sent() {
    while (_sender.segments_out().size()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        auto ack = _receiver.ackno();
        if (ack.has_value()) {
            seg.header().ack = true;
            seg.header().ackno = ack.value();
        } else seg.header().ack = false;
        // 如果接受方的窗口字段的最大值还大，则转为窗口字段的最大值。
        seg.header().win = min(_receiver.window_size(), static_cast<size_t>(numeric_limits<uint16_t>::max()));
        if (seg.header().fin) {
            if (_cur_state == stat::ESTABLISHED) _cur_state = stat::FIN_WAIT1;
            else if (_cur_state == stat::CLOSED_WAIT) _cur_state = stat::LAST_ACK;
        }
        _segments_out.push(seg);
    }
}