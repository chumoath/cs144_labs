#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t val = n + isn.raw_value();
    return WrappingInt32{val};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // use the uint64's flow, because n is based on isn
    // todo: the closest pos, but right can not less than isn
    uint64_t val = checkpoint + isn.raw_value();

    // no have right wrap 
    if (val <= 0xFFFFFFFF) {
        uint64_t cur = (val & 0xFFFFFFFF00000000) | n.raw_value();
        uint64_t left = ((val + 0x100000000) & 0xFFFFFFFF00000000) | n.raw_value();

        if (cur < isn.raw_value()) {
            val = left;
        } else {
            uint64_t diffl = left - val;
            uint64_t diffc = val < cur ? cur - val : val - cur;

            val = diffl < diffc ? left : cur;
        }
    } else {
        // three wrap
        uint64_t left = ((val + 0x100000000) & 0xFFFFFFFF00000000) | n.raw_value();
        uint64_t cur = (val & 0xFFFFFFFF00000000) | n.raw_value();
        uint64_t right = ((val - 0x100000000) & 0xFFFFFFFF00000000) | n.raw_value();
    
        uint64_t diffl = left - val;
        uint64_t diffc = val < cur ? cur - val : val - cur;
        uint64_t diffr = val - right;
        
        // no need val in the second wrap, bacause right can decide
        if (right < isn.raw_value()) {
                val = diffl < diffc ? left : cur;
        } else {
            val = diffl < diffc ? (diffl < diffr ? left : right) : (diffc < diffr ? cur : right); 
        }
    }

    // uint64's flow - isn  =>  absolute seq
    return val - isn.raw_value();
}
