/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @defgroup garuda_mib GARUDA_MIB
 * @{
 */
#ifndef _GARUDA_MIB_H_
#define _GARUDA_MIB_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "fal/fal_mib.h"

    sw_error_t
    garuda_mib_init(a_uint32_t dev_id);

#ifdef IN_MIB
#define GARUDA_MIB_INIT(rv, dev_id) \
    { \
        rv = garuda_mib_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
#define GARUDA_MIB_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG


    HSL_LOCAL sw_error_t
    garuda_get_mib_info(a_uint32_t dev_id, fal_port_t port_id,
                        fal_mib_info_t * mib_info );



    HSL_LOCAL sw_error_t
    garuda_mib_status_set(a_uint32_t dev_id, a_bool_t enable);



    HSL_LOCAL sw_error_t
    garuda_mib_status_get(a_uint32_t dev_id, a_bool_t * enable);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GARUDA_MIB_H_ */
/**
 * @}
 */
