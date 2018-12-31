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
 * @file at_cmd_tcpip_patch.c
 * @author Michael Liao
 * @date 20 Mar 2018
 * @brief File supports the TCP/IP module AT Commands.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "os.h"
#include "cmsis_os.h"
#include "at_cmd.h"
#include "at_cmd_tcpip.h"
#include "lwip_helper.h"
#include "at_cmd_common.h"
#include "at_cmd_tcpip_patch.h"
#include "at_cmd_patch.h"
#include "at_cmd_data_process.h"
#include "at_cmd_common_patch.h"

#include "sys_os_config.h"

#include "wpa_cli.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/netif.h"
#include "lwip/apps/sntp_client.h"
#include "wlannetif_patch.h"
#include "ping_cmd.h"
#include "wifi_api.h"
#include "network_config.h"
#include "at_cmd_app.h"
#include "controller_wifi_com_patch.h"


/******************************************************
 *                      Macros
 ******************************************************/
//#define AT_CMD_TCPIP_DBG

#ifdef AT_CMD_TCPIP_DBG
    #define AT_LOG                      printf
#else
    #define AT_LOG(...)
#endif

#define AT_LOGI(fmt,arg...)             printf(("[AT]: "fmt"\r\n"), ##arg)


/******************************************************
 *                    Constants
 ******************************************************/
#define AT_CMD_TCPIP_PING_IDX           16

#define IP_STR_BUF_LEN                  16
#define IP_STR_DELIM                    "."

#define AT_CMD_PING_SERV_PORT_STR       "20" // not used
#define AT_CMD_PING_RECV_TIMEOUT        1000 // ms
#define AT_CMD_PING_PERIOD              10 // ms
#define AT_CMD_PING_NUM                 3
#define AT_CMD_PING_SIZE                32
#define AT_CMD_PING_WAIT_RSP            10 // ms
#define AT_CMD_PING_WAIT_RSP_MAX_CNT    (((AT_CMD_PING_RECV_TIMEOUT + AT_CMD_PING_PERIOD) * (AT_CMD_PING_NUM + 1)) / AT_CMD_PING_WAIT_RSP)

#define NO_SOCKET -1

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/
typedef struct
{
    u32_t count;
    u32_t size;
    u32_t recv_timout;
    u32_t ping_period;
    ping_request_result_t callback;
    u8_t addr[16];
} ping_arg_t;
/******************************************************
 *               Static Function Declarations
 ******************************************************/


/******************************************************
 *               Variable Definitions
 ******************************************************/
extern ping_arg_t g_ping_arg;

ping_result_t g_tPingResult = {0};
volatile uint8_t g_u8PingCmdDone = 0;

extern uint8_t *pDataLine;
extern uint8_t  at_data_line[];

volatile at_state_type_t mdState = AT_STA_DISCONNECT;//AT_STA_INACTIVE;
volatile bool at_ip_mode = false;
volatile bool at_ipMux = false;
volatile bool ipd_info_enable = false;

uint8_t sending_id = 0;
uint32_t at_send_len = 0;

at_socket_t atcmd_socket[AT_LINK_MAX_NUM];
int at_netconn_max_num = 5;

volatile uint16_t server_timeover = 180;
int tcp_server_socket = -1;
TimerHandle_t server_timeout_timer;

char *at_sntp_server[SNTP_MAX_SERVERS];

osThreadId   at_tx_task_id = NULL;
osThreadId   at_tcp_server_task_handle = NULL;
osMessageQId at_tx_task_queue_id = NULL;
osPoolId     at_tx_task_pool_id = NULL;

osPoolDef (at_tx_task_pool, AT_DATA_TASK_QUEUE_SIZE, at_event_msg_t);

extern u8 gsta_cfg_mac[MAC_ADDR_LEN];


/******************************************************
 *               Function Definitions
 ******************************************************/

void at_server_timeout_handler(void);

int net_adapter_get_ip_info(net_adapter_if_t tcpip_if, net_adapter_ip_info_t *ip_info)
{
    struct netif *p_netif = NULL;
    lwip_tcpip_config_t tcpip_config = {{0}, {0}, {0}};

    if (tcpip_if >= TCPIP_ADAPTER_IF_MAX || ip_info == NULL) {
        return -1;
    }

    p_netif = netif_find("st1");

    if (p_netif != NULL && netif_is_up(p_netif)) {
        ip4_addr_set(&ip_info->ip, ip_2_ip4(&p_netif->ip_addr));
        ip4_addr_set(&ip_info->netmask, ip_2_ip4(&p_netif->netmask));
        ip4_addr_set(&ip_info->gw, ip_2_ip4(&p_netif->gw));

        return 0;
    }

    if (0 != tcpip_config_init(&tcpip_config)) {
        LWIP_DEBUGF(NETIF_DEBUG, ("tcpip config init fail \n"));
        return -1;
    }

    ip4_addr_copy(ip_info->ip, tcpip_config.sta_ip);
    ip4_addr_copy(ip_info->gw, tcpip_config.sta_gw);
    ip4_addr_copy(ip_info->netmask, tcpip_config.sta_mask);

    return 0;
}

/* return 1 if string contain only digits, else return 0 */
int valid_digit(char *ip_str)
{
    while (*ip_str) {
        if (*ip_str >= '0' && *ip_str <= '9')
            ++ip_str;
        else
            return 0;
    }
    return 1;
}

int at_cmd_is_valid_ip(char *sIpStr)
{
    int num, dots = 0;
    char *ptr;
    char ip_str[IP_STR_BUF_LEN] = {0};

    if((sIpStr == NULL) || (strlen(sIpStr) >= IP_STR_BUF_LEN))
        return 0;

    strcpy(ip_str, sIpStr);

    ptr = strtok(ip_str, IP_STR_DELIM);

    if (ptr == NULL)
        return 0;

    while (ptr) {

        /* after parsing string, it must contain only digits */
        if (!valid_digit(ptr))
            return 0;

        num = atoi(ptr);

        /* check for valid IP */
        if (num >= 0 && num <= 255) {
            /* parse remaining string */
            ptr = strtok(NULL, IP_STR_DELIM);
            if (ptr != NULL)
                ++dots;
        } else
            return 0;
    }

    /* valid IP string must contain 3 dots */
    if (dots != 3)
        return 0;
    return 1;
}

void at_cmd_ping_callback(ping_result_t *ptResult)
{
    if(ptResult)
    {
        memcpy((void *)&g_tPingResult, ptResult, sizeof(g_tPingResult));
    }

    g_u8PingCmdDone = 1;
    return;
}

int at_cmd_is_tcpip_ready(void)
{
    int iRet = 0;
    struct netif *ptNetIf = NULL;

    ptNetIf = netif_find(WLAN_IF_NAME);

    if((!ptNetIf) || (!netif_is_up(ptNetIf)))
    {
        goto done;
    }

    iRet = 1;

done:
    return iRet;
}

int at_cmd_get_param_as_str(char *param, char **result)
{
    char *pStr = NULL;
    char *tmp_ptr = NULL;
    uint32_t para_len = 0;

    pStr = param;
    if (pStr == NULL) {
        return -1;
    }

    if (*pStr == '\0') {
        *result = NULL;
        return 0;
    }

    para_len = strlen((char *)pStr);
    if ((pStr[0] != '"') || (pStr[para_len - 1] != '"')) {
        return -1;
    }

    if ((pStr[0] == '"') && (pStr[1] == '"') && (para_len == 2)) {
        *result = NULL;
        return 0;
    }

    pStr[para_len - 1] = '\0';
    pStr++;
    *result = pStr;
    tmp_ptr = pStr;
    while (*pStr != '\0') {
        if (*pStr == '\\') {
            if (pStr[1] == '\0') {
                return -1;
            }
            pStr++;
        } else if (*pStr == '"') {
            return -1;
        }
        // Now get character
        *tmp_ptr++ = *pStr++;
    }
    *tmp_ptr = '\0';
    return 0;
}

int at_get_socket_error_code(int sockfd)
{
    int result;
    u32_t optlen = sizeof(int);
    int err = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &result, &optlen);
    if (err == -1) {
        AT_LOGI("getsockopt failed: %d", err);
        return -1;
    }
    return result;
}

int at_show_socket_error_reason(const char *str, int sockfd)
{
    int err = at_get_socket_error_code(sockfd);

    if (err != 0) {
        AT_LOGI("%s error, error code: %d, reason: %s", str, err, lwip_strerr(err));
    }

    return err;
}

void at_update_link_count(int8_t count)
{
    static int cnt = 0;
    int tmp = 0;
    tmp = cnt + count;

    if ((cnt > 0) && (tmp <= 0)) {
        cnt = 0;
        if (mdState == AT_STA_LINKED) {
            mdState = AT_STA_UNLINKED;
        }
    } else if ((cnt == 0) && (tmp > 0)) {
        cnt = tmp;
        mdState = AT_STA_LINKED;
    }
}

void at_link_init(uint32_t conn_max)
{
    int i = 0;
    if (conn_max != 0) {
        if (conn_max > AT_LINK_MAX_NUM)
            at_netconn_max_num = AT_LINK_MAX_NUM;
        else
            at_netconn_max_num = conn_max;
    }
    //at_socket_mutex = xSemaphoreCreateMutex();

    at_memset(atcmd_socket, 0x0, at_netconn_max_num * sizeof(at_socket_t));
    for (i = 0; i < at_netconn_max_num; i++) {
        atcmd_socket[i].sock = -1;
        atcmd_socket[i].link_id = i;
        //vSemaphoreCreateBinary(pLink[i].sema);
        //xSemaphoreTake(pLink[i].sema, 0);
    }
}

at_socket_t *at_link_alloc(void)
{
    sys_prot_t prot = sys_arch_protect();

    for (int i = 0; i < AT_LINK_MAX_NUM; i++) {
        if (!atcmd_socket[i].link_en) {
            at_socket_t *s = &atcmd_socket[i];
            memset(s, 0, sizeof *s);
            s->link_en = true;
            s->link_id = i;
            sys_arch_unprotect(prot);
            return s;
        }
    }

    sys_arch_unprotect(prot);
    return NULL;
}

at_socket_t *at_link_alloc_id(int id)
{
    sys_prot_t prot = sys_arch_protect();

    if (!atcmd_socket[id].link_en) {
        at_socket_t *s = &atcmd_socket[id];
        memset(s, 0, sizeof *s);
        s->link_en = 1;
        s->link_id = id;
        sys_arch_unprotect(prot);
        return s;
    }

    sys_arch_unprotect(prot);
    return NULL;
}

at_socket_t *at_link_get_id(int id)
{
    sys_prot_t prot = sys_arch_protect();
    at_socket_t *s = &atcmd_socket[id];
    sys_arch_unprotect(prot);
    return s;
}

