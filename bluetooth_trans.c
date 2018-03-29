#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include "bluetooth_trans.h"
#include "suma_api.h"
//#include "ring_buffer.h"
//#include "bluetooth_trans_sync.h"

//此三个头文件必需
#include "pthread_base.h"
#include "davinci_base.h"
#include "suma_osa_thr.h"

//
/****************************************************************************************
 *                        Define
 ****************************************************************************************/
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
static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */
#else
#define RPTERR(fmt...)
#define RPTWRN(fmt...)
#define RPTINF(fmt...)
#define RPTDBG(fmt...)
#endif

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

#ifndef NULL
#define NULL 0
#endif

#define MODULE_NAME     "bluetooth_trans"
#define BLUETOOTH_TRANS_STACK_SIZE  (10*1024)

#define HCI_MAX_DEV 16

#define L2CAP_BUFF_MAX 1024*10

#define BLUETOOTH_BUFF_MAX (10*1024)

#define RING_BUFFER_LEN  (10*4096)
/****************************************************************************************
 *                         static
 ****************************************************************************************/
typedef struct {
    TASK_COMMON_MEMBERS;

    suma_ipc_handle ipc_in;
    suma_ipc_handle ipc_out;

	/*now only support "ttyO" or "ttyS"*/
	char serial_name[MAX_LEN_SERIAL_NAME]; 
	
    int rcv_len;
	int snd_len;//蓝牙模块缓冲区大小
    int socket_fd;
    void *RecvBuff;
	void *SndBuff;

	int *g_tun_fd;

	int sync_flag;
//	dev_cmd_head_t ack_cmd;
	//ring_buffer_handle ring_h;
} bluetooth_trans_obj;

params_def bluetooth_trans_dynamic_params_def[] = 
{
    {offsetof(bluetooth_trans_dynamic_params, ctrl), PT_VAR_INT | PT_REAL_TIME, sizeof(int32_t), FUNC_CLOSE, FUNC_OPEN},
    {offsetof(bluetooth_trans_dynamic_params, out_port), PT_VAR_INT | PT_MALLOC_FREE, sizeof(int32_t), 20, 65535},
    {0, 0, 0, 0, 0}
};

bluetooth_trans_dynamic_params glb_bluetooth_trans_dynamic_params_default = 
{
    .ctrl = FUNC_CLOSE,
    .out_port = 65500,
    
    .baud_rate = 9600,
    .serial_id = 0,
};

/****************************************************************************************
 *                        Function
 ****************************************************************************************/
 #if 0
 /*Host上插入Dongle数目以及Dongle信息*/
