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
This file contains the implementation of main algorithms of UDT protocol and 
the implementation of UDT API.

Refernece:
UDT programming manual
UDT protocol specification
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [ygu@cs.uic.edu], last updated 09/11/2003
*****************************************************************************/


#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "udt.h"


CUDT::CUDT():
//
// These constants are defined in UDT specification. They MUST NOT be changed!
//
m_iSYNInterval(10000),
m_iMaxSeqNo(1 << 30),
m_iSeqNoTH(1 << 29),
m_iMaxAckSeqNo(1 << 16),
m_iProbeInterval(16),
m_dLossRateLimit(0.01),
m_dWeight(0.125)
{
   m_pSndBuffer = NULL;
   m_pRcvBuffer = NULL;
   m_pSndLossList = NULL;
   m_pRcvLossList = NULL;
   m_pTimer = new CTimer;
   m_pIrrPktList = NULL;
   m_pACKWindow = NULL;

   // Initilize mutex and condition variables
   pthread_mutex_init(&m_SendDataLock, NULL);
   pthread_cond_init(&m_SendDataCond, NULL);
   pthread_mutex_init(&m_SendBlockLock, NULL);
   pthread_cond_init(&m_SendBlockCond, NULL);
   pthread_cond_init(&m_RecvDataCond, NULL);
   pthread_mutex_init(&m_RecvDataLock, NULL);
   pthread_mutex_init(&m_SendLock, NULL);
   pthread_mutex_init(&m_RecvLock, NULL);
   pthread_mutex_init(&m_AckLock, NULL);

   // Default UDT configurations
   m_pcIP = NULL;
   m_iPort = 0;
   m_bPortChangable = true;
   m_iMTU = 1500;
   m_bSynSending = false;
   m_bSynRecving = true;
   m_iMemFlag = 1;
   m_iRCAlg = 1;
   m_iFlightFlagSize = 25600;
   m_iUDTBufSize = 40960000;
   m_iUDPSndBufSize = 64 * 1024;
   m_iUDPRcvBufSize = 4 * 1024 * 1024;
   m_iIPversion = 4;
   m_bMulticast = false;
   memset(m_pcMCIP, '\0', 40);
   m_iMCPort = 0;
   m_bLossy = false;

   m_iRTT = 10 * m_iSYNInterval;

   m_ullCPUFrequency = m_pTimer->getCPUFrequency();

   // Initial status
   m_bOpened = false;
   m_bConnected = false;
}

CUDT::~CUDT()
{
   // destroy the data structures
   if (m_pcIP)
      delete [] m_pcIP;
   if (m_pSndBuffer)
      delete m_pSndBuffer;
   if (m_pRcvBuffer)
      delete m_pRcvBuffer;
   if (m_pSndLossList)
      delete m_pSndLossList;
   if (m_pRcvLossList)
      delete m_pRcvLossList;
   if (m_pTimer)
      delete m_pTimer;
   if (m_pIrrPktList)
      delete m_pIrrPktList;
   if (m_pACKWindow)
      delete m_pACKWindow;
   if (m_pSndThread)
      delete m_pSndThread;
   if (m_pRcvThread)
      delete m_pRcvThread;
}

void CUDT::setOpt(UDTOpt optName, const void* optval, const __int32& optlen)
{
   switch (optName)
   {
   case UDT_ADDR:
      if (m_bOpened)
         throw CUDTException(5, 1);

      if (m_pcIP)
         delete [] m_pcIP;
      m_pcIP = new char[40];
      memcpy(m_pcIP, optval, optlen);
      break;

   case UDT_PORT:
      if (m_bOpened)
         throw CUDTException(5, 1);

      m_iPort = *(__int32 *)optval;
      break;

   case UDT_PCH:
      if (m_bOpened)
         throw CUDTException(5, 1);

      m_bPortChangable = *(bool *)optval;
      break;

   case UDT_MTU:
      if (m_bOpened)
         throw CUDTException(5, 1);

      m_iMTU = *(__int32 *)optval;
      if (m_iMTU < 28)
         throw CUDTException(5, 3);
      break;

   case UDT_SNDSYN:
      m_bSynSending = *(bool *)optval;
      break;

   case UDT_RCVSYN:
      m_bSynRecving = *(bool *)optval;
      break;

   case UDT_MFLAG:
      m_iMemFlag = *(int *)optval;
      if ((m_iMemFlag < 0) || (m_iMemFlag > 2))
         throw CUDTException(5, 3);
      break;

   case UDT_RC:
      throw CUDTException(5);

      m_iRCAlg = *(__int32 *)optval;
      break;

   case UDT_FC:
      if (m_bConnected)
         throw CUDTException(5, 2);

      m_iFlightFlagSize = *(__int32 *)optval;
      if (m_iFlightFlagSize < 1)
         throw CUDTException(5, 3);
      break;

   case UDT_BUF:
      if (m_bOpened)
         throw CUDTException(5, 1);

      m_iUDTBufSize = *(__int32 *)optval;
      if (m_iUDTBufSize < m_iMTU - 28)
         throw CUDTException(5, 3);
      break;

   case UDT_USB:
      if (m_bOpened)
         throw CUDTException(5, 1);

      m_iUDPSndBufSize = *(__int32 *)optval;
      break;

   case UDT_URB:
      if (m_bOpened)
         throw CUDTException(5, 1);

      m_iUDPRcvBufSize = *(__int32 *)optval;
      break;

   case UDT_IPV:
      if (m_bOpened)
         throw CUDTException(5, 1);

      m_iIPversion = *(__int32 *)optval;
      if ((4 != m_iIPversion) && (6 != m_iIPversion))
         throw CUDTException(5, 3);
      break;

   case UDT_MC:
      throw CUDTException(5);

      m_bMulticast = *(bool *)optval;
      break;

   case UDT_MCADDR:
      throw CUDTException(5);

      memcpy(m_pcMCIP, optval, optlen);
      break;

   case UDT_MCPORT:
      throw CUDTException(5);

      m_iMCPort = *(__int32 *)optval;
      break;

   case UDT_LOSSY:
      throw CUDTException(5);

      m_bLossy = *(bool *)optval;
      break;

   default:
      throw CUDTException(5);
   }
}

