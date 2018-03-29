
/**********************************************************
 Copyright (C), 2001-2011, Sumavision Tech. Co., Ltd.
 FileName: serial.h
 Author/Version/Date: rd5/V1.0/2010.4.12
 Description:     定义一些串口驱动用到的命令字
 	数据结构.
 Version:         
 History:         
 *********************************************************/
 
#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <termios.h>

typedef struct serialAttr{
	int serialFd;	/*串口描述符*/
	int socketRvFd;/*串口透传接收socket*/
	int socketSdFd;/*串口透传发送socket*/
	int opened;/*串口是否成功打开*/
	int usage; /*串口用途，暂未用到*/
	struct termios attr;	/*串口属性，暂未用到*/
}serialAttr;

/**********************************************************
*	命令字
*/

#define SERIALSETOPTION	TCSANOW
#define	FLUSHMETHOD		TCOFLUSH

#define SL_BAUD_SET		0x01
#define SL_DBIT_SET 		0x02
#define SL_PAR_SET		0x03
#define SL_STB_SET		0x04
#define SL_FLOW_SET		0x05
#define SL_USE_SET		0x06
#define SL_MODE_SET		0x08

#define SL_BAUD_GET		0x10
#define SL_DBIT_GET 		0x20
#define SL_PAR_GET		0x30
#define SL_STB_GET		0x40
#define SL_FLOW_GET		0x50
#define SL_USE_GET		0x60
#define SL_MODE_GET		0x80

#define SL_USE_CONSL		0x01
#define SL_USE_IO			0x02

#define SL_CLOSE_IO		0xA0

int serialOpen(unsigned int type, int idx, char * ttyname);
int serialIoctl(unsigned int h, unsigned int cmd, unsigned int arg);
int serialRead(unsigned int h, char * buf, int len);
int serialWrite(unsigned int h, char * buf, int len);
int serialClose(unsigned int h);
serialAttr * serialGet(unsigned int h);

extern serialAttr serialVar[];

#endif
