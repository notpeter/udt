/******************************************************************************
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
This is the (only) header file of UDT library and for programming with UDT.

System specific types are defined here.

Data structures list:

CHandshake:	Handshake Information
UDTOpt:		UDT Options
CTimer:		Timer Facility
CUDTException:	Exception Handling Facility
CACKWindow:	ACK History Window
CPktTimeWindow:	Packet time/delay history window
CPacket:	Packet Definition
CChannel:	UDP Transport Channel
CList:		Loss Lists and Irregular Packet List
		CSndLossList
		CRcvLossList
		CIrregularLossPktList
CSndBuffer:	Sending Buffer Management 
CRcvBuffer:	Receiving Buffer Management
CUDT: 		UDT
*****************************************************************************/

/*****************************************************************************
written by:
   Yunhong Gu [ygu@cs.uic.edu], last updated 01/20/2003
*****************************************************************************/


#ifndef _UDT_H_
#define _UDT_H_


#ifndef WIN32
   #include <pthread.h>
   #include <sys/time.h>
   #include <sys/uio.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <stdio.h>
#else
   #include <windows.h>
#endif

#include <fstream> 
using namespace std;


#ifndef WIN32
   // Explicitly define 32-bit and 64-bit numbers
   #define __int32 int
   #define __int64 long long
#else
   // Windows compability
   typedef HANDLE pthread_t;
   typedef HANDLE pthread_mutex_t;
   typedef HANDLE pthread_cond_t;

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

   int UDT_API gettimeofday(timeval *tv, void*);
   int readv(SOCKET s, const iovec* vector, int count);
   int writev(SOCKET s, const iovec* vector, int count);
#endif


////////////////////////////////////////////////////////////////////////////////
struct CHandShake
{
   __int32 m_iVersion;		// UDT version
   __int32 m_iISN;		// random initial sequence number
   __int32 m_iMTU;		// MTU setting
   __int32 m_iFlightFlagSize;	// Flow control window size 
};


////////////////////////////////////////////////////////////////////////////////
enum UDTOpt
{
   UDT_ADDR,            // IP Address
   UDT_PORT,            // the request UDT port number
   UDT_PCH,             // if the request port can be changed
   UDT_MTU,             // the Maximum Transfer Unit
   UDT_SNDSYN,          // if sending is blocking
   UDT_RCVSYN,          // if receiving is blocking
   UDT_MFLAG,           // how UDT deal with the sent buffer
   UDT_RC,              // Rate control algorithm option
   UDT_FC, 	        // maximum allowed number of unacknowledged packets (Flow Control)
   UDT_BUF,		// UDT receiving buffer size
   UDT_USB,             // UDP sending buffer size
   UDT_URB,             // UDP receiving buffer size
   UDT_IPV,             // IP version [IPv4/IPv6]
   UDT_MC,		// multicast
   UDT_MCADDR,		// multicast address
   UDT_MCPORT,		// multicast port
   UDT_LOSSY		// unreliable option
};


////////////////////////////////////////////////////////////////////////////////
class CTimer
{
public:

      // Functionality:
      //    Read the CPU clock cycle into x.
      // Parameters:
      //    0) [out] x: to record cpu clock cycles.
      // Returned value:
      //    None.

   void rdtsc(unsigned __int64 &x) const;

      // Functionality:
      //    Read the CPU frequency.
      // Parameters:
      //    None.
      // Returned value:
      //    CPU frequency.

   unsigned __int64 getCPUFrequency() const;

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

private:
   unsigned __int64 m_ullSchedTime;
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
   CPktTimeWindow(const __int32& size);
   ~CPktTimeWindow();

      // Functionality:
      //    Calculate the packes arrival speed.
      // Parameters:
      //    0) None.
      // Returned value:
      //    Packet arrival speed (packets per second).

   __int32 getPktSpeed() const;

      // Functionality:
      //    Check if the rtt is increasing or not.
      // Parameters:
      //    0) None.
      // Returned value:
      //    true is RTT is increasing, otherwise false.

   bool getDelayTrend() const;

      // Functionality:
      //    Estimate the bandwidth.
      // Parameters:
      //    0) None.
      // Returned value:
      //    Estimated bandwidth (packets per second).

   __int32 getBandwidth() const;

