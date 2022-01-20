#include "tcp_connection.hh"

#include <cassert>
#include <iostream>
#include <sstream>
// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}
constexpr size_t UINT16_MAX_UL=(1lu<<UINT16_WIDTH)-1;

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return sender_.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return sender_.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return receiver_.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return last_tick_receive_segment_; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (seg.header().rst){
        linger_after_streams_finish_= false;
        sender_.stream_in().set_error();
        sender_.stream_in().end_input();
        receiver_.stream_out().end_input();
        receiver_.stream_out().set_error();
        //make bytes in fly=0
        sender_.ack_received(sender_.next_seqno(),0);
        return;
    }
    if (seg.header().ack){
        sender_.ack_received(seg.header().ackno,seg.header().win);
    }
    last_tick_receive_segment_=0;
    receiver_.segment_received(seg);

    if (receiver_.stream_out().input_ended() and not sender_.stream_in().input_ended()){
        linger_after_streams_finish_= false;
    }
    if (seg.length_in_sequence_space()!=0){
        sender_.fill_window();
        if (sender_.segments_out().empty() and seg.length_in_sequence_space()!=0){
            //something wrong with this
            sender_.send_empty_segment();
            assert(sender_.segments_out().size()==1);
            TCPSegment segment=sender_.segments_out().front();
            sender_.segments_out().pop();
            segment.header().seqno=sender_.next_seqno();
            if (receiver_.ackno()){
                segment.header().ack= true;
                segment.header().win= min(receiver_.window_size(),UINT16_MAX_UL);
                segment.header().ackno=receiver_.ackno().value();
            }
            segments_out_.emplace(segment);
        }
    } else{
        if (receiver_.ackno().has_value()) {
            if(seg.header().seqno == receiver_.ackno().value()-1){
                sender_.send_empty_segment();
            }
            else if (seg.header().seqno == receiver_.ackno().value()){
                sender_.fill_window();
            }
        }
    }
//    clog<<"sender_ segment_out.size() = "<<sender_.segments_out().size()<<endl;

    if (sender_.segments_out().empty()){
        return;
    }
    assert(not sender_.segments_out().empty());
    while (not sender_.segments_out().empty()){
        TCPSegment segment=sender_.segments_out().front();
        sender_.segments_out().pop();
        if (receiver_.ackno()){
            segment.header().ack= true;
            segment.header().win= min(receiver_.window_size(),UINT16_MAX_UL);
            segment.header().ackno=receiver_.ackno().value();
        }
        segments_out_.emplace(segment);
    }
}

bool TCPConnection::active() const { return (not (sender_.stream_in().input_ended() and bytes_in_flight() ==0 and receiver_.stream_out().input_ended())) or linger_after_streams_finish_; }

size_t TCPConnection::write(const string &data) {
//    clog<<"TCPConnection::write "<<data.length()<<endl;
//    clog<<"TCPConnection::winsz "<<receiver_.window_size()<<endl;
    assert(not sender_.stream_in().input_ended());
    auto write_bytes=sender_.stream_in().write(data);
    if (write_bytes==0){
        return 0;
    }
    sender_.fill_window();
//    assert(not sender_.segments_out().empty());
    while (not sender_.segments_out().empty()){
        TCPSegment segment=sender_.segments_out().front();
        sender_.segments_out().pop();
        if (receiver_.ackno()){
            segment.header().ack= true;
            segment.header().win= min(receiver_.window_size(),UINT16_MAX_UL);
            segment.header().ackno=receiver_.ackno().value();
        }
        segments_out_.emplace(segment);
        assert(segment.length_in_sequence_space()<=TCPConfig::MAX_PAYLOAD_SIZE);
    }
    return write_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    last_tick_receive_segment_+=ms_since_last_tick;
    if (sender_.stream_in().input_ended() and bytes_in_flight() ==0 and receiver_.stream_out().input_ended()){
        if (linger_after_streams_finish_ and time_since_last_segment_received() >= 10*cfg_.rt_timeout){
            clog<<"tick timeout"<<endl;
            linger_after_streams_finish_= false;
        }
    }
    if (sender_.consecutive_retransmissions()>=TCPConfig::MAX_RETX_ATTEMPTS){
        sender_.send_empty_segment();
        assert(sender_.segments_out().size()==1);
        TCPSegment segment=sender_.segments_out().front();
        sender_.segments_out().pop();
        segment.header().rst= true;
        segments_out_.emplace(segment);
        sender_.stream_in().end_input();
        sender_.stream_in().set_error();
        receiver_.stream_out().end_input();
        receiver_.stream_out().set_error();
        //make bytes in fly=0
        sender_.ack_received(sender_.next_seqno(),0);
//        active_= false;
        linger_after_streams_finish_= false;
        return;
    }
    sender_.tick(ms_since_last_tick);
    assert(sender_.segments_out().size()<=1);
    if (not sender_.segments_out().empty()){
        TCPSegment segment=sender_.segments_out().front();
        sender_.segments_out().pop();
        if (receiver_.ackno()){
            segment.header().ack= true;
            segment.header().win= min(receiver_.window_size(),UINT16_MAX_UL);
            segment.header().ackno=receiver_.ackno().value();
        }
        segments_out_.emplace(segment);
    }
}

void TCPConnection::end_input_stream() {
    sender_.stream_in().end_input();
    sender_.fill_window();
//    assert(not sender_.segments_out().empty());
    while (not sender_.segments_out().empty()){
        TCPSegment segment=sender_.segments_out().front();
        sender_.segments_out().pop();
        if (receiver_.ackno()){
            segment.header().ack= true;
            segment.header().win= min(receiver_.window_size(),UINT16_MAX_UL);
            segment.header().ackno=receiver_.ackno().value();
        }
        segments_out_.emplace(segment);
    }
}

void TCPConnection::connect() {
    sender_.fill_window();
    assert(sender_.segments_out().size()==1);
    segments_out_.emplace(sender_.segments_out().front());
    sender_.segments_out().pop();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            sender_.send_empty_segment();
            assert(sender_.segments_out().size()==1);
            TCPSegment segment=sender_.segments_out().front();
            sender_.segments_out().pop();
            segment.header().rst= true;
            segments_out_.emplace(segment);
            sender_.stream_in().end_input();
            sender_.stream_in().set_error();
            receiver_.stream_out().end_input();
            receiver_.stream_out().set_error();
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
