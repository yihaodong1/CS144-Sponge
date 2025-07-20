#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPConnection:: add_ackno_and_win_size(){
  while(!_sender.segments_out().empty()){
    auto seg = std::move(_sender.segments_out().front());
    _sender.segments_out().pop();
    seg.header().ack = _receiver.ackno().has_value();
    if(seg.header().ack)
      seg.header().ackno = _receiver.ackno().value();
    // header.win is uint16_t
    seg.header().win = _receiver.window_size() > UINT16_MAX? UINT16_MAX: _receiver.window_size();
    _segments_out.emplace(std::move(seg));
  }
}
size_t TCPConnection::remaining_outbound_capacity() const {
  return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
  return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
  return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
  return _current - _time_last_seg_receive;
}

void TCPConnection::segment_received(const TCPSegment &seg) {

  _time_last_seg_receive = _current;
  if(seg.header().rst){
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
    return;
  }
  // cout<<TCPState::state_summary(_sender)<<endl;
  // cout<<TCPState::state_summary(_receiver)<<endl;
  // cout<<seg.header().ack<<seg.header().syn<<endl;
  if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::LISTEN)
      && seg.header().syn){
    _receiver.segment_received(seg);
    _sender.fill_window();// receive syn then send syn+ack
  }
  else if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::SYN_SENT)
      && seg.header().syn){
    _receiver.segment_received(seg);
    if(seg.header().ack){
      _sender.ack_received(seg.header().ackno, seg.header().win);
    }else
      _sender.fill_window();// receive syn then send syn+ack(simultanenously open)
  }
  else if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::SYN_RCVD)
      && seg.header().ack){
    _receiver.segment_received(seg);
    _sender.ack_received(seg.header().ackno, seg.header().win);
  }
  else if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::ESTABLISHED)){
    _receiver.segment_received(seg);
    if(seg.header().ack)
      _sender.ack_received(seg.header().ackno, seg.header().win);
    _sender.fill_window();
    _linger_after_streams_finish = !seg.header().fin;
    
  }
  else if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::CLOSE_WAIT)){
    _receiver.segment_received(seg);
    if(seg.header().ack)
      _sender.ack_received(seg.header().ackno, seg.header().win);
    _sender.fill_window();
  }
  else if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::LAST_ACK)
    && seg.header().ack){
    _receiver.segment_received(seg);
    // check whether the ack_received is in effective
    size_t ini = _sender.bytes_in_flight();
    _sender.ack_received(seg.header().ackno, seg.header().win);
    if(ini > _sender.bytes_in_flight())
      _active = false;
  }
  else if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::FIN_WAIT_1)){
    _receiver.segment_received(seg);
    if(seg.header().ack )
      _sender.ack_received(seg.header().ackno, seg.header().win);
    if(seg.header().fin){
    }
  }
  else if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::FIN_WAIT_2)
      && seg.header().fin){
    _receiver.segment_received(seg);
  }
  else if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::CLOSING)
      && seg.header().ack){
    _receiver.segment_received(seg);
    _sender.ack_received(seg.header().ackno, seg.header().win);
  }

    // TODO: make sure at least one reply
  if(_sender.segments_out().empty() && seg.length_in_sequence_space()){
    _sender.send_empty_segment();
  }
  add_ackno_and_win_size();

}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
  size_t len = _sender.stream_in().write(data);
  _sender.fill_window();
  add_ackno_and_win_size();
  return len;

}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
  _sender.tick(ms_since_last_tick);
  _current += ms_since_last_tick;
  if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
    // abort the connection, and send a reset segment to the peer (an empty segment with
    // the rst flag set)

    // clear the sender segments_out
    _sender.segments_out() = queue<TCPSegment>();

    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
    _sender.send_empty_segment();
  }
  // TODO: end connection cleanly
  if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
      == TCPState(TCPState::State::TIME_WAIT)
      && time_since_last_segment_received() >= 10 * _cfg.rt_timeout)
      _active = false;
  add_ackno_and_win_size();
}

void TCPConnection::end_input_stream() {
  // from syn_acked to fin_sent
  _sender.stream_in().end_input();
  _sender.fill_window();
  add_ackno_and_win_size();
}

void TCPConnection::connect() {
  // sender from close to connect
  _sender.fill_window();
  add_ackno_and_win_size();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
            _active = false;
            _sender.send_empty_segment();
            add_ackno_and_win_size();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
