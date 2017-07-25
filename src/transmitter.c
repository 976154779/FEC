/*----------------------------------------------------------------
File Name  : transmitter.c
Author     : Winglab
Data	   : 2016-12-29
Description:
Receive the encoded data form transmitter and decode it and
send feedback to transmitter immediately.
------------------------------------------------------------------*/
#include"transmitter.h"
#include"cross_platform.h"
#include"err.h"
#ifdef DELAY_MEASURE
#include<time.h>
#endif // DELAY_MEASURE

enum ENC_STATE
{
	ENC_STATE_IDLE,
	ENC_STATE_SOURCE,
	ENC_STATE_REPAIR,
	ENC_STATE_OVER,
	ENC_STATE_OVER_PROC
};

typedef struct encInfo_t {
	uint32_t encTag;
	uint32_t BSN;
	uint32_t foreTagBSN;
	//ringBuffer_t *rbSource;
	ringBuffer_t *rbRepair;
	uint8_t  priority;
	uint8_t  encState;
	raptorQ_OTI_t myOTI;
	iMutex_t   mutexEncState;
	uint8_t  nOverHead;
	uint8_t  *pBlockData;
	struct RaptorQ_ptr *enc;
	struct encInfo_t *back;
	struct encInfo_t *next;
}encInfo_t;

extern char *ipTx[];
extern char *ipRx[];

static bool stateRun = true;
static iThread_t hThreadFeedbackChecker[FEEDBACK_RX_NUMBER];
static iThread_t hThreadTransmitter[DATA_TX_NUMBER];
static iThread_t hThreadEncoder[ENCODER_NUMBER];
static iThread_t hThreadSource[SOURCE_NUMBER];
static netInfo_t *sockFeedback[CHANNEL_NUMBER];
static netInfo_t *sockTransmit[CHANNEL_NUMBER];
static ringBuffer_t *rbEncode=NULL;
static ringBuffer_t *rbSource=NULL;
static uint8_t foreTagBSN=0;
static int encBuffState[ENCODER_NUMBER] = { 0 };	// if '1' the encode buffer is not empty.
static int encPriority[ENCODER_NUMBER] = {0};

static uint32_t curTag = 0;
static uint32_t floorTag = 0;
static iMutex_t mutexEncTag = iMUTEX_INIT_VALUE;							//mutex for get encode block, store encoder ID, increase floorTag
static iMutex_t mutexFloorTag = iMUTEX_INIT_VALUE;
static iSemaphore_t semaSource ;

static iMutex_t mutexFbc = iMUTEX_INIT_VALUE;
static iMutex_t mutexPriority = iMUTEX_INIT_VALUE;

static encInfo_t *encInfoIndex[ENCODER_INFO_INDEX_LEN];
static encInfo_t *priorityHead = NULL;
static encInfo_t *priorityTail = NULL;
static encInfo_t *priorityCur  = NULL;	//current encInfo
static uint8_t  priorityMax=0;

void _PriorityNew(encInfo_t *_encInfo);
void _PriorityDelete(encInfo_t *_encInfo);
void _PriorityDecrease(encInfo_t* _encInfo);

iThreadStdCall_t _SourceThread(LPVOID lpParam);
iThreadStdCall_t _EncoderThread(LPVOID lpParam);
iThreadStdCall_t _DataTransmitterThread(LPVOID lpParam);
iThreadStdCall_t _FeedbackCheckerThread(LPVOID lpParam);

void _PriorityNew(encInfo_t *_encInfo) {
	
	iMutexLock(mutexPriority);	//lock all the priority
	encInfo_t *pInfo = priorityHead;
	while (pInfo !=NULL)
	{
		pInfo->priority++;
		pInfo = pInfo->next;
	}
	_encInfo->priority = 1;

	if (priorityHead == NULL) {
		_encInfo->next = NULL;
		_encInfo->back = NULL;
		priorityHead = _encInfo;
		priorityTail = _encInfo;
		priorityCur  = _encInfo;
	}
	else 
	{
		_encInfo->next = NULL;
		_encInfo->back = priorityTail;
		priorityTail->next = _encInfo;
		priorityTail = _encInfo;
	}

	iMutexUnlock(mutexPriority);
}

