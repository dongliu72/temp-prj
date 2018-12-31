/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#ifndef __BLEWIFI_BLE_API_H__
#define __BLEWIFI_BLE_API_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


void BleWifi_Ble_Init(void);
void BleWifi_Ble_StartAdvertising(void);
void BleWifi_Ble_StopAdvertising(void);


#ifdef __cplusplus
}
#endif

#endif  // end of __BLEWIFI_BLE_API_H__
