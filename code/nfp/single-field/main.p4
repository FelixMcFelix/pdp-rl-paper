#include <v1model.p4>

header ethernet_t {
    bit<48> eth_dst;
    bit<48> eth_src;
    bit<16> eth_type;
}

struct headers_t {
    ethernet_t ethernet;
}

struct metadata_t {
}

parser my_parser (
    packet_in b,
    out headers_t headers,
    inout metadata_t meta,
    inout standard_metadata_t standard_metadata)
{
    state start {
        b.extract(headers.ethernet);
        transition accept;
    }
}

control ingress(inout headers_t headers,
    inout metadata_t meta,
    inout standard_metadata_t standard_metadata)
{
    action set_egress_spec(bit<16> port) {
        standard_metadata.egress_spec = port;
    }
    action drop() {
        mark_to_drop();
    }
    table forward {
        key = { headers.ethernet.eth_dst: exact; }
        actions = {
            set_egress_spec;
            drop;
            NoAction;
        }
        size = 65536;
        default_action = drop();
    }
    apply {
        forward.apply();
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