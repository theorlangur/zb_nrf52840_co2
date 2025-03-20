#ifndef ZB_HANDLERS_HPP_
#define ZB_HANDLERS_HPP_

#include "zb_buf.hpp"
#include "zb_desc_helper_types_ep.hpp"
#include "lib_notification_node.hpp"

namespace zb
{
    /**********************************************************************/
    /* Set Attribute value callback                                       */
    /**********************************************************************/
    using dev_callback_handler_t = void(*)(zb_zcl_device_callback_param_t *);
    using set_attr_value_handler_t = void (*)(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam);
    struct set_attr_val_gen_desc_t: EPClusterAttributeDesc_t
    {
        set_attr_value_handler_t handler;
    };

    struct SetAttrValHandlingNode: GenericNotificationNode<SetAttrValHandlingNode>
    {
        set_attr_val_gen_desc_t h;
    };

    struct dev_cb_handlers_desc
    {
        dev_callback_handler_t default_handler = nullptr;
    };

    struct GenericDeviceCBHandlingNode: GenericNotificationNode<GenericDeviceCBHandlingNode>
    {
        dev_callback_handler_t h;

        void DoNotify(zb_zcl_device_callback_param_t *pDevParam) { h(pDevParam); }
    };

    template<dev_cb_handlers_desc generic={}, set_attr_val_gen_desc_t... handlers>
    void tpl_device_cb(zb_bufid_t bufid)
    {
        zb::BufViewPtr bv{bufid};
        auto *pDevParam = bv.param<zb_zcl_device_callback_param_t>();
        pDevParam->status = RET_OK;
        switch(pDevParam->device_cb_id)
        {
            case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
                {
                    auto *pSetVal = &pDevParam->cb_param.set_attr_value_param;
                    static_assert(((handlers.handler != nullptr) && ...), "Invalid handler detected");
                    [[maybe_unused]]zb_ret_t r = 
                        ((handlers.fits(pDevParam->endpoint, pSetVal->cluster_id, pSetVal->attr_id) ? handlers.handler(pSetVal, pDevParam), RET_OK : RET_OK), ...);

                    for(auto *pN : SetAttrValHandlingNode::g_List)
                    {
                        if (pN->h.fits(pDevParam->endpoint, pSetVal->cluster_id, pSetVal->attr_id))
                            pN->h.handler(pSetVal, pDevParam);
                    }
                }
                break;
            default:
                break;
        }

        if constexpr (generic.default_handler)
            generic.default_handler(pDevParam);

        for(auto *pN : GenericDeviceCBHandlingNode::g_List)
            pN->h(pDevParam);
    }

    /**********************************************************************/
    /* NoReport callback                                                  */
    /**********************************************************************/
    struct no_report_attr_handler_desc_t: EPClusterAttributeDesc_t
    {
        zb_zcl_no_reporting_cb_t handler;
    };

    struct NoReportHandlingNode: GenericNotificationNode<NoReportHandlingNode>
    {
        no_report_attr_handler_desc_t h;
    };

    template<no_report_attr_handler_desc_t... handlers>
    void tpl_no_report_cb(zb_uint8_t ep, zb_uint16_t cluster_id, zb_uint16_t attr_id)
    {
        static_assert(((handlers.handler != nullptr) && ...), "Invalid handler detected");
        [[maybe_unused]]bool r = 
            ((handlers.fits(ep, cluster_id, attr_id) ? handlers.handler(ep, cluster_id, attr_id), true : true), ...);

        for(auto *pN : NoReportHandlingNode::g_List)
        {
            if (pN->h.fits(ep, cluster_id, attr_id))
                pN->h.handler(ep, cluster_id, attr_id);
        }
    }

    /**********************************************************************/
    /* Attribute report callback                                          */
    /**********************************************************************/
    struct report_attr_handler_desc_t: EPClusterAttributeDesc_t
    {
        zb_zcl_report_attr_cb_t handler;
    };

    struct ReportHandlingNode: GenericNotificationNode<ReportHandlingNode>
    {
        report_attr_handler_desc_t h;
    };

    template<report_attr_handler_desc_t... handlers>
    void tpl_report_cb(zb_zcl_addr_t *addr, zb_uint8_t ep, zb_uint16_t cluster_id, zb_uint16_t attr_id, zb_uint8_t attr_type, zb_uint8_t *value)
    {
        static_assert(((handlers.handler != nullptr) && ...), "Invalid handler detected");
        [[maybe_unused]]bool r = 
            ((handlers.fits(ep, cluster_id, attr_id) ? handlers.handler(addr, ep, cluster_id, attr_id, attr_type, value), true : true), ...);

        for(auto *pN : ReportHandlingNode::g_List)
        {
            if (pN->h.fits(ep, cluster_id, attr_id))
                pN->h.handler(addr, ep, cluster_id, attr_id, attr_type, value);
        }
    }
}
#endif
