/*----------------------------------------------------------------
File Name  : receiver.h
Author     : Winglab
Data	   : 2016-12-29
Description:
Encode the source data and transmit via UDP toreceiver.
Check the feedback and coordinate the encode and transmitte
pace.

------------------------------------------------------------------*/
#include"receiver.h"
#ifdef DELAY_MEASURE
#include<time.h>
#include"err.h"
#endif // DELAY_MEASURE

enum RECV_INFO_STATE
{
	RECV_INFO_STATE_IDLE,
	RECV_INFO_STATE_DECODE,
	RECV_INFO_STATE_OUTPUT
};

typedef struct recvInfo_t {
	uint32_t tag;
	uint32_t ctn;
	bool     isSetForeBSN;
	uint8_t  state;
	iSemaphore_t	semaDec;
	iMutex_t		mutex;
	ringBuffer_t	*rbRecv;
}recvInfo_t;
typedef struct decInfo_t {
	uint32_t decTag;
	uint32_t decCount;
	uint32_t BSN;
	uint32_t esiExpect;
	bool     isSetOTI;
	raptorQ_OTI_t OTI;
	ringBuffer_t  *rbDec;
	ringBuffer_t  *rbOut;
}decInfo_t;
extern char *ipTx[];
extern char *ipRx[];
static bool stateRun = true;

static netInfo_t    *sockRecv[CHANNEL_NUMBER],*sockFeedback[CHANNEL_NUMBER];
static recvInfo_t   recvInfoList[RECV_INFO_LIST_LEN];
static bufferSink_t *bufferSink;

static ringBuffer_t *rbSwitcher = NULL;
static uint32_t floorTag = 0;		// a number identify the tag that is about to be decoded
static uint32_t curTag = 0;		// a number identify the tag that is about to be put in output buffer.
								// tag < curTag indicates that the block with this tag has been decoded.
//static raptorQFeedbackInfo_t *pFeedbackInfo;
static iMutex_t mutexCurTag = iMUTEX_INIT_VALUE;
static iMutex_t mutexFloorTag = iMUTEX_INIT_VALUE;

static iThread_t hThreadDecoder[DECODER_NUMBER];
static iThread_t hThreadReceiver[DATA_RX_NUMBER];
static iThread_t hThreadFeedback[FEEDBACK_TX_NUMBER];
static iThread_t hThreadOutput;

iThreadStdCall_t _DataReceiverThread(LPVOID lpParam);
iThreadStdCall_t _FeedbackNotifierThread(LPVOID lpParam);
iThreadStdCall_t _DecoderThread(LPVOID lpParam);
iThreadStdCall_t _OutputThread(LPVOID lpParam);

void _RecvInfoDeInit(recvInfo_t* _pInfo) {
	_pInfo->tag = 0;
	_pInfo->ctn = 0;
	_pInfo->isSetForeBSN = false;
	_pInfo->state = RECV_INFO_STATE_IDLE;

	iSemaphoreDestory(_pInfo->semaDec);
	iCreateSemaphore(_pInfo->semaDec, 0, 1);
	
	RingBufferFlush(_pInfo->rbRecv);
	RingBufferDestroy(_pInfo->rbRecv);
	_pInfo->rbRecv = NULL;
}
void _RecvInfoInit(recvInfo_t* _pInfo,uint32_t _tag) {
	_pInfo->tag = _tag;
	_pInfo->ctn = 0;
	_pInfo->isSetForeBSN = false;
	_pInfo->state = RECV_INFO_STATE_DECODE;

	if (_pInfo->rbRecv != NULL) {
		RingBufferFlush(_pInfo->rbRecv);
		RingBufferDestroy(_pInfo->rbRecv);
	}
	_pInfo->rbRecv = BufferSinkGetBuffer(bufferSink);
	iSemaphorePost(_pInfo->semaDec, 1);
}

