#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h> 
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "suma_api.h"
#include "pthread_base.h"
#include "tun_trans.h"
#include "bluetooth_trans.h"

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

#ifndef NULL
#define NULL 0
#endif

/* report level and macro */
#define RPT_ERR (1) /* error, system error */
#define RPT_WRN (2) /* warning, maybe wrong, maybe OK */
#define RPT_INF (3) /* important information */
#define RPT_DBG (4) /* debug information */

#ifndef S_SPLINT_S /* FIXME */
#define RPTERR(fmt, ...) if(RPT_ERR <= rpt_lvl) fprintf(stderr, "%s: %s: %d: err: " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define RPTWRN(fmt, ...) if(RPT_WRN <= rpt_lvl) fprintf(stderr, "%s: %s: %d: wrn: " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define RPTINF(fmt, ...) if(RPT_INF <= rpt_lvl) fprintf(stderr, "%s: %s: %d: inf: " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define RPTDBG(fmt, ...) if(RPT_DBG <= rpt_lvl) fprintf(stderr, "%s: %s: %d: dbg: " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
static int rpt_lvl = RPT_DBG; /* report level: ERR, WRN, INF, DBG */
#else
#define RPTERR(fmt...)
#define RPTWRN(fmt...)
#define RPTINF(fmt...)
#define RPTDBG(fmt...)
#endif

static suma_ipc_handle bt_uart_2tun = NULL;
static suma_ipc_handle tun_2bt_uart = NULL;

int tun_fd = 0;

static void ipc_web_ctrl_resource_allc(void)
{
	suma_ipc_attr  bits_attr = glb_suma_ipc_Attrs_default;
	
    if(bt_uart_2tun == NULL)
    {
      bits_attr.align = 8;
      bits_attr.bufsize = TUN_MAX_BUFF_LEN;
      bits_attr.buf_nums = 100;
      bits_attr.type = SUMA_IPC_TYPE_OLD;
      bt_uart_2tun = suma_ipc_create(&bits_attr);
    }

	if(tun_2bt_uart == NULL)
    {
      bits_attr.align = 8;
      bits_attr.bufsize = TUN_MAX_BUFF_LEN;
      bits_attr.buf_nums = 100;
      bits_attr.type = SUMA_IPC_TYPE_OLD;
      tun_2bt_uart = suma_ipc_create(&bits_attr);
    }
}

static void bluetooth_trans_static_init(bluetooth_trans_static_params **static_params)
{
    bluetooth_trans_static_params *p_params = NULL;  

    p_params = (bluetooth_trans_static_params *)malloc(sizeof(bluetooth_trans_static_params));
    if(NULL == p_params)
    {
        RPTERR("Null pointer error!");
        return;
    }

    p_params->ipc_in = tun_2bt_uart;
    p_params->ipc_out = bt_uart_2tun;
	p_params->g_tun_fd = &tun_fd;

	strcpy(p_params->serial_name, "ttyUSB");

    *static_params = p_params;

    return;
}
static void bluetooth_trans_dynamic_init(bluetooth_trans_dynamic_params **dynamic, 
					                               bluetooth_trans_dynamic_params *dynamic_default)
{
    bluetooth_trans_dynamic_params *dynamic_params = NULL;

    if(NULL == dynamic_default)
    {
        RPTERR("Null pointer!");
        return;
    }

    dynamic_params = (bluetooth_trans_dynamic_params *)malloc(sizeof(bluetooth_trans_dynamic_params));
    if(NULL == dynamic_params)
    {
        RPTERR("Null pointer!");
        return;
    }

    memcpy(dynamic_params, dynamic_default, sizeof(bluetooth_trans_dynamic_params));
    dynamic_params->ctrl = 1;
    dynamic_params->out_port = 80;

	dynamic_params->serial_id = 0;
	dynamic_params->baud_rate = 256000;//param_list->baud_rate;

    *dynamic = dynamic_params;

    return;
}

static void tun_trans_static_init(tun_trans_static_params **static_params)
{
    tun_trans_static_params *p_params = NULL;  

    p_params = (tun_trans_static_params *)malloc(sizeof(tun_trans_static_params));
    if(NULL == p_params)
    {
        RPTERR("Null pointer error!");
        return;
    }

    p_params->ipc_in = bt_uart_2tun;
    p_params->ipc_out = tun_2bt_uart;
	p_params->tun_fd = &tun_fd;

    *static_params = p_params;

    return;
}
static void tun_trans_dynamic_init(tun_trans_dynamic_params **dynamic, 
			                               const tun_trans_dynamic_params *dynamic_default)
{
    tun_trans_dynamic_params *dynamic_params = NULL;

    if(NULL == dynamic_default)
    {
        RPTERR("Null pointer!");
        return;
    }

    dynamic_params = (tun_trans_dynamic_params *)malloc(sizeof(tun_trans_dynamic_params));
    if(NULL == dynamic_params)
    {
        RPTERR("Null pointer!");
        return;
    }

    memcpy(dynamic_params, dynamic_default, sizeof(tun_trans_dynamic_params));
    dynamic_params->ctrl = 1;
    dynamic_params->out_port = 80;

    *dynamic = dynamic_params;

    return;
}


int main(int argc, char *argv[])
{
        char c = 0;
	pthread_base_handle  handle_bt_trans;
    pthread_base_handle  handle_tun_trans;

	tun_trans_static_params *tun_static_params = NULL;
	tun_trans_dynamic_params *tun_dynamic_params = NULL;

	bluetooth_trans_static_params *bluetooth_static_params = NULL;
	bluetooth_trans_dynamic_params *bluetooth_dynamic_params = NULL;

	ipc_web_ctrl_resource_allc();
	RPTWRN("ipc create success!");
#if 1	
	tun_trans_static_init(&tun_static_params);
	tun_trans_dynamic_init(&tun_dynamic_params,&glb_tun_trans_dynamic_params_default);
	handle_tun_trans = tun_trans_create(tun_static_params,tun_dynamic_params);
	free(tun_static_params);
	free(tun_dynamic_params);
	RPTWRN("tun_trans create success!");
#endif 
	bluetooth_trans_static_init(&bluetooth_static_params);
	bluetooth_trans_dynamic_init(&bluetooth_dynamic_params,&glb_bluetooth_trans_dynamic_params_default);
	handle_bt_trans = bluetooth_trans_creat(bluetooth_static_params,bluetooth_dynamic_params);
	free(bluetooth_static_params);
	free(bluetooth_dynamic_params);
	RPTWRN("bluetooth_trans create success!");

	pthread_base_start(handle_bt_trans, 80);
        
	//pthread_base2_start(handle_tun_trans, 88);
   
    

	RPTWRN("start!");

	 while(1)
    {
        //if(fscanf(stdin, "%s %s %s", cmd, para_name, para_val))
        {
            //usb_cmd_process(sys_ctrol_fd, cmd, para_name, para_val);
        }
        c = getchar();
        if (c == 'q')
           break;
        suma_mssleep(10);
    }
	RPTWRN("delete!");
	
    pthread_base_delete(handle_bt_trans);	
    pthread_base_delete(handle_tun_trans);	
}


