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


// Sec 1: Include File
#include <stdio.h>
#include <string.h>
#include "sys_init_patch.h"
#include "cmsis_os.h"
#include "sys_os_config.h"
#include "msg_patch.h"
#include "hal_dbg_uart_patch.h"
#include "wpa2_station_app.h"

// Sec 2: Constant Definitions, Imported Symbols, miscellaneous
// the number of elements in the message queue

#define  USER_APP1_LOG              0
#define  USER_APP2_LOG              1
#define  ROM_MODULES_NUMBER         8 


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
static osThreadId g_tAppThread_1;
static osThreadId g_tAppThread_2;

osMessageQId g_tAppMessageQ;
osPoolId g_tAppMemPoolId;

// defined in wpa2_station_app.c
extern osMessageQId g_tWifiAppMessageQ;
extern osPoolId g_tWifiAppMemPoolId;

bool g_AP_connect_result = false;
bool g_AP_connect_complete = false;
uint32_t g_execution_count = 0;

// Sec 7: declaration of static function prototype
static void __Patch_EntryPoint(void) __attribute__((section(".ARM.__at_0x00420000")));
static void __Patch_EntryPoint(void) __attribute__((used));
void Main_AppInit_patch(void);
void User_Demo(void);

static void Main_AppThread_1(void *argu);
static void Main_AppThread_2(void *argu);

extern T_TracerTaskInfo g_taTracerIntTaskInfoBody[];
extern T_TracerTaskInfo g_taTracerExtTaskInfoBody[];

/***********
C Functions
***********/

// Sec 8: C Functions

void Internal_Module_Log_Set(char* module_name, bool on_off_set)
{
	  uint8_t log_level_set,i,module_index; 	
	
    if(on_off_set == true) 
        log_level_set = LOG_ALL_LEVEL;
    else
        log_level_set = LOG_NONE_LEVEL;	
    
		for (i = 0; i < TRACER_INT_TASK_NUM_MAX; i++) 
		{
			if (strcmp(module_name,g_taTracerIntTaskInfoBody[i].baName) == 0)
			{
				module_index = i;
				break;
			}
		}
		if(module_index < TRACER_INT_TASK_NUM_MAX) 
		{
		    g_taTracerIntTaskInfoBody[module_index].bLevel = log_level_set;
    } 
} 

void App_Log_Config(uint8_t log_idx, char* app_name , uint8_t level_set)
{
    //user log
    g_taTracerExtTaskInfoBody[log_idx].bLevel = level_set;
    strcpy(g_taTracerExtTaskInfoBody[log_idx].baName,app_name);
}

void Internal_Module_Log_Config(bool on_off_set)
{
	  Internal_Module_Log_Set("wifi_mac",true);			
    Internal_Module_Log_Set("controller_task",true);
    Internal_Module_Log_Set("event_loop",true);	
}

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
    // Enable APS Uart log information output 
    Hal_DbgUart_RxIntEn(1);   
    
    tracer_def_level_set (TRACER_TASK_TYPE_INTERNAL, LOG_ALL_LEVEL);
    
    // Log example executin, create two thread 
    User_Demo();
    
    // Start WIFI app execution, scan AP and try to connect WIFI_SSID
    WifiAppInit();
    
}

