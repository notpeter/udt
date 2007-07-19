/*****************************************************************************
Copyright © 2001 - 2006, The Board of Trustees of the University of Illinois.
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
This header file contains the definition of the UDP channel.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 12/12/2006
*****************************************************************************/

#ifndef __UDT_CHANNEL_H__
#define __UDT_CHANNEL_H__


#include "udt.h"
#include "packet.h"


class CChannel
{
public:
   CChannel();
   CChannel(const int& version);
   ~CChannel();

      // Functionality:
      //    Opne a UDP channel.
      // Parameters:
      //    0) [in] addr: The local address that UDP will use.
      // Returned value:
      //    None.

   void open(const sockaddr* addr = NULL);

      // Functionality:
      //    Disconnect and close the UDP entity.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void close() const;

      // Functionality:
      //    Get the UDP sending buffer size.
      // Parameters:
      //    None.
      // Returned value:
      //    Current UDP sending buffer size.

   int getSndBufSize();

      // Functionality:
      //    Get the UDP receiving buffer size.
      // Parameters:
      //    None.
      // Returned value:
      //    Current UDP receiving buffer size.

   int getRcvBufSize();

      // Functionality:
      //    Set the UDP sending buffer size.
      // Parameters:
      //    0) [in] size: expected UDP sending buffer size.
      // Returned value:
      //    None.

   void setSndBufSize(const int& size);

      // Functionality:
      //    Set the UDP receiving buffer size.
      // Parameters:
      //    0) [in] size: expected UDP receiving buffer size.
      // Returned value:
      //    None.

   void setRcvBufSize(const int& size);

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

      // Functionality:
      //    Send a packet to the given address.
      // Parameters:
      //    0) [in] addr: pointer to the destination address.
      //    1) [in] packet: reference to a CPacket entity.
      // Returned value:
      //    Actual size of data sent.

   int sendto(const sockaddr* addr, CPacket& packet) const;

      // Functionality:
      //    Receive a packet from the channel and record the source address.
      // Parameters:
      //    0) [in] addr: pointer to the source address.
      //    1) [in] packet: reference to a CPacket entity.
      // Returned value:
      //    Actual size of data received.

   int recvfrom(sockaddr* addr, CPacket& packet) const;


private:
   int m_iIPversion;                    // IP version

   #ifndef WIN32
      int m_iSocket;                    // socket descriptor
   #else
      SOCKET m_iSocket;
   #endif

   int m_iSndBufSize;                   // UDP sending buffer size
   int m_iRcvBufSize;                   // UDP receiving buffer size
};


#endif
