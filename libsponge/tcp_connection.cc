#include "tcp_connection.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// the user can wirte to sender's bytes

//   receiver =>   capacity = window_size = unread bytes + the bytes that can receive
//   sender   =>   capacity = the byte_stream's size
size_t TCPConnection::remaining_outbound_capacity() const {
    // capacity - the bytes that have not been read by sender
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _tick - _last_receive_tick; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _last_receive_tick = _tick;

    // LISTEN
    if (tcp_state == CLOSE) {
        if (seg.header().rst)
            return;
        else if (seg.header().ack)
            return;
        else if (seg.header().syn) {
            // be sent syn
            tcp_state = SYN_RECV;
        } else {
            // no need the valid seqno
            TCPSegment s;
            s.header().rst = true;
            s.header().seqno = seg.header().ackno;

            _segments_out.push(s);
            return;
        }
    }

    // RST
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }

    // receiver concern: SYN FIN seqno payload
    //      influence the receiver, may cause _receiver's bytestream is ended

    if (seg.length_in_sequence_space() != 0)
        _receiver.segment_received(seg);

    if (_receiver.stream_out().input_ended()) {
        _receive_fin = true;
        _receive_fin_tick = _tick;

        if (!_signed_linger) {
            if (_send_fin)
                _linger_after_streams_finish = true;
            else
                _linger_after_streams_finish = false;
            _signed_linger = true;
        }
    }

    // sender concern: ackno win
    //      influence the sender, may cause _sender send fin and may minus the bytes in flight by pop the segment from
    //      _wait
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);

        // already send fin, fin is acked, and no need to
        if (tcp_state == FIN_SENT && _sender.bytes_in_flight() == 0 && !_linger_after_streams_finish) {
            tcp_state = FIN_ACKED;
            _active = false;
            return;
        }
    }

    // keep-alive
    // have received SYN                         this segment's length is zero               seg's seqno == expecting
    // ackno - 1
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
        (seg.header().seqno == _receiver.ackno().value() - 1))
        _sender.send_empty_segment();

    // send all segment that sender produced
    sendSegments();

    // must send a segment to reflect receive this seg
    if (seg.length_in_sequence_space() != 0) {
        if (_segments_out.empty()) {
            _sender.send_empty_segment();
        }
    }

    sendSegments();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (data.empty())
        return 0;

    size_t data_size = data.size();
    size_t buffer_size = remaining_outbound_capacity();

    data_size = data_size < buffer_size ? data_size : buffer_size;

    _sender.stream_in().write(string(data.begin(), data.begin() + data_size));

    return data_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _tick += ms_since_last_tick;

    // listen, no need to send data or retransmit
    if (tcp_state == CLOSE)
        return;

    if (_receive_fin && _send_fin && _linger_after_streams_finish) {
        if (_tick >= _receive_fin_tick + 10 * _cfg.rt_timeout) {
            _active = false;
            _linger_after_streams_finish = false;
        }
    }

    _sender.tick(ms_since_last_tick);

    // the times of retransmission exceed the max, send RST
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        abort();
        return;
    }

    // time out's segment will be retransmitted
    //   if state in CLOSE, will send SYN
    sendSegments();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    sendSegments();
}

void TCPConnection::connect() {
    // send SYN
    assert(tcp_state == CLOSE);
    sendSegments();
    tcp_state = SYN_SENT;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            abort();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::sendSegments() {
    // _sender fill window
    _sender.fill_window();

    // next_seqno => already send's bytes, no need to ack fin
    if (_sender.next_seqno_absolute() >= _sender.stream_in().bytes_written() + 2) {
        tcp_state = FIN_SENT;
        _send_fin = true;

        if (!_signed_linger) {
            if (_receive_fin)
                _linger_after_streams_finish = false;
            else
                _linger_after_streams_finish = true;
            _signed_linger = true;
        }
    }

    // connection send actually
    while (!_sender.segments_out().empty()) {
        TCPSegment &seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        // already receive SYN
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }

        // fill the win field
        size_t window_size = _receiver.window_size();

        if (window_size > static_cast<size_t>(std::numeric_limits<uint16_t>::max()))
            window_size = static_cast<size_t>(std::numeric_limits<uint16_t>::max());

        seg.header().win = static_cast<uint16_t>(window_size);

        _segments_out.push(seg);
    }
}

void TCPConnection::abort() {
    // must have the correct seqno

    // produce RST, mainly get the next_seqno (valid)
    _sender.send_empty_segment();

    // delete all sender's segment except the RST segment
    while (_sender.segments_out().size() != 1) {
        _sender.segments_out().pop();
    }

    // get RST's segment
    TCPSegment &seg = _sender.segments_out().front();
    _sender.segments_out().pop();

    // set RST
    seg.header().rst = true;

    // set
    _segments_out.push(seg);

    // abort connection
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();

    // set inactive
    _active = false;
    _linger_after_streams_finish = false;
}