void _RecvInfoListInit() {
	for (int i = 0; i < RECV_INFO_LIST_LEN; i++) {
		recvInfo_t *pInfo = &recvInfoList[i];
		pInfo->tag = 0;
		pInfo->ctn = 0;
		pInfo->isSetForeBSN = false;
		pInfo->state   = RECV_INFO_STATE_IDLE;
		iCreateSemaphore(pInfo->semaDec, 0, 1);
		pInfo->rbRecv  = NULL;
		iCreateMutex(pInfo->mutex, false);
	}
}

/*
Name:	_DataReceiverThread
Description: The thread to receive and sotre the data .
Parameter:
@lpParam : The id of receiver.
Return:
*/
		
iThreadStdCall_t _DataReceiverThread(LPVOID lpParam)
{
	uint32_t rxID = (uint32_t)lpParam%CHANNEL_NUMBER;
	raptorQFeedbackInfo_t *pFeedbackInfo = (raptorQFeedbackInfo_t*)malloc(sizeof(raptorQFeedbackInfo_t));
	raptorQPacket_t *packetRecv=(raptorQPacket_t*)malloc(sizeof(raptorQPacket_t));
	raptorQPacket_t *packetBSN = (raptorQPacket_t*)malloc(sizeof(raptorQPacket_t));
	while (stateRun) {
		/*  recveive data  */
		NetworkUDPReceive(sockRecv[rxID], packetRecv,sizeof(raptorQPacket_t));
#ifdef DELAY_MEASURE
		packetRecv->ts[1] = iGetTime();
#endif // DELAY_MEASURE
		
		/*  filter  */		// do no need lock for tag because the symmmetry protect window
		iMutexLock(mutexCurTag);
		if ((curTag > RECV_WINDOW_LEN && packetRecv->tag < (curTag - RECV_WINDOW_LEN)) || (packetRecv->tag > curTag+RECV_WINDOW_LEN)) {
			iMutexUnlock(mutexCurTag);
			continue;
		}
		iMutexUnlock(mutexCurTag);
		/*  set the value of recvBuffInfo  */
		recvInfo_t* pRecvInfo = &recvInfoList[packetRecv->tag % RECV_INFO_LIST_LEN];

		iMutexLock(pRecvInfo->mutex);//-->lock recvInfo
		if (pRecvInfo->state == RECV_INFO_STATE_IDLE) {
			_RecvInfoInit(pRecvInfo,packetRecv->tag);
		}

		if (packetRecv->tag == pRecvInfo->tag) {

			pRecvInfo->ctn++;
			/* ------------ set feedback info ---------------- */
			pFeedbackInfo->count = pRecvInfo->ctn;
			pFeedbackInfo->tag = packetRecv->tag;
			pFeedbackInfo->id = packetRecv->id;
			pFeedbackInfo->ts[0] = packetRecv->ts[0];
			pFeedbackInfo->ts[1] = packetRecv->ts[1];
			pFeedbackInfo->ts[2] = iGetTime();
			NetworkUDPSend(sockFeedback[rxID], pFeedbackInfo, sizeof(*pFeedbackInfo));
				/* ------------ store in buffer ---------------------------*/
			if (pRecvInfo->state == RECV_INFO_STATE_DECODE) {

				if (!pRecvInfo->isSetForeBSN && packetRecv->tag>0) {
					recvInfo_t* pForeRecvInfo = &recvInfoList[(packetRecv->tag - 1) % RECV_INFO_LIST_LEN];
					if (pForeRecvInfo->state == RECV_INFO_STATE_DECODE) {
						packetBSN->tag = packetRecv->tag - 1;
						packetBSN->block_symbol_number = packetRecv->forTagBSN;
						packetBSN->type = PACKET_TYPE_BSN;
						if (RingBufferTryPut(pForeRecvInfo->rbRecv, packetBSN)) {
							pRecvInfo->isSetForeBSN = true;
						}
					}
				}
				RingBufferTryPut(pRecvInfo->rbRecv, packetRecv);
			}
		}
		iMutexUnlock(pRecvInfo->mutex);
#ifdef DEBUG_RX
		debug("\t\t\t[RX %u]tag:%ld id:%ld count:%d BSN:%d foreTagBSN:%d\r\n", rxID, packetRecv->tag,
			packetRecv->id, pFeedbackInfo->count,packetRecv->block_symbol_number,packetRecv->forTagBSN);
#endif //DEBUG_RX
	}
	free(packetBSN);
	free(packetRecv);
	return 0;
}
/*
Name:	_FeedbackNotifierThread
Description: The thread to send feedback information .
Parameter:
@lpParam : The id of the FeedbackNotifier.
Return:
*/
iThreadStdCall_t  _FeedbackNotifierThread(LPVOID lpParam)
{
	raptorQFeedbackInfo_t *pFeedbackInfo = (raptorQFeedbackInfo_t*)malloc(sizeof(raptorQFeedbackInfo_t));

	while (stateRun) {
		/* get feedback info */
		if (pFeedbackInfo->count > 0) {
			/* send feedback with limit rate */
			NetworkUDPSend(sockFeedback[0], pFeedbackInfo,sizeof(*pFeedbackInfo));
		}
	}

	return 0;
}

