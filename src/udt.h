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
This is the (only) header file of the UDT library and for programming with UDT.

System specific types are defined here.

Data structures list:

CUDTUnited:     UDT global management
CHandShake:     Handshake Information
CPerfMon:       Performance data structure
CTimer:         Timer Facility
CUDTException:  Exception Handling Facility
CACKWindow:     ACK History Window
CPktTimeWindow: Packet time/delay history window
CPacket:        Packet Definition
CChannel:       UDP Transport Channel
CList:          Loss Lists and Irregular Packet List
                CSndLossList
                CRcvLossList
                CIrregularPktList
CSndBuffer:     Sending Buffer Management
CRcvBuffer:     Receiving Buffer Management
CUDTSocket:     UDT socket structure
CUDTUnited:     UDT global management
CUDT:           UDT
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [ygu@cs.uic.edu], last updated 02/03/2006

modified by
   <programmer's name, programmer's email, last updated mm/dd/yyyy>
   <descrition of changes>
*****************************************************************************/


#ifndef _UDT_H_
#define _UDT_H_

#ifdef CUSTOM_CC
#undef NO_BUSY_WAITING
#endif

#ifndef LINUX
#undef CAPI
#endif

#ifndef WIN32
   #include <pthread.h>
   #include <sys/time.h>
   #include <sys/uio.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
#else
   #include <windows.h>
#endif

#include <fstream> 
#include <set>
#include <map>
using namespace std;


#ifndef WIN32
   // Explicitly define 32-bit and 64-bit numbers
   #define __int32 int
   #define __int64 long long

   #define UDT_API 
#else
   // Windows compability
   typedef HANDLE pthread_t;
   typedef HANDLE pthread_mutex_t;
   typedef HANDLE pthread_cond_t;
   typedef DWORD pthread_key_t;

   #ifdef UDT_EXPORTS
      #define UDT_API __declspec(dllexport)
   #else
      #define UDT_API __declspec(dllimport)
   #endif

   struct iovec
   {
      __int32 iov_len;
      char* iov_base;
   };

   int gettimeofday(timeval *tv, void*);
   int readv(SOCKET s, const iovec* vector, int count);
   int writev(SOCKET s, const iovec* vector, int count);
#endif

typedef void (*UDT_MEM_ROUTINE)(char*, int);
typedef int UDTSOCKET;

typedef set<UDTSOCKET> ud_set;
#define UD_CLR(u, uset) ((uset)->erase(u))
#define UD_ISSET(u, uset) ((uset)->find(u) != (uset)->end())
#define UD_SET(u, uset) ((uset)->insert(u))
#define UD_ZERO(uset) ((uset)->clear())


////////////////////////////////////////////////////////////////////////////////
enum UDTOpt
{
   UDT_MSS,		// the Maximum Transfer Unit
   UDT_SNDSYN,		// if sending is blocking
   UDT_RCVSYN,		// if receiving is blocking
   UDT_CC,		// custom congestion control algorithm 
   UDT_FC,		// maximum allowed number of unacknowledged packets (Flow Control)
   UDT_SNDBUF,		// maximum buffer in sending queue
   UDT_RCVBUF,		// UDT receiving buffer size
   UDT_LINGER,		// waiting for unsent data when closing
   UDP_SNDBUF,		// UDP sending buffer size
   UDP_RCVBUF,		// UDP receiving buffer size
   UDT_MAXMSG,		// maximum datagram message size
   UDT_MSGTTL,		// time-to-live of a datagram message
   UDT_RENDEZVOUS,	// rendezvous connection mode
   UDT_SNDTIMEO,	// send() timeout
   UDT_RCVTIMEO		// recv() timeout
};


////////////////////////////////////////////////////////////////////////////////
struct CHandShake
{
   __int32 m_iVersion;          // UDT version
   __int32 m_iISN;              // random initial sequence number
   __int32 m_iMSS;              // maximum segment size
   __int32 m_iFlightFlagSize;   // flow control window size
   __int32 m_iReqType;		// connection request type: -1: response, 1: initial request, 0: rendezvous request
};


////////////////////////////////////////////////////////////////////////////////
struct UDT_API CPerfMon
{
   // global measurements
   __int64 msTimeStamp;			// time since the UDT entity is started, in milliseconds
   __int64 pktSentTotal;                // total number of sent data packets, including retransmissions
   __int64 pktRecvTotal;                // total number of received packets
   __int32 pktSndLossTotal;		// total number of lost packets (sender side)
   __int32 pktRcvLossTotal;		// total number of lost packets (receiver side)
   __int32 pktRetransTotal;		// total number of retransmitted packets
   __int32 pktSentACKTotal;             // total number of sent ACK packets
   __int32 pktRecvACKTotal;             // total number of received ACK packets
   __int32 pktSentNAKTotal;             // total number of sent NAK packets
   __int32 pktRecvNAKTotal;             // total number of received NAK packets

   // local measurements
   __int64 pktSent;                     // number of sent data packets, including retransmissions
   __int64 pktRecv;                     // number of received packets
   __int32 pktSndLoss;                  // number of lost packets (sender side)
   __int32 pktRcvLoss;			// number of lost packets (receiverer side)
   __int32 pktRetrans;                  // number of retransmitted packets
   __int32 pktSentACK;			// number of sent ACK packets
   __int32 pktRecvACK;			// number of received ACK packets
   __int32 pktSentNAK;			// number of sent NAK packets
   __int32 pktRecvNAK;			// number of received NAK packets
   double mbpsSendRate;                 // sending rate in Mbps
   double mbpsRecvRate;                 // receiving rate in Mbps

   // instant measurements
   double usPktSndPeriod;               // packet sending period, in microseconds
   __int32 pktFlowWindow;               // flow window size, in number of packets
   __int32 pktCongestionWindow;         // congestion window size, in number of packets
   __int32 pktFlightSize;               // number of packets on flight
   double msRTT;                        // RTT, in milliseconds
   double mbpsBandwidth;		// estimated bandwidth, in Mbps
   __int32 byteAvailSndBuf;		// available UDT sender buffer size
   __int32 byteAvailRcvBuf;		// available UDT receiver buffer size
};


////////////////////////////////////////////////////////////////////////////////
class CTimer
{
public:

      // Functionality:
      //    Sleep for "interval" CCs.
      // Parameters:
      //    0) [in] interval: CCs to sleep.
      // Returned value:
      //    None.

   void sleep(const unsigned __int64& interval);

      // Functionality:
      //    Seelp until CC "nexttime".
      // Parameters:
      //    0) [in] nexttime: next time the caller is waken up.
      // Returned value:
      //    None.

   void sleepto(const unsigned __int64& nexttime);

      // Functionality:
      //    Stop the sleep() or sleepto() methods.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void interrupt();

public:

      // Functionality:
      //    Read the CPU clock cycle into x.
      // Parameters:
      //    0) [out] x: to record cpu clock cycles.
      // Returned value:
      //    None.

   static void rdtsc(unsigned __int64 &x);

      // Functionality:
      //    return the CPU frequency.
      // Parameters:
      //    None.
      // Returned value:
      //    CPU frequency.

   static unsigned __int64 getCPUFrequency();

private:
   unsigned __int64 m_ullSchedTime;		// next schedulled time

private:
   static unsigned __int64 s_ullCPUFrequency;	// CPU frequency : clock cycles per microsecond
   static unsigned __int64 readCPUFrequency();
};


////////////////////////////////////////////////////////////////////////////////
class CGuard
{
public:
   CGuard(pthread_mutex_t& lock);
   ~CGuard();

private:
   pthread_mutex_t& m_Mutex;	// Alias name of the mutex to be protected
   __int32 m_iLocked;		// Locking status

   void operator = (const CGuard&) {}
};


////////////////////////////////////////////////////////////////////////////////
class CACKWindow
{
public:
   CACKWindow();
   CACKWindow(const __int32& size);
   ~CACKWindow();

      // Functionality:
      //    Write an ACK record into the window.
      // Parameters:
      //    0) [in] seq: ACK seq. no.
      //    1) [in] ack: DATA ACK no.
      // Returned value:
      //    None.

