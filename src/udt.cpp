/*****************************************************************************
Copyright © 2001 - 2004, The Board of Trustees of the University of Illinois.
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
   Yunhong Gu [ygu@cs.uic.edu], last updated 02/17/2004
*****************************************************************************/

#ifndef WIN32
   #include <unistd.h>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <errno.h>
   #include <string.h>
   #include <stdlib.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif

#include <math.h>
#include "udt.h"


CUDT::CUDT():
//
// These constants are defined in UDT specification. They MUST NOT be changed!
//
m_iVersion(1),
m_iSYNInterval(10000),
m_iMaxSeqNo(1 << 30),
m_iSeqNoTH(1 << 29),
m_iMaxAckSeqNo(1 << 16),
m_iProbeInterval(16),
m_dLossRateLimit(0.01),
m_dWeight(0.125)
{
   m_pChannel = NULL;
   m_pSndBuffer = NULL;
   m_pRcvBuffer = NULL;
   m_pSndLossList = NULL;
   m_pRcvLossList = NULL;
   m_pTimer = NULL;
   m_pIrrPktList = NULL;
   m_pACKWindow = NULL;
   m_pRcvTimeWindow = NULL;

   // Initilize mutex and condition variables
   #ifndef WIN32
      pthread_mutex_init(&m_SendDataLock, NULL);
      pthread_cond_init(&m_SendDataCond, NULL);
      pthread_mutex_init(&m_SendBlockLock, NULL);
      pthread_cond_init(&m_SendBlockCond, NULL);
      pthread_mutex_init(&m_RecvDataLock, NULL);
      pthread_cond_init(&m_RecvDataCond, NULL);
      pthread_mutex_init(&m_SendLock, NULL);
      pthread_mutex_init(&m_RecvLock, NULL);
      pthread_mutex_init(&m_AckLock, NULL);
      pthread_mutex_init(&m_ShutdownLock, NULL);
   #else
      m_SendDataLock = CreateMutex(NULL, false, "SendDataLock");
      m_SendDataCond = CreateEvent(NULL, false, false, "SendDataCond");
      m_SendBlockLock = CreateMutex(NULL, false, "SendBlockLock");
      m_SendBlockCond = CreateEvent(NULL, false, false, "SendBlockCond");
      m_RecvDataLock = CreateMutex(NULL, false, "RecvDataLock");
      m_RecvDataCond = CreateEvent(NULL, false, false, "RecvDataCond");
      m_SendLock = CreateMutex(NULL, false, "SendLock");
      m_RecvLock = CreateMutex(NULL, false, "RecvLock");
      m_AckLock = CreateMutex(NULL, false, "AckLock");
      m_ShutdownLock = CreateMutex(NULL, false, "ShutdownLock");
   #endif

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
   if (m_pRcvTimeWindow)
      delete m_pRcvTimeWindow;
   if (m_pcIP)
      delete [] m_pcIP;
}

void CUDT::setOpt(UDTOpt optName, const void* optval, const __int32& optlen)
{
   switch (optName)
   {
   case UDT_ADDR:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      if (m_pcIP)
         delete [] m_pcIP;
      m_pcIP = new char[40];
      memcpy(m_pcIP, optval, optlen);
      break;

   case UDT_PORT:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iPort = *(__int32 *)optval;
      break;

   case UDT_PCH:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_bPortChangable = *(bool *)optval;
      break;

   case UDT_MTU:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iMTU = *(__int32 *)optval;
      if (m_iMTU < 28)
         throw CUDTException(5, 3, 0);
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
         throw CUDTException(5, 3, 0);
      break;

   case UDT_RC:
      throw CUDTException(5, 0, 0);

      m_iRCAlg = *(__int32 *)optval;
      break;

   case UDT_FC:
      if (m_bConnected)
         throw CUDTException(5, 2, 0);

      m_iFlightFlagSize = *(__int32 *)optval;
      if (m_iFlightFlagSize < 1)
         throw CUDTException(5, 3);
      break;

   case UDT_BUF:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iUDTBufSize = *(__int32 *)optval;
      if (m_iUDTBufSize < m_iMTU - 28)
         throw CUDTException(5, 3, 0);
      break;

   case UDT_USB:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iUDPSndBufSize = *(__int32 *)optval;
      break;

   case UDT_URB:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iUDPRcvBufSize = *(__int32 *)optval;
      break;

   case UDT_IPV:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iIPversion = *(__int32 *)optval;
      if ((4 != m_iIPversion) && (6 != m_iIPversion))
         throw CUDTException(5, 3, 0);
      break;

   case UDT_MC:
      throw CUDTException(5, 0, 0);

      m_bMulticast = *(bool *)optval;
      break;

   case UDT_MCADDR:
      throw CUDTException(5, 0, 0);

      memcpy(m_pcMCIP, optval, optlen);
      break;

   case UDT_MCPORT:
      throw CUDTException(5, 0, 0);

      m_iMCPort = *(__int32 *)optval;
      break;

   case UDT_LOSSY:
      throw CUDTException(5, 0, 0);

      m_bLossy = *(bool *)optval;
      break;

   default:
      throw CUDTException(5, 0, 0);
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
      throw CUDTException(5, 0, 0);
   }
}