void CUDT::getOpt(UDTOpt optName, void* optval, __int32& optlen)
{
   switch (optName)
   {
   case UDT_ADDR:
      if (NULL == m_pcIP)
         optlen = 0;
      else
      {
         memcpy(optval, m_pcIP, 40);
         optlen = 40;
      }
      break;

   case UDT_PORT:
      *(__int32 *)optval = m_iPort;
      optlen = sizeof(__int32);
      break;

   case UDT_PCH:
      *(bool *)optval = m_bPortChangable;
      optlen = sizeof(bool);
      break;

   case UDT_MTU:
      *(__int32 *)optval = m_iMTU;
      optlen = sizeof(__int32);
      break;

   case UDT_SNDSYN:
      *(bool *)optval = m_bSynSending;
      optlen = sizeof(bool);
      break;

   case UDT_RCVSYN:
      *(bool *)optval = m_bSynRecving;
      optlen = sizeof(bool);
      break;

   case UDT_MFLAG:
      *(int *)optval = m_iMemFlag;
      optlen = sizeof(int);
      break;

   case UDT_RC:
      *(__int32 *)optval = m_iRCAlg;
      optlen = sizeof(__int32);
      break;

   case UDT_FC:
      *(__int32 *)optval = m_iFlightFlagSize;
      optlen = sizeof(__int32);
      break;

   case UDT_BUF:
      *(__int32 *)optval = m_iUDTBufSize;
      optlen = sizeof(__int32);
      break;

   case UDT_USB:
      *(__int32 *)optval = m_iUDPSndBufSize;
      optlen = sizeof(__int32);
      break;

   case UDT_URB:
      *(__int32 *)optval = m_iUDPRcvBufSize;
      optlen = sizeof(__int32);
      break;

   case UDT_IPV:
      *(__int32 *)optval = m_iIPversion;
      optlen = sizeof(__int32);
      break;

   case UDT_MC:
      *(bool *)optval = m_bMulticast;
      optlen = sizeof(bool);
      break;

   case UDT_MCADDR:
      memcpy(optval, m_pcMCIP, 40);
      optlen = 40;
      break;

   case UDT_MCPORT:
      *(__int32 *)optval = m_iMCPort;
      optlen = sizeof(__int32);
      break;

   case UDT_LOSSY:
      *(bool *)optval = m_bLossy;
      optlen = sizeof(bool);
      break;

   default:
      throw CUDTException(5);
   }
}

__int32 CUDT::open(const __int32& port)
{
   // Initial status
   m_bClosing = false;
   m_iEXPCount = 0;

   // Initial sequence number, loss, acknowledgement, etc.
   m_iNAKCount = 0;
   m_iSndLastAck = 0;
   m_iLocalSend = 0;
   m_iLocalLoss = 0;
   m_iSndCurrSeqNo = -1;
   m_dLossRate = 0.0;

   m_iRcvLastAck = 0;
   m_iRcvLastAckAck = 0;
   m_iRcvCurrSeqNo = -1;
   m_iNextExpect = 0;
   m_bReadBuf = false;

   m_iDecCount = 1;
   m_iLastDecSeq = -1;

   m_iBandwidth = 1;
   m_bSlowStart = true;
   m_bFreeze = false;

   // Initial sending rate = 1us
   m_ullInterval = m_ullCPUFrequency;

   // Initial Window Size = 2 packet
   m_iFlowWindowSize = 2;


   if (0 != port)
      m_iPort = port;

   // Construct and open a channel
   try
   {
      m_pChannel = new CChannel();

      m_pChannel->setSndBufSize(m_iUDPSndBufSize);
      m_pChannel->setRcvBufSize(m_iUDPRcvBufSize);

      if (6 == m_iIPversion)
         m_pChannel->open6(m_iPort, m_bPortChangable, m_pcIP);
      else
         m_pChannel->open(m_iPort, m_bPortChangable, m_pcIP);
   }
   catch(CUDTException e)
   {
      // Let applications to process this exception
      throw CUDTException(e);
   }

   // Prepare all structures
   m_pSndBuffer = new CSndBuffer;
   m_pRcvBuffer = new CRcvBuffer(m_iUDTBufSize);
   m_pSndLossList = new CSndLossList(m_iFlightFlagSize, m_iSeqNoTH, m_iMaxSeqNo);
   m_pRcvLossList = new CRcvLossList(m_iFlightFlagSize, m_iSeqNoTH, m_iMaxSeqNo);
   m_pIrrPktList = new CIrregularPktList(m_iFlightFlagSize, m_iSeqNoTH, m_iMaxSeqNo);
   m_pACKWindow = new CACKWindow(4096);
   m_pRcvTimeWindow = new CPktTimeWindow;

   // Prepare threads
   m_pSndThread = new pthread_t;
   m_pRcvThread = new pthread_t;

   // Now UDT is opened.
   m_bOpened = true;

   return m_iPort;
}

__int32 CUDT::open(const char* ip, const __int32& port)
{
   if (m_pcIP)
      delete [] m_pcIP;
   m_pcIP = new char[40];
   strcpy(m_pcIP, ip);
   
   return open(port);
}

void CUDT::listen(const __int64& timeo)
{
   if (m_bMulticast)
   { 
      // not supported now.
      throw CUDTException(5);
 
      if (6 == m_iIPversion)
         m_pChannel->connect6(m_pcMCIP, m_iMCPort);
      else
         m_pChannel->connect(m_pcMCIP, m_iMCPort);
   }
   else
   {
      // Unicast connection set up

      // The UDT entity who calls listen() is an Initiator.
      m_bInitiator = true;

      // Type 0 (handshake) control packet
      CPacket initpkt;

      CHandShake initdata;

      initpkt.pack(0, (char *)&initdata);

      timeval entertime, currtime;
      gettimeofday(&entertime, 0);

      while (true)
      {
         // detect timer out
         gettimeofday(&currtime, 0);
         if ((timeo > 0) && ((currtime.tv_sec - entertime.tv_sec) * 1000000 + (currtime.tv_usec - entertime.tv_usec) > timeo))
            throw CUDTException(1, 5);

         // Listening to the port...
         initpkt.setLength(sizeof(CHandShake));
         *m_pChannel >> initpkt;
         if (initpkt.getLength() <= 0)
            continue;

         // When a peer side connects in...
         if ((1 == initpkt.getFlag()) && (0 == initpkt.getType()))
         {
            // Uses the smaller MTU between the peers        
            if (initdata.m_iMTU > m_iMTU)
               initdata.m_iMTU = m_iMTU;
            else
               m_iMTU = initdata.m_iMTU;

            // Uses the smaller Flight Window between the peers
            if (initdata.m_iFlightFlagSize > m_iFlightFlagSize)
               initdata.m_iFlightFlagSize = m_iFlightFlagSize;
            else
               m_iFlightFlagSize = initdata.m_iFlightFlagSize;

            unsigned char ipnum[16];
            // Pack the ip address into 8-bit sections           
            for (__int32 i = 0; i < 16; i ++)
               ipnum[i] = char(initdata.m_piIP[i]);

            char ipstr[40];

            // Connect to the peer side.
            if (6 == m_iIPversion)
            {
               inet_ntop(AF_INET6, ipnum, ipstr, 40);
               m_pChannel->connect6(ipstr, initdata.m_iPort);
            }
            else
            {
               inet_ntop(AF_INET, ipnum, ipstr, 16);
               m_pChannel->connect(ipstr, initdata.m_iPort);
            }

            // Send back the negotiated configurations.
            *m_pChannel << initpkt;
  
            break;
         }
      }

      m_iPktSize = m_iMTU - 28;
      m_iPayloadSize = m_iPktSize - 4;
   }


   // UDT is now running...
   pthread_create(m_pSndThread, NULL, CUDT::sndHandler, this);
   pthread_create(m_pRcvThread, NULL, CUDT::rcvHandler, this);

   // And of course, it is connected.
   m_bConnected = true;
}

