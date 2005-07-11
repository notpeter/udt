#ifndef WIN32
#include <unistd.h>
#include <cstdlib>
#include <arpa/inet.h>
#endif
#include <iostream>
#include <udt.h>
#include "cc.h"

using namespace std;

#ifndef WIN32
void* recvdata(void*);
#else
DWORD WINAPI recvdata(LPVOID);
#endif

int main(int argc, char* argv[])
{
   if ((1 != argc) && ((2 != argc) || (0 == atoi(argv[1]))))
   {
      cout << "usage: appserver [server_port]" << endl;
      return 0;
   }

   UDTSOCKET serv = UDT::socket(AF_INET, SOCK_STREAM, 0);

   // for testing with customized CC
   //UDT::setsockopt(serv, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));

   short port;
   if (2 == argc)
      port = short(atoi(argv[1]));
   else
      port = 9000;

   sockaddr_in my_addr;
   my_addr.sin_family = AF_INET;
   my_addr.sin_port = htons(port);
   my_addr.sin_addr.s_addr = INADDR_ANY;
   memset(&(my_addr.sin_zero), '\0', 8);

   if (UDT::ERROR == UDT::bind(serv, (sockaddr*)&my_addr, sizeof(my_addr)))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   cout << "server is ready at port: " << port << endl;

   if (UDT::ERROR == UDT::listen(serv, 10))
   {
      cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   int namelen;
   sockaddr_in their_addr;
   UDTSOCKET recver;

   while (true)
   {
      if (UDT::INVALID_SOCK == (recver = UDT::accept(serv, (sockaddr*)&their_addr, &namelen)))
      {
         cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
         return 0;
      }

#ifndef WIN32
      char ip[16];
      cout << "new connection: " << inet_ntop(AF_INET, &their_addr.sin_addr, ip, 16) << ":" << ntohs(their_addr.sin_port) << endl;
#else
      cout << "new connection: " << inet_ntoa(their_addr.sin_addr) << ":" << ntohs(their_addr.sin_port) << endl;
#endif

#ifndef WIN32
      pthread_t rcvthread;
      pthread_create(&rcvthread, NULL, recvdata, new UDTSOCKET(recver));
      pthread_detach(rcvthread);
#else
      CreateThread(NULL, 0, recvdata, new UDTSOCKET(recver), 0, NULL);
#endif
   }

   UDT::close(serv);

   return 1;
}

#ifndef WIN32
void* recvdata(void* usocket)
#else
DWORD WINAPI recvdata(LPVOID usocket)
#endif
{
   UDTSOCKET recver = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;

   char* data;
   int size = 10000000;
   data = new char[size];

   int handle;

   while (true)
   {
      if (UDT::ERROR == UDT::recv(recver, data, size, 0, &handle, NULL))
      {
         cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
         break;
      }
   }

   delete [] data;

   UDT::close(recver);

#ifndef WIN32
   return NULL;
#else
   return 0;
#endif
}