static int hci_host_dongle_info_get(hci_dev_info *DevInfo)
{
	struct hci_dev_list_req *dl;

	struct hci_dev_req *dr;

	struct hci_dev_info di;

	int ret = 0;
	int socket_fd = 0;
	int devReqIndex = 0;

	// 打开一个HCI socket
	if ((socket_fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0) 
	{
		RPTERR("hci socket open fail");
		perror("Can't open HCI socket.");
		return -1;
	}

	//分配一个空间给 hci_dev_list_req。这里面将放所有Dongle信息
	dl = malloc(HCI_MAX_DEV * sizeof(struct hci_dev_req) + sizeof(uint16_t))
	if (NULL == dl) 
	{
		RPTERR("hci_dev_list_req malloc fail");
		perror("Can't allocate memory");
		ret = -1;
		goto closed;
	}

	dl->dev_num = HCI_MAX_DEV;
	//使用HCIGETDEVLIST,得到所有dongle的Device ID。存放在dl中
	if (ioctl(socket_fd, HCIGETDEVLIST, (void *) dl) < 0)
	{
		RPTERR("get device list fail");
		perror("Can't get device list");
		ret = -1;
		goto failed;
	}
	
	//使用HCIGETDEVINFO，得到对应Device ID的Dongle信息。一般系统只有一个适配器
	dr = dl->dev_req;
	devReqIndex = dl->dev_num-1;
	di.dev_id = (dr+ devReqIndex)->dev_id;
	if (ioctl(socket_fd, HCIGETDEVINFO, (void *) &di) < 0)
	{
		perror("Can't get device info");
		ret = -1;
		goto failed;
	}

	/* Start HCI device */
	if (ioctl(socket_fd, HCIDEVUP, di.dev_id) < 0)
	{
		if (errno == EALREADY)
		{
			RPTERR(stderr, "Can't init device hci%d: %s (%d)\n",hdev, strerror(errno), errno);
		}
		goto failed;
	}
	
	*DevInfo = di;
failed:
	free(dl);
closed:
	close(socket_fd);
	return ret;
}

//Inquiry Scan状态表示设备可被inquiry. Page Scan状态表示设备可被连接。
static int hci_enable_scan(int fd, int hdev)
{
	struct hci_dev_req dr;

	dr.dev_id  = hdev;
	dr.dev_opt = SCAN_PAGE | SCAN_INQUIRY;

	if (ioctl(fd, HCISETSCAN, (unsigned long) &dr) < 0)
	{
		RPTERR(stderr, "Can't set scan mode on hci%d: %s (%d)\n",
						hdev, strerror(errno), errno);
		return -1;
	}
	return 0;
}

//加密认证
static int hci_enable_authentication(int fd, int hdev)
{
	struct hci_dev_req dr;

	dr.dev_id  = hdev;
	dr.dev_opt = AUTH_ENABLED;

	if (ioctl(fd, HCISETAUTH, (unsigned long) &dr) < 0)
	{
		RPTERR(stderr, "Can't set auth on hci%d: %s (%d)\n",
						hdev, strerror(errno), errno);
		return -1;
	}
	return 0;
}
static int hci_local_dongle_init(bluetooth_trans_obj *obj,int dev_id,int socket_fd)
{
	int ret = 0;
	
	//reset
	ioctl(socket_fd, HCIDEVRESET, dev_id);

	//可扫描可被连接
	ret = hci_enable_scan(socket_fd,dev_id);
	if (ret < 0)
	{
		RPTERR("hci enable scan fail");
		return -1;
	}

	//加密认证
	ret =hci_enable_authentication(socket_fd,dev_id);
	if (ret < 0)
	{
		RPTERR("hci enable authentication fail");
		return -1;
	}

	

}

//获取本地可用的蓝牙适配器识别号；搜寻周围蓝牙设备；获取周围蓝牙设备名称
static int hci_remote_device_name_get()
{
	inquiry_info *info = NULL;
    int max_rsp, num_rsp;
    int dev_id, sock, len, flags;
    int i;
    char addr[19] = { 0 };
    char name[248] = { 0 };
	
	dev_id = hci_get_route(NULL);
    sock = hci_open_dev( dev_id );
    if (dev_id < 0 || sock < 0) 
	{
        perror("opening socket");
        return -1;
    }

/* 
	查询时间最长持续1.28 * len秒。
	max_rsp 个设备返回的信息都被存储在变量ii中，
	这个变量必须有足够的空间来存储max_rsp返回的结果。
	我们推荐max_rsp取值255来完成标准10.24秒(Len = 8)的查询工作。
*/
	len = 8;
    max_rsp = 255;
/*
	进行查询操作时会把先前一次查询记录的cache刷新，
	否则flag设置为0的话，即便先前查询的设备已经不处于有效范围内，先前查询的记录也将被返回。
*/
    flags = IREQ_CACHE_FLUSH;
    info = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
    num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &info, flags);
    if( num_rsp < 0 ) perror("hci_inquiry");

    for (i = 0; i < num_rsp; i++) {
        ba2str(&(info+i)->bdaddr, addr);
        memset(name, 0, sizeof(name));
        if (hci_read_remote_name(sock, &(info+i)->bdaddr, sizeof(name),
            name, 0) < 0)
        strcpy(name, "[unknown]");
        printf("%s %s\n", addr, name);
    }

    free( info );
    hci_close_dev(sock);
}
static void bluetooth_trans_socket_close(int socket_fd)
{
	if(socket_fd > 0)
		close(socket_fd);
}
static int bluetooth_trans_start_stop(bluetooth_trans_obj *obj)
{

	int ret = -1;

    if(NULL == obj)
    {
        RPTERR("NULL pointer!");
        return -1;
    }
	
    obj->RecvBuff = malloc(L2CAP_BUFF_MAX);
    if(NULL == obj->RecvBuff)
    {
        RPTWRN("Malloc failed!");
		return -1;
    }
	obj->rcv_len = 0;

	 obj->SndBuff = malloc(L2CAP_BUFF_MAX);
    if(NULL == obj->SndBuff)
    {
        RPTWRN("Malloc failed!");
		return -1;
    }

    ret = 0;

    return ret;
}

