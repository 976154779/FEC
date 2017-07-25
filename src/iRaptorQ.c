/*----------------------------------------------------------------
File Name  : iRaptorQ.h
Author     : Winglab
Data	   : 2016-12-29
Description:
Support the middle interface between this Project and
libRaptorQ .
------------------------------------------------------------------*/
#include"iRaptorQ.h"
static raptorQ_OTI_t OTI;

void iRaptorQ_pre()
{
	struct RaptorQ_ptr *enc;
	void *blk;

	if ((blk = malloc(BLOCK_SIZE)) == NULL) {
		printf("fail to allocate space\n");
		exit(1);
	}

	if ((enc = RaptorQ_Enc(ENC_8, blk, BLOCK_SIZE, SYMBOL_SIZE, SYMBOL_SIZE, MAX_MEM)) == NULL) {
		printf("fail to initialize encoder\n");
		exit(1);
	}

	if (RaptorQ_blocks(enc) != 1) {
		printf("Number of blocks != 1\n");
		exit(1);
	}

	OTI.OTI_Comm = RaptorQ_OTI_Common(enc);
	OTI.OTI_Spec = RaptorQ_OTI_Scheme(enc);

	RaptorQ_free(&enc);

	free(blk);
}

uint8_t iRaptorQ_GetOTI(struct RaptorQ_ptr *_enc, raptorQ_OTI_t *myOTI)
{
	uint8_t res;
	if ((res=RaptorQ_blocks(_enc)) != 1) {
		printf("[ERR]Number of blocks != 1  oti\n");
		perror("Number of blocks != 1  oti\n");
		exit(1);
	}
	myOTI->OTI_Comm = RaptorQ_OTI_Common(_enc);
	myOTI->OTI_Spec = RaptorQ_OTI_Scheme(_enc);
	return res;
}

struct RaptorQ_ptr *iRaptorQ_Enc(void *data, int len)
{
	return RaptorQ_Enc(ENC_8, data, len, SYMBOL_SIZE, SYMBOL_SIZE, MAX_MEM);
}

struct RaptorQ_ptr *iRaptorQ_Dec(raptorQ_OTI_t _myOTI)
{
	return RaptorQ_Dec(DEC_8, _myOTI.OTI_Comm, _myOTI.OTI_Spec);
}

void iRaptorQ_encode_id(struct RaptorQ_ptr *enc, raptorQPacket_t  *ppac, uint32_t tag, uint8_t sbn, uint32_t esi)
{
	ppac->tag = tag;
	ppac->id = RaptorQ_id(esi, sbn);

	void *ptr = ppac->data;

	if (RaptorQ_encode_id(enc, (void **)&ptr, SYMBOL_SIZE, ppac->id) != SYMBOL_SIZE) {
		perror("failed to encode symbol\n");
		exit(1);
	}
}

int iRaptorQ_add_symbol_id(struct RaptorQ_ptr *dec, raptorQPacket_t *ppac)
{
	void *ptr = ppac->data;
	return RaptorQ_add_symbol_id(dec, (void **)&ptr, SYMBOL_SIZE, ppac->id);
}

int iRaptorQ_decode(struct RaptorQ_ptr *dec, raptorQBlock_t *pblk,uint64_t block_size)
{
	void *ptr = pblk->data;
	return (RaptorQ_decode(dec, (void **)&ptr, (size_t)block_size) == (size_t)block_size);
}
void iRaptorQCreateBlock(raptorQBlock_t *_pBlock, uint8_t *_pBlockData, uint32_t _tag)
{
	_pBlock->tag = _tag;
	memcpy(_pBlock->data, _pBlockData, BLOCK_SIZE);
}
void iRaptorQPack(ringBuffer_t * _pRb, uint8_t *data, uint32_t _tag, uint32_t _id, uint8_t _foreBSN)
{
	raptorQPacket_t pPacket;
	pPacket.tag = _tag;
	pPacket.id = _id;
	pPacket.oti = OTI;
	pPacket.forTagBSN = _foreBSN;
	pPacket.type = PACKET_TYPE_SOURCE;
	pPacket.block_symbol_number = BLOCK_SYMBOL_NUMBER;
	memcpy(pPacket.data,data,SYMBOL_SIZE);
	RingBufferPut(_pRb,&pPacket);
}