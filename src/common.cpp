/*****************************************************************************
Copyright © 2001 - 2006, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) version 2

Laboratory for Advanced Computing (LAC)
National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.lac.uic.edu/

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
mutex facility, ACK window, packet time window, and exception processing.

CTimer is a high precision timing facility, which uses the CPU clock cycle
as the minimum time unit.
CGuard is mutex facility that can automatically lock a method.
CACKWindow is the window management of UDT ACK packet.
(reference: UDT header definition: packet.h)
CPktTimeWindow is used to record and process packet sending and arrival
timing information.
CUDTException is used for UDT exception processing, which is the only
method to catch and handle UDT errors and exceptions.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [ygu@cs.uic.edu], last updated 03/16/2006

modified by
   <programmer's name, programmer's email, last updated mm/dd/yyyy>
   <descrition of changes>
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
#include "udt.h"

using namespace std;

#ifdef WIN32
   int gettimeofday(timeval *tv, void*)
   {
      LARGE_INTEGER ccf;
      if (QueryPerformanceFrequency(&ccf))
      {
         LARGE_INTEGER cc;
         QueryPerformanceCounter(&cc);
         tv->tv_sec = (long)(cc.QuadPart / ccf.QuadPart);
         tv->tv_usec = (long)((cc.QuadPart % ccf.QuadPart) / (ccf.QuadPart / 1000000));
      }
      else
      {
         unsigned __int64 ft;
         GetSystemTimeAsFileTime((FILETIME *)&ft);
         tv->tv_sec = (long)(ft / 10000000);
         tv->tv_usec = (long)((ft % 10000000) / 10);
      }

      return 0;
   }

   int readv(SOCKET s, const iovec* vector, int count)
   {
      DWORD rsize = 0;
      DWORD flag = 0;

      WSARecv(s, (LPWSABUF)vector, count, &rsize, &flag, NULL, NULL);

      return rsize;
   }

   int writev(SOCKET s, const iovec* vector, int count)
   {
      DWORD ssize = 0;

      WSASend(s, (LPWSABUF)vector, count, &ssize, 0, NULL, NULL);

      return ssize;
   }
#endif

unsigned __int64 CTimer::s_ullCPUFrequency = CTimer::readCPUFrequency();

void CTimer::rdtsc(unsigned __int64 &x)
{
   #ifdef WIN32
      if (!QueryPerformanceCounter((LARGE_INTEGER *)&x))
      {
         timeval t;
         gettimeofday(&t, 0);
         x = t.tv_sec * 1000000 + t.tv_usec;
      }
   #elif IA32
      // read CPU clock with RDTSC instruction on IA32 acrh
      __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));

      // on Windows
      /*
         unsigned __int32 a, b;
         __asm 
         {
            __emit 0x0f
            __emit 0x31
            mov a, eax
            mov b, ebx
         }
         x = b;
         x = (x << 32) + a;
      */

   #elif IA64
      __asm__ volatile ("mov %0=ar.itc" : "=r"(x) :: "memory");
   #elif AMD64
      unsigned __int32 lval, hval;
      __asm__ volatile ("rdtsc" : "=a" (lval), "=d" (hval));
      x = hval;
      x = (x << 32) | lval;
   #else
      // use system call to read time clock for other archs
      timeval t;
      gettimeofday(&t, 0);
      x = t.tv_sec * 1000000 + t.tv_usec;
   #endif
}

unsigned __int64 CTimer::readCPUFrequency()
{
   #ifdef WIN32
      __int64 ccf;
      if (QueryPerformanceFrequency((LARGE_INTEGER *)&ccf))
         return ccf / 1000000;
      else
         return 1;
   #elif IA32 || IA64 || AMD64
      // alternative: read /proc/cpuinfo

      unsigned __int64 t1, t2;

      rdtsc(t1);
      usleep(100000);
      rdtsc(t2);

      // CPU clocks per microsecond
      return (t2 - t1) / 100000;
   #else
      return 1;
   #endif
}

unsigned __int64 CTimer::getCPUFrequency()
{
   return s_ullCPUFrequency;
}

void CTimer::sleep(const unsigned __int64& interval)
{
   unsigned __int64 t;
   rdtsc(t);

   // sleep next "interval" time
   sleepto(t + interval);
}

void CTimer::sleepto(const unsigned __int64& nexttime)
{
   // Use class member such that the method can be interrupted by others
   m_ullSchedTime = nexttime;

   unsigned __int64 t;
   rdtsc(t);

   while (t < m_ullSchedTime)
   {
      #ifdef IA32
         //__asm__ volatile ("nop; nop; nop; nop; nop;");
         __asm__ volatile ("pause; rep; nop; nop; nop; nop; nop;");
      #elif IA64
         __asm__ volatile ("nop 0; nop 0; nop 0; nop 0; nop 0;");
      #elif AMD64
         __asm__ volatile ("nop; nop; nop; nop; nop;");
      #endif

      // TODO: use high precision timer if it is available

      rdtsc(t);
   }
}