   void store(const __int32& seq, const __int32& ack);

      // Functionality:
      //    Search the ACK-2 "seq" in the window, find out the DATA "ack" and caluclate RTT .
      // Parameters:
      //    0) [in] seq: ACK-2 seq. no.
      //    1) [out] ack: the DATA ACK no. that matches the ACK-2 no.
      // Returned value:
      //    RTT.

   __int32 acknowledge(const __int32& seq, __int32& ack);

private:
   __int32* m_piACKSeqNo;	// Seq. No. for the ACK packet
   __int32* m_piACK;		// Data Seq. No. carried by the ACK packet
   timeval* m_pTimeStamp;	// The timestamp when the ACK was sent

   __int32 m_iSize;		// Size of the ACK history window
   __int32 m_iHead;		// Pointer to the lastest ACK record
   __int32 m_iTail;		// Pointer to the oldest ACK record
};


////////////////////////////////////////////////////////////////////////////////
class CPktTimeWindow
{
public:
   CPktTimeWindow();
   CPktTimeWindow(const __int32& s1, const __int32& s2, const __int32& s3);
   ~CPktTimeWindow();

      // Functionality:
      //    read the minimum packet sending interval.
      // Parameters:
      //    None.
      // Returned value:
      //    minimum packet sending interval (microseconds).

   __int32 getMinPktSndInt() const;

      // Functionality:
      //    Calculate the packes arrival speed.
      // Parameters:
      //    None.
      // Returned value:
      //    Packet arrival speed (packets per second).

   __int32 getPktRcvSpeed() const;

      // Functionality:
      //    Check if the rtt is increasing or not.
      // Parameters:
      //    None.
      // Returned value:
      //    true is RTT is increasing, otherwise false.

   bool getDelayTrend() const;

      // Functionality:
      //    Estimate the bandwidth.
      // Parameters:
      //    None.
      // Returned value:
      //    Estimated bandwidth (packets per second).

   __int32 getBandwidth() const;

      // Functionality:
      //    Record time information of a packet sending.
      // Parameters:
      //    0) currtime: time stamp of the packet sending.
      // Returned value:
      //    None.

   void onPktSent(const timeval& currtime);

      // Functionality:
      //    Record time information of an arrived packet.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void onPktArrival();

      // Functionality:
      //    Record the recent RTT.
      // Parameters:
      //    0) [in] rtt: the mose recent RTT from ACK-2.
      // Returned value:
      //    None.

   void ack2Arrival(const __int32& rtt);

      // Functionality:
      //    Record the arrival time of the first probing packet.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void probe1Arrival();

      // Functionality:
      //    Record the arrival time of the second probing packet and the interval between packet pairs.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void probe2Arrival();

private:
   __int32 m_iAWSize;		// size of the packet arrival history window
   __int32* m_piPktWindow;	// packet information window
   __int32 m_iPktWindowPtr;	// position pointer of the packet info. window.

   __int32 m_iRWSize;		// size of RTT history window size
   __int32* m_piRTTWindow;	// RTT history window
   __int32* m_piPCTWindow;	// PCT (pairwise comparison test) history window
   __int32* m_piPDTWindow;	// PDT (pairwise difference test) history window
   __int32 m_iRTTWindowPtr;	// position pointer to the 3 windows above

   __int32 m_iPWSize;		// size of probe history window size
   __int32* m_piProbeWindow;	// record inter-packet time for probing packet pairs
   __int32 m_iProbeWindowPtr;	// position pointer to the probing window

   timeval m_LastSentTime;	// last packet sending time
   __int32 m_iMinPktSndInt;	// Minimum packet sending interval

   timeval m_LastArrTime;	// last packet arrival time
   timeval m_CurrArrTime;	// current packet arrival time
   timeval m_ProbeTime;		// arrival time of the first probing packet
};


////////////////////////////////////////////////////////////////////////////////
class UDT_API CUDTException
{
public:
   CUDTException(__int32 major = 0, __int32 minor = 0, __int32 err = -1);
   CUDTException(const CUDTException& e);
   virtual ~CUDTException();

      // Functionality:
      //    Get the description of the exception.
      // Parameters:
      //    None.
      // Returned value:
      //    Text message for the exception description.

   virtual const char* getErrorMessage();

      // Functionality:
      //    Get the system errno for the exception.
      // Parameters:
      //    None.
      // Returned value:
      //    errno.

   virtual const __int32 getErrorCode() const;

private:
   __int32 m_iMajor;	// major exception categories

// 0: correct condition
// 1: network setup exception
// 2: network connection broken
// 3: memory exception
// 4: file exception
// 5: method not supported
// 6+: undefined error

   __int32 m_iMinor;	// for specific error reasons

   __int32 m_iErrno;	// errno returned by the system if there is any

   char m_pcMsg[1024];	// text error message
};


////////////////////////////////////////////////////////////////////////////////
// See packet.cpp for packet structure and coding
class CChannel;

class CPacket
{
friend class CChannel;

public:
   __int32& m_iSeqNo;		// alias: sequence number
   __int32& m_iTimeStamp;	// alias: timestamp
   char*& m_pcData;		// alias: data/control information

   const static __int32 m_iPktHdrSize = 8;

public:
   CPacket();
   ~CPacket();

      // Functionality:
      //    Get the payload or the control information field length.
      // Parameters:
      //    None.
      // Returned value:
      //    the payload or the control information field length.

   __int32 getLength() const;

      // Functionality:
      //    Set the payload or the control information field length.
      // Parameters:
      //    0) [in] len: the payload or the control information field length.
      // Returned value:
      //    None.

   void setLength(const __int32& len);

      // Functionality:
      //    Pack a Control packet.
      // Parameters:
      //    0) [in] pkttype: packet type filed.
      //    1) [in] lparam: pointer to the first data structure, explained by the packet type.
      //    2) [in] rparam: pointer to the second data structure, explained by the packet type.
      //    3) [in] size: size of rparam, in number of bytes;
      // Returned value:
      //    None.

   void pack(const __int32& pkttype, void* lparam = NULL, void* rparam = NULL, const __int32& size = 0);

      // Functionality:
      //    Read the packet vector.
      // Parameters:
      //    None.
      // Returned value:
      //    Pointer to the packet vector.

   iovec* getPacketVector();

      // Functionality:
      //    Read the packet flag.
      // Parameters:
      //    None.
      // Returned value:
      //    packet flag (0 or 1).

   __int32 getFlag() const;

      // Functionality:
      //    Read the packet type.
      // Parameters:
      //    None.
      // Returned value:
      //    packet type filed (000 ~ 111).

   __int32 getType() const;

      // Functionality:
      //    Read the extended packet type.
      // Parameters:
      //    None.
      // Returned value:
      //    extended packet type filed (0x000 ~ 0xFFF).

   __int32 getExtendedType() const;

      // Functionality:
      //    Read the ACK-2 seq. no.
      // Parameters:
      //    None.
      // Returned value:
      //    packet header field (bit 16~31).

   __int32 getAckSeqNo() const;

private:
   unsigned __int32 m_nHeader[2];	// The 64-bit header field
   iovec m_PacketVector[2];		// The 2-demension vector of UDT packet [header, data]

   __int32 __pad;

   void operator = (const CPacket&) {}
};


////////////////////////////////////////////////////////////////////////////////
class CChannel
{
public:
   CChannel();
   CChannel(const __int32& version);
   ~CChannel();

      // Functionality:
      //    Opne a UDP channel.
      // Parameters:
      //    0) [in] addr: The local address that UDP will use.
      // Returned value:
      //    None.

   void open(const sockaddr* addr = NULL);

      // Functionality:
      //    Send data through the channel.
      // Parameters:
      //    0) [in] buffer: pointer to the data to be sent.
      //    1) [in] size: size of the buffer.
      // Returned value:
      //    Actual size of data sent.

   __int32 send(char* buffer, const __int32& size) const;

      // Functionality:
      //    Receive data from the channel.
      // Parameters:
      //    0) [out] buffer: received data.
      //    1) [in] size: size of the expected data received.
      // Returned value:
      //    Actual size of data received.