static void bluetooth_trans_start_stop_delete(bluetooth_trans_obj *obj)
{
	if(NULL == obj)
    {
        RPTERR("NULL pointer!");
    }

    free(obj->RecvBuff);
	free(obj->SndBuff);
}
/*建立物理链接*/
static int bluetooth_trans_malloc_free(bluetooth_trans_obj *obj)
{
	int ret = 0;
	int bus = 0;
	int socket_fd = 0;
	int dev_id = 0;

	hci_dev_info DevInfo;

	memset((char*)(&DevInfo),0,sizeof(hci_dev_info));
	//获取本地usb dongle 信息
	ret = hci_host_dongle_info_get(&DevInfo);
	if (ret < 0)
	{
		suma_mssleep(500);
		return -1;
	}
	//type低四位表示硬件接入类型，usb uart
	bus = DevInfo.type & 0x0f;
	if (bus != HCI_USB)
	{
		suma_mssleep(500);
		return -1;
	}

	dev_id = DevInfo.dev_id;
	socket_fd = hci_open_dev(dev_id);

	//本地usb dongle 初始化
	ret = hci_local_dongle_init(obj,socket_fd,&DevInfo);
	if (ret < 0)
	{
		RPTERR("hci local dongle init fail");
		return -1;
	}

	//搜寻周围设备并建立链接
	
	
	return 0; 
}
static void bluetooth_trans_malloc_free_delete(bluetooth_trans_obj *obj)
{
	bluetooth_trans_socket_close(obj->socket_fd);
}

