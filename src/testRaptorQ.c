#include"iRaptorQ.h"
#include"io.h"
#include"err.h"
#include<Windows.h>

DWORD WINAPI _EncThread1(LPVOID lpParam) {
	uint8_t encID = (uint8_t)lpParam;
	uint32_t esi;
	uint8_t *pBlockData = malloc(BLOCK_SIZE);
	raptorQBlock_t *pBlock = (raptorQBlock_t*)malloc(sizeof(raptorQBlock_t));
	raptorQPacket_t *packetRepair = (raptorQPacket_t*)malloc(sizeof(raptorQPacket_t));
	struct RaptorQ_ptr *enc;

	for (int i = 0; i < 1; i++) {
		uint32_t BSN = rand() % 30 + 2;
		BSN = 32;
		printf("[%u][start%u] a  tag:%u BSN:%d\r\n", (uint32_t)clock(), encID, i,BSN);
		/*  RaptorQ pre */
		//printf("[%u][start%u] b  tag:%u\r\n", (uint32_t)clock(), encID, i);
		enc = iRaptorQ_Enc(pBlock->data, BSN*SYMBOL_SIZE);
		raptorQ_OTI_t myOTI;
		uint8_t res = iRaptorQ_GetOTI(enc, &myOTI);
		//printf("[%u][start%u] c  tag:%u res=%d\r\n", (uint32_t)clock(), encID, i,res);
		RaptorQ_precompute(enc, 0, TRUE);
		//Sleep(40);
		//printf("[%u][start%u] d  tag:%u\r\n", (uint32_t)clock(), encID, i);
		esi = BSN;
		while (1) {
			if (esi < BSN + 10) {
				printf("[%u][enc%u] esi:%u  tag:%u\r\n", (uint32_t)clock(), encID, esi, i);
				iRaptorQ_encode_id(enc, packetRepair, pBlock->tag, 0, esi++);	//generate repair symbols.
				printf("[%u][over%u] esi:%u  tag:%u\r\n", (uint32_t)clock(), encID, esi - 1, i);
			}
			else
			{
				break;
			}
		}
		/*  free to idle state  */
		RaptorQ_free(&enc);
	}
	_endthreadex(0);
	return 0;
}


void main()
{
	HWND hThreadEnc[16];
	printf("start!\r\n");
	iRaptorQ_pre();

	for (int i = 0; i < 8; i++)
	{
		hThreadEnc[i] = (HWND)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)_EncThread1, (void*)i, 0, NULL);  // returns the thread identifier 
	}
	for (int i = 0; i < 8; i++)
	{
		WaitForSingleObject(hThreadEnc[i],INFINITE);
		CloseHandle(hThreadEnc[i]);
	}
	printf("over!\r\n");
}