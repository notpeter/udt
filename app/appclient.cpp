//
// This program sends data to a server, and output speed sample
// work with appserver.cpp
// tune the "g_time" and "g_interval" variable below for run time and sampling interval
//

#ifndef WIN32
   #include <unistd.h>
   #include <stdlib.h>
#endif

#include <iostream>
#include <udt.h>

int g_time = 100; // seconds

#ifndef TRACE
   int g_interval = 1000000; //microseconds
   static int g_sentdata = 0;
   #ifndef WIN32
      void* monitor(void*);
   #else
      DWORD WINAPI monitor(LPVOID);
   #endif
#endif

int main(int argc, char* argv[])
{
   if ((3 != argc) || (0 == atoi(argv[2])))
   {
      cout << "usage: appclient server_ip server_port" << endl;
      return 0;
   }

   CUDT* client = new CUDT;

   int intval = 100000;
   client->setOpt(UDT_FC, &intval, sizeof(int));
   intval = 91920000;
   client->setOpt(UDT_BUF, &intval, sizeof(int));
   //intval = 256000;
   //client->setOpt(UDT_USB, &intval, sizeof(int));
   //client->setOpt(UDT_URB, &intval, sizeof(int));

   #ifdef WIN32
      intval = 1052;
      client->setOpt(UDT_MTU, &intval, sizeof(int));
   #endif

   //char* ip = "206.220.241.13";
   //client->setOpt(UDT_ADDR, ip, 16);

   try
   {
      client->open();
      client->connect(argv[1], atoi(argv[2]), 1000000);
   }
   catch (CUDTException e)
   {
      cout << "error msg: " << e.getErrorMessage();
      return 0;
   }

   #ifdef TRACE
      client->trace();
   #else
      #ifndef WIN32
         pthread_t perf_monitor;
         pthread_create(&perf_monitor, NULL, monitor, client);
      #else
         CreateThread(NULL, 0, monitor, client, 0, NULL);
      #endif
   #endif

   int vallen;
   char* data;
   int size = 10000000;
   int blocknum = 0;

   timeval time1, time2;
   gettimeofday(&time1, 0);
   gettimeofday(&time2, 0);

   while ((time2.tv_sec - time1.tv_sec) + (time2.tv_usec - time1.tv_usec) / 1000000.0 < g_time)
   {
      data = new char[size];

      try
      {
         while (client->getCurrSndBufSize() > 91920000)
            #ifndef WIN32
               usleep(10);
            #else
               Sleep(1);
            #endif
         client->send(data, size);
         #ifndef TRACE
            g_sentdata += size;
         #endif
         blocknum ++;

         client->getOpt(UDT_MFLAG, &intval, vallen);
         if (0 == intval)
            delete [] data;
      }
      catch(CUDTException e)
      {
         cout << "error msg: " << e.getErrorMessage();
         return 0;
      }

      gettimeofday(&time2, 0);
   }

   client->close(CUDT::WAIT_SEND);

   cout << "Avg Speed = " << blocknum * (size/1000000.0) * 8 / g_time << "Mbps" << endl;

   delete client;

   return 1;
}

#ifndef TRACE
   #ifndef WIN32
      void* monitor(void* udt_instance)
   #else
      DWORD WINAPI monitor(LPVOID udt_instance)
   #endif
{
   CUDT* udt = static_cast<CUDT*>(udt_instance);

   int sentdata = 0;

   while (true)
   {
      #ifndef WIN32
         usleep(g_interval);
      #else
         Sleep(g_interval / 1000);
      #endif
      cout << " " << (g_sentdata - (udt->getCurrSndBufSize() - sentdata)) * 8.0 / double(g_interval) << "Mbps" << endl;
      g_sentdata = 0;
      sentdata = udt->getCurrSndBufSize();
   }

   return NULL;
}
#endif
