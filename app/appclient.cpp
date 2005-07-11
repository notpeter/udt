#ifndef WIN32
#include <unistd.h>
#include <cstdlib>
#include <arpa/inet.h>
#endif
#include <iostream>
#include <udt.h>
#include "cc.h"

using namespace std;

void DeleteBuf(char* buf, int) {delete [] buf;}

#ifndef WIN32
void* monitor(void*);
#else
DWORD WINAPI monitor(LPVOID);
#endif

int main(int argc, char* argv[])
{
   if ((3 != argc) || (0 == atoi(argv[2])))
   {
      cout << "usage: appclient server_ip server_port" << endl;
      return 0;
   }

   UDTSOCKET client = UDT::socket(AF_INET, SOCK_STREAM, 0);

   //for testing with custmized CC
   //UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));

#ifdef WIN32
   UDT::setsockopt(client, 0, UDT_MSS, new int(1052), sizeof(int));
#endif

   sockaddr_in serv_addr;
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(short(atoi(argv[2])));
#ifndef WIN32
   if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
#else
   if (INADDR_NONE == (serv_addr.sin_addr.s_addr = inet_addr(argv[1])))
#endif
   {
      cout << "incorrect network address.\n";
      return 0;
   }
   memset(&(serv_addr.sin_zero), '\0', 8);

   // connect to the server, implict bind
   if (UDT::ERROR == UDT::connect(client, (sockaddr*)&serv_addr, sizeof(serv_addr)))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   // using CC method
   //CUDPBlast* cchandle = NULL;
   //int temp;
   //UDT::getsockopt(client, 0, UDT_CC, &cchandle, &temp);
   //if (NULL != cchandle)
   //   cchandle->setRate(500);

   int size = 10000000;
   int handle = 0;
   char* data = new char[size];

#ifndef WIN32
   pthread_create(new pthread_t, NULL, monitor, &client);
#else
   CreateThread(NULL, 0, monitor, &client, 0, NULL);
#endif

   for (int i = 0; i < 1000; i ++)
   {
      //if (UDT::ERROR == UDT::send(client, new char[size], size, 0, &handle, DeleteBuf))
      if (UDT::ERROR == UDT::send(client, data, size, 0, &handle))
      {
         cout << "send: " << UDT::getlasterror().getErrorMessage() << endl;
         return 0;
      }
   }

   UDT::close(client);

   delete [] data;

   return 1;
}

#ifndef WIN32
void* monitor(void* s)
#else
DWORD WINAPI monitor(LPVOID s)
#endif
{
   UDTSOCKET u = *(UDTSOCKET*)s;

   UDT::TRACEINFO perf;

   cout << "SendRate(Mb/s) RTT(ms) FlowWindow PktSndPeriod(us) RecvACK RecvNAK" << endl;

   while (true)
   {
#ifndef WIN32
      sleep(1);
#else
      Sleep(1000);
#endif
      if (UDT::ERROR == UDT::perfmon(u, &perf))
      {
         cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
         break;
      }

      cout << perf.mbpsSendRate << "\t" 
           << perf.msRTT << "\t" 
           << perf.pktFlowWindow << "\t" 
           << perf.usPktSndPeriod << "\t" 
           << perf.pktRecvACK << "\t" 
           << perf.pktRecvNAK << endl;
   }

#ifndef WIN32
   return NULL;
#else
   return 0;
#endif
}
