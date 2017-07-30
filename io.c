/*----------------------------------------------------------------
File Name  : io.c
Author     : Winglab
Data	   : 2016-12-29
Description:
Instruction: The interface between the source data and this
program.
At present, UDP interfaces are supported to input the source
data to encode or output the decoded data.

------------------------------------------------------------------*/
#include"io.h"
#include"err.h"

static bool stateRun = true;

static ringBuffer_t *rbOut=NULL;		//packet out 
static ringBuffer_t *rbIn=NULL;			//packet in
static ringBuffer_t *rbRTCP_Back = NULL;//RTCP_Back

static ringBuffer_t *rbDataOut = NULL;	//data out
static ringBuffer_t *rbDataIn = NULL;	//data in

static netInfo_t *sockInput = NULL;
static netInfo_t *sock_RTCP_Input = NULL;
static netInfo_t *sockOutput = NULL;
static netInfo_t *sock_RTCP_Output = NULL;
static netInfo_t *sock_IN_RTCP_Back = NULL;//sock_IN_RTCP_Back
static netInfo_t *sock_IN_RTCP_Forward = NULL;
static netInfo_t *sock_OUT_RTCP_Back = NULL;
static netInfo_t *sock_OUT_RTCP_Forward = NULL;
static netInfo_t *sock_TX_RTCP_Forward = NULL;
static netInfo_t *sock_RX_RTCP_Forward = NULL;
static netInfo_t *sock_TX_RTCP_Back = NULL;
static netInfo_t *sock_RX_RTCP_Back = NULL;
static netInfo_t *sockFbRecv = NULL;
static netInfo_t *sockFbSend = NULL;

static iMutex_t mutexInputDataGet = iMUTEX_INIT_VALUE;
static iMutex_t mutexOutputDataPut = iMUTEX_INIT_VALUE;
static iMutex_t mutexPack = iMUTEX_INIT_VALUE;

static iThread_t hThreadIOInput;
static iThread_t hThreadIO_RTCP_Input;	//
static iThread_t hThreadIN_RTCP_Back;  // edited by fanli
static iThread_t hThreadIN_RTCP_Forward;  // edited by fanli
static iThread_t hThreadOUT_RTCP_Back;
static iThread_t hThreadOUT_RTCP_Forward;
static iThread_t hThreadIOOutput;
static iThread_t hThreadAutoFill;
static iThread_t hThreadFbChecher;

iThreadStdCall_t _IOInputThread(LPVOID lpParam);
iThreadStdCall_t _IOInput_RTCP_Thread(LPVOID lpParam);  //
iThreadStdCall_t _IN_RTCP_Back_Thread(LPVOID lpParam);	 // edited by fanli  _OUT_RTCP_Back_Thread
iThreadStdCall_t _IN_RTCP_Forward_Thread(LPVOID lpParam);	 // edited by fanli  _OUT_RTCP_Back_Thread
iThreadStdCall_t _OUT_RTCP_Back_Thread(LPVOID lpParam);
iThreadStdCall_t _OUT_RTCP_Forward_Thread(LPVOID lpParam);
iThreadStdCall_t _IOOutputThread(LPVOID lpParam);
iThreadStdCall_t _AutoFillThread(LPVOID lpParam);

void _IOPack(char _flag, uint16_t _len, uint8_t *_ioPacketBuff);
void _IOUnpack(uint8_t* _symbol);
static iClock_t autoFillClock = 0;

//edited by ZhangYong
//增加dataUnit_t结构体
//@说明：打包源数据构成基本单元的结构体
typedef struct dataUnit_t {
	char flag;//标志位 
	uint16_t len;//data的数据的长度
	iClock_t ts;
	uint8_t data[0];
}dataUnit_t;




void IO_InputData(IODataInfo_t *_dataInfo) {
	RingBufferPut(rbDataIn, _dataInfo);
}
void IO_OutputData(IODataInfo_t *_dataInfo) {
	RingBufferGet(rbDataOut, _dataInfo);
}

iThreadStdCall_t _AutoFillThread(LPVOID lpParam)
{
	while (stateRun)
	{
		iSleep(1);
		iMutexLock(mutexPack);
		if (iGetTime() - autoFillClock >=2) {
			_IOPack('F', 1, NULL);
		}
		iMutexUnlock(mutexPack);
	}

	return 0;
}


void _IOPack(char _flag,uint16_t _len, uint8_t *_ioPacketBuff) {
	static uint8_t buff[SYMBOL_SIZE];
	static uint16_t restBuffLen = SYMBOL_SIZE;
	static uint8_t *pBuff = buff;

	if (_flag == 'F') {
		if (restBuffLen == SYMBOL_SIZE)return;
		memcpy(pBuff, &_flag, 1);
		RingBufferPut(rbIn, buff);
		// reset the buff and restBuffLen
		pBuff = buff;			//set the point to front
		restBuffLen = SYMBOL_SIZE;

#ifdef DEBUG_F_Flag
		debug("[F Flag]\r\n");
#endif // DEBUG_F_Flag
		return;
	}

	if (restBuffLen == SYMBOL_SIZE) {
		autoFillClock = iGetTime();
	}

	uint16_t restPacketLen = 3 + _len;
	uint8_t *pIOPacket = _ioPacketBuff;

	memcpy(pIOPacket,&_flag,1);
	memcpy(pIOPacket+1, &_len, 2);

	/*   pack   */


while (restPacketLen) {
		if (restBuffLen> restPacketLen) {   // put all the rest of packet into buff
			memcpy(pBuff, pIOPacket, restPacketLen);
			pBuff += restPacketLen;
			restBuffLen -= restPacketLen;
			restPacketLen = 0;
		}
		else if(restBuffLen<= restPacketLen) //put a part of packet into the whole buff
		{
			memcpy(pBuff, pIOPacket, restBuffLen);
			pIOPacket += restBuffLen;
			restPacketLen -= restBuffLen;
			RingBufferPut(rbIn, buff);

			// reset the buff and restBuffLen
			pBuff = buff;
			restBuffLen = SYMBOL_SIZE;
		}
	}

}

