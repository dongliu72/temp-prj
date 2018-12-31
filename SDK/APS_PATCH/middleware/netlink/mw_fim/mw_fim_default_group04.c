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

/******************************************************************************
*  Filename:
*  ---------
*  mw_fim_default_group04.c
*
*  Project:
*  --------
*  NL1000 Project - the Flash Item Management (FIM) implement file
*
*  Description:
*  ------------
*  This implement file is include the Flash Item Management (FIM) function and api.
*
*  Author:
*  -------
*  Jeff Kuo
*
******************************************************************************/
/***********************
Head Block of The File
***********************/
// Sec 0: Comment block of the file


// Sec 1: Include File
#include "mw_fim_default_group04.h"


// Sec 2: Constant Definitions, Imported Symbols, miscellaneous


/********************************************
Declaration of data structure
********************************************/
// Sec 3: structure, uniou, enum, linked list


/********************************************
Declaration of Global Variables & Functions
********************************************/
// Sec 4: declaration of global variable
const le_cfg_t g_tMwFimDefaultLeCfg = 
{
    .hci_revision = FIM_HCI_Version,
    .manufacturer_name = FIM_Manufacturer_Name, 
    .lmp_pal_subversion = FIM_LMP_PAL_Subversion, 
    .hci_version = FIM_HCI_Version,
    .lmp_pal_version = FIM_LMP_PAL_Version,
    .bd_addr = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11}
};
// the address buffer of LE config
uint32_t g_u32aMwFimAddrLeCfg[MW_FIM_LE_CFG_NUM]; 

// the information table of group 04
const T_MwFimFileInfo g_taMwFimGroupTable04[] =
{
    {MW_FIM_IDX_LE_CFG, MW_FIM_LE_CFG_NUM, MW_FIM_IDX_LE_CFG_SIZE, (uint8_t*)&g_tMwFimDefaultLeCfg, g_u32aMwFimAddrLeCfg},
    // the end, don't modify and remove it
    {0xFFFFFFFF,            0x00,              0x00,               NULL,                            NULL}
};


// Sec 5: declaration of global function prototype


/***************************************************
Declaration of static Global Variables & Functions
***************************************************/
// Sec 6: declaration of static global variable


// Sec 7: declaration of static function prototype


/***********
C Functions
***********/
// Sec 8: C Functions
