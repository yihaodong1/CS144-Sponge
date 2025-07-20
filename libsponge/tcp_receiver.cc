#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
  TCPHeader header = seg.header();
  WrappingInt32 seqno = header.seqno;
  if ( header.syn ) {
    // use start_ to enable _reassembler
    start_ = true;
    zero_point_ = seqno;
    // add 1 to indicate the byte following syn
    seqno = seqno + 1;
  }
  if ( header.rst ) {
    start_ = false;
    // set ByteStream to be error
    stream_out().set_error();
  }
  if ( start_ ) {
    seqno = seqno - 1;
    // use the first unassembled byte as checkpoint
    uint64_t checkpoint = _reassembler.acknum();
    _reassembler.push_substring( seg.payload().copy(), 
        unwrap( seqno, zero_point_, checkpoint ), header.fin );
    // use fin_ to record whether the last byte has been sent
    fin_ |= header.fin;
  }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
  optional<WrappingInt32> ack;
  if(start_)
    // fin_ && _reassembler.isempty() means all the bytes pending
    // in _reassembler has been reassembled and the FIN has benn sent
    // and FIN occupy one seqno
    ack = wrap( _reassembler.acknum() + 1 + ( fin_ && _reassembler.empty() ), zero_point_ );
  return ack;
}

size_t TCPReceiver::window_size() const {
  // return stream_out().remaining_capacity() > UINT16_MAX ? UINT16_MAX : stream_out().remaining_capacity();
  return stream_out().remaining_capacity();
}