/*
@说明：从rbDataUnit中获取dataUnit，然后多个dataUnit拼接出Symbol。每次拼凑出一个Symbol则返回。
将Symbol的值复制到_symbol中。函数中有执行时间限制。
@细节：
1）每个Symbol最多的拼接时长不超过SYMBOL_TIME(3ms)，同时从该AttachSymbol函数开始总时间不超过_timeRest，所以从缓冲中获取使用RingBufferTimedGet(rbDataUnit, ... )方式；
2）从获取到Symbol里最先的dataUnit开始即设定一个时间戳，接下来的BufferGet都计算并赋予剩余的时间，剩余时间满足（1）中两个要求；
3）如果BufferGet返回的是超时则在Symbol接下来需要填充的字节填’F’结束标记，表示此Symbol拼接完成，函数返回。
4）提示：因为每次返回输出一个Symbol，时间到却拼接不满则加‘F’结束，dataUnit长度超过Symbol剩余空间则需要分割，函数中可以设置静态变量保存当前状态。
*/
//edited by ZhangYong
bool AttachSymbol( uint8_t *_symbol, iClock_t _timeRest) {
	if (_timeRest < SYMBOL_TIME) { //构建Block剩余的时间小于构建Symbol剩余的时间
		return 0; 
	}
	static dataUnit_t *dataUnit;//结构体指针
	static uint16_t restDataUnitLen =0;			//（实际上还没有装入symbol的）剩余的dataUnit长度
	uint16_t restSymbolLen = SYMBOL_SIZE;		//剩余的symbol长度	
	static uint8_t *pBuff = NULL;			//指针指向buff	
	uint8_t *pSymbol = _symbol;		//指针指向_symbol	//函数每调用一次，则重新创建一个指向symbol的指针
	
	iClock_t SymbolTimeRest = SYMBOL_TIME;	//构建Symbol剩余的时间  初始化时间为3ms		
	iClock_t TimeStart = 0;

	bool hasGetDataUnit = 0;	//用去确定本次attachSymbol函数是否需要重新获取一个dataUnit
	uint16_t temp;//nD=nS-SU

	if (restDataUnitLen == 0) {	// 表示上一个dataUnit已经用完了，需要重新获取一个dataUnit	
		RingBufferGet(rbDataUnit, dataUnit);	//先取第一个dataUnit
		pBuff = dataUnit;
		restDataUnitLen = dataUnit->len + sizeof(dataUnit_t);//dataUnit的（包含头部）总长度		
		hasGetDataUnit = 1;		//表示目前获取到了一个dataUnit
	}
	else{//restDataUnitLen>0, 则上一次一个dataUnit还没有用完，所以这次不需要再获取新的dataUnit
		hasGetDataUnit = 1;	
	}

	do {
		TimeStart = iGetTime();	
		if (hasGetDataUnit == 0) {
			SymbolTimeRest -= (iGetTime() - TimeStart);		//减少相应的Symbol剩余的时间	
			if (RingBufferTimedGet(rbDataUnit, dataUnit, SymbolTimeRest) == 1) {//获取第二个（第三个……）dataUnit
				pBuff = dataUnit;
				restDataUnitLen = dataUnit->len + sizeof(dataUnit_t);
				hasGetDataUnit = 1;	
			}
			else {	//获取第n(n>=2)个dataUnit超时了
				//symbol over
				memcpy(pSymbol, 'F', 1);	//在Symbol接下来需要填充的字节填’F’结束标记，表示此Symbol拼接完成，函数返回。
				return 1;		//生成了一个symbol
			}
		}
		temp = restSymbolLen - restDataUnitLen;//nD=nS-SU
		memcpy(pSymbol, pBuff, restSymbolLen > restDataUnitLen ? restDataUnitLen : restSymbolLen);
		if (temp > 0) {//Unit over
			restSymbolLen -= restDataUnitLen;	
			pSymbol += restDataUnitLen;		//symbol指针后移
			hasGetDataUnit = 0;		//表示接下来需要获取下一个dataUnit
		}
		else {// temp<0 symbol over / temp=0 Unit over and symbol over
			restDataUnitLen = temp < 0 ? restDataUnitLen - restSymbolLen : 0;
			pBuff = temp < 0 ? pBuff + restSymbolLen : NULL;	
			return 1;
		}
	} while (restSymbolLen > 0);
}

