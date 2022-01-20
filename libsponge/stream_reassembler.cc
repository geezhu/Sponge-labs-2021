#include "stream_reassembler.hh"
#include <cassert>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
[[maybe_unused]] void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : output_(capacity), capacity_(capacity),read_(0),end_(nullopt),sliding_window_(),map_(){
    sliding_window_.reserve(capacity_);
    sliding_window_.resize(capacity_,'\0');
    map_.reserve(capacity_);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    auto erase_read=[this](size_t begin,size_t end)->void{
        while (begin!=end){
            if (map_.count(begin%capacity_)!=0){
                map_.erase(begin%capacity_);
            }
            begin++;
        }
    };
    auto peek_read=[this](size_t begin,size_t end)->size_t{
      size_t str_size=0;
      while (begin!=end){
          if (map_.count(begin%capacity_)==0){
              return str_size;
          } else{
              str_size+=1;
          }
          begin++;
      }
      return str_size;
    };
    auto window_substr=[this](size_t begin,size_t len)->string {
        size_t bytes_to_read= min(len,output_.remaining_capacity());
        size_t substr1_length=min(bytes_to_read,capacity_-begin);
        size_t substr2_length=bytes_to_read-substr1_length;
        return sliding_window_.substr(begin,substr1_length)+sliding_window_.substr(0,substr2_length);
    };
    auto window_replace=[this](size_t begin,size_t len,const string& data_)->void {
        size_t bytes_to_write=min(len,output_.remaining_capacity());
        size_t substr1_length=min(bytes_to_write,capacity_-begin);
        size_t substr2_length=bytes_to_write-substr1_length;
        sliding_window_.replace(begin,substr1_length,data_.substr(0,substr1_length));
        sliding_window_.replace(0,substr2_length,data_.substr(substr1_length,substr2_length));
    };
    size_t available_size=output_.remaining_capacity();
    //for all substr
    size_t read_index=0;
    size_t read_size= 0;
    if (index<=read_&&index+data.length()>read_){
        read_index=read_-index;
        read_size= min(data.length()-read_index,available_size);
        output_.write(data.substr(read_index,read_size));
        available_size-=read_size;
        erase_read(read_%capacity_,read_%capacity_+read_size);
        auto old_read=read_;
        read_+=read_size;
        if (available_size!=0){
            read_index=read_%capacity_;
            read_size=peek_read(read_index,read_index+available_size);
            output_.write(window_substr(read_index,read_size));
//            available_size-=read_size;
            erase_read(old_read,read_+read_size);
            read_+=read_size;
        }
    }
    else if (index>read_&&index<read_+available_size){
        //keep the original version of overlapping data
        read_index=0;
        read_size= min(data.length(),read_+available_size-index);
        auto backup=window_substr(index%capacity_,read_size);
        window_replace((index)%capacity_,read_size,data.substr(read_index,read_size));
        for (size_t i = index; i < index+read_size; ++i) {
            if (map_.count(i%capacity_)!=0){
                sliding_window_[i%capacity_]=backup[i-index];
            } else{
                map_.insert(i%capacity_);
            }
        }
//        available_size-=read_size;
    }
    if (eof){
        if (end_){
            assert(end_.value()==index+data.length());
        } else{
            //TODO check the data in the window(valid)
            end_=index+data.length();
        }
    }
    if (end_ and read_==end_.value()){
        output_.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return map_.size(); }

bool StreamReassembler::empty() const { return map_.empty(); }
