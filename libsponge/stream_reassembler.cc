#include "stream_reassembler.hh"

#include <iostream>
#include <list>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _no_assembled_data({})
    , _no_assembled_index(0)
    , _receive_eof(false)
    , _no_assembled_bytes(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    _receive_eof = _receive_eof || eof;
    if (data.size() == 0) {
        if (_receive_eof && _no_assembled_index == index)
            _output.end_input();
        return;
    }
    size_t begin_rcv = index, end_rcv = index + data.size() - 1;
    if (end_rcv < _no_assembled_index || begin_rcv >= _capacity + _no_assembled_index) {
        return;
    }

    list<interval> res;
    size_t cur_begin = begin_rcv, cur_end = end_rcv;
    string cur_string = data;
    bool flag = false;
    for (auto i = _no_assembled_data.begin(); i != _no_assembled_data.end(); i++) {
        auto x = *i;
        if (x.end + 1 < cur_begin)
            continue;
        else if (x.begin > cur_end + 1) {
            if (!flag) {
                _no_assembled_bytes += cur_string.size();
                _no_assembled_data.insert(i, interval(cur_begin, cur_end, cur_string));
                flag = true;
                break;
            }
        } else {
            auto t = i;
            i++;
            _no_assembled_bytes -= x.data.size();
            _no_assembled_data.erase(t);
            if (x.begin <= cur_begin && x.end <= cur_end) {
                cur_string = x.data + cur_string.substr(x.end + 1 - cur_begin);
                cur_begin = x.begin;
            } else if (x.begin >= cur_begin && x.end >= cur_end) {
                cur_string = cur_string + x.data.substr(cur_end + 1 - x.begin);
                cur_end = x.end;
            } else if (x.begin <= cur_begin && x.end >= cur_end) {
                cur_begin = x.begin;
                cur_end = x.end;
                cur_string = x.data;
            }
            i--;
        }
    }

    if (!flag) {
        _no_assembled_data.emplace_back(cur_begin, cur_end, cur_string);
        _no_assembled_bytes += cur_string.size();
    }
    if (_no_assembled_data.front().begin < _no_assembled_index) {
        _no_assembled_bytes -= _no_assembled_data.front().data.size();
        _no_assembled_data.front().data =
            _no_assembled_data.front().data.substr(_no_assembled_index - _no_assembled_data.front().begin);
        _no_assembled_data.front().begin = _no_assembled_index;
        _no_assembled_bytes += _no_assembled_data.front().data.size();
    }
    if (_no_assembled_data.back().end + 1 >= _capacity + _no_assembled_index) {
        _no_assembled_bytes -= _no_assembled_data.back().data.size();
        _no_assembled_data.back().data =
            _no_assembled_data.back().data.substr(0, _capacity + _no_assembled_index - _no_assembled_data.back().begin);
        _no_assembled_data.back().end = _capacity + _no_assembled_index - 1;
        _no_assembled_bytes += _no_assembled_data.back().data.size();
    }
    if (_no_assembled_data.front().begin == _no_assembled_index) {
        size_t out_remain_cap = _output.remaining_capacity();
        string sent_data;
        if (out_remain_cap < _no_assembled_data.front().data.size()) {
            sent_data = _no_assembled_data.front().data.substr(0, out_remain_cap);
            _no_assembled_data.front().begin += out_remain_cap;
            _no_assembled_data.front().data = _no_assembled_data.front().data.substr(out_remain_cap);
            _no_assembled_index = _no_assembled_data.front().begin;
        } else {
            sent_data = _no_assembled_data.front().data;
            _no_assembled_index = _no_assembled_data.front().end + 1;
            _no_assembled_data.pop_front();
        }
        _output.write(sent_data);
        _no_assembled_bytes -= sent_data.size();
        if (_receive_eof && _no_assembled_bytes == 0)
            _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _no_assembled_bytes; }

bool StreamReassembler::empty() const { return _no_assembled_data.size() == 0; }

interval::interval(size_t b, size_t e, std::string d) : begin(b), end(e), data(d) {}
