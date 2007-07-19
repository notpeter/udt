/*****************************************************************************
Copyright © 2001 - 2007, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) version 4

National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.ncdm.uic.edu/

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at
your option) any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
*****************************************************************************/

/*****************************************************************************
This file contains implementation of UDT common routines of timer,
mutex facility, and exception processing.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 06/07/2007
*****************************************************************************/


#ifndef WIN32
   #include <unistd.h>
   #include <cstring>
   #include <cstdlib>
   #include <cerrno>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif
#include <cmath>
#include "common.h"

uint64_t CTimer::s_ullCPUFrequency = CTimer::readCPUFrequency();
#ifndef WIN32
   pthread_mutex_t CTimer::m_EventLock = PTHREAD_MUTEX_INITIALIZER;
   pthread_cond_t CTimer::m_EventCond = PTHREAD_COND_INITIALIZER;
#else
   pthread_mutex_t CTimer::m_EventLock = CreateMutex(NULL, false, NULL);
   pthread_cond_t CTimer::m_EventCond = CreateEvent(NULL, false, false, NULL);
#endif

CTimer::CTimer()
{
   #ifndef WIN32
      pthread_mutex_init(&m_TickLock, NULL);
      pthread_cond_init(&m_TickCond, NULL);
   #else
      m_TickLock = CreateMutex(NULL, false, NULL);
      m_TickCond = CreateEvent(NULL, false, false, NULL);
   #endif
}

CTimer::~CTimer()
{
   #ifndef WIN32
      pthread_mutex_destroy(&m_TickLock);
      pthread_cond_destroy(&m_TickCond);
   #else
      CloseHandle(m_TickLock);
      CloseHandle(m_TickCond);
   #endif
}

void CTimer::rdtsc(uint64_t &x)
{
   #ifdef WIN32
      if (!QueryPerformanceCounter((LARGE_INTEGER *)&x))
         x = getTime();
   #elif IA32
      uint32_t lval, hval;
      //asm volatile ("push %eax; push %ebx; push %ecx; push %edx");
      //asm volatile ("xor %eax, %eax; cpuid");
      asm volatile ("rdtsc" : "=a" (lval), "=d" (hval));
      //asm volatile ("pop %edx; pop %ecx; pop %ebx; pop %eax");
      x = hval;
      x = (x << 32) | lval;
   #elif IA64
      asm ("mov %0=ar.itc" : "=r"(x) :: "memory");
   #elif AMD64
      uint32_t lval, hval;
      asm ("rdtsc" : "=a" (lval), "=d" (hval));
      x = hval;
      x = (x << 32) | lval;
   #else
      // use system call to read time clock for other archs
      timeval t;
      gettimeofday(&t, 0);
      x = (uint64_t)t.tv_sec * (uint64_t)1000000 + (uint64_t)t.tv_usec;
   #endif
}

uint64_t CTimer::readCPUFrequency()
{
   #ifdef WIN32
      int64_t ccf;
      if (QueryPerformanceFrequency((LARGE_INTEGER *)&ccf))
         return ccf / 1000000;
      else
         return 1;
   #elif IA32 || IA64 || AMD64
      uint64_t t1, t2;

      rdtsc(t1);
      usleep(100000);
      rdtsc(t2);

      // CPU clocks per microsecond
      return (t2 - t1) / 100000;
   #else
      return 1;
   #endif
}

uint64_t CTimer::getCPUFrequency()
{
   return s_ullCPUFrequency;
}

void CTimer::sleep(const uint64_t& interval)
{
   uint64_t t;
   rdtsc(t);

   // sleep next "interval" time
   sleepto(t + interval);
}

void CTimer::sleepto(const uint64_t& nexttime)
{
   // Use class member such that the method can be interrupted by others
   m_ullSchedTime = nexttime;

   uint64_t t;
   rdtsc(t);

   while (t < m_ullSchedTime)
   {
      #ifndef NO_BUSY_WAITING
         #ifdef IA32
            __asm__ volatile ("pause; rep; nop; nop; nop; nop; nop;");
         #elif IA64
            __asm__ volatile ("nop 0; nop 0; nop 0; nop 0; nop 0;");
         #elif AMD64
            __asm__ volatile ("nop; nop; nop; nop; nop;");
         #endif
      #else
         #ifndef WIN32
            timeval now;
            timespec timeout;
            gettimeofday(&now, 0);
            if (now.tv_usec < 990000)
            {
               timeout.tv_sec = now.tv_sec;
               timeout.tv_nsec = (now.tv_usec + 10000) * 1000;
            }
            else
            {
               timeout.tv_sec = now.tv_sec + 1;
               timeout.tv_nsec = (now.tv_usec + 10000 - 1000000) * 1000;
            }
            pthread_mutex_lock(&m_TickLock);
            pthread_cond_timedwait(&m_TickCond, &m_TickLock, &timeout);
            pthread_mutex_unlock(&m_TickLock);
         #else
            WaitForSingleObject(m_TickCond, 1);
         #endif
      #endif

      rdtsc(t);
   }
}

