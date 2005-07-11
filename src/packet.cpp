/*****************************************************************************
Copyright © 2001 - 2005, The Board of Trustees of the University of Illinois.
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
This file contains the implementation of UDT packet handling modules.

A UDT packet is a 2-dimension vector of packet header and data.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [ygu@cs.uic.edu], last updated 02/06/2005

modified by
   <programmer's name, programmer's email, last updated mm/dd/yyyy>
   <descrition of changes>
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
//   |                          Time Stamp                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   bit 0:
//      0: Data Packet
//      1: Control Packet
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |1|type |        Reserved       |           ACK Seq. No.        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          Time Stamp                           |
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
//              bits 16-31:   The ACK sequence number
//              Control Info: The sequence number to which (but not include) all the previous packets have beed received
//              Optional:     RTT
//                            RTT Variance
//                            advertised flow window size (number of packets)
//                            estimated bandwidth (number of packets per second)
//      011: Negative Acknowledgement (NAK)
//              bits 16-31:   Undefined
//              Control Info: Loss list (see loss list coding below)
//      100: Congestion Warning
//              bits 16-31:   Undefined
//              Control Info: None
//      101: Shutdown
//              bits 16-31:   Undefined
//              Control Info: None
//      110: Acknowledgement of Acknowledement (ACK-square)
//              bits 16-31:   The ACK sequence number
//              Control Info: None
//      111: Explained by bits 4 - 15
//              
//   bit 4 - 15:
//      This space is used for future expansion or user defined control packets. 
//      Although a user defined packets can use this space freely, it is highly
//      recommended that lower bit is chosen first, i.e., starting from 0xFFF
//      for bits 4 - 15. For example, if a customized protocol need to add two 
//      new types of packets, 0xFFF and 0xFFE is recommended.
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
m_iSeqNo((__int32&)(m_nHeader[0])),
m_iTimeStamp((__int32&)(m_nHeader[1])),
m_pcData((char*&)(m_PacketVector[1].iov_base))
{
   m_PacketVector[0].iov_base = (char *)m_nHeader;
   m_PacketVector[0].iov_len = CPacket::m_iPktHdrSize;
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

void CPacket::pack(const __int32& pkttype, void* lparam, void* rparam, const __int32& size)
{
   // Set (bit-0 = 1) and (bit-1~3 = type)
   m_nHeader[0] = 0x80000000 | (pkttype << 28);

   // Set bit-16~31 and control information field
   switch (pkttype)
   {
   case 2: //010 - Acknowledgement (ACK)
      // ACK packet seq. no.
      if (NULL != lparam)
         m_nHeader[0] |= *(__int32 *)lparam;

      // data ACK seq. no. 
      // optional: RTT (microsends), RTT variance (microseconds) advertised flow window size (packets), and estimated link capacity (packets per second)
      m_PacketVector[1].iov_base = (char *)rparam;
      m_PacketVector[1].iov_len = size;

      break;

   case 6: //110 - Acknowledgement of Acknowledgement (ACK-2)
      // ACK packet seq. no.
      m_nHeader[0] |= *(__int32 *)lparam;

      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)&__pad; //NULL;
      m_PacketVector[1].iov_len = sizeof(__int32); //0;

      break;

   case 3: //011 - Loss Report (NAK)
      // loss list
      m_PacketVector[1].iov_base = (char *)rparam;
      m_PacketVector[1].iov_len = size;

      break;

   case 4: //100 - Congestion Warning
      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)&__pad; //NULL;
      m_PacketVector[1].iov_len = sizeof(__int32); //0
  
      break;

   case 1: //001 - Keep-alive
      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)&__pad; //NULL;
      m_PacketVector[1].iov_len = sizeof(__int32); //0

      break;

   case 0: //000 - Handshake
      // control info filed is handshake info
      m_PacketVector[1].iov_base = (char *)rparam;
      m_PacketVector[1].iov_len = size; //sizeof(CHandShake);

      break;

   case 5: //101 - Shutdown
      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)&__pad; //NULL;
      m_PacketVector[1].iov_len = sizeof(__int32); //0

      break;

   case 7: //111 - Reserved for user defined control packets
      // for extended control packet
      // "lparam" contains the extneded type information for bit 4 - 15
      // "rparam" is the control information
      m_nHeader[0] |= (*(__int32 *)lparam) << 16;

      if (NULL != rparam)
      {
         m_PacketVector[1].iov_base = (char *)rparam;
         m_PacketVector[1].iov_len = size;
      }
      else
      {
         m_PacketVector[1].iov_base = (char *)&__pad;
         m_PacketVector[1].iov_len = sizeof(__int32);
      }

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
   return m_nHeader[0] >> 31;
}

__int32 CPacket::getType() const
{
   // read bit 1~3
   return (m_nHeader[0] >> 28) & 0x00000007;
}

__int32 CPacket::getExtendedType() const
{
   // read bit 4~16
   return (m_nHeader[0] >> 16) & 0x00000FFF;
}

__int32 CPacket::getAckSeqNo() const
{
   // read bit 16~31
   return m_nHeader[0] & 0x0000FFFF;
}
