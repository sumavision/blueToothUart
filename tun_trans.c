#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>

#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

#include "suma_api.h"
#include "davinci_base.h"
#include "pthread_base.h"
#include "suma_osa_thr.h"
#include "tun_trans.h"
#include "uart_serial.h"


#define MODULE_NAME     "tun_trans"
#define TUN_DEVICE_NAME "/dev/net/tun"

#define MODULE_TSK_STACK_SIZE  (10*1024)

/* report level and macro */
#define RPT_ERR (1) /* error, system error */
#define RPT_WRN (2) /* warning, maybe wrong, maybe OK */
#define RPT_INF (3) /* important information */
#define RPT_DBG (4) /* debug information */

#ifndef S_SPLINT_S /* FIXME */
#ifdef ANDROID
#include <Antares/core/Antares_log.h>
#define RPTERR(fmt, ...) if(RPT_ERR <= rpt_lvl) log_printf(Antares_LOG_ERROR, fmt, ##__VA_ARGS__)
#define RPTWRN(fmt, ...) if(RPT_WRN <= rpt_lvl) log_printf(Antares_LOG_WARNING, fmt, ##__VA_ARGS__)
#define RPTINF(fmt, ...) if(RPT_INF <= rpt_lvl) log_printf(ANTARES_LOG_INFO, fmt, ##__VA_ARGS__)
#define RPTDBG(fmt, ...) if(RPT_DBG <= rpt_lvl) log_printf(Antares_LOG_DEBUG, fmt, ##__VA_ARGS__)
#else
#define RPTERR(fmt, ...) if(RPT_ERR <= rpt_lvl) fprintf(stderr, "%s: %s: %d: err: " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define RPTWRN(fmt, ...) if(RPT_WRN <= rpt_lvl) fprintf(stderr, "%s: %s: %d: wrn: " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define RPTINF(fmt, ...) if(RPT_INF <= rpt_lvl) fprintf(stderr, "%s: %s: %d: inf: " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define RPTDBG(fmt, ...) if(RPT_DBG <= rpt_lvl) fprintf(stderr, "%s: %s: %d: dbg: " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#endif
static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */
#else
#define RPTERR(fmt...)
#define RPTWRN(fmt...)
#define RPTINF(fmt...)
#define RPTDBG(fmt...)
#endif

typedef struct {
    TASK_COMMON_MEMBERS;

    tun_trans_dynamic_params params_inside;
    suma_ipc_handle ipc_in;
    suma_ipc_handle ipc_out;

	char tun_name[IFNAMSIZ];
	int tun_fd;
	int *g_tun_fd;
    int serial_id;

	void *buff;
    //void *SndBuff;
} tun_trans_obj;

params_def tun_trans_dynamic_params_def[] = {
    {offsetof(tun_trans_dynamic_params, ctrl), PT_VAR_INT | PT_REAL_TIME, sizeof(int32_t), FUNC_CLOSE, FUNC_OPEN},
    {offsetof(tun_trans_dynamic_params, out_port), PT_VAR_INT | PT_MALLOC_FREE, sizeof(int32_t), 20, 65535},
    {0, 0, 0, 0, 0}
};

tun_trans_dynamic_params glb_tun_trans_dynamic_params_default = 
{
    .ctrl = FUNC_CLOSE,
    .out_port = 65500,
};


extern FILE * popen ( const char * command , const char * type );
extern int pclose ( FILE * stream );

static int module_start_stop(tun_trans_obj *);
static int module_malloc_free(tun_trans_obj *);
static void module_start_stop_delete(tun_trans_obj *);
static void module_malloc_free_delete(tun_trans_obj *obj);
static int module_realtime_set(tun_trans_obj *);
static int module_process(tun_trans_obj *);
static int get_buf_from_ipc(const suma_ipc_handle ipc_in,heap_buf_handle *dst_hbuf);
static int data2ipc(suma_ipc_handle out_ipc, void *in_buff, int len);

/* create module obj */
/*@null@*/tun_trans_handle tun_trans_create(tun_trans_static_params *static_params,
                                              tun_trans_dynamic_params *dynamic_params)
{
    tun_trans_obj *obj;
    tun_trans_dynamic_params * params = NULL;

    obj = pthread_base_create(MODULE_NAME, sizeof(*obj));
    if(NULL == obj || 
       NULL == static_params)
    {
        RPTERR(" tcp_client_create NULL pointer");
        return NULL;
    }

    obj->ipc_in = static_params->ipc_in;
    obj->ipc_out = static_params->ipc_out;
	obj->g_tun_fd = static_params->tun_fd;

    /* param table init */
    obj->tab = tun_trans_dynamic_params_def;
    obj->params_nums = sizeof(tun_trans_dynamic_params_def) /(sizeof(params_def)) - 1;
    params = calloc(1, sizeof(*params));
    if(NULL == params)
    {
        RPTERR(" create NULL pointer");
        pthread_base_delete(obj);
        return NULL;
    }

    if(NULL == dynamic_params)
    {
        memcpy(params, &glb_tun_trans_dynamic_params_default, sizeof(tun_trans_dynamic_params)); 
    }
    else
    {
        memcpy(params, dynamic_params, sizeof(tun_trans_dynamic_params));
    }

    obj->params = params;
	obj->serial_id = 0;
	memset(obj->tun_name,0,IFNAMSIZ);
	obj->tun_name[0] = '\0';
    memset(&obj->params_inside, -1, sizeof(tun_trans_dynamic_params));
    obj->thread_func = NULL;
    obj->stack_size = MODULE_TSK_STACK_SIZE;
    obj->pf_start_stop  = (void *)module_start_stop;
    obj->pf_malloc_free = (void *)module_malloc_free;
    obj->pf_process     = (void *)module_process;
    obj->pf_malloc_free_delete = (void *)module_malloc_free_delete;
    obj->pf_rt_set = (void *)module_realtime_set;
    obj->pf_start_stop_delete = (void *)module_start_stop_delete;

    return obj;
}

