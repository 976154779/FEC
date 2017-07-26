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
	if (_timeRest < SYMBOL_TIME) {									//����Blockʣ���ʱ��С�ڹ���Symbolʣ���ʱ��
		return 0;
	}
	static uint8_t buff[MAX_UDP_DATA_SIZE + sizeof(dataUnit_t)];		//��ʱ���潫Ҫ���浽_symbol��ȥ��unit  //���뱣��Ϊ����
	
	static uint16_t restDataUnitLen =0;										//��ʵ���ϻ�û��װ��symbol�ģ�ʣ���dataUnit����	//���뱣��Ϊ����
	static uint16_t restSymbolLen = SYMBOL_SIZE;						//ʣ���symbol����	//���뱣��Ϊ����
	
	static uint8_t *pBuff = buff;										//ָ��ָ��buff	//���뱣��Ϊ����
	uint8_t *pSymbol = _symbol;											//ָ��ָ��_symbol	//����ÿ����һ�Σ������´���һ��ָ��symbol��ָ��
	
	static iClock_t SymbolTimeRest = SYMBOL_TIME;						//����Symbolʣ���ʱ��  ��ʼ��ʱ��Ϊ3ms		//���뱣��Ϊ����
	static iClock_t TimeStart = 0;

	bool hasGetDataUnit = 0;	//��ȥȷ������attachSymbol�����Ƿ���Ҫ���»�ȡһ��dataUnit

	if (restDataUnitLen == 0) {	//restDataUnitLen = 0; ��ʾ��һ��dataUnit�Ѿ������ˣ���Ҫ���»�ȡһ��dataUnit
		//��ȡ��һ��dataUnit
		RingBufferGet(rbDataUnit, buff);	//��rbDataUnit�л�ȡdataUnit���ŵ�pBuff��ָ��ĵ�ַ��ȥ
		memcpy(restDataUnitLen, pBuff + 2, 2);
		restDataUnitLen += sizeof(dataUnit_t);			//dataUnit�ģ�����ͷ��������
		hasGetDataUnit = 1;		//��ʾĿǰ��ȡ����һ��dataUnit
	}
	else if (restDataUnitLen > 0) {//���restDataUnitLen>0, ����һ�ε���attachSymbol������������һ��symbol��һ��dataUnit��û�����꣬������β���Ҫ�ٻ�ȡ�µ�dataUnit
		hasGetDataUnit = 1;		//��ʾĿǰ������һ��û�����dataUnit
	}

	do {
		TimeStart = iGetTime();						//��ȡ��ǰʱ��

		if (hasGetDataUnit == 0) {
			//��ȡ�ڶ�����������������dataUnit
			if (RingBufferTimedGet(rbDataUnit, buff, SymbolTimeRest) == 1) {
				memcpy(restDataUnitLen, pBuff + 2, 2);
				restDataUnitLen += sizeof(dataUnit_t);		//��ȡbuff���������dataUnit�ģ�����ͷ��������
				hasGetDataUnit = 1;		//��ʾĿǰ��ȡ����һ��dataUnit
			}
			else {	//��ȡ��n(n>=2)��dataUnit��ʱ�ˣ�˵������Symbolʣ���ʱ��Ϊ0��
				//symbol over
				memcpy(pSymbol, 'F', 1);	//���BufferGet���ص��ǳ�ʱ����Symbol��������Ҫ�����ֽ��F��������ǣ���ʾ��Symbolƴ����ɣ��������ء�
				restSymbolLen = SYMBOL_SIZE;	//����ˢ��һ������ֵ
				restDataUnitLen = 0;
				SymbolTimeRest = SYMBOL_TIME;	//����ˢ�¹���Symbolʣ���ʱ��
				pSymbol = NULL;			//��ʾһ��symbol�Ѿ���������
				pBuff = buff;	//ָ������ָ��buff�Ŀ�ͷ
				return 1;		//������һ��symbol
			}
		}

		if (restSymbolLen - restDataUnitLen > 0) {
			//Unit over
			memcpy(pSymbol, pBuff, restDataUnitLen);
			//refresh restSymbolLen and restDataUnitLen 
			restSymbolLen -= restDataUnitLen;
			restDataUnitLen = 0;
			pBuff = NULL;	//��ʾһ��unit�Ѿ�����			
			pSymbol += restDataUnitLen;		//symbolָ�����
			pBuff = buff;	//ָ������ָ��buff
			SymbolTimeRest -= (iGetTime() - TimeStart);		//������Ӧ��Symbolʣ���ʱ��
			hasGetDataUnit = 0;		//��ʾ��������Ҫ��ȡ��һ��dataUnit
		}
		else if (restSymbolLen - restDataUnitLen < 0) {
			//symbol over
			memcpy(pSymbol, pBuff, restSymbolLen);
			//refresh restSymbolLen and restDataUnitLen 
			restDataUnitLen -= restSymbolLen;
			pBuff = restSymbolLen;	//��ʾһ��unit�Ѿ�����			
			pSymbol = NULL;		//��ʾһ��symbol�Ѿ���������
			restSymbolLen = SYMBOL_SIZE;
			SymbolTimeRest = SYMBOL_TIME;		//����ˢ�¹���Symbol��ʣ���ʱ��
			return 1;
		}
		else {
			//Unit over and symbol over
			memcpy(pSymbol, pBuff, restSymbolLen);//����������������restDataUnitLen����һ��
			//refresh restSymbolLen and restDataUnitLen 
			restDataUnitLen = 0;
			restSymbolLen = SYMBOL_SIZE;
			pBuff = NULL;	//��ʾһ��unit�Ѿ�����			
			pSymbol = NULL;		//��ʾһ��symbol�Ѿ���������
			SymbolTimeRest = SYMBOL_TIME;		//����ˢ�¹���Symbol��ʣ���ʱ��
			pBuff = buff;	//ָ������ָ��buff
			return 1;
		}
	} while (restSymbolLen > 0);
}