/*
Name:	_DecoderThread
Description: The thread to decode the data.
Parameter:
@lpParam : The id of the Decoder.
Return:
*/

iThreadStdCall_t _DecoderThread(LPVOID lpParam)
{
	struct RaptorQ_ptr *hDec = NULL;
	uint32_t decID = (uint32_t)lpParam;
	raptorQBlock_t *pBlock = (raptorQBlock_t*)malloc(sizeof(raptorQBlock_t));
	raptorQPacket_t *packetRecv = (raptorQPacket_t*)malloc(sizeof(raptorQPacket_t));
	recvInfo_t *pRecvInfo = NULL;;
	bool isFinished = false;

	while (stateRun)
	{
		/*  get the pointer to current ring buffer  */

		iMutexLock(mutexFloorTag);	//-->lock floorTag
		isFinished = false;
		decInfo_t *decInfo = (decInfo_t*)malloc(sizeof(decInfo_t));
		decInfo->decTag = floorTag++;
		decInfo->decCount = 0;
		decInfo->isSetOTI = false;
		decInfo->BSN = BLOCK_SYMBOL_NUMBER;
		decInfo->esiExpect = 0;

		decInfo->rbDec = RingBufferInit(BLOCK_SYMBOL_NUMBER * 2, sizeof(raptorQPacket_t));
		decInfo->rbOut = RingBufferInit(BLOCK_SYMBOL_NUMBER + 1, SYMBOL_SIZE);

		RingBufferPut(rbSwitcher, &decInfo);
#ifdef DEBUG_DEC_START
		debug("[DEC_START%u]decTag:%u \r\n", decID, decInfo->decTag);
#endif // DEBUG_DEC_START
		iMutexUnlock(mutexFloorTag);//-->unlock floorTag

		pRecvInfo = &recvInfoList[decInfo->decTag % RECV_INFO_LIST_LEN];

		iSemaphoreWait(pRecvInfo->semaDec);// the receiver get a new tag will post this semaphore
		assert(pRecvInfo->tag == decInfo->decTag);

		while (stateRun)
		{
			/*  get the package  */

			RingBufferGet(pRecvInfo->rbRecv, packetRecv);
			assert(packetRecv->tag == decInfo->decTag);

			/* if is notify BSN packet*/
			if (packetRecv->type == PACKET_TYPE_BSN) {
				decInfo->BSN = packetRecv->block_symbol_number;
				if (decInfo->esiExpect == decInfo->BSN)	isFinished = true;
			}
			else {
				/*  block symbol number  */
				if (packetRecv->block_symbol_number < decInfo->BSN) {		//set BSI
					decInfo->BSN = packetRecv->block_symbol_number;
				}
				if (packetRecv->type == PACKET_TYPE_REPAIR && !decInfo->isSetOTI) {	//set OTI of raptore dec
					decInfo->OTI = packetRecv->oti;
					decInfo->isSetOTI = true;
				}

				/*  put source symbol to output buffer */
				if (decInfo->esiExpect < decInfo->BSN && decInfo->esiExpect == packetRecv->id && packetRecv->type== PACKET_TYPE_SOURCE) {
					RingBufferPut(decInfo->rbOut, packetRecv->data);
					decInfo->esiExpect++;
				}
				/*  decode  */
				decInfo->decCount++;
				if (decInfo->decCount < decInfo->BSN + OVERHEAD) {
					/*  decode add */
					RingBufferPut(decInfo->rbDec, packetRecv);
					if (decInfo->esiExpect == decInfo->BSN) {
						isFinished = true;
					}
				}
				else
				{
					if (decInfo->decCount == decInfo->BSN + OVERHEAD) {
						assert(decInfo->isSetOTI);
						
						hDec = iRaptorQ_Dec(decInfo->OTI);	// get decode handle
						raptorQPacket_t *packetDec = (raptorQPacket_t*)malloc(sizeof(raptorQPacket_t));
						for (uint32_t esi = 1; esi < decInfo->decCount; esi++) {
							RingBufferGet(decInfo->rbDec, packetDec);
							iRaptorQ_add_symbol_id(hDec, packetDec);
						}
						free(packetDec);
					}
					iRaptorQ_add_symbol_id(hDec, packetRecv);
					//ts=iGetTime();
					if (decInfo->esiExpect == decInfo->BSN || iRaptorQ_decode(hDec, pBlock, decInfo->BSN*SYMBOL_SIZE)) {
						isFinished = true;
						RaptorQ_free(&hDec);
					//	debug("[DECODED]tag:%d,esiExpect:%d,bsn:%d time:%d \r\n", decInfo->decTag,decInfo->esiExpect, decInfo->BSN,iGetTime()-ts);
					}
				}
			}
			if (isFinished) {
				iMutexLock(pRecvInfo->mutex);
				pRecvInfo->state = RECV_INFO_STATE_OUTPUT;
				iMutexUnlock(pRecvInfo->mutex);

#ifdef DEBUG_Decode
				debug("\t\t\t\t\t[Decode %d] curTag:%u esiExpect:%u  bsn:%u  decTag:%u \r\n",
					(int)lpParam, curTag, decInfo->esiExpect, decInfo->BSN, decInfo->decTag);
#endif // DEBUG_Decode

				/*  store in output buffer  */
				if (decInfo->esiExpect != decInfo->BSN) {	// output the rest of decode symbol
					for (uint32_t esi = decInfo->esiExpect; esi < decInfo->BSN; esi++) {
						RingBufferPut(decInfo->rbOut, pBlock->data + esi*SYMBOL_SIZE);
					}
				}

				RingBufferPut(decInfo->rbOut, pBlock->data);
				/*	process after a block of decoding */
				iMutexLock(mutexCurTag);
				curTag++;
				iMutexUnlock(mutexCurTag);

				if (pRecvInfo->tag >= RECV_WINDOW_LEN) {
					pRecvInfo = &recvInfoList[(pRecvInfo->tag - RECV_WINDOW_LEN) % RECV_INFO_LIST_LEN];
					iMutexLock(pRecvInfo->mutex);//-->lock recvInfo
					_RecvInfoDeInit(pRecvInfo);
					iMutexUnlock(pRecvInfo->mutex);
				}

				break; // go to next decode progress
			}
		}

#ifndef FAKE_RAPTORQ
		RaptorQ_free(&hDec);
#endif

	}
	free(packetRecv);
	free(pBlock);

	return 0;
}

