/*----------------------------------------------------------------
File Name  : ringbuffer.c
Author     : Winglab
Data	   : 2016-12-29
Description:
You can use this module to create and initialize some ring
buffers and get or put data to the buffer.
------------------------------------------------------------------*/
#include"ringbuffer.h"

/****************************************************************
Name: RingBufferInit
Description:
	Allocate a ring buffer, of which the number and the 
	size of storage unit are specified by the parameter.
Parameter:
	_number: the number of storage unit in a ringbuffer.
	_sizeUnit  : the byte size of each storage unit.
Return:
	A pointer to the allocated ring buffer.
*****************************************************************/
ringBuffer_t *RingBufferInit(uint32_t _number,uint32_t _sizeUnit)
{
	ringBuffer_t *pRb = (ringBuffer_t*)malloc(sizeof(ringBuffer_t) + _number * sizeof(void *));
	if (pRb == NULL) {
		perror("ring buffer init err !\r\n");
		exit(1);
	}
	pRb->number = _number;
	pRb->unitSize = _sizeUnit;
	pRb->head = 0;
	pRb->tail = 0;

	for (uint32_t i = 0; i < pRb->number; i++)
		pRb->buff[i] = malloc(pRb->unitSize);

	iCreateMutex(pRb->hMutexHead, false);
	//#define iCreateMutex(mutex,owner)				mutex=CreateMutex(NULL,owner,NULL)
	iCreateMutex(pRb->hMutexTail, false);
	iCreateSemaphore(pRb->semEmpty, pRb->number, pRb->number);
	//#define iCreateSemaphore(sema,initCtn,maxCtn)	sema=CreateSemaphore(NULL, initCtn, maxCtn, NULL)
	iCreateSemaphore(pRb->semOccupied, 0, pRb->number);

	return pRb;
}
/****************************************************************
Name: RingBufferPut
Description:
	Put data with specified length into a ring buffer.
Parameter:
	_pRb  : A pointer to the ringbuffer.
	_data : The address of data to be storeed in the ring buffer.
Return:

*****************************************************************/
void RingBufferPut(ringBuffer_t *_pRb,void *_data)
{
	if (_pRb == NULL) { 
		return ; 
	}
	void *pUnit;
	iSemaphoreWait(_pRb->semEmpty);
	//#define iSemaphoreWait(sema)					WaitForSingleObject(sema, INFINITE)
	iMutexLock(_pRb->hMutexHead);
	pUnit = _pRb->buff[_pRb->head];						//�¼����unit�ŵ�ͷ��
	memcpy(pUnit, _data, _pRb->unitSize);				// size checking
	if (++_pRb->head == _pRb->number) {
		_pRb->head = 0;
	}
	iMutexUnlock(_pRb->hMutexHead);
	iSemaphorePost(_pRb->semOccupied, 1);
	//#define iSemaphorePost(sema,num)				ReleaseSemaphore(sema, num, NULL)
}
uint32_t RingBufferTryPut(ringBuffer_t *_pRb, void *_data)
{
	if (_pRb == NULL) {
		return 0;
	}
	void *pUnit;
	if (iSemaphoreTryWait(_pRb->semEmpty) == WAIT_OBJECT_0) {
		iMutexLock(_pRb->hMutexHead);
		pUnit = _pRb->buff[_pRb->head];
		memcpy(pUnit, _data, _pRb->unitSize);				// size checking
		if (++_pRb->head == _pRb->number) {
			_pRb->head = 0;
		}
		iMutexUnlock(_pRb->hMutexHead);
		iSemaphorePost(_pRb->semOccupied, 1);
		return 1;
	}
	return 0;
}

