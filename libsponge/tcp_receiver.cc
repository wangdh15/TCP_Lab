#include "tcp_receiver.hh"
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    // 如果没有ISN，则只有抱的SYN位为True才会接收
    if (!_isn_set) {
        if (header.syn) {
            _isn_set = true;
            _isn = header.seqno;
            _ackno = _isn + 1;
        } else
            return false;
    }
    if (header.fin) _fin_set = true;

    uint64_t win_begin = unwrap(_ackno, _isn, _checkpoint);
    uint64_t win_end;
    if (window_size())
        win_end = win_begin + window_size();
    else
        win_end = win_begin + 1;
    uint64_t seq_begin = unwrap(header.seqno, _isn, _checkpoint);
    uint64_t seq_end = seq_begin + seg.length_in_sequence_space();
    uint64_t data_begin = seq_begin - 1;
    uint64_t data_end = seq_end;
    if (header.syn) data_end -= 1;
    if (header.fin) data_end -= 1;
    bool fall_inside;
    cout << seq_end << " " << seq_begin << " " << win_begin << " " << win_end << endl;
    if (seq_end < win_begin || seq_begin > win_end)
        fall_inside = false;
    else
        fall_inside = true;
    if (fall_inside) {
        _reassembler.push_substring(seg.payload().copy(), static_cast<size_t> (data_begin), header.fin);
        _checkpoint = seq_begin;
    }
    return fall_inside;
}

// 如果还没有收到isn, 则返回空
// 否则返回当前的ack号。
optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn_set)
        return nullopt;
    else
        return _ackno;
}

// 接收方的窗长度
// 由于streamout的capacity相同，
// 所以直接返回remain_capacity方法即可
size_t TCPReceiver::window_size() const {
    return stream_out().remaining_capacity();
}
