#include "wrapping_integers.hh"
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}
constexpr uint64_t UINT32_LIMIT=static_cast<uint64_t>(UINT32_MAX)+1;
using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t smaller_number=(n&(UINT32_MAX))+isn.raw_value();
    smaller_number=smaller_number&UINT32_MAX;
    return WrappingInt32{static_cast<uint32_t>(smaller_number)};
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
    uint64_t checkpoint_uint32= wrap(checkpoint,isn).raw_value();
    auto closer=[](uint64_t lhs,uint64_t mid,uint64_t rhs,uint64_t cmp)->uint64_t {
        uint64_t diff1= max(lhs,cmp)- min(lhs,cmp);
        uint64_t diff2= max(mid,cmp)- min(mid,cmp);
        uint64_t diff3= max(rhs,cmp)- min(rhs,cmp);
        auto min_diff= std::min(diff1,min(diff2,diff3));
        if (min_diff==diff1){
            return lhs;
        }
        if (min_diff==diff2){
            return mid;
        }
        return rhs;
    };
    uint64_t result=checkpoint-checkpoint_uint32+n.raw_value();
    return closer(result-UINT32_LIMIT,result,result+UINT32_LIMIT,checkpoint);
}