      // Functionality:
      //    Record time information of an arrived packet.
      // Parameters:
      //    0) None.
      // Returned value:
      //    None.

   void pktArrival();

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
      //    0) None.
      // Returned value:
      //    None.

   void probe1Arrival();

      // Functionality:
      //    Record the arrival time of the second probing packet and the interval between packet pairs.
      // Parameters:
      //    0) None.
      // Returned value:
      //    None.

   void probe2Arrival();

private:
   __int32 m_iSize;		// size of the history window

   __int32* m_piPktWindow;	// packet information window
   __int32 m_iPktWindowPtr;	// position pointer of the packet info. window.

   __int32* m_piRTTWindow;	// RTT history window
   __int32* m_piPCTWindow;	// PCT (pairwise comparison test) history window
   __int32* m_piPDTWindow;	// PDT (pairwise difference test) history window
   __int32 m_iRTTWindowPtr;	// position pointer to the 3 windows above

   __int32* m_piProbeWindow;	// record inter-packet time for probing packet pairs
   __int32 m_iProbeWindowPtr;	// position pointer to the probing window

   timeval m_LastArrTime;	// last packet arrival time
   timeval m_CurrArrTime;	// current packet arrival time
   timeval m_ProbeTime;		// arrival time of the first probing packet
};


////////////////////////////////////////////////////////////////////////////////
#ifndef WIN32
class CUDTException
#else
class UDT_API CUDTException
#endif
{
public:
   CUDTException(__int32 major = 0, __int32 minor = 0, __int32 err = -1);
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

   virtual const __int32& getErrorCode() const;

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
   char*& m_pcData;		// alias: data/control information

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
      //    Pack a DATA packet.
      // Parameters:
      //    0) [in] seqno: sequence number of the packet.
      //    1) [in] data: pointer to the payload.
      //    2) [in] size: payload size.
      // Returned value:
      //    None.

   void pack(const __int32& seqno, const char* data, const __int32& size);

      // Functionality:
      //    Pack a Control packet.
      // Parameters:
      //    0) [in] pkttype: packet type filed.
      //    1) [in] lparam: pointer to the first data structure, explained by the packet type.
      //    2) [in] rparam: pointer to the second data structure, explained by the packet type.
      // Returned value:
      //    None.

   void pack(const __int32& pkttype, void* lparam = NULL, void* rparam = NULL);

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
      //    Read the loss length of the NAK packet.
      // Parameters:
      //    None.
      // Returned value:
      //    packet header field (bit 16~31).

   __int32 getLossLength() const;

      // Functionality:
      //    Read the ACK-2 seq. no.
      // Parameters:
      //    None.
      // Returned value:
      //    packet header field (bit 16~31).

   __int32 getAckSeqNo() const;

private:
   unsigned __int32 m_nHeader;	// The 32-bit header field
   iovec m_PacketVector[2];	// The 2-demension vector of UDT packet [header, data]
};


////////////////////////////////////////////////////////////////////////////////
class CChannel
{
public:
   CChannel();
   ~CChannel();

      // Functionality:
      //    Opne a UDP channel.
      // Parameters:
      //    0) [in] port: expected port number that the UDP is bound to.
      //    1) [in] ch: if the "port" number is changable.
      //    2) [in] ip: The IP address that UDP will uses in a multi-ip host.
      // Returned value:
      //    None.

   void open(__int32& port, const bool& ch = true, const char* ip = NULL);

      // Functionality:
      //    Open an IPv6 UDP channel.
      // Parameters:
      //    0) [in] port: expected port number that the UDP is bound to.
      //    1) [in] ch: if the "port" number is changable.
      //    2) [in] ip: The IP address that UDP will uses in a multi-ip host.
      // Returned value:
      //    None.

   void open6(__int32& port, const bool& ch = true, const char* ip = NULL);

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
      //    0) [in] buffer: pointer to the buffer to write received data.
      //    1) [in] size: size of the expected data received.
      // Returned value:
      //    Actual size of data received.

   __int32 recv(char* buffer, const __int32& size) const;

      // Functionality:
      //    Read the data from the channel but the data is not removed from UDP buffer.
      // Parameters:
      //    0) [in] buffer: pointer to the buffer to write received data.
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
      //    Receive a packet from the channel and record the source address.
      // Parameters:
      //    0) [in] packet: reference to a CPacket entity.
      //    1) [in] addr: pointer to the source address.
      // Returned value:
      //    Actual size of data received.

