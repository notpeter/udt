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
****************************************************************************/

/*****************************************************************************
This file contains the implementation of UDT packet sending and receiving
routines.

UDT uses UDP for packet transfer. Data gathering/scattering is used in
both sending and receiving.

reference:
socket programming reference, writev/readv
UDT packet definition: packet.h
*****************************************************************************/

/****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 06/27/2006
*****************************************************************************/

#ifndef WIN32
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <unistd.h>
   #include <fcntl.h>
   #include <cstring>
   #include <cstdio>
   #include <cerrno>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif
#include "channel.h"
#include "packet.h"


#ifdef WIN32
   #define socklen_t int
#endif

#ifndef WIN32
   #define NET_ERROR errno
#else
   #define NET_ERROR WSAGetLastError()
#endif


CChannel::CChannel():
m_iIPversion(AF_INET),
m_iSndBufSize(65536),
m_iRcvBufSize(65536),
m_pcChannelBuf(NULL)
{
   m_pcChannelBuf = new char [9000];
}

CChannel::CChannel(const int& version):
m_iIPversion(version),
m_iSndBufSize(65536),
m_iRcvBufSize(65536),
m_pcChannelBuf(NULL)
{
   m_pcChannelBuf = new char [9000];
}

CChannel::~CChannel()
{
   delete [] m_pcChannelBuf;
}

void CChannel::open(const sockaddr* addr)
{
   // construct an socket
   m_iSocket = socket(m_iIPversion, SOCK_DGRAM, 0);

   if (m_iSocket < 0)
      throw CUDTException(1, 0, NET_ERROR);

   if (NULL != addr)
   {
      socklen_t namelen = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

      if (0 != bind(m_iSocket, addr, namelen))
         throw CUDTException(1, 3, NET_ERROR);
   }

   try
   {
      setChannelOpt();
   }
   catch (CUDTException e)
   {
      throw e;
   }
}

void CChannel::disconnect() const
{
   #ifndef WIN32
      close(m_iSocket);
   #else
      closesocket(m_iSocket);
   #endif
}

void CChannel::connect(const sockaddr* addr)
{
   const int addrlen = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   if (0 != ::connect(m_iSocket, addr, addrlen))
      throw CUDTException(1, 4, NET_ERROR);
}

int CChannel::send(char* buffer, const int& size) const
{
   return ::send(m_iSocket, buffer, size, 0);
}

int CChannel::recv(char* buffer, const int& size) const
{
   return ::recv(m_iSocket, buffer, size, 0);
}

int CChannel::peek(char* buffer, const int& size) const
{
   return ::recv(m_iSocket, buffer, size, MSG_PEEK);
}

const CChannel& CChannel::operator<<(CPacket& packet) const
{
   // convert control information into network order
   if (packet.getFlag())
      for (int i = 0, n = packet.getLength() / 4; i < n; ++ i)
         *((uint32_t *)packet.m_pcData + i) = htonl(*((uint32_t *)packet.m_pcData + i));

   // convert packet header into network order
   packet.m_nHeader[0] = htonl(packet.m_nHeader[0]);
   packet.m_nHeader[1] = htonl(packet.m_nHeader[1]);

   #if defined (UNIX)
      while (0 == writev(m_iSocket, packet.getPacketVector(), 2)) {}
   #elif defined (WIN32)
      DWORD ssize = 0;
      WSASend(m_iSocket, (LPWSABUF)(packet.getPacketVector()), 2, &ssize, 0, NULL, NULL);
   #else
      writev(m_iSocket, packet.getPacketVector(), 2);
   #endif

   // convert back into local host order
   packet.m_nHeader[0] = ntohl(packet.m_nHeader[0]);
   packet.m_nHeader[1] = ntohl(packet.m_nHeader[1]);

   if (packet.getFlag())
      for (int j = 0, n = packet.getLength() / 4; j < n; ++ j)
         *((uint32_t *)packet.m_pcData + j) = ntohl(*((uint32_t *)packet.m_pcData + j));

   return *this;
}

const CChannel& CChannel::operator>>(CPacket& packet) const
{
   // Packet length indicates if the packet is successfully received
   #ifndef WIN32
      packet.setLength(readv(m_iSocket, packet.getPacketVector(), 2) - CPacket::m_iPktHdrSize);
   #else
      DWORD rsize = 0;
      DWORD flag = 0;
      WSARecv(m_iSocket, (LPWSABUF)(packet.getPacketVector()), 2, &rsize, &flag, NULL, NULL);
      packet.setLength(rsize - CPacket::m_iPktHdrSize);
   #endif

   #ifdef UNIX
      //simulating RCV_TIMEO
      if (packet.getLength() <= 0)
      {
         usleep(10);
         packet.setLength(readv(m_iSocket, packet.getPacketVector(), 2) - CPacket::m_iPktHdrSize);
      }
   #endif

   if (packet.getLength() <= 0)
      return *this;

   // convert packet header into local host order
   packet.m_nHeader[0] = ntohl(packet.m_nHeader[0]);
   packet.m_nHeader[1] = ntohl(packet.m_nHeader[1]);

   // convert control information into local host order
   if (packet.getFlag())
      for (int i = 0, n = packet.getLength() / 4; i < n; ++ i)
         *((uint32_t *)packet.m_pcData + i) = ntohl(*((uint32_t *)packet.m_pcData + i));

   return *this;
}

