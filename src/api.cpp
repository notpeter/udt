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
This file contains the implementation of UDT API.

reference: UDT programming manual and socket programming reference
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [ygu@cs.uic.edu], last updated 01/10/2005

modified by
   <programmer's name, programmer's email, last updated mm/dd/yyyy>
   <descrition of changes>
*****************************************************************************/


#ifndef WIN32
   #include <unistd.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif

#include "udt.h"

CUDTSocket::CUDTSocket():
m_pSelfAddr(NULL),
m_pPeerAddr(NULL),
m_pUDT(NULL),
m_pQueuedSockets(NULL),
m_pAcceptSockets(NULL)
{
}

CUDTSocket::~CUDTSocket()
{
   if (4 == m_iIPversion)
   {
      if (m_pSelfAddr)
         delete (sockaddr_in*)m_pSelfAddr;
      if (m_pPeerAddr)
         delete (sockaddr_in*)m_pPeerAddr;
   }
   else
   {
      if (m_pSelfAddr)
         delete (sockaddr_in6*)m_pSelfAddr;
      if (m_pPeerAddr)
         delete (sockaddr_in6*)m_pPeerAddr;
   }

   if (m_pQueuedSockets)
      delete m_pQueuedSockets;
   if (m_pAcceptSockets)
      delete m_pAcceptSockets;
}


CUDTUnited::CUDTUnited():
m_SocketID(1 << 30)
{
   #ifndef WIN32
      pthread_mutex_init(&m_ControlLock, NULL);
      pthread_mutex_init(&m_IDLock, NULL);
      pthread_mutex_init(&m_AcceptLock, NULL);
      pthread_cond_init(&m_AcceptCond, NULL);
   #else
      m_ControlLock = CreateMutex(NULL, false, NULL);
      m_IDLock = CreateMutex(NULL, false, NULL);
      m_AcceptLock = CreateMutex(NULL, false, NULL);
      m_AcceptCond = CreateEvent(NULL, false, false, NULL);
   #endif

   #ifndef WIN32
      pthread_key_create(&m_TLSError, TLSDestroy);
   #else
      m_TLSError = TlsAlloc();
   #endif
}

CUDTUnited::~CUDTUnited()
{
   #ifndef WIN32
      pthread_mutex_destroy(&m_ControlLock);
      pthread_mutex_destroy(&m_IDLock);
      pthread_mutex_destroy(&m_AcceptLock);
      pthread_cond_destroy(&m_AcceptCond);
   #else
      CloseHandle(m_ControlLock);
      CloseHandle(m_IDLock);
      CloseHandle(m_AcceptLock);
      CloseHandle(m_AcceptCond);
   #endif

   #ifndef WIN32
      pthread_key_delete(m_TLSError);
   #else
      TlsFree(m_TLSError);
   #endif
}

UDTSOCKET CUDTUnited::newSocket(const __int32& af)
{
   // garbage collection before a new socket is created
   checkBrokenSockets();

   CUDTSocket* ns = new CUDTSocket;

   ns->m_Status = CUDTSocket::INIT;

   #ifndef WIN32
      pthread_mutex_lock(&m_IDLock);
   #else
      WaitForSingleObject(m_IDLock, INFINITE);
   #endif
   ns->m_Socket = -- m_SocketID;
   #ifndef WIN32
      pthread_mutex_unlock(&m_IDLock);
   #else
      ReleaseMutex(m_IDLock);
   #endif

   ns->m_ListenSocket = 0;
   ns->m_pUDT = new CUDT;
   ns->m_pUDT->m_SocketID = ns->m_Socket;
   if (AF_INET == af)
   {
      ns->m_pUDT->m_iIPversion = ns->m_iIPversion = 4;
      ns->m_pSelfAddr = (sockaddr*)(new sockaddr_in);
   }
   else
   {
      ns->m_pUDT->m_iIPversion = ns->m_iIPversion = 6;
      ns->m_pSelfAddr = (sockaddr*)(new sockaddr_in6);
   }

   // protect the m_Sockets structure.
   #ifndef WIN32
      pthread_mutex_lock(&m_ControlLock);
   #else
      WaitForSingleObject(m_ControlLock, INFINITE);
   #endif
   m_Sockets[ns->m_Socket] = ns;
   #ifndef WIN32
      pthread_mutex_unlock(&m_ControlLock);
   #else
      ReleaseMutex(m_ControlLock);
   #endif

   return ns->m_Socket;
}