iThreadStdCall_t _OutputThread(LPVOID lpParam) {
	decInfo_t *decInfo=NULL;
	uint8_t pSymbol[SYMBOL_SIZE];
	while (stateRun)
	{
		RingBufferGet(rbSwitcher, &decInfo);
		for (uint32_t esi = 0;;esi++)
		{
			RingBufferGet(decInfo->rbOut, pSymbol);
			if (esi < decInfo->BSN) {
				IOOutputDataPutSymbol(pSymbol);
			}
			else{
				break;
			}
		}
		RingBufferDestroy(decInfo->rbDec);
		RingBufferDestroy(decInfo->rbOut);
		free(decInfo);

	}

	return 0;
}
/*
Name:	ReceiverInit
Description: Initialize the Receiver.
Parameter:
Return:
*/
void ReceiverInit()
{
	/*  initialize values  */

	/*  initialize locks   */

	iCreateMutex(mutexFloorTag, false);

	/*  initialize ring buffers  */
	rbSwitcher = RingBufferInit(DECODER_NUMBER*3,sizeof(decInfo_t*));

	/*  initialize buffer sinks  */
	bufferSink=BufferSinkInit(5, BLOCK_SYMBOL_NUMBER * 2, sizeof(raptorQPacket_t));

	/*  initialize receive info list  */
	_RecvInfoListInit();

	/*  initialize sockets  */
	netAddr_t addrSend, addrRecv;

#ifndef  RELAY_MODE 
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		strcpy(addrSend.ip, ipTx[i]);
		addrSend.port = PORT_DATA_TX + i;
		strcpy(addrRecv.ip, ipRx[i]);
		addrRecv.port = PORT_DATA_RX + i;
		sockRecv[i] = NetworkUDPInit(&addrRecv, &addrSend);// receive
	}
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		strcpy(addrSend.ip, ipRx[i]);
		addrSend.port = PORT_FEEDBACK_TX + i;
		strcpy(addrRecv.ip, ipTx[i]);
		addrRecv.port = PORT_FEEDBACK_RX + i;
		sockFeedback[i] = NetworkUDPInit(&addrSend, &addrRecv);// feedback send
	}