   __int32 recv(char* buffer, const __int32& size) const;

      // Functionality:
      //    Read the data from the channel but the data is not removed from UDP buffer.
      // Parameters:
      //    0) [out] buffer: previewed received data.
      //    1) [in] size: size of the expected data received.
      // Returned value:
      //    Actual size of data received.

   __int32 peek(char* buffer, const __int32& size) const;

      // Functionality:
      //    Send a packet through the channel.
      // Parameters:
      //    0) [in] packet: reference to a CPacket entity.
      // Returned value:
      //    The channel itself for serial output.

   const CChannel& operator<<(CPacket& packet) const;

      // Functionality:
      //    Receive a packet from the channel.
      // Parameters:
      //    0) [out] packet: reference to a CPacket entity.
      // Returned value:
      //    The channel itself for serial input.

   const CChannel& operator>>(CPacket& packet) const;

      // Functionality:
      //    Send a packet to the given address.
      // Parameters:
      //    0) [in] packet: reference to a CPacket entity.
      //    1) [in] addr: pointer to the destination address.
      // Returned value:
      //    Actual size of data sent.

   __int32 sendto(CPacket& packet, const sockaddr* addr) const;

      // Functionality:
      //    Receive a packet from the channel and record the source address.
      // Parameters:
      //    0) [in] packet: reference to a CPacket entity.
      //    1) [in] addr: pointer to the source address.
      // Returned value:
      //    Actual size of data received.

   __int32 recvfrom(CPacket& packet, sockaddr* addr) const;

      // Functionality:
      //    Connect to the peer side whose address is in the sockaddr structure.
      // Parameters:
      //    0) [in] addr: pointer to the peer side address.
      // Returned value:
      //    None.

   void connect(const sockaddr* addr);

      // Functionality:
      //    Disconnect and close the UDP entity.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void disconnect() const;

      // Functionality:
      //    Get the UDP sending buffer size.
      // Parameters:
      //    None.
      // Returned value:
      //    Current UDP sending buffer size.

   __int32 getSndBufSize();

      // Functionality:
      //    Get the UDP receiving buffer size.
      // Parameters:
      //    None.
      // Returned value:
      //    Current UDP receiving buffer size.

   __int32 getRcvBufSize();

      // Functionality:
      //    Set the UDP sending buffer size.
      // Parameters:
      //    0) [in] size: expected UDP sending buffer size.
      // Returned value:
      //    None.

   void setSndBufSize(const __int32& size);

      // Functionality:
      //    Set the UDP receiving buffer size.
      // Parameters:
      //    0) [in] size: expected UDP receiving buffer size.
      // Returned value:
      //    None.

   void setRcvBufSize(const __int32& size);

      // Functionality:
      //    Query the socket address that the channel is using.
      // Parameters:
      //    0) [out] addr: pointer to store the returned socket address.
      // Returned value:
      //    None.

   void getSockAddr(sockaddr* addr) const;

      // Functionality:
      //    Query the peer side socket address that the channel is connect to.
      // Parameters:
      //    0) [out] addr: pointer to store the returned socket address.
      // Returned value:
      //    None.

   void getPeerAddr(sockaddr* addr) const;


private:
   __int32 m_iIPversion;

   #ifndef WIN32
      __int32 m_iSocket;		// socket descriptor
   #else
      SOCKET m_iSocket;
   #endif

   __int32 m_iSndBufSize;		// UDP sending buffer size
   __int32 m_iRcvBufSize;		// UDP receiving buffer size

   char* m_pcChannelBuf;		// buffer for temporally storage of in/out data

private:
   void setChannelOpt();
};


////////////////////////////////////////////////////////////////////////////////
class CList
{
protected:
   const bool greaterthan(const __int32& seqno1, const __int32& seqno2) const;
   const bool lessthan(const __int32& seqno1, const __int32& seqno2) const;
   const bool notlessthan(const __int32& seqno1, const __int32& seqno2) const;
   const bool notgreaterthan(const __int32& seqno1, const __int32& seqno2) const;

   const __int32 getLength(const __int32& seqno1, const __int32& seqno2) const;

   const __int32 incSeqNo(const __int32& seqno) const;
   const __int32 decSeqNo(const __int32& seqno) const;

protected:
   __int32 m_iSeqNoTH;                  // threshold for comparing seq. no.
   __int32 m_iMaxSeqNo;                 // maximum permitted seq. no.
};


////////////////////////////////////////////////////////////////////////////////
class CSndLossList: public CList
{
public:
   CSndLossList(const __int32& size, const __int32& th, const __int32& max);
   ~CSndLossList();

      // Functionality:
      //    Insert a seq. no. into the sender loss list.
      // Parameters:
      //    0) [in] seqno1: sequence number starts.
      //    1) [in] seqno2: sequence number ends.
      // Returned value:
      //    number of packets that are not in the list previously.

   __int32 insert(const __int32& seqno1, const __int32& seqno2);

      // Functionality:
      //    Remove ALL the seq. no. that are not greater than the parameter.
      // Parameters:
      //    0) [in] seqno: sequence number.
      // Returned value:
      //    None.

   void remove(const __int32& seqno);

      // Functionality:
      //    Read the loss length.
      // Parameters:
      //    None.
      // Returned value:
      //    The length of the list.

   __int32 getLossLength();

      // Functionality:
      //    Read the first (smallest) loss seq. no. in the list and remove it.
      // Parameters:
      //    None.
      // Returned value:
      //    The seq. no. or -1 if the list is empty.

   __int32 getLostSeq();

private:
   __int32* m_piData1;			// sequence number starts
   __int32* m_piData2;			// seqnence number ends
   __int32* m_piNext;			// next node in the list

   __int32 m_iHead;			// first node
   __int32 m_iLength;			// loss length
   __int32 m_iSize;			// size of the static array
   __int32 m_iLastInsertPos;		// position of last insert node

   pthread_mutex_t m_ListLock;		// used to synchronize list operation
};


////////////////////////////////////////////////////////////////////////////////
class CRcvLossList: public CList
{
public:
   CRcvLossList(const __int32& size, const __int32& th, const __int32& max);
   ~CRcvLossList();

      // Functionality:
      //    Insert a series of loss seq. no. between "seqno1" and "seqno2" into the receiver's loss list.
      // Parameters:
      //    0) [in] seqno1: sequence number starts.
      //    1) [in] seqno2: seqeunce number ends.
      // Returned value:
      //    None.

   void insert(const __int32& seqno1, const __int32& seqno2);

      // Functionality:
      //    Remove a loss seq. no. from the receiver's loss list.
      // Parameters:
      //    0) [in] seqno: sequence number.
      // Returned value:
      //    if the packet is removed (true) or no such lost packet is found (false).

   bool remove(const __int32& seqno);

      // Functionality:
      //    Read the loss length.
      // Parameters:
      //    None.
      // Returned value:
      //    the length of the list.

   __int32 getLossLength() const;

      // Functionality:
      //    Read the first (smallest) seq. no. in the list.
      // Parameters:
      //    None.
      // Returned value:
      //    the sequence number or -1 if the list is empty.

   __int32 getFirstLostSeq() const;

      // Functionality:
      //    Get a encoded loss array for NAK report.
      // Parameters:
      //    0) [out] array: the result list of seq. no. to be included in NAK.
      //    1) [out] physical length of the result array.
      //    2) [in] limit: maximum length of the array.
      //    3) [in] threshold: Time threshold from last NAK report.
      // Returned value:
      //    None.

   void getLossArray(__int32* array, __int32& len, const __int32& limit, const __int32& threshold);

private:
   __int32* m_piData1;			// sequence number starts
   __int32* m_piData2;			// sequence number ends
   timeval* m_pLastFeedbackTime;	// last feedback time of the node
   __int32* m_piCount;			// report counter
   __int32* m_piNext;			// next node in the list
   __int32* m_piPrior;			// prior node in the list;

   __int32 m_iHead;			// first node in the list
   __int32 m_iTail;			// last node in the list;
   __int32 m_iLength;			// loss length
   __int32 m_iSize;			// size of the static array
};