void CUDTUnited::newConnection(const UDTSOCKET listen, const sockaddr* peer, CHandShake* hs)
{
   // garbage collection before a new socket is created
   checkBrokenSockets();

   CUDTSocket* ns;
   CUDTSocket* ls = locate(listen);

   // if this connection has already been processed
   if (NULL != (ns = locate(listen, peer)))
   {
      if (ns->m_pUDT->m_bBroken)
      {
         // last connection from the "peer" address has been broken
         ns->m_Status = CUDTSocket::CLOSED;
         gettimeofday(&ns->m_TimeStamp, 0);
         ls->m_pQueuedSockets->erase(ns->m_Socket);
         ls->m_pAcceptSockets->erase(ns->m_Socket);
      }
      else if (hs->m_iISN == ns->m_pUDT->m_iPeerISN)
      {
         // connection already exist, this is a repeated connection request
         // pass the hand shake packet to the UDT entity
         CPacket cr;
         cr.pack(0, NULL, hs, sizeof(CHandShake));

         ns->m_pUDT->processCtrl(cr);

         return;

         //except for this situation a new connection should be started
      }
   }

   // exceeding backlog, refuse the connection request
   if (ls->m_pQueuedSockets->size() >= ls->m_uiBackLog)
      return;

   ns = new CUDTSocket;

   ns->m_Status = CUDTSocket::INIT;

   #ifndef WIN32
      pthread_mutex_lock(&m_IDLock);
   #else
      WaitForSingleObject(m_IDLock, INFINITE);
   #endif
   ns->m_Socket = -- m_SocketID;
   #ifndef WIN32
      pthread_mutex_unlock(&m_IDLock);
   #else
      ReleaseMutex(m_IDLock);
   #endif

   ns->m_ListenSocket = listen;
   ns->m_pUDT = new CUDT(*(ls->m_pUDT));
   ns->m_iIPversion = ls->m_iIPversion;
   ns->m_pUDT->m_SocketID = ns->m_Socket;

   if (4 == ls->m_iIPversion)
   {
      ns->m_pSelfAddr = (sockaddr*)(new sockaddr_in);
      ns->m_pPeerAddr = (sockaddr*)(new sockaddr_in);
      memcpy(ns->m_pPeerAddr, peer, sizeof(sockaddr_in));
   }
   else
   {
      ns->m_pSelfAddr = (sockaddr*)(new sockaddr_in6);
      ns->m_pPeerAddr = (sockaddr*)(new sockaddr_in6);
      memcpy(ns->m_pPeerAddr, peer, sizeof(sockaddr_in6));
   }

   try
   {
      ns->m_pUDT->open();
      ns->m_pUDT->connect(peer, hs);
   }
   catch (...)
   {
      // connection setup exceptions.
      // currently all I can do is to drop it silently. a rejection message should be sent anyway.

      ns->m_pUDT->close();
      delete ns->m_pUDT;
      delete ns;

      return;
   }

   // copy address information of local node
   ns->m_pUDT->m_pChannel->getSockAddr(ns->m_pSelfAddr);

   ls->m_pQueuedSockets->insert(ns->m_Socket);

   // protect the m_Sockets structure.
   #ifndef WIN32
      pthread_mutex_lock(&m_ControlLock);
   #else
      WaitForSingleObject(m_ControlLock, INFINITE);
   #endif
   m_Sockets[ns->m_Socket] = ns;
   #ifndef WIN32
      pthread_mutex_unlock(&m_ControlLock);
   #else
      ReleaseMutex(m_ControlLock);
   #endif

   // wake up a waiting accept() call
   #ifndef WIN32
      pthread_cond_signal(&m_AcceptCond);
   #else
      SetEvent(m_AcceptCond);
   #endif
}