__int32 CUDT::open(const __int32& port)
{
   if (m_bOpened)
      close();

   // Initial status
   m_bClosing = false;
   m_bShutdown = false;
   m_iEXPCount = 0;

   // Initial sequence number, loss, acknowledgement, etc.
   m_iISN = 0;
 
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

   m_iAckSeqNo = 0;

   // Initial sending rate = 1us
   m_ullInterval = m_ullCPUFrequency;

   // Initial Window Size = 2 packet
   m_iFlowWindowSize = 2;
   m_iMaxFlowWindowSize = m_iFlightFlagSize;

   #ifdef TRACE
      // trace information
      m_iTraceSend = m_iTraceRecv = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = m_iTraceLoss = 0;
   #endif

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
   m_pRcvLossList = new CRcvLossList(m_iFlightFlagSize, m_iSeqNoTH, m_iMaxSeqNo);
   m_pTimer = new CTimer;
   m_pIrrPktList = new CIrregularPktList(m_iFlightFlagSize, m_iSeqNoTH, m_iMaxSeqNo);
   m_pACKWindow = new CACKWindow(4096);
   m_pRcvTimeWindow = new CPktTimeWindow;

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
   if (!m_bOpened)
      throw CUDTException(5, 0, 0);

   if (m_bMulticast)
   { 
      // not supported now.
      throw CUDTException(5, 0, 0);
 
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

      sockaddr addr;

      timeval entertime, currtime;
      gettimeofday(&entertime, 0);

      while (true)
      {
         // detect time out
         gettimeofday(&currtime, 0);
         if ((timeo > 0) && ((currtime.tv_sec - entertime.tv_sec) * 1000000 + (currtime.tv_usec - entertime.tv_usec) > timeo))
            throw CUDTException(1, 5, 0);

         // Listening to the port...
         initpkt.setLength(sizeof(CHandShake));
         if (m_pChannel->recvfrom(initpkt, &addr) <= 0)
            continue;

         // When a peer side connects in...
         if ((1 == initpkt.getFlag()) && (0 == initpkt.getType()))
         {
            // Uses the smaller MTU between the peers        
            if (initdata.m_iMTU > m_iMTU)
               initdata.m_iMTU = m_iMTU;
            else
               m_iMTU = initdata.m_iMTU;

            // exchange info for maximum flow window size
            m_iMaxFlowWindowSize = initdata.m_iFlightFlagSize;
            initdata.m_iFlightFlagSize = m_iFlightFlagSize;

            m_iRcvLastAck = initdata.m_iISN;
            m_iRcvLastAckAck = initdata.m_iISN;
            m_iRcvCurrSeqNo = initdata.m_iISN - 1;
            m_iNextExpect = initdata.m_iISN;

            m_pChannel->connect(&addr);

            // Random Initial Sequence Number
            srand(currtime.tv_sec);
            m_iISN = initdata.m_iISN = (__int32)(double(rand()) / RAND_MAX * m_iMaxSeqNo);

            m_iSndLastAck = initdata.m_iISN;
            m_iSndCurrSeqNo = initdata.m_iISN - 1;

            // Send back the negotiated configurations.
            *m_pChannel << initpkt;
  
            break;
         }
      }

      m_iPktSize = m_iMTU - 28;
      m_iPayloadSize = m_iPktSize - 4;
   }

   // the size of sender's loss list cannot be known until now
   m_pSndLossList = new CSndLossList(m_iMaxFlowWindowSize, m_iSeqNoTH, m_iMaxSeqNo);

   // UDT is now running...
   #ifndef WIN32
      pthread_create(&m_SndThread, NULL, CUDT::sndHandler, this);
      pthread_create(&m_RcvThread, NULL, CUDT::rcvHandler, this);
   #else
      m_SndThread = CreateThread(NULL, 0, CUDT::sndHandler, this, 0, NULL);
      m_RcvThread = CreateThread(NULL, 0, CUDT::rcvHandler, this, 0, NULL);
   #endif

   // And of course, it is connected.
   m_bConnected = true;
}

