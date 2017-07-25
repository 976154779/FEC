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

typedef struct Data_t {
	uint32_t len;
	clock_t ts;
	uint8_t data[200];
}Data_t;
DWORD WINAPI SendThread(LPVOID lpParam) 
{
	Data_t myData;
	IODataInfo_t myDataInfo;

	tokenBucket_t myBucket;
	myBucket.tokenNum = 0;
	myBucket.produceTime = 0;
	myBucket.limitFlowSpeed = BYTE_RATE_DEFAULT / 1000/4;
	myBucket.limitMaxFlowSpeed = (uint32_t)BYTE_RATE_DEFAULT/4;

	char myStr[] = "HelloWord ";
	while (1)
	{
		myData.len = strlen(myStr);
		strcpy(myData.data, myStr);
		myDataInfo.len = sizeof(myData);

		while (_GetToken(&myBucket, myDataInfo.len) == 0) {
			Sleep(1);
		}
		debug("[DATA_IN] \r\n");
		myData.ts = clock();
		memcpy(myDataInfo.data, &myData, myDataInfo.len);
		IO_InputData(&myDataInfo);
	}
	_endthreadex(0);
	return 0;
}
DWORD WINAPI RecvThread(LPVOID lpParam)
{
	Data_t myData;
	IODataInfo_t myDataInfo;
	while (1)
	{
		IO_OutputData(&myDataInfo);
		memcpy(&myData,myDataInfo.data,myDataInfo.len);
		debug("[DATA_OUT] delay=%d,strLen:%d\r\n", clock() - myData.ts, myData.len);
	}
	_endthreadex(0);
	return 0;
}

void main() 
{
	//HANDLE hwnd1 = _beginthreadex(NULL, 0, pThread1, 0, 0, NULL);
#ifndef _IO_TEST_
	//WinExec("C:\\Program Files (x86)\\VideoLAN\\VLC\\vlc.exe udp://@127.0.0.1:1100  :network-caching=2000", SW_SHOWNORMAL);
#endif // !_IO_TEST_

	ReceiverInit();
	printf("ReceiverInit ok!\r\n");
	TransmitterInit();
	printf("TransmitterInit ok!\r\n");

	HWND hThreadRecv = (HWND)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)RecvThread, NULL, 0, NULL);  // returns the thread identifier 
	HWND hThreadSend = (HWND)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)SendThread, NULL, 0, NULL);  // returns the thread identifier 

#ifndef _IO_TEST_
	//WinExec("C:\\Program Files (x86)\\VideoLAN\\VLC\\vlc.exe C:\\Users\\xiong\\Documents\\Projects\\基于喷泉码的视频传输系统\\video\\DemoVideo.mp4 :sout=#transcode{vcodec=h264,vb=800,acodec=mpga,ab=128,channels=2,samplerate=44100}:udp{mux=ts,dst=127.0.0.1:1000,caching=2000} ", SW_SHOWNORMAL);
#endif // !_IO_TEST_


	system("pause");
	TransmitterClose();
	printf("TransmitterClose ok!\r\n");
	ReceiverClose();
	printf("ReceiverClose ok!\r\n");

}
