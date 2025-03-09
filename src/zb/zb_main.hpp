#ifndef ZB_MAIN_HPP_
#define ZB_MAIN_HPP_

#include "zb_types.hpp"

namespace zb
{
    inline constexpr zb_zcl_attr_t g_LastAttribute{
        .id = ZB_ZCL_NULL_ID,
        .type = ZB_ZCL_ATTR_TYPE_NULL,
        .access = (zb_uint8_t)Access::None,
        .manuf_code = ZB_ZCL_NON_MANUFACTURER_SPECIFIC,
        .data_p = nullptr
    };

    template<class T>
    struct ADesc
    {
        using simplified_tag = void;
        zb_uint16_t id;
        Access a;
        T *pData;
        Type type = TypeToTypeId<T>();
    };

    template<class T>
    constexpr zb_zcl_attr_t AttrDesc(ADesc<T> d)
    {
        return {
            .id = d.id, 
            .type = (zb_uint8_t)d.type, 
            .access = (zb_uint8_t)d.a, 
            .manuf_code = ZB_ZCL_NON_MANUFACTURER_SPECIFIC, 
            .data_p = d.pData
        };
    }

    template<zb_uint16_t id> constexpr zb_zcl_cluster_init_t get_cluster_init(Role r) { static_assert(id >= 0xfc00, "get_cluster_init not specialized for this cluster!"); return nullptr; }
#define DEFINE_GET_CLUSTER_INIT_FOR(cid) template<> constexpr zb_zcl_cluster_init_t get_cluster_init<cid>(Role r) { return r == Role::Server ? cid##_SERVER_ROLE_INIT : (r == Role::Client ? cid##_CLIENT_ROLE_INIT : NULL); }

    template<class Tag, size_t N>
    struct TAttributeList
    {
        TAttributeList(TAttributeList const&) = delete;
        TAttributeList(TAttributeList &&) = delete;
        void operator=(TAttributeList const&) = delete;
        void operator=(TAttributeList &&) = delete;


        template<class... T>
        constexpr TAttributeList(ADesc<T>... d):
            attributes{
                AttrDesc(zb::ADesc{ .id = ZB_ZCL_ATTR_GLOBAL_CLUSTER_REVISION_ID, .a = Access::Read, .pData = &rev }),
                (AttrDesc(d), ...)
                , g_LastAttribute
            },
            rev(Tag::rev())
        {
        }

        operator zb_zcl_attr_t*() { return attributes; }

        constexpr static bool is_role(Role r) { return Tag::info().role == r; }
        constexpr static size_t attributes_with_access(Access r) { return Tag::count_members_with_access(r); }
        constexpr static auto info() { return Tag::info(); }

        constexpr zb_zcl_cluster_desc_t desc()
        {
            constexpr auto ci = Tag::info();
            return zb_zcl_cluster_desc_t{
                .cluster_id = ci.id,
                    .attr_count = N + 2,
                    .attr_desc_list = attributes,
                    .role_mask = (zb_uint8_t)ci.role,
                    .manuf_code = ci.manuf_code,
                    .cluster_init = get_cluster_init<Tag::info().id>(ci.role)
            };
        }

        zb_zcl_attr_t attributes[N + 2];
        zb_uint16_t rev;
    };

    template<class ClusterTag, class... T>
    constexpr auto MakeAttributeList(ClusterTag t, ADesc<T>... d)->TAttributeList<ClusterTag, sizeof...(T)>
    {
        return TAttributeList<ClusterTag, sizeof...(T)>(d...);
    }

    template<class T, class MemType>
    using mem_attr_t = MemType T::*;

    template<class T, class MemType>
    struct cluster_mem_desc_t
    {
        mem_attr_t<T, MemType> m;
        zb_uint16_t id;
        Access a = Access::Read;
        Type type = TypeToTypeId<MemType>();

        constexpr inline bool has_access(Access _a) const { return a & _a; } 
    };

    struct cluster_info_t
    {
        zb_uint16_t id;
        zb_uint16_t rev = 0;
        Role        role = Role::Server;
        zb_uint16_t manuf_code = ZB_ZCL_MANUF_CODE_INVALID;

        constexpr bool operator==(cluster_info_t const&) const = default;
    };

    template<cluster_info_t ci, auto... ClusterMemDescriptions>
    struct cluster_struct_desc_t
    {
        static constexpr inline zb_uint16_t rev() { return ci.rev; }
        static constexpr inline auto info() { return ci; }
        static constexpr inline size_t count_members_with_access(Access a) { return ((size_t)ClusterMemDescriptions.has_access(a) + ... + 0); }

        template<cluster_info_t ci2, auto... ClusterMemDescriptions2>
        friend constexpr auto operator+(cluster_struct_desc_t<ci, ClusterMemDescriptions...> lhs, cluster_struct_desc_t<ci2, ClusterMemDescriptions2...> rhs)
        {
            static_assert(ci == ci2, "Must be the same revision");
            return cluster_struct_desc_t<
                ci,
                ClusterMemDescriptions...,
                ClusterMemDescriptions2...
                >{};
        }
    };

    template<class T, class DestT, class MemType> requires std::is_base_of_v<DestT, T>
    constexpr ADesc<MemType> cluster_mem_to_attr_desc(T& s, cluster_mem_desc_t<DestT,MemType> d) { return {.id = d.id, .a = d.a, .pData = &(s.*d.m), .type = d.type}; }

    template<class T,cluster_info_t ci, auto... ClusterMemDescriptions>
    constexpr auto cluster_struct_to_attr_list(T &s, cluster_struct_desc_t<ci, ClusterMemDescriptions...>)
    {
        return MakeAttributeList(cluster_struct_desc_t<ci, ClusterMemDescriptions...>{}, cluster_mem_to_attr_desc(s, ClusterMemDescriptions)...);
    }

