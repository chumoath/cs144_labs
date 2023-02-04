#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _buffer(make_shared<deque<char>>()), _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t num = 0;
    for (auto c : data) {
        if (_buffer->size() != _capacity)
            _buffer->push_back(c), ++num;
        else
            break;
    }

    _bytes_written += num;

    return num;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (len <= 0)
        return string();

    return string(_buffer->begin(), _buffer->begin() + len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len <= 0)
        return;

    size_t num;

    if (len < buffer_size())
        num = len;
    else
        num = buffer_size();

    for (size_t i = 0; i < num; ++i)
        _buffer->pop_front();

    // 也是一样的道理，pop 可能独立执行
    _bytes_read += num;

    // 应该放在 pop 中，因为 read 也是调用 pop，pop 可能被直接调用
    // 每次读完后，判断是否是 eof(error，这依赖于读，只要设置了 input_end，就要先检查是否为空，为空，立刻设置 eof，即
    // 不允许读了)
    //        如果 还有数据，则由 read 后 设置 eof
    if (_input_end && buffer_empty())
        _eof = true;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (len <= 0)
        return "";
    string ret;
    size_t num;
    if (len < buffer_size())
        ret = string(_buffer->begin(), _buffer->begin() + len), num = len;
    else
        ret = string(_buffer->begin(), _buffer->end()), num = buffer_size();

    pop_output(num);

    return ret;
}
//! Signal that the byte stream has reached its ending
// writer
void ByteStream::end_input() {
    _input_end = true;
    if (buffer_empty())
        _eof = true;
}

bool ByteStream::input_ended() const { return _input_end; }

size_t ByteStream::buffer_size() const { return _buffer->size(); }

bool ByteStream::buffer_empty() const { return _buffer->empty(); }

bool ByteStream::eof() const { return _eof; }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer->size(); }