int CChannel::sendto(CPacket& packet, const sockaddr* addr) const
{
   // convert control information into network order
   if (packet.getFlag())
      for (int i = 0, n = packet.getLength() / 4; i < n; ++ i)
         *((uint32_t *)packet.m_pcData + i) = htonl(*((uint32_t *)packet.m_pcData + i));

   // convert packet header into network order
   packet.m_nHeader[0] = htonl(packet.m_nHeader[0]);
   packet.m_nHeader[1] = htonl(packet.m_nHeader[1]);

   char* buf;
   if (CPacket::m_iPktHdrSize + packet.getLength() <= 9000)
      buf = m_pcChannelBuf;
   else
      buf = new char [CPacket::m_iPktHdrSize + packet.getLength()];

   memcpy(buf, packet.getPacketVector()[0].iov_base, CPacket::m_iPktHdrSize);
   memcpy(buf + CPacket::m_iPktHdrSize, packet.getPacketVector()[1].iov_base, packet.getLength());

   socklen_t addrsize = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   int ret = ::sendto(m_iSocket, buf, CPacket::m_iPktHdrSize + packet.getLength(), 0, addr, addrsize);

   #ifdef UNIX
      while (ret <= 0)
         ret = ::sendto(m_iSocket, buf, CPacket::m_iPktHdrSize + packet.getLength(), 0, addr, addrsize);
   #endif

   if (CPacket::m_iPktHdrSize + packet.getLength() > 9000)
      delete [] buf;

   // convert back into local host order
   packet.m_nHeader[0] = ntohl(packet.m_nHeader[0]);
   packet.m_nHeader[1] = ntohl(packet.m_nHeader[1]);

   if (packet.getFlag())
      for (int j = 0, n = packet.getLength() / 4; j < n; ++ j)
         *((uint32_t *)packet.m_pcData + j) = ntohl(*((uint32_t *)packet.m_pcData + j));

   return ret;
}

int CChannel::recvfrom(CPacket& packet, sockaddr* addr) const
{
   char* buf;
   if (CPacket::m_iPktHdrSize + packet.getLength() <= 9000)
      buf = m_pcChannelBuf;
   else
      buf = new char [CPacket::m_iPktHdrSize + packet.getLength()];

   socklen_t addrsize = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   int ret = ::recvfrom(m_iSocket, buf, CPacket::m_iPktHdrSize + packet.getLength(), 0, addr, &addrsize);

   #ifdef UNIX
      //simulating RCV_TIMEO
      if (ret <= 0)
      {
         usleep(10);
         ret = ::recvfrom(m_iSocket, buf, CPacket::m_iPktHdrSize + packet.getLength(), 0, addr, &addrsize);
      }
   #endif

   if (ret > CPacket::m_iPktHdrSize)
   {
      packet.setLength(ret - CPacket::m_iPktHdrSize);
      memcpy(packet.getPacketVector()[0].iov_base, buf, CPacket::m_iPktHdrSize);
      memcpy(packet.getPacketVector()[1].iov_base, buf + CPacket::m_iPktHdrSize, ret - CPacket::m_iPktHdrSize);

      // convert back into local host order
      packet.m_nHeader[0] = ntohl(packet.m_nHeader[0]);
      packet.m_nHeader[1] = ntohl(packet.m_nHeader[1]);

      if (packet.getFlag())
         for (int i = 0, n = packet.getLength() / 4; i < n; ++ i)
            *((uint32_t *)packet.m_pcData + i) = ntohl(*((uint32_t *)packet.m_pcData + i));
   }
   else
   {
      if (ret > 0)
         ret = 0;
      packet.setLength(ret);
   }

   if (CPacket::m_iPktHdrSize + packet.getLength() > 9000)
      delete [] buf;

   return ret;
}

int CChannel::getSndBufSize()
{
   socklen_t size = sizeof(socklen_t);

   getsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, &size);

   return m_iSndBufSize;
}

int CChannel::getRcvBufSize()
{
   socklen_t size = sizeof(socklen_t);

   getsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, &size);

   return m_iRcvBufSize;
}

void CChannel::setSndBufSize(const int& size)
{
   m_iSndBufSize = size;
}

void CChannel::setRcvBufSize(const int& size)
{
   m_iRcvBufSize = size;
}

void CChannel::getSockAddr(sockaddr* addr) const
{
   socklen_t namelen = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   getsockname(m_iSocket, addr, &namelen);
}

void CChannel::getPeerAddr(sockaddr* addr) const
{
   socklen_t namelen = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   getpeername(m_iSocket, addr, &namelen);
}

void CChannel::setChannelOpt()
{
   // set sending and receiving buffer size
   if ((0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, sizeof(int))) ||
       (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, sizeof(int))))
      throw CUDTException(1, 3, NET_ERROR);

   timeval tv;
   tv.tv_sec = 0;
   #if defined (BSD) || defined (OSX)
      // Known BSD bug as the day I wrote these codes.
      // A small time out value will cause the socket to block forever.
      tv.tv_usec = 10000;
   #else
      tv.tv_usec = 100;
   #endif

   #ifdef UNIX
      // Set non-blocking I/O
      // UNIX does not support SO_RCVTIMEO
      int opts = fcntl(m_iSocket, F_GETFL);
      if (-1 == fcntl(m_iSocket, F_SETFL, opts | O_NONBLOCK))
         throw CUDTException(1, 3, NET_ERROR);
   #elif WIN32
      DWORD ot = 1; //milliseconds
      if (setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&ot, sizeof(DWORD)) < 0)
         throw CUDTException(1, 3, NET_ERROR);
   #else
      // Set receiving time-out value
      if (setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(timeval)) < 0)
         throw CUDTException(1, 3, NET_ERROR);
   #endif
}