CUDT* CUDTUnited::lookup(const UDTSOCKET u)
{
   // protects the m_Sockets structure
   CGuard cg(m_ControlLock);

   map<UDTSOCKET, CUDTSocket*>::iterator i = m_Sockets.find(u);

   if (i == m_Sockets.end())
      throw CUDTException(5, 4, 0);

   return i->second->m_pUDT;
}

__int32 CUDTUnited::bind(const UDTSOCKET u, const sockaddr* name, const __int32& namelen)
{
   CUDTSocket* s = locate(u);

   if (NULL == s)
      throw CUDTException(5, 4, 0);

   // check the size of SOCKADDR structure
   if (4 == s->m_iIPversion)
   {
      if (namelen != sizeof(sockaddr_in))
         throw CUDTException(5, 3, 0);
   }
   else
   {
      if (namelen != sizeof(sockaddr_in6));
         throw CUDTException(5, 3, 0);
   }

   // cannot bind a socket more than once
   if (CUDTSocket::INIT != s->m_Status)
      throw CUDTException(5, 0, 0);

   s->m_pUDT->open(name);
   s->m_Status = CUDTSocket::OPENED;

   // copy address information of local node
   s->m_pUDT->m_pChannel->getSockAddr(s->m_pSelfAddr);

   return 0;
}

__int32 CUDTUnited::listen(const UDTSOCKET u, const __int32& backlog)
{
   CUDTSocket* s = locate(u);

   if (NULL == s)
      throw CUDTException(5, 4, 0);

   if (backlog <= 0)
      throw CUDTException(5, 3, 0);

   s->m_uiBackLog = backlog;

   // do nothing if the socket is already in listening
   if (CUDTSocket::LISTENING == s->m_Status)
      return 0;

   // a socket can listen only if is in OPENED status
   if (CUDTSocket::OPENED != s->m_Status)
      throw CUDTException(5, 5, 0);

   s->m_pUDT->listen();
   s->m_pQueuedSockets = new set<UDTSOCKET>;
   s->m_pAcceptSockets = new set<UDTSOCKET>;
   s->m_Status = CUDTSocket::LISTENING;

   return 0;
}

UDTSOCKET CUDTUnited::accept(const UDTSOCKET listen, sockaddr* addr, __int32* addrlen)
{
   CUDTSocket* ls = locate(listen);

   if (ls == NULL)
      throw CUDTException(5, 4, 0);

   // the "listen" socket must be in LISTENING status
   if (CUDTSocket::LISTENING != ls->m_Status)
      throw CUDTException(5, 6, 0);

   // non-blocking receiving, no connection available
   if ((!ls->m_pUDT->m_bSynRecving) && (0 == ls->m_pQueuedSockets->size()))
      throw CUDTException(6, 2, 0);

   #ifndef WIN32
      pthread_mutex_lock(&m_AcceptLock);
      while ((ls->m_pQueuedSockets->size() == 0) && (CUDTSocket::LISTENING == ls->m_Status))
         pthread_cond_wait(&m_AcceptCond, &m_AcceptLock);
      pthread_mutex_unlock(&m_AcceptLock);
   #else
      while ((ls->m_pQueuedSockets->size() == 0) && (CUDTSocket::LISTENING == ls->m_Status))
         WaitForSingleObject(m_AcceptCond, INFINITE);
      // wake up other "accept" calls
      if (CUDTSocket::CLOSED == ls->m_Status)
         SetEvent(m_AcceptCond);
   #endif

   // !!only one conection can be set up at each time!!

   UDTSOCKET u;

   u = *(ls->m_pQueuedSockets->begin());
   ls->m_pAcceptSockets->insert(ls->m_pAcceptSockets->end(), u);
   ls->m_pQueuedSockets->erase(ls->m_pQueuedSockets->begin());

   if (NULL != addr)
   {
      if (NULL == addrlen)
         throw CUDTException(5, 3, 0);

      if (4 == locate(u)->m_iIPversion)
         *addrlen = sizeof(sockaddr_in);
      else
         *addrlen = sizeof(sockaddr_in6);

      // copy address information of peer node
      memcpy(addr, locate(u)->m_pPeerAddr, *addrlen);
   }

   return u;
}

