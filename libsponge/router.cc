#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    DUMMY_CODE(route_prefix, prefix_length, next_hop, interface_num);
    // Your code here.

    _route_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    DUMMY_CODE(dgram);
    // Your code here.

    // ttl is zero, directly drop
    if (dgram.header().ttl == 0)
        return;
    // modify ttl
    dgram.header().ttl--;

    if (dgram.header().ttl == 0)
        return;
    // directly modify ip, then interface modify the mac

    uint64_t max_prefix = 0;
    RouteTableEntry e;
    bool matched = false;

    // find the most specific route
    for (auto &entry : _route_table) {
        uint64_t mask = getMask(entry.prefix_length);

        if ((dgram.header().dst & mask) == (entry.route_prefix & mask)) {
            // must >=. because prefix_length may be zero
            if (entry.prefix_length >= max_prefix) {
                matched = true;
                max_prefix = entry.prefix_length;
                e = entry;
            }
        }
    }

    // may be have default route, its route_prefix = 0.0.0.0, prefix_length = 0

    // no match  =>  drop directly
    if (!matched)
        return;

    // get the next_hop     =>   next_hop is empty, next_hop is dest itself; the child network in the router
    //                      =>                                               next hop is the next router's ip
    Address next_hop = e.next_hop.has_value() ? e.next_hop.value() : Address::from_ipv4_numeric(dgram.header().dst);

    // send to the interface
    _interfaces[e.interface_num].send_datagram(dgram, next_hop);
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

uint32_t Router::getMask(uint8_t prefix_length) {
    uint32_t ret = 0;

    for (int i = 0; i < prefix_length; ++i) {
        ret <<= 1;
        ret |= 1;
    }

    ret <<= (32 - prefix_length);

    return ret;
}