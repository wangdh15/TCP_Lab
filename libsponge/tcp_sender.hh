#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! 定时器类
class Timer {
  private:
    //! 当前的时间
    size_t _cur_time;

    //! 初始的RTO
    size_t _init_RTO;

    //! 目标的RTO
    size_t _cur_RTO;

    //! 是否处于开启状态
    bool _is_open;

  public:
    Timer(unsigned int init_RTO) : _cur_time(0), _init_RTO(init_RTO), _cur_RTO(init_RTO), _is_open(false) {}

    //! 判断是否处于开启状态
    bool is_open() { return _is_open; }

    //! 启动计时器
    void start() {
        _is_open = true;
        _cur_time = 0;
    }

    //! 停止计时器
    void end() {
        _is_open = false;
        _cur_time = 0;
    }

    //! 将超时间隔加倍
    void double_RTO() { _cur_RTO *= 2; }

    //! 重置RTO
    void reset_RTO() { _cur_RTO = _init_RTO; }

    //! 返回调用此次tick是否已经超时
    bool tick(const size_t ms_since_last_tick) {
        if (!is_open())
            return false;
        if (ms_since_last_tick >= _cur_RTO - _cur_time) {
            return true;
        } else {
            _cur_time += ms_since_last_tick;
            return false;
        }
    }
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    //! 存储当前连续重传的次数
    unsigned int _consecutive_retransmissions{0};

    //! 存储已发送，但是还没有被确认的数据
    std::queue<TCPSegment> _cache{};

    //! 定时器
    Timer _timer;

    //! 记录现在还有多少已发出但是没有得到确认的字节，(包括SYN和ACK)
    uint64_t _bytes_in_flight{0};

    //! 发送方当前的窗口大小，用于full_window函数使用
    uint16_t _window_size_sender{1};

    //! 是否发送过SYN
    bool _syn_sent{false};

    //! 是否发送过FIN
    bool _fin_sent{false};

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    bool ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