////////////////////////////////////////////////////////////////////////////////
class CIrregularPktList: public CList
{
public:
   CIrregularPktList(const __int32& size, const __int32& th, const __int32& max);
   ~CIrregularPktList();

      // Functionality:
      //    Read the total size error of all the irregular packets prior to "seqno".
      // Parameters:
      //    0) [in] seqno: sequence number.
      // Returned value:
      //    the total size error of all the irregular packets prior to (excluding) "seqno".

   __int32 currErrorSize(const __int32& seqno) const;

      // Functionality:
      //    Insert an irregular packet into the list.
      // Parameters:
      //    0) [in] seqno: sequence number.
      //    1) [in] errsize: size error of the current packet.
      // Returned value:
      //    None

   void addIrregularPkt(const __int32& seqno, const __int32& errsize);

      // Functionality:
      //    Remove ALL the packets prior to "seqno".
      // Parameters:
      //    0) [in] seqno: sequence number.
      // Returned value:
      //    None

   void deleteIrregularPkt(const __int32& seqno);

private:
   __int32* m_piData;			// sequence number
   __int32* m_piErrorSize;		// size error of the node
   __int32* m_piNext;			// next node in the list

   __int32 m_iHead;			// first node in the list
   __int32 m_iLength;			// number of irregular packets in the list
   __int32 m_iSize;			// size of the static array
   __int32 m_iInsertPos;		// last node insert position
};


////////////////////////////////////////////////////////////////////////////////
class CSndBuffer
{
public:
   CSndBuffer();
   ~CSndBuffer();

      // Functionality:
      //    Insert a user buffer into the sending list.
      // Parameters:
      //    0) [in] data: pointer to the user data block.
      //    1) [in] len: size of the block.
      //    2) [in] handle: handle of this request IO.
      //    3) [in] func: routine to process the buffer after IO completed.
      // Returned value:
      //    None.

   void addBuffer(const char* data, const __int32& len, const __int32& handle, const UDT_MEM_ROUTINE func);

      // Functionality:
      //    Find data position to pack a DATA packet from the furthest reading point.
      // Parameters:
      //    0) [out] data: the pointer to the data position.
      //    1) [in] len: Expected data length.
      // Returned value:
      //    Actual length of data read.

   __int32 readData(char** data, const __int32& len);

      // Functionality:
      //    Find data position to pack a DATA packet for a retransmission.
      // Parameters:
      //    0) [out] data: the pointer to the data position.
      //    1) [in] offset: offset from the last ACK point.
      //    2) [in] len: Expected data length.
      // Returned value:
      //    Actual length of data read.

   __int32 readData(char** data, const __int32 offset, const __int32& len);

      // Functionality:
      //    Update the ACK point and may release/unmap/return the user data according to the flag.
      // Parameters:
      //    0) [in] len: size of data acknowledged.
      //    1) [in] payloadsize: regular payload size that UDT always try to read.
      // Returned value:
      //    None.

   void ackData(const __int32& len, const __int32& payloadsize);

      // Functionality:
      //    Read size of data still in the sending list.
      // Parameters:
      //    None.
      // Returned value:
      //    Current size of the data in the sending list.

   __int32 getCurrBufSize() const;

      // Functionality:
      //    Query the progress of the buffer sending identified by handle.
      // Parameters:
      //    1) [in] handle: descriptor of this overlapped IO
      //    2) [out] progress: the current progress of the overlapped IO
      // Returned value:
      //    if the overlapped IO is completed.

   bool getOverlappedResult(const __int32& handle, __int32& progress);

      // Functionality:
      //    helper function to release the user buffer.
      // Parameters:
      //    1) [in]: pointer to the buffer
      //    2) [in]: buffer size
      // Returned value:
      //    Current size of the data in the sending list

  static void releaseBuffer(char* buf, __int32);

private:
   pthread_mutex_t m_BufLock;		// used to synchronize buffer operation

   struct Block
   {
      char* m_pcData;			// pointer to the data block
      __int32 m_iLength;		// length of the block

      __int32 m_iHandle;		// a unique handle to represent this senidng request
      UDT_MEM_ROUTINE m_pMemRoutine;	// function to process buffer after sending

      Block* m_next;			// next block
   } *m_pBlock, *m_pLastBlock, *m_pCurrSendBlk, *m_pCurrAckBlk;
   
   // m_pBlock:		The first block
   // m_pLastBlock:	The last block
   // m_pCurrSendBlk:	The block contains the data with the largest seq. no. that has been sent
   // m_pCurrAckBlk:	The block contains the data with the latest ACK (= m_pBlock)

   __int32 m_iCurrBufSize;		// Total size of the blocks
   __int32 m_iCurrSendPnt;		// pointer to the data with the largest current seq. no.
   __int32 m_iCurrAckPnt;		// pointer to the data with the latest ACK
};


////////////////////////////////////////////////////////////////////////////////
class CRcvBuffer
{
public:
   CRcvBuffer();
   CRcvBuffer(const __int32& bufsize);
   ~CRcvBuffer();

      // Functionality:
      //    Find a position in the buffer to receive next packet.
      // Parameters:
      //    0) [out] data: the pointer to the next data position.
      //    1) [in] offset: offset from last ACK point.
      //    2) [in] len: size of data to be written.
      // Returned value:
      //    true if found, otherwise false.

   bool nextDataPos(char** data, __int32 offset, const __int32& len);

      // Functionality:
      //    Write data into the buffer.
      // Parameters:
      //    0) [in] data: pointer to data to be copied.
      //    1) [in] offset: offset from last ACK point.
      //    2) [in] len: size of data to be written.
      // Returned value:
      //    true if a position that can hold the data is found, otherwise false.

   bool addData(char* data, __int32 offset, __int32 len);

      // Functionality:
      //    Move part of the data in buffer to the direction of the ACK point by some length.
      // Parameters:
      //    0) [in] offset: From where to move the data.
      //    1) [in] len: How much to move.
      // Returned value:
      //    None.

   void moveData(__int32 offset, const __int32& len);

      // Functionality:
      //    Read data from the buffer into user buffer.
      // Parameters:
      //    0) [out] data: data read from protocol buffer.
      //    1) [in] len: size of data to be read.
      // Returned value:
      //    true if there is enough data to read, otherwise return false.

   bool readBuffer(char* data, const __int32& len);

      // Functionality:
      //    Update the ACK point of the buffer.
      // Parameters:
      //    0) [in] len: size of data to be acknowledged.
      // Returned value:
      //    1 if a user buffer is fulfilled, otherwise 0.

   __int32 ackData(const __int32& len);

      // Functionality:
      //    Insert the user buffer into the protocol buffer.
      // Parameters:
      //    0) [in] buf: pointer to the user buffer.
      //    1) [in] len: size of the user buffer.
      //    2) [in] handle: descriptor of this overlapped receiving.
      // Returned value:
      //    Size of data that has been received by now.

   __int32 registerUserBuf(char* buf, const __int32& len, const __int32& handle, const UDT_MEM_ROUTINE func);

      // Functionality:
      //    remove the user buffer from the protocol buffer.
      // Parameters:
      //    None
      // Returned value:
      //    None.

   void removeUserBuf();

      // Functionality:
      //    Query how many buffer space left for data receiving.
      // Parameters:
      //    None.
      // Returned value:
      //    size of available buffer space (including user buffer) for data receiving.

   __int32 getAvailBufSize() const;

      // Functionality:
      //    Query how many data has been continuously received (for reading).
      // Parameters:
      //    None.
      // Returned value:
      //    size of valid (continous) data for reading.

   __int32 getRcvDataSize() const;

      // Functionality:
      //    Query the progress of the buffer sending identified by handle.
      // Parameters:
      //    1) [in] handle: descriptor of this overlapped IO
      //    2) [out] progress: the current progress of the overlapped IO
      // Returned value:
      //    if the overlapped IO is completed.

   bool getOverlappedResult(const __int32& handle, __int32& progress);

