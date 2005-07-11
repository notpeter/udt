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
UDT "C" socket API
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [ygu@cs.uic.edu], last updated 02/17/2005

modified by
   <programmer's name, programmer's email, last updated mm/dd/yyyy>
   <descrition of changes>
*****************************************************************************/


#ifdef CAPI

#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <dlfcn.h>
#include <iostream>

#include "udt.h"

const static char* const g_pcSysLibPath = "libc.so.6";
const CSysLib g_SysLib;

CSysLib::CSysLib()
{
   m_pHSysSockLib = dlopen(g_pcSysLibPath, RTLD_NOW);
   if (NULL == m_pHSysSockLib)
   {
      cerr << "UDT: cannot load " << g_pcSysLibPath << endl;
      exit(0);
   }

   socket = (int(*)(int, int, int))dlsym(m_pHSysSockLib, "socket");
   bind = (int(*)(int, const struct sockaddr*, unsigned int))dlsym(m_pHSysSockLib, "bind");
   listen = (int(*)(int, int))dlsym(m_pHSysSockLib, "listen");
   accept = (int(*)(int, struct sockaddr*, socklen_t*))dlsym(m_pHSysSockLib, "accept");
   connect = (int(*)(int, const struct sockaddr*, socklen_t))dlsym(m_pHSysSockLib, "connect");
   close = (int(*)(int))dlsym(m_pHSysSockLib, "close");
   shutdown = (int(*)(int, int))dlsym(m_pHSysSockLib, "shutdown");
   send = (ssize_t(*)(int, const void*, unsigned int, int))dlsym(m_pHSysSockLib, "send");
   recv = (ssize_t(*)(int, void*, unsigned int, int))dlsym(m_pHSysSockLib, "recv");
   write = (ssize_t(*)(int, const void*, size_t))dlsym(m_pHSysSockLib, "write");
   read = (ssize_t(*)(int, void*, size_t))dlsym(m_pHSysSockLib, "read");
   writev = (int(*)(int, const struct iovec*, size_t))dlsym(m_pHSysSockLib, "writev");
   readv = (int(*)(int, const struct iovec*, size_t))dlsym(m_pHSysSockLib, "readv");
   sendfile = (ssize_t(*)(int, int, off_t*, size_t))dlsym(m_pHSysSockLib, "sendfile");
   getpeername = (int(*)(int, struct sockaddr*, socklen_t*))dlsym(m_pHSysSockLib, "getpeername");
   getsockname = (int(*)(int, struct sockaddr*, socklen_t*))dlsym(m_pHSysSockLib, "getsockname");
   getsockopt = (int(*)(int, int, int, void*, socklen_t*))dlsym(m_pHSysSockLib, "getsockopt");
   setsockopt = (int(*)(int, int, int, const void*, socklen_t))dlsym(m_pHSysSockLib, "setsockopt");
   fcntl = (int(*)(int, int, ...))dlsym(m_pHSysSockLib, "fcntl");
   select = (int(*)(int, fd_set*, fd_set*, fd_set*, struct timeval*))dlsym(m_pHSysSockLib, "select");
}

CSysLib::~CSysLib()
{
   dlclose(m_pHSysSockLib);
}