/****************************************************************
Name: RingBufferGet
Description:
	Get data with specified length from a ring buffer.
Parameter:
	_pRb  : A pointer to the ringbuffer.
	_data : the address to stroe the got data.
Return:

*****************************************************************/
void RingBufferGet(ringBuffer_t *_pRb, void *_data)
{
	if (_pRb == NULL) {
		return;
	}
	void *pUnit;
	iSemaphoreWait(_pRb->semOccupied);//����Դ���Ƿ����0��С��0������
	iMutexLock(_pRb->hMutexTail);//������ ����
	pUnit = _pRb->buff[_pRb->tail];//ָ��ringBuffer��β��
	memcpy(_data, pUnit, _pRb->unitSize);//��pUnit�����ݸ��Ƶ�data��ȥ
	if (++_pRb->tail == _pRb->number) { 
		_pRb->tail = 0;
	}
	iMutexUnlock(_pRb->hMutexTail);
	iSemaphorePost(_pRb->semEmpty,1);
}

uint32_t RingBufferTryGet(ringBuffer_t *_pRb, void *_data)
{
	if (_pRb == NULL) {
		return 0;
	}
	void *pUnit;
		//WAIT_OBJECT_0����ʾ��ȴ��Ķ��󣨱����̡߳������壩�ѵ�����ִ����ɻ�����ͷ�
	if (iSemaphoreTryWait(_pRb->semOccupied)==WAIT_OBJECT_0) {
		iMutexLock(_pRb->hMutexTail);
		pUnit = _pRb->buff[_pRb->tail];
		memcpy(_data, pUnit, _pRb->unitSize);//ringbuffer��������ݣ�UDP���Ѿ���д�뵽_data��ָ��ĵ�ַ����ȥ��
		if (++_pRb->tail == _pRb->number)
			_pRb->tail = 0;
		iMutexUnlock(_pRb->hMutexTail);
		iSemaphorePost(_pRb->semEmpty,1);
		return 1;
	}

	return 0;
}

//���� RingBufferTimedGet(ringBuffer_t _rb, void *_data, iClock_t _timeOut)
//@˵���������趨������ʱ��Buffer Get��������ʱ���ͷ�������
//@ϸ�ڣ�ѧϰʹ��WaitForSingleObject(Linux��sem_timedwait����ͬ)�趨��ʱʱ�䡣
//��ringbuffer��ȡ��һ��dataunit�ŵ�_data��ָ��ĵ�ַ��ȥ
uint32_t RingBufferTimedGet(ringBuffer_t *_pRb, void *_data, iClock_t _timeOut) {  //iClock_t �� long ����
	if (_pRb == NULL) {
		return 0;
	}
	void *pUnit;
	//WAIT_TIMEOUT 0x00000102���ȴ���ʱ
	if (iSemaphoreTimedWait(_pRb->semOccupied,_timeOut) != WAIT_TIMEOUT) {
		iMutexLock(_pRb->hMutexTail);
		pUnit = _pRb->buff[_pRb->tail];
		memcpy(_data, pUnit, _pRb->unitSize);
		if (++_pRb->tail == _pRb->number)
			_pRb->tail = 0;
		iMutexUnlock(_pRb->hMutexTail);
		iSemaphorePost(_pRb->semEmpty, 1);
		return 1;
	}
	return 0;//��ʱ����0
}

void RingBufferFlush(ringBuffer_t *_pRb)
{
	while (iSemaphoreTryWait(_pRb->semOccupied) == WAIT_OBJECT_0) {
		iMutexLock(_pRb->hMutexTail);
		if (++_pRb->tail == _pRb->number)
			_pRb->tail = 0;
		iMutexUnlock(_pRb->hMutexTail);
		iSemaphorePost(_pRb->semEmpty,1);
	}
}

void RingBufferDestroy(ringBuffer_t *_pRb)
{
	if (_pRb == NULL) {
		return;
	}
	iMutexDestory(_pRb->hMutexTail);
	iMutexDestory(_pRb->hMutexHead);
	iSemaphoreDestory(_pRb->semEmpty);
	iSemaphoreDestory(_pRb->semOccupied);
	for (uint32_t i = 0; i < _pRb->number; i++) {
		free(_pRb->buff[i]);
	}
	free(_pRb);
	_pRb = NULL;
}