      // Functionality:
      //    Query the total size of overlapped recv buffers.
      // Parameters:
      //    None.
      // Returned value:
      //    Total size of the pending overlapped recv buffers.

   __int32 getPendingQueueSize() const;

private:
   char* m_pcData;			// pointer to the protocol buffer
   __int32 m_iSize;			// size of the protocol buffer

   __int32 m_iStartPos;			// the head position for I/O
   __int32 m_iLastAckPos;		// the last ACKed position
   __int32 m_iMaxOffset;		// the furthest "dirty" position (absolute distance from m_iLastAckPos)

   char* m_pcUserBuf;			// pointer to the user registered buffer
   __int32 m_iUserBufSize;		// size of the user buffer
   __int32 m_iUserBufAck;		// last ACKed position of the user buffer
   __int32 m_iHandle;			// unique handle to represet this IO request
   UDT_MEM_ROUTINE m_MemProcess;	// function to process user buffer after receiving

   struct Block
   {
      char* m_pcData;                   // pointer to the overlapped recv buffer
      __int32 m_iLength;                // length of the block

      __int32 m_iHandle;                // a unique handle to represent this receiving request
      UDT_MEM_ROUTINE m_pMemRoutine;    // function to process buffer after a complete receiving

      Block* m_next;                    // next block
   } *m_pPendingBlock, *m_pLastBlock;

   // m_pPendingBlock:			// the list of pending overlapped recv buffers
   // m_pLastBlock:			// the last block of pending buffers

   __int32 m_iPendingSize;		// total size of pending recv buffers
};


//////////////////////////////////////////////////////////////////////////////
class CUDT;

struct CUDTSocket
{
   CUDTSocket();
   ~CUDTSocket();

   enum UDTSTATUS {INIT = 1, OPENED, LISTENING, CONNECTED, CLOSED};
   UDTSTATUS m_Status;			// current socket state

   timeval m_TimeStamp;			// time when the socket is closed

   __int32 m_iIPversion;		// IP version
   sockaddr* m_pSelfAddr;		// pointer to the local address of the socket
   sockaddr* m_pPeerAddr;		// pointer to the peer address of the socket

   UDTSOCKET m_Socket;			// socket ID
   UDTSOCKET m_ListenSocket;		// ID of the listener socket; 0 means this is an independent socket

   CUDT* m_pUDT;			// pointer to the UDT entity

   set<UDTSOCKET>* m_pQueuedSockets;	// set of connections waiting for accept()
   set<UDTSOCKET>* m_pAcceptSockets;	// set of accept()ed connections

   pthread_cond_t m_AcceptCond;		// used to block "accept" call
   pthread_mutex_t m_AcceptLock;	// mutex associated to m_AcceptCond

   unsigned __int32 m_uiBackLog;	// maximum number of connections in queue
};

class CUDTUnited
{
public:
   CUDTUnited();
   ~CUDTUnited();

public:

      // Functionality:
      //    Create a new UDT socket.
      // Parameters:
      //    0) [in] af: IP version, IPv4 (AF_INET) or IPv6 (AF_INET6).
      //    1) [in] type: socket type, SOCK_STREAM or SOCK_DGRAM
      // Returned value:
      //    The new UDT socket ID, or INVALID_SOCK.

   UDTSOCKET newSocket(const __int32& af, const __int32& type);

      // Functionality:
      //    Create a new UDT connection.
      // Parameters:
      //    0) [in] listen: the listening UDT socket;
      //    1) [in] peer: peer address.
      //    2) [in/out] hs: handshake information from peer side (in), negotiated value (out);
      // Returned value:
      //    If the new connection is successfully created: 1 success, 0 already exist, -1 error.

   int newConnection(const UDTSOCKET listen, const sockaddr* peer, CHandShake* hs);

      // Functionality:
      //    look up the UDT entity according to its ID.
      // Parameters:
      //    0) [in] u: the UDT socket ID.
      // Returned value:
      //    Pointer to the UDT entity.

   CUDT* lookup(const UDTSOCKET u);

      // socket APIs

   __int32 bind(const UDTSOCKET u, const sockaddr* name, const __int32& namelen);
   __int32 listen(const UDTSOCKET u, const __int32& backlog);
   UDTSOCKET accept(const UDTSOCKET listen, sockaddr* addr, __int32* addrlen);
   __int32 connect(const UDTSOCKET u, const sockaddr* name, const __int32& namelen);
   __int32 close(const UDTSOCKET u);
   __int32 getpeername(const UDTSOCKET u, sockaddr* name, __int32* namelen);
   __int32 getsockname(const UDTSOCKET u, sockaddr* name, __int32* namelen);
   __int32 select(ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout);

      // Functionality:
      //    record the UDT exception.
      // Parameters:
      //    0) [in] e: pointer to a UDT exception instance.
      // Returned value:
      //    None.

   void setError(CUDTException* e);

      // Functionality:
      //    look up the most recent UDT exception.
      // Parameters:
      //    None.
      // Returned value:
      //    pointer to a UDT exception instance.

   CUDTException* getError();

private:
   map<UDTSOCKET, CUDTSocket*> m_Sockets;	// stores all the socket structures

   pthread_mutex_t m_ControlLock;		// used to synchronize UDT API

   pthread_mutex_t m_IDLock;			// used to synchronize ID generation
   UDTSOCKET m_SocketID;			// seed to generate a new unique socket ID

private:
   pthread_key_t m_TLSError;			// thread local error record (last error)
   static void TLSDestroy(void* e) {delete (CUDTException*)e;}

private:
   CUDTSocket* locate(const UDTSOCKET u);
   CUDTSocket* locate(const UDTSOCKET u, const sockaddr* peer);
   void checkBrokenSockets();
   void removeSocket(const UDTSOCKET u);
};


//////////////////////////////////////////////////////////////////////////////
class UDT_API CCC
{
friend class CUDT;

public:
   CCC();
   virtual ~CCC() {}

public:
   static const __int32 m_iCCID = 0;

public:

      // Functionality:
      //    Callback function to be called (only) at the start of a UDT connection.
      //    note that this is different from CCC(), which is always called.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   virtual void init() {}

      // Functionality:
      //    Callback function to be called when a UDT connection is closed.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   virtual void close() {}

      // Functionality:
      //    Callback function to be called when an ACK packet is received.
      // Parameters:
      //    0) [in] ackno: the data sequence number acknowledged by this ACK.
      // Returned value:
      //    None.

   virtual void onACK(const __int32&) {}

      // Functionality:
      //    Callback function to be called when a loss report is received.
      // Parameters:
      //    0) [in] losslist: list of sequence number of packets, in the format describled in packet.cpp.
      //    1) [in] size: length of the loss list.
      // Returned value:
      //    None.

   virtual void onLoss(const __int32*, const __int32&) {}

      // Functionality:
      //    Callback function to be called when a timeout event occurs.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   virtual void onTimeout() {}

      // Functionality:
      //    Callback function to be called when a data is sent.
      // Parameters:
      //    0) [in] seqno: the data sequence number.
      //    1) [in] size: the payload size.
      // Returned value:
      //    None.

   virtual void onPktSent(const CPacket*) {}

      // Functionality:
      //    Callback function to be called when a data is received.
      // Parameters:
      //    0) [in] seqno: the data sequence number.
      //    1) [in] size: the payload size.
      // Returned value:
      //    None.

   virtual void onPktReceived(const CPacket*) {}

      // Functionality:
      //    Callback function to Process a user defined packet.
      // Parameters:
      //    0) [in] pkt: the user defined packet.
      // Returned value:
      //    None.

   virtual void processCustomMsg(const CPacket*) {}

protected:

      // Functionality:
      //    Set periodical acknowldging and the ACK period.
      // Parameters:
      //    0) [in] msINT: the period to send an ACK.
      // Returned value:
      //    None.

   void setACKTimer(const __int32& msINT);

      // Functionality:
      //    Set packet-based acknowldging and the number of packets to send an ACK.
      // Parameters:
      //    0) [in] pktINT: the number of packets to send an ACK.
      // Returned value:
      //    None.

   void setACKInterval(const __int32& pktINT);