void CUDT::connect(const char* ip, const __int32& port, const __int64& timeo)
{
   if (m_bMulticast)
   {
      // not supported now.
      throw CUDTException(5);

      if (6 == m_iIPversion)
      {
         m_pChannel->addMembership(m_pcMCIP);
         m_pChannel->connect(m_pcMCIP, m_iMCPort);
      }
      else
      {
         m_pChannel->joinGroup(m_pcMCIP);
         m_pChannel->connect6(m_pcMCIP, m_iMCPort);
      }
   }
   else
   {
      // I will connect to an Initiator, so I am NOT an initiator.
      m_bInitiator = false;

      // Connect to the peer side.
      if (6 == m_iIPversion)
         m_pChannel->connect6(ip, port);
      else
         m_pChannel->connect(ip, port);

      CPacket initpkt;
      char initdata[m_iMTU];
      CHandShake* hs = (CHandShake *)initdata;

      // Get local IP address.
      unsigned char ip[16];
      if (6 == m_iIPversion)
         m_pChannel->getAddr6(ip);
      else
         m_pChannel->getAddr(ip); 

      // Unpack the ip address into 32-bit sections to avoid the side effect of network-host bit order conversions
      for (__int32 i = 0; i < 16; i ++)
         hs->m_piIP[i] = (__int32)(ip[i]);

      // This is my current configurations.
      hs->m_iPort = m_iPort;
      hs->m_iMTU = m_iMTU;
      hs->m_iFlightFlagSize = m_iFlightFlagSize;

      initpkt.pack(0, initdata);
 
      // Inform the initiator my configurations.
      *m_pChannel << initpkt;

      // Wait for the negotiated configurations from the peer side.
      initpkt.setLength(m_iMTU);
      *m_pChannel >> initpkt;

      timeval entertime, currtime;
      gettimeofday(&entertime, 0);

      while ((initpkt.getLength() < 0) || (1 != initpkt.getFlag()) || (0 != initpkt.getType()))
      {
         initpkt.setLength(sizeof(CHandShake));
         *m_pChannel << initpkt;

         initpkt.setLength(m_iMTU);
         *m_pChannel >> initpkt;

         gettimeofday(&currtime, 0);
         if ((timeo > 0) && ((currtime.tv_sec - entertime.tv_sec) * 1000000 + (currtime.tv_usec - entertime.tv_usec) > timeo))
            throw CUDTException(1, 5);
      }

      // Got it. Re-configure according to the negotiated values.
      m_iMTU = hs->m_iMTU;
      m_iFlightFlagSize = hs->m_iFlightFlagSize;
      m_iPktSize = m_iMTU - 28;
      m_iPayloadSize = m_iPktSize - 4;
   }


   // Now I am also running, a little while after the Initiator was running.
   pthread_create(m_pSndThread, NULL, CUDT::sndHandler, this);
   pthread_create(m_pRcvThread, NULL, CUDT::rcvHandler, this);

   // And, I am connected too.
   m_bConnected = true;
}

void CUDT::close(const CL_STATUS& wait)
{
   switch (wait)
   {
   case WAIT_NONE:
      break;

   case WAIT_SEND:
      while (m_pSndBuffer->getCurrBufSize() > 0)
         usleep(10);
  
      break;

   case WAIT_RECV:
      break;

   case WAIT_ALL:
      while (m_pSndBuffer->getCurrBufSize() > 0)
         usleep(10);

      break;

   default:
      break;
   }

   // Inform the threads handler to stop.
   m_bClosing = true;
   // Not connected any more.
   m_bConnected = false;

   // Signal the sender if it is waiting for application data.
   pthread_cond_signal(&m_SendDataCond);

   // Wait for the threads to exit.
   pthread_join(*m_pSndThread, NULL);
   pthread_join(*m_pRcvThread, NULL);

   // Channel is to be destroied.
   m_pChannel->disconnect();
   delete m_pChannel;

   // And structures released.
   delete m_pSndBuffer;
   delete m_pRcvBuffer;
   delete m_pSndLossList;
   delete m_pRcvLossList;
   delete m_pIrrPktList;
   delete m_pACKWindow;
   delete m_pSndThread;
   delete m_pRcvThread;

   m_pSndBuffer = NULL;
   m_pRcvBuffer = NULL;
   m_pSndLossList = NULL;
   m_pRcvLossList = NULL;
   m_pIrrPktList = NULL;
   m_pACKWindow = NULL;
   m_pSndThread = NULL;
   m_pRcvThread = NULL;

   if (m_pcIP)
      delete [] m_pcIP;
   m_pcIP = NULL;

   // CLOSED.
   m_bOpened = false;
}

