#pragma once
#include"stdint.h"
#define OS_WINDOWS
//#define OS_LINUX

#ifdef  OS_WINDOWS
	#include<WinSock2.h>
	#ifndef _WINSOCKAPI_
		#define _WINSOCKAPI_
	#endif 
	#include<Windows.h>
	#include <time.h>
#endif //OS_WINDOWS
#ifdef OS_LINUX
	#include <pthread.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <semaphore.h>
	#include <sys/time.h>
	#include <unistd.h>
	#include <stdlib.h>
#endif // OS_LINUX

#ifdef  OS_WINDOWS
#define iMUTEX_INIT_VALUE						NULL
#define iSEMAPHORE_INIT_VALUE					NULL
#define iThread_t								HANDLE
#define iMutex_t								HANDLE
#define iSemaphore_t							HANDLE
#define iThreadStdCall_t						DWORD WINAPI
#define iClock_t								clock_t
#define iCreateThread(hwnd,func,param)			hwnd=CreateThread(NULL, 0, func, (LPVOID)param, 0, NULL)
#define iThreadJoin(hwnd)						WaitForSingleObject(hwnd, INFINITE)
#define iThreadClose(hwnd)						CloseHandle(hwnd)
#define iCreateMutex(mutex,owner)				mutex=CreateMutex(NULL,owner,NULL)
#define iMutexLock(mutex)						WaitForSingleObject(mutex, INFINITE)
#define iMutexTryLock(mutex)					WaitForSingleObject(mutex,0L)
#define iMutexUnlock(mutex)						ReleaseMutex(mutex)
#define iMutexDestory(mutex)					CloseHandle(mutex)
#define iCreateSemaphore(sema,initCtn,maxCtn)	sema=CreateSemaphore(NULL, initCtn, maxCtn, NULL)
#define iSemaphoreWait(sema)					WaitForSingleObject(sema, INFINITE)
#define iSemaphoreTryWait(sema)					WaitForSingleObject(sema, 0L)
#define iSemaphoreTimedWait(sema,_timeOut)		WaitForSingleObject(sema,_timeOut) //add for function RingBufferTimedGet
#define iSemaphorePost(sema,num)				ReleaseSemaphore(sema, num, NULL)
#define iSemaphoreDestory(sema)					CloseHandle(sema)
#define iSleep(time)							Sleep(time)
#define iGetTime()								clock()
#define iCloseSocket(sock)						closesocket(sock)

void 	 _USleep(uint64_t _delayTime);

#endif // OS_WINDOWS

#ifdef OS_LINUX

#define iMUTEX_INIT_VALUE						PTHREAD_MUTEX_INITIALIZER
#define iSEMAPHORE_INIT_VALUE					(0)
#define WAIT_OBJECT_0							(0)
#define SOCKET_ERROR							(-1)
typedef void*									LPVOID;
typedef int										SOCKET;
typedef struct sockaddr_in						SOCKADDR_IN;
typedef	struct sockaddr							SOCKADDR;
typedef int										SOCKET;
#define iThread_t								pthread_t
#define iMutex_t								pthread_mutex_t
#define iSemaphore_t							sem_t
#define iThreadStdCall_t						void*
#define iClock_t								uint64_t
#define iCreateThread(hwnd,func,param)			pthread_create(&hwnd, NULL, func, param)//hwnd=CreateThread(NULL, 0, ptr, (LPVOID)param, 0, NULL)
#define iThreadJoin(hwnd)						pthread_join(hwnd, NULL)
#define iThreadClose(hwnd)						pthread_cancel(hwnd)
#define iCreateMutex(mutex,owner)				pthread_mutex_init(&mutex, NULL)
#define iMutexLock(mutex)						pthread_mutex_lock(&mutex)
#define iMutexTryLock(mutex)					pthread_mutex_trylock(&mutex)
#define iMutexUnlock(mutex)						pthread_mutex_unlock(&mutex)
#define iMutexDestory(mutex)					pthread_mutex_destroy(&mutex)
#define iCreateSemaphore(sema,initCtn,maxCtn)	sem_init(&sema, 0, initCtn)
#define iSemaphoreWait(sema)					sem_wait(&sema)
#define iSemaphoreTryWait(sema)					sem_trywait(&sema)
#define iSemaphoreTimedWait(sema,_timeOut)		sem_trywait(&sema,_timeOut)	//add for function RingBufferTimedGet
#define iSemaphorePost(sema,num)				sem_post(&sema)
#define iSemaphoreDestory(sema)					sem_destroy(&sema)

#define iSleep(time)							usleep((long)time*1000)
#define iGetTime()								getTimeOfProcess()
#define iCloseSocket(sock)						close(sock)

uint64_t getTimeOfProcess();
void 	 initStartTime();

#endif // 