      // Functionality:
      //    Set RTO value.
      // Parameters:
      //    0) [in] msRTO: RTO in macroseconds.
      // Returned value:
      //    None.

   void setRTO(const __int32& usRTO);

      // Functionality:
      //    Send a user defined control packet.
      // Parameters:
      //    0) [in] pkt: user defined packet.
      // Returned value:
      //    None.

   void sendCustomMsg(CPacket& pkt) const;

      // Functionality:
      //    retrieve performance information.
      // Parameters:
      //    None.
      // Returned value:
      //    Pointer to a performance info structure.

   const CPerfMon* getPerfInfo();

protected:
   double m_dPktSndPeriod;		// Packet sending period, in microseconds
   double m_dCWndSize;			// Congestion window size, in packets

private:
   UDTSOCKET m_UDT;                     // The UDT entity that this congestion control algorithm is bound to
   CUDT* m_pUDT;                        // UDT class instance, internal use only

   __int32 m_iACKPeriod;		// Periodical timer to send an ACK, in milliseconds 
   __int32 m_iACKInterval;		// How many packets to send one ACK, in packets
   __int32 m_iRTO;			// RTO value
   CPerfMon m_PerfInfo;			// protocol statistics information
};

class CCCVirtualFactory
{
public:
   virtual CCC* create() = 0;
   virtual CCCVirtualFactory* clone() = 0;
};

template <class T> 
class CCCFactory: public CCCVirtualFactory
{
public:
   virtual CCC* create() {return new T;}
   virtual CCCVirtualFactory* clone() {return new CCCFactory<T>;}
};


//////////////////////////////////////////////////////////////////////////////
class UDT_API CUDT
{
friend class CUDTSocket;
friend class CUDTUnited;
friend class CCC;

private: // constructor and desctructor
   CUDT();
   CUDT(const CUDT& ancestor);
   const CUDT& operator=(const CUDT&) {return *this;}
   ~CUDT();

public: //API
   static UDTSOCKET socket(int af, int type = SOCK_STREAM, int protocol = 0);
   static int bind(UDTSOCKET u, const sockaddr* name, int namelen);
   static int listen(UDTSOCKET u, int backlog);
   static UDTSOCKET accept(UDTSOCKET u, sockaddr* addr, int* addrlen);
   static int connect(UDTSOCKET u, const sockaddr* name, int namelen);
   static int close(UDTSOCKET u);
   static int getpeername(UDTSOCKET u, sockaddr* name, int* namelen);
   static int getsockname(UDTSOCKET u, sockaddr* name, int* namelen);
   static int getsockopt(UDTSOCKET u, int level, UDTOpt optname, void* optval, int* optlen);
   static int setsockopt(UDTSOCKET u, int level, UDTOpt optname, const void* optval, int optlen);
   static int shutdown(UDTSOCKET u, int how);
   static int send(UDTSOCKET u, const char* buf, int len, int flags = 0, int* handle = NULL, UDT_MEM_ROUTINE routine = NULL);
   static int recv(UDTSOCKET u, char* buf, int len, int flags = 0, int* handle = NULL, UDT_MEM_ROUTINE routine = NULL);
   static __int64 sendfile(UDTSOCKET u, ifstream& ifs, const __int64& offset, __int64& size, const int& block = 366000);
   static __int64 recvfile(UDTSOCKET u, ofstream& ofs, const __int64& offset, __int64& size, const int& block = 7320000);
   static bool getoverlappedresult(UDTSOCKET u, int handle, int& progress, bool wait = false);
   static int select(int nfds, ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout);
   static CUDTException& getlasterror();
   static int perfmon(UDTSOCKET u, CPerfMon* perf, bool clear = true);

public: // internal API
   static bool isUSock(UDTSOCKET u);

private:
      // Functionality: 
      //    initialize a UDT entity and bind to a local address.
      // Parameters: 
      //    0) [in] addr: pointer to the local address to be bound to.
      // Returned value:
      //    None.

   void open(const sockaddr* addr = NULL);

      // Functionality:
      //    Start listening to any connection request.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void listen();

      // Functionality:
      //    Connect to a UDT entity listening at address "peer".
      // Parameters:
      //    0) [in] peer: The address of the listening UDT entity.
      // Returned value:
      //    None.

   void connect(const sockaddr* peer);

      // Functionality:
      //    Connect to a UDT entity listening at address "peer", which has sent "hs" request.
      // Parameters:
      //    0) [in] peer: The address of the listening UDT entity.
      //    1) [in/out] hs: The handshake information sent by the peer side (in), negotiated value (out).
      // Returned value:
      //    None.

   void connect(const sockaddr* peer, CHandShake* hs);

      // Functionality:
      //    Close the opened UDT entity.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void close();

      // Functionality:
      //    Request UDT to send out a data block "data" with size of "len".
      // Parameters:
      //    0) [in] data: The address of the application data to be sent.
      //    1) [in] len: The size of the data block.
      //    2) [in, out] overlapped: A pointer to the returned overlapped IO handle.
      //    3) [in] func: pointer to a function to process the buffer after overlapped IO is completed.
      // Returned value:
      //    Actual size of data sent.

   __int32 send(char* data, const __int32& len,  __int32* overlapped = NULL, const UDT_MEM_ROUTINE func = NULL);

      // Functionality:
      //    Request UDT to receive data to a memory block "data" with size of "len".
      // Parameters:
      //    0) [out] data: data received.
      //    1) [in] len: The desired size of data to be received.
      //    2) [out] overlapped: A pointer to the returned overlapped IO handle.
      //    3) [in] unused.
      // Returned value:
      //    Actual size of data received.

   __int32 recv(char* data, const __int32& len, __int32* overlapped = NULL, const UDT_MEM_ROUTINE func = NULL);

      // Functionality:
      //    query the result of an overlapped IO indicated by "handle".
      // Parameters:
      //    0) [in] handle: the handle that indicates the submitted overlapped IO.
      //    1) [out] progess: how many data left to be sent/receive.
      //    2) [in] wait: wait for the IO finished or not.
      // Returned value:
      //    if the overlapped IO is completed.

   bool getOverlappedResult(const __int32& handle, __int32& progress, const bool& wait = false);

      // Functionality:
      //    Request UDT to send out a file described as "fd", starting from "offset", with size of "size".
      // Parameters:
      //    0) [in] ifs: The input file stream.
      //    1) [in] offset: From where to read and send data;
      //    2) [in] size: How many data to be sent.
      //    3) [in] block: size of block per read from disk
      // Returned value:
      //    Actual size of data sent.

   __int64 sendfile(ifstream& ifs, const __int64& offset, const __int64& size, const __int32& block = 366000);

      // Functionality:
      //    Request UDT to receive data into a file described as "fd", starting from "offset", with expected size of "size".
      // Parameters:
      //    0) [out] ofs: The output file stream.
      //    1) [in] offset: From where to write data;
      //    2) [in] size: How many data to be received.
      //    3) [in] block: size of block per write to disk
      // Returned value:
      //    Actual size of data received.

   __int64 recvfile(ofstream& ofs, const __int64& offset, const __int64& size, const __int32& block = 7320000);

      // Functionality:
      //    Configure UDT options.
      // Parameters:
      //    0) [in] optName: The enum name of a UDT option.
      //    1) [in] optval: The value to be set.
      //    2) [in] optlen: size of "optval".
      // Returned value:
      //    None.

   void setOpt(UDTOpt optName, const void* optval, const __int32& optlen);

      // Functionality:
      //    Read UDT options.
      // Parameters:
      //    0) [in] optName: The enum name of a UDT option.
      //    1) [in] optval: The value to be returned.
      //    2) [out] optlen: size of "optval".
      // Returned value:
      //    None.

   void getOpt(UDTOpt optName, void* optval, __int32& optlen);

      // Functionality:
      //    read the performance data since last sample() call.
      // Parameters:
      //    0) [in, out] perf: pointer to a CPerfMon structure to record the performance data.
      //    1) [in] clear: flag to decide if the local performance trace should be cleared.
      // Returned value:
      //    None.