static int at_resolve_dns(const char *host, struct sockaddr_in *ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    he = gethostbyname(host);
    if (he == NULL) {
        return -1;
    }
    addr_list = (struct in_addr **)he->h_addr_list;
    if (addr_list[0] == NULL) {
        return -1;
    }
    ip->sin_family = AF_INET;
    memcpy(&ip->sin_addr, addr_list[0], sizeof(ip->sin_addr));
    return 0;
}

int at_create_tcp_server(int port, int timeout_ms)
{
    struct sockaddr_in sa;
    uint32_t optval = 0;
    socklen_t len = sizeof(optval);

    if (tcp_server_socket >= 0)
    {
        msg_print_uart1("no change\r\n");
        return -1;
    }

    tcp_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_server_socket < 0)
    {
        AT_LOGI("sock err:%d\r\n",tcp_server_socket);
        return -1;
    }

    optval = 1;
    len = sizeof(optval);
    /* set listen socket to allow multiple connections */
    setsockopt(tcp_server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, len);
    setsockopt(tcp_server_socket, SOL_SOCKET, SO_REUSEPORT, &optval, len);

    at_memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htons(INADDR_ANY);
    if (bind(tcp_server_socket, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
        msg_print_uart1("bind fail\r\n");
        at_show_socket_error_reason("server bind", tcp_server_socket);
        at_socket_server_cleanup_task(tcp_server_socket);
        return -1;
    }

    /* Set the listen back log */
    /* Set maximum of pending connections for the listen socket*/
    if (listen(tcp_server_socket,at_netconn_max_num) < 0)
    {
        msg_print_uart1("listen fail\r\n");
        at_show_socket_error_reason("server listen", tcp_server_socket);
        at_socket_server_cleanup_task(tcp_server_socket);
        return -1;
    }
    if (at_socket_server_create_task(tcp_server_socket) != TRUE)
    {
        msg_print_uart1("try fail\r\n");
        at_socket_server_cleanup_task(tcp_server_socket);
        return -1;
    }

    return 0;
}