#else	
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		strcpy(addrSend.ip, IP_RELAY);
		addrSend.port = PORT_RELAY_TX + i;
		strcpy(addrRecv.ip, ipRx[i]);
		addrRecv.port = PORT_DATA_RX + i;
		sockRecv[i] = NetworkUDPInit(&addrRecv, &addrSend);// receive
	}
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		strcpy(addrSend.ip, ipRx[i]);
		addrSend.port = PORT_FEEDBACK_TX + i;
		strcpy(addrRecv.ip, IP_RELAY);
		addrRecv.port = PORT_RELAY_FEEDBACK_RX + i;
		sockFeedback[i] = NetworkUDPInit(&addrSend, &addrRecv);// feedback send
	}
#endif // ! RELAY_MODE 

	/*  create a RTCPFeedback route */
	RTCP_Forward_Recv_Init();
	RTCP_Back_Send_Init();
	/*  RaptorQ pre */
	iRaptorQ_pre();

	/* Run I/O  */
	IOOutputInit();
	/*  create threads  */
	for (int i = 0; i < DATA_RX_NUMBER;i++) {
		iCreateThread(hThreadReceiver[i] , _DataReceiverThread, (LPVOID)i);
	}
	
	for (int i = 0; i < DECODER_NUMBER; i++) {
		iCreateThread(hThreadDecoder[i], _DecoderThread, (LPVOID)i);
	}
	iCreateThread(hThreadOutput, _OutputThread, NULL);
}

/*
Name:	ReceiverClose
Description: Close the Receiver and release the memory.
Parameter:
Return:
*/
void ReceiverClose()
{
	stateRun = false;
	for (int i = 0; i < CHANNEL_NUMBER;i++) {
		iThreadJoin(hThreadReceiver[i]);
		iThreadClose(hThreadReceiver[i]);
		iThreadJoin(hThreadFeedback[i]);
		iThreadClose(hThreadFeedback[i]);
	}

	for (int i = 0; i < DECODER_NUMBER; i++) {
		iThreadJoin(hThreadDecoder[i]);
		iThreadClose(hThreadDecoder[i]);
	}

	/*  destory socket  */
	for (int i = 0; i < CHANNEL_NUMBER; i++) {
		NetworkUDPClose(sockRecv[i]);
		NetworkUDPClose(sockFeedback[i]);
	}

	/*  close buffer sink  */
	BufferSinkDestorySink(bufferSink);
	/*  destory receive buffer list  */
	/*  destory lock  */
	/*  Close I/O     */
	IOOutputDeInit();
}
