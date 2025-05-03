#ifndef ZB_DESC_HELPER_TYPES_CLUSTER_HPP_
#define ZB_DESC_HELPER_TYPES_CLUSTER_HPP_

#include "lib_object_pool.hpp"
#include "zb_desc_helper_types_attr.hpp"

namespace zb
{
    enum class FrameDirection: uint8_t
    {
        ToServer = ZB_ZCL_FRAME_DIRECTION_TO_SRV,
        ToClient = ZB_ZCL_FRAME_DIRECTION_TO_CLI
    };

    enum class AddrMode: uint8_t
    {
        NoAddr_NoEP = ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
        Group_NoEP = ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT,
        Dst16EP = ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        Dst64EP = ZB_APS_ADDR_MODE_64_ENDP_PRESENT,
        EPAsBindTableId = ZB_APS_ADDR_MODE_BIND_TBL_ID
    };

    union frame_ctl_t
    {
        struct{
            uint8_t cluster_specific     : 1;
            uint8_t manufacture_specific : 1;
            FrameDirection direction     : 1;
            uint8_t disable_default_response : 1;
        } f;
        uint8_t u8;
    };


    template<class T, class MemType>
    using mem_attr_t = MemType T::*;

    template<class T, class MemType>
    struct cluster_mem_desc_t
    {
        using MemT = MemType;

        mem_attr_t<T, MemType> m;
        zb_uint16_t id;
        Access a = Access::Read;
        Type type = TypeToTypeId<MemType>();

        constexpr inline bool has_access(Access _a) const { return a & _a; } 
        constexpr inline bool is_cvc() const { 
            if (a & Access::Report)
            {
                switch(type)
                {
                    case Type::S8:
                    case Type::U8:
                    case Type::S16:
                    case Type::U16:
                    case Type::S24:
                    case Type::U24:
                    case Type::S32:
                    case Type::U32:
                    case Type::Float:
                    case Type::HalfFloat:
                    case Type::Double:
                        return true;
                    default:
                        break;
                }
            }
            return false;
        } 
    };

    //helper for attributes
    template<auto memPtr, auto...ClusterMemDescriptions>
    struct find_cluster_mem_desc_t;

    template<auto memPtr, auto MemDesc, auto...ClusterMemDescriptions> requires (MemDesc.m == memPtr)
    struct find_cluster_mem_desc_t<memPtr, MemDesc, ClusterMemDescriptions...>
    {
        static constexpr auto mem_desc() { return MemDesc; }
    };

    template<auto memPtr, auto MemDesc, auto...ClusterMemDescriptions>
    struct find_cluster_mem_desc_t<memPtr, MemDesc, ClusterMemDescriptions...>: find_cluster_mem_desc_t<memPtr, ClusterMemDescriptions...>
    {
    };

    template<auto memPtr>
    struct find_cluster_mem_desc_t<memPtr>
    {
        static constexpr auto cmd_desc() { static_assert(sizeof(memPtr) == 0, "Pointer to a member is not an attribte"); }
    };

    //helper for commands
    template<zb_uint8_t cmd_id, zb_uint16_t manuf_code = ZB_ZCL_MANUF_CODE_INVALID>
    struct cluster_cmd_desc_t;

    template<auto MemPtr>
    constexpr uint8_t CmdIdFromCmdMemPtr = mem_ptr_traits<decltype(MemPtr)>::MemberType::kCmdId;

    template<auto MemPtr, auto...CmdMemPtrs>
    struct find_cluster_cmd_desc_t;

    template<auto MemPtr, auto CmdMemPtr, auto...CmdMemPtrs> requires (MemPtr == CmdMemPtr)
    struct find_cluster_cmd_desc_t<MemPtr, CmdMemPtr, CmdMemPtrs...>
    {
        static constexpr auto cmd_desc() { return typename mem_ptr_traits<decltype(MemPtr)>::MemberType{}; }
    };

    template<auto MemPtr, auto CmdMemPtr, auto...CmdMemPtrs>
    struct find_cluster_cmd_desc_t<MemPtr, CmdMemPtr, CmdMemPtrs...>: find_cluster_cmd_desc_t<MemPtr, CmdMemPtrs...>
    {
    };

    template<auto MemPtr>
    struct find_cluster_cmd_desc_t<MemPtr>
    {
        static constexpr auto cmd_desc() { static_assert(sizeof(MemPtr) == 0, "Pointer to a member is not a command"); }
    };

    struct cluster_info_t
    {
        zb_uint16_t id;
        zb_uint16_t rev = 0;
        Role        role = Role::Server;
        zb_uint16_t manuf_code = ZB_ZCL_MANUF_CODE_INVALID;

        constexpr bool operator==(cluster_info_t const&) const = default;
    };

    struct request_runtime_args_t
    {
        zb_addr_u dst_addr;
        uint8_t dst_ep;

