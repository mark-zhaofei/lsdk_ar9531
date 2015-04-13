/*
 *  Copyright (c) 2014 Qualcomm Atheros, Inc.  All rights reserved. 
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */

#if UMAC_SUPPORT_WNM

/*
 *  WNM message handlers for AP.
 */
#include <ieee80211_var.h>
#include <ieee80211_wnm.h>
#include <ieee80211_ioctl.h>
#include "ieee80211_wnm_priv.h"

int
__ieee80211_tfs_filter(struct ieee80211_node *ni, wbuf_t wbuf,
                    int ether_type, struct ip_header *ip);
/* 
 * Format and send Bss transition management request action frame
 */
int ieee80211_send_bstm_req(wlan_if_t vap, wlan_node_t ni,
                                 struct ieee80211_bstm_reqinfo* bstm_reqinfo)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211_rrm_cbinfo cb_info;
    struct ieee80211_action_bstm_req *bstm_req;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }

    bstm_req = (struct ieee80211_action_bstm_req*)(frm);
    bstm_req->header.ia_category = IEEE80211_ACTION_CAT_WNM;
    bstm_req->header.ia_action = IEEE80211_ACTION_BSTM_REQ;
    bstm_req->dialogtoken = bstm_reqinfo->dialogtoken;
    bstm_req->mode_candidate_list = bstm_reqinfo->candidate_list;
    bstm_req->mode_disassoc = bstm_reqinfo->disassoc;
    bstm_req->disassoc_timer = htole16(bstm_reqinfo->disassoc_timer);
    bstm_req->validity_itvl = bstm_reqinfo->validity_itvl;

    /* Add options element neibour report */ 
    cb_info.frm = (u_int8_t *)(&bstm_req->opt_ies[0]);
    cb_info.ssid = vap->iv_bss->ni_essid;
    cb_info.ssid_len = vap->iv_bss->ni_esslen;
    /* Iterate the scan table to build Neighbor report */
    wlan_scan_table_iterate(vap, ieee80211_fill_nrinfo, &cb_info);
    wbuf_set_pktlen(wbuf, (cb_info.frm - (u_int8_t*)wbuf_header(wbuf)));
    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}

/* 
 * Bss transition management query recv handler
 */
int ieee80211_recv_bstm_query(wlan_if_t vap, wlan_node_t ni, 
                  struct ieee80211_action_bstm_query *bstm_query, int frm_len)
{
    u_int8_t elem_id, len;
    u_int8_t *frm, *sfrm;
    struct ieee80211_bstm_reqinfo reqinfo;
    reqinfo.dialogtoken = bstm_query->dialogtoken;
    reqinfo.candidate_list = 1;
    reqinfo.disassoc = 0;
    reqinfo.disassoc_timer = 0;
    reqinfo.validity_itvl = BSSTRANS_LIST_VALID_ITVL;

    sfrm = frm = &bstm_query->opt_ies[0];
    while ((frm - sfrm) < frm_len) {
       elem_id = *frm++;
       len = *frm++;
       switch (elem_id) {
       case IEEE80211_ELEMID_NEIGHBOR_REPORT:
	   break;
       default :
	   break;
       }
       frm += len;
    }

    /* send bss transition management req info */
    return ieee80211_send_bstm_req(vap, ni, &reqinfo);
}

/* 
 * Bss transition management response recv handler
 */
int ieee80211_recv_bstm_resp(wlan_if_t vap, wlan_node_t ni, 
                     struct ieee80211_action_bstm_resp *bstm_resp, int frm_len)
{
    u_int8_t elem_id, len;
    u_int8_t *frm, *sfrm;
    if (bstm_resp->status == 0) {
        /* Station Transition to other BSS */
        sfrm = frm = &bstm_resp->opt_ies[IEEE80211_ADDR_LEN];
    }
    else {
        /* Station Not Transition to other BSS */
        sfrm = frm = &bstm_resp->opt_ies[0];
    }
    while ((frm - sfrm) < frm_len) {
       elem_id = *frm++;
       len = *frm++;
       switch (elem_id) {
       case IEEE80211_ELEMID_NEIGHBOR_REPORT:
	   break;
       default :
	   break;
       }
       frm += len;
   }

   return 0;
}

int 
ieee80211_wnm_bss_validate_inactivity(struct ieee80211com *ic)
{
    struct ieee80211vap *vap;
    struct ieee80211_node_table *nt;
    struct ieee80211_node *ni = NULL, *next = NULL;
    systime_t time_diff;
    int idle_period;
	int time_msec;
    rwlock_state_t lock_state;

    nt = &ic->ic_sta;

    OS_RWLOCK_READ_LOCK(&nt->nt_nodelock, &lock_state);
    TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
        vap = ni->ni_vap;

        if ((vap->iv_opmode == IEEE80211_M_HOSTAP) && 
                              (ieee80211_vap_wnm_is_set(vap) == 1)) {
            if(IEEE80211_ADDR_EQ(ni->ni_macaddr, vap->iv_myaddr) &&
                 !ni->ni_associd) {
                continue;
            }
            time_diff = OS_GET_TIMESTAMP() - ni->ni_wnm->last_rcvpkt_tstamp;
            time_msec = CONVERT_SYSTEM_TIME_TO_MS(time_diff);
            idle_period = time_msec / 1000;
            if((idle_period >= vap->wnm->wnm_bss_max_idle_period)) {
                wlan_mlme_disassoc_request(ni->ni_vap,
                    ni->ni_macaddr, IEEE80211_REASON_ASSOC_EXPIRE);
            }
        }
    }
    OS_RWLOCK_READ_UNLOCK(&nt->nt_nodelock, &lock_state);

    return 1;
}

void ieee80211_wnm_bssmax_updaterx(struct ieee80211_node *ni, int secured)
{
    struct ieee80211vap *vap = ni->ni_vap;
    if (ieee80211_is_pmf_enabled(vap, ni)) {
        if(secured) {
            ni->ni_wnm->last_rcvpkt_tstamp = OS_GET_TIMESTAMP();
        }
    } else {
        ni->ni_wnm->last_rcvpkt_tstamp = OS_GET_TIMESTAMP();
    }
}


u_int8_t *
ieee80211_add_bssmax(u_int8_t *frm, struct ieee80211vap *vap)
{
    *frm++ = IEEE80211_ELEMID_BSSMAX_IDLE_PERIOD;
    *frm++ = 3;
    *(u_int16_t *)frm = htole16(vap->wnm->wnm_bss_max_idle_period);
    frm += 2;
    *frm++ = !!ieee80211_is_pmf_enabled(vap, vap->iv_bss);

    return frm;
}

/* Flexible Multicast Service(FMS) Routines */
static int ieee80211_fms_counter_free(ieee80211_fms_t fms, int cid)
{
    if (cid >= IEEE80211_FMS_MAX_COUNTERS)
        return -1;

    if (fms->counters[cid].numfms > 0)
    {
        fms->counters[cid].numfms--;
    }
   
    if (fms->counters[cid].numfms == 0)
    {
        fms->counters[cid].inuse = 0;
        fms->counters[cid].current_count = 0;
        fms->counters[cid].del_itvl = 0;
    }
    return 0;
}

static int ieee80211_fms_counter_create(ieee80211_fms_t fms, int del_itvl, int *counterid)
{
    int cid;

    for (cid = 0; cid < IEEE80211_FMS_MAX_COUNTERS; cid++)
    {
        /* Allocate first free counter */
        if (fms->counters[cid].inuse == 0)
        {
            fms->counters[cid].inuse = 1;
            break;
        }
    }

    if (cid < IEEE80211_FMS_MAX_COUNTERS)
    {
        fms->counters[cid].numfms++;
        *counterid = cid;
    }
    else
        return -1;
    return 0;
}

static bool ieee80211_fms_is_tclass_equal(struct tclas_element *tclass1, 
                                   struct tclas_element *tclass2)
{
    u_int8_t filter_len;

    if ((tclass1 == NULL) || (tclass2 == NULL))
    {
        return false;
    }
    if (tclass1->type != tclass2->type)
    {
        return false;
    }
    else
    {
        if (tclass1->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3)
        {
            filter_len = 0;
            if ((tclass1->tclas.type3.filter_offset != tclass2->tclas.type3.filter_offset) ||
                (OS_MEMCMP(tclass1->tclas.type3.filter_value, tclass2->tclas.type3.filter_value, filter_len)) ||
                (OS_MEMCMP(tclass1->tclas.type3.filter_mask, tclass2->tclas.type3.filter_mask, filter_len)))
            {
                return false;
            }
        }
        else if (tclass1->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4)
        {
            if(tclass1->tclas.type4.type4_v4.version != tclass2->tclas.type4.type4_v4.version) {
                return false;
            }

            if (tclass1->tclas.type4.type4_v4.version ==
                                IEEE80211_WNM_TCLAS_CLAS4_VERSION_4) 
            {
                if (OS_MEMCMP(tclass1->tclas.type4.type4_v4.src_ip,tclass2->tclas.type4.type4_v4.src_ip, IEEE80211_IPV4_LEN) ||
                    OS_MEMCMP(tclass1->tclas.type4.type4_v4.dst_ip,tclass2->tclas.type4.type4_v4.dst_ip, IEEE80211_IPV4_LEN) ||
                    (tclass1->tclas.type4.type4_v4.src_port != tclass2->tclas.type4.type4_v4.src_port) ||
                    (tclass1->tclas.type4.type4_v4.dst_port != tclass2->tclas.type4.type4_v4.dst_port) ||
                    (tclass1->tclas.type4.type4_v4.dscp != tclass2->tclas.type4.type4_v4.dscp) ||
                    (tclass1->tclas.type4.type4_v4.protocol != tclass2->tclas.type4.type4_v4.protocol))
                {
                    return false;
                }
            }

            if (tclass1->tclas.type4.type4_v6.version == IEEE80211_WNM_TCLAS_CLAS4_VERSION_6)
            {
                if ((OS_MEMCMP(tclass1->tclas.type4.type4_v6.src_ip, tclass2->tclas.type4.type4_v6.src_ip, IEEE80211_IPV6_LEN)) ||
                    (OS_MEMCMP(tclass1->tclas.type4.type4_v6.dst_ip, tclass2->tclas.type4.type4_v6.dst_ip, IEEE80211_IPV6_LEN)) ||
                    (tclass1->tclas.type4.type4_v6.src_port != tclass2->tclas.type4.type4_v6.src_port) ||
                    (tclass1->tclas.type4.type4_v6.dst_port != tclass2->tclas.type4.type4_v6.dst_port) ||
                    (tclass1->tclas.type4.type4_v6.dscp != tclass2->tclas.type4.type4_v6.dscp) ||
                    (tclass1->tclas.type4.type4_v6.next_header != tclass2->tclas.type4.type4_v6.next_header) ||
                    OS_MEMCMP(tclass1->tclas.type4.type4_v6.flow_label, tclass2->tclas.type4.type4_v6.flow_label, 3))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

static bool ieee80211_fms_subelement_match(ieee80211_fms_stream_t *fms_stream, 
                                    ieee80211_fmsreq_subele_t *fmsreq_subele)
{
    struct tclas_element *tclass1, *tclass2;
    bool tclass_found = false;

    if (fms_stream->num_tclas != fmsreq_subele->num_tclas) {
        return false;
    }
    if ((fms_stream->del_itvl != fmsreq_subele->del_itvl) ||
        (fms_stream->max_del_itvl !=  fmsreq_subele->max_del_itvl) ||
        (OS_MEMCMP(&fms_stream->rate_id, &fmsreq_subele->rate_id, sizeof(ieee80211_fms_rate_identifier_t)))) {
        return false;
    }
    
    TAILQ_FOREACH(tclass2, &fmsreq_subele->tclas_head, tclas_next)
    {
        tclass_found = false;
    
        TAILQ_FOREACH(tclass1, &fms_stream->tclas_head, tclas_next)
        {
            if (ieee80211_fms_is_tclass_equal(tclass1, tclass2))
            {
                /* tclass found to go next tclass */
                tclass_found = true;
                break;
            }
        }
        if (tclass_found == false)
        {
            break;
        }
    }

    if (tclass2 == NULL && tclass_found == true){
        return true;
    }
    else {
        return false;
    }
}


static bool ieee80211_fms_subelement_tclass_match(void *head1, 
                                                  void *head2)
{
    struct tclas_element *tclass1, *tclass2;
    bool tclass_found = false;
    TAILQ_HEAD(, tclas_element) *fmsstrm_tclas;
    TAILQ_HEAD(, tclas_element) *fmsreq_tclas;

    fmsstrm_tclas = head1;
    fmsreq_tclas = head2;
    
    TAILQ_FOREACH(tclass2, fmsreq_tclas, tclas_next)
    {
        tclass_found = false;
    
        TAILQ_FOREACH(tclass1, fmsstrm_tclas, tclas_next)
        {
            if (ieee80211_fms_is_tclass_equal(tclass1, tclass2))
            {
                /* tclass found to go next tclass */
                tclass_found = true;
                break;
            }
        }
        if (tclass_found == false)
        {
            break;
        }
    }

    if (tclass2 == NULL && tclass_found == true)
    {
        return true;
    }
    else {
        return false;
    }
}

/* Deletes the specified FMS stream */
static int ieee80211_fms_stream_delete (wlan_if_t vap, wlan_node_t ni,
                                 ieee80211_fms_stream_t *fms_stream_delete)
{
    struct tclas_element *tclas;
    ieee80211_fms_stream_t  *fms_stream;
    ieee80211_fms_counter_t *fms_counter;
    ieee80211_fms_t fms;
    ieee80211_fmsreq_active_element_t *fmsact_ie;
    ieee80211_fms_act_stream_ptr_t *fmsstream_ptr;


    fms = vap->wnm->wnm_fms_data;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "*** Entering function %s ***\n", __func__);
    TAILQ_FOREACH(fmsact_ie, &ni->ni_wnm->fmsreq_act_head, fmsreq_act_next) {
            TAILQ_FOREACH(fmsstream_ptr, &fmsact_ie->fmsact_strmptr_head,strm_ptr_next) {
                fms_stream = fmsstream_ptr->stream;
                if (fms_stream->fmsid == fms_stream_delete->fmsid) {
                    fms_counter = &fms->counters[fms_stream->counter_id];
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Removing stream %d from node/database \n", fms_stream->fmsid);
                    TAILQ_REMOVE(&fmsact_ie->fmsact_strmptr_head, fmsstream_ptr, strm_ptr_next);
                    OS_FREE(fmsstream_ptr);
                    if (TAILQ_EMPTY(&fmsact_ie->fmsact_strmptr_head)) {
                        TAILQ_REMOVE(&ni->ni_wnm->fmsreq_act_head, fmsact_ie, fmsreq_act_next);
                        OS_FREE(fmsact_ie);
                    }
                    TAILQ_FOREACH(tclas, &fms_stream->tclas_head, tclas_next) {
                            TAILQ_REMOVE(&fms_stream->tclas_head, tclas, tclas_next);
                            OS_FREE(tclas);
                    }
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "%s : freeing counter %d\n", 
                                          __func__, fms_stream->counter_id); 
                    ieee80211_fms_counter_free(fms, fms_stream->counter_id);
                    TAILQ_REMOVE(&fms_counter->fms_stream_head, fms_stream, stream_next);
                    OS_FREE(fms_stream);
                }
            }
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "*** Exiting function %s ***\n", __func__);
    return 0;
}

static ieee80211_fms_stream_t* ieee80211_fms_stream_add(wlan_if_t vap, wlan_node_t ni,
                                                 ieee80211_fmsreq_subele_t *fmsreq_subele,
                                                 ieee80211_fms_subelement_status_t *fmssubele_status)
{
    ieee80211_fms_t fms;
    ieee80211_fms_counter_t *fms_counter;
    ieee80211_fms_stream_t *new_fms_stream;
    int i, counter_id, rv;

    fms = vap->wnm->wnm_fms_data;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Enter Function %s\n", __func__);


    for (i = 0; i < IEEE80211_FMS_MAX_COUNTERS; i++)
    {
        if ((fms->counters[i].inuse == 1) &&
                (fms->counters[i].del_itvl == fmsreq_subele->del_itvl))
        {
            /* Reusing the same counter i */
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Reusing counter id - %d for del_itvl %d\n", i, fmsreq_subele->del_itvl);
            counter_id = i;
            break;
        }
    }

    if (i == IEEE80211_FMS_MAX_COUNTERS)
    {
        rv = ieee80211_fms_counter_create(fms, fmsreq_subele->del_itvl, &counter_id);
        if (rv != 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " %s : line %d no free counters available\n", __func__, __LINE__);
            return NULL;
        } 
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "New counter id %d for del_itvl %d\n", counter_id, fmsreq_subele->del_itvl);
        fms_counter = &fms->counters[counter_id];
        fms_counter->counter_id = counter_id;
        fms_counter->del_itvl = fmsreq_subele->del_itvl;
        fms_counter->current_count = fmsreq_subele->del_itvl;
    }
    else {
        fms_counter = &fms->counters[counter_id];
    }

