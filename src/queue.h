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
This header file contains the definition of UDT multiplexer.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 07/16/2007
*****************************************************************************/


#ifndef __UDT_QUEUE_H__
#define __UDT_QUEUE_H__

#include "common.h"
#include "packet.h"
#include "channel.h"
#include <vector>

class CUDT;

struct CUnit
{
   CPacket m_Packet;		// packet
   int m_iFlag;			// 0: free, 1: occupied, 2: msg read but not freed (out-of-order), 3: msg dropped
};

class CUnitQueue
{
friend class CRcvQueue;
friend class CRcvBuffer;

public:
   CUnitQueue();
   ~CUnitQueue();

public:

      // Functionality:
      //    Initialize the unit queue.
      // Parameters:
      //    1) [in] size: queue size
      //    2) [in] mss: maximum segament size
      //    3) [in] version: IP version
      // Returned value:
      //    0: success, -1: failure.

   int init(const int& size, const int& mss, const int& version);

      // Functionality:
      //    Increase (double) the unit queue size.
      // Parameters:
      //    None.
      // Returned value:
      //    0: success, -1: failure.

   int increase();

      // Functionality:
      //    Decrease (halve) the unit queue size.
      // Parameters:
      //    None.
      // Returned value:
      //    0: success, -1: failure.

   int shrink();

      // Functionality:
      //    find an available unit for incoming packet.
      // Parameters:
      //    None.
      // Returned value:
      //    Pointer to the available unit, NULL if not found.

   CUnit* getNextAvailUnit();

private:
   struct CQEntry
   {
      CUnit* m_pUnit;		// unit queue
      char* m_pBuffer;		// data buffer
      int m_iSize;		// size of each queue

      CQEntry* m_pNext;
   }
   *m_pQEntry,			// pointer to the first unit queue
   *m_pCurrQueue,		// pointer to the current available queue
   *m_pLastQueue;		// pointer to the last unit queue

   CUnit* m_pAvailUnit;         // recent available unit

   int m_iSize;			// total size of the unit queue, in number of packets
   int m_iCount;		// total number of valid packets in the queue

   int m_iMSS;			// unit buffer size
   int m_iIPversion;		// IP version
};


struct CUDTList
{
   uint64_t m_llTimeStamp;	// Time Stamp
   int32_t m_iID;		// UDT Socket ID
   CUDT* m_pUDT;		// Pointer to the instance of CUDT socket

   CUDTList* m_pPrev;		// previous link
   CUDTList* m_pNext;		// next link
};


class CSndUList
{
friend class CSndQueue;

public:
   CSndUList();
   ~CSndUList();

public:

      // Functionality:
      //    Insert a new UDT instance into the list.
      // Parameters:
      //    1) [in] ts: time stamp: next processing time
      //    2) [in] id: socket ID
      //    3) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void insert(const int64_t& ts, const int32_t& id, const CUDT* u);

      // Functionality:
      //    Remove UDT instance from the list.
      // Parameters:
      //    1) [in] id: Socket ID
      // Returned value:
      //    None.

   void remove(const int32_t& id);

      // Functionality:
      //    Update the timestamp of the UDT instance on the list.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [in] u: pointer to the UDT instance
      //    3) [in] resechedule: if the timestampe shoudl be rescheduled
      // Returned value:
      //    None.

   void update(const int32_t& id, const CUDT* u, const bool& reschedule = true);

      // Functionality:
      //    Get and remove the first UDT instance on the list.
      // Parameters:
      //    1) [out] id: socket ID
      //    2) [out] u: pointer to the UDT instance
      // Returned value:
      //    UDT Socket ID if found one, otherwise -1.

   int pop(int32_t& id, CUDT*& u);

public:
   CUDTList* m_pUList;		// The head node

private:
   CUDTList* m_pLast;		// The last node

   pthread_mutex_t m_ListLock;

   pthread_mutex_t* m_pWindowLock;
   pthread_cond_t* m_pWindowCond;
};


class CRcvUList
{
public:
   CRcvUList();
   ~CRcvUList();

public:

      // Functionality:
      //    Insert a new UDT instance to the list.
      // Parameters:
      //    1) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void insert(const CUDT* u);

      // Functionality:
      //    Remove the UDT instance from the list.
      // Parameters:
      //    1) [in] id: socket ID
      // Returned value:
      //    None.

   void remove(const int32_t& id);

      // Functionality:
      //    Move the UDT instance to the end of the list, if it already exists; otherwise, do nothing.
      // Parameters:
      //    1) [in] id: socket ID
      // Returned value:
      //    None.

   void update(const int32_t& id);

      // Functionality:
      //    Insert a new UDT instance to the new entry list.
      // Parameters:
      //    1) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void newEntry(CUDT* u);

      // Functionality:
      //    Check if there is a new entry to be inserted to the rcv u list
      // Parameters:
      //    None.
      // Returned value:
      //    True if yes, otherwise false.

   bool ifNewEntry();

      // Functionality:
      //    Pick the first new entry on the waiting list.
      // Parameters:
      //    None.
      // Returned value:
      //    Pointer to a UDT instance.

   CUDT* newEntry();

public:
   CUDTList* m_pUList;		// the head node

private:
   CUDTList* m_pLast;		// the last node

   std::vector<CUDT*> m_vNewEntry;	// newly added entries, to be inserted
   pthread_mutex_t m_ListLock;
};

class CHash
{
public:
   CHash();
   ~CHash();

public:

      // Functionality:
      //    Initialize the hash table.
      // Parameters:
      //    1) [in] size: hash table size
      // Returned value:
      //    None.

