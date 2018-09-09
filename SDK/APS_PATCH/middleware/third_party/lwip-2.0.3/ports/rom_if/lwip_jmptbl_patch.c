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

#include "lwip/opt.h"
#include "sys_os_config_patch.h"

/* user api related */
extern void lwip_load_interface_socket_patch(void);

/* ipv4 related */

/* ipv6 related */


/* common */


/* network interface */

/* network config */
extern void lwip_load_interface_network_config_patch(void);

/* porting layer */
extern void lwip_load_interface_wlannetif_patch(void);

/* application && cli */
extern void lwip_load_interface_lwip_helper_patch(void);
extern void lwip_load_interface_cli_patch(void);

/* tcpip if */
extern void lwip_load_interface_tcpip_if_patch(void);

void lwip_module_interface_init_patch(void)
{
    lwip_load_interface_socket_patch();
    lwip_load_interface_wlannetif_patch();
    lwip_load_interface_network_config_patch();
    lwip_load_interface_cli_patch();
    lwip_load_interface_tcpip_if_patch();

    #ifdef OS_TASK_INFO_DUMP
    {
        extern void lwip_load_interface_sys_arch_freertos_patch(void);

        lwip_load_interface_sys_arch_freertos_patch();
    }
    #endif
    return;
}