    new_fms_stream = (ieee80211_fms_stream_t *)
        OS_MALLOC(vap->iv_ic->ic_osdev,
                (sizeof(ieee80211_fms_stream_t)), 0);

    if (new_fms_stream == NULL) {
        printk("%s: %d failed OS_MALLOC failed !\n",
                __func__, __LINE__);
        return NULL;
    }

    OS_MEMSET(new_fms_stream, 0, sizeof(ieee80211_fms_stream_t));
    new_fms_stream->counter_id = fms_counter->counter_id;
    new_fms_stream->fmsid = ++fms->fmsid;
    new_fms_stream->del_itvl = fmsreq_subele->del_itvl;
    new_fms_stream->max_del_itvl = fmsreq_subele->max_del_itvl;
    new_fms_stream->rate_id.mask = fmsreq_subele->rate_id.mask;
    new_fms_stream->rate_id.mcs_idx = fmsreq_subele->rate_id.mcs_idx;
    new_fms_stream->rate_id.rate = fmsreq_subele->rate_id.rate;
    new_fms_stream->num_tclas = fmsreq_subele->num_tclas;
    OS_MEMCPY(new_fms_stream->mcast_addr, fmsreq_subele->mcast_addr, 6);
    TAILQ_INIT(&new_fms_stream->tclas_head);
    TAILQ_CONCAT(&new_fms_stream->tclas_head, &fmsreq_subele->tclas_head, tclas_next);
    OS_MEMCPY(&new_fms_stream->tclasprocess, &fmsreq_subele->tclasprocess, 
            sizeof(struct ieee80211_tclas_processing));

    fmsreq_subele->accepted = 1;

    fmssubele_status->fmsid = new_fms_stream->fmsid;
    fmssubele_status->fms_counter = (fms_counter->counter_id << 5) | 
                                            fms_counter->current_count;
    TAILQ_INSERT_TAIL(&fms_counter->fms_stream_head, new_fms_stream, stream_next);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "*** Exiting function %s ***\n", __func__);
    return new_fms_stream;
}

static int ieee80211_recv_fmssubele_parse(wlan_if_t vap, wlan_node_t ni, 
                                   void *head, u_int8_t *frm, u_int8_t fmsreq_ie_len)
        
{
   u_int8_t                                tclass_len;
   ieee80211_fmsreq_subele_t               *fmsreq_subele;
   TAILQ_HEAD(, ieee80211_fmsreq_subele_s) *fmssubele_head;
   struct fmsresp_tclas_subele_status      tclas_status_msg = {0};
   int rv;
   u_int8_t mcast_addr[6] = {0x01, 0x00, 0x5e, 0, 0, 0};
   u_int16_t rate;
   
   fmssubele_head = head;
   TAILQ_INIT(fmssubele_head);

   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Enter Function %s\n", __func__);
   while (fmsreq_ie_len > 0) {
  
       fmsreq_subele = (ieee80211_fmsreq_subele_t *)
           OS_MALLOC(vap->iv_ic->ic_osdev,
                   (sizeof(ieee80211_fmsreq_subele_t)), 0);
       
       if (fmsreq_subele == NULL) {
           printk("%s: %d failed OS_MALLOC failed !\n",
                          __func__, __LINE__);
           return ENOMEM;
       }
       fmsreq_subele->elem_id = *frm++;
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Subelement Id %x\n", fmsreq_subele->elem_id);
       if (IEEE80211_FMS_SUBELEMENT_ID == fmsreq_subele->elem_id) {
           fmsreq_subele->len = *frm++;
           fmsreq_subele->del_itvl = *frm++;
           fmsreq_subele->max_del_itvl = *frm++;
           fmsreq_subele->rate_id.mask = *frm++;
           fmsreq_subele->rate_id.mcs_idx = *frm++;
           memcpy(&rate,frm, 2); 
           fmsreq_subele->rate_id.rate = be16toh(rate);
           frm += 2;
       
           tclass_len = fmsreq_subele->len - 6;
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "len %d di %d max di %d \n", fmsreq_subele->len, 
                   fmsreq_subele->del_itvl, fmsreq_subele->max_del_itvl);

           if(tclass_len > 0) {
               if ((rv = ieee80211_tclass_element_parse(vap, &fmsreq_subele->tclas_head,&fmsreq_subele->tclasprocess, 
                                                        &tclas_status_msg, &frm, tclass_len)) != 0) {
                   OS_FREE(fmsreq_subele);
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "function %s failed at line %d\n",
                          __func__, __LINE__);
                   return rv;
               }
               if (tclas_status_msg.ismcast != 1) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "function %s failed at line %d\n",
                          __func__, __LINE__);
                   OS_FREE(fmsreq_subele);
                   return EINVAL;
               }
               else {
                   mcast_addr[3] = (tclas_status_msg.mcast_ipaddr >> 16) & 0x7f;
                   mcast_addr[4] = (tclas_status_msg.mcast_ipaddr >> 8) & 0xff;
                   mcast_addr[5] = (tclas_status_msg.mcast_ipaddr & 0xff);
                   memcpy(&fmsreq_subele->mcast_addr, mcast_addr, 6);
               }
               
           }
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "clas_status_msg.num_tclas_elements %d\n",tclas_status_msg.num_tclas_elements);
           fmsreq_subele->num_tclas = tclas_status_msg.num_tclas_elements;
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "fmsreq_ie_len %d\n", fmsreq_ie_len);
           fmsreq_ie_len = fmsreq_ie_len - (fmsreq_subele->len + 3);
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "fmsreq_ie_len %d\n", fmsreq_ie_len);
           TAILQ_INSERT_TAIL(fmssubele_head, fmsreq_subele, fmssubele_next);
       }
       else {
           printk("function %s : %d unknown elementid %d\n",
                          __func__, __LINE__, fmsreq_subele->elem_id);
           OS_FREE(fmsreq_subele);
           return EINVAL;
       }
   }
   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Exit Function %s\n", __func__);
   return 0;
}
static void ieee80211_fms_node_stream_delete(void *arg, wlan_node_t ni)
{
    ieee80211_fms_stream_t *fms_stream_match = (ieee80211_fms_stream_t *)arg;
    ieee80211_fms_stream_t *fms_stream;
    ieee80211_fmsreq_active_element_t *fmsact_ie;
    ieee80211_fms_act_stream_ptr_t *fmsstream_ptr;
    u_int8_t stream_deleted = 0;

    if (fms_stream_match == NULL) {
        return;
    }
    
    TAILQ_FOREACH(fmsact_ie, &ni->ni_wnm->fmsreq_act_head, fmsreq_act_next) {
        TAILQ_FOREACH(fmsstream_ptr, &fmsact_ie->fmsact_strmptr_head,strm_ptr_next) {
            fms_stream = fmsstream_ptr->stream;
            if (fms_stream->fmsid == fms_stream_match->fmsid) {
                stream_deleted =1;
                fms_stream->num_sta--;
                TAILQ_REMOVE(&fmsact_ie->fmsact_strmptr_head, fmsstream_ptr, strm_ptr_next);
                OS_FREE(fmsstream_ptr);
                if (TAILQ_EMPTY(&fmsact_ie->fmsact_strmptr_head)) {
                    TAILQ_REMOVE(&ni->ni_wnm->fmsreq_act_head, fmsact_ie, fmsreq_act_next);
                    OS_FREE(fmsact_ie);
                }
                break;
            }
        }
        if (stream_deleted == 1) {
            break;
        }
    }
}

/* Sends unsolicit response to the group to indicate 
   the change in FMS stream parameters */
static int ieee80211_send_unsol_fmsrsp(wlan_if_t vap, wlan_node_t ni,
                                ieee80211_fms_subelement_status_t *fmssubele_status)
{
    wbuf_t                                 wbuf = NULL;
    u_int8_t                               *frm = NULL;
    u_int8_t                               *len;
    struct ieee80211_frame                 *wh;
    struct ieee80211_fmsrsp                *fmsrsp;
    u_int16_t rate;
    /*u_int8_t bcast_addr[6] = {0xff,0xff,0xff,0xff,0xff, 0xff};*/

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Enter Function %s: status %d del_itvl=%d max_del_itvl=%d\n", __func__, 
            fmssubele_status->element_status, fmssubele_status->del_itvl,fmssubele_status->max_del_itvl);
    fmsrsp = ni->ni_wnm->fmsrsp;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }

    *frm++ = IEEE80211_ACTION_CAT_WNM;
    *frm++ = IEEE80211_ACTION_FMS_RESP;
    *frm++ = ni->ni_wnm->fmsreq->dialog_token;

    *frm++ = IEEE80211_ELEMID_FMS_RESPONSE;
    len = frm;
    *len = 1;
    frm++;

    *frm++ = 0; /* fms_token */
    /* setup the wireless header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
#if 1
    ieee80211_send_setup(vap, ni, wh,
                         IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_ACTION,
                         vap->iv_myaddr, fmssubele_status->mcast_addr, ni->ni_bssid);
#else
    /* Multicast FMS request frames application
       using broadcast for testing */
    ieee80211_send_setup(vap, ni, wh,
                         IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_ACTION,
                         vap->iv_myaddr, bcast_addr, ni->ni_bssid);
#endif

    *frm++ = IEEE80211_FMS_STATUS_SUBELE;
    *frm++ = 15;
    *frm++ = fmssubele_status->element_status;
    *frm++ = fmssubele_status->del_itvl;
    *frm++ = fmssubele_status->max_del_itvl;
    *frm++ = fmssubele_status->fmsid;
    *frm++ = fmssubele_status->fms_counter;
    *frm++ = fmssubele_status->rate_id.mask;
    *frm++ = fmssubele_status->rate_id.mcs_idx;
    rate  = htobe16(fmssubele_status->rate_id.rate);
    OS_MEMCPY(frm, &rate, 2);
    frm += 2;

    OS_MEMCPY(frm, &fmssubele_status->mcast_addr, 6);
    frm += 6;
    *len += 17;
    
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Exit Function %s\n", __func__);
    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}

