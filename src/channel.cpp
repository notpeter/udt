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
This file contains the implementation of UDT packet sending and receiving
routines.

UDT uses UDP for packet transfer. Data gathering/scattering is used in
both sending and receiving.

reference:
UNIX manual: writev, readv
UDT packet definition: packet.h
*****************************************************************************/

/*****************************************************************************
written by 
   Yunhong Gu [ygu@cs.uic.edu], last updated 01/09/2004
*****************************************************************************/

#ifndef WIN32
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <unistd.h>
   #include <fcntl.h>
   #include <string.h>
   #include <stdio.h>
   #include <errno.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif

#include "udt.h"


// For BSD compatability
#ifdef BSD
   #define socklen_t int
#elif WIN32
   #define socklen_t int
#endif

#ifndef WIN32
   #define NET_ERROR errno
#else
   #define NET_ERROR WSAGetLastError()
#endif


CChannel::CChannel():
m_iSndBufSize(102400),
m_iRcvBufSize(307200)
{
   #ifdef WIN32
      WORD wVersionRequested;
      WSADATA wsaData;
      wVersionRequested = MAKEWORD(2, 2);

      if (0 != WSAStartup(wVersionRequested, &wsaData))
         throw CUDTException(1, 0, NET_ERROR);
   #endif
}

CChannel::~CChannel()
{
   #ifdef WIN32
      WSACleanup();
   #endif
}

void CChannel::open(__int32& port, const bool& ch, const char* ip)
{
   // construct an IPv4 socket
   m_iSocket = socket(AF_INET, SOCK_DGRAM, 0);

   if (m_iSocket < 0)
      throw CUDTException(1, 0, NET_ERROR);

   // IPv4 address
   sockaddr_in addr4;
   addr4.sin_family = AF_INET;
   addr4.sin_port = htons(port);
   if (NULL != ip)
   {
      #ifndef WIN32
         inet_pton(AF_INET, ip, &(addr4.sin_addr));
      #else
         addrinfo hint, *addr;
         getaddrinfo(ip, NULL, &hint, &addr);
         addr4.sin_addr.s_addr = ((sockaddr_in*)addr->ai_addr)->sin_addr.s_addr;
         freeaddrinfo(addr);
      #endif
   }
   else
      addr4.sin_addr.s_addr = INADDR_ANY;
   memset(&(addr4.sin_zero), '\0', 8);

   if (0 != bind(m_iSocket, (sockaddr *)&addr4, sizeof(addr4)))
   {
      if (!ch)
         throw CUDTException(1, 1, NET_ERROR);

      // searching any free port number
      addr4.sin_port = 0;
      if (-1 == bind(m_iSocket, (sockaddr *)&addr4, sizeof(addr4)))
         throw CUDTException(1, 1, NET_ERROR);
   }

   // Read the actual port number
   sockaddr_in name;
   socklen_t namelen = sizeof(sockaddr);
   getsockname(m_iSocket, (sockaddr *)&name, &namelen);
   port = ntohs(name.sin_port);

   try
   {
      setChannelOpt();
   }
   catch (CUDTException e)
   {
      throw e;
   }
}