int at_create_tcp_client(at_socket_t *link, const char *host, int port, int timeout_ms)
{
    struct sockaddr_in remote_addr;
    int optval = 0;
    int ret;
    //struct timeval tv;

    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_len = sizeof(remote_addr);
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = lwip_htons(port);

    //if stream_host is not ip address, resolve it AF_INET,servername,&serveraddr.sin_addr
    if (inet_pton(AF_INET, host, &remote_addr.sin_addr) != 1) {
        if (at_resolve_dns(host, &remote_addr) < 0) {
            AT_LOGI("DNS lookup failed\r\n");
            msg_print_uart1("DNS Fail\r\n");
            return -1;
        }
    }

    inet_addr_to_ip4addr(ip_2_ip4(&link->remote_ip), &remote_addr.sin_addr);
    //link->remote_port = port;

    /* Create the socket */
    link->sock = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (link->sock < 0) {
        at_show_socket_error_reason("TCP client create", link->sock);
        return -1;
    }

#if 0
    tv.tv_sec = 10;
    if (timeout_ms) {
        tv.tv_sec = timeout_ms;
    }
    tv.tv_usec = 0;
    setsockopt(link->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    setsockopt(link->sock,SOL_SOCKET, SO_REUSEADDR ,&optval, sizeof(optval));
    setsockopt(link->sock,SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    if (link->keep_alive > 0) {
        optval = 1;
        setsockopt(link->sock,SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
        optval = link->keep_alive;
        setsockopt(link->sock,IPPROTO_TCP, TCP_KEEPIDLE,&optval, sizeof(optval));
        optval = 1;
        setsockopt(link->sock,IPPROTO_TCP, TCP_KEEPINTVL,&optval, sizeof(optval));
        optval = 3;
        setsockopt(link->sock,IPPROTO_TCP, TCP_KEEPCNT,&optval, sizeof(optval));
    }

    AT_LOGI("sock=%d, connecting to server IP:%s, Port:%d...",
             link->sock, ipaddr_ntoa(&link->remote_ip), port);

    /* Connect */
    ret = lwip_connect(link->sock, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
    if (ret < 0) {
        at_show_socket_error_reason("TCP client connect", link->sock);
        lwip_close(link->sock);
        link->sock = -1;
        return -1;
    }

    link->link_state = AT_LINK_CONNECTED;

    return ret;
}

int at_create_udp_client(at_socket_t *link, const char *host, int remote_port, int local_port, int timeout_ms)
{
    struct sockaddr_in addr;
    int s;
    int ret = 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = lwip_htons(remote_port);

    //if stream_host is not ip address, resolve it AF_INET,servername,&serveraddr.sin_addr
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        if (at_resolve_dns(host, &addr) < 0) {
            AT_LOGI("DNS lookup failed\r\n");
            return -1;
        }
    }

    inet_addr_to_ip4addr(ip_2_ip4(&link->remote_ip), &addr.sin_addr);
    //link->remote_port = remote_port;
    //link->local_port = local_port;

    /* Create the socket */
    link->sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (link->sock < 0) {
        at_show_socket_error_reason("UDP create client", s);
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = lwip_htons(local_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (lwip_bind(link->sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        at_show_socket_error_reason("UDP bind local", s);
        return -1;
    }

    link->link_state = AT_LINK_CONNECTED;

    AT_LOGI("[sock=%d], connecting to server IP:%s, Port:%d...",
             link->sock, ipaddr_ntoa(&link->remote_ip), remote_port);

    return ret;
}

int at_close_client(at_socket_t *link)
{
    int ret = -1;
    if (link->sock >= 0) {
        lwip_shutdown(link->sock, SHUT_RDWR);
        ret = lwip_close(link->sock);
        link->sock = -1;
    }

    link->link_en = 0;
    link->link_state = AT_LINK_DISCONNECTED;
    link->link_type = AT_LINK_TYPE_INVALID;
    return ret;
}

int at_ip_send_data(uint8_t *pdata, int send_len)
{
    at_socket_t *link = at_link_get_id(sending_id);
    int actual_send = 0;
    char buf[64];

    at_sprintf(buf, "\r\nRecv %d bytes\r\n", send_len);
    msg_print_uart1(buf);

    printf("%s\r\n", pdata);

    if(link->sock < 0)
    {
        link->link_state = AT_LINK_DISCONNECTED;
        AT_LOGI("link_state is AT_LINK_DISCONNECTED\r\n");
        return -1;
    }

    if (link->link_type == AT_LINK_TYPE_TCP) {
        actual_send = lwip_write(link->sock, pdata, send_len);
        if (actual_send <= 0) {
            at_show_socket_error_reason("client send", link->sock);
            return -1;
        } else {
            AT_LOGI("id:%d,Len:%d,dp:%p\r\n", sending_id, send_len, pdata);
        }
    } else if (link->link_type == AT_LINK_TYPE_UDP) {

        struct sockaddr_in addr;
        memset(&addr, 0x0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(link->remote_port);
        inet_addr_from_ip4addr(&addr.sin_addr, ip_2_ip4(&link->remote_ip));

        AT_LOGI("sock=%d, udp send data to server IP:%s, Port:%d...",
             link->sock, ipaddr_ntoa(&link->remote_ip), link->remote_port);


        actual_send = sendto(link->sock, pdata, send_len, 0, (struct sockaddr *)&addr, sizeof(addr));
        if (actual_send <= 0) {
            at_show_socket_error_reason("client send", link->sock);
            return -1;
        } else {
            AT_LOGI("id:%d,Len:%d,dp:%p\r\n", sending_id, send_len, pdata);
        }
    }

    return 0;
}

int at_socket_read(at_socket_t *link, char *buf, int min_len, int max_len, int *p_read_len)
{
    int ret = 0;
    size_t readLen = 0;

    while (readLen < max_len) {
        buf[readLen] = '\0';
        if (readLen < min_len) {
            ret = recv(link->sock, buf + readLen, min_len - readLen, 0);
            AT_LOGI("recv [blocking] return:%d", ret);
        } else {
            ret = recv(link->sock, buf + readLen, max_len - readLen, 0);
            AT_LOGI("recv [not blocking] return:%d", ret);
            if (ret == -1 /*&& errno == EWOULDBLOCK*/) {
                AT_LOGI("recv [not blocking] EWOULDBLOCK");
                break;
            }
        }

        if (ret > 0) {
            readLen += ret;
        } else if (ret == 0) {
            break;
        } else {
            AT_LOGI("Connection error (recv returned %d)", ret);
            *p_read_len = readLen;
            return -1;
        }
    }

    AT_LOGI("Read %d bytes", readLen);
    *p_read_len = readLen;
    buf[readLen] = '\0';

    return 0;
}

int at_socket_read_set_timeout(at_socket_t *plink, int timeout_ms)
{
    fd_set readset;

    if (plink == NULL){
        return -1;
    }
    FD_ZERO(&readset);
    FD_SET(plink->sock, &readset);
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    return select(plink->sock + 1, &readset, NULL, NULL, &timeout); //0: timeout, -1:select error
}

int at_socket_write_set_timeout(at_socket_t *plink, int timeout_ms)
{
    fd_set writeset;

    if (plink == NULL){
        return -1;
    }
    FD_ZERO(&writeset);
    FD_SET(plink->sock, &writeset);
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    return select(plink->sock + 1, NULL, &writeset, NULL, &timeout); //0: timeout, -1:select error
}

int build_fd_sets(at_socket_t *server, fd_set *read_fds, fd_set *write_fds, fd_set *except_fds)
{
    FD_ZERO(read_fds);
    FD_SET(server->sock, read_fds);

    FD_ZERO(except_fds);
    FD_SET(server->sock, except_fds);

    return 0;
}

void receive_from_peer(at_socket_t *plink)
{
    struct sockaddr_in sa;
    socklen_t socklen = sizeof(struct sockaddr_in);
    char *rcv_buf = NULL;
    int len = 0;
    int32_t sock = -1;
    char temp[40];

    if (plink == NULL){
        return;
    }

    if ((rcv_buf = (char *)malloc(AT_DATA_RX_BUFSIZE * sizeof(char))) == NULL) {
        AT_LOGI("rx buffer alloc fail\r\n");
        return;
    }

    memset(&sa, 0x0, socklen);

    if (plink->link_type == AT_LINK_TYPE_TCP) {
        len = lwip_recv(plink->sock, rcv_buf, AT_DATA_RX_BUFSIZE, 0);
    } else if (plink->link_type == AT_LINK_TYPE_UDP) {
        len = lwip_recvfrom(plink->sock, rcv_buf, AT_DATA_RX_BUFSIZE, 0, (struct sockaddr*)&sa, &socklen);
    } else {
        /* TODO:AT_LINK_TYPE_SSL*/
        len = 0;
    }


    if (len > 0)
    {
        if (at_ip_mode == true) {

        } else {
            char* data = (char*)malloc(40);
            uint32_t header_len = 0;

            if (data) {
                if (plink->link_type == AT_LINK_TYPE_UDP) {
                    if (plink->change_mode == 2) {
                        inet_addr_to_ip4addr(ip_2_ip4(&plink->remote_ip), &sa.sin_addr);
                        plink->remote_port = htons(sa.sin_port);
                    } else if (plink->change_mode == 1) {
                        inet_addr_to_ip4addr(ip_2_ip4(&plink->remote_ip), &sa.sin_addr);
                        plink->remote_port = htons(sa.sin_port);
                        plink->change_mode = 0;
                    }
                }

                if (ipd_info_enable == true) {
                    if (at_ipMux) {
                        header_len = sprintf(data, "\r\n+IPD,%d,%d,"IPSTR",%d:",plink->link_id, len,
                                IP2STR(&(plink->remote_ip)), (plink->remote_port));
                    }
                    else {
                        header_len = sprintf(data, "\r\n+IPD,%d,"IPSTR",%d:", len,
                                IP2STR(&(plink->remote_ip)), (plink->remote_port));

                    }
                } else {
                    if (at_ipMux) {
                        header_len = sprintf(data,"\r\n+IPD,%d,%d:",plink->link_id, len);
                    } else {
                        header_len = sprintf(data,"\r\n+IPD,%d:",len);
                    }
                }

                // Send +IPD info
                at_uart1_write_buffer(data, header_len);
                // Send data
                at_uart1_write_buffer(rcv_buf, len);
                free(data);
                data = NULL;

                if (plink->link_state != AT_LINK_WAIT_SENDING) {
                    plink->link_state = AT_LINK_CONNECTED;
                }
                if ((plink->sock >= 0) && (plink->terminal_type == AT_REMOTE_CLIENT)) {
                    plink->server_timeout = 0;
                }
            } else {
                printf("alloc fail\r\n");
            }
        }
    }
    else
    {
        //len = 0, Connection close
        //len < 0, error
        if(plink->sock >= 0) {
            if ((at_ip_mode != TRUE) || (plink->link_state != AT_LINK_TRANSMIT_SEND)) {
                if (at_ipMux == TRUE) {
                    sprintf(temp,"%d,CLOSED\r\n", plink->link_id);
                } else {
                    sprintf(temp,"CLOSED\r\n");
                }
                msg_print_uart1(temp);

                if (plink->link_state != AT_LINK_DISCONNECTING) {
                    sock = plink->sock;
                    plink->sock = -1;
                    plink->link_type = AT_LINK_TYPE_INVALID;
                    plink->link_en = 0;
                    if (sock >= 0) {
                        close(sock);
                    }
                    plink->link_state = AT_LINK_DISCONNECTED;
            	}
            }
        }
    }

    free(rcv_buf);
}

int at_process_socket_nonblock(at_socket_t *plink)
{
    fd_set read_fds;
    fd_set write_fds;
    fd_set except_fds;
    int opt = 1;
    int ret;
    int maxfd = plink->sock;

    /* Set socket to be nonblocking */
    ret = lwip_ioctl(plink->sock, FIONBIO, &opt);
    LWIP_ASSERT("ret == 0", ret == 0);

    while (1) {
        // Select() updates fd_set's, so we need to build fd_set's before each select()call.
        build_fd_sets(plink, &read_fds, &write_fds, &except_fds);

        int activity = select(maxfd + 1, &read_fds, NULL, &except_fds, NULL);

        switch (activity) {
            case -1:
                printf("select() err\r\n");
                return -1;
            case 0:
                // select time out
                printf("select() returns 0.\r\n");
                return 0;
            default:
                if (FD_ISSET(plink->sock, &read_fds)) {
                    //if (receive_from_peer(plink->sock, &handle_received_message) != 0)
                    receive_from_peer(plink);
                    printf("ready to read.\r\n");
                }
                if (FD_ISSET(plink->sock, &except_fds)) {
                    printf("except_fds for server.\r\n");
                    return -1;
                }
        }
    }

    //return 0;
}

int handle_new_connection(int listen_sock)
{
    int loop;
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_len = sizeof(client_addr);
    char client_ipv4_str[16];
    char tmp[64];

    int new_client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
    if (new_client_sock < 0) {
        printf("accept() err");
        return -1;
    }

    inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4_str, 16);

    printf("Incoming connection from %s:%d.\n", client_ipv4_str, client_addr.sin_port);

    at_socket_t *plink;
    for (loop = 0; loop < AT_LINK_MAX_NUM; loop++)
    {
        plink = at_link_get_id(loop);
        if (plink->sock < 0) {
            break;
        }
    }

    if (loop < AT_LINK_MAX_NUM)
    {
        plink->link_state = AT_LINK_CONNECTED;
        inet_addr_to_ip4addr(ip_2_ip4(&plink->remote_ip), &client_addr.sin_addr);
        plink->remote_port = ntohs(client_addr.sin_port);
        plink->repeat_time = 0;
        plink->sock = new_client_sock;
        plink->terminal_type = AT_REMOTE_CLIENT;
        plink->server_timeout = 0;
        //at_socket_client_create_task(plink);
        at_sprintf(tmp,"%d,CONNECT\r\n",loop);
        msg_print_uart1(tmp);
        return 0;
    }
    else
    {
        at_sprintf(tmp,"connect reach max\r\n");
        msg_print_uart1(tmp);
        printf("There is too much connections. Close new connection %s:%d.\n", client_ipv4_str, client_addr.sin_port);
        close(new_client_sock);
        return -1;
    }


    //close(new_client_sock);
    //return -1;
}


void at_process_listen_socket(void *arg)
{
    int i;
    fd_set read_fds;

    int high_sock = tcp_server_socket;

    printf("Waiting for incoming connections.\n");

    while (1) {
        //build_fd_sets(&read_fds, &write_fds, &except_fds);

        FD_ZERO(&read_fds);
        FD_SET(tcp_server_socket, &read_fds);

        for (i = 0; i < AT_LINK_MAX_NUM; ++i) {
            at_socket_t *link = at_link_get_id(i);
            if (link->sock != NO_SOCKET) {
                FD_SET(link->sock, &read_fds);
            }
        }

        high_sock = tcp_server_socket;
        for (i = 0; i < AT_LINK_MAX_NUM; ++i) {
            at_socket_t *link = at_link_get_id(i);;
            if (link->sock > high_sock) {
                high_sock = link->sock;
            }
        }

        int activity = select(high_sock + 1, &read_fds, NULL, NULL, NULL);

        switch (activity) {
            case -1:
                printf("select() err\r\n");
                goto task_exit;
            case 0:
                // you should never get here
                printf("select() time out\n");
            default:
                /* All set fds should be checked. */
                if (FD_ISSET(tcp_server_socket, &read_fds))
                {
                    handle_new_connection(tcp_server_socket);
                }

                for (i = 0; i < AT_LINK_MAX_NUM; ++i) {
                    at_socket_t *link = at_link_get_id(i);
                    if (link->sock != NO_SOCKET && FD_ISSET(link->sock, &read_fds)) {
                        receive_from_peer(link);
                        //  close_client_connection(&connection_list[i]);
                        //  continue;
                        //}
                    }
                }
        }
    }

task_exit:

	if (tcp_server_socket >= 0) {
        close(tcp_server_socket);
        tcp_server_socket = -1;
    }

	at_tcp_server_task_handle = NULL;

    vTaskDelete(NULL);
}

void at_process_recv_socket(at_socket_t *plink)
{
    struct sockaddr_in sa;
    socklen_t socklen = sizeof(struct sockaddr_in);
    char *rcv_buf = NULL;
    int len = 0;
    int32_t sock = -1;
    char temp[40];

    if (plink == NULL){
        return;
    }

    rcv_buf = plink->recv_buf;

    //if (at_socket_read_set_timeout(plink, 6000) <= 0) {
    //    AT_LOGI("read timeout\r\n");
    //    return;
    //}

    //if ((rcv_buf = (char *)malloc(AT_DATA_RX_BUFSIZE * sizeof(char))) == NULL) {
    //    AT_LOGI("rx buffer alloc fail\r\n");
    //    return;
    //}

    memset(&sa, 0x0, socklen);

    if (plink->link_type == AT_LINK_TYPE_TCP) {
        len = lwip_recv(plink->sock, rcv_buf, AT_DATA_RX_BUFSIZE, 0);
    } else if (plink->link_type == AT_LINK_TYPE_UDP) {
        len = lwip_recvfrom(plink->sock, rcv_buf, AT_DATA_RX_BUFSIZE, 0, (struct sockaddr*)&sa, &socklen);
    } else {
        /* TODO:AT_LINK_TYPE_SSL*/
        len = 0;
    }


    if (len > 0)
    {
        if (at_ip_mode == true) {

        } else {
            char* data = (char*)malloc(40);
            uint32_t header_len = 0;

            if (data) {
                if (plink->link_type == AT_LINK_TYPE_UDP) {
                    if (plink->change_mode == 2) {
                        inet_addr_to_ip4addr(ip_2_ip4(&plink->remote_ip), &sa.sin_addr);
                        plink->remote_port = htons(sa.sin_port);
                    } else if (plink->change_mode == 1) {
                        inet_addr_to_ip4addr(ip_2_ip4(&plink->remote_ip), &sa.sin_addr);
                        plink->remote_port = htons(sa.sin_port);
                        plink->change_mode = 0;
                    }
                }

                if (ipd_info_enable == true) {
                    if (at_ipMux) {
                        header_len = sprintf(data, "\r\n+IPD,%d,%d,"IPSTR",%d:",plink->link_id, len,
                                IP2STR(&(plink->remote_ip)), plink->remote_port);
                    }
                    else {
                        header_len = sprintf(data, "\r\n+IPD,%d,"IPSTR",%d:", len,
                                IP2STR(&(plink->remote_ip)), plink->remote_port);

                    }
                } else {
                    if (at_ipMux) {
                        header_len = sprintf(data,"\r\n+IPD,%d,%d:",plink->link_id, len);
                    } else {
                        header_len = sprintf(data,"\r\n+IPD,%d:",len);
                    }
                }

                // Send +IPD info
                at_uart1_write_buffer(data, header_len);
                // Send data
                at_uart1_write_buffer(rcv_buf, len);
                free(data);
                data = NULL;

                if (plink->link_state != AT_LINK_WAIT_SENDING) {
                    plink->link_state = AT_LINK_CONNECTED;
                }
                if ((plink->sock >= 0) && (plink->terminal_type == AT_REMOTE_CLIENT)) {
                    plink->server_timeout = 0;
                }
            } else {
                printf("alloc fail\r\n");
            }
        }
    }
    else
    {
        //len = 0, Connection close
        //len < 0, error
        if(plink->sock >= 0) {
            if ((at_ip_mode != TRUE) || (plink->link_state != AT_LINK_TRANSMIT_SEND)) {
                if (at_ipMux == TRUE) {
                    sprintf(temp,"%d,CLOSED\r\n", plink->link_id);
                } else {
                    sprintf(temp,"CLOSED\r\n");
                }
                msg_print_uart1(temp);

                if (plink->link_state != AT_LINK_DISCONNECTING) {
                    sock = plink->sock;
                    plink->sock = -1;
                    plink->link_type = AT_LINK_TYPE_INVALID;
                    plink->link_en = 0;
                    if (sock >= 0) {
                        close(sock);
                    }
                    plink->link_state = AT_LINK_DISCONNECTED;
            	}
            }
        }
    }

    free(rcv_buf);
}

static int at_data_task_post(at_event_msg_t *msg, uint32_t timeout)
{
    if (msg == NULL) {
        return -1;
    }

    if (osMessagePut(at_tx_task_queue_id, (uint32_t)msg, timeout) != osOK) {
        printf("at data tx task_post failed\r\n");
        return -1;
    }

    return 0;
}

int at_data_task_send(at_event_msg_t *msg)
{
    int iRet = -1;
    at_event_msg_t *lmsg = NULL;

    if (msg == NULL) {
        goto done;
    }

    if (at_tx_task_queue_id == NULL) {
        goto done;
    }

    lmsg = (at_event_msg_t *)osPoolCAlloc(at_tx_task_pool_id);

    if(lmsg == NULL)
    {
        goto done;
    }

    memcpy(lmsg, msg, sizeof(at_event_msg_t));
    lmsg->param = NULL;

    if ((msg->param) && (msg->length)) {
        lmsg->param = (void *)malloc(msg->length);
        if(lmsg->param != NULL) {
            memcpy((void *)lmsg->param, (void *)msg->param, msg->length);
        } else {
            printf("FATAL: at data tx task loop send message allocate fail \r\n");
            goto done;
        }
    }

    iRet = at_data_task_post(lmsg, osWaitForever);

done:
    if(iRet)
    {
        if(lmsg)
        {
            if(lmsg->param)
            {
                free(lmsg->param);
            }

            osPoolFree(at_tx_task_pool_id, lmsg);
        }
    }

    return iRet;
}

void at_data_tx_task(void *arg)
{
    osEvent event;
    at_event_msg_t *pMsg;

    while(1)
    {
        event = osMessageGet(at_tx_task_queue_id, osWaitForever);
        if (event.status == osEventMessage)
        {
            pMsg = (at_event_msg_t*) event.value.p;
            switch(pMsg->event)
            {
                case AT_DATA_TX_EVENT:
                    AT_LOGI("at data event: %02X\r\n", pMsg->event);
                    if (at_ip_send_data(pMsg->param, pMsg->length) < 0)
                        msg_print_uart1("\r\nSEND FAIL\r\n");
                    else
                        msg_print_uart1("\r\nSEND OK\r\n");
                    data_process_unlock();
                    break;
                case AT_DATA_TIMER_EVENT:
                    at_server_timeout_handler();
                    break;
                default:
                    AT_LOGI("FATAL: unknow at event: %02X\r\n", pMsg->event);
                    break;
            }
            if(pMsg->param != NULL)
                free(pMsg->param);
            osPoolFree(at_tx_task_pool_id, pMsg);
        }
    }
}

void at_socket_process_task(void *arg)
{
    at_socket_t* plink = (at_socket_t*)arg;
    bool link_count_flag = false;

    if (plink == NULL) {
        plink->task_handle = NULL;
        vTaskDelete(NULL);
    }

    //xSemaphoreTake(plink->sema,0);
    if ((plink->link_type == AT_LINK_TYPE_TCP) || (plink->link_type == AT_LINK_TYPE_SSL)) {
        link_count_flag = true;
        at_update_link_count(1);
    }

    for(;;)
    {
        if ((plink->link_state == AT_LINK_DISCONNECTED) || (plink->link_state == AT_LINK_DISCONNECTING)) {
            break;
        }

        at_process_recv_socket(plink);
    }

    if (link_count_flag) {
        at_update_link_count(-1);
    }

    AT_LOGI("socket recv delete\r\n");

    if(plink->link_state == AT_LINK_DISCONNECTING) {
    	//xSemaphoreGive(plink->sema);
        plink->link_state = AT_LINK_DISCONNECTED;
    }

    plink->task_handle = NULL;
    vTaskDelete(NULL);
}
void at_create_tcpip_tx_task(void)
{
    osThreadDef_t task_def;
    osMessageQDef_t queue_def;

    /* Create task */
    task_def.name = "at_tx_data";
    task_def.stacksize = 512;
    task_def.tpriority = OS_TASK_PRIORITY_APP;
    task_def.pthread = at_data_tx_task;
    at_tx_task_id = osThreadCreate(&task_def, (void*)NULL);
    if(at_tx_task_id == NULL)
    {
        AT_LOGI("at_data Tx Task create fail \r\n");
    }
    else
    {
        AT_LOGI("at_data Tx Task create successful \r\n");
    }

    /* Create memory pool */
    at_tx_task_pool_id = osPoolCreate (osPool(at_tx_task_pool));
    if (!at_tx_task_pool_id)
    {
        printf("at_data TX Task Pool create Fail \r\n");
    }

    /* Create message queue*/
    queue_def.item_sz = sizeof(at_event_msg_t);
    queue_def.queue_sz = AT_DATA_TASK_QUEUE_SIZE;
    at_tx_task_queue_id = osMessageCreate(&queue_def, at_tx_task_id);
    if(at_tx_task_queue_id == NULL)
    {
        printf("at_data Tx create queue fail \r\n");
    }
}

void at_create_tcpip_data_task(void)
{
    at_create_tcpip_tx_task();
}

int at_socket_client_create_task(at_socket_t *link)
{
    char task_name[10];
    osThreadDef_t task_def;

    at_sprintf(task_name,"socket%d",link->link_id);

    /* Create task */
    task_def.name = task_name;
    task_def.stacksize = 512;
    task_def.tpriority = OS_TASK_PRIORITY_APP;
    task_def.pthread = at_socket_process_task;
    link->task_handle = osThreadCreate(&task_def, (void*)link);
    if(link->task_handle == NULL)
    {
        AT_LOGI("at_data Rx Task create fail \r\n");
        return -1;
    }
    else
    {
        AT_LOGI("at_data Rx Task create successful \r\n");
    }

    return 0;
}

int at_socket_client_cleanup_task(at_socket_t* plink)
{
    int ret = -1;

    if (plink == NULL) {
        return ret;
    }
    //AT_SOCKET_LOCK();
    ret = at_close_client(plink);
    if (plink->task_handle) {
        vTaskDelete(plink->task_handle);
        plink->task_handle = NULL;
    }

    if (plink->recv_buf) {
        free(plink->recv_buf);
        plink->recv_buf = NULL;
    }
    //AT_SOCKET_UNLOCK();
    return ret;
}

void at_socket_server_listen_task(void *arg)
{
    int loop = 0;
    int listen_sock = -1;
    struct sockaddr_in sa;
    socklen_t len = sizeof(sa);
    int client_sock = -1;
    char tmp[16];
    at_socket_t *plink;

    if (arg == NULL) {
        goto task_exit;
    }

    listen_sock= *(uint32_t*)arg;
    if (listen_sock < 0) {
        goto task_exit;
    }

    for(;;)
    {
        client_sock = accept(listen_sock, (struct sockaddr *)&sa, &len);
        if (client_sock < 0)
            break;

        //AT_SOCKET_LOCK();
        for (loop = 0; loop < AT_LINK_MAX_NUM; loop++) {
                plink = at_link_get_id(loop);
                if (plink->sock < 0) {
                    break;
                }
        }

        if (loop < AT_LINK_MAX_NUM)
        {
            if ((plink->recv_buf = (char *)malloc(AT_DATA_RX_BUFSIZE * sizeof(char))) == NULL) {
                AT_LOGI("rx buffer alloc fail\r\n");
                goto task_exit;
            }
            plink->link_state = AT_LINK_CONNECTED;
            inet_addr_to_ip4addr(ip_2_ip4(&plink->remote_ip), &sa.sin_addr);
            plink->remote_port = ntohs(sa.sin_port);
            plink->repeat_time = 0;
            plink->sock = client_sock;
            plink->terminal_type = AT_REMOTE_CLIENT;
            plink->server_timeout = 0;
            plink->link_type = AT_LINK_TYPE_TCP;//SSL
            at_sprintf(tmp,"%d,CONNECT\r\n",loop);
            msg_print_uart1(tmp);

            at_socket_client_create_task(plink);
            //AT_SOCKET_UNLOCK();
        }
        else
        {
            //AT_SOCKET_UNLOCK();
            at_sprintf(tmp,"connect reach max\r\n");
            msg_print_uart1(tmp);
            close(client_sock);
        }
    }

task_exit:

	if (listen_sock >= 0) {
        close(listen_sock);
        listen_sock = -1;
        tcp_server_socket = -1;
    }

	at_tcp_server_task_handle = NULL;

    vTaskDelete(NULL);
}

int at_socket_server_create_task(int sock)
{
    osThreadDef_t task_def;

    if ((sock < 0) || (at_tcp_server_task_handle != NULL)) {
        return false;
    }

    /* Create task */
    task_def.name = "server";
    task_def.stacksize = 512;
    task_def.tpriority = OS_TASK_PRIORITY_APP;
    task_def.pthread = at_socket_server_listen_task;
    at_tcp_server_task_handle = osThreadCreate(&task_def, (void*)&tcp_server_socket);
    if(at_tcp_server_task_handle == NULL)
    {
        AT_LOGI("at tcp server task create fail \r\n");
    }
    else
    {
        AT_LOGI("at tcp server task create successful \r\n");
    }

    return 1;
}

int at_socket_server_cleanup_task(int sock)
{
    int i;
    char temp[64] ={0};
    at_socket_t *plink = NULL;

	AT_LOGI("at_socket_server_cleanup_task entry\r\n");
    if ((sock < 0) /*|| (sock != tcp_server_socket)*/) {
        return 0;
    }

	AT_LOGI("at_tcp_server_task_handle:%p\r\n",at_tcp_server_task_handle);

    // Destory server socket listener task
    if (at_tcp_server_task_handle) {
        vTaskDelete(at_tcp_server_task_handle);
        at_tcp_server_task_handle = NULL;
    }

	AT_LOGI("tcp_server_socket:%d\r\n",sock);
    if (sock >= 0) {
        close(sock);
        sock = -1;
        tcp_server_socket = -1;
    }

    //close all accept client connection socket
    for (i = 0; i < at_netconn_max_num; i++) {
        plink = at_link_get_id(i);
        if ((plink->sock >= 0) && (plink->terminal_type == AT_REMOTE_CLIENT)) {
           at_socket_client_cleanup_task(plink);
           at_sprintf(temp,"%d,CLOSED\r\n", i);
           msg_print_uart1(temp);
        }
    }
	AT_LOGI("at_socket_server_cleanup_task leave\r\n");
    return 1;
}

void at_server_timeout_cb( TimerHandle_t xTimer )
{
/*
    uint8_t loop = 0;
    at_socket_t *link = NULL;

    for (loop = 0; loop < at_netconn_max_num; loop++) {
        link = at_link_get_id(loop);
        if ((link->sock >= 0) && (link->terminal_type == AT_REMOTE_CLIENT)) {
            link->server_timeout++;

            if (link->server_timeout >= server_timeover) {
                at_socket_client_cleanup_task(link);
            }
        }
    }
*/
    at_event_msg_t msg = {0};

    msg.event = AT_DATA_TIMER_EVENT;
    msg.length = 0;
    msg.param = NULL;
    at_data_task_send(&msg);
}

void at_server_timeout_handler(void)
{
    uint8_t loop = 0;
    at_socket_t *link = NULL;

    for (loop = 0; loop < at_netconn_max_num; loop++) {
        link = at_link_get_id(loop);
        if ((link->sock >= 0) && (link->terminal_type == AT_REMOTE_CLIENT)) {
            link->server_timeout++;

            if (link->server_timeout >= server_timeover) {
                at_socket_client_cleanup_task(link);
                if (server_timeout_timer) {
                    xTimerStop(server_timeout_timer, portMAX_DELAY);
                    xTimerDelete(server_timeout_timer, portMAX_DELAY);
                    server_timeout_timer = NULL;
                }
            }
        }
    }
    AT_LOGI("at_server_timeout_handler\r\n");
}

/*
 * @brief Command at+at_cmd_tcpip_cipstatus
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipstatus(char *buf, int len, int mode)
{
    uint8_t* temp = NULL;
    uint8_t i = 0;
    struct sockaddr_in local_addr;
    socklen_t local_name_len;
    uint8_t pStr[4];
    int sc_type = 0;
    socklen_t optlen = sizeof(sc_type);
    uint8_t ret = AT_RESULT_CODE_ERROR;

    temp = (uint8_t*)at_malloc(64);

    at_sprintf(temp, "STATUS:%d\r\n", mdState);
    msg_print_uart1((char*)temp);

    for(i = 0; i < AT_LINK_MAX_NUM; i++)
    {
        at_socket_t *pLink = at_link_get_id(i);
        if(pLink->sock >= 0)
        {
            local_name_len = sizeof(local_addr);
            if(getsockopt(pLink->sock, SOL_SOCKET, SO_TYPE, &sc_type, &optlen) < 0)
            {
                goto exit;
            }

            if(getsockname(pLink->sock, (struct sockaddr*)&local_addr, &local_name_len) < 0)
            {
                goto exit;
            }

            at_memset(pStr, 0x0 ,sizeof(pStr));
            if(sc_type == SOCK_STREAM) // tcp
            {
                pStr[0] = 'T';
                pStr[1] = 'C';
                pStr[2] = 'P';
            }
            else if(sc_type == SOCK_DGRAM) // udp
            {
                pStr[0] = 'U';
                pStr[1] = 'D';
                pStr[2] = 'P';
            }
            else // raw
            {
                pStr[0] = 'R';
                pStr[1] = 'A';
                pStr[2] = 'W';
            }

            at_sprintf(temp, "%s:%d,\"%s\",\""IPSTR"\",%d,%d,%d\r\n",
                    "+CIPSTATIS",
                    pLink->link_id,
                    pStr,
                    IP2STR(&pLink->remote_ip),
                    pLink->remote_port,
                    ntohs(local_addr.sin_port),
                    pLink->terminal_type);
            msg_print_uart1((char*)temp);
        }
    }

    ret = AT_RESULT_CODE_OK;

exit:
    at_free(temp);
    at_response_result(ret);

    return ret;
}

/*
 * @brief Command at+cipdomain
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipdomain(char *buf, int len, int mode)
{
    char *argv[AT_MAX_CMD_ARGS] = {0};
    int argc = 0;
    char *pStr = NULL;
    ip_addr_t ip_address;
    struct sockaddr_in addr;
    char tmp[32];
    int cnt = 1;
    int ret = false;

    if (!_at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        AT_LOGI("at_cmd_buf_to_argc_argv fail\r\n");
        goto exit;
    }

    if (argc != 2)
        goto exit;

    pStr = at_cmd_param_trim(argv[cnt++]);
    if (!pStr)
    {
        AT_LOGI("Invalid param\r\n");
        goto exit;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = lwip_htons(80);

    if (inet_pton(AF_INET, pStr, &addr.sin_addr) != 1) {
        if (at_resolve_dns(pStr, &addr) < 0) {
            msg_print_uart1((char*)"domain fail\r\n");
            goto exit;
        }
        ret = 1;
    }
    inet_addr_to_ip4addr(ip_2_ip4(&ip_address), &addr.sin_addr);

    at_sprintf(tmp,"+CIPDOMAIN:"IPSTR"\r\n",IP2STR(&ip_address));
    msg_print_uart1(tmp);

exit:
    if (ret)
        msg_print_uart1("\r\nOK\r\n");
    else
        msg_print_uart1("\r\nERROR\r\n");

    return ret;
}

/*
 * @brief Command at+cipstart
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipstart(char *buf, int len, int mode)
{
    char *argv[AT_MAX_CMD_ARGS] = {0};
    int argc = 0;
    char *pstr = NULL;
    char *remote_addr;
    char response[16];
    uint8_t linkID;
    uint8_t linkType = AT_LINK_TYPE_INVALID;
    uint8_t para = 1;
    int changType = 0;
    int remotePort = 0, localPort = 0;
    int keepAliveTime = 0;
    at_socket_t *link;
    uint8_t ret = AT_RESULT_CODE_ERROR;

    if (!_at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        AT_LOGI("at_cmd_buf_to_argc_argv fail\r\n");
        goto exit;
    }

    if (argc < 4)
    {
        AT_LOGI("Invalid param\r\n");
        goto exit;
    }

    if (at_ipMux)
    {
        linkID = atoi(argv[para++]);
    }
    else
    {
        linkID = 0;
    }

    if (linkID >= AT_LINK_MAX_NUM)
    {
        msg_print_uart1("ID ERROR\r\n");
        goto exit;
    }

    pstr = at_cmd_param_trim(argv[para++]);
    if(!pstr)
    {
        msg_print_uart1("Link type ERROR\r\n");
        goto exit;
    }

    if (at_strcmp(pstr, "TCP") == NULL)
    {
        linkType = AT_LINK_TYPE_TCP;
    }
    else if (at_strcmp(pstr, "UDP") == NULL)
    {
        linkType = AT_LINK_TYPE_UDP;
    }
    else if (at_strcmp(pstr, "SSL") == NULL)
    {
        linkType = AT_LINK_TYPE_SSL;
    }
    else
    {
        msg_print_uart1("Link type ERROR\r\n");
        goto exit;
    }
    remote_addr = at_cmd_param_trim(argv[para++]);
    if(!remote_addr)
    {
        msg_print_uart1("IP ERROR\r\n");
        goto exit;
    }
    AT_LOGI("%s\r\n", remote_addr);

    remotePort = atoi(argv[para++]);
    if((remotePort == 0))
    {
        msg_print_uart1("Miss param\r\n");
        goto exit;
    }

    if((remotePort > 65535) || (remotePort < 0))
    {
       goto exit;
    }

    AT_LOGI("remote port %d\r\n", remotePort);

    if (linkType == AT_LINK_TYPE_TCP)
    {
        if (para < argc)
        {
            keepAliveTime = atoi(argv[para++]);
            AT_LOGI("keepAliveTime %d\r\n", keepAliveTime);
            if ((keepAliveTime > 7200) || (keepAliveTime < 0)) {
                 goto exit;
            }
        }
        changType = 1;
    }
    else if (linkType == AT_LINK_TYPE_UDP)
    {
        if (para < argc)
        {
            localPort = atoi(argv[para++]);
            AT_LOGI("localPort %d\r\n", localPort);
            if (localPort == 0)
            {
                msg_print_uart1("Miss param2\r\n");
                goto exit;
            }
            if ((localPort > 65535) || (localPort <= 0))
            {
                goto exit;
            }
        }

        if (para < argc)
        {
            changType = atoi(argv[para++]); //last param
        }
        AT_LOGI("changType %d\r\n", changType);
    }
    else if (linkType == AT_LINK_TYPE_SSL)
    {
        AT_LOGI("SSL not implement yet\r\n");
    }

    link = at_link_alloc_id(linkID);
    if (link == NULL)
    {
        msg_print_uart1("ALREAY CONNECT\r\n");
        goto exit;
    }

    link->link_type = linkType;
    link->link_state = AT_LINK_DISCONNECTED;
    link->change_mode = changType;
    link->remote_port = remotePort;
    link->local_port = localPort;
    link->repeat_time = 0;
    link->keep_alive = keepAliveTime;
    link->terminal_type = AT_LOCAL_CLIENT;

    if ((link->recv_buf = (char *)malloc(AT_DATA_RX_BUFSIZE * sizeof(char))) == NULL) {
        AT_LOGI("rx buffer alloc fail\r\n");
        goto exit;
    }

    switch (linkType)
    {
        case AT_LINK_TYPE_TCP:
            if (at_create_tcp_client(link, remote_addr, remotePort, 0) < 0)
            {
                msg_print_uart1("CONNECT FAIL\r\n");
                at_close_client(link);
                goto exit;
            }
            break;

        case AT_LINK_TYPE_UDP:
            if (at_create_udp_client(link, remote_addr, remotePort, localPort, 0) < 0)
            {
                msg_print_uart1("CONNECT FAIL\r\n");
                at_close_client(link);
                goto exit;
            }
            break;

        case AT_LINK_TYPE_SSL:
            msg_print_uart1("Not Support Yet\r\n");
        default:
            goto exit;
    }

    if(at_ipMux)
    {
        at_sprintf(response,"%d,CONNECT\r\n", linkID);
        msg_print_uart1(response);
    }
    else
    {
        msg_print_uart1("CONNECT\r\n");
    }

    ret = AT_RESULT_CODE_OK;

    at_socket_client_create_task(link);

exit:
    at_response_result(ret);

    return ret;
}

/*
 * @brief Command at+cipsend
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipsend(char *buf, int len, int mode)
{
    char *param = NULL;
    at_socket_t *link;
    int send_id = 0;
    int send_len = 0;
    uint8_t ret = AT_RESULT_CODE_ERROR;

    switch (mode) {
        case AT_CMD_MODE_EXECUTION:
            //TODO:start sending data in transparent transmission mode.
            break;

        case AT_CMD_MODE_SET:  // AT+CIPSEND= link,<op>
            param = strtok(buf, "=");

            if (at_ipMux) {
                /* Multiple connections */
                param = strtok(NULL, ",");
                send_id = (uint8_t)atoi(param);
                if (send_id >= AT_LINK_MAX_NUM) {
                    goto exit;
                }
            } else {
                send_id = 0;
            }

            link = at_link_get_id(send_id);
            if (link->sock < 0) {
                msg_print_uart1("link is not connected\r\n");
                goto exit;
            }

            param = strtok(NULL, "\0");

            send_len = atoi(param);
            if (send_len > AT_DATA_LEN_MAX) {
               msg_print_uart1("data length is too long\r\n");
               goto exit;
            }


            if (link->link_type == AT_LINK_TYPE_UDP)
            {
              //TODO: Remote IP and ports can be set in UDP transmission:
              //AT+CIPSEND=[<link ID>,]<length>[,<remote IP>,<remote port>]
            }

            //switch port input to TCP/IP module
            sending_id = send_id;
            at_send_len = send_len;
            pDataLine = at_data_line;
            data_process_lock(LOCK_TCPIP, at_send_len);
            at_response_result(AT_RESULT_CODE_OK);
            msg_print_uart1("\r\n> ");
            ret = AT_RESULT_CODE_IGNORE;
            break;

        default :
            break;
    }

exit:
    at_response_result(ret);
    return ret;
}

/*
 * @brief Command at+cipsendEX
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipsendex(char *buf, int len, int mode)
{

    uint8_t ret;

    ret = _at_cmd_tcpip_cipsend(buf, len, mode);

    if (ret == AT_RESULT_CODE_IGNORE) {
        at_socket_t *link = at_link_get_id(sending_id);
        link->send_mode = AT_SEND_MODE_SENDEX;
    }

    return ret;
}

/*
 * @brief Command at+cipclose
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipclose(char *buf, int len, int mode)
{
    char *param = NULL;
    char resp_buf[32];
    uint8_t link_id, id;
    uint8_t ret = AT_RESULT_CODE_ERROR;
    at_socket_t *link;

    switch (mode) {
        case AT_CMD_MODE_EXECUTION:
            if (at_ipMux)
            {
                msg_print_uart1("MUX=1\r\n");
                goto exit;
            }

            link = at_link_get_id(0);
            if (link->sock >= 0)
            {
                at_socket_client_cleanup_task(link);
                at_sprintf(resp_buf,"%d,CLOSED\r\n", 0);
                msg_print_uart1(resp_buf);
                ret = AT_RESULT_CODE_OK;
            }

            break;

        case AT_CMD_MODE_SET:
            if(at_ipMux == 0)
            {
                msg_print_uart1("MUX=0\r\n");
                goto exit;
            }

            param = strtok(buf, "=");
            param = strtok(NULL, "\0");
            link_id = atoi(param);

            if (link_id == AT_LINK_MAX_NUM) {
                /* when ID=5, all connections will be closed.*/
                for(id = 0; id < AT_LINK_MAX_NUM; id++) {
                    link = at_link_get_id(id);

                    if (link->link_en == 1 || link->sock >=0) {
                        at_socket_client_cleanup_task(link);
                        at_sprintf(resp_buf,"%d,CLOSED\r\n", id);
                        msg_print_uart1(resp_buf);
                    }
                }
            } else {
                link = at_link_get_id(link_id);
                if (link->link_en == 1 || link->sock >=0) {
                    at_socket_client_cleanup_task(link);
                    at_sprintf(resp_buf,"%d,CLOSED\r\n", link_id);
                    msg_print_uart1(resp_buf);
                }
            }
            ret = AT_RESULT_CODE_OK;
            break;

        default :
            ret = AT_RESULT_CODE_IGNORE;
            break;
    }

exit:
    at_response_result(ret);

    return true;
}