void CTimer::interrupt()
{
   // schedule the sleepto time to the current CCs, so that it will stop
   rdtsc(m_ullSchedTime);

   tick();
}

void CTimer::tick()
{
   #ifndef WIN32
      pthread_cond_signal(&m_TickCond);
   #else
      SetEvent(m_TickCond);
   #endif
}

uint64_t CTimer::getTime()
{
   #ifndef WIN32
      timeval t;
      gettimeofday(&t, 0);
      return t.tv_sec * 1000000ULL + t.tv_usec;
   #else
      LARGE_INTEGER ccf;
      if (QueryPerformanceFrequency(&ccf))
      {
         LARGE_INTEGER cc;
         QueryPerformanceCounter(&cc);
         return (cc.QuadPart / ccf.QuadPart) * 1000000ULL + (cc.QuadPart % ccf.QuadPart) / (ccf.QuadPart / 1000000);
      }
      else
      {
         FILETIME ft;
         GetSystemTimeAsFileTime(&ft);
         return ((((uint64_t)ft.dwHighDateTime) << 32) + ft.dwLowDateTime) / 10;
      }
   #endif
}

void CTimer::triggerEvent()
{
   #ifndef WIN32
      pthread_cond_signal(&m_EventCond);
   #else
      SetEvent(m_EventCond);
   #endif
}

void CTimer::waitForEvent()
{
   #ifndef WIN32
      timeval now;
      timespec timeout;
      gettimeofday(&now, 0);
      if (now.tv_usec < 990000)
      {
         timeout.tv_sec = now.tv_sec;
         timeout.tv_nsec = (now.tv_usec + 10000) * 1000;
      }
      else
      {
         timeout.tv_sec = now.tv_sec + 1;
         timeout.tv_nsec = (now.tv_usec + 10000 - 1000000) * 1000;
      }
      pthread_mutex_lock(&m_EventLock);
      pthread_cond_timedwait(&m_EventCond, &m_EventLock, &timeout);
      pthread_mutex_unlock(&m_EventLock);
   #else
      WaitForSingleObject(m_EventCond, 1);
   #endif
}


//
// Automatically lock in constructor
CGuard::CGuard(pthread_mutex_t& lock):
m_Mutex(lock)
{
   #ifndef WIN32
      m_iLocked = pthread_mutex_lock(&m_Mutex);
   #else
      m_iLocked = WaitForSingleObject(m_Mutex, INFINITE);
   #endif
}

// Automatically unlock in destructor
CGuard::~CGuard()
{
   #ifndef WIN32
      if (0 == m_iLocked)
         pthread_mutex_unlock(&m_Mutex);
   #else
      if (WAIT_FAILED != m_iLocked)
         ReleaseMutex(m_Mutex);
   #endif
}

//
CUDTException::CUDTException(int major, int minor, int err):
m_iMajor(major),
m_iMinor(minor)
{
   if (-1 == err)
      #ifndef WIN32
         m_iErrno = errno;
      #else
         m_iErrno = GetLastError();
      #endif
   else
      m_iErrno = err;
}

CUDTException::CUDTException(const CUDTException& e):
m_iMajor(e.m_iMajor),
m_iMinor(e.m_iMinor),
m_iErrno(e.m_iErrno)
{
}

CUDTException::~CUDTException()
{
}

