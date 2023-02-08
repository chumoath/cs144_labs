#include "tcp_receiver.hh"
#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
std::pair<string, uint64_t> TCPReceiver::get_valid_string(uint64_t absolute_seqno, uint64_t absolute_ackno, Buffer & payLoad) {
    uint64_t dataBegin = absolute_seqno;
    uint64_t dataEnd = absolute_seqno + payLoad.size();
    uint64_t winBegin = absolute_ackno;
    uint64_t winEnd = absolute_ackno + window_size();
    
    /*  debug
    cout << "absolute_ackno: " << absolute_ackno << endl;
    cout << "absolute_seqno: " << absolute_seqno << endl;
    cout << "dataBegin: " << dataBegin << endl;
    cout << "dataEnd: " << dataEnd << endl;
    cout << "winBegin: " << winBegin << endl;
    cout << "winEnd: " << winEnd << endl;
    */

    string s;
    uint64_t index;
    // invalid
    if (dataEnd <= winBegin || dataBegin >= winEnd) {
        // cout << "data empty" << endl;
        s = "";
        index = winBegin - 1;
    }
    // left
    else if (dataBegin < winBegin && dataEnd <= winEnd) {
        // cout << "left" << endl;
        uint64_t l = winBegin - dataBegin;
        s = string(payLoad.str().begin() + l, payLoad.str().end());
        index = winBegin - 1;
    }
    // middle
    else if (dataBegin >= winBegin && dataEnd <= winEnd) {
        // cout << "middle" << endl;
        s = string(payLoad.str().begin(), payLoad.str().end());
        index = dataBegin - 1;
    }
    // right
    else if (dataBegin >= winBegin && dataEnd > winEnd){
        // cout << "right" << endl;
        uint64_t r = dataEnd - winEnd;
        s = string(payLoad.str().begin(), payLoad.str().end() - r);
        index = dataBegin - 1;
    }
    // cover
    else if (dataBegin <= winBegin && dataEnd >= winEnd) {
        // cout << "cover" << endl;
        uint64_t l = winBegin - dataBegin;
        uint64_t r = dataEnd - winEnd;
        s = string(payLoad.str().begin() + l, payLoad.str().end() - r);
        index = winBegin - 1;
    }
    return std::pair<string, uint64_t>(s, index);
}

bool TCPReceiver::get_fin(TCPHeader & header, Buffer & payLoad, uint64_t absolute_seqno, uint64_t absolute_ackno) {
    if (!header.fin) return false;

    // entire window = window_size + unread_bytes
    uint64_t winEnd = absolute_ackno + window_size();
    uint64_t dataEnd = absolute_seqno + payLoad.size();

    // 数据 超出了 范围， 不能发送 fin
    if (dataEnd > winEnd) return false;

    return true;
}

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    Buffer payLoad = seg.payload();

    // // 不需要过度优化，跟着测试来即可
    // // SYN
    // if (header.syn) {
    //     _ackno = _isn = header.seqno;
    //     _syn = true;
    // }
    
    // // 没有同步，不做处理，直接丢弃
    // if (!_syn) return;

    // // 接收数据的序列号，为了防止 SYN 和 FIN 和 数据一起发送
    // //     FIN 可以 和 数据一起处理，因为 逻辑上，FIN 在 数据 之后，所以 index 就是 seqno
    // //     SYN 带的数据，数据的序列号 必须是 略过了 SYN 的 1

    // // 数据的 序号    
    // WrappingInt32 seqno = header.seqno;

    // seqno = seqno + header.syn;

    // uint64_t absolute_seqno = unwrap(seqno, _isn, _reassembler.ackno_index());
    // // 要用写入数据之前的
    // uint64_t absolute_ackno = unwrap(_ackno, _isn, _reassembler.ackno_index()); 

    // _reassembler.push_substring(payLoad.copy(), absolute_seqno - 1, header.fin);

    // // 同步了，而且 序列号 和 确认号 相同
    // // 未同步
    // absolute_seqno = absolute_seqno - header.syn;

    // // 未同步不会到达这里

    // // 确认号 增加的不能只是 现在包的长度，有的包会提前到
    // if (absolute_seqno == absolute_ackno) {
    //     // _reassembled 保存下一个期待的 ackno_index
    //     size_t ackno_idx = _reassembler.ackno_index();
    //     // absolute_ackno 是 当前收到的包的序列号，还要加上 fin
    //     _ackno = _ackno + (ackno_idx + 1 - absolute_ackno) + header.fin + header.syn;
    //     //                             data
    // }



    switch (status) {
    case Status::CLOSED:
    {
        // process SYN
        if (header.syn) {
            status = Status::SYN_RECV;
            _isn = header.seqno;
            _ackno = _isn + 1;
            _syn = true;
            header.seqno = header.seqno + 1;
        }
        else {
            // 未同步，直接丢弃
            break;
        }
    }
    case Status::SYN_RECV:
    {
       
        uint64_t absolute_ackno = unwrap(_ackno, _isn, _reassembler.ackno_index());
        uint64_t absolute_seqno = unwrap(header.seqno, _isn, _reassembler.ackno_index());
 
        std::pair<string, uint64_t> pair = get_valid_string(absolute_seqno, absolute_ackno, payLoad);    
        bool fin = get_fin(header, payLoad, absolute_seqno, absolute_ackno);

        // 带 fin 的包 可能提前到达，期待的包 可能后面才到，它没有带 fin，所以，无法使用该 fin，使 ackno + 1
        _reassembler.push_substring(pair.first, pair.second, fin);

        // debug
        // cout << "actual write bytes: " << _reassembler.stream_out().bytes_written() << endl;
        // cout << "unreassemble bytes: " << _reassembler.unassembled_bytes() << endl;
        // cout << "window_size: " << window_size() << endl;

        // _reassembler.dump_cache_graph(_isn.raw_value());
        
        // 增加 _ackno

        // ******************************************        *********************************************************
        // ******************************************   bug  *********************************************************
        // ******************************************        *********************************************************

        // 这里有 bug， absolute_seqno 可能是 在确认号的左边，所以 要使用 实际写入数据开始的的 absolute_seqno

        // pair.second 是 实际要输入数据 的 索引号，所以 index + 1 = 实际输入数据的absolute_seqno

        // *************************************    modigy bug   ******************************************************
        absolute_seqno = pair.second + 1;
        if (absolute_ackno == absolute_seqno) {
            //                     下一个需要的 absolute_ackno     当前收到的
            // _ackno 是 下一个期望收到的 序列号，所以，必须 全都放到 byte_stream 才能加 fin       
            _ackno = _ackno + (_reassembler.ackno_index() + 1 - absolute_ackno) + _reassembler.stream_out().input_ended();
            //                                                                              只有 向 byte_stream 输入 EOF 后，该值 才会加上
            // stream_out().eof()  是 用户读的时候判断是否读到 eof
        }

        // avoid _ackno is increased
        if (_reassembler.stream_out().input_ended()) {
            status = FIN_RECV;
        }
    }
    case Status::ESTABLISHED:
        ;
    case Status::FIN_RECV:
        ;
    }

}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn) return {};
    // 必须使用 _ackno 记录，因为 _reassemble 的 next_idx 不 标记 fin
    return _ackno;
}

// 根据 测试的要求，调整 到底那块是  window_size
size_t TCPReceiver::window_size() const {
    return _reassembler.window_size();
}