static int bluetooth_trans_L2cap_socket_create(bluetooth_trans_obj *obj)
{
	int ret = 0;  
    int sk = 0;
	int optval = 1;
	
    struct sockaddr_l2 local_addr; 

	struct l2cap_options opts;  
    int optlen = 0;  
	
	sk = socket(PF_BLUETOOTH, SOCK_SEQPACKET,BTPROTO_L2CAP);  //发送数据，使用SOCK_SEQPACKET为好  
    if(sk < 0)  
    {  
        perror("\nsocket():");  
        return -1;  
    }
#if 0
	if(setsockopt(sk, SOL_L2CAP, SO_REUSEADDR, 
                  (const void *)&optval, sizeof(optval)) < 0)
    {
        RPTWRN("setsockopt SO_REUSEADDR failed");
        close(listen_fd);
        return -1;
    }
#endif    
    //bind
	memset(&local_addr, 0, sizeof(struct sockaddr_l2)); 
    local_addr.l2_family = PF_BLUETOOTH;  
    local_addr.l2_psm = htobs(0x1001);  //last psm  
    bacpy(&local_addr.l2_bdaddr,BDADDR_ANY); 
	char addr[100] = {0};
	ba2str(&local_addr.l2_bdaddr,addr);
	RPTWRN("l2_bdaddr : %s",addr);
    ret = bind(sk, (struct sockaddr*)&local_addr, sizeof(struct sockaddr));  
    if(ret < 0)  
    {  
        perror("\nbind()");  
        return -1; 
    }  
     
    //get opts  
    // in mtu 和 out mtu.每个包的最大值  
    memset(&opts, 0, sizeof(opts));  
    optlen = sizeof(opts);  
    getsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS,&opts, &optlen);  
    RPTWRN("\nomtu:[%d]. imtu:[%d].flush_to:[%d]. mode:[%d]\n", opts.omtu, opts.imtu, opts.flush_to,opts.mode);  
     
    //set opts. default value  
    opts.omtu = 0;  
    opts.imtu = 672;  
    if (setsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS,&opts, sizeof(opts)) < 0)  
    {  
        perror("\nsetsockopt():");  
        return -1;   
    }

	obj->socket_fd = sk;

	return 0;
}
static int bluetooth_trans_realtime_set(bluetooth_trans_obj *obj)
{
	int ret = 0;
	
	suma_ipc_handle ipc_in = NULL;

	//清空buff
	ipc_in = obj->ipc_in;
    suma_ipc_flush(ipc_in);


	//L2CAP层 socket 创建 绑定到一个usb dongle
	ret = bluetooth_trans_L2cap_socket_create(obj);
	if(ret < 0)  
    {  
        perror("\nlisten()"); 
		//重新创建socket
		obj->malloc_free_rst = DEF_RST;
        return -1;    
    }  
	
	//listen  
    ret = listen(obj->socket_fd, 10);  
    if(ret < 0)  
    {  
        perror("\nlisten()"); 
		//重新创建socket
		obj->malloc_free_rst = DEF_RST;
        return -1;    
    }  
    return 0;
}
static int bluetooth_trans_snd_data(bluetooth_trans_obj *obj,int nsk)
{
	void *pBuff = NULL;
	int SndDataLen = 0;

	suma_ipc_handle ipc_in = NULL;

	if(NULL == obj)
    {
        RPTERR("NULL pointer!");
		return -1;
    }

	ipc_in = obj->ipc_in;
	pBuff = obj->SndBuff;

	//发送数据
	while(suma_ipc_dst_get_nums(ipc_in) > 0)
	{
		datafromipc(ipc_in,pBuff,&SndDataLen);
		if (SndDataLen == 0)
		{
			RPTWRN("\n ipc_indata snd len == 0\n"); 
			continue;
		}

		send(nsk, pBuff, SndDataLen, 0);
	}

	return 0;
}
static int bluetooth_trans_read_data(bluetooth_trans_obj *obj,int nsk)  
{  
    //struct pollfd fds[10];  
    struct  pollfd   fds[100];

	suma_ipc_handle ipc_out = NULL;

	void *pBuff = NULL;
	int RecvDataLen = 0;
	int BuffSize = 0;
     
    fds[0].fd   = nsk;  
    fds[0].events = POLLIN | POLLHUP;

	if(NULL == obj)
    {
        RPTERR("NULL pointer!");
		return -1;
    }

	ipc_out= obj->ipc_out;
	pBuff = obj->RecvBuff;
	BuffSize = L2CAP_BUFF_MAX;
	RecvDataLen = 0;
	memset(pBuff, 0 , obj->rcv_len); 
	obj->rcv_len = 0;
    while(RecvDataLen < L2CAP_BUFF_MAX)  
    {  
		// poll可以查出连接断连，但需要注意：断开的revent值为：11001B。也就是说：POLLIN |POLLERR |POLLHUP
        if(poll(fds, 1, -1) < 0)  
        {  
            perror("\npoll():");  
        }  
        if(fds[0].revents & POLLHUP)  
        {  
            //hang up  
            RPTWRN("\n[%d] Hang up\n",nsk);    
            return -1;  
        }  
        if((fds[0].revents & POLLIN) == 0)  
        {   
	        RPTWRN("\n[%d] poll not in\n",nsk);
            return -1;
        }

		//read data  
        RecvDataLen = recv(nsk, pBuff, BuffSize,0);
		if(RecvDataLen < 0)
		{
			RPTERR("recv data error");
			return -1;
		}
		if (RecvDataLen == 0)
		{
			RPTERR("recv data finished");
			break;
		}
		RPTWRN("recv data %s",(char*)pBuff);

		obj->rcv_len += RecvDataLen;
		if(RecvDataLen < BuffSize)
		{
			pBuff += RecvDataLen;
			BuffSize -= RecvDataLen;
			RecvDataLen = 0;
		}
	
	} 

	if (obj->rcv_len == 0)
	{
		return 0;
	}
	//若接收到数据，写入IPC
	data2ipc(ipc_out,obj->buff,obj->rcv_len);
	
    return 0;  
}  
static int bluetooth_trans_process(bluetooth_trans_obj *obj)
{
	int nsk = 0;  
	int ret = 0;
	
    char str[16] = {0};
	
	struct sockaddr_l2 remote_addr;
	int addr_len = 0;

	addr_len = sizeof(struct sockaddr_l2);
	memset(&remote_addr, 0, addr_len);  
    nsk = accept(obj->socket_fd, (struct sockaddr*)(&remote_addr), &addr_len);  
    if(nsk < 0)  
    {  
        perror("\naccept():");
		//重新监听
		obj->real_time_rst = DEF_RST;
        return -1;  
    }  
    ba2str(&(remote_addr.l2_bdaddr),str);  
    RPTWRN("\npeerbdaddr:[%s].\n", str);  //得到peer的信息

	//将ipc数据 发送 到蓝牙模块
	ret = bluetooth_trans_snd_data(obj,nsk);

	//从usb 蓝牙模块 读取数据 到 ipc
	ret = bluetooth_trans_read_data(obj,nsk);
	if (ret < 0)
	{
		//重新监听
		obj->real_time_rst = DEF_RST;
		bluetooth_trans_socket_close(nsk);
		return -1; 
	}

	return 0;
}
static void bluetooth_trans_socket_close(int socket_fd)
{
	if(socket_fd > 0)
		close(socket_fd);
}
#endif

