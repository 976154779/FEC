/*----------------------------------------------------------------
File Name  : videoSend.c
Author     : Winglab
Data	   : 2016-12-29
Description: 
	Set the I/O interface and start the vedio realtime 
transmit program.
------------------------------------------------------------------*/
#include"ringbuffer.h"
#include"transmitter.h"
#include"receiver.h"
#include"io.h"
#include"err.h"
#include"cross_platform.h"

static netInfo_t *sockRecv=NULL;

iThread_t hThreadSend;
iThread_t hThreadFbChecher;

iThreadStdCall_t SendThread(LPVOID lpParam)
{
	Data_t myData;
	IODataInfo_t myDataInfo;
	uint32_t id = 0;
	tokenBucket_t myBucket;
	myBucket.tokenNum = 0;
	myBucket.produceTime = 0;
	myBucket.limitFlowSpeed = BYTE_RATE_DEFAULT / 1000 / 2;
	myBucket.limitMaxFlowSpeed = (uint32_t)BYTE_RATE_DEFAULT / 2;

	char myStr[] = "HelloWord ";
	while (1)
	{
		myData.len = strlen(myStr);
		myData.id = id++;
		strcpy(myData.data, myStr);
		myDataInfo.len = sizeof(myData);
		while (_GetToken(&myBucket, myDataInfo.len) == 0) {
			iSleep(1);
		}
		//debug("[DATA_IN] id=%d\r\n", myData.id);
		myData.ts = iGetTime();
		memcpy(myDataInfo.data, &myData, myDataInfo.len);
		IO_InputData(&myDataInfo);
	}

	return 0;
}

iThreadStdCall_t FbChecherThread(LPVOID lpParam) {
	DataFb_t myDataFb;
	while (1)
	{
		int res=NetworkUDPReceive(sockRecv,&myDataFb,sizeof(myDataFb));
		debug("[DataFb] id=%ld,delay=%ld \r\n", myDataFb.id, iGetTime() - myDataFb.ts);
	}
}

void main() 
{
#ifdef OS_LINUX
	initStartTime();
#endif // OS_LINUX
#ifndef  IO_NETWORK_MODE
	netAddr_t addrSend, addrRecv;
	strcpy(addrSend.ip, IP_RX_0);
	addrSend.port = 3510;
	strcpy(addrRecv.ip, IP_TX_0);
	addrRecv.port = 3520;
	sockRecv = NetworkUDPInit(&addrRecv, &addrSend);
#endif // ! IO_NETWORK_MODE
	TransmitterInit();
	printf("TransmitterInit ok! \r\n");
#ifndef  IO_NETWORK_MODE
	iCreateThread(hThreadSend, SendThread, NULL);  // returns the thread identifier 
	iCreateThread(hThreadFbChecher, FbChecherThread, NULL);  // returns the thread identifier 
#endif // ! IO_NETWORK_MODE
#ifdef IO_NETWORK_MODE
	#ifdef OS_WINDOWS
		#ifdef X64
			system("E:\\lab\\sw\\RQS\\x64\\Debug\\vlcT.bat");
		#else
			//system("E:\\lab\\src\\FountainVideo\\Debug\\vlcT.bat");
		#endif // X64
	#endif
#endif // IO_NETWORK_MODE
#ifdef  OS_WINDOWS
	system("pause");
#endif
#ifdef  OS_LINUX
	pause();
#endif
	TransmitterClose();
	printf("TransmitterClose ok!\r\n");

}