   __int32 recvfrom(CPacket& packet, sockaddr* addr) const;

      // Functionality:
      //    Bind the local UDP entity to the peer side for sending/receiving.
      // Parameters:
      //    0) [in] ip: Peer side IP address.
      //    1) [in] port: Peer side UDP port.
      // Returned value:
      //    None.

   void connect(const char* ip, const __int32& port);

      // Functionality:
      //    Bind the local UDP entity to the peer side for sending/receiving, using IPv6.
      // Parameters:
      //    0) [in] ip: Peer side IP address.
      //    1) [in] port: Peer side UDP port.
      // Returned value:
      //    None.

   void connect6(const char* ip, const __int32& port);

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
      //    Add the curreny UDP entity to the multicast group of "ip".
      // Parameters:
      //    0) [in] ip: Multicast address.
      // Returned value:
      //    None.

   void addMembership(const char* mcip);

      // Functionality:
      //    Add the curreny IPv6 UDP entity to the multicast group of "ip".
      // Parameters:
      //    0) [in] ip: IPv6 Multicast address.
      // Returned value:
      //    None.

   void joinGroup(const char* mcip);

      // Functionality:
      //    Query the IP address that the channel is using.
      // Parameters:
      //    0) [in] ip: address for returned IP address.
      // Returned value:
      //    None.

   void getAddr(unsigned char* ip) const;

      // Functionality:
      //    Query the IPv6 address that the channel is using.
      // Parameters:
      //    0) [in] ip: address for returned IP address.
      // Returned value:
      //    None.

   void getAddr6(unsigned char* ip) const;

private:
   #ifndef WIN32
      __int32 m_iSocket;		// socket descriptor
   #else
      SOCKET m_iSocket;
   #endif

   __int32 m_iSndBufSize;		// UDP sending buffer size
   __int32 m_iRcvBufSize;		// UDP receiving buffer size

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

   pthread_mutex_t m_ListLock;          // muext for link processing
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
      //    None.

   void remove(const __int32& seqno);

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
      //    0) [in] array: pointer to the result array.
      //    1) [out] physical length of the result array.
      //    2) [in] limit: maximum length of the array.
      //    3) [in] interval: Time threshold from last NAK report.
      // Returned value:
      //    None.

   void getLossArray(__int32* array, __int32* len, const __int32& limit, const __int32& interval);

private:
   __int32* m_piData1;			// sequence number starts
   __int32* m_piData2;			// sequence number ends
   timeval* m_pLastFeedbackTime;	// last feedback time of the node
   __int32* m_piCount;			// report counter
   __int32* m_piNext;			// next node in the list

   __int32 m_iHead;			// first node in the list
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
      //    Read the total size error.
      // Parameters:
      //    None.
      // Returned value:
      //    the total size error.

   __int32 currErrorSize() const;

      // Functionality:
      //    Read the total size error of all the irregular packets prior to "seqno".
      // Parameters:
      //    0) [in] seqno: sequence number.
      // Returned value:
      //    the total size error of all the irregular packets prior to "seqno".

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
   __int32 m_iLength;			// loss length
   __int32 m_iCurrErrSize;              // total size error for the list

   __int32 m_iSize;			// size of the static array
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
      //    2) [in] flag: how to deal with the buffer after it is sent.
      // Returned value:
      //    None.

   void addBuffer(const char* data, const __int32& len, const __int32& flag = 1);

      // Functionality:
      //    Find data position to pack a DATA packet from the furthest reading point.
      // Parameters:
      //    0) [in] data: pointer to the pointer of the data position.
      //    1) [in] len: Expected data length.
      // Returned value:
      //    Actual length of data read.

   __int32 readData(char** data, const __int32& len);

      // Functionality:
      //    Find data position to pack a DATA packet for a retransmission.
      // Parameters:
      //    0) [in] data: pointer to the pointer of the data position.
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

private:
   pthread_mutex_t m_BufLock;

   struct Block
   {
      char* m_pcData;			// pointer to the data block
      __int32 m_iLength;		// length of the block
      __int32 m_iFlag;			// how to process the block after sending: 