static int ieee80211_fms_stream_modify(wlan_if_t vap, wlan_node_t ni,
                                ieee80211_fms_stream_t *fms_stream_match,
                                ieee80211_fmsreq_subele_t *fmsreq_subele,
                                ieee80211_fms_subelement_status_t *fmssubele_status) 
{
   ieee80211_fms_t fms = vap->wnm->wnm_fms_data; 
   u_int8_t new_del_itvl, new_max_del_itvl;
   u_int8_t old_del_itvl, old_max_del_itvl;
   u_int8_t rx_del_itvl, rx_max_del_itvl;
   ieee80211_fms_subelement_status_t unsol_subele_status;
   int unsol_rsp_send = 1;
   ieee80211_fms_counter_t *fms_counter, *new_counter;
   int unsol_fms_status=-1;
   int i, counter_id, rv;

   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "*** Entering function %s\n", __func__);
   /* FMS stream delivery interval has changed */
   old_del_itvl = fms_stream_match->del_itvl;
   old_max_del_itvl = fms_stream_match->max_del_itvl;

   rx_del_itvl = fmsreq_subele->del_itvl;
   rx_max_del_itvl = fmsreq_subele->max_del_itvl;

   if ((old_max_del_itvl == 0 && rx_max_del_itvl != 0) ||
           (old_max_del_itvl != 0 && rx_max_del_itvl !=0 && rx_max_del_itvl < old_max_del_itvl))
   {
       new_max_del_itvl = rx_max_del_itvl;
       if (old_del_itvl > rx_max_del_itvl)
       {
           unsol_fms_status = IEEE80211_WNM_FMSSUBELE_ALTE_MAX_DEL_ITVL_CHG;
           new_del_itvl = rx_max_del_itvl;
       }
       else
       {
           if (old_del_itvl < rx_del_itvl)
           {
               new_del_itvl = rx_del_itvl;
           }
           else if (old_del_itvl >= rx_del_itvl)
           {
               new_del_itvl = old_del_itvl;

           }
       }
   }
   else if ((old_max_del_itvl != 0 &&  rx_max_del_itvl == 0) ||
           (old_max_del_itvl != 0 && rx_max_del_itvl !=0 && rx_max_del_itvl > old_max_del_itvl))
   {
       new_max_del_itvl = old_max_del_itvl;
       if (rx_del_itvl > old_max_del_itvl)
       {
           new_del_itvl = old_max_del_itvl;
       }
       else
       {
           if (rx_del_itvl >= old_del_itvl)
           {
               new_del_itvl = rx_del_itvl;
           }
           else if (rx_del_itvl < old_del_itvl)
           {
               new_del_itvl = old_del_itvl;
           }
       }
   }
   else if (rx_max_del_itvl == old_max_del_itvl)
   {
       new_max_del_itvl = rx_max_del_itvl;
       if (old_del_itvl < rx_del_itvl)
       {
           new_del_itvl = rx_del_itvl;
       }
       else if (old_del_itvl >= rx_del_itvl)
       {
           new_del_itvl = old_del_itvl;
       }
   }
   else
   {
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "new_del_itvl/new_max_del_itvl computation failed\n");
       return EINVAL;
   }

   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "old %d.%d rx %d.%d new %d.%d\n", old_del_itvl, old_max_del_itvl,
           rx_del_itvl, rx_max_del_itvl, new_del_itvl, new_max_del_itvl);
   if (new_del_itvl != old_del_itvl)
   {
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "new_del_itvl %d old_del_itvl %d creating new counter\n", new_del_itvl, old_del_itvl);
       for (i = 0; i < IEEE80211_FMS_MAX_COUNTERS; i++)
       {
           if ((fms->counters[i].inuse == 1) &&
                   (fms->counters[i].del_itvl == new_del_itvl))
           {
               /* Reusing the same counter i */
               IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Reusing counter id - %d for del_itvl %d\n", i, fmsreq_subele->del_itvl);
               counter_id = i;
               break;
           }
       }

       if (i == IEEE80211_FMS_MAX_COUNTERS)
       {
           rv = ieee80211_fms_counter_create(fms, new_del_itvl, &counter_id);
           if (rv != 0) {
               IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " %s : line %d no free counters available\n", __func__, __LINE__);
               return ENOMEM;
           }
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "New counter id %d for del_itvl %d\n", counter_id, new_del_itvl);
           new_counter = &fms->counters[counter_id];
           new_counter->counter_id = counter_id;
           new_counter->del_itvl = new_del_itvl;
           new_counter->current_count = new_del_itvl;
       }
       else {
           new_counter = &fms->counters[counter_id];
       }

       fms_counter = &fms->counters[fms_stream_match->counter_id];
       TAILQ_REMOVE(&fms_counter->fms_stream_head, fms_stream_match, stream_next);
       if (TAILQ_EMPTY(&fms_counter->fms_stream_head)) {
           fms_counter->inuse = 0;
       }

       fms_stream_match->counter_id = counter_id;
       fms_stream_match->del_itvl = new_del_itvl;
       fms_stream_match->max_del_itvl = new_max_del_itvl;
       /* Moving the stream to new counter */
       TAILQ_INSERT_TAIL(&new_counter->fms_stream_head, fms_stream_match, stream_next);
       fmssubele_status->fms_counter = (new_counter->counter_id << 5 | new_counter->current_count);
       fmssubele_status->fmsid = fms_stream_match->fmsid;
   }

   if (new_max_del_itvl != old_max_del_itvl && new_del_itvl == old_del_itvl)  {
       unsol_fms_status = IEEE80211_WNM_FMSSUBELE_ALTE_MAX_DEL_ITVL_CHG;
   }
   else if (new_del_itvl != old_del_itvl && new_max_del_itvl == old_max_del_itvl) {
       unsol_fms_status = IEEE80211_WNM_FMSSUBELE_ALTE_DEL_ITVL_CHANGE; 
   }
   else if (new_del_itvl != old_del_itvl && new_max_del_itvl != old_max_del_itvl) {
       unsol_fms_status = IEEE80211_WNM_FMSSUBELE_ALTE_MAX_DEL_ITVL_CHG;
   }
   else if (OS_MEMCMP(&fms_stream_match->rate_id, &fmsreq_subele->rate_id, 
                     sizeof(ieee80211_fms_rate_identifier_t))) {
       unsol_fms_status = IEEE80211_WNM_FMSSUBELE_ALTE_MCAST_RATE_CHANGE;
   }
   else {
       unsol_rsp_send = 0;
   }

   if (unsol_rsp_send == 1) {
       OS_MEMSET(&unsol_subele_status, 0, sizeof(ieee80211_fms_subelement_status_t));
       unsol_subele_status.element_status = unsol_fms_status;
       unsol_subele_status.del_itvl = new_del_itvl;
       unsol_subele_status.max_del_itvl = new_max_del_itvl;
       unsol_subele_status.fmsid = fms_stream_match->fmsid;
       OS_MEMCPY(unsol_subele_status.mcast_addr,fms_stream_match->mcast_addr, 6); 
       OS_MEMCPY(&unsol_subele_status.rate_id, &fms_stream_match->rate_id, 
               sizeof(ieee80211_fms_rate_identifier_t)); 
       ieee80211_send_unsol_fmsrsp(vap, ni, &unsol_subele_status);
   }

   if (new_max_del_itvl != rx_max_del_itvl) {
       fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_ALTE_MAX_DEL_ITVL_CHG;
   }
   else if (new_del_itvl != rx_del_itvl) {
       fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_ALTE_DEL_ITVL;
   } 
   else if (new_del_itvl == rx_del_itvl) { 
       fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_ACCEPT;
   }
   else {
       fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_DENY_UNSPECIFIED;
   }
   fmssubele_status->del_itvl = new_del_itvl;
   fmssubele_status->max_del_itvl = new_max_del_itvl;
   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "*** Exiting function %s\n", __func__);
   return 0;
}
static int ieee80211_fmsreq_subelement_modify(wlan_if_t vap, wlan_node_t ni,
                                       void *head1, void *head2, void *head3)
{  struct tclas_element *tclas;
   TAILQ_HEAD(, ieee80211_fmsreq_subele_s) *fmssubele_head;
   TAILQ_HEAD(, ieee80211_fms_status_subelement_s) *status_subele_head;
   TAILQ_HEAD(, ieee80211_fms_act_stream_ptr) *fmsact_strmptr_head;
   ieee80211_fmsreq_subele_t          *fmsreq_subele;
   ieee80211_fms_stream_t             *fms_stream=NULL, *fms_stream_match=NULL;
   ieee80211_fms_subelement_status_t  *fmssubele_status;
   ieee80211_fms_act_stream_ptr_t     *fmsstream_ptr = NULL;
   ieee80211_fms_status_subelement_t  *status_subele=NULL;
   ieee80211_fms_counter_t            *fms_counter = NULL, *new_counter;
   ieee80211_fms_t                    fms;
   int                                exact_match,stream_updated;
   int i, rv;
   int stream_deleted, counter_id;

   fmssubele_head = head1;
   status_subele_head = head2;
   fmsact_strmptr_head = head3;
   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Enter Function %s\n", __func__);
   fms = vap->wnm->wnm_fms_data;

   TAILQ_INIT(status_subele_head);


    TAILQ_FOREACH(fmsreq_subele, fmssubele_head, fmssubele_next) {

       stream_updated = 0;
       stream_deleted = 0;
       exact_match = 0;
       /* If it is not new FMS request */
       TAILQ_FOREACH(fmsstream_ptr, fmsact_strmptr_head, strm_ptr_next) {

           fms_stream_match = NULL;
           fms_stream = fmsstream_ptr->stream;

           if (ieee80211_fms_subelement_match(fms_stream, fmsreq_subele))
           { 
               /* Received old FMS request with same FMS stream from the same client
                * do not increment num_sta counter */
               fms_stream_match = fms_stream;
               exact_match = 1; 
               break;
           }
           else if (ieee80211_fms_subelement_tclass_match(&fms_stream->tclas_head, &fmsreq_subele->tclas_head)) {
               fms_stream_match = fms_stream; 
               if (fmsreq_subele->del_itvl == 0) {
                   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Delivery interval is zero - deleting stream from node\n");
                   stream_deleted = 1; 
                   TAILQ_REMOVE(fmsact_strmptr_head, fmsstream_ptr, strm_ptr_next);
                   OS_FREE(fmsstream_ptr);

                   fms_stream->num_sta--;
                   if (fms_stream->num_sta == 0) {
                       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Deleting stream from the database\n");
                       TAILQ_FOREACH(tclas, &fms_stream->tclas_head, tclas_next) {
                           TAILQ_REMOVE(&fms_stream->tclas_head, tclas, tclas_next);
                           OS_FREE(tclas);
                       }
                       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "%s : freeing counter %d\n", 
                                          __func__, fms_stream->counter_id); 
                       ieee80211_fms_counter_free(fms, fms_stream->counter_id);
                       fms_counter = &fms->counters[fms_stream->counter_id];
                       TAILQ_REMOVE(&fms_counter->fms_stream_head, fms_stream, stream_next);
                       OS_FREE(fms_stream);
                   }
                   break;
               }
               else {
                   stream_updated = 1;
                   break;
               }
           }
       }
   
       if (fms_stream_match == NULL) {
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Received new FMS stream in the old FMS request\n");
           continue;
       }

       if (stream_deleted == 1) {
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Old stream deleted - continue processing next subelement\n");
           continue;
       }

       status_subele = (ieee80211_fms_status_subelement_t *)
                       OS_MALLOC(vap->iv_ic->ic_osdev,(sizeof(ieee80211_fms_status_subelement_t)), 0);

       if (status_subele == NULL) {
           printk("%s: %d failed OS_MALLOC failed !\n",
                   __func__, __LINE__);
           return ENOMEM;
       }


       fmssubele_status = (ieee80211_fms_subelement_status_t *)
                       OS_MALLOC(vap->iv_ic->ic_osdev,(sizeof(ieee80211_fms_subelement_status_t)), 0);

       if (fmssubele_status == NULL) {
                printk("%s: %d failed OS_MALLOC failed !\n",
                          __func__, __LINE__);
           OS_FREE(status_subele);
           printk("function %s :%d OS_MALLOC failed !\n",
                   __func__, __LINE__);
           return ENOMEM;
       }

       OS_MEMSET(fmssubele_status, 0,sizeof (ieee80211_fms_subelement_status_t));
       OS_MEMSET(status_subele, 0,sizeof (ieee80211_fms_status_subelement_t));

       if (exact_match == 1 && fms_stream != NULL) {
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Found exact match\n"); 
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "fms_stream->num_sta %d\n", fms_stream->num_sta);
           fms_counter = &fms->counters[fms_stream->counter_id];
           fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_ACCEPT;
           fmssubele_status->del_itvl = fms_stream->del_itvl;
           fmssubele_status->max_del_itvl = fms_stream->max_del_itvl;
           fmssubele_status->fmsid = fms_stream->fmsid;
           fmssubele_status->fms_counter = (fms_stream->counter_id << 5 | fms_counter->current_count);
           memcpy(&fmssubele_status->rate_id, &fms_stream->rate_id, sizeof(ieee80211_fms_rate_identifier_t));
           memcpy(fmssubele_status->mcast_addr, fms_stream->mcast_addr, 6);
       }
       else if (stream_updated == 1 && fms_stream_match != NULL) {
           if (fms_stream_match->num_sta == 1) {
               /* If this is the only station uses the stream 
                  move the stream to other counter */
               fms_counter = &fms->counters[fms_stream_match->counter_id];
               TAILQ_REMOVE(&fms_counter->fms_stream_head, fms_stream_match, stream_next);
               if (TAILQ_EMPTY(&fms_counter->fms_stream_head)) {
                   fms_counter->inuse = 0;
               }

               for (i = 0; i < IEEE80211_FMS_MAX_COUNTERS; i++)
               {
                   if ((fms->counters[i].inuse == 1) &&
                           (fms->counters[i].del_itvl == fmsreq_subele->del_itvl))
                   {
                       /* Reusing the same counter i */
                       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Reusing counter id - %d for del_itvl %d\n", i, fmsreq_subele->del_itvl);
                       counter_id = i;
                       break;
                   }
               }
               if (i == IEEE80211_FMS_MAX_COUNTERS)
               {
                   rv = ieee80211_fms_counter_create(fms, fmsreq_subele->del_itvl, &counter_id);
                   if (rv != 0) {
                       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " %s : line %d no free counters available\n", __func__, __LINE__);
                        OS_FREE(fmssubele_status);
                        OS_FREE(status_subele);
                       return ENOMEM;
                   }
                   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "New counter id %d for del_itvl %d\n", counter_id, fmsreq_subele->del_itvl);
                   new_counter = &fms->counters[counter_id];
                   new_counter->counter_id = counter_id;
                   new_counter->del_itvl = fmsreq_subele->del_itvl;
                   new_counter->current_count = fmsreq_subele->del_itvl;
               }
               else {
                   new_counter = &fms->counters[counter_id];
               }

               fms_stream_match->counter_id = counter_id;
               fms_stream_match->del_itvl = fmsreq_subele->del_itvl;
               fms_stream_match->max_del_itvl = fmsreq_subele->max_del_itvl;
               OS_MEMCPY(&fms_stream_match->rate_id, &fmsreq_subele->rate_id, sizeof(ieee80211_fms_rate_identifier_t));
               OS_MEMCPY(fms_stream_match->mcast_addr, fmsreq_subele->mcast_addr, 6);

               /* Moving the stream to new counter */
               TAILQ_INSERT_TAIL(&new_counter->fms_stream_head, fms_stream_match, stream_next);
               fmssubele_status->del_itvl = fmsreq_subele->del_itvl;
               fmssubele_status->max_del_itvl = fmsreq_subele->max_del_itvl;
               fmssubele_status->fms_counter = (new_counter->counter_id << 5 | new_counter->current_count);
               fmssubele_status->fmsid = fms_stream_match->fmsid;
               fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_ACCEPT;
           }
           else {
               rv = ieee80211_fms_stream_modify(vap, ni, fms_stream_match, 
                                                fmsreq_subele, fmssubele_status);
               /* Remove the stream ptr from the node 
                  if it fails to modify the existing stream due to conflicts
                  station will send new request with correct params */
               if (fmssubele_status->element_status != IEEE80211_WNM_FMSSUBELE_ACCEPT) {
                   fms_stream_match->num_sta--;
                   TAILQ_REMOVE(fmsact_strmptr_head, fmsstream_ptr, strm_ptr_next);
                   OS_FREE(fmsstream_ptr);
               } 
           }
           OS_MEMCPY(&fmssubele_status->rate_id, &fmsreq_subele->rate_id, sizeof(ieee80211_fms_rate_identifier_t));
           OS_MEMCPY(fmssubele_status->mcast_addr, fmsreq_subele->mcast_addr, 6); 
      }
       else {
           OS_FREE(fmssubele_status);
           OS_FREE(status_subele);
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Invalid FMS request\n");
           return EINVAL;
       }

       status_subele->subelementid = IEEE80211_FMS_STATUS_SUBELE;
       status_subele->subele_status = (void *)fmssubele_status;
       TAILQ_INSERT_TAIL(status_subele_head, status_subele, status_subele_next);
    }

    return 0;
}
static int ieee80211_fmsreq_subelement_process(wlan_if_t vap, wlan_node_t ni, 
                                        void *head1, void *head2, void *head3)
{
   TAILQ_HEAD(, ieee80211_fmsreq_subele_s) *fmssubele_head;
   TAILQ_HEAD(, ieee80211_fms_status_subelement_s) *status_subele_head;
   TAILQ_HEAD(, ieee80211_fms_act_stream_ptr) *fmsact_strmptr_head;
   ieee80211_fmsreq_subele_t          *fmsreq_subele;
   ieee80211_fms_stream_t             *fms_stream=NULL;
   ieee80211_fms_stream_t             *fms_stream_match=NULL;
   ieee80211_fms_subelement_status_t  *fmssubele_status;
   ieee80211_fms_act_stream_ptr_t     *fmsstream_ptr = NULL;
   ieee80211_fms_status_subelement_t  *status_subele;
   ieee80211_fms_counter_t            *fms_counter = NULL;
   ieee80211_fms_t                    fms;
   int                                exact_match,num_matches, partial_match;
   int                                cid, rv, old_del_itvl;

   fmssubele_head = head1;
   status_subele_head = head2;
   fmsact_strmptr_head = head3;
   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Enter Function %s\n", __func__);
   fms = vap->wnm->wnm_fms_data;

   TAILQ_INIT(status_subele_head);

   TAILQ_FOREACH(fmsreq_subele, fmssubele_head, fmssubele_next) {

       exact_match = 0;
       partial_match = 0;
       num_matches = 0;
       old_del_itvl = 0;
       status_subele = (ieee80211_fms_status_subelement_t *)
            OS_MALLOC(vap->iv_ic->ic_osdev,(sizeof(ieee80211_fms_status_subelement_t)), 0);
           
       if (status_subele == NULL) {
           printk("function %s :%d OS_MALLOC failed !\n",
                     __func__, __LINE__);
           return ENOMEM;
       }

       fmssubele_status = (ieee80211_fms_subelement_status_t *)
           OS_MALLOC(vap->iv_ic->ic_osdev,(sizeof(ieee80211_fms_subelement_status_t)), 0);

       if (fmssubele_status == NULL) {
           printk("function %s :%d OS_MALLOC failed !\n",
                     __func__, __LINE__);
           OS_FREE(status_subele);
           return ENOMEM;
       }


       OS_MEMSET(fmssubele_status, 0,sizeof (ieee80211_fms_subelement_status_t));
       OS_MEMSET(status_subele, 0,sizeof (ieee80211_fms_status_subelement_t));
       /* Search the global stream list when received a new FMS request */

       for (cid = 0; cid < IEEE80211_FMS_MAX_COUNTERS; cid++)
       {
           fms_counter = &fms->counters[cid];
           if (fms_counter->inuse == 1)
           {
               IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "fms_counter id %d is in use\n", cid);

               TAILQ_FOREACH(fms_stream, &fms_counter->fms_stream_head, stream_next)
               {
                   if (ieee80211_fms_subelement_match(fms_stream, fmsreq_subele))
                   { 
                       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Found exact match\n"); 
                       exact_match = 1;
                       break;
                   }
            
                   if (ieee80211_fms_subelement_tclass_match(&fms_stream->tclas_head, &fmsreq_subele->tclas_head))
                   {
                       num_matches++;
                       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "num_matches %d\n", num_matches);
                       if (num_matches > 1) {
                           /* Deny FMS stream with all tclass classifiers matching 
                              two streams with different delivery intervals */
                           if (old_del_itvl != fms_stream->del_itvl) {
                               IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "FMS stream matches two streams with different del_itvls\n"); 
                               break;
                           }
                       }

                       old_del_itvl = fms_stream->del_itvl;
                       if ((fms_stream->num_tclas != fmsreq_subele->num_tclas) && 
                               (fms_stream->del_itvl == fmsreq_subele->del_itvl)) {
                           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Partial match\n");
                           partial_match = 1;
                       }
                       fms_stream_match = fms_stream;
                   }
               }
           }
           if (exact_match || num_matches) {
               break;
           }
       }
       
       if (exact_match == 1 && fms_counter != NULL && fms_stream != NULL)
       {
           fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_ACCEPT;
           fmssubele_status->del_itvl = fms_stream->del_itvl;
           fmssubele_status->max_del_itvl = fms_stream->max_del_itvl;
           fmssubele_status->fmsid = fms_stream->fmsid;
           fmssubele_status->fms_counter = (fms_counter->counter_id << 5 | fms_counter->current_count);
           memcpy(&fmssubele_status->rate_id, &fms_stream->rate_id, sizeof(ieee80211_fms_rate_identifier_t));
           memcpy(fmssubele_status->mcast_addr, fms_stream->mcast_addr, 6);
       }
       else if (num_matches > 1)
       {
           /* Deny FMS stream with all tclass classifiers matching 
              two streams with different delivery intervals */
           fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_DENY_CLASSIFIER;
           fmssubele_status->del_itvl = fmsreq_subele->del_itvl;
           fmssubele_status->max_del_itvl = fmsreq_subele->max_del_itvl;
           fmssubele_status->fmsid = 0;
           fmssubele_status->fms_counter = 0;
           memcpy(&fmssubele_status->rate_id, &fmsreq_subele->rate_id, sizeof(ieee80211_fms_rate_identifier_t));
           memcpy(fmssubele_status->mcast_addr, fmsreq_subele->mcast_addr, 6);
       }
       else if (num_matches == 1 && fms_stream_match != NULL)
       {
           rv = ieee80211_fms_stream_modify(vap, ni, fms_stream_match, 
                                            fmsreq_subele, fmssubele_status);
           fms_stream = fms_stream_match;
       }
       else
       {
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Creating new FMS stream\n");
           fms_stream = ieee80211_fms_stream_add(vap, ni, fmsreq_subele, fmssubele_status);
           if (fms_stream == NULL){
               fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_TERM_LACK_RESOURCES;
           }
           else {
               fmssubele_status->element_status = IEEE80211_WNM_FMSSUBELE_ACCEPT;
           }

           fmssubele_status->del_itvl = fmsreq_subele->del_itvl;
           fmssubele_status->max_del_itvl = fmsreq_subele->max_del_itvl;
           fmssubele_status->rate_id.mask = fmsreq_subele->rate_id.mask;
           fmssubele_status->rate_id.mcs_idx = fmsreq_subele->rate_id.mcs_idx;
           fmssubele_status->rate_id.rate = fmsreq_subele->rate_id.rate;
           memcpy(fmssubele_status->mcast_addr, fmsreq_subele->mcast_addr, 6);
       }

       if (fmssubele_status->element_status == IEEE80211_WNM_FMSSUBELE_ACCEPT) {
           fms_stream->num_sta++;
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Number of clients for stream %d = %d\n",
                   fms_stream->fmsid, fms_stream->num_sta);
           fmsstream_ptr = (ieee80211_fms_act_stream_ptr_t *)
               OS_MALLOC(vap->iv_ic->ic_osdev,(sizeof(ieee80211_fms_act_stream_ptr_t)), 0);
           
           if (fmsstream_ptr == NULL) {
               printk("function %s :%d OS_MALLOC failed !\n",
                       __func__, __LINE__);
               OS_FREE(status_subele);
               OS_FREE(fmssubele_status);
               return ENOMEM;
           }

           fmsstream_ptr->stream = fms_stream;
           TAILQ_INSERT_TAIL(fmsact_strmptr_head, fmsstream_ptr, strm_ptr_next);
       }

       status_subele->subelementid = IEEE80211_FMS_STATUS_SUBELE;
       status_subele->subele_status = (void *)fmssubele_status;
       TAILQ_INSERT_TAIL(status_subele_head, status_subele, status_subele_next);
   }

   IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Exit Function %s\n", __func__);
   return 0;
}