/*@˵������ϳ�һ��Block��ͬʱ��Դ��(symbol)����������rbSend�����͡�
@ϸ�ڣ�
1��ʹ��AttachSymbol������ȡSymbol����󲻳���MAX_SYMBOL_NUMBER��Symbol����һ��Block, ����ÿ��block����ʱ�䲻����BLOCK_TIME��10ms����
2��_block��������block�����ݣ�AttachSymbol��_symbolӦ��ֱ��ָ��_block�Ķ�Ӧλ�ã���
3��_encInfo����������encTag, BSN��foreTagBSN��
4��Դ��Ҳ��ֱ�Ӵ�������ݵ�rbSend�д����͡�
*/
//edited by ZhangYong
void GetBlock(uint8_t *_block, encInfo_t *_encInfo) {
	uint32_t symbolNumber = 0;//Ŀǰ�Ѿ�����block�е�symbol�ĸ���
	iClock_t restBlockTime = BLOCK_TIME;//������blockʣ���ʱ��
	
	//_block = (void *)malloc(BLOCK_SYMBOL_NUMBER*SYMBOL_SIZE);//�����ڴ棬���ڴ��Block
	uint8_t *pBlock = _block;//ָ��ָ��block�ڴ濪ʼ��λ��
	uint32_t symbolId = 0;//block����symbol����ţ���0��31
	
	while (symbolNumber <= MAX_SYMBOL_NUMBER && restBlockTime > 0) {//��󲻳���MAX_SYMBOL_NUMBER��Symbol����һ��Block, ����ÿ��block����ʱ�䲻����BLOCK_TIME��10ms��
		iClock_t TimeStart = iGetTime();

		uint8_t *_symbol=(uint8_t *)malloc(SYMBOL_SIZE);	//��_symbol�����ڴ�		
		if (AttachSymbol(_symbol, restBlockTime) == 1) {//����һ��symbol
			iRaptorQPack(rbSend, _symbol, encInfo->_tag, symbolId, encInfo->_foreBSN);	//ֱ�ӽ�Դ�����������rbSend������
			memcpy(pBlock, _symbol, SYMBOL_SIZE);
			pBlock += SYMBOL_SIZE;
			restBlockTime -= (iGetTime() - TimeStart);//ˢ��blockʣ���ʱ��
			symbolId++;
		}
		else {//����һ��symbolʧ�� //����Blockʣ���ʱ��С�ڹ���Symbolʣ���ʱ��	
			break;//���ɺ���һ��block�����ü���attachSymbol��
		}
	}
}