/*@说明：组合出一个Block，同时将源码(symbol)打包后输出到rbSend待发送。
@细节：
1）使用AttachSymbol函数获取Symbol，最大不超过MAX_SYMBOL_NUMBER个Symbol构成一个Block, 此外每个block构建时间不超过BLOCK_TIME（10ms）；
2）_block用来传递block的数据（AttachSymbol的_symbol应该直接指向_block的对应位置）；
3）_encInfo是用来传递encTag, BSN和foreTagBSN；
4）源码也会直接打包并传递到rbSend中待发送。
*/
//edited by ZhangYong
void GetBlock(encInfo_t *_encInfo) {
	uint32_t esi = 0;//目前已经放入block中的symbol的个数//block当中symbol的序号，从0到31
	iClock_t restBlockTime = BLOCK_TIME;//构建此block剩余的时间
	iClock_t TimeStart;
	uint8_t *pBlock = _encInfo->blockData;//指针指向block内存开始的位置
	
	while (esi <= MAX_SYMBOL_NUMBER && restBlockTime > 0) {//最大不超过MAX_SYMBOL_NUMBER个Symbol构成一个Block, 此外每个block构建时间不超过BLOCK_TIME（10ms）
		TimeStart = iGetTime();

		uint8_t *_symbol=(uint8_t *)malloc(SYMBOL_SIZE);	//给_symbol分配内存		

		restBlockTime -= (iGetTime() - TimeStart);//刷新block剩余的时间
		if (AttachSymbol(_symbol, restBlockTime) == 1) {//生成一个symbol
			iRaptorQPack(rbSend, _symbol, encInfo->_tag, esi, encInfo->_foreBSN);	//直接将源码打包后输出到rbSend待发送
			memcpy(pBlock, _symbol, SYMBOL_SIZE);
			pBlock += SYMBOL_SIZE;
			esi++;
		}
		else {//生成一个symbol失败，因为构建Block剩余的时间小于构建Symbol剩余的时间	
			break;//生成好了一个block，不用继续attachSymbol了
		}
	}
}

