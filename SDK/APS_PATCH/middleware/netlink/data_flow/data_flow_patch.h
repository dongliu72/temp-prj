/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#ifndef __DATA_FLOW_PATCH_H__
#define __DATA_FLOW_PATCH_H__


#include "ipc.h"


extern T_IpcHandleFp ipc_m0_msg_handle;


int ipc_rf_at_process_M0ToM3_patch(void* pdata);
int ipc_wifi_aps_rx_handle_patch(uint32_t dwType, uint32_t dwIdx, void *pBuf, uint32_t dwBufSize);
int ipc_m0_msg_handle_patch(uint32_t dwType, uint32_t dwIdx, void *pBuf, uint32_t dwBufSize);


#endif //#ifndef __DATA_FLOW_PATCH_H__

