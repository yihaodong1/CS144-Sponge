#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

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
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , RTO_ms_{retx_timeout}{}

TCPSegment TCPSender::construct_seg( uint16_t window_size, bool& flag )
{
  TCPSegment seg;
  seg.header().seqno = wrap(_next_seqno, _isn);
  seg.header().rst = _stream.error();
  seg.header().syn = _next_seqno == 0;

  // use flag to decide whether send msg
  // When window_size_ > bytes_fly_, there is capability for SYN
  flag |= ( seg.header().syn && ( window_size > bytes_fly_ ) );

  // Calculate the length of payload
  uint64_t len
    = window_size > bytes_fly_ ? min( static_cast<size_t>(window_size) - bytes_fly_, TCPConfig::MAX_PAYLOAD_SIZE ) - seg.header().syn : 0;
  if ( _stream.buffer_size() && len > 0 ) {
    // When there are payload, should send msg
    flag |= 1;
    seg.payload() = Buffer(_stream.read(len));
  }

  if ( _stream.eof()
       // as FIN also occupy space
       && window_size > bytes_fly_ + seg.length_in_sequence_space() ) {
    seg.header().fin = true;
    flag |= 1;
  }
  return seg;
}
uint64_t TCPSender::bytes_in_flight() const { return bytes_fly_; }

void TCPSender::fill_window() {
  while ( window_size_ > bytes_fly_ ) {

    bool flag = false;
    TCPSegment seg = construct_seg( window_size_, flag );
    if ( flag && !fin_ ) {
      // use fin_ to record the send has finished, avoiding
      // sending FIN repeatly
      fin_ = seg.header().fin;
      _next_seqno += seg.length_in_sequence_space();
      bytes_fly_ += seg.length_in_sequence_space();
      queue_.push( seg );
      _segments_out.push(seg);
      // Start the timer
      timer_start_ = true;
    } else {
      break;
    }
  }
  // Special case for window_size_ == 0
  if ( window_size_ == 0 ) {
    bool flag = false;
    TCPSegment seg = construct_seg( 1, flag );
    if ( flag && !fin_ ) {
      fin_ = seg.header().fin;
      _next_seqno += seg.length_in_sequence_space();
      bytes_fly_ += seg.length_in_sequence_space();
      queue_.push( seg );
      _segments_out.push(seg);
      timer_start_ = true;
    }
  }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
  if(unwrap( ackno, _isn, bytes_acked_ ) <= _next_seqno){
    window_size_ = window_size;
    while ( !queue_.empty() ) {
      // find the msg qualified
      auto it = queue_.front();
      if (unwrap( it.header().seqno, _isn, bytes_acked_ ) + it.length_in_sequence_space()
          > unwrap( ackno, _isn, bytes_acked_ ) ) {
        break;
      }
      bytes_fly_ -= it.length_in_sequence_space();
      bytes_acked_ += it.length_in_sequence_space();
      queue_.pop();

      // if there are no bytes_fly_, no need to start timer
      timer_start_ = bytes_fly_ != 0;
      RTO_ms_ = _initial_retransmission_timeout;
      transmissions_ = 0;
      current_time_ = 0;
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
  if ( timer_start_ )
    current_time_ += ms_since_last_tick;
  if ( current_time_ >= RTO_ms_ ) {
    if ( !queue_.empty() ) {
      // find the earliest msg to retransmit
      auto it = queue_.front();
      _segments_out.push(it);
    }
    if ( window_size_ ) {
      RTO_ms_ = RTO_ms_ << 1;
      ++transmissions_;
    }
    current_time_ = 0;
  }
}

unsigned int TCPSender::consecutive_retransmissions() const { return transmissions_; }

void TCPSender::send_empty_segment() {
  TCPSegment seg;
  seg.header().seqno = wrap(_next_seqno, _isn);
  seg.header().rst = _stream.error();
  _segments_out.push(seg);
}
