#ifndef _TUN_TRANS_SYNC_H_
#define _TUN_TRANS_SYNC_H_

    
#include "suma_ipc.h"

#define TUN_MAX_BUFF_LEN    (10*1024)

typedef enum{
    TUN_CLIENT_CTRL,
    TUN_CLIENT_LISTEN_PORT,
    TUN_CLIENT_DEF_NUMS
}TUN_CLIENT_ID_ITEMS;

typedef struct _tun_trans_static_params
{
    suma_ipc_handle ipc_in;
    suma_ipc_handle ipc_out;

	int *tun_fd;
}tun_trans_static_params;

typedef struct _tun_trans_dynamic_params{
    int32_t ctrl;
    int32_t out_port;
}tun_trans_dynamic_params;

extern  tun_trans_dynamic_params glb_tun_trans_dynamic_params_default;

typedef void * tun_trans_handle;

tun_trans_handle tun_trans_create(tun_trans_static_params *static_params,
                                              tun_trans_dynamic_params *dynamic_params);

#endif
