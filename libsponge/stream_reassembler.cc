#include "stream_reassembler.hh"

#include <cassert>
#include <sstream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
[[maybe_unused]] void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
StreamReassembler::StreamReassembler(const size_t capacity) : output_(capacity), capacity_(capacity),read_(0),end_(nullopt){
}
size_t StreamReassembler::unassembled_bytes() const {
    size_t count=0;
    auto iter=buffers_.begin();
    auto end=buffers_.end();
    while (iter!=end){
        count+=iter->size();
        iter++;
    }
    return count;
}

bool StreamReassembler::empty() const {
    return buffers_.empty();
}
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    auto erase_read=[this](uint64_t begin,size_t len)->void{
      if(buffers_.empty()){
          return;
      }
      size_t erase_size;
      auto iter=buffers_.begin();
      auto end=buffers_.end();
      assert(begin<=iter->begin_index());
      while (iter!=end and len!=0){
          if (begin<iter->begin_index()){
              erase_size= min(iter->begin_index()-begin,len);
          } else{
              assert(begin==iter->begin_index());
              erase_size= min(len,iter->size());
              iter->remove_prefix(erase_size);
              iter++;
          }
          len-=erase_size;
          begin+=erase_size;
      }
      if (iter!=buffers_.begin() and (iter-1)->size()!=0){
          iter--;
      }
      if (iter!=buffers_.begin()){
          buffers_.erase(buffers_.begin(),iter);
      } else if (iter->size()==0){
          buffers_.erase(iter);
      }
    };
    auto peek_read=[this](uint64_t begin,size_t len)->size_t{
      size_t str_size=0;
      size_t read_size;
      auto iter=buffers_.begin();
      auto end=buffers_.end();
//      assert(iter!=end and begin<=iter->begin_index());
      while (iter!=end and begin== iter->begin_index() and len!=0){
          read_size= min(len,iter->size());
          len-=read_size;
          str_size+=read_size;
          begin+=read_size;
          iter++;
      }
      return str_size;
    };
    auto window_substr=[this](size_t len)->string {
      //must erase every useless buffer and prefix
      auto iter=buffers_.begin();
//      auto end=buffers_.end();
      size_t bytes_to_read= min(len,output_.remaining_capacity());
      //merge
      string string_buffer;
      size_t read_bytes;
      while (bytes_to_read!=0){
          read_bytes= min(bytes_to_read,iter->size());
          string_buffer.append(string (iter->str().data(),read_bytes));
          iter->remove_prefix(read_bytes);
          bytes_to_read-=read_bytes;
          iter++;
      }
      if (iter!=buffers_.begin() and (iter-1)->size()!=0){
          iter--;
      }
      if (iter!=buffers_.begin()){
          buffers_.erase(buffers_.begin(),iter);
      } else if (iter->size()==0){
          buffers_.erase(iter);
      }
      return string_buffer;
    };
    auto window_replace=[this](uint64_t data_begin_index,size_t len,const string& data_)->void {
      size_t bytes_to_write=min(len,output_.remaining_capacity());
      auto end_buffer_index=buffers_.empty()?0lu:(buffers_.end()-1)->end_index();
      auto end_data_index=data_begin_index+bytes_to_write;
      size_t write_index=0;
      size_t write_length;
      size_t ignore_length;
      if (bytes_to_write!=0 and (buffers_.empty() or end_buffer_index < data_begin_index)){
          write_length=bytes_to_write;
          ignore_length=0;
          RAII_Substr substr(data_begin_index,write_index,write_length,ignore_length,bytes_to_write,data_);
          buffers_.emplace_back(data_begin_index,substr.to_string());
      }
      if (not buffers_.empty() and bytes_to_write!=0 and (buffers_.end()-1)->begin_index()<=data_begin_index){
          write_length=max(end_data_index,end_buffer_index)-end_buffer_index;
          ignore_length= min(end_buffer_index,end_data_index)-data_begin_index;
          RAII_Substr substr(data_begin_index,write_index,write_length,ignore_length,bytes_to_write,data_);
          substr.ignore_first();
          (buffers_.end()-1)->append(substr.to_string());
      }
      auto end=buffers_.end();
      auto iter=buffers_.begin();
      while (iter!=end and bytes_to_write!=0){
          end_buffer_index=iter->end_index();
          if (end_buffer_index < data_begin_index){
              iter++;
              continue;
          }
          if (iter->begin_index()<=data_begin_index){
              auto last_of_buffer=max(end_data_index,end_buffer_index);
              if (iter+1!=end){
                  last_of_buffer= min(last_of_buffer,(iter+1)->begin_index());
              }
              write_length=last_of_buffer-end_buffer_index;
              ignore_length=min(end_buffer_index,end_data_index)-data_begin_index;
              RAII_Substr substr(data_begin_index,write_index,write_length,ignore_length,bytes_to_write,data_);
              substr.ignore_first();
              iter->append(substr.to_string());
              iter++;
          }
          else if (iter->begin_index()>data_begin_index){
              //assert(iter -buffers_.begin()<(1u<<31));
              if (end_data_index<iter->begin_index()){
//                  write_index=0;
                  assert(write_index==0);
                  {
                      write_length=bytes_to_write;
                      ignore_length=0;
                      RAII_Substr substr(data_begin_index,write_index,write_length,ignore_length,bytes_to_write,data_);
                      buffers_.insert(iter,ExtendBuffer(data_begin_index,substr.to_string()));
                  }
                  assert(bytes_to_write==0);
              } else{
                  write_length=iter->begin_index()-data_begin_index;
                  ignore_length=min(end_data_index,end_buffer_index)-iter->begin_index();
                  RAII_Substr substr(data_begin_index,write_index,write_length,ignore_length,bytes_to_write,data_);
                  iter->push_front(substr.to_string());
              }
          }
      }
      assert(bytes_to_write==0);
    };
    size_t available_size=output_.remaining_capacity();
    size_t read_index;
    size_t read_size;
    if (index<=read_ and index+data.length()>read_){
        read_index=read_-index;
        read_size= min(data.length()-read_index,available_size);
        output_.write(data.substr(read_index,read_size));
        available_size-=read_size;
        erase_read(read_,read_size);
        read_+=read_size;
        if (available_size!=0){
            read_size=peek_read(read_,available_size);
            if (read_size!=0){
                output_.write(window_substr(read_size));
            }
            read_+=read_size;
        }
    }else if (index>read_&&index<read_+available_size){
        read_index=0;
        read_size= min(data.length(),read_+available_size-index);
        window_replace(index,read_size,data.substr(read_index,read_size));
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