void CChannel::open6(__int32& port, const bool& ch, const char* ip)
{
   // construct an IPv6 socket
   m_iSocket = socket(AF_INET6, SOCK_DGRAM, 0);

   if (m_iSocket < 0)
      throw CUDTException(1, 0, NET_ERROR);

   // IPv6 address
   sockaddr_in6 addr6;
   memset(&addr6, '\0', sizeof(sockaddr_in6));
   addr6.sin6_family = AF_INET6;
   addr6.sin6_port = htons(port);
   if (NULL != ip)
   {
      #ifndef WIN32
         inet_pton(AF_INET6, ip, &(addr6.sin6_addr));
      #else
         addrinfo hint, *addr;
         getaddrinfo(ip, NULL, &hint, &addr);
         addr6.sin6_addr = ((sockaddr_in6*)addr->ai_addr)->sin6_addr;
         freeaddrinfo(addr);
      #endif
   }
   else
      addr6.sin6_addr = in6addr_any;

   if (0 != bind(m_iSocket, (sockaddr *)&addr6, sizeof(addr6)))
   {
      if (!ch)
         throw CUDTException(1, 1, NET_ERROR);

      // searching any free port
      addr6.sin6_port = 0;
      if (-1 == bind(m_iSocket, (sockaddr *)&addr6, sizeof(addr6)))
         throw CUDTException(1, 1, NET_ERROR);
   }

   // read the actual port number
   sockaddr_in6 name;
   socklen_t namelen = sizeof(sockaddr);
   getsockname(m_iSocket, (sockaddr *)&name, &namelen);
   port = ntohs(name.sin6_port);

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

void CChannel::connect(const char* ip, const __int32& port)
{
   addrinfo hints, *addr4, *pt;

   //initilize addrinfo structure.
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = PF_INET;
   hints.ai_socktype = SOCK_DGRAM;

   // convert the port number into string
   char portstr[32];
   sprintf(portstr, "%d", port);

   //looking for IPv4 address
   if (getaddrinfo(ip, portstr, &hints, &addr4) < 0)
      throw CUDTException(1, 3, NET_ERROR);

   // try to connect each possible address
   pt = addr4;
   while (NULL != pt)
   {
      if (0 == ::connect(m_iSocket, pt->ai_addr, int(pt->ai_addrlen)))
         break;

      pt = pt->ai_next;
   }

   freeaddrinfo(addr4);

   // failed.
   if (NULL == pt)
      throw CUDTException(1, 4, NET_ERROR);
}

void CChannel::connect6(const char* ip, const __int32& port)
{
   addrinfo hints, *addr6, *pt;

   //initialize addrinfo structure
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = PF_INET6;
   hints.ai_socktype = SOCK_DGRAM;

   //convert port number into string
   char portstr[32];
   sprintf(portstr, "%d", port);

   //query IPv6 address
   if (getaddrinfo(ip, portstr, &hints, &addr6) < 0)
      throw CUDTException(1, 3, NET_ERROR);

   //try to connect each possible address
   pt = addr6;
   while (NULL != pt)
   {
      if (0 == ::connect(m_iSocket, pt->ai_addr, int(pt->ai_addrlen)))
         break;

      pt = pt->ai_next;
   }

   //free addrinfo structure
   freeaddrinfo(addr6);

   //failed. raise exception.
   if (NULL == pt)
      throw CUDTException(1, 4, NET_ERROR);
}

void CChannel::connect(const sockaddr* addr)
{
   if (0 != ::connect(m_iSocket, addr, sizeof(sockaddr)))
      throw CUDTException(1, 4, NET_ERROR);
}

__int32 CChannel::send(char* buffer, const __int32& size) const
{
   return ::send(m_iSocket, buffer, size, 0);
}

__int32 CChannel::recv(char* buffer, const __int32& size) const
{
   return ::recv(m_iSocket, buffer, size, 0);
}

__int32 CChannel::peek(char* buffer, const __int32& size) const
{
   return ::recv(m_iSocket, buffer, size, MSG_PEEK);
}

const CChannel& CChannel::operator<<(CPacket& packet) const
{
   // convert control information into network order
   if (packet.getFlag())
      for (__int32 i = 0, n = packet.getLength() / sizeof(__int32); i < n; i ++)
         *((__int32 *)packet.m_pcData + i) = htonl(*((__int32 *)packet.m_pcData + i));

   // convert packet header into network order
   packet.m_nHeader = htonl(packet.m_nHeader);

   #ifdef UNIX
      while (0 == writev(m_iSocket, packet.getPacketVector(), 2)) {}
   #else
      writev(m_iSocket, packet.getPacketVector(), 2);
   #endif

   // convert back into local host order
   packet.m_nHeader = ntohl(packet.m_nHeader);
   if (packet.getFlag())
      for (__int32 i = 0, n = packet.getLength() / sizeof(__int32); i < n; i ++)
         *((__int32 *)packet.m_pcData + i) = ntohl(*((__int32 *)packet.m_pcData + i));

   return *this;
}

const CChannel& CChannel::operator>>(CPacket& packet) const
{
   // Packet length indicates if the packet is successfully received
   packet.setLength(readv(m_iSocket, packet.getPacketVector(), 2) - sizeof(__int32));

   #ifdef UNIX
      //simulating RCV_TIMEO
      if (packet.getLength() <= 0)
      {
         usleep(10);
         packet.setLength(readv(m_iSocket, packet.getPacketVector(), 2) - sizeof(__int32));
      }
   #endif

   if (packet.getLength() <= 0)
      return *this;

   // convert packet header into local host order
   packet.m_nHeader = ntohl(packet.m_nHeader);

   // convert control information into local host order
   if (packet.getFlag())
      for (__int32 i = 0, n = packet.getLength() / sizeof(__int32); i < n; i ++)
         *((__int32 *)packet.m_pcData + i) = ntohl(*((__int32 *)packet.m_pcData + i));

   return *this;
}

__int32 CChannel::recvfrom(CPacket& packet, sockaddr* addr) const
{
   char* buf = new char [packet.getPacketVector()[0].iov_len + packet.getPacketVector()[1].iov_len];
   socklen_t addrsize = sizeof(sockaddr);

   if (::recvfrom(m_iSocket, buf, packet.getPacketVector()[0].iov_len + packet.getPacketVector()[1].iov_len, MSG_PEEK, addr, &addrsize) <= 0)
      return 0;

   (*this) >> packet;

   return packet.getLength();
}

__int32 CChannel::getSndBufSize()
{
   socklen_t size;

   getsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, &size);

   return m_iSndBufSize;
}