    template<class zb_struct>
    constexpr auto get_cluster_description();

    template<class ZbS>
    constexpr auto to_attributes(ZbS &s) { return cluster_struct_to_attr_list(s, get_cluster_description<ZbS>()); }


    template<class... T>
    struct TClusterList
    {
        static constexpr size_t N = sizeof...(T);

        TClusterList(TClusterList const&) = delete;
        TClusterList(TClusterList &&) = delete;
        void operator=(TClusterList const&) = delete;
        void operator=(TClusterList &&) = delete;

        static constexpr size_t reporting_attributes_count() { return (T::attributes_with_access(Access::Report) + ... + 0); }
        static constexpr size_t cvc_level_ctrl_attributes_count() { return ((T::info().id == ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL ? T::attributes_with_access(Access::Report) : 0) + ... + 0); }
        static constexpr size_t server_cluster_count() { return (T::is_role(Role::Server) + ... + 0); }
        static constexpr size_t client_cluster_count() { return (T::is_role(Role::Client) + ... + 0); }

        constexpr TClusterList(T&... d):
            clusters{
                (d.desc(), ...)
            },
            reporting_attributes(reporting_attributes_count()),
            cvc_level_ctrl_attributes(cvc_level_ctrl_attributes_count()),
            server_count(server_cluster_count()),
            client_count(client_cluster_count())
        {
        }

        zb_zcl_cluster_desc_t clusters[N];
        size_t reporting_attributes;
        size_t cvc_level_ctrl_attributes;
        zb_uint8_t server_count;
        zb_uint8_t client_count;
    };

    template<class... ClusterAttributesDesc>
    constexpr auto to_clusters(ClusterAttributesDesc&... c) -> TClusterList<ClusterAttributesDesc...>
    {
        return {c...};
    }

    struct zb_af_simple_desc_base
    {                                                                                       
        zb_uint8_t    endpoint;                 /* Endpoint */                                
        zb_uint16_t   app_profile_id;           /* Application profile identifier */          
        zb_uint16_t   app_device_id;            /* Application device identifier */           
        zb_bitfield_t app_device_version:4;     /* Application device version */              
        zb_bitfield_t reserved:4;               /* Reserved */                                
        zb_uint8_t    app_input_cluster_count;  /* Application input cluster count */         
        zb_uint8_t    app_output_cluster_count; /* Application output cluster count */        
        /* Application input and output cluster list */                                       
        //zb_uint16_t   app_cluster_list[(in_clusters_count) + (out_clusters_count)];           
    } ZB_PACKED_STRUCT;                                                                      

    template<size_t ServerCount, size_t ClientCount>
    struct TSimpleDesc: zb_af_simple_desc_1_1_t
    {
        zb_uint16_t app_cluster_list_ext[(ServerCount + ClientCount) - 2];
    } ZB_PACKED_STRUCT;

    template<class Clusters>
    struct EPDesc
    {
        using SimpleDesc = TSimpleDesc<Clusters::server_cluster_count(), Clusters::client_cluster_count()>;

        template<class T1, class T2, class... T> requires std::is_same_v<TClusterList<T1, T2, T...>, Clusters>
        constexpr EPDesc(zb_uint8_t ep, zb_uint16_t dev_id, zb_uint8_t dev_ver, TClusterList<T1, T2, T...> &clusters):
            simple_desc{ 
                {
                    .endpoint = ep, 
                    .app_profile_id = ZB_AF_HA_PROFILE_ID, 
                    .app_device_id = dev_id,
                    .app_device_version = dev_ver,
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
                .ep_id = ep,
                .profile_id = ZB_AF_HA_PROFILE_ID,
                .device_handler = nullptr,
                .identify_handler = nullptr,
                .reserved_size = 0,
                .reserved_ptr = nullptr,
                .cluster_count = sizeof...(T),
                .cluster_desc_list = clusters.clusters,
                .simple_desc = &simple_desc,
                .rep_info_count = Clusters::reporting_attributes_count(),
                .reporting_info = rep_ctx,
                .cvc_alarm_count = Clusters::cvc_level_ctrl_attributes_count(),
                .cvc_alarm_info = lev_ctrl_ctx
            }
        {
        }

        SimpleDesc simple_desc;
        zb_zcl_reporting_info_t rep_ctx[Clusters::reporting_attributes_count()];
        zb_zcl_cvc_alarm_variables_t lev_ctrl_ctx[Clusters::cvc_level_ctrl_attributes_count()];
        zb_af_endpoint_desc_t ep;
    };

    template<class... T>
    constexpr auto configure_ep(zb_uint8_t ep, zb_uint16_t dev_id, zb_uint8_t dev_ver, TClusterList<T...> &clusters) -> EPDesc<TClusterList<T...>>
    {
        return {ep, dev_id, dev_ver, clusters};
    }

    template<size_t N>
    struct Device
    {
        template<class... Clusters>
        constexpr Device(EPDesc<Clusters>&...eps):
            endpoints{&eps.ep...},
            ctx{.ep_count = N, .ep_desc_list = endpoints}
        {
        }

        operator zb_af_device_ctx_t*() { return &ctx; }

        zb_af_endpoint_desc_t *endpoints[N];
        zb_af_device_ctx_t ctx;
    };

    template<class... Clusters>
    Device(EPDesc<Clusters>&...eps) -> Device<sizeof...(Clusters)>;

    template<class... Clusters>
    constexpr auto configure_device(EPDesc<Clusters>&...eps)
    {
        return Device{eps...};
    }
}

#endif