void _PriorityDecrease(encInfo_t *_encInfo)
{
	if(_encInfo->priority>1)_encInfo->priority--;
	encInfo_t *pNext = _encInfo->next;
	if (pNext == NULL) {
		return;
	}
	while (pNext != NULL)
	{
		/* search node */
		if ((pNext->priority < _encInfo->priority)||
			(pNext->priority == _encInfo->priority && pNext->encTag>_encInfo->encTag)){

			if (pNext == _encInfo->next)break;
			/* priorityCur */
			if (priorityCur == _encInfo) {
				priorityCur = _encInfo->next;
			}
			/* delete node */
			_encInfo->next->back = _encInfo->back;
			if (_encInfo->back != NULL) {
				_encInfo->back->next = _encInfo->next; 
			}else{
				priorityHead = _encInfo->next;
			}
			/* insert node */
			_encInfo->back = pNext->back;
			_encInfo->next = pNext;

			pNext->back->next = _encInfo;
			pNext->back = _encInfo;

			return;
		}
		pNext = pNext->next;
	}
	if (pNext == NULL) {
		if (priorityCur == _encInfo) {
			priorityCur = _encInfo->next;
		}
		/* delete node */
		_encInfo->next->back = _encInfo->back;
		if (_encInfo->back != NULL) {
			_encInfo->back->next = _encInfo->next;
		}
		else {
			priorityHead = _encInfo->next;
		}
		/* insert node */
		_encInfo->next = NULL;
		_encInfo->back = priorityTail;

		priorityTail->next = _encInfo;
		priorityTail = _encInfo;
	}

}

void _PriorityDelete(encInfo_t *_encInfo)
{
	iMutexLock(mutexPriority);	//lock all the priority

	if (_encInfo->back == NULL) {		// first node
		priorityHead = _encInfo->next;
	}
	else{
		_encInfo->back->next = _encInfo->next;
	}

	if (_encInfo->next == NULL) {		// last node
		priorityTail = _encInfo->back;
	}
	else{
		_encInfo->next->back= _encInfo->back;
	}
	if (priorityCur == _encInfo) {
		priorityCur = (_encInfo->next==NULL?priorityHead: _encInfo->next);
	}
	iMutexUnlock(mutexPriority);
}

iThreadStdCall_t _SourceThread(LPVOID lpParam) {
	uint8_t *pBlockData = (uint8_t*)malloc(BLOCK_SIZE);
	raptorQPacket_t *packetRepair = (raptorQPacket_t*)malloc(sizeof(raptorQPacket_t));

	while (stateRun)
	{

		encInfo_t *encInfo = (encInfo_t*)malloc(sizeof(encInfo_t));
		encInfo->nOverHead = BLOCK_SYMBOL_NUMBER;
		iCreateMutex(encInfo->mutexEncState, false);
		encInfo->BSN = BLOCK_SYMBOL_NUMBER;	//must be initilized with a maxmum value first.
		//encInfo->rbSource = RingBufferInit(BLOCK_SYMBOL_NUMBER + 1, sizeof(raptorQPacket_t));
		encInfo->rbRepair = RingBufferInit(OVERHEAD, sizeof(raptorQPacket_t));
		encInfo->pBlockData = (uint8_t*)malloc(BLOCK_SIZE);

		iMutexLock(mutexFloorTag);
		iSemaphoreWait(semaSource);

		encInfo->encTag = floorTag++;
		encInfo->foreTagBSN = foreTagBSN;
		encInfo->encState = ENC_STATE_IDLE;
		//set index array
		encInfoIndex[encInfo->encTag%ENCODER_INFO_INDEX_LEN] = encInfo;
		//add to priority link list
		encInfo->encState = ENC_STATE_SOURCE;
		_PriorityNew(encInfo);

		encInfo->BSN = IOInputDataGetBlock(rbSource, encInfo->pBlockData, encInfo->encTag,encInfo->foreTagBSN); //return the real Block Symbol Number
		// put a encInfo to rbRepair to start encoding
		foreTagBSN = encInfo->BSN;
		debug("[SOURCE]tag:%d bsn:%d\r\n",encInfo->encTag,encInfo->BSN);
		RingBufferPut(rbEncode, &encInfo);

		iMutexUnlock(mutexFloorTag);

	}
	free(packetRepair);
	free(pBlockData);

	return 0;
}