   void sample(CPerfMon* perf, bool clear = true);

private:
   static CUDTUnited s_UDTUnited;		// UDT global management base

public:
#ifndef WIN32
   const static UDTSOCKET INVALID_SOCK = -1;	// invalid socket descriptor
   const static int ERROR = -1;                 // socket api error returned value
#else
   const static int INVALID_SOCK = -1;
   #undef ERROR
   const static int ERROR = -1;
#endif

private:
   UDTSOCKET m_SocketID;			// UDT socket number
   __int32 m_iSockType;				// Type of the UDT connection (SOCK_STREAM or SOCK_DGRAM)

private: // Version
   const __int32 m_iVersion;			// UDT version, for compatibility use

private: // Threads, data channel, and timing facility
#ifndef WIN32
   bool m_bSndThrStart;				// lazy snd thread creation
#endif
   pthread_t m_SndThread;			// Sending thread
   pthread_t m_RcvThread;			// Receiving thread
   CChannel* m_pChannel;			// UDP channel
   CTimer* m_pTimer;				// Timing facility
   unsigned __int64 m_ullCPUFrequency;		// CPU clock frequency, used for Timer

private: // Timing intervals
   const __int32 m_iSYNInterval;                // Periodical Rate Control Interval, 10 microseconds
   const __int32 m_iSelfClockInterval;		// ACK interval for self-clocking

private: // Packet size and sequence number attributes
   __int32 m_iPktSize;				// Maximum/regular packet size, in bytes
   __int32 m_iPayloadSize;			// Maximum/regular payload size, in bytes
   const __int32 m_iMaxSeqNo;			// Maximum data sequence number
   const __int32 m_iSeqNoTH;			// The threshold used to compare 2 sequence numbers
   const __int32 m_iMaxAckSeqNo;		// Maximum ACK sequence number

private: // Options
   __int32 m_iMSS;				// Maximum Segment Size
   bool m_bSynSending;				// Sending syncronization mode
   bool m_bSynRecving;				// Receiving syncronization mode
   __int32 m_iFlightFlagSize;			// Maximum number of packets in flight from the peer side
   __int32 m_iSndQueueLimit;			// Maximum length of the sending buffer queue
   __int32 m_iUDTBufSize;			// UDT buffer size (for receiving)
   linger m_Linger;				// Linger information on close
   __int32 m_iUDPSndBufSize;			// UDP sending buffer size
   __int32 m_iUDPRcvBufSize;			// UDP receiving buffer size
   __int32 m_iMaxMsg;				// Maximum message size of datagram UDT connection
   __int32 m_iMsgTTL;				// Time-to-live of a datagram message
   __int32 m_iIPversion;			// IP version
   bool m_bRendezvous;				// Rendezvous connection mode
   __int32 m_iSndTimeOut;			// sending timeout in milliseconds
   __int32 m_iRcvTimeOut;			// receiving timeout in milliseconds

   const __int32 m_iProbeInterval;		// Number of regular packets between two probing packet pairs
   const __int32 m_iQuickStartPkts;		// Number of packets to be sent as a quick start

private: // CCC
   CCCVirtualFactory* m_pCCFactory;		// Factory class to create a specific CC instance
   CCC* m_pCC;                                  // custom congestion control class

private: // Status
   volatile bool m_bListening;			// If the UDT entit is listening to connection
   volatile bool m_bConnected;			// Whether the connection is on or off
   volatile bool m_bClosing;			// If the UDT entity is closing
   volatile bool m_bShutdown;			// If the peer side has shutdown the connection
   volatile bool m_bBroken;			// If the connection has been broken
   bool m_bOpened;				// If the UDT entity has been opened
   bool m_bSndSlowStart;			// If UDT is during slow start phase (snd side flag)
   bool m_bRcvSlowStart;			// If UDT is during slow start phase (rcv side flag)
   bool m_bFreeze;				// freeze the data sending
   __int32 m_iEXPCount;				// Expiration counter
   __int32 m_iBandwidth;			// Estimated bandwidth

private: // connection setup
   pthread_t m_ListenThread;

   #ifndef WIN32
      static void* listenHandler(void* listener);
   #else
      static DWORD WINAPI listenHandler(LPVOID listener);
   #endif

private: // Sending related data
   CSndBuffer* m_pSndBuffer;			// Sender buffer
   CSndLossList* m_pSndLossList;		// Sender loss list
   CPktTimeWindow* m_pSndTimeWindow;		// Packet sending time window

   volatile unsigned __int64 m_ullInterval;	// Inter-packet time, in CPU clock cycles
   unsigned __int64 m_ullLastDecRate;		// inter-packet time when last decrease occurs
   unsigned __int64 m_ullTimeDiff;		// aggregate difference in inter-packet time

   __int32 m_iFlowWindowSize;                   // Flow control window size
   __int32 m_iMaxFlowWindowSize;                // Maximum flow window size = flight flag size of the peer side
   volatile double m_dCongestionWindow;         // congestion window size

   __int32 m_iNAKCount;				// NAK counter
   __int32 m_iDecRandom;			// random threshold on decrease by number of loss events
   __int32 m_iAvgNAKNum;			// average number of NAKs per congestion

   timeval m_LastSYNTime;			// the timestamp when last rate control occured
   bool m_bLoss;				// if there is any loss during last RC period

   volatile __int32 m_iSndLastAck;		// Last ACK received
   __int32 m_iSndLastDataAck;			// The real last ACK that updates the sender buffer and loss list
   __int32 m_iSndCurrSeqNo;			// The largest sequence number that has been sent
   __int32 m_iLastDecSeq;			// Sequence number sent last decrease occurs

   __int32 m_iISN;				// Initial Sequence Number

private: // Receiving related data
   CRcvBuffer* m_pRcvBuffer;			// Receiver buffer
   CRcvLossList* m_pRcvLossList;		// Receiver loss list
   CIrregularPktList* m_pIrrPktList;		// Irregular sized packet list
   CACKWindow* m_pACKWindow;			// ACK history window
   CPktTimeWindow* m_pRcvTimeWindow;		// Packet arrival time window

   __int32 m_iRTT;				// RTT
   __int32 m_iRTTVar;				// RTT variance

   __int32 m_iRcvLastAck;			// Last sent ACK
   unsigned __int64 m_ullLastAckTime;		// Timestamp of last ACK
   __int32 m_iRcvLastAckAck;			// Last sent ACK that has been acknowledged
   __int32 m_iAckSeqNo;				// Last ACK sequence number
   __int32 m_iRcvCurrSeqNo;			// Largest received sequence number
   __int32 m_iNextExpect;			// Sequence number of next speculated packet to receive

   volatile bool m_bReadBuf;			// Application has called "recv" but has not finished
   volatile char* m_pcTempData;			// Pointer to the buffer that application want to put received data into
   volatile __int32 m_iTempLen;			// Size of the "m_pcTempData"
   volatile UDT_MEM_ROUTINE m_iTempRoutine;	// pointer ot a routine function to process "m_pcTempData"

   __int32 m_iUserBufBorder;			// Sequence number of last packet that will fulfill a user buffer

   unsigned __int64 m_ullLastWarningTime;	// Last time that a warning message is sent

   __int32 m_iPeerISN;				// Initial Sequence Number of the peer side

   __int32 m_iFlowControlWindow;		// flow control window size to be advertised

private: // Overlapped IO related
   __int32 m_iSndHandle;			// seed used to generate an overlapped sending handle
   __int32 m_iRcvHandle;			// seed used to generate an overlapped receiving handle

private: // synchronization: mutexes and conditions
   pthread_mutex_t m_ConnectionLock;		// used to synchronize connection operation

   pthread_cond_t m_SendDataCond;		// used to block sending when there is no data
   pthread_mutex_t m_SendDataLock;		// lock associated to m_SendDataCond

   pthread_cond_t m_SendBlockCond;		// used to block "send" call
   pthread_mutex_t m_SendBlockLock;		// lock associated to m_SendBlockCond

   pthread_mutex_t m_AckLock;			// used to protected sender's loss list when processing ACK

