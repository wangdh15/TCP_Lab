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
//    _receiver.segment_received(seg); // 确定rec可以管理好自己的状态
    if (seg.header().syn) {  // 收到了一个头部带有syn的字段
        if (!_syn_rec) { // 自己之前没有收到过syn字段的数据
            _syn_rec = true;
            _receiver.segment_received(seg);  //发给自己的rec，用于设置好rec的isn
            if (seg.header().ack) {  // 带有ack字段，则表示我是客户端，收到了服务器端的回应
                _sender.ack_received(seg.header().ackno, seg.header().win);
                _sender.send_empty_segment();
                tmp_sent();
            } else { // 自己收到了一个syn，而且自己之前没有收到syn，则自己需要发送一个syn+ack
                if (_syn_sent) { // 如果自己之前发过syn，则只需要发送一个ack即可。
                    _sender.send_empty_segment();
                    tmp_sent();
                } else { // 如果自己之前没有发送过syn，则说明自己是第一次收到对面发来的syn，这个时候需要发送一个syn+ack的报文。
                    _syn_sent = true;
                    connect(); // 直接调用connect方法，就发送了一个syn + ack的报文了。
                }
            }
        }
    } else {
        if (_syn_rec && seg.header().ack) {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }
    }
//    if (seg.header().syn && !seg.header().ack && _sender.next_seqno_absolute() == 0) { // 说明当前处于CLOSED状态，收到了来自客户端的syn请求
//        _sender.ack_received(_sender.next_seqno(), seg.header().win);  // 没有ack号，传入一个和isn相同的序列号，并设置好自己的接受窗口
//    }
//    if (seg.header().ack) {
//        _sender.ack_received(seg.header().ackno, seg.header().win);  // 如果seg中的ack为true，则直接将这个发送给自己的sen
//    }
//    tmp_sent();  //尝试情况sender的发送队列。
//
//    if (seg.header().fin) { // 收到了来自对方的fin，如果自己之前发送过fin，则表示自己需要等待，如果自己之前没有发过，则不需要等待
//        if (_sender.fin_sent()) _linger_after_streams_finish = true;
//        else _linger_after_streams_finish = false;
//    }
//
//    if (_sender.fin_sent() && _sender.bytes_in_flight() == 0) {
//        if (!_linger_after_streams_finish) {
//            _finish = true;  // 表示服务器端可以直接关闭了。
//        }
//    }
}

bool TCPConnection::active() const { return true; }

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
//    if (_cur_state != stat::CLOSED) return;
//    _cur_state = stat::SYN_SENT;
    _syn_sent = true;
    _sender.fill_window();
    tmp_sent(); // 一开始的时候，自己的rec没有收到任何东西，所以这个方法中不会携带ack字段
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
        if (ack.has_value()) {  // 除了客户端第一次发送syn之外，这个字段应该都是true
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