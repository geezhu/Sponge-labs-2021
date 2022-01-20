#include "tcp_receiver.hh"
#include <cassert>
#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    WrappingInt32 seqno=seg.header().seqno;
    if (seg.header().syn){
        if (isn_){
//            assert(isn_.value()==seqno);
            if (isn_.value()==seqno){
                //rfc793:34
                //get confused
            }
        }
        isn_=seqno;
        seqno= WrappingInt32(seqno.raw_value()+1);
//        assert(seg.length_in_sequence_space()<=2);
    }
    if (!isn_){
        return;
    }
    auto absolute_seqno= unwrap(seqno,isn_.value(),absolute_sequence_number_);
    auto unreachable_seqno= WrappingInt32(ackno()->raw_value()+window_size());
    auto absolute_unreachable_seqno= unwrap(unreachable_seqno,isn_.value(),absolute_sequence_number_);
    if (absolute_seqno>=absolute_unreachable_seqno){
        //nothing to do
        return;
    }
    uint64_t absolute_index= unwrap(seqno,isn_.value(),absolute_sequence_number_);
    string && str=string();
    if (!seg.payload().str().empty()){
        str=string(seg.payload().str().data(),seg.payload().str().length());
    }
    reassembler_.push_substring(str,absolute_index-1, seg.header().fin);
    absolute_sequence_number_=unwrap(WrappingInt32(stream_out().bytes_written()), WrappingInt32(0),absolute_sequence_number_);
    if (isn_){
        absolute_sequence_number_+=1;
    }
    if (stream_out().input_ended()){
        absolute_sequence_number_+=1;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    uint32_t flag_cout=0;
    if (!isn_){
        return nullopt;
    } else{
        flag_cout+=1;
    }
    if (stream_out().input_ended()){
        flag_cout+=1;
    }
    uint64_t absolute_ackno=unwrap(WrappingInt32(stream_out().bytes_written()+flag_cout), WrappingInt32(0),absolute_sequence_number_);
    return wrap(absolute_ackno,isn_.value());
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