static int data2ipc(suma_ipc_handle out_ipc, void *RecvBuff, int RecvLen)
{
    int err = -1;
    int time_out;
    heap_buf_handle heap_buf;
    uint8_t *dst_buff = NULL;

    if( NULL == RecvBuff )
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
        memcpy(dst_buff, RecvBuff, RecvLen);
        heap_buf_set_usedsize(heap_buf, RecvLen);
        suma_ipc_src_release(out_ipc, heap_buf);
        err = 0;
    }

    return err;
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

    if(suma_ipc_dst_get_nums(ipc_in) <= 0)//get the num of buffer where contains TS data and judge whether there is TS data need to be send to fpga or not 
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
static void datafromipc(suma_ipc_handle ipc_in,void *SndBuff,int *snd_len)
{
	heap_buf_handle hbuf;
	uint8_t *buf_ptr = NULL;
	int buf_size = 0;
	
    get_buf_from_ipc(ipc_in, &hbuf);
    buf_ptr = heap_buf_get_useptr(hbuf);
    buf_size = heap_buf_get_usedsize(hbuf);
    if(0 != buf_size)
    {
        memcpy(SndBuff, buf_ptr, buf_size);
    }
	*snd_len = buf_size;
    suma_ipc_dst_release(ipc_in, hbuf);
}

static int bluetooth_trans_start_stop(bluetooth_trans_obj *obj)
{
	int ret = -1;

    if(NULL == obj)
    {
        RPTERR("NULL pointer!");
        return -1;
    }

   // obj->ring_h = ring_buffer_create(RING_BUFFER_LEN);
   // if(NULL == obj->ring_h)
    {
       // goto failed;
    }
    ret = 0;

//failed:
    return ret;
}

static void bluetooth_trans_start_stop_delete(bluetooth_trans_obj *obj)
{
	if(NULL == obj)
    {
        RPTERR("NULL pointer!");
    }

   // if(NULL != obj->ring_h)
    {
       // ring_buffer_destroy(obj->ring_h);
    }

    return;
}
static int bluetooth_trans_uart_ioctrl_set(int h, int baud_rate)
{
    int ret = 0;
    /*38400 baud rate*/
    ret = serialIoctl(h, SL_BAUD_SET, baud_rate);
    /*8 data bits*/
    ret |= serialIoctl(h, SL_DBIT_SET, 8);
    /*no parity*/
    ret |= serialIoctl(h, SL_PAR_SET, 0);
    /*1 stop bits*/
    ret |= serialIoctl(h, SL_STB_SET, 1);

    return ret;
}
//create a session;
static int bluetooth_trans_uart_open(bluetooth_trans_obj * handle)
{
	
    int serial_id = 0;
	int baud_rate = 0;
		
	char *serial_name = NULL;
	serialAttr *s = NULL;
	bluetooth_trans_dynamic_params *params = NULL; 

    if (NULL == handle)
	{
	    RPTERR( "bluetooth_trans_obj NULL pointer!\n");		
        return -1;
	}

	params = (handle->params);
	serial_name = handle->serial_name;
	
	//set the serival params
	baud_rate = params->baud_rate; 
	serial_id = params->serial_id;
	
    s = serialGet(serial_id);
    if(!s->opened)
	{
        if(serialOpen(0, serial_id, serial_name) < 0)
		{
            RPTERR("serialOpen@ttyO failed..%s%d\n",serial_name,serial_id);
            return -1;
        }

		if(bluetooth_trans_uart_ioctrl_set(serial_id, baud_rate))
		{
            RPTERR("setTtyAttr@tty 0failed...\n");
            return -1;
        }
		RPTWRN("serial Open @ttyO %s%d %d success..\n",serial_name,serial_id,baud_rate);
    }
    return 0;
}
static void bluetooth_trans_uart_close(bluetooth_trans_obj *h)
{
    serialAttr *s = NULL;

	bluetooth_trans_dynamic_params *params = NULL; 

    if(NULL == h) 
	{
		return;
	}

	params = (h->params);

    s = serialGet(params->serial_id);
    if(s->opened)
	{
        serialClose(params->serial_id);
    }
}

