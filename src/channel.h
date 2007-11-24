/*****************************************************************************
Copyright (c) 2001 - 2007, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
This header file contains the definition of the UDP channel.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 06/13/2006
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
      //    Send data through the channel.
      // Parameters:
      //    0) [in] buffer: pointer to the data to be sent.
      //    1) [in] size: size of the buffer.
      // Returned value:
      //    Actual size of data sent.

   int send(char* buffer, const int& size) const;

      // Functionality:
      //    Receive data from the channel.
      // Parameters:
      //    0) [out] buffer: received data.
      //    1) [in] size: size of the expected data received.
      // Returned value:
      //    Actual size of data received.

   int recv(char* buffer, const int& size) const;

      // Functionality:
      //    Read the data from the channel but the data is not removed from UDP buffer.
      // Parameters:
      //    0) [out] buffer: previewed received data.
      //    1) [in] size: size of the expected data received.
      // Returned value:
      //    Actual size of data received.

   int peek(char* buffer, const int& size) const;

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

   int sendto(CPacket& packet, const sockaddr* addr) const;

      // Functionality:
      //    Receive a packet from the channel and record the source address.
      // Parameters:
      //    0) [in] packet: reference to a CPacket entity.
      //    1) [in] addr: pointer to the source address.
      // Returned value:
      //    Actual size of data received.

   int recvfrom(CPacket& packet, sockaddr* addr) const;

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

private:
   int m_iIPversion;                    // IP version

   #ifndef WIN32
      int m_iSocket;                    // socket descriptor
   #else
      SOCKET m_iSocket;
   #endif

   int m_iSndBufSize;                   // UDP sending buffer size
   int m_iRcvBufSize;                   // UDP receiving buffer size

   char* m_pcChannelBuf;                // buffer for temporally storage of in/out data

private:
   void setChannelOpt();
};


#endif