/*
 * @brief Command at+cifsr
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cifsr(char *buf, int len, int mode)
{
    net_adapter_ip_info_t adapter_ip_info;
    char temp[64];
    uint8_t bssid[6] = {0};
    wifi_mode_t wifi_mode = WIFI_MODE_STA;
    int ret = AT_RESULT_CODE_OK;

    if (mode == AT_CMD_MODE_EXECUTION)
    {
        //TODO:Get wifi mode by wifi_api
        if (wifi_mode == WIFI_MODE_STA)
        {
            at_memset(&adapter_ip_info,0x0,sizeof(adapter_ip_info));
            // Only when the STA is connected to an AP then the STA IP can be queried
            if(wifi_station_get_connect_status() == STATION_GOT_IP) {
                net_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &adapter_ip_info);
            }

            at_sprintf(temp, "%s:STAIP,\""IPSTR"\"\r\n", "+CIFSR", IPV42STR(&adapter_ip_info.ip));
            at_uart1_printf(temp);

            wpa_cli_getmac(bssid);
            at_sprintf(temp, "%s:STAMAC,\""MACSTR"\"\r\n", "+CIFSR", MAC2STR(bssid));
            at_uart1_printf(temp);
        }
    }
    else
    {
        ret = AT_RESULT_CODE_IGNORE;
    }

//exit:
    at_response_result(ret);

    return ret;
}

/*
 * @brief Command at+cipmux
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipmux(char *buf, int len, int mode)
{
    int  ipMux;
    char resp_buf[32];
    uint8_t ret = AT_RESULT_CODE_ERROR;

    switch (mode) {
        case AT_CMD_MODE_READ:
            at_sprintf(resp_buf, "%s:%d\r\n", "+CIPMUX", at_ipMux);
            msg_print_uart1(resp_buf);
            break;

        case AT_CMD_MODE_SET:

            if (mdState == AT_STA_LINKED)
            {
                msg_print_uart1("link is builded\r\n");
                goto exit;
            }

            if (at_strstr((char *)buf, "AT+CIPMUX=0") != NULL) {
                ipMux = 0;
            } else if (at_strstr((char *)buf, "AT+CIPMUX=1") != NULL) {
                ipMux = 1;
            } else {
                goto exit;
            }

            if (ipMux == 1)
            {
                if (at_ip_mode == true)  // now serverEn is 0
                {
                    msg_print_uart1("IPMODE must be 0\r\n");
                    goto exit;
                }
                at_ipMux = true;
            }
            else
            {
                if (tcp_server_socket >= 0)
                {
                    msg_print_uart1("CIPSERVER must be 0\r\n");
                    goto exit;
                }
                if((at_ipMux == TRUE) && (mdState == AT_STA_LINKED))
                {
                    msg_print_uart1("Connection exists\r\n");
                    goto exit;
                }
                at_ipMux = false;
            }
            break;

        default :
            ret = AT_RESULT_CODE_IGNORE;
            goto exit;
    }

    ret = AT_RESULT_CODE_OK;
exit:
    at_response_result(ret);

    return ret;
}

/*
 * @brief Command at+cipserver
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipserver(char *buf, int len, int mode)
{
    char *argv[AT_MAX_CMD_ARGS] = {0};
    int argc = 0;
    //char *pstr = NULL;
    //uint8_t linkType = AT_LINK_TYPE_INVALID;
    uint8_t para = 1;
    int32_t server_enable = 0;
    int32_t port = 0;
    //int32_t ssl_ca_en = 0;
    //at_socket_t *link;
    uint8_t ret = AT_RESULT_CODE_ERROR;

    if (!_at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        AT_LOGI("at_cmd_buf_to_argc_argv fail\r\n");
        goto exit;
    }

    if (argc < 2)
    {
        goto exit;
    }

    if (at_ipMux == FALSE)
    {
        goto exit;
    }

    server_enable = atoi(argv[para++]);

    if (server_enable == 1)
    {
        if (para != argc)
        {
            port = atoi(argv[para++]);
            if ((port > 65535) || (port <= 0))
            {
                goto exit;
            }

            AT_LOGI("port %d\r\n", port);
        }
        else
        {
            port = AT_SERVER_DEFAULT_PORT;
        }

        if (para != argc)
        {
            #if 0
            pstr = at_cmd_param_trim(argv[para++]);
            if (!pstr)
            {
                goto exit;
            }
            if (at_strcmp(pstr, "SSL") == NULL)
            {
                linkType = AT_LINK_TYPE_SSL;
            }
            else
            {
                linkType = AT_LINK_TYPE_TCP;
            }

            if (para != argc)
            {
                ssl_ca_en = atoi(argv[para++]);
            }
            #endif
        }
    }

    if (server_enable == 1)
    {
        at_create_tcp_server(port, 0);
    }
    else
    {
        if (tcp_server_socket < 0)
        {
            msg_print_uart1("no change\r\n");
            ret = AT_RESULT_CODE_OK;
            goto exit;
        }
        else
        {
            at_socket_server_cleanup_task(tcp_server_socket);
        }
    }

    ret = AT_RESULT_CODE_OK;

exit:
    at_response_result(ret);
    return true;
}

/*
 * @brief Command at+cipmode
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipmode(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+savetranslink
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_savetranslink(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+cipsto
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipsto(char *buf, int len, int mode)
{
    char *argv[AT_MAX_CMD_ARGS] = {0};
    int argc = 0;
    char resp_buf[32];
    int timeout = 0;
    at_socket_t *link;

    uint8_t ret = AT_RESULT_CODE_ERROR;

    switch (mode) {
        case AT_CMD_MODE_READ:
            at_sprintf(resp_buf, "%s:%d\r\n", "+CIPSTO", server_timeover);
            at_uart1_printf(resp_buf);
            break;

        case AT_CMD_MODE_SET:

            if (!_at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
            {
                AT_LOGI("at_cmd_buf_to_argc_argv fail\r\n");
                goto exit;
            }

            if (argc != 2)
                goto exit;

            timeout = atoi(argv[1]);
            if ((timeout > 7200) || (timeout < 0)) //max timeout 7200s
            {
                goto exit;
            }
            if (timeout != server_timeover)
            {
                server_timeover = timeout;
                if (server_timeover == 0) {
                    uint8_t idx = 0;
                    for(idx = 0; idx < at_netconn_max_num; idx++) {
                        link = at_link_get_id(idx);
                        if((link->sock >= 0) && (link->terminal_type == AT_REMOTE_CLIENT)) {
                            link->server_timeout = 0;
                        }
                    }
                    if (server_timeout_timer) {
                        xTimerStop(server_timeout_timer, portMAX_DELAY);
                        xTimerDelete(server_timeout_timer, portMAX_DELAY);
                        server_timeout_timer = NULL;
                    }
                } else {
                    if (server_timeout_timer == NULL) {
                        server_timeout_timer = xTimerCreate("ServerTimoutTmr",
                                              (1 * 1000 / portTICK_PERIOD_MS),
                                              pdTRUE,
                                              NULL,
                                              at_server_timeout_cb);
                        xTimerStart(server_timeout_timer, 0);
                    }
                }
            } else {
                msg_print_uart1("not change\r\n");
            }

            break;

        default :
            ret = AT_RESULT_CODE_IGNORE;
            goto exit;
    }

    ret = AT_RESULT_CODE_OK;
exit:
    at_response_result(ret);

    return ret;
}

/*
 * @brief Command at+ciupdate
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_ciupdate(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+cipdinfo
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipdinfo(char *buf, int len, int mode)
{
    char *argv[AT_MAX_CMD_ARGS] = {0};
    char temp[64] = {0};
    int argc = 0;
    int ipdinfo_en = 0;
    uint8_t ret = AT_RESULT_CODE_ERROR;

    switch (mode) {
        case AT_CMD_MODE_READ:
            at_sprintf(temp, "%s:%s\r\n", "+CIPDINFO", ipd_info_enable? "TRUE":"FALSE" );
            msg_print_uart1(temp);
            ret = AT_RESULT_CODE_OK;
            break;

        case AT_CMD_MODE_SET:
            if (!_at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
            {
                AT_LOGI("at_cmd_buf_to_argc_argv fail\r\n");
                goto exit;
            }

            if (argc != 2)
                goto exit;

            ipdinfo_en = atoi(argv[1]);

            if(ipdinfo_en == 0) {
                ipd_info_enable = false;
            } else if(ipdinfo_en == 1) {
                ipd_info_enable = true;
            } else {
                goto exit;
            }

            ret = AT_RESULT_CODE_OK;
            break;

        default :
            ret = AT_RESULT_CODE_IGNORE;
            break;
    }

exit:
    at_response_result(ret);

    return true;
}

/*
 * @brief Command at+cipsntpcfg
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipsntpcfg(char *buf, int len, int mode)
{
    char *argv[AT_MAX_CMD_ARGS] = {0};
    int argc = 0;
    uint32_t para = 1;
    int32_t timezone = 0;
    int32_t sntp_enable = 0;
    uint8_t count = 0;
    char* pstr = NULL;
    char* server[SNTP_MAX_SERVERS];

    uint8_t ret = AT_RESULT_CODE_ERROR;

    at_memset(server, 0x0, sizeof(server));

    if (!_at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        AT_LOGI("at_cmd_buf_to_argc_argv fail\r\n");
        goto exit;
    }

    if (argc < 2)
    {
        AT_LOGI("Invalid param\r\n");
        goto exit;
    }

    sntp_enable = atoi(argv[para++]);
    timezone = atoi(argv[para++]);

	if ((timezone < -11) || (timezone > 13)) {
		goto exit;
	}

	for(count = 0; count < SNTP_MAX_SERVERS; count++) {
	    if(para != argc) {
	    	// sntp server
	    	if ((pstr = at_cmd_param_trim(argv[para++])) == NULL) {
	    	    goto exit;
 		    }

			if ((pstr == NULL) || (at_strlen(pstr) == 0) || (at_strlen(pstr) > 32)) {
				goto exit;
			}

			server[count] = pstr;
	    } else {
			break;
		}
	}

    if (para != argc) {
		goto exit;
	}

	if (count == 0) {
#if (SNTP_MAX_SERVERS > 0)
        server[0] = "cn.ntp.org.cn";
#endif

#if (SNTP_MAX_SERVERS > 1)
        server[1] = "ntp.sjtu.edu.cn";
#endif

#if (SNTP_MAX_SERVERS > 2)
        server[2] = "us.pool.ntp.org";
#endif
	}

    if (sntp_enable)
    {
    	sntp_stop();

    	//at_snprintf(buf,sizeof(buf),"GMT%+d",timezone);
        //setenv("TZ", (char*)buf, 1);
    	//tzset();
        for(count = 0; count < SNTP_MAX_SERVERS; count++) {
            if (at_sntp_server[count] != NULL) {
                at_free(at_sntp_server[count]);
                at_sntp_server[count] = NULL;
            }

            if (server[count] != NULL) {
                at_sntp_server[count] = at_malloc(at_strlen(server[count]) + 1);
                if (at_sntp_server[count] == NULL) {
                    goto exit;
                }

                at_strcpy(at_sntp_server[count], server[count]);
            }
            sntp_setservername(count, at_sntp_server[count]);
    	}
    	sntp_init();
    }
    else
    {
        sntp_stop();
    }
    ret = AT_RESULT_CODE_OK;
exit:
    at_response_result(ret);

    return ret;
}

/*
 * @brief Command at+cipstamac
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipstamac(char *buf, int len, int mode)
{
    char *argv[AT_MAX_CMD_ARGS] = {0};
    int argc = 0;
    uint8_t ret = AT_RESULT_CODE_ERROR;
    uint8_t mac[6] = {0};
    char temp[64]={0};
    char *pstr;
    int state;
    
    switch (mode) {
        case AT_CMD_MODE_READ:
            wpa_cli_getmac(mac);
            at_sprintf(temp, "%s:\""MACSTR"\"\r\n", "+CIPSTAMAC",MAC2STR(mac));
            msg_print_uart1(temp);
            ret = AT_RESULT_CODE_OK;

            break;
        case AT_CMD_MODE_SET:
            if (!_at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS)) {
                AT_LOGI("at_cmd_buf_to_argc_argv fail\r\n");
                goto exit;
            }

            if (argc < 2) {
                goto exit;
            }

            pstr = at_cmd_param_trim(argv[1]);
            if (!pstr)
            {
                AT_LOGI("Invalid param\r\n");
                goto exit;
            }
            //printf("%s\n",pstr);
            hwaddr_aton2(pstr, mac);

            if(is_multicast_ether_addr(mac)) {
                AT_LOGI("Invalid mac address \r\n");
                goto exit;
            }

            if ((mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0x00) {
                AT_LOGI("Invalid mac address \r\n");
                goto exit;
            }

            if (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF && 
                mac[3] == 0xFF && mac[4] == 0xFF && mac[5] == 0xFF) {
                AT_LOGI("Invalid mac address \r\n");
                goto exit;
            }
    
            state = wpas_get_state();
            if(state == WPA_COMPLETED || state == WPA_ASSOCIATED) {
                AT_LOGI("In connected, set mac address failed\r\n");
                ret = AT_RESULT_CODE_FAIL;
                goto exit;
            }

            wpa_cli_setmac(mac);

            ret = AT_RESULT_CODE_OK;

            break;
        default :
            ret = AT_RESULT_CODE_IGNORE;
            break;
    }

exit:
    at_response_result(ret);

    return true;
}

/*
 * @brief Command at+cipsta
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipsta(char *buf, int len, int mode)
{
    /** TBD, Need LWIP's api to get it */

    msg_print_uart1("\r\nOK\r\n");
    return true;
}

