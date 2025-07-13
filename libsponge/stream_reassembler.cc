#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
  uint64_t orig_len = data.size();
  uint64_t capacity = _output.remaining_capacity();
  uint64_t left_bound = max( index, current );
  uint64_t right_bound = min( index + data.size(), current + capacity );
  // use ready_close to record if it is to be close,
  // when is_last_substring and the last substr right
  ready_close |= ( eof && index + orig_len == right_bound );
  if ( left_bound >= right_bound ) {
    if ( ready_close && pend_ == 0 )
      _output.end_input();
    return;
  }
  string data1 = data.substr( left_bound - index, right_bound - left_bound );

  auto it = m.lower_bound( left_bound );
  // find the overlap between the next one
  if ( it != m.end() && it->first <= right_bound ) {
    while ( it != m.end() && it->first <= right_bound ) {
      // use loop to combine all the substr, like insert b, d, abcd
      if ( it->first + it->second.size() > right_bound ) {
        data1.replace( data1.begin() + it->first - left_bound, data1.end(), it->second );
        right_bound = it->first + it->second.size();
      }
      pend_ -= it->second.size();
      m.erase( it );
      it = m.lower_bound( left_bound );
    }
  }
  // find the overlap between the previous one
  if ( it != m.begin() ) {
    auto prev_it = prev( it );
    if ( prev_it->first + prev_it->second.size() >= left_bound ) {
      if ( prev_it->first + prev_it->second.size() < right_bound ) {
        pend_ += right_bound - prev_it->first - prev_it->second.size();
        prev_it->second.replace(
          prev_it->second.begin() + left_bound - prev_it->first, prev_it->second.end(), data1 );
      }
    } else {
      // no overlap between the previous one
      m.insert( make_pair( left_bound, data1 ) );
      pend_ += data1.size();
    }

  } else {
    // there is no previous one
    m.insert( make_pair( left_bound, data1 ) );
    pend_ += data1.size();
  }

  it = m.lower_bound( current );
  while ( it != m.end() && it->first == current ) {
    // push the Reassembled byte into writer
    _output.write( it->second );
    current += it->second.size();
    pend_ -= it->second.size();
    m.erase( it );
    it = m.lower_bound( current );
  }
  if ( ready_close && pend_ == 0 )
    _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return pend_; }

bool StreamReassembler::empty() const { return pend_ == 0; }