static int module_start_stop(tun_trans_obj *obj)
{
    int ret = -1;
    RPTWRN("module_start_stop start\n!");

    if(NULL == obj)
    {
        RPTERR("NULL pointer!");
        return -1;
    }
    obj->buff = malloc(TUN_MAX_BUFF_LEN);
    if(NULL == obj->buff)
    {
        RPTDBG("Malloc failed!");
    }
    else
    {
        RPTDBG("Malloc OK!");
    }

    ret = 0;

    return ret;
}

static void module_start_stop_delete(tun_trans_obj *obj)
{
    RPTWRN("module_start_stop stop\n!");
    if(NULL == obj)
    {
        RPTERR("NULL pointer!");
    }
	
	if(NULL != obj->buff)
	    free(obj->buff);
	
    return;
}
static int tun_trans_device_open(char *dev, int flags)
{
	struct ifreq ifr;
    int fd, err;

    assert(dev != NULL);

    if ((fd = open(TUN_DEVICE_NAME, O_RDWR |O_NOCTTY |O_NDELAY)) < 0)
    {
		RPTERR("tun_trans_device_open fail");
		perror("tun_create");
		return fd;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags |= flags;
    
    if (*dev != '\0')
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
        
    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 )
    {
        close(fd);
        return err;
    }
    
    strcpy(dev, ifr.ifr_name);

    return fd;

}

static int module_malloc_free(tun_trans_obj *obj)
{
    tun_trans_dynamic_params  *params_extern = NULL;
    tun_trans_dynamic_params  *params_inside = NULL;

	int tun, ret;

	char *tun_name = NULL;
	
	FILE* cmdFile;
	char cmd[100] = {0};
	char ip[32] = "10.11.0.101";
    char gw[32] = "10.11.0.1";

    params_extern = obj->params;
    params_inside = &(obj->params_inside);   
    params_inside->out_port = params_extern->out_port;

	//打开虚拟网卡
	tun_name = obj->tun_name;
    //tun_name[0] = '\0';
    tun = tun_trans_device_open(tun_name, IFF_TAP | IFF_NO_PI);
    if (tun < 0) 
	{
		RPTERR("TUN open fail");
		perror("tun_create");
        return -1;
    }
    RPTWRN("TUN name is %s\n", tun_name);

	//成功开启后配置IP
	//sprintf(cmd,"ifconfig %s %s",tun_name,ip);
        sprintf(cmd,"ifconfig %s %s netmask 255.255.255.0 && route add default gw %s",tun_name,ip,gw);
	cmdFile = popen(cmd, "r");
	pclose(cmdFile);
#if 0
        memset(cmd,0,sizeof(cmd));
        sprintf(cmd,"ifconfig add -net 10.11.0.0/24 dev %s",tun_name);
	cmdFile = popen(cmd, "r");
	pclose(cmdFile);
#endif
	//激活网卡
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"ifconfig %s %s up",tun_name,ip);
	cmdFile = popen(cmd, "r");
	pclose(cmdFile);

	
	obj->tun_fd = tun;

	*(obj->g_tun_fd) = tun;

    return 0;
}

static void module_malloc_free_delete(tun_trans_obj *obj)
{
    if(obj->tun_fd > 0)
    {
        close(obj->tun_fd);
    }
    return;
}

