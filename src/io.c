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
//iThreadStdCall_t _AutoFillThread(LPVOID lpParam);

//void _IOPack(char _flag, uint16_t _len, uint8_t *_ioPacketBuff);
//void _IOUnpack(uint8_t* _symbol);
//static iClock_t autoFillClock = 0;

//edited by ZhangYong
//����dataUnit_t�ṹ��
//@˵�������Դ���ݹ��ɻ�����Ԫ�Ľṹ��
typedef struct dataUnit_t {
	char flag;//��־λ 
	uint16_t len;//data�����ݵĳ���
	iClock_t ts;
	uint8_t data[0];
}dataUnit_t;




void IO_InputData(IODataInfo_t *_dataInfo) {
	RingBufferPut(rbDataIn, _dataInfo);
}
void IO_OutputData(IODataInfo_t *_dataInfo) {
	RingBufferGet(rbDataOut, _dataInfo);
}
/*
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
*/
/*
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

/*
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
*/


/*
@˵������rbDataUnit�л�ȡdataUnit��Ȼ����dataUnitƴ�ӳ�Symbol��ÿ��ƴ�ճ�һ��Symbol�򷵻ء�
��Symbol��ֵ���Ƶ�_symbol�С���������ִ��ʱ�����ơ�
@ϸ�ڣ�
1��ÿ��Symbol����ƴ��ʱ��������SYMBOL_TIME(3ms)��ͬʱ�Ӹ�AttachSymbol������ʼ��ʱ�䲻����_timeRest�����Դӻ����л�ȡʹ��RingBufferTimedGet(rbDataUnit, ... )��ʽ��
2���ӻ�ȡ��Symbol�����ȵ�dataUnit��ʼ���趨һ��ʱ�������������BufferGet�����㲢����ʣ���ʱ�䣬ʣ��ʱ�����㣨1��������Ҫ��
3�����BufferGet���ص��ǳ�ʱ����Symbol��������Ҫ�����ֽ��F��������ǣ���ʾ��Symbolƴ����ɣ��������ء�
4����ʾ����Ϊÿ�η������һ��Symbol��ʱ�䵽ȴƴ�Ӳ�����ӡ�F��������dataUnit���ȳ���Symbolʣ��ռ�����Ҫ�ָ�����п������þ�̬�������浱ǰ״̬��
*/
//edited by ZhangYong
bool AttachSymbol( uint8_t *_symbol, iClock_t _timeRest) {
	if (_timeRest < SYMBOL_TIME) {				//����Blockʣ���ʱ��С�ڹ���Symbolʣ���ʱ��
		return 0;
	}
	static uint8_t buff[MAX_UDP_DATA_SIZE + sizeof(dataUnit_t)];	//��ʱ���潫Ҫ���浽_symbol��ȥ��unit  //���뱣��Ϊ����
	_symbol=(uint8_t *)malloc(SYMBOL_SIZE);		//��_symbol�����ڴ�
	static uint16_t restBuffLen = sizeof(buff);		//ʵ���ϻ�û��װ��symbol��ʣ���dataUnit����	//���뱣��Ϊ����
	static uint16_t restSymbolLen = SYMBOL_SIZE;		//ʣ���symbol����	//���뱣��Ϊ����
	
	static uint8_t *pBuff = buff;					//ָ��ָ��buff	//���뱣��Ϊ����
	uint8_t *pSymbol = _symbol;				//ָ��ָ��_symbol	
	
	iClock_t BlockTimeRest = _timeRest;				//����Blockʣ���ʱ��
	static iClock_t SymbolTimeRest = SYMBOL_TIME;	//����Symbolʣ���ʱ��  ��ʼ��ʱ��Ϊ3ms		//���뱣��Ϊ����

	iClock_t TimeStart = iGetTime();
	
		//��ȡ��һ��dataUnit
		if (RingBufferTimedGet(rbDataUnit, pBuff, SymbolTimeRest) == 1) {	//��rbDataUnit�л�ȡdataUnit���ŵ�pBuff��ָ��ĵ�ַ��ȥ
			memcpy(restBuffLen, pBuff + 2, 2);
			restBuffLen += sizeof(dataUnit_t);//��ȡbuff������ͷ�����ĳ���

			if (restBuffLen > SYMBOL_SIZE) {	//��ȡ��dataUnit���ȴ���SYMBOL_SIZE
				//��Ҫ��Ƭ
				memcpy(pSymbol, pBuff, SYMBOL_SIZE);
				restSymbolLen = 0;//=0
				restBuffLen -= SYMBOL_SIZE;//>0
				pBuff += SYMBOL_SIZE;//ָ�����		//���뱣������������
				pSymbol = NULL;//ָ��Ϊ�� ��ʾһ��symbol�Ѿ��γɺ���
				SymbolTimeRest = SYMBOL_TIME;//����ˢ�¹���Symbolʣ���ʱ��
				restSymbolLen = SYMBOL_SIZE;//����ˢ��һ������ֵ
				return 1;//������һ��symbol
			}
			else if (restBuffLen == SYMBOL_SIZE) {//��ȡ��dataUnit����ǡ�õ���SYMBOL_SIZE
				memcpy(pSymbol, pBuff, SYMBOL_SIZE);//������buff���ŵ�symbol��ȥ
				restSymbolLen = 0;
				restBuffLen = 0;
				pBuff = NULL;//ָ��Ϊ�� ��ʾһ��dataUnit�Ѿ�������
				pSymbol = NULL;//ָ��Ϊ�� ��ʾһ��symbol�Ѿ��γɺ���
				SymbolTimeRest = SYMBOL_TIME;//����ˢ�¹���Symbolʣ���ʱ��
				restSymbolLen = SYMBOL_SIZE;//����ˢ��һ������ֵ
				pBuff = buff;//ָ������ָ��buff�Ŀ�ͷ
				return 1;//������һ��symbol
			}
			else {		//��ȡ��dataUnit����С��SYMBOL_SIZE
				memcpy(pSymbol, pBuff, sizeof(buff));//������buff���ŵ�symbol��ȥ
				restSymbolLen -= sizeof(buff);//����0
				restBuffLen = 0;//=0
				pBuff = NULL;//ָ��Ϊ�� ��ʾһ��dataUnit�Ѿ�������
				pSymbol += sizeof(buff);//ָ�����
				SymbolTimeRest -= (iGetTime() - TimeStart);//���¼��㹹��Symbolʣ���ʱ��
				pBuff = buff;//ָ������ָ��buff�Ŀ�ͷ
			}
		}
		else {//��ȡ��һ��dataUnit�ͳ�ʱ�ˣ�˵������Symbolʣ���ʱ��Ϊ0��
			//memcpy(pSymbol, 'F', 1);//���BufferGet���ص��ǳ�ʱ����Symbol��������Ҫ�����ֽ��F��������ǣ���ʾ��Symbolƴ����ɣ��������ء�
			//return 1;//������һ��symbol
			return 0;
		}
	
	//�ڶ��λ�ȡdataUnit�������Ρ����ĴΡ���
	while (restSymbolLen > 0) {
		TimeStart = iGetTime();//���»�ȡ��ǰʱ��
		if (RingBufferTimedGet(rbDataUnit, pBuff, SymbolTimeRest) == 1) {
			memcpy(restBuffLen, pBuff + 2, 2);
			restBuffLen += sizeof(dataUnit_t);//��ȡbuff������ͷ�����ĳ���
			if (restBuffLen > restSymbolLen) {	//��ȡ��dataUnit���ȴ���restSymbolLen
				//����Ҫ��Ƭ
				memcpy(pSymbol, pBuff, restSymbolLen);
				restSymbolLen = 0;
				restBuffLen -= restSymbolLen;//>0
				pBuff += restSymbolLen;//ָ�����
				pSymbol = NULL;//ָ��Ϊ�� ��ʾһ��symbol�Ѿ��γɺ���
				SymbolTimeRest = SYMBOL_TIME;//����ˢ�¹���Symbolʣ���ʱ��
				restSymbolLen = SYMBOL_SIZE;//����ˢ��һ������ֵ
				return 1;
			}
			else if (restBuffLen == restSymbolLen) {//��ȡ��dataUnit���ȵ�restSymbolLen
				memcpy(pSymbol, pBuff, restSymbolLen);//������buff���ŵ�symbol��ȥ
				restSymbolLen = 0;
				restBuffLen = 0;
				pBuff = NULL;//ָ��Ϊ�� ��ʾһ��dataUnit�Ѿ�������
				pSymbol = NULL;//ָ��Ϊ�� ��ʾһ��symbol�Ѿ��γɺ���
				SymbolTimeRest = SYMBOL_TIME;//����ˢ�¹���Symbolʣ���ʱ��
				restSymbolLen = SYMBOL_SIZE;//����ˢ��һ������ֵ
				pBuff = buff;//ָ������ָ��buff�Ŀ�ͷ
				return 1;//������һ��symbol
			}
			else {		//��ȡ��dataUnit����С��restSymbolLen
				memcpy(pSymbol, pBuff, sizeof(buff));//������buff���ŵ�symbol��ȥ
				restSymbolLen -= sizeof(buff);//����0
				restBuffLen = 0;//=0
				pBuff = NULL;//ָ��Ϊ�� ��ʾһ��dataUnit�Ѿ�������
				pSymbol += sizeof(buff);//ָ��Ϊ�� ��ʾһ��symbol�Ѿ��γɺ���
				SymbolTimeRest -= (iGetTime() - TimeStart);//���ٹ���Symbolʣ���ʱ��
				pBuff = buff;//ָ������ָ��buff�Ŀ�ͷ
			}
		}
		else {//��ȡ��n(n>=2)��dataUnit��ʱ�ˣ�˵������Symbolʣ���ʱ��Ϊ0��
			memcpy(pSymbol, 'F', 1);//���BufferGet���ص��ǳ�ʱ����Symbol��������Ҫ�����ֽ��F��������ǣ���ʾ��Symbolƴ����ɣ��������ء�
			restSymbolLen = SYMBOL_SIZE;//����ˢ��һ������ֵ
			SymbolTimeRest = SYMBOL_TIME;//����ˢ�¹���Symbolʣ���ʱ��
			pBuff = buff;//ָ������ָ��buff�Ŀ�ͷ
			return 1;
		}			
	}


}