__int32 CUDTUnited::connect(const UDTSOCKET u, const sockaddr* name, const __int32& namelen)
{
   CUDTSocket* s = locate(u);

   if (NULL == s)
      throw CUDTException(5, 4, 0);

   // check the size of SOCKADDR structure
   if (4 == s->m_iIPversion)
   {
      if (namelen != sizeof(sockaddr_in))
         throw CUDTException(5, 3, 0);
   }
   else
   {
      if (namelen != sizeof(sockaddr_in6));
         throw CUDTException(5, 3, 0);
   }

   // a socket can "connect" only if it is in INIT or OPENED status
   if (CUDTSocket::INIT == s->m_Status)
      s->m_pUDT->open();
   else if (CUDTSocket::OPENED != s->m_Status)
      throw CUDTException(5, 2, 0);

   s->m_pUDT->connect(name);
   s->m_Status = CUDTSocket::CONNECTED;

   // copy address information of local node
   s->m_pUDT->m_pChannel->getSockAddr(s->m_pSelfAddr);

   // record peer address
   if (4 == s->m_iIPversion)
      s->m_pPeerAddr = (sockaddr*)(new sockaddr_in);
   else
      s->m_pPeerAddr = (sockaddr*)(new sockaddr_in6);
   s->m_pUDT->m_pChannel->getPeerAddr(s->m_pPeerAddr);

   return 0;
}

__int32 CUDTUnited::close(const UDTSOCKET u)
{
   CUDTSocket* s = locate(u);
   
   if (NULL == s)
      throw CUDTException(5, 4, 0);

   CUDTSocket::UDTSTATUS os = s->m_Status;

   // synchronize with garbage collection.
   #ifndef WIN32
      pthread_mutex_lock(&m_ControlLock);
   #else
      WaitForSingleObject(m_ControlLock, INFINITE);
   #endif
   s->m_Status = CUDTSocket::CLOSED;
   #ifndef WIN32
      pthread_mutex_unlock(&m_ControlLock);
   #else
      ReleaseMutex(m_ControlLock);
   #endif

   // broadcast all "accpet" waiting
   if (CUDTSocket::LISTENING == os)
      #ifndef WIN32
         pthread_cond_broadcast(&m_AcceptCond);
      #else
         SetEvent(m_AcceptCond);
      #endif

   CUDT* udt = s->m_pUDT;

   udt->close();

   // a socket will not be immediated removed when it is closed
   // in order to prevent other methods from accessing invalid address
   // a timer is started and the socket will be removed after approximately 1 second
   gettimeofday(&s->m_TimeStamp, 0);

   return 0;
}

__int32 CUDTUnited::getpeername(const UDTSOCKET u, sockaddr* name, __int32* namelen)
{
   CUDTSocket* s = locate(u);

   if (NULL == s)
      throw CUDTException(5, 4, 0);

   if (!s->m_pUDT->m_bConnected)
      throw CUDTException(2, 2, 0);

   if (4 == s->m_iIPversion)
      *namelen = sizeof(sockaddr_in);
   else
      *namelen = sizeof(sockaddr_in6);

   // copy address information of peer node
   memcpy(name, s->m_pPeerAddr, *namelen);

   return 0;
}