static int bluetooth_trans_uart_read_ready(int serial_fd, uint32_t msTimeOut)
{
	fd_set fd;
    struct timeval tv;

	int ret = 0;

    if(serial_fd < 2)
	{
		return(-1);
    }

    tv.tv_sec = msTimeOut/1000;
    tv.tv_usec = (msTimeOut%1000)*1000;

    FD_ZERO(&fd);
	FD_SET(serial_fd, &fd);

	ret = select(serial_fd + 1, &fd, NULL, NULL, &tv);

	return ret;
}
static int bluetooth_trans_snd_data(bluetooth_trans_obj *obj,int nsk)
{
	suma_ipc_handle ipc_in = NULL;
	heap_buf_handle hbuf = NULL;
	uint8_t *buf_ptr = "serial port sumavision 3";
	int buf_size = 0;

	bluetooth_trans_dynamic_params *params = NULL;

	if(NULL == obj)
    {
        RPTERR("NULL pointer!");
		return -1;
    }

	ipc_in = obj->ipc_in;
	params = (bluetooth_trans_dynamic_params *)(obj->params);
    buf_size = strlen(buf_ptr);

    serialWrite(params->serial_id,(char*)buf_ptr,buf_size);
    RPTWRN("uart write data :[buff]%s,[len]%d",buf_ptr,buf_size);
#if 0
	if(suma_ipc_dst_get_nums(ipc_in) <= 0)
    {
        suma_mssleep(100);
        return 0; 
    }

	//发送数据
	while(suma_ipc_dst_get_nums(ipc_in) > 0)
	{
	    get_buf_from_ipc(ipc_in, &hbuf);
	    buf_ptr = heap_buf_get_useptr(hbuf);
	    buf_size = heap_buf_get_usedsize(hbuf);
		if(0 == buf_size)
		{
			RPTWRN("\n ipc_indata snd len == 0\n"); 
			continue;
		}

        serialWrite(params->serial_id,(char*)buf_ptr,buf_size);
		//RPTWRN("uart write data :[buff]%s,[len]%d",buf_ptr,buf_size);
	   
	    suma_ipc_dst_release(ipc_in, hbuf);
	}
#endif	
	return 0;
}
static int bluetooth_trans_read_data(bluetooth_trans_obj *obj,int nsk)  
{  
	int ret = 0;
	int rd_len = 0;
	int len = 0;

	uint8_t sync_buf;

	suma_ipc_handle ipc_out = NULL;

	char *pRecv = NULL;
	
	serialAttr *s = NULL;

	bluetooth_trans_dynamic_params *params = NULL; 
     
	if(NULL == obj)
    {
        RPTERR("NULL pointer!");
		return -1;
    }

	ipc_out= obj->ipc_out;
	params = (obj->params);
	pRecv = (char *)obj->RecvBuff;
	s = serialGet(params->serial_id);
	while (bluetooth_trans_uart_read_ready(s->serialFd,5) > 0 && rd_len < BLUETOOTH_BUFF_MAX)
	{
		len = serialRead(params->serial_id, pRecv,BLUETOOTH_BUFF_MAX - rd_len);
		if(len < 0)
		{
			//printf("GetUcmpMsg@serialRead error...\n");
			RPTDBG( "GetUcmpMsg@serialRead error...\n");
			return -1;
		} 
		
		if (len == 0)
		{
			RPTDBG( "GetUcmpMsg@serialRead error...\n");
			suma_mssleep(10);
			continue;
		}

		pRecv += len;
		rd_len += len;
	}

	if (0 != rd_len)
	{
		//data2ipc(ipc_out, obj->RecvBuff, rd_len);
		//len = write(*(obj->g_tun_fd), obj->RecvBuff, rd_len );
		RPTWRN( "bluetooth recv data : [RecvBuff]%s,[rd_len]%d\n",obj->RecvBuff,rd_len);
	}
	
    return 0;  
}  
static int bluetooth_trans_process(bluetooth_trans_obj *obj)
{
	int nsk = 0;  
	int ret = 0;

	//将ipc数据 发送 到蓝牙模块
	ret = bluetooth_trans_snd_data(obj,nsk);

	//从 蓝牙模块 读取数据 到 ipc
	ret = bluetooth_trans_read_data(obj,nsk);

	//suma_mssleep(100);

	return 0;
}
static void bluetooth_trans_uart(bluetooth_trans_handle h)
{
	int ret = 0;
	
    bluetooth_trans_obj * handle = h;  

    if (NULL == h)
    {
        RPTERR( "bluetooth_trans_uart NULL pointer!\n");
        goto cleanup;
    }
	
    handle->exit_ctl = FALSE;
    handle->exit_done_flg = FALSE;
    
    while (handle->exit_ctl == FALSE) /*lint warning modified*/
	{  
        handle->start_stop_rst = DEF_REMAIN;

		ret = bluetooth_trans_start_stop(handle);
		if (ret < 0)
		{
            RPTERR("bluetooth_trans_uart ring buff failed.");
			suma_mssleep(500);
			continue;
		}
		
        RPTDBG("bluetooth_trans_uart start_stop_rst start");
        while (handle->start_stop_rst == DEF_REMAIN)
		{
            handle->malloc_free_rst = DEF_REMAIN;
            RPTWRN("bluetooth_trans_uart malloc_free_rst start");
			
            while (handle->malloc_free_rst == DEF_REMAIN)
			{			
				ret = bluetooth_trans_uart_open(handle);
				if (ret < 0)
				{
                    RPTERR("bluetooth_trans_uart uart open failed.");
					suma_mssleep(500);
					continue;
    			}
				
                handle->real_time_rst = DEF_REMAIN;
                RPTWRN("bluetooth_trans_uart real_time_rst start");
				
                while (handle->real_time_rst == DEF_REMAIN)
                {
					bluetooth_trans_process(handle);
                }
				bluetooth_trans_uart_close(handle);
            }  
        }
		bluetooth_trans_start_stop_delete(handle);

    }

    cleanup:
    suma_mssleep(10);    
    handle->exit_done_flg = TRUE;
    RPTWRN("pthread_uart uart pthread end");
}

