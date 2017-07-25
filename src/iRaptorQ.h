/*----------------------------------------------------------------
File Name  : iRaptorQ.h
Author     : Winglab
Data	   : 2016-12-29
Description:
	Support the middle interface between this Project and
libRaptorQ .
------------------------------------------------------------------*/
#pragma once
#include"cross_platform.h"
#include<string.h>
#include<stdint.h>
#include<stdlib.h>
#include<stdio.h>
#include<stdbool.h>
#include"RaptorQ/cRaptorQ.h"
#include"parameter.h" 
#include"ringbuffer.h"

enum PACKET_TYPE
{
	PACKET_TYPE_SOURCE,
	PACKET_TYPE_SOURCE_FINISHED,
	PACKET_TYPE_REPAIR,
	PACKET_TYPE_BSN,
};

typedef struct raptorQ_OTI_t {
	RaptorQ_OTI_Common_Data OTI_Comm;
	RaptorQ_OTI_Scheme_Specific_Data OTI_Spec;
} raptorQ_OTI_t;

typedef struct raptorQPacket_t {
	uint32_t tag;
	uint32_t id;
	iClock_t ts[4];
	raptorQ_OTI_t oti;
	uint8_t block_symbol_number;
	uint8_t forTagBSN;
	uint8_t type;
	uint8_t data[SYMBOL_SIZE];
}raptorQPacket_t;

typedef struct raptorQFeedbackInfo_t {
	uint32_t feedbackID;	//identify a feedback of the same packet which has the same tag and id 
	uint32_t tag;
	uint32_t id;
	iClock_t ts[4];
	uint32_t count;
}raptorQFeedbackInfo_t;

typedef struct raptorQBlock_t {
	uint32_t tag;
	uint8_t  data[BLOCK_SIZE];
}raptorQBlock_t;

/*
*	necessary preparations before initializing encoder/decoder
*/
void iRaptorQ_pre();

/*
*	Initialize a RaptorQ encoder
*	@data: point to the data that will be encoded
*	@len: specify the original data length
*/
struct RaptorQ_ptr *iRaptorQ_Enc(void *data, int len);

/*
*	Initialize a RaptorQ decoder
*/
struct RaptorQ_ptr *iRaptorQ_Dec(raptorQ_OTI_t _myOTI);

/*
*	generate an encoded symbol
*	@enc: the RaptorQ encoder instance used for encoding
*	@ppac: where the generated symbol would be stored
*	@tag: specify which data block this packet belongs to
*	@sbn: set 0 only
*	@esi: identify the symbol
*/
void iRaptorQ_encode_id(struct RaptorQ_ptr *enc, raptorQPacket_t *ppac,
	uint32_t tag, uint8_t sbn, uint32_t esi);

/*
*	add an encoded symbol to the RaptorQ decoder instance
*	@dec: the RaptorQ decoder instance used for decoding
*	@ppac: where the received symbol is stored
*/
int iRaptorQ_add_symbol_id(struct RaptorQ_ptr *dec, raptorQPacket_t *ppac);

/*
*	recover the original data block
*	@dec: the RaptorQ decoder instance used for decoding
*	@pblk: where the recovered original data block would be stored
*/
int iRaptorQ_decode(struct RaptorQ_ptr *dec, raptorQBlock_t *pblk,uint64_t block_size);
void iRaptorQCreateBlock(raptorQBlock_t *_pBlock, uint8_t *_pBlockData, uint32_t _tag);
void iRaptorQPack(ringBuffer_t * _pRb, uint8_t *data, uint32_t _tag, uint32_t _id, uint8_t _foreBSN);
uint8_t iRaptorQ_GetOTI(struct RaptorQ_ptr *_enc, raptorQ_OTI_t *myOTI); 