/*************************************************************************
* FUNCTION:
*   Thread_demo
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
void User_Demo()
{
    osThreadDef_t tThreadDef;
    osMessageQDef_t tMessageDef;
    osPoolDef_t tMemPoolDef;

    // create the thread for AppThread_1
    tThreadDef.name = "demo_app1";
    tThreadDef.pthread = Main_AppThread_1;
    tThreadDef.tpriority = OS_TASK_PRIORITY_APP;        // osPriorityNormal
    tThreadDef.instances = 0;                           // reserved, it is no used
    tThreadDef.stacksize = OS_TASK_STACK_SIZE_APP;      // (512), unit: 4-byte, the size is 512*4 bytes
    g_tAppThread_1 = osThreadCreate(&tThreadDef, NULL);
    if (g_tAppThread_1 == NULL)
    {
        printf("To create the thread for AppThread_1 is fail.\n");
    }
    
    // create the thread for AppThread_2
    tThreadDef.name = "demo_app2";
    tThreadDef.pthread = Main_AppThread_2;
    tThreadDef.tpriority = OS_TASK_PRIORITY_APP;        // osPriorityNormal
    tThreadDef.instances = 0;                           // reserved, it is no used
    tThreadDef.stacksize = OS_TASK_STACK_SIZE_APP;      // (512), unit: 4-byte, the size is 512*4 bytes
    g_tAppThread_2 = osThreadCreate(&tThreadDef, NULL);
    if (g_tAppThread_2 == NULL)
    {
        printf("To create the thread for AppThread_2 is fail.\n");
    }
    else
    {
        printf("To create the thread for AppThread_2 successful \r\n");
    }
    
    // create the message queue for AppMessageQ
    tMessageDef.queue_sz = APP_MESSAGE_Q_SIZE;          // number of elements in the queue
    tMessageDef.item_sz = sizeof(S_MessageQ);           // size of an item
    tMessageDef.pool = NULL;                            // reserved, it is no used
    g_tAppMessageQ = osMessageCreate(&tMessageDef, g_tAppThread_2);
    if (g_tAppMessageQ == NULL)
    {
        printf("To create the message queue for AppMessageQ is fail.\n");
    }
    else
    {
        printf("To create the message queue for AppMessageQ successful \r\n");
    }
    
    // create the memory pool for AppMessageQ
    tMemPoolDef.pool_sz = APP_MESSAGE_Q_SIZE;           // number of items (elements) in the pool
    tMemPoolDef.item_sz = sizeof(S_MessageQ);           // size of an item
    tMemPoolDef.pool = NULL;                            // reserved, it is no used
    g_tAppMemPoolId = osPoolCreate(&tMemPoolDef);
    if (g_tAppMemPoolId == NULL)
    {
        printf("To create the memory pool for g_tAppMemPoolId is fail.\n");
    }    
    else
    {
        printf("To create the memory pool for g_tAppMemPoolId successful \r\n");
    }
}


/*************************************************************************
* FUNCTION:
*   Main_AppThread_1
*
* DESCRIPTION:
*   the application thread 1
*
* PARAMETERS
*   1. argu     : [In] the input argument
*
* RETURNS
*   none
*
*************************************************************************/
static void Main_AppThread_1(void *argu)
{
    uint32_t ulCount = 0; 
    while (1)
    {
        printf("demo_app1: %d\r\n",ulCount);
        osDelay(2000);      // delay 1000ms (1sec)
        ulCount++;
    }
}

/*************************************************************************
* FUNCTION:
*   Main_AppThread_2
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
static void Main_AppThread_2(void *argu)
{
    uint32_t ulCount = 0;
    char AP_ssid_name[32] = WIFI_SSID;
    S_MessageQ tMsg;
    osEvent tEvent;
    S_MessageQ *ptMsgPool;

    while (1)
    {        
        printf("demo_app2: %d, g_execution_count:%d \r\n",ulCount,g_execution_count);
        
        // receive the message from AppMessageQ
        tEvent = osMessageGet(g_tAppMessageQ, osWaitForever);
        if (tEvent.status != osEventMessage)
        {
            printf("To receive the message from AppMessageQ is fail.\n");
            continue;
        }    
        // get the content of message
        ptMsgPool = (S_MessageQ *)tEvent.value.p;

        // means received message from user_app thread 
        if(ptMsgPool->ulEventId == UAER_WIFI_APP_CONNECT_COMPLETE)
        //if(g_AP_connect_complete) 
        {
            g_AP_connect_result = ptMsgPool->bConnectResult;

            // check last time AP connection result 
            if(g_AP_connect_result == true)
            {
                printf("demo_app2: %s connection is success. \r\n",AP_ssid_name);
            }
            else
            {
                printf("demo_app2: %s connection is failed. \r\n",AP_ssid_name);
            }

            // indicate WIFI AP connection procedure is finished. 
            if ((g_execution_count%2) == 1)
            {
                printf("demo_app2: Enable ROM internal module log output \r\n");
                // enable ROM internal module log output 
                Internal_Module_Log_Config(true);

                printf("demo_app2: Enable user demo_app1 log output \r\n");
                // enable user demo_app1 log output                 
                App_Log_Config(USER_APP1_LOG,"demo_app1",LOG_ALL_LEVEL);                
            }
            else 
            {
                printf("demo_app2: Disable ROM internal module log output \r\n");
                // disable ROM internal module log output
                Internal_Module_Log_Config(false);

                printf("demo_app2: Disable user demo_app1 log output \r\n");
                // disable user demo_app1 log output 
                App_Log_Config(USER_APP1_LOG,"demo_app1",LOG_NONE_LEVEL);                
            }

            // Send message to WIFI user_app thread, let it begin another Scan->connect procedure 

            tMsg.ulEventId = UAER_WIFI_APP_RE_CONNECT;
            wifi_AppMessageQSend(&tMsg, g_tWifiAppMessageQ,g_tWifiAppMemPoolId);  
            
            g_execution_count++;
        }
        osDelay(2000);      // delay 1000ms (1sec)
        ulCount++; 
    }
}