/*
 * @brief Command at+cipap
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipap(char *buf, int len, int mode)
{
    msg_print_uart1("\r\nOK\r\n");
    return true;
}

/*
 * @brief Command at+cipsntptime
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_cipsntptime(char *buf, int len, int mode)
{
	char tmp[64] = {0};
	//time_t now;
	struct tm timeinfo;
    uint8_t ret = AT_RESULT_CODE_OK;

    //time(&now);
	//localtime_r(&now, &timeinfo);

	if (buf) {
		at_sprintf(tmp,"%s:%s","+CIPSNTPTIME",asctime(&timeinfo));
		at_uart1_printf(tmp);
	}

    at_response_result(ret);
    return ret;
}

/*
 * @brief Command at+ping
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_ping(char *buf, int len, int mode)
{
    int iRet = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};
    int argc = 0;
    char baIpStrBuf[IP_STR_BUF_LEN] = {0};
    char *sTarget = NULL;
    uint32_t u32Cnt = 0;

    if(mode != AT_CMD_MODE_SET)
    {
        AT_LOG("[%s %d] invalid mode[%d]\n", __func__, __LINE__, mode);
        goto done;
    }

    if(!at_cmd_is_tcpip_ready())
    {
        AT_LOG("[%s %d] at_cmd_is_tcpip_ready fail\n", __func__, __LINE__);
        goto done;
    }

    if(!_at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        AT_LOG("[%s %d] _at_cmd_buf_to_argc_argv fail\n", __func__, __LINE__);
        goto done;
    }

    if(argc < 2)
    {
        AT_LOG("[%s %d] no param\n", __func__, __LINE__);
        goto done;
    }

    AT_LOG("[%s %d] argc[%d] argv[1]: [%s][%d]\n", __func__, __LINE__, argc, argv[1], strlen(argv[1]));

    //sTarget = at_cmd_param_trim(argv[1]);
    sTarget = argv[1];

    if(!sTarget)
    {
        AT_LOG("[%s %d] invalid param\n", __func__, __LINE__);
        goto done;
    }

    if(at_cmd_is_valid_ip(sTarget))
    {
        strcpy(baIpStrBuf, sTarget);
    }
    else
    {
        struct addrinfo *ptAddrInfo = NULL;

        if(getaddrinfo(sTarget, AT_CMD_PING_SERV_PORT_STR, NULL, &ptAddrInfo))
        {
            AT_LOG("[%s %d] getaddrinfo fail\n", __func__, __LINE__);
            goto done;
        }

        if(ptAddrInfo)
        {
            struct sockaddr_in *ptAddrIn = (struct sockaddr_in *)ptAddrInfo->ai_addr;

            snprintf(baIpStrBuf, sizeof(baIpStrBuf), "%s", inet_ntoa(ptAddrIn->sin_addr));
            freeaddrinfo(ptAddrInfo);
        }
        else
        {
            AT_LOG("[%s %d] ptAddrInfo is NULL\n", __func__, __LINE__);
            goto done;
        }
    }

    AT_LOG("[%s %d] baIpStrBuf[%s][%d]\n", __func__, __LINE__, baIpStrBuf, strlen(baIpStrBuf));

    g_tPingResult.recv_num = 0;

    // recv_timout and ping_period won't be assigned in ping_request
    g_ping_arg.recv_timout = AT_CMD_PING_RECV_TIMEOUT;
    g_ping_arg.ping_period = AT_CMD_PING_PERIOD;

    g_u8PingCmdDone = 0;

    ping_request(AT_CMD_PING_NUM, baIpStrBuf, PING_IP_ADDR_V4, AT_CMD_PING_SIZE, at_cmd_ping_callback);

    while(u32Cnt < AT_CMD_PING_WAIT_RSP_MAX_CNT)
    {
        if(g_u8PingCmdDone)
        {
            if(g_tPingResult.recv_num)
            {
                iRet = 1;
            }

            break;
        }

        osDelay(AT_CMD_PING_WAIT_RSP);
        ++u32Cnt;
    }

done:
    if(iRet)
    {
        at_output("\r\n+PING:%u\r\nOK\r\n", g_tPingResult.avg_time);
    }
    else
    {
        at_output("\r\n+PING:TIMEOUT\r\nERROR\r\n");
    }

    return iRet;
}

/*
 * @brief Command at+tcpipstart
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_start(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+at_cmd_tcpip_end
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_end(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcpiprsv
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_rsv(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+at_cmd_tcpip_httpdevconf_start
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_httpdevconf_start(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+at_cmd_tcpip_httpdevconf_stop
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_httpdevconf_stop(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcp
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcp(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcpm
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpm(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcplist
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcplist(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcpleave
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpleave(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcps
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcps(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcpslist
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpslist(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcpsdis
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpsdis(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcpsstop
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpsstop(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+tcpsm
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpsm(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+udps
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_udps(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+udpsstop
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_udpsstop(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+udp
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_udp(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+mqttsubscribe_start
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_mqttsubscribe_start(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+mqttsubscribe_stop
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_mqttsubscribe_stop(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+mqttpublish
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_mqttpublish(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+mqttshowlist
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_mqttshowlist(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_svr_start
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_svr_start(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_svr_stop
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_svr_stop(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_svr_add_service
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_svr_add_service(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_svr_delete_service
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_svr_delete_service(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_svr_show_service
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_svr_show_service(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_client_start
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_client_start(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_client_get
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_client_stop(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_client_post
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_client_get(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_client_put
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_client_put(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_client_delete
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_client_delete(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_client_observe
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_client_observe(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_client_observe_cancel
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_client_observe_cancel(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+coap_client_observe_show
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_coap_client_observe_show(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+sendemail
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_sendemail(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+http_sendreq
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_http_sendreq(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+https_sendreq
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_https_sendreq(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+iperf
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_iperf(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+mdns_start
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_mdns_start(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+mdns_add_service
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_mdns_add_service(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+mdns_del_service
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_mdns_del_service(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+mdns_stop
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_mdns_stop(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+ssdp_start
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_ssdp_start(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+ssdp_stop
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_ssdp_stop(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+ssdp_show_notice
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_ssdp_show_notice(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+ssdp_send_msearch
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_ssdp_send_msearch(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+show_dns_info
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_show_dns_info(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command at+set_dns_svr
 *
 * @param [in] argc count of parameters
 *
 * @param [in] argv parameters array
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_set_dns_svr(char *buf, int len, int mode)
{
    return true;
}

/*
 * @brief Command Sample code to do TCP/IP test
 *
 * @return 0 fail 1 success
 *
 */