bluetooth_trans_handle bluetooth_trans_creat(bluetooth_trans_static_params *static_params,
	                                              bluetooth_trans_dynamic_params *dynamic_params)
{
	bluetooth_trans_obj *obj;
	bluetooth_trans_dynamic_params * params = NULL;

	char omap_name[] = "ttyO";
	char nomal_name[] = "ttyS";

    obj = pthread_base_create(MODULE_NAME, sizeof(*obj));
    if(NULL == obj || NULL == static_params)
    {
        RPTERR(" tcp_client_create NULL pointer");
        return NULL;
    }

    obj->ipc_in = static_params->ipc_in;
    obj->ipc_out = static_params->ipc_out;
	obj->g_tun_fd = (static_params->g_tun_fd);

    /* param table init */
    obj->tab = bluetooth_trans_dynamic_params_def;
    obj->params_nums = sizeof(bluetooth_trans_dynamic_params_def) /(sizeof(params_def)) - 1;
    params = calloc(1, sizeof(*params));
    if(NULL == params)
    {
        RPTERR(" create NULL pointer");
        pthread_base_delete(obj);
        return NULL;
    }

    if(NULL == dynamic_params)
    {
        memcpy(params, &glb_bluetooth_trans_dynamic_params_default, sizeof(bluetooth_trans_dynamic_params)); 
    }
    else
    {
        memcpy(params, dynamic_params, sizeof(bluetooth_trans_dynamic_params));
    }

    obj->params = params;
    strcpy(obj->serial_name,static_params->serial_name);
#if 0
	 /*only support ttyO or ttyS*/
    if((strcmp(nomal_name, static_params->serial_name) == 0 )|| (strcmp(omap_name, static_params->serial_name) == 0 ) )
    {
        strcpy(obj->serial_name,static_params->serial_name);
    }
    else
    {
        RPTWRN( "serial name should be %s or %s", omap_name,nomal_name );
		free(params);
		pthread_base_delete(obj);
		return NULL;
    }
#endif
	obj->RecvBuff = malloc(BLUETOOTH_BUFF_MAX);
    if(NULL == obj->RecvBuff)
    {
	    free(params);
		pthread_base_delete(obj);
        RPTWRN("Malloc RecvBuff failed!");
		return NULL;
    }
	obj->rcv_len = 0;

	 obj->SndBuff = malloc(BLUETOOTH_BUFF_MAX);
    if(NULL == obj->SndBuff)
    {
	    free(params);
		pthread_base_delete(obj);
        RPTWRN("Malloc SndBuff failed!");
		return NULL;
    }
	obj->snd_len = 1;


	obj->thread_func = (SUMA_ThrEntryFunc)bluetooth_trans_uart;
    obj->stack_size = BLUETOOTH_TRANS_STACK_SIZE;
	
    return obj;
}