/*
Name:	_EncoderThread
Description: Thread to encoded data.
Parameter:
@lpParam : The id of the encoder.
Return:
*/
iThreadStdCall_t _EncoderThread(LPVOID lpParam)
{
	uint8_t encID = (uint8_t)lpParam;
	encInfo_t *encInfo = NULL;

	raptorQPacket_t *packetRepair = (raptorQPacket_t*)malloc(sizeof(raptorQPacket_t));

	uint32_t esi=0;

	while (stateRun)
	{
		RingBufferGet(rbEncode, &encInfo);

		encInfo->enc = iRaptorQ_Enc(encInfo->pBlockData, encInfo->BSN * SYMBOL_SIZE);
		iRaptorQ_GetOTI(encInfo->enc, &encInfo->myOTI);
#ifdef DEBUG_ENC
		debug("[ENC %u]enc tag:%d \r\n", (uint32_t)encID, encInfo->encTag);
#endif // DEBUG_ENC_REPAIR
		RaptorQ_precompute(encInfo->enc, 0, true);

		esi = encInfo->BSN;

		while (stateRun) {
			if (esi < RaptorQ_max_repair(encInfo->enc, 0)) {

#ifdef DEBUG_ENC_REPAIR
				debug("[ENC_REPAIR %u]enc tag:%d esi:%d\r\n", (uint32_t)encID, encInfo->encTag, esi);
#endif // DEBUG_ENC_REPAIR

				iRaptorQ_encode_id(encInfo->enc, packetRepair, encInfo->encTag, 0, esi);	//generate repair symbols.
				packetRepair->block_symbol_number = encInfo->BSN;
				packetRepair->forTagBSN = encInfo->foreTagBSN;
				packetRepair->oti = encInfo->myOTI;
				packetRepair->type = PACKET_TYPE_REPAIR;
				if (esi < encInfo->BSN + OVERHEAD + 2) {
					RingBufferPut(rbSource,packetRepair);
				}
				else {
					RingBufferPut(encInfo->rbRepair, packetRepair);					//sotre in ring buffer 
				}
				esi++;
				
				if (encInfo->encState == ENC_STATE_OVER) {
					/*  free to idle state  */
					encInfo->encState = ENC_STATE_OVER_PROC;
					RaptorQ_free(&encInfo->enc);
					free(encInfo->pBlockData);
					free(encInfo);
					break;
				}
			}
			else {
				perror("out of max repair id! \r\n");
				exit(1);
			}
		}
	}

	return 0;
}
/*
Name:	_DataTransmitterThread
Description: Thread to transmit the encoded data.
Parameter:
@lpParam : The id of the transmitter.
Return:
*/
iThreadStdCall_t _DataTransmitterThread(LPVOID lpParam)
{
	raptorQPacket_t packetSend;
	uint32_t txID = (uint32_t)lpParam % CHANNEL_NUMBER;
	encInfo_t *pInfo = NULL;
	bool isGet = false;
	iClock_t ts;
	while (stateRun) {
		isGet = false;
		ts=iGetTime();
		if(RingBufferTryGet(rbSource,&packetSend)){
			isGet = true;
		}
		else{
			if(iMutexTryLock(mutexPriority)==0){
				if (priorityCur != NULL ){
					pInfo = priorityCur;	//gei the pointor at the currentnode need to be process
					priorityCur = (priorityCur->next == NULL ? priorityCur = priorityHead : priorityCur->next);
					if(RingBufferTryGet(pInfo->rbRepair, &packetSend)){
						isGet = true;
						if (--pInfo->nOverHead == 0) {		//  Finishing sending a number of overhead repair packets, the priority decressed.
							pInfo->nOverHead = OVERHEAD;
							_PriorityDecrease(pInfo);
						}
						if (priorityCur != NULL && pInfo->priority != priorityCur->priority)priorityCur = priorityHead; // Current node is not the maxmum priority node, then the priority jump to the head of node list
					}
				}
				iMutexUnlock(mutexPriority);
			}
		}
		if (isGet) {
#ifdef DEBUG_TX
			debug("\t\t[TX%u] tag:%ld id:%ld time:%d\r\n", (uint32_t)lpParam, packetSend.tag, packetSend.id,iGetTime()-ts);
#endif //DEBUG_TX
			NetworkUDPSendLimit(sockTransmit[txID], &packetSend, sizeof(packetSend), (uint32_t)BYTE_RATE_DEFAULT);
		}
	}
	return 0;
}

