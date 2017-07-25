/*----------------------------------------------------------------
File Name  : videoSend.c
Author     : Winglab
Data	   : 2016-12-29
Description: 
	Set the I/O interface and start the vedio realtime 
transmit program.
------------------------------------------------------------------*/
#include"cross_platform.h"
#include"ringbuffer.h"
#include"transmitter.h"
#include"receiver.h"
#include"io.h"
#include"err.h"

iThread_t	hThreadRecv;

static netInfo_t* sockSend=NULL;

iThreadStdCall_t RecvThread(LPVOID lpParam)
{
	Data_t myData;
	IODataInfo_t myDataInfo;
	DataFb_t myDataFb;

	while (1)
	{
		IO_OutputData(&myDataInfo);
		memcpy(&myData, myDataInfo.data, myDataInfo.len);
		myDataFb.id = myData.id;
		myDataFb.ts = myData.ts;
		NetworkUDPSend(sockSend,&myDataFb,sizeof(myDataFb));
		//debug("[DATA_OUT] id=%d ,strLen:%d\r\n",myData.id,  myData.len);
	}

	return 0;
}

void main() 
{

#ifdef OS_LINUX	
	initStartTime();
#endif

#ifdef IO_NETWORK_MODE
	#ifdef OS_WINDOWS
		//WinExec("C:\\Program Files (x86)\\VideoLAN\\VLC\\vlc.exe udp://@127.0.0.1:1150  :network-caching=2000", SW_SHOWNORMAL);
	#endif
#endif // IO_NETWORK_MODE
#ifndef  IO_NETWORK_MODE
	netAddr_t addrSend, addrRecv;
	strcpy(addrSend.ip,IP_RX_0);
	addrSend.port = 3510;
	strcpy(addrRecv.ip, IP_TX_0);
	addrRecv.port = 3520;
	sockSend = NetworkUDPInit(&addrSend, &addrRecv);	
#endif
	ReceiverInit();
	printf("ReceiverInit ok!\r\n");
#ifndef  IO_NETWORK_MODE
	iCreateThread(hThreadRecv, RecvThread,0);  // returns the thread identifier 
#endif
#ifdef IO_NETWORK_MODE
	#ifdef OS_WINDOWS
		#ifdef X64
			system("E:\\lab\\sw\\RQS\\x64\\Debug\\vlcR.bat");
		#else
			//system("E:\\lab\\src\\FountainVideo\\Debug\\vlcR.bat");
		#endif // X64
	#endif
#endif // IO_NETWORK_MODE

#ifdef OS_WINDOWS
	system("pause");
#endif
#ifdef  OS_LINUX
	pause();
#endif
	ReceiverClose();
	printf("ReceiverClose ok!\r\n");

}
