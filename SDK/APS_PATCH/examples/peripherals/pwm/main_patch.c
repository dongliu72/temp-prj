/******************************************************************************
*  Copyright 2018, Opulinks Technology Ltd. 
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd.  (C) 2018
******************************************************************************/

/******************************************************************************
*  Filename:
*  ---------
*  main_patch.c
*
*  Project:
*  --------
*  NL1000 Project - the main patch implement file
*
*  Description:
*  ------------
*  This implement file is include the main patch function and api.
*
*  Author:
*  -------
*  SH SDK 
*
******************************************************************************/
/***********************
Head Block of The File
***********************/
// Sec 0: Comment block of the file
/******************************************************************************
*  Test code brief
*  These examples show how to configure PWM settings and use PWM driver APIs.
*
*  App_Pin_InitConfig() function complete PWM port pin assignment and parameter setting.
*  All PWM port pin definition and parameters are defined in global structure OPL1000_periph
*  PWM port number is defined by OPL1000_periph.pwm_num
*  Each PWM port pin assignment and parameters are defined by OPL1000_periph.pwm[i]  
*  After implementing App_Pin_InitConfig, waveform will be generated on defined PWM port.  
*  Note: For multiple PWM port, their clock source shall be same, either 22MHz or 32kHz. 
******************************************************************************/


// Sec 1: Include File
#include <stdio.h>
#include <string.h>
#include "sys_init_patch.h"
#include "cmsis_os.h"
#include "sys_os_config.h"
#include "Hal_pinmux_pwm.h"

// Sec 2: Constant Definitions, Imported Symbols, miscellaneous


// the event type of message


/********************************************
Declaration of data structure
********************************************/
// Sec 3: structure, uniou, enum, linked list


/********************************************
Declaration of Global Variables & Functions
********************************************/
// Sec 4: declaration of global variable


// Sec 5: declaration of global function prototype
typedef void (*T_Main_AppInit_fp)(void);
extern T_Main_AppInit_fp Main_AppInit;


/***************************************************
Declaration of static Global Variables & Functions
***************************************************/
// Sec 6: declaration of static global variable
static osThreadId g_tAppThread;

// Sec 7: declaration of static function prototype
static void __Patch_EntryPoint(void) __attribute__((section(".ARM.__at_0x00420000")));
static void __Patch_EntryPoint(void) __attribute__((used));
void Main_AppInit_patch(void);
static void Main_AppThread(void *argu);
static void pwm_test(void);


/***********
C Functions
***********/
// Sec 8: C Functions

/*************************************************************************
* FUNCTION:
*   __Patch_EntryPoint
*
* DESCRIPTION:
*   the entry point of SW patch
*
* PARAMETERS
*   none
*
* RETURNS
*   none
*
*************************************************************************/
static void __Patch_EntryPoint(void)
{
    // don't remove this code
    SysInit_EntryPoint();
    
    // application init
    Main_AppInit = Main_AppInit_patch;
}

/*************************************************************************
* FUNCTION:
*   App_Pin_InitConfig
*
* DESCRIPTION:
*   Initialize  the pin assignment
*
* PARAMETERS
*   none
*
* RETURNS
*   none
*
*************************************************************************/
void App_Pin_InitConfig(void)
{
    uint8_t pwm_num = OPL1000_periph.pwm_num;
	  uint8_t i,pwm_index_mask = 0, pwm_idx; 
	
	  if(pwm_num > 0) 
		{	
			Hal_Pinmux_Pwm_Init();
			
			// Disable all PWM output 
			Hal_Pinmux_Pwm_Disable(HAL_PWM_IDX_ALL);
			
			for (i=0; i<pwm_num;i++)
			{
				pwm_idx = Hal_PinMux_Get_Index(OPL1000_periph.pwm[i].pin);
				pwm_index_mask =  pwm_index_mask | pwm_idx;				
				// pwm[0] corresponding to PWM4 - IO19, complex mode config   
				Hal_Pinmux_Pwm_Config(&OPL1000_periph.pwm[i]);
			}
			
			Hal_Pinmux_Pwm_Enable(pwm_index_mask);
	  }	
}


/*************************************************************************
* FUNCTION:
*   Main_AppInit_patch
*
* DESCRIPTION:
*   the initial of application
*
* PARAMETERS
*   none
*
* RETURNS
*   none
*
*************************************************************************/
void Main_AppInit_patch(void)
{
    // init the pin assignment
    App_Pin_InitConfig();
    
    printf("configuration is finished \r\n");
	
    pwm_test();
}

/*************************************************************************
* FUNCTION:
*   Main_AppThread
*
* DESCRIPTION:
*   the application thread 2
*
* PARAMETERS
*   1. argu     : [In] the input argument
*
* RETURNS
*   none
*
*************************************************************************/
static void Main_AppThread(void *argu)
{
    while (1)
    {
        osDelay(1500);      // delay 500 ms
		printf("PWM Running \r\n");
    }
}

/*************************************************************************
* FUNCTION:
*   pwm test 
*
* DESCRIPTION:
*   This is a blank function, just create a thread for debug aim 
*
* PARAMETERS
*   none
*
* RETURNS
*   none
*
*************************************************************************/
static void pwm_test(void)
{
    osThreadDef_t tThreadDef;
    
    // create the thread for AppThread
    tThreadDef.name = "App";
    tThreadDef.pthread = Main_AppThread;
    tThreadDef.tpriority = OS_TASK_PRIORITY_APP;        // osPriorityNormal
    tThreadDef.instances = 0;                           // reserved, it is no used
    tThreadDef.stacksize = OS_TASK_STACK_SIZE_APP;      // (512), unit: 4-byte, the size is 512*4 bytes
    g_tAppThread = osThreadCreate(&tThreadDef, NULL);
    if (g_tAppThread == NULL)
    {
        printf("To create the thread for AppThread is fail.\n");
    }
    
}
