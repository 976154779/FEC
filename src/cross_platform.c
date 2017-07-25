#include"cross_platform.h"

static struct timeval sysStartTime={0,0};

#ifdef OS_WINDOWS
/*
Name:	_USleep
Description: microseconds sleep
Parameter:
@_delayTime: The time of sleep.
Return:
*/
void _USleep(uint64_t _delayTime) {
	LARGE_INTEGER m_liPerfFreq = { 0 };
	LARGE_INTEGER m_liPerfStart = { 0 };
	QueryPerformanceCounter(&m_liPerfStart);
	LARGE_INTEGER liPerfNow = { 0 };
	double time;
	for (;;)
	{
		QueryPerformanceCounter(&liPerfNow);
		time = (((liPerfNow.QuadPart -
			m_liPerfStart.QuadPart) * 1000000) / (double)m_liPerfFreq.QuadPart);
		if (time >= _delayTime)
			break;
	}
}

#endif


#ifdef OS_LINUX
uint64_t getTimeOfProcess()
{
	struct timeval tv;
	gettimeofday(&tv,NULL);
	tv.tv_sec=tv.tv_sec-sysStartTime.tv_sec;
	tv.tv_usec=tv.tv_usec-sysStartTime.tv_usec;	
	return ((uint32_t)tv.tv_sec*1000+tv.tv_usec/1000);
}

void initStartTime()
{
	gettimeofday(&sysStartTime,NULL);
}
#endif