int _at_cmd_tcpip_sample(void)
{
    return true;
}


/**
  * @brief AT Command Table for TCP/IP Module
  *
  */
_at_command_t _gAtCmdTbl_Tcpip[] =
{
#if defined(__AT_CMD_ENABLE__)
    { "AT+CIPSTATUS",        _at_cmd_tcpip_cipstatus,   "Get connection status" },
    { "AT+CIPDOMAIN",        _at_cmd_tcpip_cipdomain,   "DNS domain function" },
    { "AT+CIPSTART",         _at_cmd_tcpip_cipstart,    "Establish TCP connection, UDP transmission" },
    { "AT+CIPSEND",          _at_cmd_tcpip_cipsend,     "Send data" },
    { "AT+CIPSENDEX",        _at_cmd_tcpip_cipsendex,   "Send data till \0" },
    { "AT+CIPCLOSE",         _at_cmd_tcpip_cipclose,    "Close TCP/UDP" },
    { "AT+CIFSR",            _at_cmd_tcpip_cifsr,       "Get local IP address" },
    { "AT+CIPMUX",           _at_cmd_tcpip_cipmux,      "Set multi-connection mode" },
    { "AT+CIPSERVER",        _at_cmd_tcpip_cipserver,   "Set TCP server" },
    { "AT+CIPMODE",          _at_cmd_tcpip_cipmode,     "Set transmission mode" },
    { "AT+SAVETRANSLINK",    _at_cmd_tcpip_savetranslink, "Save to flash" },
    { "AT+CIPSTO",           _at_cmd_tcpip_cipsto,      "Set timeout of TCP server" },
    { "AT+CIUPDATE",         _at_cmd_tcpip_ciupdate,    "Update Wi-Fi software" },
    { "AT+CIPDINFO",         _at_cmd_tcpip_cipdinfo,    "When receive data, +IPD remote IP/port" },
    { "AT+CIPSNTPCFG",       _at_cmd_tcpip_cipsntpcfg,  "Configuration for SNTP server" },
    { "AT+CIPSNTPTIME",      _at_cmd_tcpip_cipsntptime, "Query SNTP time" },
    { "AT+CIPSTAMAC",        _at_cmd_tcpip_cipstamac,    "Set MAC address of station " },
    { "AT+CIPSTA",           _at_cmd_tcpip_cipsta,       "Set Station IP" },
    { "AT+CIPAP",            _at_cmd_tcpip_cipap,        "Set softAP IP" },
    { "AT+PING",             _at_cmd_tcpip_ping,        "Function PING" },
    { "AT+TCPIPSTART",       _at_cmd_tcpip_start,       "TCP/IP Start" },    //Back Door
    { "AT+TCPIPEND",         _at_cmd_tcpip_end,         "TCP/IP End" },      //Back Door
    { "AT+TCPIPRSV",         _at_cmd_tcpip_rsv,         "TCP/IP Reserved" }, //Back Door
    { "AT+HTTPDEVCONF_START",_at_cmd_tcpip_httpdevconf_start, "Start HTTP Server to do data configuration" },
    { "AT+HTTPDEVCONF_STOP", _at_cmd_tcpip_httpdevconf_stop,  "Stop data configuration" },
    { "AT+TCP",              _at_cmd_tcp,               "Connect to TCP server" },
    { "AT+TCPM",             _at_cmd_tcpm,              "Send data to TCP server" },
    { "AT+TCPLIST",          _at_cmd_tcplist,           "Show all connections of the remote TCP server" },
    { "AT+TCPLEAVE",         _at_cmd_tcpleave,          "Disconnect the TCP connection" },
    { "AT+TCPS",             _at_cmd_tcps,              "Start TCP server" },
    { "AT+TCPSLIST",         _at_cmd_tcpslist,          "Show all connections of the local TCP server" },
    { "AT+TCPSDIS",          _at_cmd_tcpsdis,           "Disconnect a specified TCP connection of the local TCP server" },
    { "AT+TCPSSTOP",         _at_cmd_tcpsstop,          "Stop local TCP server" },
    { "AT+TCPSM",            _at_cmd_tcpsm,             "Send data to remote TCP client" },
    { "AT+UDPS",             _at_cmd_udps,              "Start local UDP server" },
    { "AT+UDPSSTOP",         _at_cmd_udpsstop,          "Stop local UDP server" },
    { "AT+UDP",              _at_cmd_udp,               "Send data to remote UDP client" },
    { "AT+MQTTSUBSCRIBE_START",         _at_cmd_tcpip_mqttsubscribe_start,       "Set topic by MQTT subscribe" },
    { "AT+MQTTSUBSCRIBE_STOP",          _at_cmd_tcpip_mqttsubscribe_stop,        "Stop MQTT subscribe" },
    { "AT+MQTTPUBLISH",                 _at_cmd_tcpip_mqttpublish,               "Send message by MQTT publish on specified topic" },
    { "AT+MQTTSHOWLIST",                _at_cmd_tcpip_mqttshowlist,              "Show all topics of local MQTT subscribe" },
    { "AT+COAP_SVR_START",              _at_cmd_tcpip_coap_svr_start,            "Start CoAP server" },
    { "AT+COAP_SVR_STOP",               _at_cmd_tcpip_coap_svr_stop,             "Stop CoAP server" },
    { "AT+COAP_SVR_ADD_SERVICE",        _at_cmd_tcpip_coap_svr_add_service,      "Add service on CoAP server" },
    { "AT+COAP_SVR_DELETE_SERVICE",     _at_cmd_tcpip_coap_svr_delete_service,   "Delete service on CoAP server" },
    { "AT+COAP_SVR_SHOW_SERVICE",       _at_cmd_tcpip_coap_svr_show_service,     "Show service on CoAP server" },
    { "AT+COAP_CLIENT_START",           _at_cmd_tcpip_coap_client_start,         "Connect a CoAP server" },
    { "AT+COAP_CLIENT_STOP",            _at_cmd_tcpip_coap_client_start,         "Stop a CoAP server" },
    { "AT+COAP_CLIENT_GET",             _at_cmd_tcpip_coap_client_stop,          "Do GET function to CoAP server" },
    { "AT+COAP_CLIENT_POST",            _at_cmd_tcpip_coap_client_get,           "Do POST function to CoAP server" },
    { "AT+COAP_CLIENT_PUT",             _at_cmd_tcpip_coap_client_put,           "Do PUT function to CoAP server" },
    { "AT+COAP_CLIENT_DELETE",          _at_cmd_tcpip_coap_client_delete,        "Do DELETE function to CoAP server" },
    { "AT+COAP_CLIENT_OBSERVE",         _at_cmd_tcpip_coap_client_observe,       "Do OBSERVE function to CoAP server" },
    { "AT+COAP_CLIENT_OBSERVE_CANCEL",  _at_cmd_tcpip_coap_client_observe_cancel,"Do OBSERVE Cancel function to CoAP server" },
    { "AT+COAP_CLIENT_OBSERVE_SHOW",    _at_cmd_tcpip_coap_client_observe_show,  "Show all URI services" },
    { "AT+SENDEMAIL",                   _at_cmd_tcpip_sendemail,                 "Send Email" },
    { "AT+HTTP_SENDREQ",                _at_cmd_tcpip_http_sendreq,              "Send HTTP request to web server" },
    { "AT+HTTPS_SENDREQ",               _at_cmd_tcpip_https_sendreq,             "Send HTTPS request to web server" },
    { "AT+IPERF",                       _at_cmd_tcpip_iperf,                     "Run IPERF" },
    { "AT+MDNS_START",                  _at_cmd_tcpip_mdns_start,                "Enable mDNS" },
    { "AT+MDNS_ADD_SERVICE",            _at_cmd_tcpip_mdns_add_service,          "Add mDNS service" },
    { "AT+MDNS_DEL_SERVICE",            _at_cmd_tcpip_mdns_del_service,          "Delete mDNS service" },
    { "AT+MDNS_STOP",                   _at_cmd_tcpip_mdns_stop,                 "Disable mDNS" },
    { "AT+SSDP_START",                  _at_cmd_tcpip_ssdp_start,                "Open SSDP" },
    { "AT+SSDP_STOP",                   _at_cmd_tcpip_ssdp_stop,                 "Close SSDP" },
    { "AT+SSDP_SHOW_NOTICE",            _at_cmd_tcpip_ssdp_show_notice,          "Specify if show notification message" },
    { "AT+SSDP_SEND_MSEARCH",           _at_cmd_tcpip_ssdp_send_msearch,         "Send M-Search request" },
    { "AT+SHOW_DNS_INFO",               _at_cmd_tcpip_show_dns_info,             "Show DNS server information" },
    { "AT+SET_DNS_SVR",                 _at_cmd_tcpip_set_dns_svr,               "Set DNS server" },
#endif
    { NULL,                             NULL,                                   NULL},
};

/*
 * @brief Global variable g_AtCmdTbl_Tcpip_Ptr retention attribute segment
 *
 */
RET_DATA _at_command_t *_g_AtCmdTbl_Tcpip_Ptr;

/*
 * @brief AT Command Interface Initialization for TCP/IP modules
 *
 */
void _at_cmd_tcpip_func_init(void)
{
    /** Command Table (TCP/IP) */
    _g_AtCmdTbl_Tcpip_Ptr = _gAtCmdTbl_Tcpip;
}