static int ieee80211_recv_fmsreq_process(wlan_if_t vap, wlan_node_t ni, 
                                  struct ieee80211_fmsreq *fmsreq,
                                  struct ieee80211_fmsrsp *fmsrsp)
{
    ieee80211_fms_response_t   *fmsrsp_ie;
    ieee80211_fms_request_t    *fmsreq_ie;
    ieee80211_fmsreq_active_element_t *fmsact_ie;
    ieee80211_fms_t fms;
    int rv=0;

    fms = vap->wnm->wnm_fms_data;

    TAILQ_INIT(&fmsrsp->fmsrsp_head);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Enter Function %s\n", __func__);

    TAILQ_FOREACH(fmsreq_ie, &fmsreq->fmsreq_head, fmsreq_next) {

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Processing req element %p\n", fmsreq_ie);
        fmsrsp_ie = (ieee80211_fms_response_t *)
                      OS_MALLOC(vap->iv_ic->ic_osdev,
                      (sizeof(ieee80211_fms_response_t)), 0);

        if (fmsrsp_ie == NULL) {
            printk("function %s :%d OS_MALLOC failed !\n",
                     __func__, __LINE__);
            ieee80211_wnm_free_fmsrsp(&fmsrsp->fmsrsp_head);
            return ENOMEM;
        }
      
        if (fmsreq_ie->fms_token == 0) {

            fmsact_ie = (ieee80211_fmsreq_active_element_t *)
                    OS_MALLOC(vap->iv_ic->ic_osdev,(sizeof(ieee80211_fmsreq_active_element_t)), 0);
       

            if (fmsact_ie == NULL){
                ieee80211_wnm_free_fmsrsp(&fmsrsp->fmsrsp_head);
                printk("function %s :%d OS_MALLOC failed !\n",
                     __func__, __LINE__);

                OS_FREE(fmsrsp_ie);
                return ENOMEM;
            }

            TAILQ_INIT(&fmsact_ie->fmsact_strmptr_head);
            rv = ieee80211_fmsreq_subelement_process(vap, ni, 
                                                     &fmsreq_ie->fmssubele_head, 
                                                     &fmsrsp_ie->status_subele_head, 
                                                     &fmsact_ie->fmsact_strmptr_head);

            /* Assign the unique fms_token if atleast one 
               fms subelement is accepted */
            if (!TAILQ_EMPTY(&fmsact_ie->fmsact_strmptr_head)) {
                fmsrsp_ie->fms_token = ++fms->fms_token;
                fmsact_ie->fms_token = fmsrsp_ie->fms_token;
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Adding token %d fmsact_ie %p to ni %p\n", fmsact_ie->fms_token,fmsact_ie, ni);
                TAILQ_INSERT_TAIL(&ni->ni_wnm->fmsreq_act_head, fmsact_ie, fmsreq_act_next);
            }
            else {
                OS_FREE(fmsact_ie);
            }
        }
        else {
             TAILQ_FOREACH(fmsact_ie, &ni->ni_wnm->fmsreq_act_head, fmsreq_act_next) {

                 IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "received token %d saved %d fmsact_ie %p ni %p\n", 
                         fmsreq_ie->fms_token, fmsact_ie->fms_token, fmsact_ie, ni);
                 if (fmsact_ie->fms_token == fmsreq_ie->fms_token) {

                     rv = ieee80211_fmsreq_subelement_modify(vap, ni, 
                                                             &fmsreq_ie->fmssubele_head, 
                                                     &fmsrsp_ie->status_subele_head, 
                                                     &fmsact_ie->fmsact_strmptr_head);
                     if (TAILQ_EMPTY(&fmsact_ie->fmsact_strmptr_head)) {
                         TAILQ_REMOVE(&ni->ni_wnm->fmsreq_act_head, fmsact_ie, fmsreq_act_next);
                         OS_FREE(fmsact_ie);
                     }
                     else {
                         fmsrsp_ie->fms_token = fmsact_ie->fms_token;
                     }
                 }
             }
        }

