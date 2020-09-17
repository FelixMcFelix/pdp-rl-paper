#include <v1model.p4>

header ethernet_t {
    bit<48> eth_dst;
    bit<48> eth_src;
    bit<16> eth_type;
}

header ipv4_t {
    bit<4> version;
    bit<4> ihl;
    bit<6> dscp;
    bit<2> ecn;
    bit<16> total_len;
    bit<16> identification;
    bit<3> flags;
    bit<13> frag_offset;
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

// Chase these up with Netronome. Seem to be malfunctioning.
header_union ip_t {
    ipv4_t ip4;
    ipv6_t ip6;
}

header udp_t {
    bit<16> src_port;
    bit<16> dst_port;
    bit<16> len;
    bit<16> cksum;
}

header tcp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<32> seqNo;
    bit<32> ackNo;
    bit<4>  dataOffset;
    bit<3>  res;
    bit<3>  ecn;
    bit<6>  ctrl;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgentPtr;
}

header rl_t {
    bit<8> rl_type;
}

header rl_cfg_t {
    bit<8> cfg_type;
}

header rl_ins_t {
    bit<32> offset;
}

struct headers_t {
    ethernet_t ethernet;
    //ip_t ip;
    ipv4_t ip4;
    ipv6_t ip6;
    tcp_t tcp;
    udp_t udp;
    rl_t rt;
    rl_cfg_t rct;
    rl_ins_t ins;
}

// NOTE: think about header_union for ipv4/6, and for RL messages?

struct metadata_t {
    bit<8> eh;
}

// So calling this causes a compiler crash. Neat.
parser rl_parser (
    packet_in b,
    out headers_t headers,
    inout metadata_t meta,
    inout standard_metadata_t standard_metadata) {

    const bit<8> RL_T_CFG = 0;
    const bit<8> RL_T_INS = 1;

    const bit<8> RL_CFG_T_TILES = 0;

    state start {
        b.extract(headers.rt);
        transition select(headers.rt.rl_type) {
            RL_T_CFG: cfg;
            RL_T_INS: insert;
            _: reject;
        }
    }

    state cfg {
        b.extract(headers.rct);
        transition accept;
    }

    state insert {
        b.extract(headers.ins);
        transition accept;
    }
}

parser my_parser (
    packet_in b,
    out headers_t headers,
    inout metadata_t meta,
    inout standard_metadata_t standard_metadata) {

    const bit<16> ETH_IPV4 = 0x0800;
    const bit<16> ETH_IPV6 = 0x86DD;

    const bit<6> DSCP_TRAP = 0b000011;

    const bit<8> PROTO_TCP = 0x06;
    const bit<8> PROTO_UDP = 0x11;

    const bit<8> RL_T_CFG = 0;
    const bit<8> RL_T_INS = 1;

    const bit<8> RL_CFG_T_TILES = 0;

    state start {
        b.extract(headers.ethernet);
        transition select(headers.ethernet.eth_type) {
            ETH_IPV4: s_ip4;
            ETH_IPV6: s_ip6;
            _: accept;
        }
    }

    state s_ip4 {
        //b.extract(headers.ip.ip4);
        b.extract(headers.ip4);
        /* DSCP determines whether these are RL control packets. */
        //transition select(headers.ip.ip4.dscp) {
        transition select(headers.ip4.dscp) {
            DSCP_TRAP: rl;
            _: ip4_transport;
        }
    }

    state ip4_transport {
        //transition select(headers.ip.ip4.protocol) {
        transition select(headers.ip4.protocol) {
            PROTO_TCP: tx_tcp;
            PROTO_UDP: tx_udp;
            _: accept;
        }
    }

    state s_ip6 {
        // b.extract(headers.ip.ip6);
        b.extract(headers.ip6);
        //transition select(headers.ip.ip6.dscp) {
        transition select(headers.ip6.dscp) {
            DSCP_TRAP: rl;
            _: ip6_transport;
        }
    }

    state ip6_transport {
        //transition select(headers.ip.ip6.next_header) {
        transition select(headers.ip6.next_header) {
            PROTO_TCP: tx_tcp;
            PROTO_UDP: tx_udp;
            _: accept;
        }
    }

    // NOTE: these are named tx_* because the build randomly breaks if I just use tcp/udp.
    // Name overlap?
    state tx_udp {
        b.extract(headers.udp);
        transition accept;
    }

    state tx_tcp {
        b.extract(headers.tcp);
        transition accept;
    }

    state rl {
        b.extract(headers.udp);
        // rl_parser.apply(b, headers, meta, standard_metadata);
        b.extract(headers.rt);
        transition select(headers.rt.rl_type) {
            RL_T_CFG: cfg;
            RL_T_INS: insert;
        }
    }

    state cfg {
        b.extract(headers.rct);
        transition accept;
    }

    state insert {
        b.extract(headers.ins);
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
        standard_metadata.egress_spec = standard_metadata.ingress_port;
        bit<48> tmp = headers.ethernet.eth_src;
        headers.ethernet.eth_src = headers.ethernet.eth_dst;
        headers.ethernet.eth_dst = tmp;
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
        //b.emit(headers.ip);
        b.emit(headers.ip4);
        b.emit(headers.ip6);
        b.emit(headers.tcp);
        b.emit(headers.udp);
        b.emit(headers.rt);
        b.emit(headers.rct);
        b.emit(headers.ins);
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