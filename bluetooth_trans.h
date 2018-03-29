#ifndef _BLUETOOTH_TRANS_H_
#define _BLUETOOTH_TRANS_H_

#include "suma_ipc.h"
#include "uart_serial.h"

#define MAX_LEN_SERIAL_NAME 16

typedef struct _bluetooth_trans_static_params
{
    suma_ipc_handle ipc_in;
    suma_ipc_handle ipc_out;
	/*now only support "ttyO" or "ttyS"*/
	char serial_name[MAX_LEN_SERIAL_NAME]; 

	int *g_tun_fd;
}bluetooth_trans_static_params;

typedef struct _bluetooth_trans_dynamic_params{
    int32_t ctrl;
    int32_t out_port;

	int baud_rate;
	int serial_id;
}bluetooth_trans_dynamic_params;

extern  bluetooth_trans_dynamic_params glb_bluetooth_trans_dynamic_params_default;

typedef void * bluetooth_trans_handle;

bluetooth_trans_handle bluetooth_trans_creat(bluetooth_trans_static_params *static_params,
	                                              bluetooth_trans_dynamic_params *dynamic_params);

#endif