__int32 CUDTUnited::getsockname(const UDTSOCKET u, sockaddr* name, __int32* namelen)
{
   CUDTSocket* s = locate(u);

   if (NULL == s)
      throw CUDTException(5, 4, 0);

   if (4 == s->m_iIPversion)
      *namelen = sizeof(sockaddr_in);
   else
      *namelen = sizeof(sockaddr_in6);

   // copy address information of local node
   memcpy(name, s->m_pSelfAddr, *namelen);

   return 0;
}

__int32 CUDTUnited::select(ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout)
{
   timeval entertime, currtime;

   gettimeofday(&entertime, 0);
   gettimeofday(&currtime, 0);

   __int64 to;
   if (NULL == timeout)
      to = (__int64)1 << 62;
   else
      to = timeout->tv_sec * 1000000 + timeout->tv_usec;

   __int32 count = 0;

   do
   {
      CUDTSocket* s;

      // query read sockets
      if (NULL != readfds)
         for (set<UDTSOCKET>::iterator i = readfds->begin(); i != readfds->end(); ++ i)
         {
            if (NULL == (s = locate(*i)))
               throw CUDTException(5, 4, 0);

            if ((s->m_pUDT->m_bConnected && (s->m_pUDT->m_pRcvBuffer->getRcvDataSize() > 0))
               || (!s->m_pUDT->m_bListening && (s->m_pUDT->m_bBroken || !s->m_pUDT->m_bConnected))
               || (s->m_pUDT->m_bListening && (s->m_pQueuedSockets->size() > 0)))
               ++ count;
         }

      // query write sockets
      if (NULL != writefds)
         for (set<UDTSOCKET>::iterator i = writefds->begin(); i != writefds->end(); ++ i)
         {
            if (NULL == (s = locate(*i)))
               throw CUDTException(5, 4, 0);

            if (s->m_pUDT->m_bConnected && (s->m_pUDT->m_pSndBuffer->getCurrBufSize() < s->m_pUDT->m_iSndQueueLimit))
               count ++;
         }

      // query expections on sockets
      /*
      if (NULL != exceptfds)
         for (set<UDTSOCKET>::iterator i = exceptfds->begin(); i != exceptfds->end(); ++ i)
         {
            if (NULL == (s = locate(*i)))
               throw CUDTException(5, 4, 0);

            // check connection request status
               count ++;
         }
      */

      if (0 < count)
         break;

      #ifndef WIN32
         usleep(10);
      #else
         Sleep(1);
      #endif

      gettimeofday(&currtime, 0);

   } while (to > ((currtime.tv_sec - entertime.tv_sec) * 1000000 + currtime.tv_usec - entertime.tv_usec));

   if (0 < count)
   {
      CUDTSocket* s;

      count = 0;

      // query read sockets
      if (NULL != readfds)
         for (set<UDTSOCKET>::iterator i = readfds->begin(); i != readfds->end(); ++ i)
         {
            if (NULL == (s = locate(*i)))
               throw CUDTException(5, 4, 0);

            if ((s->m_pUDT->m_bConnected && (s->m_pUDT->m_pRcvBuffer->getRcvDataSize() > 0))
               || (!s->m_pUDT->m_bListening && (s->m_pUDT->m_bBroken || !s->m_pUDT->m_bConnected))
               || (s->m_pUDT->m_bListening && (s->m_pQueuedSockets->size() > 0)))
               ++ count;
         }

      // query write sockets
      if (NULL != writefds)
         for (set<UDTSOCKET>::iterator i = writefds->begin(); i != writefds->end(); ++ i)
         {
            if (NULL == (s = locate(*i)))
               throw CUDTException(5, 4, 0);

            if (s->m_pUDT->m_bConnected && (s->m_pUDT->m_pSndBuffer->getCurrBufSize() < s->m_pUDT->m_iSndQueueLimit))
               count ++;
         }

      // query expections on sockets
      /*
      if (NULL != exceptfds)
         for (set<UDTSOCKET>::iterator i = exceptfds->begin(); i != exceptfds->end(); ++ i)
         {
            if (NULL == (s = locate(*i)))
               throw CUDTException(5, 4, 0);

            // check connection request status
               count ++;
         }
      */
   }

   return count;
}