   pthread_cond_t m_WindowCond;			// used to block sending when flow window is exceeded
   pthread_mutex_t m_WindowLock;		// lock associated to m_WindowLock

   pthread_cond_t m_RecvDataCond;		// used to block "recv" when there is no data
   pthread_mutex_t m_RecvDataLock;		// lock associated to m_RecvDataCond

   pthread_cond_t m_OverlappedRecvCond;		// used to block "recv" when overlapped receving is in progress
   pthread_mutex_t m_OverlappedRecvLock;	// lock associated to m_OverlappedRecvCond

   pthread_mutex_t m_HandleLock;		// used to generate unique send/recv handle

   pthread_mutex_t m_SendLock;			// used to synchronize "send" call
   pthread_mutex_t m_RecvLock;			// used to synchronize "recv" call

   void initSynch();
   void destroySynch();
   void releaseSynch();

private: // Thread handlers
   #ifndef WIN32
      static void* sndHandler(void* sender);
      static void* rcvHandler(void* recver);
   #else
      static DWORD WINAPI sndHandler(LPVOID sender);
      static DWORD WINAPI rcvHandler(LPVOID recver);
   #endif

private: // congestion control 
   void rateControl();
   void flowControl(const __int32& recvrate);

private: // Generation and processing of control packet
   void sendCtrl(const __int32& pkttype, void* lparam = NULL, void* rparam = NULL, const __int32& size = 0);
   void processCtrl(CPacket& ctrlpkt);

private: // Trace
   timeval m_StartTime;				// timestamp when the UDT entity is started
   __int64 m_llSentTotal;			// total number of sent data packets, including retransmissions
   __int64 m_llRecvTotal;			// total number of received packets
   __int32 m_iSndLossTotal;			// total number of lost packets (sender side)
   __int32 m_iRcvLossTotal;                     // total number of lost packets (receiver side)
   __int32 m_iRetransTotal;			// total number of retransmitted packets
   __int32 m_iSentACKTotal;			// total number of sent ACK packets
   __int32 m_iRecvACKTotal;			// total number of received ACK packets
   __int32 m_iSentNAKTotal;			// total number of sent NAK packets
   __int32 m_iRecvNAKTotal;			// total number of received NAK packets

   timeval m_LastSampleTime;                    // last performance sample time
   __int64 m_llTraceSent;			// number of pakctes sent in the last trace interval
   __int64 m_llTraceRecv;			// number of pakctes received in the last trace interval
   __int32 m_iTraceSndLoss;                     // number of lost packets in the last trace interval (sender side)
   __int32 m_iTraceRcvLoss;                     // number of lost packets in the last trace interval (receiver side)
   __int32 m_iTraceRetrans;                     // number of retransmitted packets in the last trace interval
   __int32 m_iSentACK;				// number of ACKs sent in the last trace interval
   __int32 m_iRecvACK;				// number of ACKs received in the last trace interval
   __int32 m_iSentNAK;				// number of NAKs sent in the last trace interval
   __int32 m_iRecvNAK;				// number of NAKs received in the last trace interval

private: // internal data
   char* m_pcTmpBuf;
};


// Global UDT API
namespace UDT
{
typedef CUDTException ERRORINFO;
typedef UDTOpt SOCKOPT;
typedef CPerfMon TRACEINFO;
typedef ud_set UDSET;

UDT_API extern const UDTSOCKET INVALID_SOCK;
UDT_API extern const int ERROR;

UDT_API inline UDTSOCKET socket(int af, int type = SOCK_STREAM, int protocol = 0)
{
   return CUDT::socket(af, type, protocol);
}

UDT_API inline int bind(UDTSOCKET u, const struct sockaddr* name, int namelen)
{
   return CUDT::bind(u, name, namelen);
}

UDT_API inline int listen(UDTSOCKET u, int backlog)
{
   return CUDT::listen(u, backlog);
}

UDT_API inline UDTSOCKET accept(UDTSOCKET u, struct sockaddr* addr, int* addrlen)
{
   return CUDT::accept(u, addr, addrlen);
}

UDT_API inline int connect(UDTSOCKET u, const struct sockaddr* name, int namelen)
{
   return CUDT::connect(u, name, namelen);
}

UDT_API inline int close(UDTSOCKET u)
{
   return CUDT::close(u);
}

UDT_API inline int getpeername(UDTSOCKET u, struct sockaddr* name, int* namelen)
{
   return CUDT::getpeername(u, name, namelen);
}

UDT_API inline int getsockname(UDTSOCKET u, struct sockaddr* name, int* namelen)
{
   return CUDT::getsockname(u, name, namelen);
}

UDT_API inline int getsockopt(UDTSOCKET u, int level, SOCKOPT optname, void* optval, int* optlen)
{
   return CUDT::getsockopt(u, level, optname, optval, optlen);
}

UDT_API inline int setsockopt(UDTSOCKET u, int level, SOCKOPT optname, const void* optval, int optlen)
{
   return CUDT::setsockopt(u, level, optname, optval, optlen);
}

UDT_API inline int shutdown(UDTSOCKET u, int how)
{
   return CUDT::shutdown(u, how);
}

UDT_API inline int send(UDTSOCKET u, const char* buf, int len, int flags = 0, int* handle = NULL, UDT_MEM_ROUTINE routine = NULL)
{
   return CUDT::send(u, buf, len, flags, handle, routine);
}

UDT_API inline int recv(UDTSOCKET u, char* buf, int len, int flags = 0, int* handle = NULL, UDT_MEM_ROUTINE routine = NULL)
{
   return CUDT::recv(u, buf, len, flags, handle, routine);
}

UDT_API inline __int64 sendfile(UDTSOCKET u, ifstream& ifs, const __int64& offset, __int64& size, const int& block = 366000)
{
   return CUDT::sendfile(u, ifs, offset, size, block);
}

UDT_API inline __int64 recvfile(UDTSOCKET u, ofstream& ofs, const __int64& offset, __int64& size, const int& block = 7320000)
{
   return CUDT::recvfile(u, ofs, offset, size, block);
}

UDT_API inline bool getoverlappedresult(UDTSOCKET u, int handle, int& progress, bool wait = false)
{
   return CUDT::getoverlappedresult(u, handle, progress, wait);
}

UDT_API inline int select(int nfds, UDSET* readfds, UDSET* writefds, UDSET* exceptfds, const struct timeval* timeout)
{
   return CUDT::select(nfds, readfds, writefds, exceptfds, timeout);
}

UDT_API inline ERRORINFO getlasterror()
{
   return CUDT::getlasterror();
}

UDT_API inline int perfmon(UDTSOCKET u, TRACEINFO* perf, bool clear = true)
{
   return CUDT::perfmon(u, perf, clear);
}

}

#ifdef CAPI
class CSysLib
{
public:
   CSysLib();
   ~CSysLib();

private:
   void* m_pHSysSockLib;

public:
   int (*socket)(int, int, int);
   int (*bind)(int, const struct sockaddr*, unsigned int);
   int (*listen)(int, int);
   int (*accept)(int, struct sockaddr*, socklen_t*);
   int (*connect)(int, const struct sockaddr*, socklen_t);
   int (*close)(int);
   int (*shutdown)(int, int);
   ssize_t (*send)(int, const void*, unsigned int, int);
   ssize_t (*recv)(int, void*, unsigned int, int);
   ssize_t (*write)(int, const void*, size_t);
   ssize_t (*read)(int, void*, size_t);
   int (*writev)(int, const struct iovec*, size_t);
   int (*readv)(int, const struct iovec*, size_t);
   ssize_t (*sendfile)(int, int, off_t*, size_t);
   int (*getpeername)(int, struct sockaddr*, socklen_t*);
   int (*getsockname)(int, struct sockaddr*, socklen_t*);
   int (*getsockopt)(int, int, int, void*, socklen_t*);
   int (*setsockopt)(int, int, int, const void*, socklen_t);
   int (*fcntl)(int, int, ...);
   int (*select)(int, fd_set*, fd_set*, fd_set*, struct timeval*);
};

extern const CSysLib g_SysLib;

#endif

#endif
