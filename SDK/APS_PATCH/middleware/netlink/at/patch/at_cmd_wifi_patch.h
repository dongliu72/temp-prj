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

/**
 * @file at_cmd_wifi_patch.h
 * @author Michael Liao
 * @date 20 Mar 2018
 * @brief File containing declaration of at_cmd_wifi api & definition of structure for reference.
 *
 */

#ifndef __AT_CMD_WIFI_PATCH_H__
#define __AT_CMD_WIFI_PATCH_H__

#define AT_WIFI_SHOW_ECN_BIT        0x00000001U
#define AT_WIFI_SHOW_SSID_BIT       0x00000002U
#define AT_WIFI_SHOW_RSSI_BIT       0x00000004U
#define AT_WIFI_SHOW_MAC_BIT        0x00000008U
#define AT_WIFI_SHOW_CHANNEL_BIT    0x00000010U

void _at_cmd_wifi_func_init(void);

#endif

