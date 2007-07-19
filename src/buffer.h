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
This header file contains the definition of UDT buffer structure and operations.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 04/08/2007
*****************************************************************************/

#ifndef __UDT_BUFFER_H__
#define __UDT_BUFFER_H__


#include "udt.h"
#include "list.h"
#include "queue.h"
#include <fstream>

class CSndBuffer
{
public:
   CSndBuffer(const int& mss);
   ~CSndBuffer();

      // Functionality:
      //    Insert a user buffer into the sending list.
      // Parameters:
      //    0) [in] data: pointer to the user data block.
      //    1) [in] len: size of the block.
      //    5) [in] ttl: time to live in milliseconds
      //    6) [in] seqno: sequence number of the first packet in the block, for DGRAM only
      //    7) [in] order: if the block should be delivered in order, for DGRAM only
      // Returned value:
      //    None.

   void addBuffer(const char* data, const int& len, const int& ttl = -1, const int32_t& seqno = 0, const bool& order = false);

      // Functionality:
      //    Find data position to pack a DATA packet from the furthest reading point.
      // Parameters:
      //    0) [out] data: the pointer to the data position.
      //    1) [in] len: Expected data length.
      //    2) [out] msgno: message number of the packet.
      // Returned value:
      //    Actual length of data read.

   int readData(char** data, const int& len, int32_t& msgno);

      // Functionality:
      //    Find data position to pack a DATA packet for a retransmission.
      // Parameters:
      //    0) [out] data: the pointer to the data position.
      //    1) [in] offset: offset from the last ACK point.
      //    2) [in] len: Expected data length.
      //    3) [out] msgno: message number of the packet.
      //    4) [out] seqno: sequence number of the first packet in the message
      //    5) [out] msglen: length of the message
      // Returned value:
      //    Actual length of data read.

   int readData(char** data, const int offset, const int& len, int32_t& msgno, int32_t& seqno, int& msglen);

      // Functionality:
      //    Update the ACK point and may release/unmap/return the user data according to the flag.
      // Parameters:
      //    0) [in] len: size of data acknowledged.
      //    1) [in] payloadsize: regular payload size that UDT always try to read.
      // Returned value:
      //    None.

   void ackData(const int& len, const int& payloadsize);

      // Functionality:
      //    Read size of data still in the sending list.
      // Parameters:
      //    None.
      // Returned value:
      //    Current size of the data in the sending list.

   int getCurrBufSize() const;

private:
   pthread_mutex_t m_BufLock;           // used to synchronize buffer operation

   struct Block
   {
      char* m_pcData;                   // pointer to the data block
      int m_iLength;                    // length of the block

      uint64_t m_OriginTime;            // original request time
      int m_iTTL;                       // time to live
      int32_t m_iMsgNo;                 // message number
      int32_t m_iSeqNo;                 // sequence number of first packet
      int m_iInOrder;                   // flag indicating if the block should be delivered in order

      Block* m_next;                    // next block
   } *m_pBlock, *m_pLastBlock, *m_pCurrSendBlk, *m_pCurrAckBlk;

   // m_pBlock:         The first block
   // m_pLastBlock:     The last block
   // m_pCurrSendBlk:   The block contains the data with the largest seq. no. that has been sent
   // m_pCurrAckBlk:    The block contains the data with the latest ACK (= m_pBlock)

   int m_iCurrBufSize;                  // Total size of the blocks
   int m_iCurrSendPnt;                  // pointer to the data with the largest current seq. no.
   int m_iCurrAckPnt;                   // pointer to the data with the latest ACK

   int32_t m_iNextMsgNo;                // next message number

   int m_iMSS;                          // maximum seqment/packet size
};

////////////////////////////////////////////////////////////////////////////////

class CRcvBuffer
{
public:
   CRcvBuffer(CUnitQueue* queue);
   CRcvBuffer(const int& bufsize, CUnitQueue* queue);
   ~CRcvBuffer();

      // Functionality:
      //    Write data into the buffer.
      // Parameters:
      //    0) [in] unit: pointer to a data unit containing new packet
      //    1) [in] offset: offset from last ACK point.
      // Returned value:
      //    0 is success, -1 if data is repeated.

   int addData(CUnit* unit, int offset);

      // Functionality:
      //    Read data into a user buffer.
      // Parameters:
      //    0) [in] data: pointer to user buffer.
      //    1) [in] len: length of user buffer.
      // Returned value:
      //    size of data read.

   int readBuffer(char* data, const int& len);

      // Functionality:
      //    Read data directly into file.
      // Parameters:
      //    0) [in] file: C++ file stream.
      //    1) [in] len: expected length of data to write into the file.
      // Returned value:
      //    size of data read.

   int readBufferToFile(std::ofstream& file, const int& len);

      // Functionality:
      //    Update the ACK point of the buffer.
      // Parameters:
      //    0) [in] len: size of data to be acknowledged.
      // Returned value:
      //    1 if a user buffer is fulfilled, otherwise 0.

   void ackData(const int& len);

      // Functionality:
      //    Query how many buffer space left for data receiving.
      // Parameters:
      //    None.
      // Returned value:
      //    size of available buffer space (including user buffer) for data receiving.

   int getAvailBufSize() const;

      // Functionality:
      //    Query how many data has been continuously received (for reading).
      // Parameters:
      //    None.
      // Returned value:
      //    size of valid (continous) data for reading.

   int getRcvDataSize() const;

      // Functionality:
      //    mark the message to be dropped from the message list.
      // Parameters:
      //    0) [in] msgno: message nuumer.
      // Returned value:
      //    None.

   void dropMsg(const int32_t& msgno);

      // Functionality:
      //    read a message.
      // Parameters:
      //    0) [out] data: buffer to write the message into.
      //    1) [in] len: size of the buffer.
      // Returned value:
      //    actuall size of data read.

   int readMsg(char* data, const int& len);

private:
   CUnit** m_pUnit;                     // pointer to the protocol buffer
   int m_iSize;                         // size of the protocol buffer
   CUnitQueue* m_pUnitQueue;		// the shared unit queue

   int m_iStartPos;                     // the head position for I/O (inclusive)
   int m_iLastAckPos;                   // the last ACKed position (exclusive)
					// EMPTY: m_iStartPos = m_iLastAckPos   FULL: m_iStartPos = m_iLastAckPos + 1
   int m_iMaxPos;			// the furthest data position

   int m_iNotch;			// the starting read point of the first unit
};


#endif