extern "C" 
{

int socket(int af, int type, int flag)
{
   if (SOCK_STREAM == type)
      return CUDT::socket(af, type, flag);

   return (*g_SysLib.socket)(af, type, flag);
}

int bind(int u, const struct sockaddr* name, unsigned int namelen)
{
   if (CUDT::isUSock(u))
      return CUDT::bind(u, name, namelen);

   return (*g_SysLib.bind)(u, name, namelen);
}

int listen(int u, int backlog)
{
   if (CUDT::isUSock(u))
      return CUDT::listen(u, backlog);

   return (*g_SysLib.listen)(u, backlog);
}

int accept(int u, struct sockaddr* addr, socklen_t* addrlen)
{
   if (CUDT::isUSock(u))
      return CUDT::accept(u, addr, (int*)addrlen);

   return (*g_SysLib.accept)(u, addr, addrlen);
}

int connect(int u, const struct sockaddr* name, socklen_t namelen)
{
   if (CUDT::isUSock(u))
      return CUDT::connect(u, name, namelen);

   return (*g_SysLib.connect)(u, name, namelen);
}

int close(int u)
{
   if (CUDT::isUSock(u))
      return CUDT::close(u);

   return (*g_SysLib.close)(u);
}

int getpeername(int u, struct sockaddr* name, socklen_t* namelen)
{
   if (CUDT::isUSock(u))
      return CUDT::getpeername(u, name, (int*)namelen);

   return (*g_SysLib.getpeername)(u, name, namelen);
}

int getsockname(int u, struct sockaddr* name, socklen_t* namelen)
{
   if (CUDT::isUSock(u))
      return CUDT::getsockname(u, name, (int*)namelen);

   return (*g_SysLib.getsockname)(u, name, namelen);
}

int getsockopt(int u, int level, int optname, void* optval, socklen_t* optlen)
{
   if (!CUDT::isUSock(u))
      return (*g_SysLib.getsockopt)(u, level, optname, optval, optlen);

   switch (optname)
   {
   case SO_SNDBUF:
      return CUDT::getsockopt(u, level, UDT_SNDBUF, optval, (int*)optlen);

   case SO_RCVBUF:
      return CUDT::getsockopt(u, level, UDT_RCVBUF, optval, (int*)optlen);

   case SO_LINGER:
      return CUDT::getsockopt(u, level, UDT_LINGER, optval, (int*)optlen);

   default:
      return 0;
   }
}

int setsockopt(int u, int level, int optname, const void* optval, socklen_t optlen)
{
   if (!CUDT::isUSock(u))
      return (*g_SysLib.setsockopt)(u, level, optname, optval, optlen);

   switch (optname)
   {
   case SO_SNDBUF:
      return CUDT::setsockopt(u, level, UDT_SNDBUF, optval, optlen);

   case SO_RCVBUF:
      return CUDT::setsockopt(u, level, UDT_RCVBUF, optval, optlen);

   case SO_LINGER:
      return CUDT::setsockopt(u, level, UDT_LINGER, optval, optlen);

   default:
      return 0;
   }
}

int fcntl(int fd, int cmd, ...)
{
   va_list ap;
   long arg;
   flock* lock;

   switch (cmd)
   {
   case F_DUPFD:
   case F_GETFD:
   case F_GETFL:
   case F_GETOWN:
   case F_SETOWN:
   //case F_GETSIG:
   //case F_SETSIG:
      return (*g_SysLib.fcntl)(fd, cmd);

   case F_SETFD:
   case F_SETFL:
      va_start(ap, cmd);
      arg = va_arg(ap, long);
      va_end(ap);

      if (CUDT::isUSock(fd))
      {
         if ((cmd == F_SETFL) && (arg == O_NONBLOCK))
         {
            bool syn = false;
            CUDT::setsockopt(fd, 0, UDT_SNDSYN, &syn, sizeof(bool));
            CUDT::setsockopt(fd, 0, UDT_RCVSYN, &syn, sizeof(bool));
            return 0;
         }
         return -1;
      }

      return (*g_SysLib.fcntl)(fd, cmd, arg);

   case F_GETLK:
   case F_SETLK:
   case F_SETLKW:
      va_start(ap, cmd);
      lock = va_arg(ap, flock*);
      va_end(ap);

      return (*g_SysLib.fcntl)(fd, cmd, lock);

   default:
      return -1;
   }
}

int shutdown(int u, int how)
{
   if (CUDT::isUSock(u))
      return CUDT::shutdown(u, how);

   return (*g_SysLib.shutdown)(u, how);
}

ssize_t send(int u, const void* buf, unsigned int len, int flags)
{
   if (CUDT::isUSock(u))
      return CUDT::send(u, (char*)buf, len, flags, NULL, NULL);

   return (*g_SysLib.send)(u, buf, len, flags);
}

ssize_t recv(int u, void* buf, unsigned int len, int flags)
{
   if (CUDT::isUSock(u))
      return CUDT::recv(u, (char*)buf, len, flags, NULL, NULL);

   return (*g_SysLib.recv)(u, buf, len, flags);
}

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
   if (!CUDT::isUSock(out_fd))
      return (*g_SysLib.sendfile)(out_fd, in_fd, offset, count);

   const unsigned __int32 unitsize = 367000;
   unsigned __int32 currsize = 0;
   char* buf = new char[unitsize];

   if (-1 == lseek(in_fd, *offset, SEEK_SET))
      return -1;

   while (count > currsize)
   {
      __int32 realsize = 0;
      __int32 handle;

      if (-1 == (realsize = (*g_SysLib.read)(in_fd, buf, unitsize)))
         break;

      if (-1 == CUDT::send(out_fd, buf, realsize, 0, &handle, NULL))
         break;

      currsize += realsize;
   }

   delete [] buf;

   *offset += currsize;

   if (0 == currsize)
      return -1;
   return currsize;
}

int select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
   // don't suppor select currently.

   cerr << "UDT: unable to execute select." << endl;
   exit(0);
}

}

#endif