//@˵������Symbol�������dataUnit��������Ȼ����Output Socket���dataUnit���data��
//@ϸ�ڣ�ע������flagλ��ÿ��ֻ����һ��Symbol, ���Զ���δ����Ĳ����Ƚ�״̬�þ�̬�����洢��
//edited by ZhangYong
void DetachSymbol(uint8_t *_symbol) {
	uint8_t *pSymbol = _symbol;//ָ��ָ��symbol�Ŀ�ʼλ��   
	static uint8_t buff[MAX_UDP_DATA_SIZE + sizeof(dataUnit_t)];		//��ʱ���潫Ҫ�����unit  //���뱣��Ϊ����
	static uint8_t *pBuff = buff;//ָ��ָ��buff�Ŀ�ʼλ��
	static uint16_t restSymbolLen = SYMBOL_SIZE;//ʣ��δ����symbol�ĳ���
	static uint16_t dataUnitHeadLen = sizeof(dataUnit_t);//dataUnit�ײ��ĳ���
	static uint16_t restDataUnitHeadLen = 0;//�ݴ�δ�����dataUnit�ײ��ĳ���
	static uint16_t restDataLen = 0;//ʣ��dataUnit��data�ĳ���
	
	//״̬��ת����־
	static bool switchState = 0;//0��ʾget head��1��ʾget data  //���뱣��Ϊ����

	static uint8_t firstRunging = 1;//�״����б�����
	//get head
	if (firstRunging == 1) {//������ֻ����һ��
		memcpy(pBuff, pSymbol, sizeof(dataUnit_t));//��dataUnit�ײ�������buff��ȥ
		memcpy(restDataLen, pSymbol + 2, 2);//��dataUnit���ײ���len�ֶε�ֵ��������dataUnit��data�ĳ��ȣ�������restDataLen
		pBuff += sizeof(dataUnit_t);//ָ�����
		pSymbol += sizeof(dataUnit_t);//ָ�����
		//head over
		//refresh
		restSymbolLen -= sizeof(dataUnit_t);
		firstRunging = 0;
		switchState = 1;//->get data
	}

	while (1) {
		switch (switchState) {
			case 0:	
					//������һ�κ������õ�get head
					if (restDataUnitHeadLen > 0) {//��һ��get headֻget����һ����dataUnit���ײ�
						memcpy(pBuff, pSymbol, restDataUnitHeadLen);//��dataUnit�ײ���ʣ�µĲ��֣�������buff��ȥ
						memcpy(restDataLen, pSymbol + 2, 2);//���¸�ֵrestDataLen
						pBuff += restDataUnitHeadLen;//ָ�����
						pSymbol += restDataUnitHeadLen;//ָ�����
						//head over
						//refresh
						restSymbolLen -= restDataUnitHeadLen;
						switchState = 1;//->get data
						continue;
					}
					//get head
					if (restSymbolLen - dataUnitHeadLen > 0) {
						memcpy(pBuff, pSymbol, sizeof(dataUnit_t));//��dataUnit�ײ�������buff��ȥ
						memcpy(restDataLen, pSymbol + 2, 2);//���¸�ֵrestDataLen
						pBuff += sizeof(dataUnit_t);//ָ�����
						pSymbol += sizeof(dataUnit_t);//ָ�����
						//head over
						//refresh
						restSymbolLen -= dataUnitHeadLen;
						switchState = 1;//->get data
						continue;
					}
					else if (restSymbolLen - dataUnitHeadLen == 0) {
						memcpy(pBuff, pSymbol, sizeof(dataUnit_t));//��dataUnit�ײ�������buff��ȥ
						memcpy(restDataLen, pSymbol + 2, 2);//���¸�ֵrestDataLen
						pBuff += sizeof(dataUnit_t);//ָ�����
						pSymbol = NULL;//��ʾһ��symbol�Ѿ�������
									   //head over
									   //new symbol
									   //refresh
						restSymbolLen = 0;//��ʾһ��symbol�Ѿ�������
						switchState = 1;//->get data
						return;//һ��symbol�������ˣ���δƴ�Ӻõ�dataUnit������buff��
					}
					else {
						restDataUnitHeadLen = abs(restSymbolLen - dataUnitHeadLen);//������������һ�μ�����ȡʣ�µ�dataUintͷ��
						memcpy(pBuff, pSymbol, restSymbolLen);//��dataUnit�ײ��Ĳ��ֿ�����buff��ȥ
						pBuff += restSymbolLen;//ָ�����
						pSymbol = NULL;//��ʾһ��symbol�Ѿ�������
									   //head not over
									   //new symbol
									   //refresh
						restSymbolLen = 0;//��ʾһ��symbol�Ѿ�������
						switchState = 0;//->get head again
						return;	//һ��symbol�������ˣ���δƴ�Ӻõ�dataUnit������buff��	
					}

			case 1:	
					//get data
					if (restSymbolLen - restDataLen > 0) {
						memcpy(pBuff, pSymbol, restDataLen);//��dataUnit�е����ݿ�����buff��ȥ
						pBuff += restDataLen;//ָ�����
						pSymbol += restDataLen;//ָ�����
						//data over
						//refresh
						restSymbolLen -= restDataLen;
						restDataLen = 0;//��ʾһ��unit�Ѿ�ƴ�Ӻ���
						pBuff = buff;
						uint16_t dataLen = 0;
						memcpy(dataLen, pBuff + 2, 2);
						NetworkUDPSend(sockOutput, pBuff + sizeof(dataUnit_t), dataLen);	//��Output Socket���dataUnit���data
						switchState = 0;//->get head
						continue;
					}
					else if (restSymbolLen - restDataLen == 0) {
						memcpy(pBuff, pSymbol, restDataLen);//��dataUnit�е����ݿ�����buff��ȥ��������������ΪrestSymbolLenҲһ��
						pBuff += restDataLen;//ָ�����
						pSymbol = NULL;//��ʾһ��symbol�Ѿ�������
						//data over
						//new symbol
						//refresh
						restSymbolLen = 0;//��ʾһ��symbol�Ѿ�������
						restDataLen = 0;//��ʾһ��unit�Ѿ�ƴ�Ӻ���
						pBuff = buff;
						uint16_t dataLen = 0;
						memcpy(dataLen, pBuff + 2, 2);
						NetworkUDPSend(sockOutput, pBuff + sizeof(dataUnit_t), dataLen);	//��Output Socket���dataUnit���data
						switchState = 0;//->get head
						return;
					}
					else {
						memcpy(pBuff, pSymbol, restSymbolLen);//��dataUnit�е�����(ǰ�벿�֣�������buff��ȥ
						pBuff += restSymbolLen;//ָ�����
						pSymbol = NULL;//��ʾһ��symbol�Ѿ�������
						//new symbol
						//refresh
						restSymbolLen = 0;//��ʾһ��symbol�Ѿ�������
						restDataLen -= restSymbolLen;//��ʾ��Ҫ������ȡ��һ��symbol��Ҳ���ǻ�ȡdataUnit���ݲ��ֵ��°벿�֣�
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
//edited by ZhangYong   ��δ�޸����
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