   void init(const int& size);

      // Functionality:
      //    Look for a UDT instance from the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      // Returned value:
      //    Pointer to a UDT instance, or NULL if not found.

   CUDT* lookup(const int32_t& id);

      // Functionality:
      //    Retrive a received packet that is temporally stored in the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [out] packet: the returned packet
      // Returned value:
      //    Data length of the packet, or -1.

   int retrieve(const int32_t& id, CPacket& packet);

      // Functionality:
      //    Store a packet in the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [in] unit: information for the packet
      // Returned value:
      //    None.

   void setUnit(const int32_t& id, CUnit* unit);

      // Functionality:
      //    Insert an entry to the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void insert(const int32_t& id, const CUDT* u);

      // Functionality:
      //    Remove an entry from the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      // Returned value:
      //    None.

   void remove(const int32_t& id);

private:
   struct CBucket
   {
      int32_t m_iID;		// Socket ID
      CUDT* m_pUDT;		// Socket instance

      CBucket* m_pNext;		// next bucket

      CUnit* m_pUnit;		// tempory buffer for a received packet
   } **m_pBucket;		// list of buckets (the hash table)

   int m_iHashSize;		// size of hash table
};

class CRendezvousQueue
{
public:
   CRendezvousQueue();
   ~CRendezvousQueue();

public:
   void insert(const UDTSOCKET& id, const int& ipv, const sockaddr* addr);
   void remove(const UDTSOCKET& id);
   bool retrieve(const sockaddr* addr, UDTSOCKET& id, const UDTSOCKET& peerid);

private:
   struct CRL
   {
      UDTSOCKET m_iID;
      UDTSOCKET m_iPeerID;
      int m_iIPversion;
      sockaddr* m_pPeerAddr;
   };
   std::vector<CRL> m_vRendezvousID;         // The sockets currently in rendezvous mode

   pthread_mutex_t m_RIDVectorLock;
};

class CSndQueue
{
friend class CUDT;
friend class CUDTUnited;

public:
   CSndQueue();
   ~CSndQueue();

public:

      // Functionality:
      //    Initialize the sending queue.
      // Parameters:
      //    1) [in] c: UDP channel to be associated to the queue
      //    2) [in] t: Timer
      // Returned value:
      //    None.

   void init(const CChannel* c, const CTimer* t);

      // Functionality:
      //    Send out a packet to a given address.
      // Parameters:
      //    1) [in] addr: destination address
      //    2) [in] packet: packet to be sent out
      // Returned value:
      //    Size of data sent out.

   int sendto(const sockaddr* addr, CPacket& packet);

private:
#ifndef WIN32
   static void* worker(void* param);
#else
   static DWORD WINAPI worker(LPVOID param);
#endif

   pthread_t m_WorkerThread;

private:
   CSndUList* m_pSndUList;		// List of UDT instances for data sending
   CChannel* m_pChannel;                // The UDP channel for data sending
   CTimer* m_pTimer;			// Timing facility

   pthread_mutex_t m_WindowLock;
   pthread_cond_t m_WindowCond;

   volatile bool m_bClosing;		// closing the worker
};


class CRcvQueue
{
friend class CUDT;
friend class CUDTUnited;

public:
   CRcvQueue();
   ~CRcvQueue();

public:

      // Functionality:
      //    Initialize the receiving queue.
      // Parameters:
      //    1) [in] size: queue size
      //    2) [in] mss: maximum packet size
      //    3) [in] version: IP version
      //    4) [in] hsize: hash table size
      //    5) [in] c: UDP channel to be associated to the queue
      //    6) [in] t: timer
      // Returned value:
      //    None.

   void init(const int& size, const int& payload, const int& version, const int& hsize, const CChannel* c, const CTimer* t);

      // Functionality:
      //    Read a packet for a specific UDT socket id.
      // Parameters:
      //    1) [in] id: Socket ID
      //    2) [out] packet: received packet
      // Returned value:
      //    Data size of the packet

   int recvfrom(const int32_t& id, CPacket& packet);

private:
#ifndef WIN32
   static void* worker(void* param);
#else
   static DWORD WINAPI worker(LPVOID param);
#endif

   pthread_t m_WorkerThread;

private:
   CUnitQueue m_UnitQueue;	// The received packet queue

   CRcvUList* m_pRcvUList;	// List of UDT instances that will read packets from the queue
   CHash* m_pHash;		// Hash table for UDT socket looking up
   CChannel* m_pChannel;	// UDP channel for receving packets
   CTimer* m_pTimer;		// shared timer with the snd queue

   pthread_mutex_t m_PassLock;
   pthread_cond_t m_PassCond;

   volatile UDTSOCKET m_ListenerID;		// The only listening socket that is associated to the queue, if there is one
   CRendezvousQueue* m_pRendezvousQueue;	// The list of sockets in rendezvous mode

   int m_iPayloadSize;			// packet payload size

   volatile bool m_bClosing;		// closing the workder
};


class CMultiplexer
{
public:
   CSndQueue* m_pSndQueue;	// The sending queue
   CRcvQueue* m_pRcvQueue;	// The receiving queue
   CChannel* m_pChannel;	// The UDP channel for sending and receiving
   CTimer* m_pTimer;		// The timer

   int m_iPort;			// The UDP port number of this multiplexer
   int m_iIPversion;		// IP version
   int m_iMTU;			// MTU
   int m_iRefCount;		// number of UDT instances that are associated with this multiplexer
   bool m_bReusable;		// if this one can be shared with others
};

#endif