        using PoolType = ObjectPool<request_runtime_args_t, 4>;
        static PoolType g_Pool;
    };
    using RequestPtr = request_runtime_args_t::PoolType::Ptr<request_runtime_args_t::g_Pool>;
    inline constinit request_runtime_args_t::PoolType request_runtime_args_t::g_Pool{};

    struct request_args_t
    {
        uint8_t ep;
        zb_callback_t cb = nullptr;
        uint16_t profile_id = ZB_AF_HA_PROFILE_ID;
    };

    struct request_args_intern_t: request_args_t
    {
        AddrMode addr_mode;
    };

    //as NTTP to cluster description template type
    template<zb_uint8_t cmd_id, zb_uint16_t manuf_code>
    struct cluster_cmd_desc_t
    {
        static constexpr uint8_t kCmdId = cmd_id;
            
        template<cluster_info_t i, request_args_t r>
        static void request()
        {
            request_runtime_args_t *pArgs = request_runtime_args_t::g_Pool.Acquire();
            zigbee_get_out_buf_delayed_ext(
                    &on_out_buf_ready<i, {{.ep = r.ep, .cb = r.cb, .profile_id = r.profile_id}, AddrMode::NoAddr_NoEP}>
                    , (uint16_t)request_runtime_args_t::g_Pool.PtrToIdx(pArgs), 0);
        }

        template<cluster_info_t i, request_args_t r>
        static void request(uint16_t short_addr, uint8_t ep)
        {
            request_runtime_args_t *pArgs = request_runtime_args_t::g_Pool.Acquire();
            pArgs->dst_ep = ep;
            pArgs->dst_addr.addr_short = short_addr;
            zigbee_get_out_buf_delayed_ext(
                    &on_out_buf_ready<i, {{.ep = r.ep, .cb = r.cb, .profile_id = r.profile_id}, AddrMode::Dst16EP}>
                    , (uint16_t)request_runtime_args_t::g_Pool.PtrToIdx(pArgs), 0);
        }

        template<cluster_info_t i, request_args_t r>
        static void request(zb_ieee_addr_t ieee_addr, uint8_t ep)
        {
            request_runtime_args_t *pArgs = request_runtime_args_t::g_Pool.Acquire();
            pArgs->dst_ep = ep;
            std::memcpy(pArgs->dst_addr.addr_long, ieee_addr, sizeof(zb_ieee_addr_t));
            zigbee_get_out_buf_delayed_ext(
                    &on_out_buf_ready<i, {{.ep = r.ep, .cb = r.cb, .profile_id = r.profile_id}, AddrMode::Dst64EP}>
                    , (uint16_t)request_runtime_args_t::g_Pool.PtrToIdx(pArgs), 0);
        }

        template<cluster_info_t i, request_args_t r>
        static void request(uint16_t group_addr)
        {
            request_runtime_args_t *pArgs = request_runtime_args_t::g_Pool.Acquire();
            pArgs->dst_addr.addr_short = group_addr;
            zigbee_get_out_buf_delayed_ext(
                    &on_out_buf_ready<i, {{.ep = r.ep, .cb = r.cb, .profile_id = r.profile_id}, AddrMode::Group_NoEP}>
                    , (uint16_t)request_runtime_args_t::g_Pool.PtrToIdx(pArgs), 0);
        }

        template<cluster_info_t i, request_args_t r>
        static void request(uint8_t bind_table_id)
        {
            request_runtime_args_t *pArgs = request_runtime_args_t::g_Pool.Acquire();
            pArgs->dst_ep = bind_table_id;
            zigbee_get_out_buf_delayed_ext(
                    &on_out_buf_ready<i, {{.ep = r.ep, .cb = r.cb, .profile_id = r.profile_id}, AddrMode::EPAsBindTableId}>
                    , (uint16_t)request_runtime_args_t::g_Pool.PtrToIdx(pArgs), 0);
        }

        template<cluster_info_t i, request_args_intern_t r>
        static void on_out_buf_ready(zb_bufid_t bufid, uint16_t poolIdx)
        {
            request_runtime_args_t *pArgs = request_runtime_args_t::g_Pool.IdxToPtr(poolIdx);
            RequestPtr raii(pArgs);

            constexpr uint16_t manu_code = i.manuf_code != ZB_ZCL_MANUF_CODE_INVALID ? i.manuf_code : manuf_code;

            frame_ctl_t f{.f{
                .cluster_specific = true, 
                .manufacture_specific = manu_code != ZB_ZCL_MANUF_CODE_INVALID
                , .direction = FrameDirection::ToServer
                , .disable_default_response = false
            }};
            ZB_ZCL_GET_SEQ_NUM();
            uint8_t* ptr = (uint8_t*)zb_zcl_start_command_header(bufid, f.u8, manu_code, cmd_id, nullptr);
            zb_ret_t ret = zb_zcl_finish_and_send_packet(bufid, ptr, &pArgs->dst_addr, (uint8_t)r.addr_mode, pArgs->dst_ep, r.ep, r.profile_id, i.id, r.cb);
            if (RET_OK != ret && r.cb)
                r.cb(uint8_t(ret));
        }
    };