__int32 CChannel::getRcvBufSize()
{
   socklen_t size;

   getsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, &size);

   return m_iRcvBufSize;
}

void CChannel::setSndBufSize(const __int32& size)
{
   m_iSndBufSize = size;
}

void CChannel::setRcvBufSize(const __int32& size)
{
   m_iRcvBufSize = size;
}

void CChannel::addMembership(const char* mcip)
{
   ip_mreq mreq;

   mreq.imr_multiaddr.s_addr=inet_addr(mcip);
   mreq.imr_interface.s_addr=htonl(INADDR_ANY);

   if (setsockopt(m_iSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
      throw CUDTException(1, 4, NET_ERROR);
}

void CChannel::joinGroup(const char* mcip)
{
   ipv6_mreq mreq;

   #ifndef WIN32
      inet_pton(AF_INET6, mcip, mreq.ipv6mr_multiaddr.s6_addr);
   #else
      addrinfo hint, *addr;
      getaddrinfo(mcip, NULL, &hint, &addr);
      memcpy(mreq.ipv6mr_multiaddr.s6_addr, &(((sockaddr_in6*)addr->ai_addr)->sin6_addr), sizeof(in6_addr));
      freeaddrinfo(addr);
   #endif
   mreq.ipv6mr_interface = 0;

   if (setsockopt(m_iSocket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&mreq, sizeof(mreq)) < 0)
      throw CUDTException(1, 4, NET_ERROR); 
}

void CChannel::getAddr(unsigned char* ip) const
{
   sockaddr_in name;
   socklen_t namelen = sizeof(sockaddr);
   getsockname(m_iSocket, (sockaddr *)&name, &namelen);
   memcpy(ip, (char *)&(name.sin_addr), sizeof(name.sin_addr));
}

void CChannel::getAddr6(unsigned char* ip) const
{
   sockaddr_in6 name;
   socklen_t namelen = sizeof(sockaddr);
   getsockname(m_iSocket, (sockaddr *)&name, &namelen);
   memcpy(ip, (char *)&(name.sin6_addr), sizeof(name.sin6_addr));
}

void CChannel::setChannelOpt()
{
   // set sending and receiving buffer size
   setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, sizeof(__int32));
   setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, sizeof(__int32));

   timeval tv;
   tv.tv_sec = 0;
   #ifdef BSD
      // Known BSD bug as the day I wrote these codes.
      // A small time out value will cause the socket to block forever.
      tv.tv_usec = 10000;
   #else
      tv.tv_usec = 100;
   #endif

   #ifdef UNIX
      // Set non-blocking I/O
      // UNIX does not support SO_RCVTIMEO
      __int32 opts = fcntl(m_iSocket, F_GETFL);
      if (-1 == fcntl(m_iSocket, F_SETFL, opts | O_NONBLOCK))
         throw CUDTException(1, 2, NET_ERROR);
   #elif WIN32
      DWORD ot = 1; //milliseconds
      if (setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&ot, sizeof(DWORD)) < 0)
         throw CUDTException(1, 2, NET_ERROR);
   #else
      // Set receiving time-out value
      if (setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(timeval)) < 0)
         throw CUDTException(1, 2, NET_ERROR);
   #endif
}