CUDTSocket* CUDTUnited::locate(const UDTSOCKET u)
{
   CGuard cg(m_ControlLock);

   map<UDTSOCKET, CUDTSocket*>::iterator i = m_Sockets.find(u);

   if (i == m_Sockets.end())
      return NULL;
   else
      return i->second;
}

CUDTSocket* CUDTUnited::locate(const UDTSOCKET u, const sockaddr* peer)
{
   CGuard cg(m_ControlLock);

   map<UDTSOCKET, CUDTSocket*>::iterator i = m_Sockets.find(u);

   // look up the "peer" address in queued sockets set
   for (set<UDTSOCKET>::iterator j = i->second->m_pQueuedSockets->begin(); j != i->second->m_pQueuedSockets->end(); ++ j)
   {
      map<UDTSOCKET, CUDTSocket*>::iterator k = m_Sockets.find(*j);

      if (4 == i->second->m_iIPversion)
      {
         // compare IPv4 address
         if ((((sockaddr_in*)peer)->sin_port == ((sockaddr_in*)k->second->m_pPeerAddr)->sin_port) && (((sockaddr_in*)peer)->sin_addr.s_addr == ((sockaddr_in*)k->second->m_pPeerAddr)->sin_addr.s_addr))
            return k->second;
      }
      else
      {
         // compare IPv6 address
         if (((sockaddr_in6*)peer)->sin6_port == ((sockaddr_in6*)k->second->m_pPeerAddr)->sin6_port)
         {
            __int32* addr1 = (__int32*)&(((sockaddr_in6*)peer)->sin6_addr);
            __int32* addr2 = (__int32*)&(((sockaddr_in6*)k->second->m_pPeerAddr)->sin6_addr);

            __int32 m = 4;
            for (; m > 0; -- m)
               if (addr1[m] != addr2[m])
                  break;

            if (m > 0)
               return k->second;
         }
      }
   }

   // look up the "peer" address in accepted sockets
   for (set<UDTSOCKET>::iterator j = i->second->m_pAcceptSockets->begin(); j != i->second->m_pAcceptSockets->end(); ++ j)
   {
      map<UDTSOCKET, CUDTSocket*>::iterator k = m_Sockets.find(*j);

      if (4 == i->second->m_iIPversion)
      {
         // compare IPv4 address
         if ((((sockaddr_in*)peer)->sin_port == ((sockaddr_in*)k->second->m_pPeerAddr)->sin_port) && (((sockaddr_in*)peer)->sin_addr.s_addr == ((sockaddr_in*)k->second->m_pPeerAddr)->sin_addr.s_addr))
            return k->second;
      }
      else
      {
         // compare IPv6 address
         if (((sockaddr_in6*)peer)->sin6_port == ((sockaddr_in6*)k->second->m_pPeerAddr)->sin6_port)
         {
            __int32* addr1 = (__int32*)&(((sockaddr_in6*)peer)->sin6_addr);
            __int32* addr2 = (__int32*)&(((sockaddr_in6*)k->second->m_pPeerAddr)->sin6_addr);

            __int32 m = 4;
            for (; m > 0; -- m)
               if (addr1[m] != addr2[m])
                  break;

            if (m > 0)
               return k->second;
         }
      }
   }

   return NULL;
}