void* CUDT::sndHandler(void* sender)
{
   CUDT* self = static_cast<CUDT *>(sender);

   CPacket datapkt;
   __int32 payload;
   __int32 offset;

   bool probe = false;

   unsigned __int64 entertime;

   while (!self->m_bClosing)
   {
      // Remember the time the last packet is sent.
      self->m_pTimer->rdtsc(entertime);

      // Loss retransmission always has higher priority.
      if ((datapkt.m_iSeqNo = self->m_pSndLossList->getLostSeq()) >= 0)
      {
         // protect m_iSndLastAck from updating by ACK processing
         CGuard ackguard(self->m_AckLock);

         if ((datapkt.m_iSeqNo >= self->m_iSndLastAck) && (datapkt.m_iSeqNo < self->m_iSndLastAck + self->m_iSeqNoTH))
            offset = (datapkt.m_iSeqNo - self->m_iSndLastAck) * self->m_iPayloadSize;
         else if (datapkt.m_iSeqNo < self->m_iSndLastAck - self->m_iSeqNoTH)
            offset = (datapkt.m_iSeqNo + self->m_iMaxSeqNo - self->m_iSndLastAck) * self->m_iPayloadSize;
         else
            continue;

         if ((payload = self->m_pSndBuffer->readData(&(datapkt.m_pcData), offset, self->m_iPayloadSize)) == 0)
            continue;
      }
      // If no loss, pack a new packet.
      else
      {
         if (self->m_iFlowWindowSize <= ((self->m_iSndCurrSeqNo - self->m_iSndLastAck + 1 + self->m_iMaxSeqNo) % self->m_iMaxSeqNo))
         {
            //wait for next packet sending time
            self->m_pTimer->sleepto(entertime + self->m_ullInterval);

            continue;
         }

         if (0 == (payload = self->m_pSndBuffer->readData(&(datapkt.m_pcData), self->m_iPayloadSize)))
         {
            //check if the sender buffer is empty
            if (0 == self->m_pSndBuffer->getCurrBufSize())
            {
               // If yes, sleep here until a signal comes.
               pthread_mutex_lock(&(self->m_SendDataLock));
               while ((0 == self->m_pSndBuffer->getCurrBufSize()) && (!self->m_bClosing))
                  pthread_cond_wait(&(self->m_SendDataCond), &(self->m_SendDataLock));
               pthread_mutex_unlock(&(self->m_SendDataLock));
            }

            continue;
         }

         self->m_iSndCurrSeqNo = (self->m_iSndCurrSeqNo + 1) % self->m_iMaxSeqNo;
         datapkt.m_iSeqNo = self->m_iSndCurrSeqNo;
 
         if (0 == self->m_iSndCurrSeqNo % self->m_iProbeInterval)
            probe = true;
      }

      // Now sending.
      datapkt.setLength(payload);
      *(self->m_pChannel) << datapkt;

      self->m_iLocalSend ++;

      if (probe)
      {
         probe = false;

         // sends out probing packet pair
         continue;
      }
      else if (self->m_bFreeze)
      {
         // sending is fronzen!
         self->m_pTimer->sleepto(entertime + self->m_iSYNInterval * self->m_ullCPUFrequency + self->m_ullInterval);

         self->m_bFreeze = false;
      }
      else
         // wait for an inter-packet time.
         self->m_pTimer->sleepto(entertime + self->m_ullInterval);
   }

   return NULL;
}

