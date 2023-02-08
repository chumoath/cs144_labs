#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>
#include <assert.h>
using namespace std;

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
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t sum = 0;

    for (auto & reSeg : _wait)
        sum += reSeg.seg.length_in_sequence_space();

    return sum;
}

void TCPSender::fill_window() {
send:
    switch (state) {
    case CLOSE:
    {
        Retransmission reSeg;
        reSeg.seg.header().syn = true;
        reSeg.seg.header().seqno = _isn;

        reSeg.ddlTick = _ddlTick + _initial_retransmission_timeout;

        _segments_out.push(reSeg.seg);
        _wait.push_back(reSeg);
        _next_seqno += reSeg.seg.length_in_sequence_space();
        state = SYN_SENT;
        break;
    }
    case SYN_SENT:
    {
        break;
    }
    case SYN_ACKED:
    {
        // assemble segment
        Retransmission reSeg;
        // all data is read, eof() return true, otherwise it is not valid
        // should use input_end
        bool fin = stream_in().input_ended();
        size_t data_size = stream_in().buffer_size();
        size_t to_send_sz = data_size + fin;
        // no data to send and no need to send fin
        //         it is necessary
        if (to_send_sz == 0 || _window_size == 0) return;

        // window_size == 0 and no data to wait, send one byte avoiding can not be awaked
        // it should not here, because only receive's segment window_size is zero, then set can use is one
        //if (_window_size == 0) {
        //    if (_wait.empty()) _window_size = 1;
        //    else
        //        return;
        //}
        

        // here, size may include fin

        // MAX_PAYLOAD_SIZE only restrict the payload, not restrict the fin
        //size = to_send_sz < _window_size ? (to_send_sz < TCPConfig::MAX_PAYLOAD_SIZE ? to_send_sz : TCPConfig::MAX_PAYLOAD_SIZE) : 
        //                                   (_window_size < TCPConfig::MAX_PAYLOAD_SIZE ? _window_size : TCPConfig::MAX_PAYLOAD_SIZE);

        // data_size
        //       filter by MAX_PLAYLOAD_SIZE
        data_size = data_size < TCPConfig::MAX_PAYLOAD_SIZE ? data_size : TCPConfig::MAX_PAYLOAD_SIZE;

        //       filter by _window_size
        data_size = data_size < _window_size ? data_size : _window_size;

        // eof is already sent, data is extracted completely, data_size < _window_size
        if (_stream.input_ended() && data_size == _stream.buffer_size() && data_size < _window_size) fin = true;
        else
            fin = false;

        // here, size no include fin
        // body
        reSeg.ddlTick = _ddlTick + _initial_retransmission_timeout;
        Buffer payLoad(stream_in().read(data_size));
        reSeg.seg.payload() = payLoad;

        // header
        if (data_size != 0) reSeg.seg.header().psh = true;
        
        reSeg.seg.header().seqno = next_seqno();

        reSeg.seg.header().fin = fin;     

        if (fin) state = FIN_SENT;

        // next_seqno
        _next_seqno += reSeg.seg.length_in_sequence_space();

        // must minus the length by _window_size, because it is possible that fill_window is invoked for many times and no have ack
        _window_size -= reSeg.seg.length_in_sequence_space();


        _segments_out.push(reSeg.seg);
        _wait.push_back(reSeg);

        // only if have window, and have data to send, must send, and only splice
        //      like a loop, but not replace the next call fill_window
        if (_window_size != 0)
            goto send;
        break;
    }
    case FIN_SENT:
        ;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absolute_ackno = unwrap(ackno, _isn, _next_seqno);

    bool flag = false;

    // only the window_size is valid, _next_seqno is the  expecting ackno
    //         absolute is the last ackno that alreay received
    if (absolute_ackno == _window_begin_seqno) 
        goto window;

    // error, the ackno is possbily repeat 
    // if (absolute_ackno == 1) state = SYN_ACKED;
    //          if absolute_ackno = 1 repeat,  state = SYN_ACKED, will send the fin repeatly
    if (absolute_ackno == 1 && state == SYN_SENT) state = SYN_ACKED;  

    // impossible ackno, ackno is the next seqno that will send, and is the receiver need's next
    // ackno must be valid

    // success for ack, then open next item's retransmit

    // ack must be greater than the window first seqno
    if (absolute_ackno >= _window_begin_seqno && absolute_ackno <= _next_seqno) {
        for (auto & reSeg : _wait) {
            uint64_t absolute_seqno = unwrap(reSeg.seg.header().seqno, _isn, _next_seqno);
            if (absolute_seqno + reSeg.seg.length_in_sequence_space() <= absolute_ackno) {
                _wait.pop_front();
                flag = true;
            } else break;
        }
    }

    // ack is successful, and the next is time out,  send
    if (flag && !_wait.empty()) {
        Retransmission & reSeg = _wait.front();
        // only open, don't send again
        reSeg.ddlTick = _ddlTick + _initial_retransmission_timeout;
    }


    // ackno is valid, then _window_begin is possible changed
    if (flag && _wait.empty()) {
        _window_begin_seqno = _next_seqno;
    } else {
        _window_begin_seqno = unwrap(_wait.front().seg.header().seqno, _isn, _next_seqno);
    }
    // the window_size must be updated


    // _window_size is the actual bytes that can be sent
    //       window_size is updated when a ack arrive, in the moment, the SYN already pop, so SYN no occupy the window_size
    //       and the window_size is updated first when a ack for SYN arrive
    
    // if ackno is a part of a segment, it is invalid, the window_size is invaid, still use last
    if (!flag) return;

window:
    if (window_size == 0) _window_size = 1, sendOneByte = true;
    else _window_size = window_size, sendOneByte = false;


    // _window_size is the number of bytes of can send
    if (_wait.empty())
        _window_size = _window_size;
    else {
        // still have segment is waiting, its window is already use
        //assert(_next_seqno > _window_begin_seqno);
        //assert(_window_size > _next_seqno - _window_begin_seqno);
        //                         the bytes of waiting data
        if (_window_size <= _next_seqno - _window_begin_seqno)
            _window_size = 0;
        else
            _window_size = _window_size - (_next_seqno - _window_begin_seqno);
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // the elapsed time
    _ddlTick += ms_since_last_tick;
    if (_wait.empty()) return;

    // only retransmission the first, per time only retransmit one, until it is acked
    auto & reSeg = _wait.front();
    // time out
    if (reSeg.ddlTick <= _ddlTick) {

        // error => ddlTick is ddl time, not a time segment
        ++reSeg.re_cnt;
        
        // exceed the max retry times
        if (reSeg.re_cnt > TCPConfig::MAX_RETX_ATTEMPTS) {

        }

        if (sendOneByte) {
            assert(reSeg.seg.length_in_sequence_space() == 1);
            reSeg.ddlTick = _ddlTick + _initial_retransmission_timeout;
        }
        else 
            reSeg.ddlTick = _ddlTick + std::pow(2, reSeg.re_cnt) * _initial_retransmission_timeout;
        
        _segments_out.push(reSeg.seg);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
/*
    uint64_t max = 0;
    for (auto & reSeg : _wait) {
        if (reSeg.re_cnt > max)
            max = reSeg.re_cnt;
    }

    return max;
*/
    if (_wait.empty()) return 0;
    
    return _wait.front().re_cnt;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    // sender: seqno SYN FIN payload
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