void CUDT::connect(const char* ip, const __int32& port, const __int64& timeo)
{
   if (!m_bOpened)
      throw CUDTException(5, 0, 0);

   if (m_bMulticast)
   {
      // not supported now.
      throw CUDTException(5, 0, 0);

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
      char* initdata = new char [m_iMTU];
      CHandShake* hs = (CHandShake *)initdata;

      // This is my current configurations.
      hs->m_iVersion = m_iVersion;
      hs->m_iMTU = m_iMTU;
      hs->m_iFlightFlagSize = m_iFlightFlagSize;

      // Random Initial Sequence Number
      timeval currtime;
      gettimeofday(&currtime, 0);
      srand(currtime.tv_sec);
      m_iISN = hs->m_iISN = (__int32)(double(rand()) / RAND_MAX * m_iMaxSeqNo);

      m_iSndLastAck = hs->m_iISN;
      m_iSndCurrSeqNo = hs->m_iISN - 1;

      initpkt.pack(0, initdata);
 
      // Inform the initiator my configurations.
      *m_pChannel << initpkt;

      // Wait for the negotiated configurations from the peer side.
      initpkt.setLength(m_iMTU);
      *m_pChannel >> initpkt;

      timeval entertime;
      gettimeofday(&entertime, 0);

      while ((initpkt.getLength() < 0) || (1 != initpkt.getFlag()) || (0 != initpkt.getType()))
      {
         initpkt.setLength(sizeof(CHandShake));
         *m_pChannel << initpkt;

         initpkt.setLength(m_iMTU);
         *m_pChannel >> initpkt;

         gettimeofday(&currtime, 0);
         if ((timeo > 0) && ((currtime.tv_sec - entertime.tv_sec) * 1000000 + (currtime.tv_usec - entertime.tv_usec) > timeo))
            throw CUDTException(1, 5, 0);
      }

      // Got it. Re-configure according to the negotiated values.
      m_iMTU = hs->m_iMTU;
      m_iMaxFlowWindowSize = hs->m_iFlightFlagSize;
      m_iPktSize = m_iMTU - 28;
      m_iPayloadSize = m_iPktSize - 4;

      m_iRcvLastAck = hs->m_iISN;
      m_iRcvLastAckAck = hs->m_iISN;
      m_iRcvCurrSeqNo = hs->m_iISN - 1;
      m_iNextExpect = hs->m_iISN;

      delete [] initdata;
   }

   // the size of sender's loss list cannot be known until now
   m_pSndLossList = new CSndLossList(m_iMaxFlowWindowSize, m_iSeqNoTH, m_iMaxSeqNo);

   // Now I am also running, a little while after the Initiator was running.
   #ifndef WIN32
      pthread_create(&m_SndThread, NULL, CUDT::sndHandler, this);
      pthread_create(&m_RcvThread, NULL, CUDT::rcvHandler, this);
   #else
      m_SndThread = CreateThread(NULL, 0, CUDT::sndHandler, this, 0, NULL);
      m_RcvThread = CreateThread(NULL, 0, CUDT::rcvHandler, this, 0, NULL);
   #endif

   // And, I am connected too.
   m_bConnected = true;
}

void CUDT::close(const CL_STATUS& wait)
{
   CGuard cg(m_ShutdownLock);

   if (!m_bOpened)
      return;

   switch (wait)
   {
   case WAIT_NONE:
      break;

   case WAIT_SEND:
      if (NULL == m_pSndBuffer)
         break;

      while ((m_pSndBuffer->getCurrBufSize() > 0) && (m_bConnected))
         #ifndef WIN32
            usleep(10);
         #else
            Sleep(1);
         #endif

      // if the connection broken, an exception can be throwed.

      break;

   case WAIT_RECV:
      break;

   case WAIT_ALL:
      if (NULL == m_pSndBuffer)
         break;

      while ((m_pSndBuffer->getCurrBufSize() > 0) && (m_bConnected))
         #ifndef WIN32
            usleep(10);
         #else
            Sleep(1);
         #endif

      break;

   default:
      break;
   }

   // inform the peer side with a "shutdown" packet
   if (!m_bShutdown)
      sendCtrl(5);

   // Inform the threads handler to stop.
   m_bClosing = true;
   // Not connected any more.
   m_bConnected = false;

   // Signal the sender and recver if they are waiting for data.
   #ifndef WIN32
      pthread_cond_signal(&m_SendDataCond);
      pthread_mutex_lock(&m_SendLock);
      pthread_mutex_unlock(&m_SendLock);
      pthread_cond_signal(&m_RecvDataCond);
      pthread_mutex_lock(&m_RecvLock);
      pthread_mutex_unlock(&m_RecvLock);
   #else
      SetEvent(m_SendDataCond);
      WaitForSingleObject(m_SendLock, INFINITE);
      ReleaseMutex(m_SendLock);
      SetEvent(m_RecvDataCond);
      WaitForSingleObject(m_RecvLock, INFINITE);
      ReleaseMutex(m_RecvLock);
   #endif

   // Wait for the threads to exit.
   m_pTimer->interrupt();
   #ifndef WIN32
      pthread_join(m_SndThread, NULL);
      pthread_join(m_RcvThread, NULL);
   #else
      WaitForSingleObject(m_SndThread, INFINITE);
      WaitForSingleObject(m_RcvThread, INFINITE);
   #endif

   // Channel is to be destroyed.
   if (m_pChannel)
   {
      m_pChannel->disconnect();
      delete m_pChannel;
      m_pChannel = NULL;
   }

   // And structures released.
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
   if (m_pRcvTimeWindow)
      delete m_pRcvTimeWindow;

   m_pSndBuffer = NULL;
   m_pRcvBuffer = NULL;
   m_pSndLossList = NULL;
   m_pRcvLossList = NULL;
   m_pTimer = NULL;
   m_pIrrPktList = NULL;
   m_pACKWindow = NULL;
   m_pRcvTimeWindow = NULL;

   if (m_pcIP)
   {
      delete [] m_pcIP;
      m_pcIP = NULL;
   }

   #ifdef TRACE
      if ((m_bTraceEnabled) && (stdout != m_TraceOutput))
         fclose(m_TraceOutput);
   #endif

   // CLOSED.
   m_bOpened = false;
}

