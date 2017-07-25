/*----------------------------------------------------------------
File Name  : buffersink.c
Author     : Winglab
Data	   : 2016-12-29
Description:
	This module can initialize/destory a buffer sink which can
store specific number of ring buffers. You can get an empty ring
buffer from the buffer sink without waitting for the buffer to
be initizlized because this progress has been processed by one
thread of the buffer sink automaticly. Once the sink is not full,
it can be replenished immediately.
------------------------------------------------------------------*/

#include"buffersink.h"

static uint32_t sinkCount = 0; // The current number of buffer sink

iThreadStdCall_t _SinkBufferSupplyThread(LPVOID lpParam);
/*
Name:	BufferSinkGetBuffer
Description: Get an address of a empty buffer, of which the parameters
of the new  buffer is defined in _pSink.
Parameter:
@_pSink: The structure of the buffer sink.
Return: The address of the empty buffer.
*/
ringBuffer_t *BufferSinkGetBuffer(bufferSink_t* _pSink)
{
	ringBuffer_t *pRb=NULL;
	if (_pSink->id>0) { 
		RingBufferGet(_pSink->rbSink, &pRb);
	}
	return pRb;
}

/*
Name:	BufferSinkDestorySink
Description: Destory the specified buffer sink and release the memory.
Parameter:
@_pSink: The structure of the buffer sink.
Return:
*/
void BufferSinkDestorySink(bufferSink_t *_pSink)
{
	ringBuffer_t *pRb;
	/*  terminate thread  */
	_pSink->id = 0;
	iThreadJoin(_pSink->hWnd);
	iThreadClose(_pSink->hWnd);
	/*  destory ring buffer  */
	while (RingBufferTryGet(_pSink->rbSink,&pRb)) 
	{
		RingBufferDestroy(pRb);
	}
	RingBufferDestroy(_pSink->rbSink);
	sinkCount--;
	free(_pSink);
}
/*
Name:	_SinkBufferSupplyThread
Description: The thread to initilize new ring buffers to replenished
the not full sink.
Parameter:
@lpParam: The pointer of the structure of the buffer sink.
Return: 
*/
iThreadStdCall_t _SinkBufferSupplyThread(LPVOID lpParam)
{
	bufferSink_t *pSink=(bufferSink_t*)lpParam;
	ringBuffer_t *pRb;
	while (pSink->id) {
		pRb = RingBufferInit(pSink->buffLen, pSink->buffUnitSize);
		RingBufferPut(pSink->rbSink, &pRb);
	}
	return 0;
}
/*
Name:	BufferSinkInit
Description: Initialize a new buffer sink .
Parameter:
@_sinkLen: The max number of buffer in the sink.
@_buffLen: The number of storage unit in a buffer.
@_buffUnitSize: The size of one storage unit in buffer.
Return: the pointer of the structure of the buffer sink
*/
bufferSink_t *BufferSinkInit(uint32_t _sinkLen, uint32_t _buffLen, uint32_t _buffUnitSize)
{
	bufferSink_t *pSink = (bufferSink_t *)malloc(sizeof(bufferSink_t));
	if (pSink == NULL) {
		perror("Error: malloc sink unsuccessfully.\r\n ");
		exit(1);
	}
	/* set values */
	pSink->id = ++sinkCount;
	pSink->sinkLen = _sinkLen;
	pSink->buffLen = _buffLen;
	pSink->buffUnitSize = _buffUnitSize;

	/* initialize address ring buffer */
	ringBuffer_t *pRb = RingBufferInit(pSink->sinkLen, sizeof(ringBuffer_t*));
	if (pRb == NULL) {
		perror("can not init sink buffer! \r\n");
		exit(1);
	}
	pSink->rbSink = pRb;
	/* create buffer-supply thread */
	iCreateThread(pSink->hWnd, _SinkBufferSupplyThread, pSink);

	return pSink;
}