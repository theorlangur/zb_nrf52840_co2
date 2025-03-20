#ifndef ZB_DESC_HELPER_TYPES_EP_HPP_
#define ZB_DESC_HELPER_TYPES_EP_HPP_

#include "zb_desc_helper_types_cluster.hpp"

namespace zb
{
    template<size_t ServerCount, size_t ClientCount>
    struct TSimpleDesc: zb_af_simple_desc_1_1_t
    {
        zb_uint16_t app_cluster_list_ext[(ServerCount + ClientCount) - 2];
    } ZB_PACKED_STRUCT;

    template<class MemPtr>
    struct mem_ptr_traits
    {
        static constexpr bool is_mem_ptr = false;
    };

    template<class T, class MemT>
    struct mem_ptr_traits<MemT T::*>
    {
        static constexpr bool is_mem_ptr = true;
        using ClassType = T;
        using MemberType = MemT;
    };

    
    template<cluster_info_t ci, auto mem_desc> requires requires { typename decltype(mem_desc)::MemT; }
    struct AttributeAccess
    {
        using MemT = decltype(mem_desc)::MemT;

        auto operator=(MemT const& v)
        {
            return zb_zcl_set_attr_val(ep.ep_id, ci.id, (zb_uint8_t)ci.role, mem_desc.id, (zb_uint8_t*)&v, ZB_TRUE);
        }

        zb_af_endpoint_desc_t &ep;
    };

    struct EPClusterAttributeDesc_t
    {
        static constexpr uint16_t kANY_CLUSTER = 0xffff;
        static constexpr uint16_t kANY_ATTRIBUTE = 0xffff;
        static constexpr uint8_t  kANY_EP = 0xff;

        zb_uint8_t ep = kANY_EP;
        uint16_t cluster = kANY_CLUSTER;
        uint16_t attribute = kANY_ATTRIBUTE;

        constexpr bool fits(zb_uint8_t _ep, uint16_t _cluster, uint16_t _attr) const
        {
            return ((_ep == ep) || (ep == kANY_EP))
                && ((_cluster == cluster) || (cluster == kANY_CLUSTER))
                && ((_attr == attribute) || (attribute == kANY_ATTRIBUTE));
        }
    };

    struct EPBaseInfo
    {
        zb_uint8_t ep;
        zb_uint16_t dev_id;
        zb_uint8_t dev_ver;
    };

    template<EPBaseInfo i, class Clusters>
    struct EPDesc
    {
        using SimpleDesc = TSimpleDesc<Clusters::server_cluster_count(), Clusters::client_cluster_count()>;

        template<class T1, class T2, class... T> requires std::is_same_v<TClusterList<T1, T2, T...>, Clusters>
        constexpr EPDesc(TClusterList<T1, T2, T...> &clusters):
            simple_desc{ 
                {
                    .endpoint = i.ep, 
                    .app_profile_id = ZB_AF_HA_PROFILE_ID, 
                    .app_device_id = i.dev_id,
                    .app_device_version = i.dev_ver,
                    .reserved = 0,
                    .app_input_cluster_count = Clusters::server_cluster_count(),
                    .app_output_cluster_count = Clusters::client_cluster_count(),
                    .app_cluster_list = {T1::info().id, T2::info().id}
                },
                { T::info().id... }//rest
            },
            rep_ctx{},
            lev_ctrl_ctx{},
            ep{
                .ep_id = i.ep,
                .profile_id = ZB_AF_HA_PROFILE_ID,
                .device_handler = nullptr,
                .identify_handler = nullptr,
                .reserved_size = 0,
                .reserved_ptr = nullptr,
                .cluster_count = sizeof...(T) + 2,
                .cluster_desc_list = clusters.clusters,
                .simple_desc = &simple_desc,
                .rep_info_count = Clusters::reporting_attributes_count(),
                .reporting_info = rep_ctx,
                .cvc_alarm_count = Clusters::cvc_level_ctrl_attributes_count(),
                .cvc_alarm_info = lev_ctrl_ctx
            }
        {
        }

        template<auto memPtr>
        auto attr()
        {
            using MemPtrType = decltype(memPtr);
            static_assert(mem_ptr_traits<MemPtrType>::is_mem_ptr, "Only member pointer is allowed");

            using ClassType = mem_ptr_traits<MemPtrType>::ClassType;
            //using MemType = mem_ptr_traits<MemPtrType>::MemberType;
            using ClusterDescType = decltype(get_cluster_description<ClassType>());
            static_assert(Clusters::has_info(ClusterDescType::info()), "Requested cluster is not part of the EP");

            return AttributeAccess<ClusterDescType::info(), ClusterDescType::template get_member_description<memPtr>()>{ep};
        }

        template<auto memPtr>
        constexpr EPClusterAttributeDesc_t handler_filter_for_attribute()
        {
            using MemPtrType = decltype(memPtr);
            static_assert(mem_ptr_traits<MemPtrType>::is_mem_ptr, "Only member pointer is allowed");

            using ClassType = mem_ptr_traits<MemPtrType>::ClassType;
            //using MemType = mem_ptr_traits<MemPtrType>::MemberType;
            using ClusterDescType = decltype(get_cluster_description<ClassType>());
            static_assert(Clusters::has_info(ClusterDescType::info()), "Requested cluster is not part of the EP");
            return {.ep = i.ep, .cluster = ClusterDescType::info().id, .attribute = ClusterDescType::template get_member_description<memPtr>().id};
        }

        template<class Cluster>
        constexpr EPClusterAttributeDesc_t handler_filter_for_cluster()
        {
            using ClusterDescType = decltype(get_cluster_description<Cluster>());
            static_assert(Clusters::has_info(ClusterDescType::info()), "Requested cluster is not part of the EP");
            return {.ep = i.ep, .cluster = ClusterDescType::info().id};
        }

        SimpleDesc simple_desc;
        zb_zcl_reporting_info_t rep_ctx[Clusters::reporting_attributes_count()];
        zb_zcl_cvc_alarm_variables_t lev_ctrl_ctx[Clusters::cvc_level_ctrl_attributes_count()];
        zb_af_endpoint_desc_t ep;
    };
}
#endif