/*@˵������ϳ�һ��Block��ͬʱ��Դ�����������rbSend�����͡�
@ϸ�ڣ�
1��ʹ��AttachSymbol������ȡSymbol����󲻳���MAX_SYMBOL_NUMBER��Symbol����һ��Block, ����ÿ��block����ʱ�䲻����BLOCK_TIME��10ms����
2��_block��������block�����ݣ�AttachSymbol��_symbolӦ��ֱ��ָ��_block�Ķ�Ӧλ�ã���
3��_encInfo����������encTag, BSN��foreTagBSN��
4��Դ��Ҳ��ֱ�Ӵ�������ݵ�rbSend�д����͡�
*/
//edited by ZhangYong
void GetBlock(void *_block, encInfo_t *_encInfo) {
	uint32_t symbol_number = 0;
	while (symbol_number <= MAX_SYMBOL_NUMBER || <=BLOCK_TIME) {
		if (AttachSymbol(



}

//@˵������Symbol�������dataUnit��������Ȼ����Output Socket���dataUnit���data��
//@ϸ�ڣ�ע������flagλ��ÿ��ֻ����һ��Symbol, ���Զ���δ����Ĳ����Ƚ�״̬�þ�̬�����洢��
//edited by ZhangYong
void DetachSymbol(void *_symbol) {




}


/*
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
*/

/*
Name:	_IOInputThread
Description: The thread of Input interface. It receive data from socket
and store in input buffer.
Parameter:
Return:
*/
//edited by ZhangYong
iThreadStdCall_t _IOInputThread(LPVOID lpParam)
{
	char aData = 0;
#ifdef IO_NETWORK_MODE
	uint8_t *dataBuff = (uint8_t*)malloc(MAX_UDP_DATA_SIZE ); //ָ��ָ��UDP���ݱ��е�����
#else
	uint8_t *packetBuff = (uint8_t*)malloc(SYMBOL_SIZE + 3);
#endif // IO_NETWORK_MODE
	int16_t recvLen=0;//�յ���UDP����ʵ�ʵĵĳ��ȣ�������UDPͷ����

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
		recvLen = NetworkUDPReceive(sockInput, packetBuff + 3, SYMBOL_SIZE);	//put payload����Ч���أ���UDP�����ݲ��֣� into the body of packet 
		iClock_t ts = iGetTime();//��һ��ʱ���timestamp
		memcpy(packetBuff + 3 + recvLen, &ts, sizeof(ts));//��ʱ���4bytes�ŵ�packetBuff�����ĸ��ֽ�����
		recvLen += sizeof(ts);
		*/
		
		recvLen = NetworkUDPReceive(sockInput, dataBuff, MAX_UDP_DATA_SIZE);	//put payload����Ч���أ���UDP�����ݲ��֣� into the body of packet 
		dataUnit_t *dataUnit = (dataUnit_t *)malloc(sizeof(dataUnit_t) + recvLen);
		dataUnit->flag = 'R';
		dataUnit->len = recvLen;
		dataUnit->ts= iGetTime();//��һ��ʱ���timestamp
		memcpy(dataUnit->data,dataBuff,recvLen);//��ʱ���4bytes�ŵ�dataUnit_t����,��2����Ϊ�ֽڶ���
#else
		RingBufferGet(rbDataIn, &dataInfo);
		memcpy(packetBuff + 3, dataInfo.data, dataInfo.len);
		recvLen = dataInfo.len;
#endif // IO_NETWORK_MODE

#ifdef DEBUG_IO_IN_DATA
		debug("[IO IN DATA]len=%d\r\n",recvLen);
#endif // DEBUG_IO_IN_DATA

		iMutexLock(mutexPack);
		//_IOPack('R', recvLen, packetBuff);//TODO:�ĳ� getblock �� attachsymbol����
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
