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

#ifndef __AT_CMD_COMMOM_PATCH_H__
#define __AT_CMD_COMMOM_PATCH_H__
/**
 * @file at_cmd_common_patch.h
 * @author Michael Liao
 * @date 20 Mar 2018
 * @brief File containing declaration of at_cmd_common api & definition of structure for reference.
 *
 */

/**
 * @brief AT_VER
 *
 */
#define AT_VER "1.0"

enum {
    AT_STATE_IDLE,              // idle
    AT_STATE_PROCESSING,        // at mode, in busy
    AT_STATE_SENDING_RECV,      // UART1 ISR push receive data to cmd line and switch to this state then notify AT task
    AT_STATE_SENDING_DATA,      // sending data mode
    AT_STATE_TRANSMIT           // transmit transparently
};

typedef enum {
    AT_RESULT_CODE_OK           = 0x00,
    AT_RESULT_CODE_ERROR        = 0x01,
    AT_RESULT_CODE_FAIL         = 0x02,
    AT_RESULT_CODE_SEND_OK      = 0x03,
    AT_RESULT_CODE_SEND_FAIL    = 0x04,
    AT_RESULT_CODE_IGNORE       = 0x05,
    AT_RESULT_CODE_PROCESS_DONE = 0x06, // not response string
    AT_RESULT_CODE_MAX
} at_result_code_string_index_t;

#define at_printf(fmt,...)          printf(fmt,##__VA_ARGS__)
#define at_strlen(s)                strlen((char*)(s))
#define at_strcpy(dest,src)         strcpy((char*)(dest),(char*)(src))
#define at_strncpy(dest,src,n)      strncpy((char*)(dest),(char*)(src),n)
#define at_strcmp(s1,s2)            strcmp((char*)(s1),(char*)(s2))
#define at_strncmp(s1,s2,n)         strncmp((char*)(s1),(char*)(s2),n)
#define at_strstr(s1,s2)            strstr((char*)(s1),(char*)(s2))
#define at_sprintf(s,...)           sprintf((char*)(s), ##__VA_ARGS__)
#define at_snprintf(s,size,...)     snprintf((char*)(s),size, ##__VA_ARGS__)
#define at_memset(dest,x,n)         memset(dest,x,n)
#define at_memcpy(dest,src,n)       memcpy(dest,src,n)
#define at_memcmp(s1,s2,n)          memcmp(s1,s2,n)
#define at_malloc(size)             malloc(size)
#define at_free(ptr)                free(ptr)


/**
 * @brief Function Pointer Type for API _uart1_rx_int_at_data_receive_ble
 *
 */
typedef void (*_uart1_rx_int_at_data_receive_ble_fp_t)(uint32_t u32Data);

/**
 * @brief Function Pointer Type for API _uart1_rx_int_at_data_receive_tcpip
 *
 */
typedef void (*_uart1_rx_int_at_data_receive_tcpip_fp_t)(uint32_t u32Data);

/**
 * @brief Function Pointer Type for API set_echo_on
 *
 */
typedef void (*set_echo_on_fp_t)(int on);

/**
 * @brief Extern Function _uart1_rx_int_at_data_receive_ble
 *
 */
extern _uart1_rx_int_at_data_receive_ble_fp_t _uart1_rx_int_at_data_receive_ble;

/**
 * @brief Extern Function uart1_rx_int_at_data_receive_tcpip
 *
 */
extern _uart1_rx_int_at_data_receive_tcpip_fp_t _uart1_rx_int_at_data_receive_tcpip;

/**
 * @brief Extern Function set_echo_on
 *
 */
extern set_echo_on_fp_t set_echo_on;

int _at_cmd_buf_to_argc_argv(char *pbuf, int *argc, char *argv[]);
char *at_cmd_param_trim(char *sParam);

void at_cmd_common_func_init_patch(void);
void at_uart1_write_buffer(char *buf, int len);
void at_uart1_printf(char *str);
void at_response_result(uint8_t result_code);

int wpas_get_state(void);
int wpas_get_assoc_freq(void);

#endif //__AT_CMD_COMMOM_PATCH_H__