/*
Name:	_FeedbackCheckerThread
Description: Thread to check the feedback.
Parameter: 
@lpParam : The id of the feeddback checker.
Return:
*/

iThreadStdCall_t _FeedbackCheckerThread(LPVOID lpParam)
{
	int fbrID = (uint32_t)lpParam % CHANNEL_NUMBER;
	int oldID = 0;
	int recvN = 0;
	raptorQFeedbackInfo_t pFeedbackInfo;
	raptorQPacket_t packet;
	encInfo_t *encInfo = NULL;

	while (stateRun)
	{
		/*  receive feedback  */
		recvN = NetworkUDPReceive(sockFeedback[fbrID], &pFeedbackInfo, sizeof(raptorQFeedbackInfo_t));
		if (recvN>0) {

#ifdef DELAY_MEASURE
			pFeedbackInfo.ts[3] = iGetTime();
#endif // DELAY_MEASURE

			encInfo = encInfoIndex[pFeedbackInfo.tag % ENCODER_INFO_INDEX_LEN];

			iMutexLock(mutexFbc);	//different tag do not use the same mutex
			/*  filter: pass the old feedback  */
			if (encInfoIndex[pFeedbackInfo.tag % ENCODER_INFO_INDEX_LEN] == NULL ||
				encInfo->encTag != pFeedbackInfo.tag) {
				iMutexUnlock(mutexFbc);		//unlock mutexEncState
				continue;
			}
			/*  check the feedback  */
			if (pFeedbackInfo.count >= encInfo->BSN + OVERHEAD){
				_PriorityDelete(encInfo);
				encInfoIndex[pFeedbackInfo.tag % ENCODER_INFO_INDEX_LEN] = NULL;
				iMutexUnlock(mutexFbc);		//unlock mutexEncState
				iSemaphorePost(semaSource, 1);
				ringBuffer_t *rbRepair = encInfo->rbRepair;
				encInfo->encState = ENC_STATE_OVER;			// must be sure the new buffer is ready ,then initialize the old encoder thread.

				while (encInfo->encState == ENC_STATE_OVER){
					RingBufferTryGet(rbRepair, &packet);
				}
				RingBufferDestroy(rbRepair);
			}
			else{
				iMutexUnlock(mutexFbc);		//unlock curTag
			}
#ifdef DEBUG_FB
			//if (iGetTime()-pFeedbackInfo.ts[0]>1) {
			debug("\t\t\t\t[FB%u]tag:%ld  id:%ld  count:%ld \tdelayOut:%ld \t(a:%ld,b:%ld,c:%ld,d:%ld) \r\n", 
				(uint32_t)lpParam,
				pFeedbackInfo.tag,
				pFeedbackInfo.id,
				pFeedbackInfo.count,
				iGetTime() - pFeedbackInfo.ts[0],	// delay: out
				pFeedbackInfo.ts[1] - pFeedbackInfo.ts[0],	//delay: send
				pFeedbackInfo.ts[2] - pFeedbackInfo.ts[1],	//delay: receive and process
				pFeedbackInfo.ts[3] - pFeedbackInfo.ts[2],	//delay: feedback send
				iGetTime() - pFeedbackInfo.ts[3]
				);	//delay: feedback check progress
				
#endif //DEBUG
		}
	}
	return 0;
}
/*
Name:	TransmitterInit
Description: Initialize the transmitter
Parameter:
Return:
*/
void TransmitterInit()
{
	/*  initialize value */

	/* initialize lock */

	iCreateMutex(mutexFloorTag, false );
	iCreateMutex(mutexPriority, false );
	iCreateMutex(mutexFbc, false);
	iCreateSemaphore(semaSource, ENCODER_WINDOW_LEN, ENCODER_WINDOW_LEN);
	/* initialize encoder ring buffer and variants */

	rbEncode = RingBufferInit(ENCODER_WINDOW_LEN,sizeof(encInfo_t*));
	rbSource = RingBufferInit(ENCODER_WINDOW_LEN*BLOCK_SYMBOL_NUMBER,sizeof(raptorQPacket_t));
	/* create and set up socket */
	netAddr_t addrSend;
	netAddr_t addrRecv;
#ifndef RELAY_MODE
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		strcpy(addrSend.ip, ipTx[i]);
		addrSend.port = PORT_DATA_TX + i;
		strcpy(addrRecv.ip, ipRx[i]);
		addrRecv.port = PORT_DATA_RX + i;
		sockTransmit[i] = NetworkUDPInit(&addrSend, &addrRecv);  //send
	}
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		strcpy(addrSend.ip, ipRx[i]);
		addrSend.port = PORT_FEEDBACK_TX + i;
		strcpy(addrRecv.ip, ipTx[i]);
		addrRecv.port = PORT_FEEDBACK_RX + i;
		sockFeedback[i] = NetworkUDPInit(&addrRecv, &addrSend);   //feedback receive
	}
