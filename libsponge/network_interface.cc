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

    // assemble EthernetFrame

    // header
    EthernetFrame frame;
    frame.header() = assembleOneEtherHeader(_ethernet_address, _ethernet_address, EthernetHeader::TYPE_IPv4);
    frame.payload() = std::move(dgram.serialize());

    // if ip in map
    // send right away
    if (_ip2mac.find(next_hop_ip) != _ip2mac.end()) {
        // fill the dst's ethernet_address
        frame.header().dst = _ip2mac.find(next_hop_ip)->second.second;
        _frames_out.push(std::move(frame));
        return;
    }


    // ip not in map

    // send to wait queue
    if (_waitArp.find(next_hop_ip) == _waitArp.end())
        _waitArp[next_hop_ip] = make_shared<queue<EthernetFrame>>();
    
    _waitArp[next_hop_ip]->push(std::move(frame));

    // send arp request

    // in the last 5 seconds send, not send
    auto iter = _arp_requests.find(next_hop_ip);

    if (iter != _arp_requests.end()) {
        // timeout, resend
        if (iter->second.first <= _tick) {
            _frames_out.push(iter->second.second);
            iter->second.first = _tick + 5 * 1000;
        }

        // no timeout, no send
        return;
    }


    // no find, send
    EthernetFrame arpFrame;
    // arp msg's dst should be empty, namely invalid
    ARPMessage arpMsg = assembleOneARP(_ethernet_address, {0}, _ip_address.ipv4_numeric(), next_hop_ip, ARPMessage::OPCODE_REQUEST);
    arpFrame.header() = assembleOneEtherHeader(_ethernet_address, ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP);
    arpFrame.payload() = std::move(arpMsg.serialize());

    _arp_requests[next_hop_ip] = {_tick + 5 * 1000, arpFrame};

    _frames_out.push(std::move(arpFrame));
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {

    switch (frame.header().type) {
    case EthernetHeader::TYPE_ARP:
    { 
        ARPMessage arpMsg;
        // parse error
        if (arpMsg.parse(frame.payload().concatenate()) != ParseResult::NoError) return {};
        
        // dest ip is not me
        if (arpMsg.target_ip_address != _ip_address.ipv4_numeric()) return {};


        if (arpMsg.opcode == ARPMessage::OPCODE_REQUEST) {
            // request, dst should be ff:ff:ff:ff:ff:ff
            if (frame.header().dst != ETHERNET_BROADCAST) return {};

            // 此处应该将 主动送上门的 ip->mac 映射起来
            _ip2mac[arpMsg.sender_ip_address] = {_tick + 30 * 1000, arpMsg.sender_ethernet_address};

            // receive arp request
            EthernetFrame f;
            arpMsg = assembleOneARP(_ethernet_address, arpMsg.sender_ethernet_address, _ip_address.ipv4_numeric(), arpMsg.sender_ip_address, ARPMessage::OPCODE_REPLY);

            f.header() = assembleOneEtherHeader(_ethernet_address, frame.header().src, EthernetHeader::TYPE_ARP);
            f.payload() = std::move(arpMsg.serialize());

            _frames_out.push(f);
            
        } else if (arpMsg.opcode == ARPMessage::OPCODE_REPLY) {
            
            // reply, dst should be my mac address
            if (frame.header().dst != _ethernet_address) return {};
            // receive arp reply
            EthernetAddress mac = arpMsg.sender_ethernet_address;
            uint32_t ip = arpMsg.sender_ip_address;

            // insert to cache            
            _ip2mac.insert({ip, {_tick + 30 * 1000, mac}});

            // remove from _arp_requests
            auto iter = _arp_requests.find(ip);
            if (iter != _arp_requests.end())
                _arp_requests.erase(iter);

            // send all waiting frame
            sendFrameWaitArp(ip, mac);
        }

        return {};
        break;
    }
    case EthernetHeader::TYPE_IPv4:
    {
       // the frame is not sent to me
       if (frame.header().dst != _ethernet_address) return {};

       InternetDatagram datagram;

        if (datagram.parse(frame.payload().concatenate()) != ParseResult::NoError) return {};

        return optional<InternetDatagram>{std::move(datagram)};
        
        break;
    }

    default:
        return {};
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    _tick += ms_since_last_tick;

    // traverse _ip2mac to check whether out of date or not
    auto iter = _ip2mac.begin();

    while (iter != _ip2mac.end()) {
        // out of date
        auto next_iter = ++iter;
        
        --iter;

        if (iter->second.first <= _tick)
            _ip2mac.erase(iter);

        iter = next_iter;
    }
}



ARPMessage NetworkInterface::assembleOneARP(EthernetAddress src, EthernetAddress dst, uint32_t ip_src, uint32_t ip_dst, uint16_t opcode) {
    
    ARPMessage arpMsg;
    arpMsg.opcode = opcode;
    arpMsg.sender_ip_address = ip_src;
    arpMsg.target_ip_address = ip_dst;
    arpMsg.sender_ethernet_address = src;
    arpMsg.target_ethernet_address = dst;

    return arpMsg;
}

EthernetHeader NetworkInterface::assembleOneEtherHeader(EthernetAddress src, EthernetAddress dst, uint16_t type) {
    EthernetHeader etherHeader;
    etherHeader.src = src;
    etherHeader.dst = dst;
    etherHeader.type = type;
    return etherHeader;
}


void NetworkInterface::sendFrameWaitArp(uint32_t ip, EthernetAddress mac) {

        // send all this ip's segment
        auto wait_send_iter = _waitArp.find(ip);
        if (wait_send_iter == _waitArp.end()) return;

        shared_ptr<queue<EthernetFrame>> wait_send_queue_ptr = wait_send_iter->second;

        while (!wait_send_queue_ptr->empty()) {

            EthernetFrame f = wait_send_queue_ptr->front();
            wait_send_queue_ptr->pop();
            f.header().dst = mac;

            _frames_out.push(f);
        }

        _waitArp.erase(wait_send_iter);
}