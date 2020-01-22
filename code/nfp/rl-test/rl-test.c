#include <pif_plugin.h>

// Seems to be some  magic naming here: this connects to the p4 action "filter_func"
int pif_plugin_filter_func(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data) {
    return PIF_PLUGIN_RETURN_DROP;
}