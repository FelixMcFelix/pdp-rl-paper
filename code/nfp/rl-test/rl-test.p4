#include <v1model.p4>

header ethernet_t {
    bit<48> eth_dst;
    bit<48> eth_src;
    bit<16> eth_type;
}

#define ETH_IPV4 0x0800
#define ETH_IPV6 0x86DD

header ipv4_t {
    bit<4> version;
    bit<4> ihl;
    bit<6> dscp;
    bit<2> ecn;
    bit<16> total_len;
    bit<16> identification;
    bit<3> flags;
    bit<15> frag_offset;
    bit<8> ttl;
    bit<8> protocol;
    bit<16> header_cksum;
    bit<32> src_ip;
    bit<32> dst_ip;
    // Options? how?
}

header ipv6_t {
    bit<4> version;
    bit<6> dscp;
    bit<2> ecn;
    bit<20> flow_label;
    bit<16> payload_len;
    bit<8> next_header;
    bit<8> hop_limit;
    bit<128> src_addr;
    bit<128> dst_addr;
}

struct headers_t {
    ethernet_t ethernet;
    ipv4_t ipv4;
    ipv6_t ipv6;
}

struct metadata_t {
    bit<8> eh;
}

parser my_parser (
    packet_in b,
    out headers_t headers,
    inout metadata_t meta,
    inout standard_metadata_t standard_metadata) {
    state start {
        b.extract(headers.ethernet);
        transition select(headers.ethernet.eth_type) {
            ETH_IPV4: ip4;
            ETH_IPV6: ip6;
            _: accept;
        }
    }

    state ip4 {
        b.extract(headers.ipv4);
        transition accept;
    }

    state ip6 {
        b.extract(headers.ipv6);
        transition accept;
    }
}

extern void filter_func();

control ingress(inout headers_t headers,
    inout metadata_t meta,
    inout standard_metadata_t standard_metadata) {
    action drop() {
        filter_func();
    }
    table at {
        key = {}
        actions = {
            drop();
        }
    }
    apply {
        at.apply(); // if this needs fields, then insert at callsite...
    }
}

control egress(inout headers_t headers,
    inout metadata_t meta,
    inout standard_metadata_t standard_metadata) {
    apply {}
}

control deparser(packet_out b, in headers_t headers) {
    apply {
        b.emit(headers.ethernet);
        b.emit(headers.ipv4);
        b.emit(headers.ipv6);
    }
}

control verify_cksum(
    inout headers_t headers, inout metadata_t meta) {
    apply {}
}

control compute_cksum(
    inout headers_t headers, inout metadata_t meta) {
    apply {}
}

V1Switch(
    my_parser(),
    verify_cksum(),
    ingress(),
    egress(),
    compute_cksum(),
    deparser()) main;