void* CUDT::rcvHandler(void* recver)
{
   CUDT* self = static_cast<CUDT *>(recver);

   CPacket packet;
   char payload[self->m_iPayloadSize];
   bool nextslotfound;
   __int32 offset;
   __int32 loss;

   // time
   unsigned __int64 currtime;
   unsigned __int64 nextacktime;
   unsigned __int64 nextnaktime;
   unsigned __int64 nextsyntime;
   unsigned __int64 nextexptime;

   // SYN interval, in clock cycles
   unsigned __int64 ullsynint = self->m_iSYNInterval * self->m_ullCPUFrequency;

   // ACK, NAK, and EXP intervals, in clock cycles
   unsigned __int64 ullackint = ullsynint;
   unsigned __int64 ullnakint = self->m_iRTT * self->m_ullCPUFrequency;
   unsigned __int64 ullexpint = 11 * ullsynint;

   // Set up the timers.
   self->m_pTimer->rdtsc(nextsyntime);
   nextsyntime += ullsynint;
   self->m_pTimer->rdtsc(nextacktime);
   nextacktime += ullackint;
   self->m_pTimer->rdtsc(nextnaktime);
   nextnaktime += ullnakint;
   self->m_pTimer->rdtsc(nextexptime);
   nextexptime += ullexpint;
   

   while (!self->m_bClosing)
   {
      // "recv"/"recvfile" is called, blocking mode is activated, and not enough received data in the protocol buffer
      if (self->m_bReadBuf)
      {
         // Check if there is enough data now.
         pthread_mutex_lock(&(self->m_RecvDataLock));
         self->m_bReadBuf = self->m_pRcvBuffer->readBuffer(const_cast<char*>(self->m_pcTempData), const_cast<__int32&>(self->m_iTempLen));
         pthread_mutex_unlock(&(self->m_RecvDataLock));

         // Still no?! Register the application buffer.
         if (!self->m_bReadBuf)
         {
            offset = self->m_pRcvBuffer->registerUserBuf(const_cast<char*>(self->m_pcTempData), const_cast<__int32&>(self->m_iTempLen));
            // there is no seq. wrap for user buffer border. If it exceeds the max. seq., we just ignore it.
            self->m_iUserBufBorder = self->m_iRcvLastAck + (__int32)ceil(double(self->m_iTempLen - offset) / self->m_iPayloadSize);
         }
         else
         // Otherwise, inform the blocked "recv"/"recvfile" call that the expected data has arrived.
         {
            self->m_bReadBuf = false;
            pthread_cond_signal(&(self->m_RecvDataCond));
         }
      }

      self->m_pTimer->rdtsc(currtime);

      // temperory variable to record first loss in receiver's loss list
      loss = self->m_pRcvLossList->getFirstLostSeq();

      // Query the timers if any of them is expired.
      if ((currtime > nextacktime) || (loss >= self->m_iUserBufBorder) || (((self->m_iRcvCurrSeqNo + 1) % self->m_iMaxSeqNo >= self->m_iUserBufBorder) && (loss < 0)))
      {
         // ACK timer expired, or user buffer is fulfilled.
         self->sendCtrl(2);

         self->m_pTimer->rdtsc(currtime);
         nextacktime = currtime + ullackint;
      }
      if ((currtime > nextnaktime) && (loss >= 0))
      {
         // NAK timer expired, and there is loss to be reported.
         self->sendCtrl(3);

         self->m_pTimer->rdtsc(currtime);
         nextnaktime = currtime + ullnakint;
      }
      if (currtime > nextsyntime)
      {
         ullnakint = (self->m_iRTT * self->m_ullCPUFrequency);
         //do not resent the loss report within too short period
         if (ullnakint < ullsynint)
            ullnakint = ullsynint;

         // Periodical rate control.
         if (self->m_iLocalSend > 0)
            self->rateControl();

         self->m_pTimer->rdtsc(currtime);
         nextsyntime = currtime + ullsynint;
      }
      if ((currtime > nextexptime) && (0 == self->m_pSndLossList->getLossLength()))
      {
         // Haven't receive any information from the peer, it is dead?!
         if (self->m_iEXPCount > 32)
         {
            //
            // Connection is broken. 
            // UDT does not signal any information about this instead of to stop quietly.
            // Apllication will detect this when it calls any UDT methods next time.
            //
            self->m_bClosing = true;
            self->m_bConnected = false;

            pthread_cond_signal(&(self->m_SendBlockCond));
            pthread_cond_signal(&(self->m_RecvDataCond));

            continue;
         }

         // sender: Insert all the packets sent after last received acknowledgement into the sender loss list.
         if (((self->m_iSndCurrSeqNo + 1) % self->m_iMaxSeqNo) != self->m_iSndLastAck)
            self->m_pSndLossList->insert(const_cast<__int32&>(self->m_iSndLastAck), self->m_iSndCurrSeqNo);
         // receiver only: send out a keep-alive packet
         else
            self->sendCtrl(1);

         ++ self->m_iEXPCount;
         ullexpint = (self->m_iEXPCount * self->m_iRTT + self->m_iSYNInterval) * self->m_ullCPUFrequency;

         self->m_pTimer->rdtsc(currtime);
         nextexptime = currtime + ullexpint;
      }

      ////////////////////////////////////////////////////////////////////////////////////////////
      // Below is the packet receiving/processing part.

      packet.setLength(self->m_iPayloadSize);

      offset = self->m_iNextExpect - self->m_iRcvLastAck;
      if (offset < -self->m_iSeqNoTH)
         offset += self->m_iMaxSeqNo;

      // Look for a slot for the speculated data.
      if (!(self->m_pRcvBuffer->nextDataPos(&(packet.m_pcData), offset * self->m_iPayloadSize - self->m_pIrrPktList->currErrorSize(offset + self->m_iRcvLastAck), self->m_iPayloadSize)))
      {
         packet.m_pcData = payload;
         nextslotfound = false;
      }
      else
         nextslotfound = true;

      // Receiving...
      *(self->m_pChannel) >> packet;

      // Got nothing?
      if (packet.getLength() <= 0)
         continue;

      // Just heard from the peer, reset the expiration count.
      self->m_iEXPCount = 0;
      if (((self->m_iSndCurrSeqNo + 1) % self->m_iMaxSeqNo) == self->m_iSndLastAck)
         nextexptime = currtime + ullexpint;

      // But this is control packet, process it!
      if (packet.getFlag())
      {
         self->m_pTimer->rdtsc(currtime);
         if ((2 <= packet.getType()) && (4 >= packet.getType()))
            nextexptime = currtime + ullexpint;
         self->processCtrl(packet);
         continue;
      }

      // update time/delay information
      self->m_pRcvTimeWindow->pktArrival();

      // check if it is probing packet pair
      if (packet.m_iSeqNo % self->m_iProbeInterval < 2)
      {
         if (0 == packet.m_iSeqNo % self->m_iProbeInterval)
            self->m_pRcvTimeWindow->probe1Arrival();
         else
            self->m_pRcvTimeWindow->probe2Arrival();
      }

      // update the number of packets received since last SYN
      //self->m_iLocalRecv ++;

      offset = packet.m_iSeqNo - self->m_iRcvLastAck;
      if (offset < -self->m_iSeqNoTH)
         offset += self->m_iMaxSeqNo;

      // Data is too old, discard it!
      if ((offset >= self->m_iFlightFlagSize) || (offset < 0))
         continue;

      // Oops, the speculation is wrong...
      if ((packet.m_iSeqNo != self->m_iNextExpect) || (!nextslotfound))
      {
         // Put the received data explicitly into the right slot.
         if (!(self->m_pRcvBuffer->addData(packet.m_pcData, offset * self->m_iPayloadSize - self->m_pIrrPktList->currErrorSize(packet.m_iSeqNo), packet.getLength())))
            continue;

         // Loss detection.
         if (((packet.m_iSeqNo > self->m_iRcvCurrSeqNo + 1) && (packet.m_iSeqNo - self->m_iRcvCurrSeqNo < self->m_iSeqNoTH)) || (packet.m_iSeqNo < self->m_iRcvCurrSeqNo - self->m_iSeqNoTH))
         {
            // If loss found, insert them to the receiver loss list
            self->m_pRcvLossList->insert(self->m_iRcvCurrSeqNo + 1, packet.m_iSeqNo - 1);

            // pack loss list for NAK
            __int32 lossdata[2];
            lossdata[0] = (self->m_iRcvCurrSeqNo + 1) | 0x80000000;
            lossdata[1] = packet.m_iSeqNo - 1;
            __int32 losslen = packet.m_iSeqNo - self->m_iRcvCurrSeqNo - 1;
            if (losslen < 0)
               losslen += self->m_iMaxSeqNo;

            // Generate loss report immediately.
            self->sendCtrl(3, &losslen, lossdata);
         }
      }

      // This is not a regular fixed size packet...
      if (packet.getLength() != self->m_iPayloadSize)
         self->m_pIrrPktList->addIrregularPkt(packet.m_iSeqNo, self->m_iPayloadSize - packet.getLength());

      // Update the current largest sequence number that has been received.
      if (((packet.m_iSeqNo > self->m_iRcvCurrSeqNo) && (packet.m_iSeqNo - self->m_iRcvCurrSeqNo < self->m_iSeqNoTH)) || (packet.m_iSeqNo < self->m_iRcvCurrSeqNo - self->m_iSeqNoTH))
      {
         self->m_iRcvCurrSeqNo = packet.m_iSeqNo;

         // Speculate next packet.
         self->m_iNextExpect = (self->m_iRcvCurrSeqNo + 1) % self->m_iMaxSeqNo;
      }
      else
      // Or it is a retransmitted packet, remove it from receiver loss list.
      {
         self->m_pRcvLossList->remove(packet.m_iSeqNo);

         if (packet.getLength() < self->m_iPayloadSize)
            self->m_pRcvBuffer->moveData((offset + 1) * self->m_iPayloadSize - self->m_pIrrPktList->currErrorSize(packet.m_iSeqNo), self->m_iPayloadSize - packet.getLength());
      }
   }

   return NULL;
}

