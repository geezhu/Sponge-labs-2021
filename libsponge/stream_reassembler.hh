#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"
#include "buffer.hh"
#include <unordered_set>
#include <cstdint>
#include <string>
#include <iostream>
//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class ExtendBuffer: public Buffer{
  private:
    uint64_t index_;
  public:
    ExtendBuffer() = default;
    //! \brief Construct by taking ownership of a string
    ExtendBuffer(uint64_t index,std::string &&str) noexcept : Buffer(std::move(str)),index_(index) {}
    void append(const Buffer &other) {
        append(std::string(other.str().data(),other.str().length()));
    }
    void append(std::string &&str) {
        str=std::string(this->str().data(), this->str().length())+str;
        _storage=std::make_shared<std::string>(std::move(str));
        _starting_offset=0;
    }
    void push_front(std::string &&str) {
        append_prefix(str.length());
        str.append(std::string(this->str().data(), this->str().length()));
        _storage=std::make_shared<std::string>(std::move(str));
        _starting_offset=0;
    }
    void remove_prefix(const size_t n){
        Buffer::remove_prefix(n);
        index_+=n;
    };
    void append_prefix(const size_t n){
        index_-=n;
    };
    uint64_t end_index()const{
        return index_+size();
    }
    uint64_t begin_index() const{
        return index_;
    }
};
class RAII_Substr{
  private:
    uint64_t &absolute_number_;
    size_t& write_index_;
    size_t write_length_;
    size_t ignore_length_;
    size_t& bytes_to_write_;
    const std::string_view data_;
  public:
    explicit RAII_Substr(uint64_t &absolute_number,size_t& data_index,size_t write,size_t ignore,size_t& bytes_to_write,const std::string& data):absolute_number_(absolute_number),write_index_(data_index),write_length_(write),ignore_length_(ignore),bytes_to_write_(bytes_to_write),data_(data.data(),data.length()){
        write_length_=write;
        ignore_length_=ignore;
    };
    size_t write_length() const{
        return write_length_;
    }
    size_t ignore_length() const{
        return ignore_length_;
    }
    void ignore_first(){
        write_index_+=ignore_length_;
        bytes_to_write_-=ignore_length_;
        absolute_number_+=ignore_length_;
        ignore_length_=0;
//        return ignore_length_;
    }
    std::string to_string() const {
        return {data_.data()+write_index_,write_length_};
    }
    ~RAII_Substr(){
        bytes_to_write_-=write_length_+ignore_length_;
        write_index_+=ignore_length_+write_length_;
        absolute_number_+=ignore_length_+write_length_;
    }
};
//#endif
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream output_;  //!< The reassembled in-order byte stream
    size_t capacity_;    //!< The maximum number of bytes
    uint64_t read_;
    std::optional<uint64_t> end_;
    std::deque<ExtendBuffer> buffers_{};
  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return output_; }
    ByteStream &stream_out() { return output_; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