        if (rv != 0) {
            OS_FREE(fmsrsp_ie);
            ieee80211_wnm_free_fmsrsp(&fmsrsp->fmsrsp_head);
            printk("%s : %d subelement processing failed !\n",
                    __func__, __LINE__);
            return ENOMEM;
        }
        if (!TAILQ_EMPTY(&fmsrsp_ie->status_subele_head)) {
            TAILQ_INSERT_TAIL(&fmsrsp->fmsrsp_head, fmsrsp_ie, fmsrsp_next);
        }
        else {
            OS_FREE(fmsrsp_ie);
        }
    }
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Exit Function %s\n", __func__);
    return rv;
}

static int ieee80211_send_fms_rsp(wlan_if_t vap, wlan_node_t ni)
{
    wbuf_t                                 wbuf = NULL;
    u_int8_t                               *frm = NULL;
    u_int8_t                               *len;
    struct ieee80211_fmsrsp                *fmsrsp;
    ieee80211_fms_response_t               *fmsrsp_ele;
    ieee80211_fms_status_subelement_t      *status_subele;
    u_int16_t rate;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Enter Function %s\n", __func__);
    fmsrsp = ni->ni_wnm->fmsrsp;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }

    *frm++ = IEEE80211_ACTION_CAT_WNM;
    *frm++ = IEEE80211_ACTION_FMS_RESP;
    *frm++ = ni->ni_wnm->fmsreq->dialog_token;

    TAILQ_FOREACH(fmsrsp_ele, &fmsrsp->fmsrsp_head,  fmsrsp_next)
    {
        *frm++ = IEEE80211_ELEMID_FMS_RESPONSE;
        len = frm;
        *len = 1;
        frm++;
        *frm++ = fmsrsp_ele->fms_token;
        TAILQ_FOREACH(status_subele, &fmsrsp_ele->status_subele_head, status_subele_next)
        {
            if (status_subele->subelementid == IEEE80211_FMS_STATUS_SUBELE)
            {
                ieee80211_fms_subelement_status_t *fmssubele_status;

                fmssubele_status = (ieee80211_fms_subelement_status_t *)
                                    status_subele->subele_status;
                *frm++ = IEEE80211_FMS_STATUS_SUBELE;
                *frm++ = 15;
                *frm++ = fmssubele_status->element_status;
                *frm++ = fmssubele_status->del_itvl;
                *frm++ = fmssubele_status->max_del_itvl;
                *frm++ = fmssubele_status->fmsid;
                *frm++ = fmssubele_status->fms_counter;
                *frm++ = fmssubele_status->rate_id.mask;
                *frm++ = fmssubele_status->rate_id.mcs_idx;
                rate  = htobe16(fmssubele_status->rate_id.rate);
                OS_MEMCPY(frm, &rate, 2);
                frm += 2;
                OS_MEMCPY(frm, &fmssubele_status->mcast_addr, 6);
                frm += 6;
                *len += 17;
            }
            else if (status_subele->subelementid == IEEE80211_TCLASS_STATUS_SUBELE)
            {
                struct tclas_element *tclass_element;
                ieee80211_tclass_subelement_status_t *tclas_status;
                u_int8_t *tclas_len;

                tclas_status = (ieee80211_tclass_subelement_status_t *)
                                status_subele->subele_status;
                *frm++ = IEEE80211_TCLASS_STATUS_SUBELE;
                tclas_len = frm;
                *tclas_len = 1;
                frm += 1;
                *frm++ = tclas_status->fmsid;

                TAILQ_FOREACH(tclass_element, &tclas_status->tclas_head, tclas_next)
                {
                    *frm++ = tclass_element->elemid;
                    *frm++ = tclass_element->len;
                    *frm++ = tclass_element->up;
                    *frm++ = tclass_element->type;
                    *frm++ = tclass_element->mask;
                    *tclas_len += 5;

                    if (tclass_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3)
                    {
                        *(u_int16_t *)frm++ = tclass_element->tclas.type3.filter_offset;
                        *tclas_len += 2;
                        memcpy(frm, tclass_element->tclas.type3.filter_value, 
                               tclass_element->tclas.type3.filter_len);
                        frm += tclass_element->tclas.type3.filter_len;
                        *tclas_len += tclass_element->tclas.type3.filter_len;
                        memcpy(frm, tclass_element->tclas.type3.filter_mask, tclass_element->tclas.type3.filter_len);
                        frm += tclass_element->tclas.type3.filter_len;
                        *tclas_len += tclass_element->tclas.type3.filter_len;
                    }
   
                    if (tclass_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4)
                    {
                        *frm++ = tclass_element->tclas.type4.type4_v4.version;
                        if(tclass_element->tclas.type4.type4_v4.version == IEEE80211_WNM_TCLAS_CLAS4_VERSION_4)
                        {
                            memcpy(frm, tclass_element->tclas.type4.type4_v4.src_ip, IEEE80211_IPV4_LEN);
                            frm += IEEE80211_IPV4_LEN;
                            memcpy(frm, tclass_element->tclas.type4.type4_v4.dst_ip, IEEE80211_IPV4_LEN);
                            frm += IEEE80211_IPV4_LEN;
                            memcpy(frm, &tclass_element->tclas.type4.type4_v4.src_port, 2);
                            frm += 2;
                            memcpy(frm, &tclass_element->tclas.type4.type4_v4.dst_port, 2);
                            frm += 2;
                            *frm++= tclass_element->tclas.type4.type4_v4.dscp;
                            *frm++= tclass_element->tclas.type4.type4_v4.protocol;
                            *frm++= 0; /* Reserved byte */
                            *tclas_len += 15;
                        }
                        else if (tclass_element->tclas.type4.type4_v6.version == IEEE80211_WNM_TCLAS_CLAS4_VERSION_6)
                        {
                            memcpy(frm, tclass_element->tclas.type4.type4_v6.src_ip, IEEE80211_IPV6_LEN);
                            frm += IEEE80211_IPV6_LEN;
                            memcpy(frm, tclass_element->tclas.type4.type4_v6.dst_ip, IEEE80211_IPV6_LEN);
                            frm += IEEE80211_IPV6_LEN;
                            memcpy(frm, &tclass_element->tclas.type4.type4_v4.src_port, 2);
                            frm += 2;
                            memcpy(frm, &tclass_element->tclas.type4.type4_v4.dst_port, 2);
                            frm += 2;
                            *frm++= tclass_element->tclas.type4.type4_v6.dscp;
                            *frm++= tclass_element->tclas.type4.type4_v6.next_header;
                            memcpy(frm, tclass_element->tclas.type4.type4_v6.flow_label, 3);
                            frm += 3;
                            *tclas_len += 41;
                        }
                    }
                }
                *len += *tclas_len;
                *len += 2;
            }
        }
    }
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Exit Function %s\n", __func__);
    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}

