#ifndef ZB_DESC_HELPER_TYPES_HPP_
#define ZB_DESC_HELPER_TYPES_HPP_

#include "zb_types.hpp"

namespace zb
{
    //this template function is to be specialized for every cluster struct
    template<class zb_struct>
    constexpr inline auto get_cluster_description() { static_assert(sizeof(zb_struct) == 0, "Cluster description not found for type"); }

    //base template + a macro to get proper init functions for server/client roles depending on the cluster
    template<zb_uint16_t id> constexpr zb_zcl_cluster_init_t get_cluster_init(Role r) { static_assert(id >= 0xfc00, "get_cluster_init not specialized for this cluster!"); return nullptr; }
#define DEFINE_GET_CLUSTER_INIT_FOR(cid) template<> constexpr zb_zcl_cluster_init_t get_cluster_init<cid>(Role r) { return r == Role::Server ? cid##_SERVER_ROLE_INIT : (r == Role::Client ? cid##_CLIENT_ROLE_INIT : NULL); }
#define DEFINE_NULL_CLUSTER_INIT_FOR(cid) template<> constexpr zb_zcl_cluster_init_t get_cluster_init<cid>(Role r) { return NULL; }


}
#endif
