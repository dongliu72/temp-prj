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

/******************************************************************************
*  Filename:
*  ---------
*  hal_wdt_patch.h
*
*  Project:
*  --------
*  NL1000_A0 series
*
*  Description:
*  ------------
*  This include file defines the proto-types of Watchdog functions
*
*  Author:
*  -------
*  Luke Liao
******************************************************************************/

#ifndef __HAL_WDT_PATCH_H__
#define __HAL_WDT_PATCH_H__

/***********************
Head Block of The File
***********************/
// Sec 0: Comment block of the file

// Sec 1: Include File 
#include <stdint.h>

// Sec 2: Constant Definitions, Imported Symbols, miscellaneous

/********************************************
Declaration of data structure
********************************************/
// Sec 3: structure, uniou, enum, linked list...


/********************************************
Declaration of Global Variables & Functions
********************************************/
// Sec 4: declaration of global  variable

// Sec 5: declaration of global function prototype

/***************************************************
Declaration of static Global Variables &  Functions
***************************************************/
// Sec 6: declaration of static global  variable

// Sec 7: declaration of static function prototype

/***********
C Functions
***********/
// Sec 8: C Functions
extern void Hal_Wdt_Init_patch(uint32_t u32Ticks);
extern void Hal_Wdt_InitForInt_patch(uint32_t u32Ticks);
extern void Hal_Wdt_Start_patch(void);
extern void Hal_Wdt_Stop_patch(void);
extern void Hal_Wdt_Feed_patch(uint32_t u32Ticks);
extern void Hal_Wdt_Clear_patch(void);

#endif
