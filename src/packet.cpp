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
This file contains the implementation of UDT packet handling modules.

A UDT packet is a 2-dimension vector of packet header and data.
*****************************************************************************/

/*****************************************************************************
written by 
   Yunhong Gu [ygu@cs.uic.edu], last updated 12/16/2003
*****************************************************************************/


//////////////////////////////////////////////////////////////////////////////
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                        Packet Header                          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                                                               |
//   ~              Data / Control Information Field                 ~
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|                         Sequence Number                     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   bit 0:
//      0: Data Packet
//      1: Control Packet
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |1|type |        Reserved       |   Loss Length / ACK Seq. No.  |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   bit 1-3:
//      000: Protocol Connection Handshake
//              bits 16-31:   Undefined
//              Control Info: Handshake information (see CHandShake)
//      001: Keep-alive
//              bits 16-31:   Undefined
//              Control Info: None
//      010: Acknowledgement (ACK)
//              bits 16-31:   Sequence number of the ACK
//              Control Info: The sequence number to which (but not include) all the previous packets have beed received
//                            RTT
//                            the packets receving rate
//                            estimated bandwidth
//      011: Negative Acknowledgement (NAK)
//              bits 16-31:   Number of the loss carried in the packet
//              Control Info: Loss list (see loss list coding below)
//      100: Congestion Warning
//              bits 16-31:   Undefined
//              Control Info: None
//      101: Shutdown
//              bits 16-31:   Undefined
//              Control Info: None
//      110: Acknowledgement of Acknowledement (ACK-square)
//              bits 16-31:   The ACK sequence number with which the ACK packet is received.
//              Control Info: None
//      111: Explained by bits 4 - 15, reserved for future use
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |1|                 Sequence Number a (first)                   |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|                 Sequence Number b (last)                    |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|                 Sequence Number (single)                    |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Loss List Field Coding:
//      For any consectutive lost seqeunce numbers that the differnece between
//      the last and first is more than 1, only record the first (a) and the
//      the last (b) sequence numbers in the loss list field, and modify the
//      the first bit of a to 1.
//      For any single loss or consectutive loss less than 2 packets, use
//      the original sequence numbers in the field.


#include "udt.h"


// Set up the aliases in the constructure
CPacket::CPacket():
m_iSeqNo((__int32&)(m_nHeader)),
m_pcData((char*&)(m_PacketVector[1].iov_base))
{
   m_PacketVector[0].iov_base = (char *)&m_nHeader;
   m_PacketVector[0].iov_len = sizeof(__int32);
}

CPacket::~CPacket()
{
}

__int32 CPacket::getLength() const
{
   return m_PacketVector[1].iov_len;
}

void CPacket::setLength(const __int32& len)
{
   m_PacketVector[1].iov_len = len;
}

void CPacket::pack(const __int32& seqno, const char* data, const __int32& size)
{
   // replicate the sequence number into header
   m_iSeqNo = seqno;

   // point the second demension to the payload
   m_PacketVector[1].iov_base = const_cast<char *>(data);
   m_PacketVector[1].iov_len = size;
}

void CPacket::pack(const __int32& pkttype, void* lparam, void* rparam)
{
   // Set (bit-0 = 1) and (bit-1~3 = type)
   m_nHeader = 0x80000000 | (pkttype << 28);

   // Set bit-16~31 and control information field
   switch (pkttype)
   {
   case 2: //010 - Acknowledgement (ACK)
      // ACK packet seq. no.
      m_nHeader |= *(__int32 *)lparam;

      // data ACK seq. no., RTT, data receiving rate (packets per second), and estimated link capacity (packets per second)
      m_PacketVector[1].iov_base = (char *)rparam;
      m_PacketVector[1].iov_len = sizeof(__int32) * 4;

      break;

   case 6: //110 - Acknowledgement of Acknowledgement (ACK-2)
      // ACK packet seq. no.
      m_nHeader |= *(__int32 *)lparam;

      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)lparam; //NULL;
      m_PacketVector[1].iov_len = sizeof(__int32); //0;

      break;

   case 3: //011 - Loss Report (NAK)
      // loss length
      m_nHeader |= *(__int32 *)lparam;

      // loss list
      m_PacketVector[1].iov_base = (char *)rparam;
      m_PacketVector[1].iov_len = *((__int32 *)lparam + 1) * sizeof(__int32);

      break;

   case 4: //100 - Congestion Warning
      // Header only, no control information
      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)lparam; //NULL;
      m_PacketVector[1].iov_len = sizeof(__int32); //0
  
      break;

   case 1: //001 - Keep-alive
      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)lparam; //NULL;
      m_PacketVector[1].iov_len = sizeof(__int32); //0

      break;

   case 0: //000 - Handshake
      // control info filed is handshake info
      m_PacketVector[1].iov_base = (char *)lparam;
      m_PacketVector[1].iov_len = sizeof(CHandShake);

      break;

   case 5: //101 - Shutdown
      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)lparam; //NULL;
      m_PacketVector[1].iov_len = sizeof(__int32); //0

      break;

   case 7: //111 - Resevered for future use
      break;

   default:
      break;
   }
}

iovec* CPacket::getPacketVector()
{
   return m_PacketVector;
}

__int32 CPacket::getFlag() const
{
   // read bit 0
   return m_nHeader >> 31;
}

__int32 CPacket::getType() const
{
   // read bit 1~3
   return (m_nHeader >> 28) & 0x00000007;
}

__int32 CPacket::getLossLength() const
{
   // read bit 16~31
   return m_nHeader & 0x0000FFFF;
}

__int32 CPacket::getAckSeqNo() const
{
   // read bit 16~31
   return m_nHeader & 0x0000FFFF;
}
