/*****************************************************************************
Copyright © 2001 - 2003, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT)

Laboratory for Advanced Computing (LAC)
National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.lac.uic.edu/

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software (UDT) and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimers.

Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimers in the documentation
and/or other materials provided with the distribution.

Neither the names of the University of Illinois, LAC/NCDM, nor the names
of its contributors may be used to endorse or promote products derived
from this Software without specific prior written permission.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*****************************************************************************/

/*****************************************************************************
This file contains implementation of UDT common routines of timer, 
mutex facility, ACK window, and exception processing.

CTimer is a high precision timing facility, which uses the CPU clock cycle 
as the minimum time unit.
CGuard is mutex facility that can automatically lock a method.
CACKWindow is the window management of UDT ACK packet.
(reference: UDT header definition: packet.h)
CUDTException is used for UDT exception processing, which is the only 
method to catch and handle UDT errors and exceptions.
*****************************************************************************/

/*****************************************************************************
written by 
   Yunhong Gu [ygu@cs.uic.edu], last updated 06/11/2003
*****************************************************************************/


#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "udt.h"


void CTimer::rdtsc(unsigned __int64 &x) const
{
   #ifdef IA32
      // read CPU clock with RDTSC instruction on IA32 acrh
      __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
   #else
      // use system call to read time clock for other archs
      timeval t;
      gettimeofday(&t, 0);
      x = t.tv_sec * 1000000 + t.tv_usec;
   #endif

   //TODO: add machine instrcutions for different archs
}

