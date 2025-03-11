#ifndef ZB_MAIN_HPP_
#define ZB_MAIN_HPP_

#include "zb_desc_helper_types_device.hpp"

namespace zb
{
    //returns TAttributeList with properly initialized array of zb_zcl_attr_t 
    //based on the information obtained via get_cluster_description for the passed structure type of ZbS
    template<class ZbS>
    constexpr auto to_attributes(ZbS &s) { return cluster_struct_to_attr_list(s, get_cluster_description<ZbS>()); }

    //given multiples of TAttributeList<> objects this constructs and returns a TClusterList<..> 
    //with a properly initialized array of zb_zcl_cluster_desc_t
    template<class... ClusterAttributesDesc>
    constexpr auto to_clusters(ClusterAttributesDesc&... c) -> TClusterList<ClusterAttributesDesc...> { return {c...}; }

    //given multiples clusters in form of TClusterList<> this constructs and returns a EPDesc<..> 
    //with a proper zb_af_simple_desc_1_1_t-derived simple description and a properyl initialized zb_af_endpoint_desc_t
    //also with the automatically calculated and reserved space for reporting attributes and level control alarms
    template<class... T>
    constexpr auto configure_ep(zb_uint8_t ep, zb_uint16_t dev_id, zb_uint8_t dev_ver, TClusterList<T...> &clusters) -> EPDesc<TClusterList<T...>>
    {
        return {ep, dev_id, dev_ver, clusters};
    }

    //packs existing Endpoints together into a Device, with properly initialized zb_af_device_ctx_t
    template<class... Clusters>
    constexpr auto configure_device(EPDesc<Clusters>&...eps) { return Device{eps...}; }
}

#endif