static int module_realtime_set(tun_trans_obj *obj)
{
    tun_trans_dynamic_params  *params_extern = NULL;
    tun_trans_dynamic_params  *params_inside = NULL;
    suma_ipc_handle ipc_in = NULL;

    params_extern = obj->params;
    params_inside = &(obj->params_inside);

    params_inside->ctrl = params_extern->ctrl;
    ipc_in = obj->ipc_in;
    suma_ipc_flush(ipc_in);

	suma_mssleep(1000);

    return 0;
}
static int tun_trans_read_ready(int tun_fd, uint32_t msTimeOut)
{
	fd_set fd;
    struct timeval tv;

	int ret = 0;

    if(tun_fd < 2)
	{
		return(-1);
    }

    tv.tv_sec = msTimeOut/1000;
    tv.tv_usec = (msTimeOut%1000)*1000;

    FD_ZERO(&fd);
	FD_SET(tun_fd, &fd);

	ret = select(tun_fd + 1, &fd, NULL, NULL, &tv);

	return ret;
}
static int tun_trans_read(tun_trans_obj *obj)
{
	int len = 0;
	int rd_len = 0;
	int fd = 0;

	void *pBuff = NULL;

	suma_ipc_handle out_ipc = NULL;

	if(NULL == obj)
    {
        RPTERR("NULL pointer!");
		return -1;
    }

	pBuff = obj->buff;
	fd    = obj->tun_fd;
	out_ipc = obj->ipc_out;

	//从虚拟网卡读取数据
	while (tun_trans_read_ready(fd,5) > 0 && rd_len < TUN_MAX_BUFF_LEN)
	{
		len = read(fd,pBuff,TUN_MAX_BUFF_LEN - rd_len);
		if (len < 0)
		{
			RPTERR("tun_read_data fail");
            perror("tun read fail");
			return -1;
		}

		pBuff += len;
		rd_len += len;
	}

	//将虚拟网卡数据写入IPC
	if (rd_len != 0)
	{
		//data2ipc(out_ipc, obj->buff, rd_len);
        serialWrite(obj->serial_id,obj->buff,rd_len);
		//RPTWRN("tun_read_data [buff]%s,[rd_len]%d",obj->buff,rd_len);
	}
	
	return 0;
}

static int tun_trans_write(tun_trans_obj *obj)
{
	int len = 0;
	int fd = 0;
	
	int buf_size = 0;
	heap_buf_handle hbuf = NULL;
	uint8_t *buf_ptr = NULL;

	void *pBuff = NULL;
	suma_ipc_handle ipc_in = NULL;

	pBuff = obj->buff;
	fd    = obj->tun_fd;
	ipc_in = obj->ipc_in;

	//将IPC数据写入虚拟网卡
	while(suma_ipc_dst_get_nums(ipc_in) > 0)
	{
	    get_buf_from_ipc(ipc_in, &hbuf);
	    buf_ptr = heap_buf_get_useptr(hbuf);
	    buf_size = heap_buf_get_usedsize(hbuf);
	    if (buf_size == 0)
	    {
			RPTWRN("\n ipc_indata snd len == 0\n"); 
			continue;
	    }
	    len = write(fd, buf_ptr, buf_size );
        if (len != buf_size)
        {
            RPTWRN("tun write data fail :[buff]%s,[len]%d,[buf_size]%d",buf_ptr,len,buf_size);            
            //perror("tun write fail");
            break;
        }

        //RPTWRN("tun write data :[buff]%s,[len]%d",buf_ptr,len);
	    suma_ipc_dst_release(ipc_in, hbuf);
	}
	
	return 0;
}
static int module_process(tun_trans_obj *obj)
{
	//tun_trans_write(obj);

	tun_trans_read(obj);

	//suma_mssleep(100);

	return 0;
}
static int get_buf_from_ipc(const suma_ipc_handle ipc_in,heap_buf_handle *dst_hbuf)
{    
    int get_num = 1; 
    heap_buf_handle hbuf;

    if(ipc_in == NULL)
    {
        RPTERR("fpga_snd_create_buffer NULL pointer");
        return -1;
    }

    if(suma_ipc_dst_get_nums(ipc_in) <= 0)
    {
        /* wait for the TS data in */
        suma_mssleep(5);
        while(get_num < 5) 
        { 
            if(suma_ipc_dst_get_nums(ipc_in) <= 0)
            { 
                suma_mssleep(5);
                get_num++;
            }
            else
            {
                break;
            }
        }
        if(get_num >= 5)
        {   
            RPTDBG("ipcin fifo1 is empty,discard fifo0 first buffer(%d %d)",suma_ipc_dst_get_nums(ipc_in),
                   suma_ipc_src_get_nums(ipc_in));
            return -1;
        }
        suma_ipc_dst_get(ipc_in,&hbuf);
    }
    else
    {
        suma_ipc_dst_get(ipc_in,&hbuf);
    }

    *dst_hbuf = hbuf;
    return 0;
}

static int data2ipc(suma_ipc_handle out_ipc, void *in_buff, int len)
{
    int err = -1;
    int time_out;
    heap_buf_handle heap_buf = NULL;
    uint8_t *dst_buff = NULL;

    if( NULL == in_buff )
    {
        RPTWRN( "Invalid para");
        return -1;
    }

    time_out = 5;
    while( (out_ipc) &&
           (suma_ipc_src_get_nums(out_ipc)<=0) && 
           time_out--
    )
    {
        suma_mssleep(5);
    }

    if(suma_ipc_src_get_nums(out_ipc)<=0)
    {
        RPTDBG( "get out ipc failed");
        err = -1;
    }
    else
    {
        suma_ipc_src_get(out_ipc, &heap_buf);
        dst_buff = heap_buf_get_useptr(heap_buf);
        memcpy(dst_buff, in_buff, len);
        heap_buf_set_usedsize(heap_buf, len);
        suma_ipc_src_release(out_ipc, heap_buf);
        err = 0;
    }

    return err;
}


