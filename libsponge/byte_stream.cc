#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity): capacity_(capacity) {}

size_t ByteStream::write(const string &data) {
  size_t a = remaining_capacity();
  if ( a > data.size() ) {
    buffer += data;
    bytes_pushed_ += data.size();
    return data.size();
  } else {
    buffer += data.substr( 0, a );
    bytes_pushed_ += a;
    return a;
  }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
  return buffer.substr(0, len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
  size_t buffered = buffer_size();
  if ( buffered > len ) {
    buffer = buffer.substr( len );
    bytes_popped_ += len;
  } else {
    buffer.erase();
    bytes_popped_ += buffered;
  }
}
//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
  std::string s = peek_output(len);
  pop_output(len);
  return s;
}

void ByteStream::end_input() { close_ = true;}

bool ByteStream::input_ended() const { return close_; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.size() == 0; }

bool ByteStream::eof() const { return close_ && buffer_empty(); }

size_t ByteStream::bytes_written() const { return bytes_pushed_; }

size_t ByteStream::bytes_read() const { return bytes_popped_; }

size_t ByteStream::remaining_capacity() const { return capacity_ - buffer.size(); }
