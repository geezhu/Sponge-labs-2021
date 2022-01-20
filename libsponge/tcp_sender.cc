#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <cassert>
#include <iostream>
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
    : isn_(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , initial_retransmission_timeout_{retx_timeout}
    , stream_(capacity)
    ,retransmission_timeout_(initial_retransmission_timeout_){}

uint64_t TCPSender::bytes_in_flight() const { return next_seqno_-ackno_; }

void TCPSender::fill_window() {
    TCPSegment segment;
    if (next_seqno_==0){
        segment.header().seqno=isn_;
        segment.header().syn= true;
        assert(window_size_==1);
        segments_out_.emplace(segment);
        segments_backup_.emplace(segment);
        next_seqno_+=segment.length_in_sequence_space();
        return;
    }
    if (bytes_in_flight()>window_size_ or (bytes_in_flight() == window_size_ and window_size_!=0)){
//        clog<<"bytes_in_flight("<<bytes_in_flight()<<") = window_size_("<<window_size_<<")"<<endl;
        return;
    }
    auto fin_send= stream_.input_ended()&&next_seqno_==stream_.bytes_read()+2;
    uint16_t payload_size=0;
    uint16_t flag_size=0;
    if (fin_send){
//        assert(window_size_==0);
        payload_size=0;
        flag_size=0;
    }
    if (!fin_send){
        auto available_size=stream_in().buffer_size();
        payload_size= min(window_size_-bytes_in_flight(),available_size);

        if (stream_in().input_ended()){
            flag_size=1;
        }
        if (payload_size+flag_size>window_size_-bytes_in_flight()){
            flag_size=0;
        }
        if (window_size_==0) {
            //treat as window_size_==1;
            if (available_size!=0) {
                payload_size= 1;
                flag_size=0;
            } else{
                if (stream_in().input_ended()){
                    payload_size=0;
                    flag_size=1;
                }
            }
        }

    }
    auto make_segment=[this](uint16_t send_size_)->TCPSegment{
        std::string  str;
        TCPSegment segment_;
        segment_.header().seqno=next_seqno();
        segment_.header().fin= true;
        assert(send_size_<=stream_in().buffer_size()+1);
        if (stream_in().buffer_size()>=send_size_){
            segment_.header().fin= false;
            segment_.payload()=Buffer(stream_in().read(send_size_));
        }
        else{
            segment_.header().fin= stream_in().input_ended();
            segment_.payload()=Buffer(stream_in().read(send_size_-1));
        }
        next_seqno_+=segment_.length_in_sequence_space();
        return segment_;
    };
    while (payload_size!=0 or flag_size != 0){
        if (payload_size>TCPConfig::MAX_PAYLOAD_SIZE){
            segment=make_segment(TCPConfig::MAX_PAYLOAD_SIZE);
            payload_size-=TCPConfig::MAX_PAYLOAD_SIZE;
        }else{
            assert(payload_size<=TCPConfig::MAX_PAYLOAD_SIZE);
            segment=make_segment(payload_size+flag_size);
            flag_size-=flag_size;
            payload_size-=payload_size;
        }
        if (segments_backup_.empty()){
            last_tick_from_retransmission_=0;
            retransmission_timeout_=initial_retransmission_timeout_;
        }
        segments_out_.emplace(segment);
        segments_backup_.emplace(segment);
        assert(segment.length_in_sequence_space()-segment.header().fin<=TCPConfig::MAX_PAYLOAD_SIZE);
    }
    assert(payload_size==0 and flag_size==0);
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto new_ackno=unwrap(ackno,isn_,ackno_);
    if (new_ackno>next_seqno_){
        //can not equal for that ack is asking for next unassemble byte
//        clog<<"receive "<<ackno.raw_value()<<" ("<<new_ackno<<") >=next_seqno("<<next_seqno_<<")"<<endl;
        return;
    }
    if (ackno_>new_ackno){
        auto diff=ackno_-new_ackno;
        auto new_window_size=window_size;
        if (new_window_size<=diff){
            new_window_size=0;
        }
        else{
            new_window_size=new_window_size-diff;
        }
        if (new_window_size>window_size_){
            window_size_=new_window_size;
        }
    }
    if (ackno_==new_ackno){
        if (window_size>window_size_){
            window_size_=window_size;
        }
    }
    if (ackno_<new_ackno){
//        assert(window_size+new_ackno>=window_size_+ackno_);
        ackno_=new_ackno;
        window_size_=window_size;
        while (!segments_backup_.empty()){
            uint64_t segment_seqno= unwrap(segments_backup_.front().header().seqno,isn_,ackno_);
            size_t segment_length= segments_backup_.front().length_in_sequence_space();
            if (ackno_>=segment_seqno+segment_length){
                segments_backup_.pop();
            } else{
                break;
            }
        }
        last_tick_from_retransmission_=0;
        retransmission_timeout_=initial_retransmission_timeout_;
    }

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    constexpr uint32_t MAXSLEEP =(1<<31);
    if (segments_backup_.empty()){
        return;
    }
    last_tick_from_retransmission_+=ms_since_last_tick;
    if (last_tick_from_retransmission_>=retransmission_timeout_){
        segments_out().emplace(segments_backup_.front());
        assert(retransmission_timeout_<MAXSLEEP);
        if (window_size_!=0){
            retransmission_timeout_<<=1;
        }
        last_tick_from_retransmission_=0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    uint64_t increase_times=retransmission_timeout_/initial_retransmission_timeout_;
    uint exp=0;
    while (increase_times!=1){
        increase_times=increase_times>>1;
        exp+=1;
    }
    return exp;
}

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno= wrap(next_seqno_-1,isn_);
    segment.payload()=Buffer();
    segments_out_.emplace(segment);
}