      // 0: return to application (leave it along)	
      // 1: delete
      // 2: munmap

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
      //    0) [in] data: pointer of pointer to the next data position.
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
      //    0) [in] data: pointer to the user buffer.
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
      //    0) buf: pointer to the user buffer.
      //    1) len: size of the user buffer.
      // Returned value:
      //    Size of data that has been received by now.

   __int32 registerUserBuf(char* buf, const __int32& len);

      // Functionality:
      //    Query how many data has been received into user buffer.
      // Parameters:
      //    None.
      // Returned value:
      //    Size of valid data in user buffer; or 0 if no user buffer presented.

   __int32 getCurUserBufSize() const;

private:
   char* m_pcData;			// pointer to the protocol buffer
   __int32 m_iSize;			// size of the protocol buffer

   __int32 m_iStartPos;			// the head position for I/O
   __int32 m_iLastAckPos;		// the last ACKed position
   __int32 m_iMaxOffset;		// the furthest "dirty" position

   char* m_pcUserBuf;			// pointer to the user registered buffer
   __int32 m_iUserBufSize;		// size of the user buffer
   __int32 m_iUserBufAck;		// last ACKed position of the user buffer
};


//////////////////////////////////////////////////////////////////////////////
#ifndef WIN32
class CUDT
#else
class UDT_API CUDT
#endif
{
public:
   CUDT();
   ~CUDT();

      // Functionality: 
      //    open a UDT entity and bound to a local port.
      // Parameters: 
      //    0) [in] port: the port number to be bound to.
      // Returned value:
      //    the actual port number the UDT entity is bound to.

   __int32 open(const __int32& port = 0);

      // Functionality:
      //    open a UDT entity and bound to a local port and a specified IP address.
      // Parameters:
      //    0) [in] ip: the local IP address that UDT must uses.
      //    0) [in] port: the port number to be bound to.
      // Returned value:
      //    the actual port number the UDT entity is bound to.

   __int32 open(const char* ip, const __int32& port = 0);

      // Functionality:
      //    Listen and wait for a UDT entity to connect.
      // Parameters:
      //    0) [in] timeo: Timer out value for connection, in microseconds.
      // Returned value:
      //    None.

   void listen(const __int64& timeo = 0);

      // Functionality:
      //    Connect to a UDT entity listenning at "ip":"port".
      // Parameters:
      //    0) [in] ip: The IP address of the listening UDT entity.
      //    1) [in] port: The port number to connect to.
      //    2) [in] timeo: Timer out value for connection, in microseconds.
      // Returned value:
      //    None.

   void connect(const char* ip, const __int32& port, const __int64& timeo = 0);

      // Functionality:
      //    Close the opened UDT entity. Closed UDT entity can be opened again.
      // Parameters:
      //    0) [in] wait: close immediately (WAIT_NONE, default), do not close until all data are sent (WAIT_SEND),
      //                  do not close until all data are received (WAIT_RECV), or mix of WAIT_SEND and WAIT_RECV (WAIT_ALL).
      // Returned value:
      //    None.

   enum CL_STATUS {WAIT_NONE, WAIT_SEND, WAIT_RECV, WAIT_ALL};
   void close(const CL_STATUS& wait = WAIT_NONE);

      // Functionality:
      //    Request UDT to send out a data block "data" with size of "len".
      // Parameters:
      //    0) [in] data: The address of the application data to be sent.
      //    1) [in] len: The size of the data block.
      // Returned value:
      //    None.

   __int32 send(char* data, const __int32& len);

      // Functionality:
      //    Request UDT to receive data to a memory block "data" with size of "len".
      // Parameters:
      //    0) [in] data: The address of the application data to be received.
      //    1) [in] len: The desired size of data to be received.
      // Returned value:
      //    Actual size of data that has been sent.

   __int32 recv(char* data, const __int32& len);

      // Functionality:
      //    Request UDT to send out a file described as "fd", starting from "offset", with size of "size".
      // Parameters:
      //    0) [in] ifs: The input file stream.
      //    1) [in] offset: From where to read and send data;
      //    2) [in] size: How many data to be sent.
      // Returned value:
      //    Actual size of received data.

   __int64 sendfile(ifstream& ifs, const __int64& offset, const __int64& size);

      // Functionality:
      //    Request UDT to receive data into a file described as "fd", starting from "offset", with expected size of "size".
      // Parameters:
      //    0) [in] ofs: The output file stream.
      //    1) [in] offset: From where to write data;
      //    2) [in] size: How many data to be received.
      // Returned value:
      //    Actual size of data that has been sent.

   __int64 recvfile(ofstream& ofs, const __int64& offset, const __int64& size);

      // Functionality:
      //    Configure UDT options.
      // Parameters:
      //    0) [in] optName: The enum name of a UDT option.
      //    1) [in] optval: The value to be set.
      //    2) [in] optlen: size of "optval".
      // Returned value:
      //    Actual size of received data.

   void setOpt(UDTOpt optName, const void* optval, const __int32& optlen);

      // Functionality:
      //    Read UDT options.
      // Parameters:
      //    0) [in] optName: The enum name of a UDT option.
      //    1) [in] optval: The value to be returned.
      //    2) [out] optlen: size of "optval".
      // Returned value:
      //    The value of the current configuraton of the option.

   void getOpt(UDTOpt optName, void* optval, __int32& optlen);

      // Functionality:
      //    Query the current sending buffer size.
      // Parameters:
      //    None.
      // Returned value:
      //    The size of data in the sending buffer.

   __int32 getCurrSndBufSize();

#ifdef TRACE
      // Functionality:
      //    Trace the UDT performance. 
      // Parameters:
      //    0) [in] interval: trace interval, in microseconds.
      //    1) [in] tracefile: output file.
      // Returned value:
      //    None

   void trace(const __int32& interval = 1000000, const char* tracefile = NULL);
#endif

private: // Version
   const __int32 m_iVersion;

private: // Threads, data channel, and timing facility
   pthread_t m_SndThread;			// Sending thread
   pthread_t m_RcvThread;			// Receiving thread
   CChannel* m_pChannel;			// UDP channel
   CTimer* m_pTimer;				// Timing facility
   unsigned __int64 m_ullCPUFrequency;		// CPU clock frequency, used for Timer

private: // Timing intervals
   const __int32 m_iSYNInterval;                // Periodical Rate Control Interval, 10 microseconds

private: // Packet size and sequence number attributes
   __int32 m_iPktSize;				// Maximum/regular packet size, in bytes
   __int32 m_iPayloadSize;			// Maximum/regular payload size, in bytes
   const __int32 m_iMaxSeqNo;			// Maximum data sequence number
   const __int32 m_iSeqNoTH;			// The threshold used to compare 2 sequence numbers
   const __int32 m_iMaxAckSeqNo;		// Maximum ACK sequence number

private: // Options
   char* m_pcIP;				// IP address
   __int32 m_iPort;				// Port number
   bool m_bPortChangable;			// If the request port number is changable
   __int32 m_iMTU;				// MTU
   bool m_bSynSending;				// Sending syncronization mode
   bool m_bSynRecving;				// Receiving syncronization mode
   __int32 m_iMemFlag;				// How UDT deal with the sent buffer (0: return; 1: release; 2:munmap)
   __int32 m_iRCAlg;				// Rate control algorithm
   __int32 m_iFlightFlagSize;			// Maximum number of packets in flight from the peer side
   __int32 m_iUDTBufSize;			// UDT buffer size (for receiving)
   __int32 m_iUDPSndBufSize;			// UDP sending buffer size
   __int32 m_iUDPRcvBufSize;			// UDP receiving buffer size
   __int32 m_iIPversion;			// IP version
   bool m_bMulticast;				// Multicast
   char m_pcMCIP[40];				// Multicast IP address
   __int32 m_iMCPort;				// Multicast port number
   bool m_bLossy;				// Unreliable option

   const __int32 m_iProbeInterval;		// Numbers of regular packets between two probing packet pairs

private: // Status
   volatile bool m_bConnected;			// Whether the connection is on or off
   volatile bool m_bClosing;			// If the UDT entity is closing
   volatile bool m_bShutdown;			// If the peer side has shutdown the connection
   bool m_bOpened;				// If the UDT entity has been opened
   bool m_bInitiator;				// If the UDT entity initilize the connection
   bool m_bSlowStart;				// If UDT is during slow start phase
   bool m_bFreeze;				// freeze the data sending
   __int32 m_iEXPCount;				// Expiration counter
   __int32 m_iBandwidth;			// Estimated bandwidth

   pthread_mutex_t m_ShutdownLock;

private: // Sending related data
   CSndBuffer* m_pSndBuffer;			// Sender buffer
   CSndLossList* m_pSndLossList;		// Sender loss list

   pthread_cond_t m_SendDataCond;		
   pthread_mutex_t m_SendDataLock;

   pthread_cond_t m_SendBlockCond;
   pthread_mutex_t m_SendBlockLock;

   pthread_mutex_t m_AckLock;

   volatile unsigned __int64 m_ullInterval;	// Inter-packet time, in CPU clock cycles

   __int32 m_iNAKCount;				// Number of NAK received since last SYN
   volatile __int32 m_iSndLastAck;		// Last ACK received
   __int32 m_iLocalSend;			// Number of packets sent since last SYN
   __int32 m_iLocalLoss;			// Number of packet loss since last SYN
   __int32 m_iSndCurrSeqNo;			// The largest sequence number that has been sent

   const double m_dLossRateLimit;		// The upper limit of packet loss that is allowed
   const double m_dWeight;			// Factor for EWMA filter of loss rate
   double m_dLossRate;				// EWMA loss rate

   __int32 m_iLastDecSeq;			// Sequence number sent last decrease occurs
   __int32 m_iDecCount;				// Number of sending rate decrease

   __int32 m_iISN;				// Initial Sequence Number

   __int32 m_iFlowWindowSize;                   // Flow control window size
   __int32 m_iMaxFlowWindowSize;		// Maximum flow window size = flight flag size of the peer side

private: // Receiving related data
   CRcvBuffer* m_pRcvBuffer;			// Receiver buffer
   CRcvLossList* m_pRcvLossList;		// Receiver loss list
   CIrregularPktList* m_pIrrPktList;		// Irregular sized packet list
   CACKWindow* m_pACKWindow;			// ACK history window
   CPktTimeWindow* m_pRcvTimeWindow;		// Packet arrival time window

   __int32 m_iRTT;				// RTT

   __int32 m_iRcvLastAck;			// Last sent ACK
   unsigned __int64 m_ullLastAckTime;		// Timestamp of last ACK
   __int32 m_iRcvLastAckAck;			// Last sent ACK that has been acknowledged
   __int32 m_iAckSeqNo;				// Last ACK sequence number
   __int32 m_iRcvCurrSeqNo;			// Largest received sequence number
   __int32 m_iNextExpect;			// Sequence number of next speculated packet to receive
   __int32 m_iLocalRecv;			// Number of packets received since last SYN

   pthread_cond_t m_RecvDataCond;
   pthread_mutex_t m_RecvDataLock;

   volatile bool m_bReadBuf;			// Application has called "recv" but has not finished
   volatile char* m_pcTempData;			// Pointer to the buffer that application want to put received data into
   volatile __int32 m_iTempLen;			// Size of the "m_pcTempData"

   __int32 m_iUserBufBorder;			// Sequence number of last packet that will fulfill a user buffer

   unsigned __int64 m_ullLastWarningTime;	// Last time that a warning message is sent

private: // Thread-safe protectors
   pthread_mutex_t m_SendLock;
   pthread_mutex_t m_RecvLock;

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
   void sendCtrl(const __int32& pkttype, void* lparam = NULL, void* rparam = NULL);
   void processCtrl(CPacket& ctrlpkt);

#ifdef TRACE
private: // Trace
   bool m_bTraceEnabled;			// flag: if trace() has been called previously
   __int32 m_iTraceInterval;			// trace interval, in microseconds
   FILE* m_TraceOutput;				// trace output file

   __int32 m_iTraceSend;			// total number of pakctes sent in the last trace interval
   __int32 m_iTraceRecv;			// total number of pakctes received in the last trace interval
   __int32 m_iSentACK;				// total number of ACKs sent in the last trace interval
   __int32 m_iRecvACK;				// total number of ACKs received in the last trace interval
   __int32 m_iSentNAK;				// total number of NAKs sent in the last trace interval
   __int32 m_iRecvNAK;				// total number of NAKs received in the last trace interval
   __int32 m_iTraceLoss;			// total number of lost packets in the last trace interval

   timeval m_LastSampleTime;			// last trace output time

   void sample();
#endif
};

#endif
