#ifndef ZB_SIGNALS_HPP_
#define ZB_SIGNALS_HPP_

#include "zb_buf.hpp"

extern "C"{
#include <zigbee/zigbee_app_utils.h>
}
namespace zb
{
    struct sig_handlers_t
    {
        using generic_handler_t = void(*)(zb_ret_t status);
        using dev_annce_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_device_annce_params_t *p);
        using leave_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_leave_params_t *p);
        using dev_associated_handler_t = void(*)(zb_ret_t status, zb_nwk_signal_device_associated_params_t *p);
        using leave_indication_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_leave_indication_params_t *p);
        //using zgp_commisioning_handler_t = void(*)(zb_ret_t status, zb_zgp_signal_commissioning_params_t *p);
        using can_sleep_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_can_sleep_params_t *p);
        using dev_authorized_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_device_authorized_params_t *p);
        using dev_update_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_device_update_params_t *p);
        //using pan_id_conflict_handler_t = void(*)(zb_ret_t status, zb_start_pan_id_conflict_resolution_t *p);
        using nlme_status_indication_handler_t = void(*)(zb_ret_t status, zb_nwk_command_status_t *p);
        using nwk_permit_join_status_handler_t = void(*)(zb_ret_t status, zb_uint8_t *p);

        generic_handler_t on_skip_startup = nullptr;
        dev_annce_handler_t on_dev_annce = nullptr;
        leave_handler_t on_leave = nullptr;
        generic_handler_t on_error = nullptr;
        generic_handler_t on_dev_first_start = nullptr;
        generic_handler_t on_dev_reboot = nullptr;
        generic_handler_t on_steering = nullptr;
        generic_handler_t on_formation = nullptr;
        generic_handler_t on_finding_and_binding_target_finished = nullptr;
        generic_handler_t on_finding_and_binding_initiator_finished = nullptr;
        dev_associated_handler_t on_dev_associated = nullptr;
        leave_indication_handler_t on_leave_indication = nullptr;
        generic_handler_t on_wwah_rejoined_started = nullptr;
        can_sleep_handler_t on_can_sleep = nullptr;
        generic_handler_t on_production_config_ready = nullptr;
        generic_handler_t on_nwk_no_active_links_left = nullptr;
        generic_handler_t on_sub_ghz_suspend = nullptr;
        generic_handler_t on_sub_ghz_resume = nullptr;
        dev_authorized_handler_t on_dev_authorized = nullptr;
        dev_update_handler_t on_dev_update = nullptr;
        nlme_status_indication_handler_t on_nlme_status_indication = nullptr;
        generic_handler_t on_tcswap_db_backup_required = nullptr;
        generic_handler_t on_tcswapped = nullptr;
        generic_handler_t on_tc_rejoin_done = nullptr;
        nwk_permit_join_status_handler_t on_nwk_permit_join = nullptr;
        generic_handler_t on_steering_cancelled = nullptr;
        generic_handler_t on_formation_cancelled = nullptr;
        generic_handler_t on_ready_to_shut = nullptr;
    };

    template<sig_handlers_t h = {}>
    zb_ret_t tpl_signal_handler(zb_bufid_t bufid)
    {
        BufPtr b{bufid};
        zb_zdo_app_signal_hdr_t *pHdr;
        auto signalId = zb_get_app_signal(bufid, &pHdr);
        zb_ret_t status = zb_buf_get_status(bufid);
        switch(signalId)
        {
            //case ZB_ZDO_SIGNAL_DEFAULT_START://deprecated
            //break;
            case ZB_ZDO_SIGNAL_SKIP_STARTUP:
                if constexpr (h.on_skip_startup) h.on_skip_startup(status);
                break;
            case ZB_ZDO_SIGNAL_DEVICE_ANNCE://zb_zdo_signal_device_annce_params_t
                if constexpr (h.on_dev_annce) h.on_dev_annce(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_device_annce_params_t));
                break;
            case ZB_ZDO_SIGNAL_LEAVE://zb_zdo_signal_leave_params_t
                if constexpr (h.on_leave) h.on_leave(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_leave_params_t));
                break;
            case ZB_ZDO_SIGNAL_ERROR:
                if constexpr (h.on_error) h.on_error(status);
                break;
            case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
                if constexpr (h.on_dev_first_start) h.on_dev_first_start(status);
                break;
            case ZB_BDB_SIGNAL_DEVICE_REBOOT:
                if constexpr (h.on_dev_reboot) h.on_dev_reboot(status);
                break;
            case ZB_BDB_SIGNAL_STEERING:
                if constexpr (h.on_steering) h.on_steering(status);
                break;
            case ZB_BDB_SIGNAL_FORMATION:
                if constexpr (h.on_formation) h.on_formation(status);
                break;
            case ZB_BDB_SIGNAL_FINDING_AND_BINDING_TARGET_FINISHED:
                if constexpr (h.on_finding_and_binding_target_finished) h.on_finding_and_binding_target_finished(status);
                break;
            case ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED:
                if constexpr (h.on_finding_and_binding_initiator_finished) h.on_finding_and_binding_initiator_finished(status);
                break;
            case ZB_NWK_SIGNAL_DEVICE_ASSOCIATED://zb_nwk_signal_device_associated_params_t, deprecated
                if constexpr (h.on_dev_associated) h.on_dev_associated(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_nwk_signal_device_associated_params_t));
                break;
            case ZB_ZDO_SIGNAL_LEAVE_INDICATION://zb_zdo_signal_leave_indication_params_t
                if constexpr (h.on_leave_indication) h.on_leave_indication(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_leave_indication_params_t));
                break;
            case ZB_BDB_SIGNAL_WWAH_REJOIN_STARTED:
                if constexpr (h.on_wwah_rejoined_started) h.on_wwah_rejoined_started(status);
                break;
                //case ZB_ZGP_SIGNAL_COMMISSIONING://zb_zgp_signal_commissioning_params_s
                //break;
            case ZB_COMMON_SIGNAL_CAN_SLEEP://zb_zdo_signal_can_sleep_params_t
                if constexpr (h.on_can_sleep) h.on_can_sleep(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_can_sleep_params_t));
                break;
            case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
                if constexpr (h.on_production_config_ready) h.on_production_config_ready(status);
                break;
            case ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT:
                if constexpr (h.on_nwk_no_active_links_left) h.on_nwk_no_active_links_left(status);
                break;
            case ZB_SIGNAL_SUBGHZ_SUSPEND:
                if constexpr (h.on_sub_ghz_suspend) h.on_sub_ghz_suspend(status);
                break;
            case ZB_SIGNAL_SUBGHZ_RESUME:
                if constexpr (h.on_sub_ghz_resume) h.on_sub_ghz_resume(status);
                break;
            case ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED://zb_zdo_signal_device_authorized_params_t
                if constexpr (h.on_dev_authorized) h.on_dev_authorized(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_device_authorized_params_t));
                break;
            case ZB_ZDO_SIGNAL_DEVICE_UPDATE://zb_zdo_signal_device_update_params_t
                if constexpr (h.on_dev_update) h.on_dev_update(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_device_update_params_t));
                break;
                //case ZB_NWK_SIGNAL_PANID_CONFLICT_DETECTED://zb_start_pan_id_conflict_resolution
                //break;
            case ZB_NLME_STATUS_INDICATION://zb_nwk_command_status_t
                if constexpr (h.on_nlme_status_indication) h.on_nlme_status_indication(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_nwk_command_status_t));
                break;
            case ZB_TCSWAP_DB_BACKUP_REQUIRED_SIGNAL:
                if constexpr (h.on_tcswap_db_backup_required) h.on_tcswap_db_backup_required(status);
                break;
            case ZB_TC_SWAPPED_SIGNAL:
                if constexpr (h.on_tcswapped) h.on_tcswapped(status);
                break;
            case ZB_BDB_SIGNAL_TC_REJOIN_DONE:
                if constexpr (h.on_tc_rejoin_done) h.on_tc_rejoin_done(status);
                break;
            case ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS://zb_uint8_t
                if constexpr (h.on_nwk_permit_join) h.on_nwk_permit_join(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_uint8_t));
                break;
            case ZB_BDB_SIGNAL_STEERING_CANCELLED:
                if constexpr (h.on_steering_cancelled) h.on_steering_cancelled(status);
                break;
            case ZB_BDB_SIGNAL_FORMATION_CANCELLED:
                if constexpr (h.on_formation_cancelled) h.on_formation_cancelled(status);
                break;
            case ZB_SIGNAL_READY_TO_SHUT:
                if constexpr (h.on_ready_to_shut) h.on_ready_to_shut(status);
                break;
        }

        return zigbee_default_signal_handler(bufid);
    }
}
#endif
