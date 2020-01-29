#include <pif_plugin.h>

// https://groups.google.com/d/msg/open-nfp/K9qJRHPsf4c/lZSC7_BhCAAJ
// rely on this link, it has much useful knowlwedge about how to access payload
// in current version of sdk...

// Seems to be some  magic naming here: this connects to the p4 action "filter_func"
int pif_plugin_filter_func(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data) {
    __mem40 uint8_t *payload;
    uint32_t mu_len, ctm_len;
    int count, to_read;

    // packet my be split between "cluster target memory"
    // and "MU".
    // parsed headers are always in CTM, apparently.
    if (pif_pkt_info_global.p_is_split) {
        mu_len = pif_pkt_info_global.p_len - ((256 << pif_pkt_info_global.p_ctm_size) - pkt.p_offset);
    } else {
        mu_len = 0;
    }

    count = pif_pkt_info_global.pkt_len - pif_pkt_info_global.pkt_pl_off - mu_len;
    payload = pky_ctm_ptr40(pkt.p_isl, pkt.p_pnum, pkt.p_offset);
    payload += pif_pkt_info_spec.pkt_pl_off;

    // Do stuff with payload (CTM chunk)
    // TODO
    while (count) {
        to_read = 0; // max of chunk size and count

        // STUDY lines 66--92 of that file.
        // seem to nead to use "mem_read8" to pull data into
        // tx registers, then pull out if I want to iterate over them...
        break;
    }

    // Do stuff with remainder of payload.
    if (mu_len) {
        // Point to the portion of payload data in MU.
        // and then index in by the amount of data which was kept locally in ctm,
        payload = (__mem40 void *)((uint64_t)pif_pkt_info_global.p_muptr << 11);
        payload += 256 << pif_pkt_info_global.p_ctm_size;

        count = mu_len;
        while (count) {
            break;
        }
    }

    return PIF_PLUGIN_RETURN_DROP;
}