void CUDT::sendCtrl(const __int32& pkttype, void* lparam, void* rparam)
{
   CPacket ctrlpkt;

   __int32 losslen[2];
   __int32 ack;
   __int32 data[m_iPayloadSize];

   unsigned __int64 currtime;

   switch (pkttype)
   {
   case 2: //010 - Acknowledgement
      // If there is no loss, the ACK is the current largest sequence number plus 1.
      if (0 == m_pRcvLossList->getLossLength())
      {
         if ((m_iRcvCurrSeqNo >= m_iRcvLastAck) && (m_iRcvCurrSeqNo - m_iRcvLastAck < m_iSeqNoTH))
            ack = m_iRcvCurrSeqNo - m_iRcvLastAck + 1;
         else if (m_iRcvLastAck - m_iRcvCurrSeqNo > m_iSeqNoTH)
            ack = m_iRcvCurrSeqNo + m_iMaxSeqNo - m_iRcvLastAck + 1;
         else
            break;
      }
      else
      // Otherwise it is the smallest sequence number in the receiver loss list.
      {
         ack = m_pRcvLossList->getFirstLostSeq() - m_iRcvLastAck;

         if (ack > m_iSeqNoTH)
            break;
         else if (ack < -m_iSeqNoTH)
            ack += m_iMaxSeqNo;
      }

      m_pTimer->rdtsc(currtime);

      // There is new received packet to acknowledge, update related information.
      if (0 < ack)
      {
         m_iRcvLastAck = (m_iRcvLastAck + ack) % m_iMaxSeqNo;

         if (m_pRcvBuffer->ackData(ack * m_iPayloadSize - m_pIrrPktList->currErrorSize(m_iRcvLastAck)))
         {
            pthread_cond_signal(&m_RecvDataCond);
            m_iUserBufBorder = m_iRcvLastAck + (__int32)ceil(double(m_iUDTBufSize) / m_iPayloadSize);
         }

         m_pIrrPktList->deleteIrregularPkt(m_iRcvLastAck);
      }
      else if ((__int32)(currtime - m_ullLastAckTime) < (2 * m_iRTT))
         break;

      // Send out the ACK only if has not been received by the sender before
      if (((m_iRcvLastAck > m_iRcvLastAckAck) && (m_iRcvLastAck - m_iRcvLastAckAck < m_iSeqNoTH)) || (m_iRcvLastAck < m_iRcvLastAckAck - m_iSeqNoTH))
      {
         m_iAckSeqNo = (m_iAckSeqNo + 1) % m_iMaxAckSeqNo;
         data[0] = m_iRcvLastAck;
         data[1] = m_iRTT;
         data[2] = m_pRcvTimeWindow->getPktSpeed();
         data[3] = m_pRcvTimeWindow->getBandwidth();
         ctrlpkt.pack(2, &m_iAckSeqNo, data);
         *m_pChannel << ctrlpkt;

         m_pACKWindow->store(m_iAckSeqNo, m_iRcvLastAck);

         m_pTimer->rdtsc(m_ullLastAckTime);

         //m_iLocalRecv = 0;
      }

      break;

   case 6: //110 - Acknowledgement of Acknowledgement
      ctrlpkt.pack(6, lparam);

      *m_pChannel << ctrlpkt;

      break;

   case 3: //011 - Loss Report
      if (lparam)
         if (1 == *(__int32 *)lparam)
         {
            // only 1 loss packet

            losslen[0] = 1;
            losslen[1] = 1;
            ctrlpkt.pack(3, losslen, (__int32 *)rparam + 1);
         }
         else
         {
            // more than 1 loss packets

            losslen[0] = *(__int32 *)lparam;
            losslen[1] = 2;
            ctrlpkt.pack(3, losslen, rparam);
         }
      else if (m_pRcvLossList->getLossLength() > 0)
      {
         // this is periodically NAK report

         // read loss list from the local receiver loss list
         m_pRcvLossList->getLossArray(data, losslen, m_iPayloadSize / sizeof(__int32), m_iRTT);

         if (0 == losslen[0])
            break;

         ctrlpkt.pack(3, losslen, data);
      }
      else
         // no loss, break
         break;

      *m_pChannel << ctrlpkt;

      break;

   case 4: //100 - Congestion Warning
      ctrlpkt.pack(4, data);

      *m_pChannel << ctrlpkt;

      m_pTimer->rdtsc(m_ullLastWarningTime);

      break;

   case 1: //001 - Keep-alive
      //
      // The "&ack" in the parameter list is meaningless.
      // It is just for the convinience of the implementation.
      //
      ctrlpkt.pack(1, data);
      *m_pChannel << ctrlpkt;

      break;

   case 0: //000 - Handshake
      ctrlpkt.pack(0, lparam);
      *m_pChannel << ctrlpkt;

      break;

   case 5: //101 - Unused
      break;

   case 7: //111 - Resevered for future use
      break;

   default:
      break;
   }
}