void CTimer::interrupt()
{
   // schedule the sleepto time to the current CCs, so that it will stop
   rdtsc(m_ullSchedTime);
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
CACKWindow::CACKWindow():
m_piACKSeqNo(NULL),
m_piACK(NULL),
m_pTimeStamp(NULL),
m_iSize(1024),
m_iHead(0),
m_iTail(0)
{
   m_piACKSeqNo = new __int32[m_iSize];
   m_piACK = new __int32[m_iSize];
   m_pTimeStamp = new timeval[m_iSize];

   m_piACKSeqNo[0] = -1;
}

CACKWindow::CACKWindow(const __int32& size):
m_piACKSeqNo(NULL),
m_piACK(NULL),
m_pTimeStamp(NULL),
m_iSize(size),
m_iHead(0),
m_iTail(0)
{
   m_piACKSeqNo = new __int32[m_iSize];
   m_piACK = new __int32[m_iSize];
   m_pTimeStamp = new timeval[m_iSize];

   m_piACKSeqNo[0] = -1;
}

CACKWindow::~CACKWindow()
{
   delete [] m_piACKSeqNo;
   delete [] m_piACK;
   delete [] m_pTimeStamp;
}

void CACKWindow::store(const __int32& seq, const __int32& ack)
{
   m_piACKSeqNo[m_iHead] = seq;
   m_piACK[m_iHead] = ack;
   gettimeofday(m_pTimeStamp + m_iHead, 0);

   m_iHead = (m_iHead + 1) % m_iSize;

   // overwrite the oldest ACK since it is not likely to be acknowledged
   if (m_iHead == m_iTail)
      m_iTail = (m_iTail + 1) % m_iSize;
}

__int32 CACKWindow::acknowledge(const __int32& seq, __int32& ack)
{
   if (m_iHead >= m_iTail)
   {
      // Head has not exceeded the physical boundary of the window

      for (__int32 i = m_iTail, n = m_iHead; i < n; ++ i)
         // looking for indentical ACK Seq. No.
         if (seq == m_piACKSeqNo[i])
         {
            // return the Data ACK it carried
            ack = m_piACK[i];

            // calculate RTT
            timeval currtime;
            gettimeofday(&currtime, 0);
            __int32 rtt = (currtime.tv_sec - m_pTimeStamp[i].tv_sec) * 1000000 + currtime.tv_usec - m_pTimeStamp[i].tv_usec;
            if (i == m_iHead)
            {
               m_iTail = m_iHead = 0;
               m_piACKSeqNo[0] = -1;
            }
            else
               m_iTail = (i + 1) % m_iSize;

            return rtt;
         }

      // Bad input, the ACK node has been overwritten
      return -1;
   }

   // Head has exceeded the physical window boundary, so it is behind tail
   for (__int32 i = m_iTail, n = m_iHead + m_iSize; i < n; ++ i)
      // looking for indentical ACK seq. no.
      if (seq == m_piACKSeqNo[i % m_iSize])
      {
         // return Data ACK
         i %= m_iSize;
         ack = m_piACK[i];

         // calculate RTT
         timeval currtime;
         gettimeofday((timeval *)&currtime, 0);
         __int32 rtt = (currtime.tv_sec - m_pTimeStamp[i].tv_sec) * 1000000 + currtime.tv_usec - m_pTimeStamp[i].tv_usec;
         if (i == m_iHead)
         {
            m_iTail = m_iHead = 0;
            m_piACKSeqNo[0] = -1;
         }
         else
            m_iTail = (i + 1) % m_iSize;

         return rtt;
      }

   // bad input, the ACK node has been overwritten
   return -1;
}

//
CPktTimeWindow::CPktTimeWindow():
m_iAWSize(16),
m_piPktWindow(NULL),
m_iRWSize(16),
m_piRTTWindow(NULL),
m_piPCTWindow(NULL),
m_piPDTWindow(NULL),
m_iPWSize(16),
m_piProbeWindow(NULL)
{
   m_piPktWindow = new __int32[m_iAWSize];
   m_piRTTWindow = new __int32[m_iRWSize];
   m_piPCTWindow = new __int32[m_iRWSize];
   m_piPDTWindow = new __int32[m_iRWSize];
   m_piProbeWindow = new __int32[m_iPWSize];

   m_iPktWindowPtr = 0;
   m_iRTTWindowPtr = 0;
   m_iProbeWindowPtr = 0;

   gettimeofday(&m_LastSentTime, 0);
   gettimeofday(&m_LastArrTime, 0);
   m_iMinPktSndInt = 1000000;

   for (__int32 i = 0; i < m_iAWSize; ++ i)
      m_piPktWindow[i] = 1;

   for (__int32 i = 0; i < m_iRWSize; ++ i)   
      m_piRTTWindow[i] = m_piPCTWindow[i] = m_piPDTWindow[i] = 0;

   for (__int32 i = 0; i < m_iPWSize; ++ i)
      m_piProbeWindow[i] = 1000;
}

CPktTimeWindow::CPktTimeWindow(const __int32& s1, const __int32& s2, const __int32& s3):
m_iAWSize(s1),
m_piPktWindow(NULL),
m_iRWSize(s2),
m_piRTTWindow(NULL),
m_piPCTWindow(NULL),
m_piPDTWindow(NULL),
m_iPWSize(s3),
m_piProbeWindow(NULL)
{
   m_piPktWindow = new __int32[m_iAWSize];
   m_piRTTWindow = new __int32[m_iRWSize];
   m_piPCTWindow = new __int32[m_iRWSize];
   m_piPDTWindow = new __int32[m_iRWSize];
   m_piProbeWindow = new __int32[m_iPWSize];

   m_iPktWindowPtr = 0;
   m_iRTTWindowPtr = 0;
   m_iProbeWindowPtr = 0;

   gettimeofday(&m_LastSentTime, 0);
   gettimeofday(&m_LastArrTime, 0);
   m_iMinPktSndInt = 1000000;

   for (__int32 i = 0; i < m_iAWSize; ++ i)
      m_piPktWindow[i] = 1;

   for (__int32 i = 0; i < m_iRWSize; ++ i)
      m_piRTTWindow[i] = m_piPCTWindow[i] = m_piPDTWindow[i] = 0;

   for (__int32 i = 0; i < m_iPWSize; ++ i)
      m_piProbeWindow[i] = -1;
}

CPktTimeWindow::~CPktTimeWindow()
{
   delete [] m_piPktWindow;
   delete [] m_piRTTWindow;
   delete [] m_piPCTWindow;
   delete [] m_piPDTWindow;
   delete [] m_piProbeWindow;
}

__int32 CPktTimeWindow::getMinPktSndInt() const
{
   return m_iMinPktSndInt;
}

__int32 CPktTimeWindow::getPktRcvSpeed() const
{
   // sorting
   __int32 temp;
   for (__int32 i = 0, n = (m_iAWSize >> 1) + 1; i < n; ++ i)
      for (__int32 j = i, m = m_iAWSize; j < m; ++ j)
         if (m_piPktWindow[i] > m_piPktWindow[j])
         {
            temp = m_piPktWindow[i];
            m_piPktWindow[i] = m_piPktWindow[j];
            m_piPktWindow[j] = temp;
         }

   // read the median value
   __int32 median = (m_piPktWindow[(m_iAWSize >> 1) - 1] + m_piPktWindow[m_iAWSize >> 1]) >> 1;
   __int32 count = 0;
   __int32 sum = 0;
   __int32 upper = median << 3;
   __int32 lower = median >> 3;

   // median filtering
   for (__int32 i = 0, n = m_iAWSize; i < n; ++ i)
      if ((m_piPktWindow[i] < upper) && (m_piPktWindow[i] > lower))
      {
         ++ count;
         sum += m_piPktWindow[i];
      }

   // claculate speed, or return 0 if not enough valid value
   if (count > (m_iAWSize >> 1))
      return (__int32)ceil(1000000.0 / (sum / count));
   else
      return 0;
}

bool CPktTimeWindow::getDelayTrend() const
{
   double pct = 0.0;
   double pdt = 0.0;

   for (__int32 i = 0, n = m_iRWSize; i < n; ++ i)
      if (i != m_iRTTWindowPtr)
      {
         pct += m_piPCTWindow[i];
         pdt += m_piPDTWindow[i];
      }

   // calculate PCT and PDT value
   pct /= m_iRWSize - 1;
   if (0 != pdt)
      pdt = (m_piRTTWindow[(m_iRTTWindowPtr - 1 + m_iRWSize) % m_iRWSize] - m_piRTTWindow[m_iRTTWindowPtr]) / pdt;

   // PCT/PDT judgement
   // reference: M. Jain, C. Dovrolis, Pathload: a measurement tool for end-to-end available bandwidth
   return ((pct > 0.66) && (pdt > 0.45)) || ((pct > 0.54) && (pdt > 0.55));
}

__int32 CPktTimeWindow::getBandwidth() const
{
   // sorting
   __int32 temp;
   for (__int32 i = 0, n = (m_iPWSize >> 1) + 1; i < n; ++ i)
      for (__int32 j = i, m = m_iPWSize; j < m; ++ j)
         if (m_piProbeWindow[i] > m_piProbeWindow[j])
         {
            temp = m_piProbeWindow[i];
            m_piProbeWindow[i] = m_piProbeWindow[j];
            m_piProbeWindow[j] = temp;
         }

   // read the median value
   __int32 median = (m_piProbeWindow[(m_iPWSize >> 1) - 1] + m_piProbeWindow[m_iPWSize >> 1]) >> 1;
   __int32 count = 1;
   __int32 sum = median;
   __int32 upper = median << 3;
   __int32 lower = median >> 3;

   // median filtering
   for (__int32 i = 0, n = m_iPWSize; i < n; ++ i)
      if ((m_piProbeWindow[i] < upper) && (m_piProbeWindow[i] > lower))
      {
         ++ count;
         sum += m_piProbeWindow[i];
      }

   return (__int32)ceil(1000000.0 / (double(sum) / double(count)));
}

void CPktTimeWindow::onPktSent(const timeval& currtime)
{
   __int32 interval = (currtime.tv_sec - m_LastSentTime.tv_sec) * 1000000 + currtime.tv_usec - m_LastSentTime.tv_usec;

   if ((interval < m_iMinPktSndInt) && (interval > 0))
      m_iMinPktSndInt = interval;

   m_LastSentTime = currtime;
}

void CPktTimeWindow::onPktArrival()
{
   gettimeofday(&m_CurrArrTime, 0);
   
   // record the packet interval between the current and the last one
   m_piPktWindow[m_iPktWindowPtr] = (m_CurrArrTime.tv_sec - m_LastArrTime.tv_sec) * 1000000 + m_CurrArrTime.tv_usec - m_LastArrTime.tv_usec;

   // the window is logically circular
   m_iPktWindowPtr = (m_iPktWindowPtr + 1) % m_iAWSize;

   // remember last packet arrival time 
   m_LastArrTime = m_CurrArrTime;
}

void CPktTimeWindow::ack2Arrival(const __int32& rtt)
{
   // record RTT, comparison (1 or 0), and absolute difference
   m_piRTTWindow[m_iRTTWindowPtr] = rtt;
   m_piPCTWindow[m_iRTTWindowPtr] = (rtt > m_piRTTWindow[(m_iRTTWindowPtr - 1 + m_iRWSize) % m_iRWSize]) ? 1 : 0;
   m_piPDTWindow[m_iRTTWindowPtr] = abs(rtt - m_piRTTWindow[(m_iRTTWindowPtr - 1 + m_iRWSize) % m_iRWSize]);

   // the window is logically circular
   m_iRTTWindowPtr = (m_iRTTWindowPtr + 1) % m_iRWSize;
}

void CPktTimeWindow::probe1Arrival()
{
   gettimeofday(&m_ProbeTime, 0);
}

void CPktTimeWindow::probe2Arrival()
{
   gettimeofday(&m_CurrArrTime, 0);

   // record the probing packets interval
   m_piProbeWindow[m_iProbeWindowPtr] = (m_CurrArrTime.tv_sec - m_ProbeTime.tv_sec) * 1000000 + m_CurrArrTime.tv_usec - m_ProbeTime.tv_usec;

   // the window is logically circular
   m_iProbeWindowPtr = (m_iProbeWindowPtr + 1) % m_iPWSize;
}

//
CCC::CCC():
m_dPktSndPeriod(1.0),
m_dCWndSize(16.0),
m_iACKPeriod(0),
m_iACKInterval(0),
m_iRTO(-1)
{
}

void CCC::setACKTimer(const __int32& msINT)
{
   m_iACKPeriod = msINT;
}

void CCC::setACKInterval(const __int32& pktINT)
{
   m_iACKInterval = pktINT;
}

void CCC::setRTO(const __int32& usRTO)
{
   m_iRTO = usRTO;
}

void CCC::sendCustomMsg(CPacket& pkt) const
{
   if (NULL != m_pUDT)
      *m_pUDT->m_pChannel << pkt;
}

const CPerfMon* CCC::getPerfInfo()
{
   if (NULL != m_pUDT)
      m_pUDT->sample(&m_PerfInfo, false);

   return &m_PerfInfo;
}

//
CUDTException::CUDTException(__int32 major, __int32 minor, __int32 err):
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

        case 3:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "no buffer available for overlapped reading");

           break;

        case 4:
           strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
           strcpy(m_pcMsg + strlen(m_pcMsg), "non-blocking overlapped recv is on going");

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

const __int32 CUDTException::getErrorCode() const
{
   return m_iMajor * 1000 + m_iMinor;
}
