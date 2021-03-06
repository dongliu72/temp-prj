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

#ifndef _AT_CMD_FUNC_PATCH_H_
#define _AT_CMD_FUNC_PATCH_H_


#include "at_cmd.h"


#define AT_CMD_EXT_TBL_LST


typedef struct S_CmdTblLst
{
    _at_command_t *taCmdTbl;
    struct S_CmdTblLst *ptNext;
} T_CmdTblLst;


void at_func_init_patch(void);
int at_cmd_process_func_register(void *ptr);

#endif /* _AT_CMD_FUNC_PATCH_H_ */