void CUDT::processCtrl(CPacket& ctrlpkt)
{
   __int32 ack;
   __int32* losslist;
   __int32 rtt = -1;
   unsigned __int64 currtime;

   switch (ctrlpkt.getType())
   {
   case 2: //010 - Acknowledgement
      // read ACK seq. no.
      ack = ctrlpkt.getAckSeqNo();

      // send ACK acknowledgement
      sendCtrl(6, &ack);

      // Got data ACK
      ack = *(__int32 *)ctrlpkt.m_pcData;

      // protect packet retransmission
      pthread_mutex_lock(&m_AckLock);

      // acknowledge the sending buffer
      if ((ack > m_iSndLastAck) && (ack - m_iSndLastAck < m_iSeqNoTH))
         m_pSndBuffer->ackData((ack - m_iSndLastAck) * m_iPayloadSize, m_iPayloadSize);
      else if (ack < m_iSndLastAck - m_iSeqNoTH)
         m_pSndBuffer->ackData((ack - m_iSndLastAck + m_iMaxSeqNo) * m_iPayloadSize, m_iPayloadSize);
      else
      {
         // discard it if it is a repeated ACK
         pthread_mutex_unlock(&m_AckLock);
         break;
      }

      // update sending variables
      m_iSndLastAck = ack;
      m_pSndLossList->remove((m_iSndLastAck - 1 + m_iMaxSeqNo) % m_iMaxSeqNo);

      pthread_mutex_unlock(&m_AckLock);

      // signal a waiting "send" call if all the data has been sent
      pthread_mutex_lock(&m_SendBlockLock);
      if ((m_bSynSending) && (0 == m_pSndBuffer->getCurrBufSize()))
         pthread_cond_signal(&m_SendBlockCond);
      pthread_mutex_unlock(&m_SendBlockLock);

      // Update RTT
      if (m_iRTT == m_iSYNInterval)
         m_iRTT = *((__int32 *)ctrlpkt.m_pcData + 1);
      else
         m_iRTT = (m_iRTT * 7 + *((__int32 *)ctrlpkt.m_pcData + 1)) >> 3;

      // Update Flow Window Size
      flowControl(*((__int32 *)ctrlpkt.m_pcData + 2));

      // Update Estimated Bandwidth
      if (0 != *((__int32 *)ctrlpkt.m_pcData + 3))
         m_iBandwidth = (m_iBandwidth * 7 + *((__int32 *)ctrlpkt.m_pcData + 3)) >> 3;

      // Wake up the waiting sender and correct the sending rate
      if (m_ullInterval > m_iRTT * m_ullCPUFrequency)
      {
         m_ullInterval = m_iRTT * m_ullCPUFrequency;
         m_pTimer->interrupt();
      }

      break;

   case 6: //110 - Acknowledgement of Acknowledgement
      // update RTT
      rtt = m_pACKWindow->acknowledge(ctrlpkt.getAckSeqNo(), ack);

      if (rtt <= 0)
         break;

      m_pRcvTimeWindow->ack2Arrival(rtt);

      // check packet delay trend
      m_pTimer->rdtsc(currtime);
      if (m_pRcvTimeWindow->getDelayTrend() && (currtime - m_ullLastWarningTime > m_iRTT * m_ullCPUFrequency * 2))
         sendCtrl(4);

      // RTT EWMA
      if (m_iRTT == m_iSYNInterval)
         m_iRTT = rtt;
      else
         m_iRTT = (m_iRTT * 7 + rtt) >> 3;

      // update last ACK that has been received by the sender
      if (((m_iRcvLastAckAck < ack) && (ack - m_iRcvLastAckAck < m_iSeqNoTH)) || (m_iRcvLastAckAck > ack + m_iSeqNoTH))
         m_iRcvLastAckAck = ack;

      break;

   case 3: //011 - Loss Report
      //Slow Start Stopped, if it is not
      m_bSlowStart = false;

      losslist = (__int32 *)(ctrlpkt.m_pcData);

      // Rate Control on Loss
      if ((((losslist[0] & 0x7FFFFFFF) > m_iLastDecSeq) && ((losslist[0] & 0x7FFFFFFF) - m_iLastDecSeq < m_iSeqNoTH)) || ((losslist[0] & 0x7FFFFFFF) < m_iLastDecSeq - m_iSeqNoTH))
      {
         m_ullInterval = (__int64)(m_ullInterval * 1.125);

         m_iLastDecSeq = m_iSndCurrSeqNo;

         m_bFreeze = true;

         m_iNAKCount = 1;
         m_iDecCount = 4;
      }
      else if (++ m_iNAKCount >= pow(2.0, m_iDecCount))
      {
         m_iDecCount ++;

         m_ullInterval = (__int64)(m_ullInterval * 1.125);
      }

      losslist = (__int32 *)(ctrlpkt.m_pcData);

      // decode loss list message and insert loss into the sender loss list
      for (__int32 i = 0, n = (__int32)(ctrlpkt.getLength() / sizeof(__int32)); i < n; i ++)
         if ((losslist[i] & 0x80000000) && ((losslist[i] & 0x7FFFFFFF) >= m_iSndLastAck))
         {
            m_iLocalLoss += m_pSndLossList->insert(losslist[i] & 0x7FFFFFFF, losslist[i + 1]);
            i ++;
         }
         else
            if (losslist[i] >= m_iSndLastAck)
               m_iLocalLoss += m_pSndLossList->insert(losslist[i], losslist[i]);

      break;

   case 4: //100 - Delay Warning
      //Slow Start Stopped, if it is not
      m_bSlowStart = false;

      // One way packet delay is increasing, so decrease the sending rate
      m_ullInterval = (__int64)ceil(m_ullInterval * 1.125);

      m_iLastDecSeq = m_iSndCurrSeqNo;

      m_iNAKCount = 1;
      m_iDecCount = 4;

      break;

   case 1: //001 - Keep-alive
      // The only purpose of keep-alive packet is to tell the peer is still alive
      // nothing need to be done.

      break;

   case 0: //000 - Handshake
      if (m_bInitiator)
      {
         // The peer side has not received the handshake message, so it keeping query
         // resend the handshake packet

         CHandShake initdata;
         initdata.m_iMTU = m_iMTU;
         initdata.m_iFlightFlagSize = m_iFlightFlagSize;
         sendCtrl(0, (char *)&initdata);
      }

      // I am not an initiator, so both the initiator and I must have received the message before I came here

      break;

   case 5: //101 - Unused
      break;

   case 7: //111 - Reserved for future use
      break;

   default:
      break;
   }
}

void CUDT::rateControl()
{
   double currlossrate = m_iLocalLoss / m_iLocalSend;

   if (currlossrate > 1.0)
      currlossrate = 1.0;

   m_iLocalSend = 0;
   m_iLocalLoss = 0;

   m_dLossRate = m_dLossRate * m_dWeight + currlossrate * (1 - m_dWeight);

   if (m_dLossRate > m_dLossRateLimit)
      return;

   // During Slow Start, no rate increase
   if (m_bSlowStart)
      return;

   double inc;

   if (1000000.0 / m_ullInterval * m_ullCPUFrequency > m_iBandwidth)
      inc = 1.0 / m_iMTU;
   else
   {
      // inc = max(10 ^ ceil(log10( (B - C)*MTU * 8 ) * Beta / MTU, 1/MTU)
      // Beta = 1.5 * 10^(-6)

      inc = pow(10, ceil(log10((m_iBandwidth - 1000000.0 / m_ullInterval * m_ullCPUFrequency) * m_iMTU * 8))) * 0.0000015 / m_iMTU;

      if (inc < 1.0/m_iMTU)
         inc = 1.0/m_iMTU;
   }

   //double inc = pow(10, ceil(log10(m_iBandwidth))) / (1000.0 * (1000000.0 / m_iSYNInterval));

   m_ullInterval = (__int64)((m_ullInterval * m_iSYNInterval * m_ullCPUFrequency) / (m_ullInterval * inc + m_iSYNInterval * m_ullCPUFrequency));

   if (m_ullInterval < m_ullCPUFrequency)
      m_ullInterval = m_ullCPUFrequency;
}