    template<auto... attributeMemberDesc>
    struct cluster_attributes_desc_t
    {
        static constexpr inline size_t count_members_with_access(Access a) { return ((size_t)attributeMemberDesc.has_access(a) + ... + 0); }
        static constexpr inline size_t count_cvc_members() { return ((size_t)attributeMemberDesc.is_cvc() + ... + 0); }
        template<auto memPtr>
        static constexpr inline auto get_member_description() { return find_cluster_mem_desc_t<memPtr, attributeMemberDesc...>::mem_desc(); }

        template<auto... attributeMemberDesc2>
        friend constexpr auto operator+(cluster_attributes_desc_t<attributeMemberDesc...> lhs, cluster_attributes_desc_t<attributeMemberDesc2...> rhs)
        {
            return cluster_attributes_desc_t< attributeMemberDesc..., attributeMemberDesc2... >{};
        }
    };

    template<auto... cmdMemberDesc>
    struct cluster_commands_desc_t
    {
        template<auto memPtr>
        static constexpr inline auto get_cmd_description() { return find_cluster_cmd_desc_t<memPtr, cmdMemberDesc...>::cmd_desc(); }

        template<auto... cmdMemberDesc2>
        friend constexpr auto operator+(cluster_commands_desc_t<cmdMemberDesc...> lhs, cluster_commands_desc_t<cmdMemberDesc2...> rhs)
        {
            return cluster_commands_desc_t< cmdMemberDesc..., cmdMemberDesc2... >{};
        }
    };

    template<cluster_info_t ci, auto attributes = cluster_attributes_desc_t<>{}, auto cmds = cluster_commands_desc_t<>{}>//auto = cluster_attributes_desc_t
    struct cluster_struct_desc_t
    {
        static constexpr inline zb_uint16_t rev() { return ci.rev; }
        static constexpr inline auto info() { return ci; }
        static constexpr inline size_t count_members_with_access(Access a) { return attributes.count_members_with_access(a); }
        static constexpr inline size_t count_cvc_members() { return attributes.count_cvc_members(); }

        template<auto memPtr>
        static constexpr inline auto get_member_description() { return attributes.template get_member_description<memPtr>(); }

        template<auto memPtr>
        static constexpr inline auto get_cmd_description() { return cmds.template get_cmd_description<memPtr>(); }

        template<cluster_info_t ci2, cluster_attributes_desc_t attributes2, cluster_commands_desc_t cmds2>
        friend constexpr auto operator+(cluster_struct_desc_t<ci, attributes, cmds> lhs, cluster_struct_desc_t<ci2, attributes2, cmds2> rhs)
        {
            static_assert(ci == ci2, "Must be the same revision");
            return cluster_struct_desc_t< ci, attributes + attributes2, cmds + cmds2>{};
        }
    };

    template<class T, class DestT, class MemType> requires std::is_base_of_v<DestT, T>
    constexpr ADesc<MemType> cluster_mem_to_attr_desc(T& s, cluster_mem_desc_t<DestT,MemType> d) { return {.id = d.id, .a = d.a, .pData = &(s.*d.m), .type = d.type}; }

    template<class T,cluster_info_t ci, auto... ClusterMemDescriptions, cluster_attributes_desc_t<ClusterMemDescriptions...> attributes, cluster_commands_desc_t cmds>
    constexpr auto cluster_struct_to_attr_list(T &s, cluster_struct_desc_t<ci, attributes, cmds>)
    {
        return MakeAttributeList(&s, cluster_mem_to_attr_desc(s, ClusterMemDescriptions)...);
    }

    template<class... T>
    struct TClusterList
    {
        static constexpr size_t N = sizeof...(T);

        TClusterList(TClusterList const&) = delete;
        TClusterList(TClusterList &&) = delete;
        void operator=(TClusterList const&) = delete;
        void operator=(TClusterList &&) = delete;

        static constexpr size_t reporting_attributes_count() { return (T::attributes_with_access(Access::Report) + ... + 0); }
        static constexpr size_t cvc_attributes_count() { return (T::cvc_attributes() + ... + 0); }

        static constexpr size_t server_cluster_count() { return (T::is_role(Role::Server) + ... + 0); }
        static constexpr size_t client_cluster_count() { return (T::is_role(Role::Client) + ... + 0); }
        static constexpr bool has_info(cluster_info_t ci) { return ((T::info() == ci) || ...); }

        constexpr TClusterList(T&... d):
            clusters{ d.desc()... }
        {
        }

        zb_zcl_cluster_desc_t clusters[N];
    };
}
#endif