void CUDTUnited::checkBrokenSockets()
{
   CGuard cg(m_ControlLock);

   // set of sockets To Be Removed
   set<UDTSOCKET> tbr;

   for (map<UDTSOCKET, CUDTSocket*>::iterator i = m_Sockets.begin(); i != m_Sockets.end(); ++ i)
   {
      if (CUDTSocket::CLOSED != i->second->m_Status)
      {
         // garbage collection
         if ((i->second->m_pUDT->m_bBroken) && (0 == i->second->m_pUDT->m_pRcvBuffer->getRcvDataSize()))
         {
            //close broken connections and start removal timer
            i->second->m_Status = CUDTSocket::CLOSED;
            gettimeofday(&i->second->m_TimeStamp, 0);

            // remove from listener's queue
            map<UDTSOCKET, CUDTSocket*>::iterator j = m_Sockets.find(i->second->m_ListenSocket);
            if (j != m_Sockets.end())
               j->second->m_pQueuedSockets->erase(i->second->m_Socket);
         }
      }
      else
      {
         // timeout, delete the socket
         timeval currtime;
         gettimeofday(&currtime, 0);
         // timeout 1-2 seconds to destroy a socket with broken connection
         if (currtime.tv_sec - i->second->m_TimeStamp.tv_sec >= 2)
            tbr.insert(i->second->m_Socket);

         // sockets cannot be removed here because it will invalidate the map iterator
      }
   }

   // remove those timeout sockets
   for (set<UDTSOCKET>::iterator i = tbr.begin(); i != tbr.end(); ++ i)
      removeSocket(*i);
}

void CUDTUnited::removeSocket(const UDTSOCKET u)
{
   map<UDTSOCKET, CUDTSocket*>::iterator i = m_Sockets.find(u);

   // invalid socket ID
   if (i == m_Sockets.end())
      return;

   if (0 != i->second->m_ListenSocket)
   {
      // if it is an accepted socket, remove it from the listener's queue
      map<UDTSOCKET, CUDTSocket*>::iterator j = m_Sockets.find(i->second->m_ListenSocket);

      if (j != m_Sockets.end())
         j->second->m_pAcceptSockets->erase(u);
   }
   else if (NULL != i->second->m_pQueuedSockets)
   {
      // if it is a listener, remove all un-accepted sockets in its queue
      for (set<UDTSOCKET>::iterator j = i->second->m_pQueuedSockets->begin(); j != i->second->m_pQueuedSockets->end(); ++ j)
      {
         m_Sockets[*j]->m_pUDT->close();
         delete m_Sockets[*j]->m_pUDT;
         delete m_Sockets[*j];
         m_Sockets.erase(*j);
      }
   }

   // delete this one
   m_Sockets[u]->m_pUDT->close();
   delete m_Sockets[u]->m_pUDT;
   delete m_Sockets[u];
   m_Sockets.erase(u);
}

void CUDTUnited::setError(CUDTException* e)
{
   #ifndef WIN32
      delete (CUDTException*)pthread_getspecific(m_TLSError);
      pthread_setspecific(m_TLSError, e);
   #else
      delete (CUDTException*)TlsGetValue(m_TLSError);
      TlsSetValue(m_TLSError, e);
   #endif
}

CUDTException* CUDTUnited::getError()
{
   #ifndef WIN32
      if(NULL == pthread_getspecific(m_TLSError))
         pthread_setspecific(m_TLSError, new CUDTException);
      return (CUDTException*)pthread_getspecific(m_TLSError);
   #else
      if(NULL == TlsGetValue(m_TLSError))
         TlsSetValue(m_TLSError, new CUDTException);
      return (CUDTException*)TlsGetValue(m_TLSError);
   #endif
}


//
UDTSOCKET CUDT::socket(int af, int, int)
{
   try
   {
      return s_UDTUnited.newSocket(af);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return INVALID_UDTSOCK;
   }
}