int ieee80211_recv_fms_req(wlan_if_t vap, wlan_node_t ni,
                           u_int8_t *frm, int frm_len)
{
    u_int8_t                 *sfrm;
    ieee80211_fms_request_t  *fmsreq_ie; 
    struct ieee80211_fmsreq  *fmsreq;
    struct ieee80211_fmsrsp  *fmsrsp;
    int                      rv;
    u_int8_t                 subelem_len;
    
    fmsreq = ni->ni_wnm->fmsreq;
    fmsrsp = ni->ni_wnm->fmsrsp;

    TAILQ_INIT(&fmsreq->fmsreq_head);

   
    sfrm = frm;
    frm = frm + 2;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Enter Function %s\n", __func__);
   
    fmsreq->dialog_token = *frm++;

    while ((frm - sfrm) < frm_len)
    {
        fmsreq_ie = (ieee80211_fms_request_t *)
                   OS_MALLOC(vap->iv_ic->ic_osdev,
                  (sizeof(ieee80211_fms_request_t)), 0);

        if (fmsreq_ie == NULL) {
            printk("function %s : %d OS_MALLOC failed !\n",
                    __func__, __LINE__);
            ieee80211_wnm_free_fmsreq(&fmsreq->fmsreq_head);
            return ENOMEM;
        }

        fmsreq_ie->elem_id = *frm++;

        if(IEEE80211_ELEMID_FMS_REQUEST == fmsreq_ie->elem_id) {

            subelem_len = *frm++;

            fmsreq_ie->fms_token = *frm++;

            if (subelem_len > 0)
            {
                rv = ieee80211_recv_fmssubele_parse(vap, ni, &fmsreq_ie->fmssubele_head, frm, subelem_len);
                if (rv != 0)
                {
                    OS_FREE(fmsreq_ie);
                    ieee80211_wnm_free_fmsreq(&fmsreq->fmsreq_head);
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, 
                                      "function %s : %d ieee80211_recv_fmssubele_parse failed !\n",
                                      __func__, __LINE__);
                    return ENOMEM;
                }
            }
       
            frm = frm + subelem_len - 1;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Adding req element %p\n", fmsreq_ie);
            TAILQ_INSERT_TAIL(&fmsreq->fmsreq_head, fmsreq_ie, fmsreq_next);
        }
        else {
           OS_FREE(fmsreq_ie);
           ieee80211_wnm_free_fmsreq(&fmsreq->fmsreq_head);
           printk("%s : %d unknown request type\n",
                    __func__, __LINE__);
           return EINVAL;
        }
    }

    if ((rv = ieee80211_recv_fmsreq_process(vap, ni, fmsreq, fmsrsp)) != 0)
    {
        ieee80211_wnm_free_fmsreq(&fmsreq->fmsreq_head);
        printk("function %s: %d ieee80211_recv_fmsreq_process failed!\n",
                    __func__, __LINE__);
        return rv;
    }

    ieee80211_wnm_free_fmsreq(&fmsreq->fmsreq_head);
    if (!TAILQ_EMPTY(&fmsrsp->fmsrsp_head)) {
        rv = ieee80211_send_fms_rsp(vap, ni);
    }
    ieee80211_wnm_free_fmsrsp(&fmsrsp->fmsrsp_head);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " Exiting Function %s\n", __func__);

    return rv;
}
void
ieee80211_fms_filter(struct ieee80211_node *ni, wbuf_t wbuf,
                     int ether_type, struct ip_header *ip,int hdrsize)
{
    ieee80211_fms_counter_t *fms_counter;
    ieee80211_fms_stream_t *fms_stream;
    ieee80211_fms_t fms;
    int filter_len, i;
    unsigned char *bp;
    u_int32_t saddr, daddr;
    int tclas_count = 0;
    int tclas_match = 0;
    int filter_match = 1;
    int status = 0;
    int buf_len = 0;
    struct tclas_element *tclas_element;
    u_int8_t protocol;
    u_int8_t dscp;
    u_int8_t sport_match;
    u_int8_t dport_match;
    u_int16_t sport;
    u_int16_t dport;
    u_int16_t *tcpudphdr;
    u_int16_t offset;

    fms = ni->ni_vap->wnm->wnm_fms_data;

    for (i = 0; i < IEEE80211_FMS_MAX_COUNTERS; i++) {
        fms_counter = &fms->counters[i];
        TAILQ_FOREACH(fms_stream, &fms_counter->fms_stream_head, stream_next) {

            tclas_count = 0;
            tclas_match = 0;
            if (fms_stream->del_itvl_changed == 1) {
                continue;
            }
            TAILQ_FOREACH(tclas_element, &fms_stream->tclas_head, tclas_next) {

                tclas_count++;
                if(tclas_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) {
                    offset = tclas_element->tclas.type3.filter_offset;
                    filter_len = (tclas_element->len - 5) / 2;
                    buf_len = wbuf_get_pktlen(wbuf);
                    if(buf_len < (filter_len)) {
                        filter_match = 0;
                        break;
                    }
                    bp = (u_int8_t *)(wbuf->data);
                    for(i = 0; i < filter_len; i++) {
                        if((bp[offset + i] &
							tclas_element->tclas.type3.filter_mask[i]) !=
							(tclas_element->tclas.type3.filter_value[i] &
							tclas_element->tclas.type3.filter_mask[i])) {
                            filter_match = 0;
                            break;
                         }
                     }
                     if(filter_match) {
                        if(fms_stream->tclasprocess.tclas_process == 1) {
                            tclas_match = tclas_count;
                            break;
                        }
  	                    tclas_match++;
                     }
                } else if(tclas_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4) {
                    if ((ether_type == ETHERTYPE_IP)  &&
                        (tclas_element->tclas.type4.type4_v4.version ==
                                IEEE80211_WNM_TCLAS_CLAS4_VERSION_4)) {
                        adf_net_iphdr_t *hdr;
                        hdr = (adf_net_iphdr_t *)(ip);

                        sport = tclas_element->tclas.type4.type4_v4.src_port;
                        dport = tclas_element->tclas.type4.type4_v4.dst_port;
                        protocol = tclas_element->tclas.type4.type4_v4.protocol;
                        dscp = tclas_element->tclas.type4.type4_v4.dscp & (~INET_ECN_MASK);

                        memcpy(&saddr,
                             tclas_element->tclas.type4.type4_v4.src_ip, IEEE80211_IPV4_LEN);
                        memcpy(&daddr,
                                tclas_element->tclas.type4.type4_v4.dst_ip, IEEE80211_IPV4_LEN);


                        if ((hdr->ip_proto == protocol) &&
                            (saddr == (hdr->ip_saddr)) &&
                            (daddr == (hdr->ip_daddr)) &&
                            (dscp == (hdr->ip_tos & ~INET_ECN_MASK))) {
                            tcpudphdr = (u_int16_t*)((u_int32_t *)hdr + hdr->ip_hl);
                            sport_match = 1;
                            if (sport && (sport != (*tcpudphdr))) {
                                sport_match = 0;
                            }
                            dport_match = 1;
                            if (dport && (dport != (*(tcpudphdr + 1)))) {
                                dport_match = 0;
                            }
                            if (dport_match && sport_match) {
                                if(fms_stream->tclasprocess.tclas_process == 1) {
                                    tclas_match = tclas_count;
                                    break;
                                } 
                                tclas_match++;
                            }
                        }
                    } else if ((ether_type == ETHERTYPE_IPV6) &&
                        (tclas_element->tclas.type4.type4_v6.version ==
                           IEEE80211_WNM_TCLAS_CLAS4_VERSION_6)) {
                        adf_net_ipv6hdr_t *ipv6;
                        unsigned long ver_pri_flowlabel;
                        unsigned long pri;
                        unsigned long *hdr;
                        u_int8_t *src_ip;
                        u_int8_t *dst_ip;
                        u_int8_t *flowlbl;
                        u_int8_t tos;
                        u_int8_t nexthdr;

                        hdr = (unsigned long*)(ip);
                        ipv6 = (adf_net_ipv6hdr_t *) hdr;
                        ver_pri_flowlabel = *hdr;
                        sport = tclas_element->tclas.type4.type4_v4.src_port;
                        dport = tclas_element->tclas.type4.type4_v4.dst_port;
                        flowlbl = tclas_element->tclas.type4.type4_v6.flow_label;
                        dscp = tclas_element->tclas.type4.type4_v6.dscp &
                                                          (~INET_ECN_MASK);
                        nexthdr = tclas_element->tclas.type4.type4_v6.next_header;
                        pri = ((ver_pri_flowlabel) & IPV6_PRIORITY_MASK) >> IPV6_PRIORITY_SHIFT;
                        tos = (pri & (~INET_ECN_MASK)) >> IP_PRI_SHIFT;
                        src_ip = tclas_element->tclas.type4.type4_v6.src_ip;
                        dst_ip = tclas_element->tclas.type4.type4_v6.dst_ip;
                        if ((ipv6->ipv6_nexthdr == nexthdr) &&
                            (OS_MEMCMP(ipv6->ipv6_flow_lbl, flowlbl, 3) == 0) &&
                            (OS_MEMCMP(src_ip, ipv6->ipv6_saddr.s6_addr, IEEE80211_IPV6_LEN) == 0) &&
                            (OS_MEMCMP(dst_ip, ipv6->ipv6_daddr.s6_addr, IEEE80211_IPV6_LEN) == 0) &&
                            (tos == dscp)) {
                            tcpudphdr = (u_int16_t *)(ipv6 + 1);
                            sport_match = 1;
                            if (sport && (sport != (*tcpudphdr))) {
                                sport_match = 0;
                            }
                            dport_match = 1;
                            if (dport && (dport != (*(tcpudphdr + 1)))) {
                                dport_match = 0;
                            }
                            if (dport_match && sport_match) {
                                if(fms_stream->tclasprocess.tclas_process == 1) {
                                    tclas_match = tclas_count;
                                    break;
                                }  
                                tclas_match++;
                            }
                       }
                    } else {
                        tclas_match++;
                    }
                }
            }
            if(tclas_count == tclas_match) {
                 status = 1;
                 break;
            } 
        }
        if(status) {
            wbuf_set_fmsstream(wbuf);
            wbuf_set_fmsqid(wbuf, fms_counter->counter_id);
            break;
        }
    }
}

/* Creates the FMS descriptor to be sent out in the beacon.
 * On return, fmsie points to the desc or NULL, fmsie_len points to len of desc.
 */
int ieee80211_wnm_setup_fmsdesc_ie(struct ieee80211_node *ni, int dtim, u_int8_t **fmsie,
                                   u_int8_t *fmsie_len, u_int32_t *fms_counter_mask)
{
    struct tclas_element *tclas;
    struct ieee80211vap *vap = ni->ni_vap;
    ieee80211_wnm_t wnm = vap->wnm;
    ieee80211_fms_t fms = vap->wnm->wnm_fms_data;
    ieee80211_fms_counter_t *fms_counter;
    ieee80211_fms_stream_t *fms_stream;
    struct ieee80211_fms_desc_ie *fmsdesc;
    u_int8_t *fmsid;
    int fms_desc_len = 1; /* at least 1, if present */

    if (!ieee80211_vap_wnm_is_set(vap) || !wnm ||
        (vap->iv_opmode != IEEE80211_M_HOSTAP)) {
        *fmsie = NULL;
        *fmsie_len = 0;
        return 0;
    }

    OS_MEMZERO(wnm->wnm_fms_desc_ie, sizeof(wnm->wnm_fms_desc_ie));
    fmsdesc = (struct ieee80211_fms_desc_ie *)wnm->wnm_fms_desc_ie;
    fmsid = (u_int8_t *)fmsdesc;
    fmsid += 2; /* elemid+len */
    OS_MEMZERO(fmsdesc, sizeof(wnm->wnm_fms_desc_ie));

    fmsdesc->id = IEEE80211_ELEMID_FMS_DESCRIPTOR;
    {
        int i;
        int numcounters = 0;

        for (i = 0; i < IEEE80211_FMS_MAX_COUNTERS; i++) {
            fms_counter = &fms->counters[i];
            if (fms_counter->inuse == 1) {
                numcounters++;
                fmsdesc->fmscounters[i].id = fms_counter->counter_id;

                if (dtim)
                    fms_counter->current_count--;
        
                fmsdesc->fmscounters[i].count = fms_counter->current_count;
                fms_desc_len++; /* id, count are in the same byte */
            }
        }
        fmsdesc->numcounters = numcounters;
        fmsid += fms_desc_len;
        for (i = 0;i < IEEE80211_FMS_MAX_COUNTERS; i++) {
            fms_counter = &fms->counters[i];
            if (fms_counter->inuse == 1 && fms_counter->current_count == 0) {
                /* Append FMSIDs of the streams when associated counter becomes 
                   zero and resetting fms counter to delivery interval 
                 */
                *fms_counter_mask |= (1 << i);
                TAILQ_FOREACH(fms_stream, &fms_counter->fms_stream_head, stream_next) {
                    *fmsid = fms_stream->fmsid;
                    fmsid++;
                    fms_desc_len++;
                    if (fms_stream->del_itvl_changed == 1) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "%s : Deleting stream %d and free counter %d\n", 
                                          __func__,fms_stream->fmsid, fms_stream->counter_id); 
                        ieee80211_fms_counter_free(fms, fms_stream->counter_id);
                        TAILQ_FOREACH(tclas, &fms_stream->tclas_head, tclas_next) {
                            TAILQ_REMOVE(&fms_stream->tclas_head, tclas, tclas_next);
                            OS_FREE(tclas);
                        }

                        TAILQ_REMOVE(&fms_counter->fms_stream_head, fms_stream, stream_next);
                        OS_FREE(fms_stream);
                    }
                }

                fms_counter->current_count = fms_counter->del_itvl;
            }
        }
    }
    fmsdesc->len = fms_desc_len;
    *fmsie = (u_int8_t*)fmsdesc;
    *fmsie_len = (fms_desc_len + 2); /* plus elem id and len */
    return 0;
}
int ieee80211_recv_tfs_req(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len)
{
    u_int8_t status = -1;

    if(frm_len == 3) {
        wlan_wnm_free_tfs(&ni->ni_wnm->tfsreq->tfs_req_head);
        return 1;
    }

    frm = frm + 2;

    ni->ni_wnm->tfsreq->dialogtoken = *frm++;
    frm = ieee80211_add_tfs_req(vap, ni, frm, frm_len);

    if(frm != NULL) {
        status = ieee80211_send_tfs_rsp(vap, ni);
        if(status == 0) {
            wlan_wnm_free_tfsrsp(&ni->ni_wnm->tfsrsp->tfs_rsp_head);
        }
    }
    return 1;
}


