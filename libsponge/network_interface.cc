#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame f;
    if(_ip_to_eth_table.find(next_hop_ip) != _ip_to_eth_table.end()){
      EthernetAddress dst = _ip_to_eth_table[next_hop_ip].addr;
      f.header().dst = dst;
      f.header().src = _ethernet_address;
      f.header().type = EthernetHeader::TYPE_IPv4;
      f.payload().append(dgram.serialize());
      _frames_out.push(f);
    }else{
      if(_time_table.find(next_hop_ip) != _time_table.end()
        && _current - _time_table[next_hop_ip] < 5000){
          return;
      }else{
        f.header().dst = ETHERNET_BROADCAST;
        f.header().src = _ethernet_address;
        f.header().type = EthernetHeader::TYPE_ARP;
        ARPMessage a;
        a.opcode = ARPMessage::OPCODE_REQUEST;
        a.sender_ethernet_address = _ethernet_address;
        a.sender_ip_address = _ip_address.ipv4_numeric();
        a.target_ip_address = next_hop_ip;
        f.payload().append(a.serialize());
        _frames_out.push(f);
        _time_table[next_hop_ip] = _current;
        _dgram_table[next_hop_ip] = dgram;
      }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst == ETHERNET_BROADCAST || frame.header().dst == _ethernet_address){
      if(frame.header().type == EthernetHeader::TYPE_IPv4){
        InternetDatagram i;
        if(i.parse(frame.payload()) == ParseResult::NoError){
          return i;
        }
      }else if(frame.header().type == EthernetHeader::TYPE_ARP){
        ARPMessage a;

        if(a.parse(frame.payload()) == ParseResult::NoError){
          refresh(a.sender_ip_address, a.sender_ethernet_address);
          // _ip_to_eth_table[a.sender_ip_address] = a.sender_ethernet_address;
        }else{
          return {};
        }

        if(a.opcode == ARPMessage::OPCODE_REQUEST && a.target_ip_address == _ip_address.ipv4_numeric()){
          EthernetFrame f = frame;
          f.header().dst = frame.header().src;
          f.header().src = _ethernet_address;
          ARPMessage m;
          m.sender_ethernet_address = _ethernet_address;
          m.sender_ip_address = _ip_address.ipv4_numeric();
          m.target_ethernet_address = a.sender_ethernet_address;
          m.target_ip_address = a.sender_ip_address;
          m.opcode = ARPMessage::OPCODE_REPLY;
          f.payload() = m.serialize();
          _frames_out.push(f);
        }

        if(a.opcode == ARPMessage::OPCODE_REPLY){
          if(_time_table.find(a.sender_ip_address) != _time_table.end()){
            EthernetFrame f;
            _time_table.erase(a.sender_ip_address);
            InternetDatagram dgram = _dgram_table[a.sender_ip_address];
            _dgram_table.erase(a.sender_ip_address);
            f.header().dst = a.sender_ethernet_address;
            f.header().src = _ethernet_address;
            f.header().type = EthernetHeader::TYPE_IPv4;
            f.payload().append(dgram.serialize());
            _frames_out.push(f);
          }
        }
      }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
  _current += ms_since_last_tick;
  refresh(optional<uint32_t>(), optional<EthernetAddress>());
}

void NetworkInterface::refresh(optional<uint32_t> ip_addr, optional<EthernetAddress> eth_addr){
  if(ip_addr.has_value()){
    auto it = _ip_to_eth_table.find(ip_addr.value());
    if(it != _ip_to_eth_table.end()){
      _expire_table.erase(it->second.ptr);
    }

    ExpireItem item(ip_addr.value(), _current);
    auto expire_it = _expire_table.insert(item); // 插入新的过期项并保存其迭代器

    // 更新 ip->eth 映射表，同时记录指向 expire_table 的指针
    _ip_to_eth_table.insert_or_assign(ip_addr.value(), EthItem(eth_addr.value(), expire_it));
    // _ip_to_eth_table[ip_addr.value()] = EthItem(eth_addr.value(), expire_it);

  }else{
    while (!_expire_table.empty() && _expire_table.begin()->_time + 30000 <= _current) {
        auto item = *_expire_table.begin();
        _expire_table.erase(_expire_table.begin());
        _ip_to_eth_table.erase(item._ip);
    }
  }
}