int CUDT::bind(UDTSOCKET u, const sockaddr* name, int namelen)
{
   try
   {
      return s_UDTUnited.bind(u, name, namelen);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

int CUDT::listen(UDTSOCKET u, int backlog)
{
   try
   {
      return s_UDTUnited.listen(u, backlog);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

UDTSOCKET CUDT::accept(UDTSOCKET u, sockaddr* addr, int* addrlen)
{
   try
   {
      return s_UDTUnited.accept(u, addr, addrlen);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return INVALID_UDTSOCK;
   }
}

int CUDT::connect(UDTSOCKET u, const sockaddr* name, int namelen)
{
   try
   {
      return s_UDTUnited.connect(u, name, namelen);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

int CUDT::close(UDTSOCKET u)
{
   try
   {
      return s_UDTUnited.close(u);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

int CUDT::getpeername(UDTSOCKET u, sockaddr* name, int* namelen)
{
   try
   {
      return s_UDTUnited.getpeername(u, name, namelen);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

int CUDT::getsockname(UDTSOCKET u, sockaddr* name, int* namelen)
{
   try
   {
      return s_UDTUnited.getsockname(u, name, namelen);;
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

int CUDT::getsockopt(UDTSOCKET u, int, UDTOpt optname, void* optval, int* optlen)
{
   try
   {
      CUDT* udt = s_UDTUnited.lookup(u);

      udt->getOpt(optname, optval, *optlen);

      return 0;
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

int CUDT::setsockopt(UDTSOCKET u, int, UDTOpt optname, const void* optval, int optlen)
{
   try
   {
      CUDT* udt = s_UDTUnited.lookup(u);

      udt->setOpt(optname, optval, optlen);

      return 0;
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

int CUDT::shutdown(UDTSOCKET u, int how)
{
   try
   {
      //CUDT* udt = s_UDTUnited.lookup(u);
      //udt->shutdown(how);
      return 0;
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

int CUDT::send(UDTSOCKET u, const char* buf, int len, int, int* handle, UDT_MEM_ROUTINE routine)
{
   try
   {
      CUDT* udt = s_UDTUnited.lookup(u);

      return udt->send((char*)buf, len, handle, routine);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

int CUDT::recv(UDTSOCKET u, char* buf, int len, int, int* handle, UDT_MEM_ROUTINE routine)
{
   try
   {
      CUDT* udt = s_UDTUnited.lookup(u);

      return udt->recv(buf, len, handle, routine);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

streampos CUDT::sendfile(UDTSOCKET u, ifstream& ifs, const streampos& offset, streampos& size, const int& block)
{
   try
   {
      CUDT* udt = s_UDTUnited.lookup(u);

      return udt->sendfile(ifs, offset, size, block);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

streampos CUDT::recvfile(UDTSOCKET u, ofstream& ofs, const streampos& offset, streampos& size, const int& block)
{
   try
   {
      CUDT* udt = s_UDTUnited.lookup(u);

      return udt->recvfile(ofs, offset, size, block);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

bool CUDT::getoverlappedresult(UDTSOCKET u, int handle, int& progress, bool wait)
{
   try
   {
      CUDT* udt = s_UDTUnited.lookup(u);

      return udt->getOverlappedResult(handle, progress, wait);
   }
   catch (CUDTException e)
   {
      // false and -1 means an error; false and positive value means incompleted IO.
      progress = -1;

      s_UDTUnited.setError(new CUDTException(e));
      return false;
   }
}

int CUDT::select(int, ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout)
{
   if ((NULL == readfds) && (NULL == writefds) && (NULL == exceptfds))
   {
      s_UDTUnited.setError(new CUDTException(5, 3, 0));
      return UDT_ERROR;
   }

   try
   {
      return s_UDTUnited.select(readfds, writefds, exceptfds, timeout);
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
}

CUDTException& CUDT::getlasterror()
{
   return *s_UDTUnited.getError();
}

int CUDT::perfmon(UDTSOCKET u, CPerfMon* perf)
{
#ifdef TRACE
   try
   {
      CUDT* udt = s_UDTUnited.lookup(u);

      udt->sample(perf);

      return 0;
   }
   catch (CUDTException e)
   {
      s_UDTUnited.setError(new CUDTException(e));
      return UDT_ERROR;
   }
#else
   s_UDTUnited.setError(new CUDTException(5, 0, 0));
   return UDT_ERROR;
#endif
}