#ifndef WIN32
void* CUDT::sndHandler(void* sender)
#else
DWORD WINAPI CUDT::sndHandler(LPVOID sender)
#endif
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
               #ifndef WIN32
                  pthread_mutex_lock(&(self->m_SendDataLock));
                  while ((0 == self->m_pSndBuffer->getCurrBufSize()) && (!self->m_bClosing))
                     pthread_cond_wait(&(self->m_SendDataCond), &(self->m_SendDataLock));
                  pthread_mutex_unlock(&(self->m_SendDataLock));
               #else
                  WaitForSingleObject(self->m_SendDataLock, INFINITE);
                  while ((0 == self->m_pSndBuffer->getCurrBufSize()) && (!self->m_bClosing))
                  {
                     ReleaseMutex(self->m_SendDataLock);
                     WaitForSingleObject(self->m_SendDataCond, INFINITE);
                     WaitForSingleObject(self->m_SendDataLock, INFINITE);
                  }
                  ReleaseMutex(self->m_SendDataLock);
               #endif
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

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

#ifndef WIN32
void* CUDT::rcvHandler(void* recver)
#else
DWORD WINAPI CUDT::rcvHandler(LPVOID recver)
#endif
{
   CUDT* self = static_cast<CUDT *>(recver);

   CPacket packet;
   char* payload = new char [self->m_iPayloadSize];
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
   unsigned __int64 ullexpint = 2 * self->m_iRTT + ullsynint;

   // Set up the timers.
   self->m_pTimer->rdtsc(nextsyntime);
   nextsyntime += ullsynint;
   self->m_pTimer->rdtsc(nextacktime);
   nextacktime += ullackint;
   self->m_pTimer->rdtsc(nextnaktime);
   nextnaktime += ullnakint;
   self->m_pTimer->rdtsc(nextexptime);
   nextexptime += ullexpint;

   #ifdef TRACE
      unsigned __int64 nexttracetime;
      self->m_pTimer->rdtsc(nexttracetime);
      nexttracetime += self->m_iTraceInterval * self->m_ullCPUFrequency;
   #endif
   

   while (!self->m_bClosing)
   {
      // "recv"/"recvfile" is called, blocking mode is activated, and not enough received data in the protocol buffer
      if (self->m_bReadBuf)
      {
         // Check if there is enough data now.
         #ifndef WIN32
            pthread_mutex_lock(&(self->m_RecvDataLock));
            self->m_bReadBuf = self->m_pRcvBuffer->readBuffer(const_cast<char*>(self->m_pcTempData), const_cast<__int32&>(self->m_iTempLen));
            pthread_mutex_unlock(&(self->m_RecvDataLock));
         #else
            WaitForSingleObject(self->m_RecvDataLock, INFINITE);
            self->m_bReadBuf = self->m_pRcvBuffer->readBuffer(const_cast<char*>(self->m_pcTempData), const_cast<__int32&>(self->m_iTempLen));
            ReleaseMutex(self->m_RecvDataLock);
         #endif

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
            #ifndef WIN32
               pthread_cond_signal(&(self->m_RecvDataCond));
            #else
               SetEvent(self->m_RecvDataCond);
            #endif
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
         // Periodical rate control.
         if (self->m_iLocalSend > 0)
            self->rateControl();

         self->m_pTimer->rdtsc(currtime);
         nextsyntime = currtime + ullsynint;

         #ifdef TRACE
            if ((self->m_bTraceEnabled) && (currtime > nexttracetime))
            {
               self->sample();
               nexttracetime = currtime + self->m_iTraceInterval * self->m_ullCPUFrequency;
            }
         #endif
      }
      if ((currtime > nextexptime) && (0 == self->m_pSndLossList->getLossLength()))
      {
         // Haven't receive any information from the peer, it is dead?!
         if (self->m_iEXPCount > 16)
         {
            //
            // Connection is broken. 
            // UDT does not signal any information about this instead of to stop quietly.
            // Apllication will detect this when it calls any UDT methods next time.
            //
            self->m_bClosing = true;
            self->m_bConnected = false;

            #ifndef WIN32
               pthread_cond_signal(&(self->m_SendBlockCond));
               pthread_cond_signal(&(self->m_RecvDataCond));
            #else
               SetEvent(self->m_SendBlockCond);
               SetEvent(self->m_RecvDataCond);
            #endif

            continue;
         }

         // sender: Insert all the packets sent after last received acknowledgement into the sender loss list.
         if (((self->m_iSndCurrSeqNo + 1) % self->m_iMaxSeqNo) != self->m_iSndLastAck)
            self->m_pSndLossList->insert(const_cast<__int32&>(self->m_iSndLastAck), self->m_iSndCurrSeqNo);
         // receiver only: send out a keep-alive packet
         else
            self->sendCtrl(1);

         ++ self->m_iEXPCount;

         ullexpint = ((self->m_iEXPCount + 1) * self->m_iRTT + self->m_iSYNInterval) * self->m_ullCPUFrequency;

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
      if (!(self->m_pRcvBuffer->nextDataPos(&(packet.m_pcData), offset * self->m_iPayloadSize - self->m_pIrrPktList->currErrorSize(self->m_iNextExpect), self->m_iPayloadSize)))
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
         self->processCtrl(packet);

         if ((2 == packet.getType()) || (6 == packet.getType()))
         {
            ullnakint = self->m_iRTT * self->m_ullCPUFrequency;
            //do not resent the loss report within too short period
            if (ullnakint < ullsynint)
               ullnakint = ullsynint;

            ullexpint = ((self->m_iEXPCount + 1) * self->m_iRTT + self->m_iSYNInterval) * self->m_ullCPUFrequency;
         }

         self->m_pTimer->rdtsc(currtime);
         if ((2 <= packet.getType()) && (4 >= packet.getType()))
            nextexptime = currtime + ullexpint;

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

      #ifdef TRACE
         self->m_iTraceRecv ++;
      #endif

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

   delete [] payload;

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

void CUDT::sendCtrl(const __int32& pkttype, void* lparam, void* rparam)
{
   CPacket ctrlpkt;

   __int32 losslen[2];
   __int32 ack;
   __int32* data = new __int32 [m_iPayloadSize];

   unsigned __int64 currtime;

   switch (pkttype)
   {
   case 2: //010 - Acknowledgement
      // If there is no loss, the ACK is the current largest sequence number plus 1;
      // Otherwise it is the smallest sequence number in the receiver loss list.
      if (0 == m_pRcvLossList->getLossLength())
         ack = (m_iRcvCurrSeqNo + 1) % m_iMaxSeqNo;
      else
         ack = m_pRcvLossList->getFirstLostSeq();

      m_pTimer->rdtsc(currtime);

      // There is new received packet to acknowledge, update related information.
      if (((ack > m_iRcvLastAck) && (ack - m_iRcvLastAck < m_iSeqNoTH)) || (ack < m_iRcvLastAck - m_iSeqNoTH))
      {
         int acksize = (ack - m_iRcvLastAck + m_iMaxSeqNo) % m_iMaxSeqNo;
         m_iRcvLastAck = ack;

         if (m_pRcvBuffer->ackData(acksize * m_iPayloadSize - m_pIrrPktList->currErrorSize(m_iRcvLastAck)))
         {
            #ifndef WIN32
               pthread_cond_signal(&m_RecvDataCond);
            #else
               SetEvent(m_RecvDataCond);
            #endif
            m_iUserBufBorder = m_iRcvLastAck + (__int32)ceil(double(m_iUDTBufSize) / m_iPayloadSize);
         }

         m_pIrrPktList->deleteIrregularPkt(m_iRcvLastAck);
      }
      else if (ack == m_iRcvLastAck)
      {
         if ((__int32)(currtime - m_ullLastAckTime) < (2 * m_iRTT))
            break;
      }
      else
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

         #ifdef TRACE
            m_iSentACK ++;
         #endif
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

      #ifdef TRACE
         m_iSentNAK ++;
         m_iTraceLoss += losslen[0];
      #endif

      break;

   case 4: //100 - Congestion Warning
      //
      // The "data" in the parameter list is meaningless.
      // It is just for the convinience of the implementation.
      //
      ctrlpkt.pack(4, data);

      *m_pChannel << ctrlpkt;

      m_pTimer->rdtsc(m_ullLastWarningTime);

      break;

   case 1: //001 - Keep-alive
      //
      // The "data" in the parameter list is meaningless.
      // It is just for the convinience of the implementation.
      //
      ctrlpkt.pack(1, data);
      *m_pChannel << ctrlpkt;

      break;

   case 0: //000 - Handshake
      ctrlpkt.pack(0, lparam);
      *m_pChannel << ctrlpkt;

      break;

   case 5: //101 - Shutdown
      //
      // The "data" in the parameter list is meaningless.
      // It is just for the convinience of the implementation.
      //
      ctrlpkt.pack(5, data);
      *m_pChannel << ctrlpkt;

      break;

   case 7: //111 - Resevered for future use
      break;

   default:
      break;
   }

   delete [] data;
}

void CUDT::processCtrl(CPacket& ctrlpkt)
{
   __int32 ack;
   __int32* losslist;
   __int32 rtt = -1;
   //unsigned __int64 currtime;

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
      #ifndef WIN32
         pthread_mutex_lock(&m_AckLock);
      #else
         WaitForSingleObject(m_AckLock, INFINITE);
      #endif

      // acknowledge the sending buffer
      if ((ack > m_iSndLastAck) && (ack - m_iSndLastAck < m_iSeqNoTH))
         m_pSndBuffer->ackData((ack - m_iSndLastAck) * m_iPayloadSize, m_iPayloadSize);
      else if (ack < m_iSndLastAck - m_iSeqNoTH)
         m_pSndBuffer->ackData((ack - m_iSndLastAck + m_iMaxSeqNo) * m_iPayloadSize, m_iPayloadSize);
      else
      {
         // discard it if it is a repeated ACK
         #ifndef WIN32
            pthread_mutex_unlock(&m_AckLock);
         #else
            ReleaseMutex(m_AckLock);
         #endif

         break;
      }

      // update sending variables
      m_iSndLastAck = ack;
      m_pSndLossList->remove((m_iSndLastAck - 1 + m_iMaxSeqNo) % m_iMaxSeqNo);

      #ifndef WIN32
         pthread_mutex_unlock(&m_AckLock);
      #else
         ReleaseMutex(m_AckLock);
      #endif

      // signal a waiting "send" call if all the data has been sent
      #ifndef WIN32
         pthread_mutex_lock(&m_SendBlockLock);
         if ((m_bSynSending) && (0 == m_pSndBuffer->getCurrBufSize()))
            pthread_cond_signal(&m_SendBlockCond);
         pthread_mutex_unlock(&m_SendBlockLock);
      #else
         if ((m_bSynSending) && (0 == m_pSndBuffer->getCurrBufSize()))
            SetEvent(m_SendBlockCond);
      #endif

      // Update RTT
      if (m_iRTT == 10 * m_iSYNInterval)
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

      #ifdef TRACE
         m_iRecvACK ++;
      #endif

      break;

   case 6: //110 - Acknowledgement of Acknowledgement
      // update RTT
      rtt = m_pACKWindow->acknowledge(ctrlpkt.getAckSeqNo(), ack);

      if (rtt <= 0)
         break;

      //
      // Well, I decide to temporaly disable the use of delay.
      // a good idea but the algorithm to detect it is not good enough.
      // I'll come back later...
      //

      //m_pRcvTimeWindow->ack2Arrival(rtt);

      // check packet delay trend
      //m_pTimer->rdtsc(currtime);
      //if (m_pRcvTimeWindow->getDelayTrend() && (currtime - m_ullLastWarningTime > m_iRTT * m_ullCPUFrequency * 2))
      //   sendCtrl(4);

      // RTT EWMA
      if (m_iRTT == 10 * m_iSYNInterval)
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
         if ((losslist[i] & 0x80000000) && (((losslist[i] & 0x7FFFFFFF) >= m_iSndLastAck) || ((losslist[i] & 0x7FFFFFFF) < m_iSndLastAck - m_iSeqNoTH)))
         {
            m_iLocalLoss += m_pSndLossList->insert(losslist[i] & 0x7FFFFFFF, losslist[i + 1]);
            i ++;
         }
         else
            if ((losslist[i] >= m_iSndLastAck) || (losslist[i] < m_iSndLastAck - m_iSeqNoTH))
               m_iLocalLoss += m_pSndLossList->insert(losslist[i], losslist[i]);

      #ifdef TRACE
         m_iRecvNAK ++;
      #endif

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
      if ((m_bInitiator) && (-1 == m_iRcvCurrSeqNo) && (0 == m_iSndLastAck))
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

   case 5: //101 - Shutdown
      m_bShutdown = true;

      m_bClosing = true;
      m_bConnected = false;

      // Signal the sender and recver if they are waiting for data.
      #ifndef WIN32
         pthread_cond_signal(&m_SendDataCond);
         pthread_mutex_lock(&m_SendLock);
         pthread_mutex_unlock(&m_SendLock);
         pthread_cond_signal(&m_RecvDataCond);
         pthread_mutex_lock(&m_RecvLock);
         pthread_mutex_unlock(&m_RecvLock);
      #else
         SetEvent(m_SendDataCond);
         WaitForSingleObject(m_SendLock, INFINITE);
         ReleaseMutex(m_SendLock);
         SetEvent(m_RecvDataCond);
         WaitForSingleObject(m_RecvLock, INFINITE);
         ReleaseMutex(m_RecvLock);
      #endif

      m_pChannel->disconnect();

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

   #ifdef TRACE
      m_iTraceSend += m_iLocalSend;
      m_iTraceLoss += m_iLocalLoss;
   #endif

   m_iLocalSend = 0;
   m_iLocalLoss = 0;

   m_dLossRate = m_dLossRate * m_dWeight + currlossrate * (1 - m_dWeight);

   if (m_dLossRate > m_dLossRateLimit)
      return;

   // During Slow Start, no rate increase
   if (m_bSlowStart)
      return;

   double inc;

   if (1000000.0 / m_ullInterval * m_ullCPUFrequency >= m_iBandwidth)
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
      m_iFlowWindowSize = (m_iSndLastAck > m_iISN) ? (m_iSndLastAck - m_iISN) : (m_iSndLastAck - m_iISN + m_iMaxSeqNo);

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

   if (m_iFlowWindowSize > m_iMaxFlowWindowSize)
   {
      m_iFlowWindowSize = m_iMaxFlowWindowSize;
      m_bSlowStart = false;
   }
}

__int32 CUDT::send(char* data, const __int32& len)
{
   CGuard sendguard(m_SendLock);

   // throw an exception if not connected
   if (!m_bConnected)
      throw CUDTException(2, 0, 0);

   if (len <= 0)
      return 0;

   // insert the user buffer into the sening list
   #ifndef WIN32
      pthread_mutex_lock(&m_SendDataLock);
      m_pSndBuffer->addBuffer(data, len, m_iMemFlag);
      pthread_mutex_unlock(&m_SendDataLock);
   #else
      WaitForSingleObject(m_SendDataLock, INFINITE);
      m_pSndBuffer->addBuffer(data, len, m_iMemFlag);
      ReleaseMutex(m_SendDataLock);
   #endif

   // release the data automatically when it is sent if memflag is 1
   //if (1 == m_iMemFlag)
   //   data = NULL;

   // signal the sending thread in case that it is waiting
   #ifndef WIN32
      pthread_cond_signal(&m_SendDataCond);
   #else
      SetEvent(m_SendDataCond);
   #endif

   // wait here during a blocking sending
   #ifndef WIN32
      pthread_mutex_lock(&m_SendBlockLock);
      while ((m_bConnected) && (m_bSynSending) && (0 != m_pSndBuffer->getCurrBufSize()))
         pthread_cond_wait(&m_SendBlockCond, &m_SendBlockLock);
      pthread_mutex_unlock(&m_SendBlockLock);
   #else
      while ((m_bConnected) && (m_bSynSending) && (0 != m_pSndBuffer->getCurrBufSize()))
         WaitForSingleObject(m_SendBlockCond, INFINITE);
   #endif

   // check if the sending is successful or the connection is broken
   if (!m_bConnected)
      if (m_bSynSending)
         return m_pSndBuffer->getCurrBufSize();
      else
         return 0;

   return len;
}

__int32 CUDT::recv(char* data, const __int32& len)
{
   CGuard recvguard(m_RecvLock);

   // throw an exception if not connected
   if (!m_bConnected)
      throw CUDTException(2, 0, 0);

   if (len <= 0)
      return 0;

   // try to read data from the protocol buffer
   if (m_pRcvBuffer->readBuffer(data, len))
      return len;

   // return in non-blocking mode
   if (!m_bSynRecving)
      return 0;

   // otherwise register the user buffer and wait here
   #ifndef WIN32
      pthread_mutex_lock(&m_RecvDataLock);
   #else
      WaitForSingleObject(m_RecvDataLock, INFINITE);
   #endif

   m_pcTempData = data;
   m_iTempLen = len;
   m_bReadBuf = true;

   #ifndef WIN32
      pthread_cond_wait(&m_RecvDataCond, &m_RecvDataLock);
      pthread_mutex_unlock(&m_RecvDataLock);
   #else
      ReleaseMutex(m_RecvDataLock);
      WaitForSingleObject(m_RecvDataCond, INFINITE);
   #endif

   // check if the receiving is successful or the connection is broken
   if (!m_bConnected)
      return m_pRcvBuffer->getCurUserBufSize();

   return len;
}

__int64 CUDT::sendfile(ifstream& ifs, const __int64& offset, const __int64& size)
{
   CGuard sendguard(m_SendLock);

   if (!m_bConnected)
      throw CUDTException(2, 0, 0);

   if (size <= 0)
      return 0;

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
         ifs.read(tempbuf, unitsize);

         #ifndef WIN32
            pthread_mutex_lock(&m_SendDataLock);
            while (getCurrSndBufSize() >= 44040000)
               usleep(10);
            m_pSndBuffer->addBuffer(tempbuf, unitsize);
            pthread_mutex_unlock(&m_SendDataLock);
            pthread_cond_signal(&m_SendDataCond);
         #else
            WaitForSingleObject(m_SendDataLock, INFINITE);
            while (getCurrSndBufSize() >= 44040000)
               Sleep(1);
            m_pSndBuffer->addBuffer(tempbuf, unitsize);
            ReleaseMutex(m_SendDataLock);
            SetEvent(m_SendDataCond);
         #endif
      }
      catch (CUDTException e)
      {
         if (!m_bConnected)
            return unitsize * count - m_pSndBuffer->getCurrBufSize();

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
         #ifndef WIN32
            pthread_mutex_lock(&m_SendDataLock);
            while (getCurrSndBufSize() >= 44040000)
               usleep(10);
            m_pSndBuffer->addBuffer(tempbuf, (__int32)(size - unitsize * (count - 1)));
            pthread_mutex_unlock(&m_SendDataLock);
            pthread_cond_signal(&m_SendDataCond);
         #else
            WaitForSingleObject(m_SendDataLock, INFINITE);
            while (getCurrSndBufSize() >= 44040000)
               Sleep(1);
            m_pSndBuffer->addBuffer(tempbuf, (__int32)(size - unitsize * (count - 1)));
            ReleaseMutex(m_SendDataLock);
            SetEvent(m_SendDataCond);
         #endif
      }
      catch (CUDTException e)
      {
         if (!m_bConnected)
            return size - m_pSndBuffer->getCurrBufSize();

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
         #ifndef WIN32
            usleep(10);
         #else
            Sleep(1);
         #endif
   }
   catch (CUDTException e)
   {
      if (!m_bConnected)
         return size - m_pSndBuffer->getCurrBufSize();

      throw e;
   }

   return size;
}

__int64 CUDT::recvfile(ofstream& ofs, const __int64& offset, const __int64& size)
{
   if (!m_bConnected)
      throw CUDTException(2);

   if (size <= 0)
      return 0;

   __int32 unitsize = 7340000;
   __int64 count = 1;
   char* tempbuf = new char[unitsize];
   __int32 recvsize;

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
         recvsize = recv(tempbuf, unitsize);
         ofs.write(tempbuf, recvsize);

         if (recvsize < unitsize)
         {
            m_bSynRecving = syn;
            return unitsize * (count - 1) + recvsize;
         }
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
         recvsize = recv(tempbuf, (__int32)(size - unitsize * (count - 1)));
         ofs.write(tempbuf, recvsize);

         if (recvsize < (__int32)(size - unitsize * (count - 1)))
         {
            m_bSynRecving = syn;
            return unitsize * (count - 1) + recvsize;
         }
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

   return size;
}

__int32 CUDT::getCurrSndBufSize()
{
   if (!m_bConnected)
      throw CUDTException(2, 0, 0);

   return m_pSndBuffer->getCurrBufSize();
}

#ifdef TRACE
void CUDT::trace(const __int32& interval, const char* tracefile)
{
   if (m_bTraceEnabled)
      return;

   m_bTraceEnabled = true;
   m_iTraceInterval = interval;
   gettimeofday(&m_LastSampleTime, 0);

   try
   {
      if (NULL == tracefile)
         m_TraceOutput = stdout;
      else
         m_TraceOutput = fopen(tracefile, "w");
   }
   catch(...)
   {
      throw CUDTException(4);
   }

   if (m_iTraceInterval < 10 * m_iSYNInterval)
   {
      fprintf(m_TraceOutput, "TRACE INTERVAL TOO SMALL. TRACE REQUEST REJECTED.\n");
      m_bTraceEnabled = false;
      return;
   }

   fprintf(m_TraceOutput,"SendRate(Mbps) RecvRate(Mbps) FlowWindowSize(pkts) InterPktTime(us) RTT(ms) sentACK recvACK sentNAK recvNAK LossRate(%%)\n");
}

void CUDT::sample()
{
   timeval currtime;
   gettimeofday(&currtime, 0);

   double interval = (currtime.tv_sec - m_LastSampleTime.tv_sec) * 1000000.0 + currtime.tv_usec - m_LastSampleTime.tv_usec;

   fprintf(m_TraceOutput, "\t%f", m_iTraceSend * m_iPayloadSize * 8.0 / interval);
   fprintf(m_TraceOutput, "\t%f", m_iTraceRecv * m_iPayloadSize * 8.0 / interval);
   fprintf(m_TraceOutput, "\t%d", m_iFlowWindowSize);
   fprintf(m_TraceOutput, "\t%f", m_ullInterval / double(m_ullCPUFrequency));
   fprintf(m_TraceOutput, "\t%f", m_iRTT/1000.0);
   fprintf(m_TraceOutput, "\t%d", m_iSentACK);
   fprintf(m_TraceOutput, "\t%d", m_iRecvACK);
   fprintf(m_TraceOutput, "\t%d", m_iSentNAK);
   fprintf(m_TraceOutput, "\t%d", m_iRecvNAK);
   fprintf(m_TraceOutput, "\t%f%%", (m_iTraceSend > 0 ? m_iTraceLoss * 100.0 / m_iTraceSend : 0.0));
   fprintf(m_TraceOutput, "\n");

   m_iTraceSend = m_iTraceRecv = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = m_iTraceLoss = 0;

   m_LastSampleTime = currtime;
}
#endif