unsigned __int64 CTimer::getCPUFrequency() const
{
   #ifdef IA32
   {
      // alternative: read /proc/cpuinfo

      unsigned __int64 t1, t2;

      rdtsc(t1);
      usleep(100000);
      rdtsc(t2);

      // CPU clocks per microsecond
      return (t2 - t1) / 100000;
   }
   #else
      return 1;
   #endif
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
// Automatically lock in constructure
CGuard::CGuard(pthread_mutex_t& lock):
m_Mutex(lock)
{
   m_iLocked = pthread_mutex_lock(&m_Mutex);
}

// Automatically unlock in destructure
CGuard::~CGuard()
{
   if (0 == m_iLocked)
      pthread_mutex_unlock(&m_Mutex);
}


//
CACKWindow::CACKWindow():
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

      for (__int32 i = m_iTail; i <= m_iHead; i ++)
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

   // head has exceeded the physical window boundary, so it is behind to tail
   for (__int32 i = m_iTail; i <= m_iHead + m_iSize; i ++)
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
m_iSize(16)
{
   m_piPktWindow = new __int32[m_iSize];
   m_piRTTWindow = new __int32[m_iSize];
   m_piPCTWindow = new __int32[m_iSize];
   m_piPDTWindow = new __int32[m_iSize];
   m_piProbeWindow = new __int32[m_iSize];

   m_iPktWindowPtr = 0;
   m_iRTTWindowPtr = 0;
   m_iProbeWindowPtr = 0;

   m_bFirstRound = true;

   gettimeofday(&m_LastArrTime, 0);

   // initialize RTT/PCT/PDT values
   for (__int32 i = 0; i < m_iSize; i ++)
      m_piProbeWindow[i] = m_piRTTWindow[i] = m_piPCTWindow[i] = m_piPDTWindow[i] = 0;
}

CPktTimeWindow::CPktTimeWindow(const __int32& size):
m_iSize(size)
{
   m_piPktWindow = new __int32[m_iSize];
   m_piRTTWindow = new __int32[m_iSize];
   m_piPCTWindow = new __int32[m_iSize];
   m_piPDTWindow = new __int32[m_iSize];
   m_piProbeWindow = new __int32[m_iSize];

   m_iPktWindowPtr = 0;
   m_iRTTWindowPtr = 0;
   m_iProbeWindowPtr = 0;

   m_bFirstRound = true;

   gettimeofday(&m_LastArrTime, 0);

   // initialize RTT/PCT/PDT values
   for (__int32 i = 0; i < m_iSize; i ++)
      m_piProbeWindow[i] = m_piRTTWindow[i] = m_piPCTWindow[i] = m_piPDTWindow[i] = 0;
}

CPktTimeWindow::~CPktTimeWindow()
{
   delete [] m_piPktWindow;
   delete [] m_piRTTWindow;
   delete [] m_piPCTWindow;
   delete [] m_piPDTWindow;
   delete [] m_piProbeWindow;
}

__int32 CPktTimeWindow::getPktSpeed() const
{
   // during slow start phase, there is not enough data in history window
   // measure the latest two packets arrival times
   if (m_bFirstRound)
   {
      if ((m_iPktWindowPtr > 1) && (m_piPktWindow[m_iPktWindowPtr - 1] < m_piPktWindow[m_iPktWindowPtr - 2] * 2))
         return (__int32)ceil(1000000.0 / m_piPktWindow[m_iPktWindowPtr - 1]);

      return 0;
   }

   // sorting
   __int32 temp;
   for (__int32 i = 0; i < ((m_iSize >> 1) + 1); ++ i)
      for (__int32 j = i; j < m_iSize; ++ j)
         if (m_piPktWindow[i] > m_piPktWindow[j])
         {
            temp = m_piPktWindow[i];
            m_piPktWindow[i] = m_piPktWindow[j];
            m_piPktWindow[j] = temp;
         }

   // read the median value
   __int32 median = (m_piPktWindow[(m_iSize >> 1) - 1] + m_piPktWindow[m_iSize >> 1]) >> 1;
   __int32 count = 0;
   __int32 sum = 0;

   // median filtering
   for (__int32 i = 0; i < m_iSize; ++ i)
      if ((m_piPktWindow[i] < (median << 3)) && (m_piPktWindow[i] > (median >> 3)))
      {
         ++ count;
         sum += m_piPktWindow[i];
      }

   // claculate speed, or return 0 if not enough valid value
   if (count > (m_iSize >> 1))
      return (__int32)ceil(1000000.0 / (sum / count));
   else
      return 0;
}

bool CPktTimeWindow::getDelayTrend() const
{
   double pct = 0.0;
   double pdt = 0.0;

   for (__int32 i = 0; i < m_iSize; ++i)
      if (i != m_iRTTWindowPtr)
      {
         pct += m_piPCTWindow[i];
         pdt += m_piPDTWindow[i];
      }

   // calculate PCT and PDT value
   pct /= m_iSize - 1;
   if (0 != pdt)
      pdt = (m_piRTTWindow[(m_iRTTWindowPtr - 1 + m_iSize) % m_iSize] - m_piRTTWindow[m_iRTTWindowPtr]) / pdt;

   // PCT/PDT judgement
   // reference: M. Jain, C. Dovrolis, Pathload: a measurement tool for end-to-end available bandwidth
   return ((pct > 0.66) && (pdt > 0.45)) || ((pct > 0.54) && (pdt > 0.55));
}

__int32 CPktTimeWindow::getBandwidth() const
{
   // sorting
   __int32 temp;
   for (__int32 i = 0; i < ((m_iSize >> 1) + 1); ++ i)
      for (__int32 j = i; j < m_iSize; ++ j)
         if (m_piProbeWindow[i] > m_piProbeWindow[j])
         {
            temp = m_piProbeWindow[i];
            m_piProbeWindow[i] = m_piProbeWindow[j];
            m_piProbeWindow[j] = temp;
         }

   // read the median value
   __int32 median = (m_piProbeWindow[(m_iSize >> 1) - 1] + m_piProbeWindow[m_iSize >> 1]) >> 1;

   if (0 == median)
      return 0;

   return (__int32)(1000000.0 / median);
}

void CPktTimeWindow::pktArrival()
{
   gettimeofday(&m_CurrArrTime, 0);
   
   // record the packet interval between the current and the last one
   m_piPktWindow[m_iPktWindowPtr] = (m_CurrArrTime.tv_sec - m_LastArrTime.tv_sec) * 1000000 + m_CurrArrTime.tv_usec - m_LastArrTime.tv_usec;

   // the window is logically circular
   m_iPktWindowPtr = (m_iPktWindowPtr + 1) % m_iSize;

   // slow start stops after the window is fulfilled
   if (0 == m_iPktWindowPtr)
      m_bFirstRound = false;

   // remember last packet arrival time 
   m_LastArrTime = m_CurrArrTime;
}

void CPktTimeWindow::ack2Arrival(const __int32& rtt)
{
   // record RTT, comparison (1 or 0), and absolute difference
   m_piRTTWindow[m_iRTTWindowPtr] = rtt;
   m_piPCTWindow[m_iRTTWindowPtr] = (rtt > m_piRTTWindow[(m_iRTTWindowPtr - 1 + m_iSize) % m_iSize]) ? 1 : 0;
   m_piPDTWindow[m_iRTTWindowPtr] = abs(rtt - m_piRTTWindow[(m_iRTTWindowPtr - 1 + m_iSize) % m_iSize]);

   // the window is logically circular
   m_iRTTWindowPtr = (m_iRTTWindowPtr + 1) % m_iSize;
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
   m_iProbeWindowPtr = (m_iProbeWindowPtr + 1) % m_iSize;

   // remember last packet arrival time
   m_LastArrTime = m_CurrArrTime;
}


//
CUDTException::CUDTException(__int32 major, __int32 minor, __int32 err):
m_iMajor(major),
m_iMinor(minor),
m_iErrno(err)
{
}

CUDTException::~CUDTException()
{
}

const char* CUDTException::getErrorMessage()
{
   // translate "Major" code into text message, "Minor" code is omitted.

   switch (m_iMajor)
   {
      case 0:
        strcpy(m_pcMsg, "Success");
        break;

      case 1:
        strcpy(m_pcMsg, "Couldn't set up network connection");
        break;

      case 2:
        strcpy(m_pcMsg, "Connection broken");
        break;

      case 3:
        strcpy(m_pcMsg, "Memory exceptions occurs");
        break;

      case 4:
        strcpy(m_pcMsg, "File exceptions occurs");
        break;

      case 5:
        strcpy(m_pcMsg, "Operation not supported");
        break;

      default:
        strcpy(m_pcMsg, "Undefined error");
   }

   // Adding "errno" information
   if (0 != m_iErrno)
   {
      strcpy(m_pcMsg + strlen(m_pcMsg), ": ");
      strncpy(m_pcMsg + strlen(m_pcMsg), strerror(m_iErrno), 1024 - strlen(m_pcMsg) - 2);
   }

   // Adding a CR/LF charactor
   strcpy(m_pcMsg + strlen(m_pcMsg), ".\n");

   return m_pcMsg;
}

const __int32& CUDTException::getErrorCode() const
{
   return m_iErrno;
}