#else
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		strcpy(addrSend.ip, ipTx[i]);
		addrSend.port = PORT_DATA_TX + i;
		strcpy(addrRecv.ip, IP_RELAY);
		addrRecv.port = PORT_RELAY_RX+i;
		sockTransmit[i] = NetworkUDPInit(&addrSend, &addrRecv);  //send
	}
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		strcpy(addrSend.ip, IP_RELAY);
		addrSend.port = PORT_RELAY_FEEDBACK_TX + i;
		strcpy(addrRecv.ip, ipTx[i]);
		addrRecv.port = PORT_FEEDBACK_RX + i;
		sockFeedback[i] = NetworkUDPInit(&addrRecv, &addrSend);   //feedback receive
	}
#endif // !RELAY_MODE
	
	RTCP_Forward_Send_Init();
	RTCP_Back_Recv_Init();

	/*  RaptorQ pre */
	iRaptorQ_pre();
	/*  Run I/O     */
	IOInputInit();
	/*  create thread */
	for (int i = 0; i < ENCODER_NUMBER; i++) {
		iCreateThread(hThreadEncoder[i], _EncoderThread,(LPVOID)i);
	}

	/*     Main Thread    */
	for (int i = 0; i < DATA_TX_NUMBER; i++) {
		iCreateThread(hThreadTransmitter[i], _DataTransmitterThread,(LPVOID)i);
	}
	for (int i = 0; i < FEEDBACK_RX_NUMBER; i++) {
		iCreateThread(hThreadFeedbackChecker[i], _FeedbackCheckerThread,(LPVOID)i);
	}

	for (int i = 0; i < SOURCE_NUMBER; i++) {
		iCreateThread(hThreadSource[i], _SourceThread,(LPVOID)i);
	}

}
/*
Name:	TransmitterInit
Description: Close the transmitter and release the memory
Parameter:
Return:
*/
void TransmitterClose()
{
	stateRun = false;
	for (int i = 0; i < FEEDBACK_RX_NUMBER; i++) {
		iThreadJoin(hThreadFeedbackChecker[i]);
		iThreadClose(hThreadFeedbackChecker[i]);
	}
	for (int i = 0; i < ENCODER_NUMBER; i++) {
		iThreadJoin(hThreadEncoder[i]);
		iThreadClose(hThreadEncoder[i]);
	}
	for (int i = 0; i < DATA_TX_NUMBER; i++) {
		iThreadJoin(hThreadTransmitter[i]);
		iThreadClose(hThreadTransmitter[i]);
	}
	/*  destory socket  */
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		NetworkUDPClose(sockTransmit[i]);
		NetworkUDPClose(sockFeedback[i]);
	}
	/*  destory buffer  */

	/*  destory lock  */

	/*  Close I/O     */
	IOInputDeInit();
}