u_int8_t *ieee80211_add_tfs_req(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len)
{
    uint8_t *filter_value;
    uint8_t *filter_mask;
    int tfs_length;
    int subele_length;
    TAILQ_HEAD(, ieee80211_tfs_request) tfs_req_head;
    TAILQ_HEAD(, ieee80211_tfs_response) tfs_rsp_head;
    struct ieee80211_tfs_subelement_req *tfs_subele;
    struct ieee80211_tfs_subelement_rsp *tfs_subrsp;
    struct ieee80211_tfs_request *tfs_req;
    struct ieee80211_tfs_response *tfs_rsp;
    struct tclas_element *tclas;
	int filter_len;
    u_int8_t *sfrm;

    TAILQ_INIT(&tfs_req_head);

    TAILQ_INIT(&tfs_rsp_head);


    sfrm = frm;
    while ((frm - sfrm) <= frm_len) {
        if(*frm != IEEE80211_ELEMID_TFS_REQUEST) {
            break;
        }
        tfs_req = (struct ieee80211_tfs_request *)
                  OS_MALLOC(vap->iv_ic->ic_osdev,
                      (sizeof(struct ieee80211_tfs_request)), 0);
        if (tfs_req == NULL) {
            wlan_wnm_free_tfs(&tfs_req_head);
            OS_FREE(tfs_req);
            return NULL;
        }

        tfs_rsp = (struct ieee80211_tfs_response *)
                  OS_MALLOC(vap->iv_ic->ic_osdev,
                      (sizeof(struct ieee80211_tfs_response)), 0);
        if (tfs_rsp == NULL) {
            wlan_wnm_free_tfs(&tfs_req_head);
            wlan_wnm_free_tfsrsp(&tfs_rsp_head);
            OS_FREE(tfs_rsp);
            return NULL;
        }

        tfs_req->tfs_elemid = *frm++;
        tfs_req->length = *frm++;
        tfs_length = tfs_req->length;
        tfs_req->tfs_id = *frm++;
        tfs_req->tfs_action_code = *frm++;

        TAILQ_INIT(&tfs_req->tfs_req_sub);

        TAILQ_INIT(&tfs_rsp->tfs_rsp_sub);

        tfs_length = tfs_length - 2;

        while(tfs_length > 0) {
            tfs_subele = (struct ieee80211_tfs_subelement_req *)
                      OS_MALLOC(vap->iv_ic->ic_osdev,
                            (sizeof(struct ieee80211_tfs_subelement_req)),0);
            if (tfs_subele == NULL) {
                OS_FREE(tfs_req);
                OS_FREE(tfs_rsp);
                OS_FREE(tfs_subele);
                wlan_wnm_free_tfs(&tfs_req_head);
                wlan_wnm_free_tfsrsp(&tfs_rsp_head);
                return NULL;
            }

            tfs_subrsp = (struct ieee80211_tfs_subelement_rsp *)
                      OS_MALLOC(vap->iv_ic->ic_osdev,
                            (sizeof(struct ieee80211_tfs_subelement_rsp)),0);
            if (tfs_subrsp == NULL) {
                OS_FREE(tfs_req);
                OS_FREE(tfs_rsp);
                OS_FREE(tfs_subele);
                OS_FREE(tfs_subrsp);
                wlan_wnm_free_tfs(&tfs_rsp_head);
                wlan_wnm_free_tfsrsp(&tfs_rsp_head);
                return NULL;
            }

            tfs_subele->elem_id = *frm++;
            tfs_subele->length = *frm++;
            subele_length = tfs_subele->length;
            tfs_length = tfs_length - (subele_length + 2);

            TAILQ_INIT(&tfs_subele->tclas_head);
            while(subele_length != 0) {

                tclas = (struct tclas_element *)
                        OS_MALLOC(vap->iv_ic->ic_osdev,
                              (sizeof(struct tclas_element)), 0);
                if (tclas == NULL) {
                    wlan_wnm_free_tfs(&tfs_req_head);
                    wlan_wnm_free_tfsrsp(&tfs_rsp_head);
                    OS_FREE(tfs_req);
                    OS_FREE(tfs_rsp);
                    OS_FREE(tfs_subele);
                    OS_FREE(tfs_subrsp);
                    OS_FREE(tclas);
                    return NULL;
                }

                if(*frm == IEEE80211_ELEMID_TCLAS) {
                    tclas->elemid = *frm++;
                    tclas->len = *frm++;
                    tclas->up = *frm++;
                    tclas->type = *frm++;
                    tclas->mask = *frm++;
                    if(tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) {
                        filter_len = (tclas->len - 5) / 2;
                        filter_value = (uint8_t *) OS_MALLOC(vap->iv_ic->
                                    ic_osdev, filter_len, GFP_KERNEL);
                        if (filter_value == NULL) {
                            wlan_wnm_free_tfs(&tfs_req_head);
                            wlan_wnm_free_tfsrsp(&tfs_rsp_head);
                            OS_FREE(tfs_req);
                            OS_FREE(tfs_rsp);
                            OS_FREE(tfs_subele);
                            OS_FREE(tfs_subrsp);
                            OS_FREE(tclas);
                            OS_FREE(filter_value);
                            return NULL;
                        }
                        filter_mask = (uint8_t *) OS_MALLOC(vap->iv_ic->
                                    ic_osdev, filter_len, GFP_KERNEL);
                        if (filter_mask == NULL) {
                            wlan_wnm_free_tfs(&tfs_req_head);
                            wlan_wnm_free_tfsrsp(&tfs_rsp_head);
                            OS_FREE(tfs_req);
                            OS_FREE(tfs_rsp);
                            OS_FREE(tfs_subele);
                            OS_FREE(tfs_subrsp);
                            OS_FREE(tclas);
                            OS_FREE(filter_mask);
                            return NULL;
                        }
                        tclas->tclas.type3.filter_offset = be16toh(*(u_int16_t* )frm);
                        frm += 2;
                        memcpy(filter_value, frm , filter_len);
                        tclas->tclas.type3.filter_value = filter_value;
                        frm += filter_len;
                        memcpy(filter_mask, frm, filter_len);
                        tclas->tclas.type3.filter_mask = filter_mask;
                        frm += filter_len;
                    } else if (tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4) {
                        if(*frm == IEEE80211_WNM_TCLAS_CLAS4_VERSION_4) {
                            tclas->tclas.type4.type4_v4.version = *frm++;
                            memcpy(tclas->tclas.type4.type4_v4.src_ip, frm ,
                                                   IEEE80211_IPV4_LEN);
                            frm += IEEE80211_IPV4_LEN;
                            memcpy(tclas->tclas.type4.type4_v4.dst_ip, frm ,
                                                    IEEE80211_IPV4_LEN);
                            frm += IEEE80211_IPV4_LEN;
                            tclas->tclas.type4.type4_v4.src_port =
                                                    be16toh(*(u_int16_t* )frm);
                            frm += 2;

                            tclas->tclas.type4.type4_v4.dst_port =
                                                    be16toh(*(u_int16_t* )frm);
                            frm += 2;
                            tclas->tclas.type4.type4_v4.dscp = *frm++;
                            tclas->tclas.type4.type4_v4.protocol = *frm++;
                            tclas->tclas.type4.type4_v4.reserved = *frm++;
                        } else if(*frm == IEEE80211_WNM_TCLAS_CLAS4_VERSION_6) {
                            tclas->tclas.type4.type4_v6.version = *frm++;
                            memcpy(tclas->tclas.type4.type4_v6.src_ip, frm ,
                                                    IEEE80211_IPV6_LEN);
                            frm += IEEE80211_IPV6_LEN;
                            memcpy(tclas->tclas.type4.type4_v6.dst_ip, frm ,
                                                    IEEE80211_IPV6_LEN);
                            frm += IEEE80211_IPV6_LEN;
                            tclas->tclas.type4.type4_v6.src_port =
                                                    be16toh(*(u_int16_t* )frm);
                            frm += 2;
                            tclas->tclas.type4.type4_v6.dst_port =
                                                    be16toh(*(u_int16_t* )frm);
                            frm += 2;
                            tclas->tclas.type4.type4_v6.dscp = *frm++;
                            tclas->tclas.type4.type4_v6.next_header = *frm++;
                            memcpy(tclas->tclas.type4.type4_v6.flow_label,
                                                                    frm, 3);
                            frm += 3;
                        }
                    } else {
                        tfs_subrsp->status = IEEE80211_TFS_INVALID_CLASSIFIER;
                        break;
                    }
                
                } else if(*frm == IEEE80211_ELEMID_TCLAS_PROCESS) {
                    tfs_subele->tclasprocess.elem_id = *frm++;
                    tfs_subele->tclasprocess.length = *frm++;
                    tfs_subele->tclasprocess.tclas_process = *frm++;
                    break;
                } else {
                    tfs_subrsp->status = IEEE80211_TFS_REASON_UNKNOWN;
                    break;
                }
                TAILQ_INSERT_TAIL(&tfs_subele->tclas_head, tclas, tclas_next);
                subele_length = subele_length - (tclas->len + 2);
            }

            if(tfs_length > 255) {
                tfs_subrsp->status = IEEE80211_TFS_REASON_UNKNOWN;
		    }

            if(tfs_subrsp->status == IEEE80211_TFS_ACCEPT) {
			    TAILQ_INSERT_TAIL(&tfs_req->tfs_req_sub, tfs_subele, tsub_next);
            } else {
                wlan_wnm_delete_tfs_req(ni, tfs_req->tfs_id);
            }

    	    tfs_subrsp->tfsid = tfs_req->tfs_id;
            TAILQ_INSERT_TAIL(&tfs_rsp->tfs_rsp_sub, tfs_subrsp, tsub_next);
        }
        if(TAILQ_FIRST(&tfs_req->tfs_req_sub) != NULL) {
		    TAILQ_INSERT_TAIL(&tfs_req_head, tfs_req, tfs_req_next);
        }
        tfs_rsp->tfs_elemid  = IEEE80211_ELEMID_TFS_RESPONSE;
		TAILQ_INSERT_TAIL(&tfs_rsp_head, tfs_rsp, tfs_rsp_next);
    }

    wlan_wnm_free_tfsrsp(&ni->ni_wnm->tfsrsp->tfs_rsp_head);
    TAILQ_CONCAT(&ni->ni_wnm->tfsrsp->tfs_rsp_head,
                        &tfs_rsp_head, tfs_rsp_next);
    if(TAILQ_FIRST(&ni->ni_wnm->tfsrsp->tfs_rsp_head) != NULL) {
        wlan_wnm_free_tfs(&ni->ni_wnm->tfsreq->tfs_req_head);
        TAILQ_CONCAT(&ni->ni_wnm->tfsreq->tfs_req_head,
                        &tfs_req_head, tfs_req_next);
    } else {
        if(TAILQ_FIRST(&ni->ni_wnm->tfsreq->tfs_req_head) != NULL) {
            wlan_wnm_free_tfs(&ni->ni_wnm->tfsreq->tfs_req_head);
        }
    }

    if(TAILQ_FIRST(&ni->ni_wnm->tfsreq->tfs_req_head) == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, " TFS REQ NULL \n");
    }
    return frm;
}

u_int8_t *
ieee80211_add_tfsrsp_ie(wlan_if_t vap, wlan_node_t ni, u_int8_t *frm)
{
    struct ieee80211_tfs_response *tfs_rsp;
    u_int8_t *len;

    TAILQ_FOREACH(tfs_rsp, &ni->ni_wnm->tfsrsp->tfs_rsp_head, tfs_rsp_next)
    {
        struct ieee80211_tfs_subelement_rsp *tfs_subrsp;
        *frm++ = tfs_rsp->tfs_elemid;
        len = frm;
        *len = 0;
        frm++;
        TAILQ_FOREACH(tfs_subrsp, &tfs_rsp->tfs_rsp_sub, tsub_next)
        {
            *frm++ = tfs_subrsp->elem_id = 1;
            *frm++ = 2;
            *frm++ = tfs_subrsp->status;
            *frm++ = tfs_subrsp->tfsid;
            *len += 4;
        }
    }

    return frm;
}

int ieee80211_send_tfs_rsp(wlan_if_t vap, wlan_node_t ni)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }

    *frm++ = IEEE80211_ACTION_CAT_WNM;
    *frm++ = IEEE80211_ACTION_TFS_RESP;
    *frm++ = ni->ni_wnm->tfsreq->dialogtoken;

    frm = ieee80211_add_tfsrsp_ie(vap, ni, frm);

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));

    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}

int ieee80211_tfs_filter(struct ieee80211vap *vap, wbuf_t wbuf)
{
    struct ether_header *eh;
    u_int8_t *daddr;
    struct ieee80211_node *ni;
    int status = 1;

    if (wbuf) {
        eh = (struct ether_header *)wbuf_header(wbuf);
        daddr = eh->ether_dhost;

        ni = ieee80211_find_txnode(vap, daddr);

        if (ni == NULL) {
            return 1;
        }

        if ((eh->ether_type != htons(ETHERTYPE_PAE)) &&
            (vap->iv_opmode == IEEE80211_M_HOSTAP) &&
            (ni != vap->iv_bss)) {
                struct ip_header *ip;
                ip = (struct ip_header *) (wbuf_header(wbuf) + 
                                        sizeof(struct ether_header));
                status = __ieee80211_tfs_filter(ni, wbuf, eh->ether_type, ip);
        }
        ieee80211_free_node(ni);
    }
    return status;

}

