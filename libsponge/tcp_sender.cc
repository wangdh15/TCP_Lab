#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(static_cast<size_t>(_initial_retransmission_timeout)) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (_fin_sent)
        return;
    uint64_t window_begin, window_end;
    if (_cache.size())
        window_begin = unwrap(_cache.front().header().seqno, _isn, _next_seqno);
    else
        window_begin = _next_seqno;
    window_end = window_begin + (_window_size_sender ? _window_size_sender : 1) - 1;
    uint64_t cur = _next_seqno;
    while (cur <= window_end && (!_stream.buffer_empty() || _stream.eof() || !_syn_sent)) {
        TCPSegment seg;
        if (!_syn_sent) {
            seg.header().syn = true;
            _syn_sent = true;
        }
        size_t cur_len = seg.length_in_sequence_space();
        size_t remain_len = min(TCPConfig::MAX_PAYLOAD_SIZE, window_end - cur + 1) - cur_len;
        string pay_load = _stream.read(remain_len);
        if (pay_load.size() + 1 <= remain_len && _stream.eof()) {
            seg.header().fin = true;
            _fin_sent = true;
        }
        seg.payload() = Buffer(static_cast<string &&>(pay_load));
        seg.header().seqno = next_seqno();
        _next_seqno += seg.length_in_sequence_space();
        cur += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
        _segments_out.push(seg);
        _cache.push(seg);
        if (_cache.size() == 1)
            _timer.start();
        if (_fin_sent)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_seq = unwrap(ackno, _isn, _next_seqno);
    if (abs_seq > _next_seqno)
        return false;
    bool flag = false;
    while (_cache.size()) {
        TCPSegment ele = _cache.front();
        uint64_t abs_seq_ele = unwrap(ele.header().seqno, _isn, _next_seqno);
        if (abs_seq_ele < abs_seq) {
            flag = true;
            _bytes_in_flight -= ele.length_in_sequence_space();  // 维护已发出但是还没有得到确认的字节数
            _cache.pop();
        } else {
            break;
        }
    }
    _window_size_sender = window_size;
    // 说明有数据被确认，那么就需要处理计时器
    if (flag) {
        _timer.end();
        // 将超时间隔设为初始值
        _timer.reset_RTO();
        _consecutive_retransmissions = 0;  // 重传次数清零
        if (_cache.size())
            _timer.start();  // 当还有数据没有被确认时，才重启定时器。
    }
    // 尝试发送数据
    fill_window();
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.is_open())
        return;
    if (_timer.tick(ms_since_last_tick)) {
        _timer.end();
        _timer.double_RTO();
        _segments_out.push(_cache.front());
        _consecutive_retransmissions++;
        _timer.start();
    }
}

//! 返回当前连续重传的次数
unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

//! 发送空段，仅用来向对方发送ACK的信息，不携带任何数据，
//! 也不占用序号
void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