const char* CUDTException::getErrorMessage()
{
   // translate "Major:Minor" code into text message.

   switch (m_iMajor)
   {
      case 0:
        strcpy(m_pcMsg, "Success");
        break;

      case 1:
        strcpy(m_pcMsg, "Connection setup failure");

        switch (m_iMinor)
        {
        case 1:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "connection time out");

           break;

        case 2:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "connection rejected");

           break;

        case 3:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "unable to create/configure UDP socket");

           break;

        case 4:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "abort for security reasons");

           break;

        default:
           break;
        }

        break;

      case 2:
        switch (m_iMinor)
        {
        case 1:
           strcpy(m_pcMsg, "Connection was broken");

           break;

        case 2:
           strcpy(m_pcMsg, "Connection does not exist");

           break;

        default:
           break;
        }

        break;

      case 3:
        strcpy(m_pcMsg, "System resource failure");

        switch (m_iMinor)
        {
        case 1:
           strcpy(m_pcMsg, "unable to create new threads");

           break;

        case 2:
           strcpy(m_pcMsg, "unable to allocate buffers");

           break;

        default:
           break;
        }

        break;

      case 4:
        strcpy(m_pcMsg, "File system failure");

        switch (m_iMinor)
        {
        case 1:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "cannot seek read position");

           break;

        case 2:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "failure in read");

           break;

        case 3:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "cannot seek write position");

           break;

        case 4:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "failure in write");

           break;

        default:
           break;
        }

        break;

      case 5:
        strcpy(m_pcMsg, "Operation not supported");
 
        switch (m_iMinor)
        {
        case 1:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "Cannot do this operation on a BOUND socket");

           break;

        case 2:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "Cannot do this operation on a CONNECTED socket");

           break;

        case 3:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "Bad parameters");

           break;

        case 4:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "Invalid socket ID");

           break;

        case 5:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "Cannot do this operation on an UNBOUND socket");

           break;

        case 6:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "Socket is not in listening state");

           break;

        case 7:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "Listen/accept is not supported in rendezous connection setup");

           break;

        case 8:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "Cannot call connect on UNBOUND socket in rendezvous connection setup");

           break;

        case 9:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "This operation is not supported in SOCK_STREAM mode");

           break;

        case 10:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "This operation is not supported in SOCK_DGRAM mode");

           break;

        default:
           break;
        }

        break;

     case 6:
        strcpy(m_pcMsg, "Non-blocking call failure");

        switch (m_iMinor)
        {
        case 1:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "no buffer available for sending");

           break;

        case 2:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "no data available for reading");

           break;

        default:
           break;
        }

        break;

      default:
        strcpy(m_pcMsg, "Unknown error");
   }

   // Adding "errno" information
   if (0 < m_iErrno)
   {
      strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
      #ifndef WIN32
         strncpy(m_pcMsg + strlen(m_pcMsg), strerror(m_iErrno), 1024 - strlen(m_pcMsg) - 2);
      #else
         LPVOID lpMsgBuf;
         FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, m_iErrno, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
         strncpy(m_pcMsg + strlen(m_pcMsg), (char*)lpMsgBuf, 1024 - strlen(m_pcMsg) - 2);
         LocalFree(lpMsgBuf);
      #endif
   }

   // period
   #ifndef WIN32
      strcpy(m_pcMsg + strlen(m_pcMsg), ".");
   #endif

   return m_pcMsg;
}

const int CUDTException::getErrorCode() const
{
   return m_iMajor * 1000 + m_iMinor;
}

void CUDTException::clear()
{
   m_iMajor = 0;
   m_iMinor = 0;
   m_iErrno = 0;
}

const int CUDTException::SUCCESS = 0;
const int CUDTException::ECONNSETUP = 1000;
const int CUDTException::ENOSERVER = 1001;
const int CUDTException::ECONNREJ = 1002;
const int CUDTException::ESOCKFAIL = 1003;
const int CUDTException::ESECFAIL = 1004;
const int CUDTException::ECONNFAIL = 2000;
const int CUDTException::ECONNLOST = 2001;
const int CUDTException::ENOCONN = 2002;
const int CUDTException::ERESOURCE = 3000;
const int CUDTException::ETHREAD = 3001;
const int CUDTException::ENOBUF = 3002;
const int CUDTException::EFILE = 4000;
const int CUDTException::EINVRDOFF = 4001;
const int CUDTException::ERDPERM = 4002;
const int CUDTException::EINVWROFF = 4003;
const int CUDTException::EWRPERM = 4004;
const int CUDTException::EINVOP = 5000;
const int CUDTException::EBOUNDSOCK = 5001;
const int CUDTException::ECONNSOCK = 5002;
const int CUDTException::EINVPARAM = 5002;
const int CUDTException::EINVSOCK = 5003;
const int CUDTException::EUNBOUNDSOCK = 5004;
const int CUDTException::ENOLISTEN = 5005;
const int CUDTException::ERDVNOSERV = 5006;
const int CUDTException::ERDVUNBOUND = 5007;
const int CUDTException::ESTREAMILL = 5008;
const int CUDTException::EDGRAMILL = 5009;
const int CUDTException::EASYNCFAIL = 6000;
const int CUDTException::EASYNCSND = 6001;
const int CUDTException::EASYNCRCV = 6002;
const int CUDTException::EUNKNOWN = -1;


//
bool CIPAddress::ipcmp(const sockaddr* addr1, const sockaddr* addr2, const int& ver)
{
   if (AF_INET == ver)
   {
      sockaddr_in* a1 = (sockaddr_in*)addr1;
      sockaddr_in* a2 = (sockaddr_in*)addr2;

      if ((a1->sin_port == a2->sin_port) && (a1->sin_addr.s_addr == a2->sin_addr.s_addr))
         return true;
   }
   else
   {
      sockaddr_in6* a1 = (sockaddr_in6*)addr1;
      sockaddr_in6* a2 = (sockaddr_in6*)addr2;

      if (a1->sin6_port == a2->sin6_port)
      {
         for (int i = 0; i < 16; ++ i)
            if (*((char*)&(a1->sin6_addr) + i) != *((char*)&(a1->sin6_addr) + i))
               return false;

         return true;
      }
   }

   return false;
}