//@说明：将Symbol拆解成逐个dataUnit并解析，然后用Output Socket输出dataUnit里的data。
//@细节：注意区分flag位，每次只处理一个Symbol, 所以对于未处理的部分先将状态用静态变量存储。
//edited by ZhangYong
void DetachSymbol(uint8_t *_symbol) {
	static dataUnit_t dataUnit;		//暂时保存将要输出的unit 
	
	static uint8_t *pBuff = (uint8_t *)malloc(;//指针指向buff的开始位置
	static uint16_t dataUnitHeadLen = sizeof(dataUnit_t);//dataUnit首部的长度
	static uint16_t restDataUnitHeadLen = 0;//暂存未处理的dataUnit首部的长度
	static uint16_t restDataLen = 0;//剩余dataUnit中data的长度
	
	uint8_t *pSymbol = _symbol;//指针指向symbol的开始位置   	
	uint16_t restSymbolLen = SYMBOL_SIZE;//剩余未处理symbol的长度
	
	//状态机转换标志
	static bool switchState = 0;//0表示get head，1表示get data  
	
	uint16_t temp;//nD=nS-nU
	uint16_t dataLen;//将要发送的unit的数据长度
	uint8_t flag;//用于判断uint的flag
	while (1) {
		switch (switchState) {
			case 0:	//get head
					//继续上一次函数调用的get head
					if (restDataUnitHeadLen > 0) {//上一次get head只get到了一部分dataUnit的首部
						memcpy(pBuff, pSymbol, restDataUnitHeadLen);//把dataUnit首部（剩下的部分）拷贝到buff中去
						memcpy(restDataLen, pBuff + 2, 2);//重新赋值restDataLen
						pBuff += restDataUnitHeadLen;//指针后移
						pSymbol += restDataUnitHeadLen;//指针后移
						//head over//refresh
						restSymbolLen -= restDataUnitHeadLen;
						switchState = 1;//->get data
						continue;
					}
					//get head
					temp = restSymbolLen - dataUnitHeadLen;
					if (temp >= 1) {
						memcpy(flag, pSymbol, 1);
						if (flag == 'F') {//表示后面都是填充的内容，本symbol处理完毕
							return;
						}
					}
					memcpy(pBuff, pSymbol, restSymbolLen >= dataUnitHeadLen ? dataUnitHeadLen : restSymbolLen);
					if (temp >= 0) {
						memcpy(restDataLen, pSymbol + 2, 2);//重新赋值restDataLen
						pBuff += dataUnitHeadLen;//指针后移
						pSymbol = temp > 0 ? pSymbol + dataUnitHeadLen : NULL;//指针后移/表示一个symbol已经用完了
						//head over
						//refresh
						restSymbolLen -= dataUnitHeadLen;
						switchState = 1;//->get data
						if (temp > 0) {
							continue;
						}
						else {//new symbol
							return;//一个symbol处理完了，还未拼接好的dataUnit保存在buff中
						}
					}	
					else {
						restDataUnitHeadLen = dataUnitHeadLen - restSymbolLen;//保存下来，下一次继续获取剩下的dataUint头部
						pBuff += restSymbolLen;//指针后移
						//head not over
						//new symbol
						//refresh
						restSymbolLen = 0;//表示一个symbol已经用完了
						switchState = 0;//->get head again
						return;	//一个symbol处理完了，还未拼接好的dataUnit保存在buff中	
					}

			case 1:	//get data
					temp = restSymbolLen - restDataLen;
					memcpy(pBuff, pSymbol, restSymbolLen >= restDataLen ? restDataLen : restSymbolLen );//把dataUnit中的数据拷贝到buff中去
					if (temp >= 0) {	
						pSymbol = temp > 0 ? restDataLen : NULL;//指针后移/表示一个symbol已经用完了
						//data over
						//refresh
						restSymbolLen -= restDataLen;
						restDataLen = 0;//表示一个unit已经拼接好了
						pBuff = buff;
						dataLen = 0;
						memcpy(dataLen, pBuff + 2, 2);
						NetworkUDPSend(sockOutput, pBuff + sizeof(dataUnit_t), dataLen);	//用Output Socket输出dataUnit里的data
						switchState = 0;//->get head
						if (temp > 0) {
							continue;
						}
						else {//new symbol
							return;
						}
					}
					else {
						pBuff += restSymbolLen;//指针后移
						pSymbol = NULL;//表示一个symbol已经用完了
						//new symbol
						//refresh
						restDataLen -= restSymbolLen;//表示还要继续获取下一个symbol（也就是获取dataUnit数据部分的下半部分）
						restSymbolLen = 0;//表示一个symbol已经用完了
						switchState = 1;//get data again
						return;	
					}
		}
	}
}

void _IOUnpack(uint8_t* _symbol)
{
	static IOPacket_t ioPacket;
	static uint16_t recvHeadLen = 0;
	static uint16_t restPacketLen = 0;
	static uint8_t ioPacketHeadBuff[3];
	static uint8_t* pIoPacketData = ioPacket.data;
#ifndef IO_NETWORK_MODE 
	IODataInfo_t dataInfo;
#endif // !IO_NETWORK_MODE
	uint8_t* pBuff = _symbol;
	uint16_t restBuffLen = SYMBOL_SIZE;

	while (restBuffLen)
	{
		if (recvHeadLen < 3) {
			memcpy(ioPacketHeadBuff + recvHeadLen, pBuff, 1);
			pBuff++;
			restBuffLen--;
			pIoPacketData++;
			recvHeadLen++;
			if (recvHeadLen == 1) {
				ioPacket.flag = ioPacketHeadBuff[0];
				if (ioPacket.flag == 'F')
				{
					recvHeadLen = 0;
#ifdef DEBUG_F_packet
					debug("[F packet] restLen=%d \r\n", restBuffLen);
#endif // DEBUG_F_packet
					return;
				}
			}
			else if (recvHeadLen == 3) {	// have got a packet
				memcpy(&ioPacket.len, ioPacketHeadBuff + 1,2);
				restPacketLen = ioPacket.len;
				pIoPacketData = ioPacket.data;
				assert(ioPacket.flag == 'R' || ioPacket.flag == 'T');
			}
		}
		else
		{
			if (restBuffLen >= restPacketLen) {
				if (ioPacket.flag == 'R'|| ioPacket.flag == 'T') {
					memcpy(pIoPacketData, pBuff, restPacketLen);
#ifdef IO_NETWORK_MODE 
					if(ioPacket.flag == 'R')
						NetworkUDPSend(sockOutput, ioPacket.data, ioPacket.len-sizeof(iClock_t));	//output_rtp
					if (ioPacket.flag == 'T')
						NetworkUDPSend(sock_RTCP_Output, ioPacket.data, ioPacket.len - sizeof(iClock_t));	//output_rtcp
					iClock_t ts;
					memcpy(&ts, ioPacket.data + ioPacket.len - sizeof(iClock_t), sizeof(iClock_t));
					NetworkUDPSend(sockFbSend,&ts,sizeof(ts));
#else
					dataInfo.len = ioPacket.len;
					memcpy(dataInfo.data, ioPacket.data, ioPacket.len);
					RingBufferPut(rbDataOut, &dataInfo);
#endif // !IO_NETWORK_MODE
				}
				restBuffLen -= restPacketLen;
				pBuff += restPacketLen;
				// reset
				recvHeadLen = 0; 
			}
			else
			{
				if (ioPacket.flag == 'R'|| ioPacket.flag == 'T')memcpy(pIoPacketData, pBuff, restBuffLen);
				restPacketLen -= restBuffLen;
				pIoPacketData += restBuffLen;
				restBuffLen = 0;

			}
		}
	}
}


/*
Name:	_IOInputThread
Description: The thread of Input interface. It receive data from socket
and store in input buffer.
Parameter:
Return:
*/
//edited by ZhangYong   还未修改完成
iThreadStdCall_t _IOInputThread(LPVOID lpParam)
{
	char aData = 0;
#ifdef IO_NETWORK_MODE
	uint8_t *dataBuff = (uint8_t*)malloc(MAX_UDP_DATA_SIZE ); //指针指向UDP数据报中的数据
#else
	uint8_t *packetBuff = (uint8_t*)malloc(SYMBOL_SIZE + 3);
#endif // IO_NETWORK_MODE
	int16_t recvLen=0;//收到的UDP数据实际的的长度（不包含UDP头部）

#ifndef IO_NETWORK_MODE 
	IODataInfo_t dataInfo;
#endif // !IO_NETWORK_MODE

	while (stateRun)
	{
#ifdef _IO_TEST_
		for (int i = 0; i < SYMBOL_SIZE; i++) {	//pack 
			pBuffSym[i] = aData;
			aData++;
		}
		RingBufferPut(rbIn, pBuffSym);
#else		
		//--------------------pack----------------------------------

#ifdef IO_NETWORK_MODE
		/*
		recvLen = NetworkUDPReceive(sockInput, packetBuff + 3, SYMBOL_SIZE);	//put payload（有效负载，即UDP的数据部分） into the body of packet 
		iClock_t ts = iGetTime();//加一个时间戳timestamp
		memcpy(packetBuff + 3 + recvLen, &ts, sizeof(ts));//把时间戳4bytes放到packetBuff最后的四个字节里面
		recvLen += sizeof(ts);
		*/
		
		recvLen = NetworkUDPReceive(sockInput, dataBuff, MAX_UDP_DATA_SIZE);	//put payload（有效负载，即UDP的数据部分） into the body of packet 
		dataUnit_t *dataUnit = (dataUnit_t *)malloc(sizeof(dataUnit_t) + recvLen);
		dataUnit->flag = 'R';
		dataUnit->len = recvLen;
		dataUnit->ts= iGetTime();//加一个时间戳timestamp
		memcpy(dataUnit->data,dataBuff,recvLen);//把时间戳4bytes放到dataUnit_t里面,乘2是因为字节对齐
#else
		RingBufferGet(rbDataIn, &dataInfo);
		memcpy(packetBuff + 3, dataInfo.data, dataInfo.len);
		recvLen = dataInfo.len;
#endif // IO_NETWORK_MODE

#ifdef DEBUG_IO_IN_DATA
		debug("[IO IN DATA]len=%d\r\n",recvLen);
#endif // DEBUG_IO_IN_DATA

		iMutexLock(mutexPack);
		//_IOPack('R', recvLen, packetBuff);//TODO:改成 getblock 和 attachsymbol函数
		iMutexUnlock(mutexPack);

#endif // _IO_TEST_
	}
	free(dataBuff);
	return 0;
}

/*
Name:	_IOInput_RTCP_Thread
Description: The thread of Input interface. It receive RTCP_data from socket
and store in input buffer.
Parameter:
Return:
*/
iThreadStdCall_t _IOInput_RTCP_Thread(LPVOID lpParam)
{
	char aData = 0;
#ifdef IO_NETWORK_MODE
	uint8_t *packetBuff = (uint8_t*)malloc(SYMBOL_SIZE + 3 + sizeof(clock_t));
#else
	uint8_t *packetBuff = (uint8_t*)malloc(SYMBOL_SIZE + 3);
#endif // IO_NETWORK_MODE
	int16_t recvLen = 0;

#ifndef IO_NETWORK_MODE 
	IODataInfo_t dataInfo;
#endif // !IO_NETWORK_MODE

	while (stateRun)
	{
#ifdef _IO_TEST_
		for (int i = 0; i < SYMBOL_SIZE; i++) {	//pack 
			pBuffSym[i] = aData;
			aData++;
		}
		RingBufferPut(rbIn, pBuffSym);
#else		
		//--------------------pack----------------------------------

#ifdef IO_NETWORK_MODE

		recvLen = NetworkUDPReceive(sock_RTCP_Input, packetBuff + 3, SYMBOL_SIZE);	//put payload into the body of packet 
		iClock_t ts = iGetTime();
		memcpy(packetBuff + 3 + recvLen, &ts, sizeof(ts));
		recvLen += sizeof(ts);
#else 
		RingBufferGet(rbDataIn, &dataInfo);
		memcpy(packetBuff + 3, dataInfo.data, dataInfo.len);
		recvLen = dataInfo.len;


#endif // IO_NETWORK_MODE

#ifdef DEBUG_IO_IN_DATA
		debug("[IO IN DATA]len=%d\r\n", recvLen);
#endif // DEBUG_IO_IN_DATA

		iMutexLock(mutexPack);
		_IOPack('T', recvLen, packetBuff);
		iMutexUnlock(mutexPack);

#endif // _IO_TEST_
	}
	free(packetBuff);
	return 0;
}

/*
Name:	_OUT_RTCP_Forward_Thread
Description: The thread of RTCP_Back interface. It send data by socket
from rbRTCP_Back buffer.
Parameter:
Return:
*/

iThreadStdCall_t _OUT_RTCP_Forward_Thread(LPVOID lpParam)	// edited by fanli
{
	int16_t recvLen = 0;
	uint8_t *OUT_RTCP_Forward_Buff = (uint8_t*)malloc(SYMBOL_SIZE);
	while (stateRun) {
		recvLen = NetworkUDPReceive(sock_RX_RTCP_Forward, OUT_RTCP_Forward_Buff, SYMBOL_SIZE);
		if (recvLen)
			NetworkUDPSend(sock_OUT_RTCP_Forward, OUT_RTCP_Forward_Buff, recvLen);
	}
	return 0;
}

iThreadStdCall_t _IN_RTCP_Forward_Thread(LPVOID lpParam)
{
	int16_t recvLen = 0;
	uint8_t *IN_RTCP_Forward_Buff = (uint8_t*)malloc(SYMBOL_SIZE);
	while (stateRun) {
		recvLen = NetworkUDPReceive(sock_IN_RTCP_Forward, IN_RTCP_Forward_Buff, SYMBOL_SIZE);
		if (recvLen)
			NetworkUDPSend(sock_TX_RTCP_Forward, IN_RTCP_Forward_Buff, recvLen);
	}
	return 0;
}
/*
Name:	_IN_RTCP_Back_Thread
Description: The thread of RTCP_Back interface. It send data by socket
from rbRTCP_Back buffer.
Parameter:
Return:
*/

iThreadStdCall_t _IN_RTCP_Back_Thread(LPVOID lpParam)
{
	int16_t recvLen = 0;
	uint8_t *IN_RTCP_Back_Buff = (uint8_t*)malloc(SYMBOL_SIZE);
	while (stateRun){
		recvLen = NetworkUDPReceive(sock_IN_RTCP_Back, IN_RTCP_Back_Buff, SYMBOL_SIZE);
		if(recvLen)
			NetworkUDPSend(sock_TX_RTCP_Back, IN_RTCP_Back_Buff, recvLen);
		}
	return 0;
}


/*
Name:	_OUT_RTCP_Back_Thread
Description: The thread of RTCP_Back interface. It send data by socket
from rbRTCP_Back buffer.
Parameter:
Return:
*/

iThreadStdCall_t _OUT_RTCP_Back_Thread(LPVOID lpParam)	// edited by fanli
{
	int16_t recvLen = 0;
	uint8_t *OUT_RTCP_Back_Buff = (uint8_t*)malloc(SYMBOL_SIZE);
	while (stateRun) {
		recvLen = NetworkUDPReceive(sock_RX_RTCP_Back, OUT_RTCP_Back_Buff, SYMBOL_SIZE);
		if (recvLen)
			NetworkUDPSend(sock_OUT_RTCP_Back, OUT_RTCP_Back_Buff, recvLen);
	}
	return 0;
}



/*
Name:	_IOOutputThread
Description: The thread of Output interface. It send data by socket
from output buffer.
Parameter:
Return:
*/
iThreadStdCall_t _IOOutputThread(LPVOID lpParam)
{

#ifdef _IO_TEST_
	char aData = 0;
#endif // _IO_TEST_
	char *pSymbol = (char*)malloc(SYMBOL_SIZE);

	while (stateRun)
	{
		RingBufferGet(rbOut, pSymbol);

#ifdef _IO_TEST_
		for (int i=0; i < SYMBOL_SIZE; i++){	//unpack

#ifndef FAKE_RAPTORQ
			assert(pSymbol[i] == aData);
#endif // !FAKE_RAPTORQ
			aData++;
		}
#else
		//--------------------unpack-------------------------------
		_IOUnpack(pSymbol);

#endif // _IO_TEST_

	}
	free(pSymbol);
	return 0;
}

iThreadStdCall_t _FbChecherThread(LPVOID lpParam) {
	
	iClock_t ts;
	while (1)
	{
		NetworkUDPReceive(sockFbRecv, &ts, sizeof(ts));
		debug("[netDataFb] delay=%ld \r\n", iGetTime() - ts);
	}
}
/*
Name:	IOInputDataGetBlock
Description: Get a block of data from the Input interface.
Parameter:
@_pBlock : a pointer store one block of data.
Return:
*/
uint32_t IOInputDataGetBlock(ringBuffer_t *_pRb,uint8_t* _pBlock,uint32_t _tag,uint8_t _foreBSN)
{
	uint8_t *pGet = _pBlock;

	uint32_t esi=0;
	bool	isBreak = false;
	//WaitForSingleObject(mutexInputDataGet, INFINITE);
	// get the first symbol and record the start time.
	RingBufferGet(rbIn, pGet);
	iClock_t tt = iGetTime();

	iRaptorQPack(_pRb, pGet, _tag, esi,_foreBSN);
	pGet += SYMBOL_SIZE;
	//geti the rest of symbols, the whole time is less than 10ms
	for (esi = 1; esi < BLOCK_SYMBOL_NUMBER; esi++)
	{
		while (!RingBufferTryGet(rbIn, pGet)) {
			if (iGetTime() - tt >= BLOCK_TIME) {
				isBreak = true;
				break;
			}
			iSleep(1);
		}
	
		if(isBreak==false){
			iRaptorQPack(_pRb, pGet, _tag, esi,_foreBSN);
			pGet += SYMBOL_SIZE;
		}
		else{
			break;
		}
	}
	//iMutexUnlock(mutexInputDataGet);

#ifdef DEBUG_ONE_BLOCK
	debug("[ONE BLOCK] blockSymbolNumber=%d \r\n", esi);
#endif // DEBUG_ONE_BLOCK

#ifdef DEBUG_TT
	debug("[TT]%d\r\n", iGetTime()-tt);
#endif // DEBUG_TT

	return esi;
}

/*
Name:	IOOutputDataPutBlock
Description: Output a block of data to the Output interface.
Parameter:
@_pBlock : a pointer store one block of data.
Return:
*/
uint32_t IOOutputDataPutBlock(int _command, uint8_t* _pData,uint32_t blockSymbolNumber)
{
	uint8_t *pPut = _pData;
	raptorQPacket_t *pPacket = (raptorQPacket_t *)_pData;
	static uint32_t esiExpect = 0;
	iMutexLock(mutexOutputDataPut);
	switch (_command)
	{
	case PUT_BLOCK_START:
		esiExpect = 0;
	break;
	case PUT_BLOCK_CURRENT:
#ifdef DEBUG_out_a
		debug("[out a] symbol: esiExpect=%d id=%d tag=%d\r\n", esiExpect, pPacket->id, pPacket->tag);
#endif // DEBUG_out_a
		if (pPacket->block_symbol_number < blockSymbolNumber)blockSymbolNumber = pPacket->block_symbol_number;
		if (esiExpect == pPacket->id && esiExpect < blockSymbolNumber) {
			RingBufferPut(rbOut, pPacket->data);
			esiExpect++;
		};
	break;
	case PUT_BLOCK_REST:
		pPut = _pData + esiExpect*SYMBOL_SIZE;
		for (uint32_t i = esiExpect; i < blockSymbolNumber; i++){
			RingBufferPut(rbOut, pPut);
#ifdef DEBUG_out_b
			debug("[out b] symbol: i=%d\r\n", i);
#endif // DEBUG_out_b
			pPut += SYMBOL_SIZE;
		}
	break;

	}
	iMutexUnlock(mutexOutputDataPut);
	return esiExpect;
}
void IOOutputDataPutSymbol(uint8_t *pPut) 
{
	RingBufferPut(rbOut, pPut);
}
/*
Name:	IOInputInit
Description: Initialize the Input interface.
Parameter:
Return:
*/
void IOInputInit()
{
	/* initialize value */
	/* initialize lock */

	iCreateMutex(mutexInputDataGet,false);

	iCreateMutex(mutexPack, false);
	/* initialize ring buffer */
	rbIn = RingBufferInit(IO_INPUT_RBUFF_NUMBER, SYMBOL_SIZE);

#ifndef IO_NETWORK_MODE
	rbDataIn = RingBufferInit(50,sizeof(IODataInfo_t));
#endif // !IO_NETWORK_MODE

	/* create and set up socket */
#ifndef _IO_TEST_
#ifdef IO_NETWORK_MODE
	netAddr_t addrSend, addrRecv;
	strcpy(addrSend.ip, "127.0.0.1");
	addrSend.port = PORT_IO_INPUT+1;
	strcpy(addrRecv.ip, "127.0.0.1");
	addrRecv.port = PORT_IO_INPUT;

	sockInput=NetworkUDPInit(&addrRecv, NULL);
#ifdef IO_RTCP_INPUT		//
	strcpy(addrSend.ip, "127.0.0.1");
	addrSend.port = PORT_IO_RTCP_INPUT;//
	strcpy(addrRecv.ip, "127.0.0.1");
	addrRecv.port = PORT_IO_RTCP_INPUT;

	sock_RTCP_Input = NetworkUDPInit(&addrRecv, NULL);
#endif
	strcpy(addrSend.ip, IP_RX_0);
	addrSend.port = 3510;
	strcpy(addrRecv.ip, IP_TX_0);
	addrRecv.port = 3520;
	sockFbRecv = NetworkUDPInit(&addrRecv, &addrSend);

#endif // IO_NETWORK_MODE
#endif // !_IO_TEST_
	/* run threads */
	iCreateThread(hThreadIOInput, _IOInputThread, NULL);  // returns the thread identifier  

#ifdef IO_RTCP_INPUT
	iCreateThread(hThreadIO_RTCP_Input, _IOInput_RTCP_Thread, NULL);  // returns the thread identifier  
#endif

	iCreateThread(hThreadAutoFill, _AutoFillThread, NULL);  // returns the thread identifier 

#ifdef IO_NETWORK_MODE
	iCreateThread(hThreadFbChecher, _FbChecherThread, NULL);  // returns the thread identifier 
#endif // IO_NETWORK_MODE

}

/*
Name:	RTCP_Forward_Send_Init
Description: Initialize the Output interface.
Parameter:
Return:
*/
void RTCP_Forward_Send_Init()
{
	/* initialize ring buffer */
	// rbRTCP_Back = RingBufferInit(IO_RTCP_Back_RBUFF_NUMBER, RTCP_SIZE);
	netAddr_t addrSend, addrRecv;
	strcpy(addrSend.ip, "127.0.0.1");
	addrSend.port = PORT_IO_RTCP_INPUT;
	strcpy(addrRecv.ip, "127.0.0.1");
	addrRecv.port = PORT_IO_RTCP_OUTPUT + 1;
	sock_IN_RTCP_Forward = NetworkUDPInit(&addrSend, NULL);
	/* run threads *///sock_TX_RTCP_Back
	strcpy(addrSend.ip, IP_TX_0);
	addrSend.port = PORT_IO_RTCP_INPUT ;
	strcpy(addrRecv.ip, IP_RX_0);
	addrRecv.port = PORT_IO_RTCP_OUTPUT ;
	sock_TX_RTCP_Forward = NetworkUDPInit(&addrSend, &addrRecv);
	iCreateThread(hThreadIN_RTCP_Forward, _IN_RTCP_Forward_Thread, NULL);  // returns the thread identifier 
}
//

/*
Name:	RTCP_Forward_Recv_Init
Description: Initialize the Output interface.
Parameter:
Return:
*/
void RTCP_Forward_Recv_Init()
{
	/* initialize ring buffer */
	// rbRTCP_Back = RingBufferInit(IO_RTCP_Back_RBUFF_NUMBER, RTCP_SIZE);
	netAddr_t addrSend, addrRecv;
	strcpy(addrSend.ip, "127.0.0.1");
	addrSend.port = PORT_IO_RTCP_OUTPUT - 3;
	strcpy(addrRecv.ip, "127.0.0.1");
	addrRecv.port = PORT_IO_RTCP_OUTPUT;
	sock_OUT_RTCP_Forward = NetworkUDPInit(&addrSend, &addrRecv);
	/* run threads *///sock_TX_RTCP_Back
	strcpy(addrSend.ip, IP_TX_0);
	addrSend.port = PORT_IO_RTCP_OUTPUT ;
	strcpy(addrRecv.ip, IP_RX_0);
	addrRecv.port = PORT_IO_RTCP_INPUT ;
	sock_RX_RTCP_Forward = NetworkUDPInit(&addrRecv, &addrSend);
	iCreateThread(hThreadOUT_RTCP_Forward, _OUT_RTCP_Forward_Thread, NULL);  // returns the thread identifier 
}


/*
Name:	RTCP_Back_Send_Init
Description: Initialize the Output interface.
Parameter:
Return:
*/
void RTCP_Back_Send_Init()
{
	/* initialize ring buffer */
   // rbRTCP_Back = RingBufferInit(IO_RTCP_Back_RBUFF_NUMBER, RTCP_SIZE);
	netAddr_t addrSend, addrRecv;
	strcpy(addrSend.ip, "127.0.0.1");
	addrSend.port = PORT_IO_RTCP_OUTPUT;
	strcpy(addrRecv.ip, "127.0.0.1");
	addrRecv.port = PORT_IO_RTCP_INPUT+1;
	sock_IN_RTCP_Back = NetworkUDPInit(&addrSend, NULL);
	/* run threads *///sock_TX_RTCP_Back
	strcpy(addrSend.ip, IP_RX_0);
	addrSend.port = PORT_IO_RTCP_OUTPUT + 1;
	strcpy(addrRecv.ip, IP_TX_0);
	addrRecv.port = PORT_IO_RTCP_INPUT + 1;
	sock_TX_RTCP_Back = NetworkUDPInit(&addrSend, &addrRecv);
    iCreateThread(hThreadIN_RTCP_Back, _IN_RTCP_Back_Thread, NULL);  // returns the thread identifier 
} 

/*
Name:	RTCP_Back_Recv_Init
Description: Initialize the Output interface.
Parameter:
Return:
*/
void RTCP_Back_Recv_Init()
{
	/* initialize ring buffer */
	// rbRTCP_Back = RingBufferInit(IO_RTCP_Back_RBUFF_NUMBER, RTCP_SIZE);
	netAddr_t addrSend, addrRecv;
	strcpy(addrSend.ip, "127.0.0.1");
	addrSend.port = PORT_IO_RTCP_INPUT-3;
	strcpy(addrRecv.ip, "127.0.0.1");
	addrRecv.port = PORT_IO_RTCP_INPUT;
	sock_OUT_RTCP_Back = NetworkUDPInit(&addrSend, &addrRecv);
	/* run threads *///sock_TX_RTCP_Back
	strcpy(addrSend.ip, IP_RX_0);
	addrSend.port = PORT_IO_RTCP_OUTPUT + 1;
	strcpy(addrRecv.ip, IP_TX_0);
	addrRecv.port = PORT_IO_RTCP_INPUT + 1;
	sock_RX_RTCP_Back = NetworkUDPInit(&addrRecv, &addrSend);
	iCreateThread(hThreadOUT_RTCP_Back, _OUT_RTCP_Back_Thread, NULL);  // returns the thread identifier 
}

/*
Name:	IOOutputInit
Description: Initialize the Output interface.
Parameter:
Return:
*/
void IOOutputInit()
{
	/* initialize value */

	/* initialize lock */
	iCreateMutex(mutexOutputDataPut,false);

	/* initialize ring buffer */
	rbOut = RingBufferInit(IO_OUTPUT_RBUFF_NUMBER, SYMBOL_SIZE);
#ifndef IO_NETWORK_MODE
	rbDataOut = RingBufferInit(50, sizeof(IODataInfo_t));
#endif // !IO_NETWORK_MODE
	/* create and set up socket */

#ifndef _IO_TEST_
#ifdef IO_NETWORK_MODE
	netAddr_t addrSend, addrRecv;
	strcpy(addrSend.ip, "127.0.0.1");
    //addrSend.port = PORT_IO_OUTPUT+1;  //edited by fanli
	addrSend.port = PORT_IO_OUTPUT - 1;
	strcpy(addrRecv.ip, "127.0.0.1");
	addrRecv.port = PORT_IO_OUTPUT;
	sockOutput = NetworkUDPInit(&addrSend, &addrRecv);

	strcpy(addrSend.ip, "127.0.0.1");  //edited by fanli
	addrSend.port = PORT_IO_OUTPUT - 3;
	strcpy(addrRecv.ip, "127.0.0.1");
	addrRecv.port = PORT_IO_RTCP_OUTPUT;
	sock_RTCP_Output = NetworkUDPInit(&addrSend, &addrRecv);

	strcpy(addrSend.ip, IP_RX_0);
	addrSend.port = 3510;
	strcpy(addrRecv.ip, IP_TX_0);
	addrRecv.port = 3520;
	sockFbSend = NetworkUDPInit(&addrSend, &addrRecv);

#endif // IO_NETWORK_MODE
#endif // !_IO_TEST_

	/* run threads */
	iCreateThread(hThreadIOOutput,_IOOutputThread, NULL);  // returns the thread identifier 

}

/*
Name:	IOInputDeInit
Description: Stop and destory the Input interface.
Parameter:
Return:
*/
void IOInputDeInit()
{
	/* terminate thread */
	stateRun = false;
	iThreadJoin(hThreadIOInput);
	iThreadClose(hThreadIOInput);
	iThreadJoin(hThreadAutoFill);
	iThreadClose(hThreadAutoFill);
	/* destory ring buffer */
	RingBufferDestroy(rbIn);
	/* destory lock */
	iMutexDestory(mutexInputDataGet);
}

/*
Name:	IOOutputDeInit
Description: Stop and destory the Output interface.
Parameter:
Return:
*/
void IOOutputDeInit()
{
	/* terminate thread */
	stateRun = false;
	iThreadJoin(hThreadIOOutput);
	iThreadClose(hThreadIOOutput);
	/* destory ring buffer */
	RingBufferDestroy(rbOut);
	/* destory lock */
	iMutexDestory(mutexOutputDataPut);
}
