#include "byte_stream.hh"

#include <cassert>
#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
[[maybe_unused]] void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) :
    error_(false),capacity_(capacity),buffer_(),write_(0),read_(0),
    bytes_written_(0),bytes_read_(0),full_(false),eoi_(false){
    buffer_.reserve(capacity_);
}

size_t ByteStream::write(const string &data) {
//    clog<<"ByteStream::write "<<data.length()<<endl;
//    clog<<"ByteStream::capacity "<<capacity_<<endl;
    assert(!eoi_);
    auto buffer_write=[this](size_t data_length,const string& data_){
        if (data_length==0){
            return;
        }
        buffer_.replace(write_,data_length,data_);
        write_+=data_length;
        write_=write_%capacity_;
        bytes_written_+=data_length;
        full_=(write_==read_);
    };
    size_t bytes_to_write= min(data.length(),remaining_capacity());
    size_t substr1_length=min(bytes_to_write,capacity_-write_);
    size_t substr2_length=bytes_to_write-substr1_length;
    buffer_write(substr1_length,data.substr(0,substr1_length));
    buffer_write(substr2_length,data.substr(substr1_length,substr2_length));
    return substr1_length+substr2_length;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t bytes_to_read= min(len,buffer_size());
    size_t substr1_length=min(bytes_to_read,capacity_-read_);
    size_t substr2_length=bytes_to_read-substr1_length;
    return buffer_.substr(read_,substr1_length)+buffer_.substr(0,substr2_length);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len==0){
        return;
    }
    size_t bytes_to_pop= min(len,buffer_size());
    //when bytes_to_pop=0 remaining_capacity=capacity_,len will not be 0
    read_+=bytes_to_pop;
    read_=read_%capacity_;
    bytes_read_+=bytes_to_pop;
    full_= false;//avoid nothing to pop
}
//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto output= peek_output(len);
    pop_output(len);
    return output;
}

void ByteStream::end_input() {
    eoi_= true;
}

bool ByteStream::input_ended() const { return eoi_; }

size_t ByteStream::buffer_size() const {
    if (full_){
        return capacity_;
    }
    return (capacity_+write_-read_)%capacity_;
}

bool ByteStream::buffer_empty() const { return read_==write_ and !full_; }

bool ByteStream::eof() const { return eoi_ and buffer_empty(); }

size_t ByteStream::bytes_written() const { return bytes_written_; }

size_t ByteStream::bytes_read() const { return bytes_read_; }

size_t ByteStream::remaining_capacity() const {
    if (buffer_empty()){
        return capacity_;
    }
    return (capacity_+read_-write_)%capacity_;
}