int
__ieee80211_tfs_filter(struct ieee80211_node *ni, wbuf_t wbuf,
                    int ether_type, struct ip_header *ip)
{
    struct ieee80211_tfs_request *tfs_req, *temp_treq;
    int filter_len, i;
    unsigned char *bp;
    u_int32_t saddr, daddr;
    int tclas_count = 0;
    int tclas_match = 0;
    int filter_match = 1;
    int status = 1;
    int status_count = 0;
    int buf_len = 0;
    u_int8_t tfsid[10];
    u_int8_t tfsid_count = 0;
    struct ieee80211_tfs_subelement_req *tfs_subele;
    struct tclas_element *tclas_element;
    u_int8_t protocol;
    u_int8_t dscp;
    u_int8_t sport_match;
    u_int8_t dport_match;
    u_int16_t sport;
    u_int16_t dport;
    u_int16_t *tcpudphdr;
    u_int16_t offset;


    TAILQ_FOREACH_SAFE(tfs_req, &ni->ni_wnm->tfsreq->tfs_req_head, tfs_req_next,
                                                               temp_treq) {
        TAILQ_FOREACH(tfs_subele, &tfs_req->tfs_req_sub, tsub_next) {

            tclas_count = 0;
            tclas_match = 0;
            TAILQ_FOREACH(tclas_element, &tfs_subele->tclas_head, tclas_next) {

                tclas_count++;
                if(tclas_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) {
                    struct llc llc;
                    unsigned char *bp_llc;
                    int useBTEP;
                    struct ether_header *eh;

                    eh = (struct ether_header *)wbuf_header(wbuf);
                    useBTEP = (eh->ether_type == htons(ETHERTYPE_AARP)) ||
                        (eh->ether_type == htons(ETHERTYPE_IPX));
                    offset = tclas_element->tclas.type3.filter_offset;
                    filter_len = (tclas_element->len - 5) / 2;
                    buf_len = wbuf_get_pktlen(wbuf);
                    if(buf_len < (filter_len)) {
                        filter_match = 0;
                        break;
                    }
                    bp_llc =(u_int8_t *) &llc;
                    llc.llc_dsap = llc.llc_ssap = LLC_SNAP_LSAP;
                    llc.llc_control = LLC_UI;
                    if (useBTEP) {
                        llc.llc_snap.org_code[0] = BTEP_SNAP_ORGCODE_0; /* 0x0 */
                         llc.llc_snap.org_code[1] = BTEP_SNAP_ORGCODE_1; /* 0x0 */
                        llc.llc_snap.org_code[2] = BTEP_SNAP_ORGCODE_2; /* 0xf8 */
                    } else {
                        llc.llc_snap.org_code[0] = RFC1042_SNAP_ORGCODE_0; /* 0x0 */
                        llc.llc_snap.org_code[1] = RFC1042_SNAP_ORGCODE_1; /* 0x0 */
                        llc.llc_snap.org_code[2] = RFC1042_SNAP_ORGCODE_2; /* 0x0 */
                    }
                    llc.llc_snap.ether_type = eh->ether_type; 
                    bp = (u_int8_t *) (wbuf_header(wbuf) + sizeof(struct ether_header));
                    for(i = 0; i < filter_len; i++) {
                        if((offset + i) < sizeof(llc)) {
                            if((bp_llc[offset + i] &
                                tclas_element->tclas.type3.filter_mask[i]) !=
                                (tclas_element->tclas.type3.filter_value[i] &
                                tclas_element->tclas.type3.filter_mask[i])) {
                              filter_match = 0;
                              break;
                           }
                        } else if((bp[offset + i - sizeof(llc)] &
                                    tclas_element->tclas.type3.filter_mask[i]) !=
                                    (tclas_element->tclas.type3.filter_value[i] &
                                    tclas_element->tclas.type3.filter_mask[i])) {
                            filter_match = 0;
                            break;
                         }
                     }
                     if(filter_match) {
                        if(tfs_subele->tclasprocess.tclas_process == 1) {
                            tclas_match = tclas_count;
                            break;
                        }
  	                    tclas_match++;
                     } else {
                        filter_match = 1;
                     }
                } else if(tclas_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4) {
                    if ((ether_type == ETHERTYPE_IP)  &&
                        (tclas_element->tclas.type4.type4_v4.version ==
                                IEEE80211_WNM_TCLAS_CLAS4_VERSION_4)) {
                        adf_net_iphdr_t *hdr;
                        hdr = (adf_net_iphdr_t *)(ip);

                        sport = tclas_element->tclas.type4.type4_v4.src_port;
                        dport = tclas_element->tclas.type4.type4_v4.dst_port;
                        protocol = tclas_element->tclas.type4.type4_v4.protocol;
                        dscp = tclas_element->tclas.type4.type4_v4.dscp & (~INET_ECN_MASK);

                        memcpy(&saddr,
                             tclas_element->tclas.type4.type4_v4.src_ip, IEEE80211_IPV4_LEN);
                        memcpy(&daddr,
                                tclas_element->tclas.type4.type4_v4.dst_ip, IEEE80211_IPV4_LEN);


                        if ((hdr->ip_proto == protocol) &&
                            (saddr == (hdr->ip_saddr)) &&
                            (daddr == (hdr->ip_daddr)) &&
                            (dscp == (hdr->ip_tos & ~INET_ECN_MASK))) {
                            tcpudphdr = (u_int16_t*)((u_int32_t *)hdr + hdr->ip_hl);
                            sport_match = 1;
                            if (sport && (sport != (*tcpudphdr))) {
                                sport_match = 0;
                            }
                            dport_match = 1;
                            if (dport && (dport != (*(tcpudphdr + 1)))) {
                                dport_match = 0;
                            }
                            if (dport_match && sport_match) {
                                if(tfs_subele->tclasprocess.tclas_process == 1) {
                                    tclas_match = tclas_count;
                                    break;
                                } 
                                tclas_match++;
                            }
                        }
                    } else if ((ether_type == ETHERTYPE_IPV6) &&
                        (tclas_element->tclas.type4.type4_v6.version ==
                           IEEE80211_WNM_TCLAS_CLAS4_VERSION_6)) {
                        adf_net_ipv6hdr_t *ipv6;
                        unsigned long ver_pri_flowlabel;
                        unsigned long pri;
                        unsigned long *hdr;
                        u_int8_t *src_ip;
                        u_int8_t *dst_ip;
                        u_int8_t *flowlbl;
                        u_int8_t tos;
                        u_int8_t nexthdr;

                        hdr = (unsigned long*)(ip);
                        ipv6 = (adf_net_ipv6hdr_t *) hdr;
                        ver_pri_flowlabel = *hdr;
                        sport = tclas_element->tclas.type4.type4_v4.src_port;
                        dport = tclas_element->tclas.type4.type4_v4.dst_port;
                        flowlbl = tclas_element->tclas.type4.type4_v6.flow_label;
                        dscp = tclas_element->tclas.type4.type4_v6.dscp &
                                                          (~INET_ECN_MASK);
                        nexthdr = tclas_element->tclas.type4.type4_v6.next_header;
                        pri = ((ver_pri_flowlabel) & IPV6_PRIORITY_MASK) >> IPV6_PRIORITY_SHIFT;
                        tos = (pri & (~INET_ECN_MASK)) >> IP_PRI_SHIFT;
                        src_ip = tclas_element->tclas.type4.type4_v6.src_ip;
                        dst_ip = tclas_element->tclas.type4.type4_v6.dst_ip;
                        if ((ipv6->ipv6_nexthdr == nexthdr) &&
                            (OS_MEMCMP(ipv6->ipv6_flow_lbl, flowlbl, 3) == 0) &&
                            (OS_MEMCMP(src_ip, ipv6->ipv6_saddr.s6_addr, IEEE80211_IPV6_LEN) == 0) &&
                            (OS_MEMCMP(dst_ip, ipv6->ipv6_daddr.s6_addr, IEEE80211_IPV6_LEN) == 0) &&
                            (tos == dscp)) {
                            tcpudphdr = (u_int16_t *)(ipv6 + 1);
                            sport_match = 1;
                            if (sport && (sport != (*tcpudphdr))) {
                                sport_match = 0;
                            }
                            dport_match = 1;
                            if (dport && (dport != (*(tcpudphdr + 1)))) {
                                dport_match = 0;
                            }
                            if (dport_match && sport_match) {
                                if(tfs_subele->tclasprocess.tclas_process == 1) {
                                    tclas_match = tclas_count;
                                    break;
                                }  
                                tclas_match++;
                            }
                       }
                    } else {
                        tclas_match++;
                    }
                }
            }
            if(tclas_count == tclas_match) {
                status = 1;
                status_count++;
                break;
            } else {
                status = 0;
           }
        }
        if(status) {
            if(IEEE80211_TFS_NOTIFY & tfs_req->tfs_action_code) {
                tfsid[tfsid_count++] = tfs_req->tfs_id;
            }
            if(IEEE80211_TFS_DELETE & tfs_req->tfs_action_code) {
                wlan_wnm_delete_tfs_req(ni, tfs_req->tfs_id);
            }
        }
    }

    if((tfsid_count != 0) && (tfsid_count < 10)) {
        ieee80211_tfs_notify(ni, tfsid_count, tfsid);
    }

    if(status_count > 0) {
        status = 1;
    }
    return status;
}

int ieee80211_tfs_notify(struct ieee80211_node *ni,u_int8_t tfs_count,
                                                    u_int8_t  *tfsid)
{
     struct ieee80211vap *vap = ni->ni_vap;
     wbuf_t wbuf = NULL;
     u_int8_t *frm = NULL, i;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }


    *frm++ = IEEE80211_ACTION_CAT_WNM;
    *frm++ = IEEE80211_ACTION_TFS_NOTIFY;
    *frm++ = tfs_count;
    for(i = 0; i < tfs_count; i++) {
        *frm++ = tfsid[i];
    }
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}


int ieee80211_recv_tim_req(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len)
{
    u_int8_t *efrm;
    int status;

    efrm = frm + frm_len;
    frm += 2;

    ni->ni_wnm->timbcast_dialogtoken = *frm++;

    status = ieee80211_parse_timreq_ie(frm, frm + frm_len, ni);

    if (status == 0) {
        status = ieee80211_send_tim_response(ni->ni_vap, ni);
    }

    return status;
}

int ieee80211_parse_timreq_ie(u_int8_t *frm, u_int8_t *efrm, wlan_node_t ni)
{
    int status = -EINVAL;

    ni->ni_wnm->timbcast_status = IEEE80211_WNM_TIMREQUEST_DENIED;

    while (((frm + 1) < efrm) && (frm + frm[1] + 1) < efrm) {
        switch (frm[0]) {
            case IEEE80211_ELEMID_TIM_BCAST_REQUEST:
                ni->ni_wnm->timbcast_interval = frm[2];
                status = 0;
            break;
        }
        frm += frm[1] + 2;
    }

    return status;
}

int wlan_calculate_timbcast_interval(wlan_if_t vap)
{
    struct ieee80211_node_table *nt;
    struct ieee80211_node *ni = NULL, *next = NULL;
    struct ieee80211com *ic;
    rwlock_state_t lock_state;
    int found = 0;

    ic = vap->iv_ic;
    nt = &ic->ic_sta;

    OS_RWLOCK_READ_LOCK(&nt->nt_nodelock, &lock_state);
    TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
        if(vap == ni->ni_vap) {
            if(IEEE80211_ADDR_EQ(ni->ni_macaddr, vap->iv_myaddr) &&
                 !ni->ni_associd) {
                continue;
            }
            if (vap->wnm->wnm_timbcast_interval == ni->ni_wnm->timbcast_interval) {
                found = 1;
                break;
            }
        }
    }
    if (found == 0) {
        vap->wnm->wnm_timbcast_interval = 0;
    }

    OS_RWLOCK_READ_UNLOCK(&nt->nt_nodelock, &lock_state);

    return vap->wnm->wnm_timbcast_interval;
}

int
ieee80211_send_tim_response(wlan_if_t vap, wlan_node_t ni)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }

    *frm++ = IEEE80211_ACTION_CAT_WNM;
    *frm++ = IEEE80211_ACTION_TIM_RESP;
    *frm++ = ni->ni_wnm->timbcast_dialogtoken;

    frm = ieee80211_add_timresp_ie(vap, ni, frm);

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));

    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}

u_int8_t *ieee80211_add_timresp_ie(wlan_if_t vap, wlan_node_t ni,
                                               u_int8_t *frm)
{
    int status = IEEE80211_WNM_TIMREQUEST_DENIED;
    int tim_interval = 0;
    u_int8_t *len;
    int i;
    u_int32_t basicrate = 0;

    *frm++ = IEEE80211_ELEMID_TIM_BCAST_RESPONSE;
    len = frm;
    frm++;

    if (ni->ni_wnm->timbcast_interval) {
        vap->wnm->wnm_timbcast_interval =
                            wlan_calculate_timbcast_interval(vap);
        if (vap->wnm->wnm_timbcast_interval) {
            if(vap->wnm->wnm_timbcast_interval >= ni->ni_wnm->timbcast_interval) {
                if ((vap->wnm->wnm_timbcast_interval % ni->ni_wnm->timbcast_interval)
                                                                    == 0) {
                    status = IEEE80211_WNM_TIMREQUEST_ACCEPT;
                    vap->wnm->wnm_timbcast_interval = ni->ni_wnm->timbcast_interval;
                    tim_interval = vap->wnm->wnm_timbcast_interval;
                }
            } else {
                if ((ni->ni_wnm->timbcast_interval % vap->wnm->wnm_timbcast_interval)
                                                                    == 0) {
                    status = IEEE80211_WNM_TIMREQUEST_OVERRIDDEN;
                    tim_interval = vap->wnm->wnm_timbcast_interval;
                }
            }
        } else {
            tim_interval = vap->wnm->wnm_timbcast_interval =
                                ni->ni_wnm->timbcast_interval;
            status = IEEE80211_WNM_TIMREQUEST_ACCEPT;
        }
    } else {
        if (vap->wnm->wnm_timbcast_interval) {
            tim_interval = ni->ni_wnm->timbcast_interval =
					vap->wnm->wnm_timbcast_interval =
                            wlan_calculate_timbcast_interval(vap);
            status = IEEE80211_WNM_TIMREQUEST_ACCEPT;
        }
    }

    if (status == IEEE80211_WNM_TIMREQUEST_DENIED) {
        *frm++ = status;
        *len = 1;
    } else {
        *frm++ = status;
        if (ni->ni_wnm->timbcast_interval) {
            *frm++ = tim_interval;
            *(u_int32_t *)frm = htole32(vap->wnm->wnm_timbcast_offset);
            frm += 4;
            if (vap->wnm->wnm_timbcast_enable & IEEE80211_WNM_TIM_HIGHRATE_ENABLE) {
                *(u_int16_t *)frm = htole16(vap->wnm->wnm_timbcast_highrate);
            } else {
                memset(frm, 0, sizeof(u_int16_t));
            }
            frm += 2;
            if (vap->wnm->wnm_timbcast_enable &
                    IEEE80211_WNM_TIM_LOWRATE_ENABLE) {
                for (i=0; i < ni->ni_rates.rs_nrates; i++) {
                    if ((basicrate == 0) ||
                    ((basicrate & IEEE80211_RATE_VAL) >
                        (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL))) {
                        basicrate = ni->ni_rates.rs_rates[i];
                    }
                }
                vap->wnm->wnm_timbcast_lowrate = basicrate & IEEE80211_RATE_VAL;
                *(u_int16_t *)frm = htole16(vap->wnm->wnm_timbcast_lowrate);
            } else {
                memset(frm, 0, sizeof(u_int16_t));
            }
            frm += 2;
            *len = 10;
        } else {
            *len = 1;
        }
        ni->ni_wnm->timbcast_status = IEEE80211_WNM_TIMREQUEST_ACCEPT;
    }

    return frm;
}


#endif /* UMAC_SUPPORT_WNM */



