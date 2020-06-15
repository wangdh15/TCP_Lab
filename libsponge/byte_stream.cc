#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _buff(), _cap(capacity), _byte_read(0), _byte_written(0), _end_input(false) {}

size_t ByteStream::write(const string &data) {
    size_t cnt = 0;
    while (cnt < data.size() && _buff.size() < _cap) {
        _buff.push_back(data[cnt]);
        _byte_written++;
        cnt++;
    }
    return cnt;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ans = "";
    for (size_t i = 0; i < min(len, _buff.size()); i++) {
        ans += _buff[i];
    }
    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t cnt = 0;
    while (cnt < len && !_buff.empty()) {
        _buff.pop_front();
        _byte_read++;
        cnt++;
    }
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buff.size(); }

bool ByteStream::buffer_empty() const { return _buff.empty(); }

bool ByteStream::eof() const { return _buff.empty() && _end_input; }

size_t ByteStream::bytes_written() const { return _byte_written; }

size_t ByteStream::bytes_read() const { return _byte_read; }

size_t ByteStream::remaining_capacity() const { return _cap - _buff.size(); }