void CUDT::flowControl(const __int32& recvrate)
{
   if (m_bSlowStart)
   {
      m_iFlowWindowSize = m_iSndLastAck;

      //if (recvrate > 0)
      //{
      //   if (m_ullCPUFrequency == m_ullInterval)
      //      m_ullInterval = (__int64)(1000000.0 / recvrate * m_ullCPUFrequency);
      //   else
      //      m_ullInterval = (__int64)(m_ullInterval * 0.875 + 1000000.0 / recvrate * m_ullCPUFrequency * 0.125);
      //}
   }
   else if (recvrate > 0)
      m_iFlowWindowSize = (__int32)ceil(m_iFlowWindowSize * 0.875 + recvrate / 1000000.0 * (m_iRTT + m_iSYNInterval) * 0.125);

   if (m_iFlowWindowSize > m_iFlightFlagSize)
   {
      m_iFlowWindowSize = m_iFlightFlagSize;
      m_bSlowStart = false;
   }
}

void CUDT::send(char* data, const __int32& len)
{
   CGuard sendguard(m_SendLock);

   // throw an exception if not connected
   if (!m_bConnected)
      throw CUDTException(2);

   if (len <= 0)
      return;

   // insert the user buffer into the sening list
   pthread_mutex_lock(&m_SendDataLock);
   m_pSndBuffer->addBuffer(data, len, m_iMemFlag);
   pthread_mutex_unlock(&m_SendDataLock);

   // release the data automatically when it is sent if memflag is 1
   if (1 == m_iMemFlag)
      data = NULL;

   // signal the sending thread in case that it is waiting
   pthread_cond_signal(&m_SendDataCond);

   // wait here during a blocking sending
   pthread_mutex_lock(&m_SendBlockLock);
   while ((m_bConnected) && (m_bSynSending) && (0 != m_pSndBuffer->getCurrBufSize()))
      pthread_cond_wait(&m_SendBlockCond, &m_SendBlockLock);
   pthread_mutex_unlock(&m_SendBlockLock);

   // check if the sending is successful or the connection is broken
   if (!m_bConnected)
      throw CUDTException(2);
}

void CUDT::recv(char* data, const __int32& len)
{
   CGuard recvguard(m_RecvLock);

   // throw an exception if not connected
   if (!m_bConnected)
      throw CUDTException(2);

   if (len <= 0)
      return;

   // try to read data from the protocol buffer
   if (m_pRcvBuffer->readBuffer(data, len))
      return;

   // return in non-blocking mode
   if (!m_bSynRecving)
      return;

   // otherwise register the user buffer and wait here
   pthread_mutex_lock(&m_RecvDataLock);

   m_pcTempData = data;
   m_iTempLen = len;
   m_bReadBuf = true;

   pthread_cond_wait(&m_RecvDataCond, &m_RecvDataLock);
   pthread_mutex_unlock(&m_RecvDataLock);

   // check if the receiving is successful or the connection is broken
   if (!m_bConnected)
      throw CUDTException(2);

   return;
}

void CUDT::sendfile(ifstream& ifs, const __int64& offset, const __int64& size)
{
   CGuard sendguard(m_SendLock);

   if (!m_bConnected)
      throw CUDTException(2);

   if (size <= 0)
      return;

   char* tempbuf;
   __int32 unitsize = 367000;
   __int64 count = 1;

   // positioning...
   try
   {
      ifs.seekg(offset);
   }
   catch (...)
   {
      throw CUDTException(4, 1);
   }

   // sending block by block
   while (unitsize * count <= size)
   {
      tempbuf = new char[unitsize];

      try
      {
         ifs.read(tempbuf, unitsize) < 0;

         pthread_mutex_lock(&m_SendDataLock);
         while (getCurrSndBufSize() >= 44040000)
            usleep(10);
         m_pSndBuffer->addBuffer(tempbuf, unitsize);
         pthread_mutex_unlock(&m_SendDataLock);
 
         pthread_cond_signal(&m_SendDataCond);
      }
      catch (CUDTException e)
      {
         throw e;
      }
      catch (...)
      {
         throw CUDTException(4, 2);
      }

      count ++;
   }
   if (size - unitsize * (count - 1) > 0)
   {
      tempbuf = new char[size - unitsize * (count - 1)];

      try
      {
         ifs.read(tempbuf, size - unitsize * (count - 1));

         pthread_mutex_lock(&m_SendDataLock);
         while (getCurrSndBufSize() >= 44040000)
            usleep(10);
         m_pSndBuffer->addBuffer(tempbuf, (__int32)(size - unitsize * (count - 1)));
         pthread_mutex_unlock(&m_SendDataLock);

         pthread_cond_signal(&m_SendDataCond);
      }
      catch (CUDTException e)
      {
         throw e;
      }
      catch (...)
      {
         throw CUDTException(4, 2);
      }
   }

   // Wait until all the data is sent out
   try
   {
      while (getCurrSndBufSize() > 0)
         usleep(10);
   }
   catch (CUDTException e)
   {
      throw e;
   }
}

void CUDT::recvfile(ofstream& ofs, const __int64& offset, const __int64& size)
{
   if (!m_bConnected)
      throw CUDTException(2);

   if (size <= 0)
      return;

   __int32 unitsize = 7340000;
   __int64 count = 1;
   char* tempbuf = new char[unitsize];

   // "recvfile" is always blocking.   
   bool syn = m_bSynRecving;
   m_bSynRecving = true;

   // positioning...
   try
   {
      ofs.seekp(offset);
   }
   catch (...)
   {
      throw CUDTException(4, 3);
   }

   // receiving...
   while (unitsize * count <= size)
   {
      try
      {
         recv(tempbuf, unitsize);
         ofs.write(tempbuf, unitsize);
      }
      catch (CUDTException e)
      {
         throw e;
      }
      catch (...)
      {
         throw CUDTException(4, 4);
      }

      count ++;
   }
   if (size - unitsize * (count - 1) > 0)
   {
      try
      {
         recv(tempbuf, (__int32)(size - unitsize * (count - 1)));
         ofs.write(tempbuf,  size - unitsize * (count - 1));
      }
      catch (CUDTException e)
      {
         throw e;
      }
      catch (...)
      {
         throw CUDTException(4, 4);
      }
   }

   // recover the original receiving mode
   m_bSynRecving = syn;

   delete [] tempbuf;

   return;
}

__int32 CUDT::getCurrSndBufSize()
{
   if (!m_bConnected)
      throw CUDTException(2);

   return m_pSndBuffer->getCurrBufSize();
}
