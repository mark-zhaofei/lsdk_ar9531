/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_igmp.h"
#include "fal_uk_if.h"

sw_error_t
fal_port_igmps_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_IGMPS_MODE_SET, dev_id, (a_uint32_t) port_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_igmps_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_IGMPS_MODE_GET, dev_id, (a_uint32_t) port_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_MLD_CMD_SET, dev_id, (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_igmp_mld_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_MLD_CMD_GET, dev_id, (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_port_igmp_mld_join_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_PT_JOIN_SET, dev_id, (a_uint32_t) port_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_port_igmp_mld_join_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_PT_JOIN_GET, dev_id, (a_uint32_t) port_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_port_igmp_mld_leave_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_PT_LEAVE_SET, dev_id, (a_uint32_t) port_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_igmp_mld_leave_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_PT_LEAVE_GET, dev_id, (a_uint32_t) port_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_rp_set(a_uint32_t dev_id, fal_pbmp_t pts)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_RP_SET, dev_id, (a_uint32_t) pts);
    return rv;
}

sw_error_t
fal_igmp_mld_rp_get(a_uint32_t dev_id, fal_pbmp_t * pts)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_RP_GET, dev_id, (a_uint32_t) pts);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_creat_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_CREAT_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_creat_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_CREAT_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_static_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_STATIC_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_static_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_STATIC_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_leaky_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_LEAKY_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_leaky_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_LEAKY_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_v3_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_V3_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_v3_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_V3_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_queue_set(a_uint32_t dev_id, a_bool_t enable, a_uint32_t queue)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_QUEUE_SET, dev_id, (a_uint32_t)enable, (a_uint32_t)queue);
    return rv;
}

sw_error_t
fal_igmp_mld_entry_queue_get(a_uint32_t dev_id, a_bool_t * enable, a_uint32_t * queue)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IGMP_ENTRY_QUEUE_GET, dev_id, (a_uint32_t)enable, (a_uint32_t)queue);
    return rv;
}

sw_error_t
fal_port_igmp_mld_learn_limit_set(a_uint32_t dev_id, fal_port_t port_id,
                                  a_bool_t enable, a_uint32_t cnt)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_IGMP_LEARN_LIMIT_SET, dev_id, port_id, enable, cnt);
    return rv;
}

sw_error_t
fal_port_igmp_mld_learn_limit_get(a_uint32_t dev_id, fal_port_t port_id,
                                  a_bool_t * enable, a_uint32_t * cnt)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_IGMP_LEARN_LIMIT_GET, dev_id, port_id, (a_uint32_t)enable, (a_uint32_t)cnt);
    return rv;
}

sw_error_t
fal_port_igmp_mld_learn_exceed_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
                                       fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_IGMP_LEARN_EXCEED_CMD_SET, dev_id, port_id, (a_uint32_t)cmd);
    return rv;
}

sw_error_t
fal_port_igmp_mld_learn_exceed_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
                                       fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_IGMP_LEARN_EXCEED_CMD_GET, dev_id, port_id, (a_uint32_t)cmd);
    return rv;
}


