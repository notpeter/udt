// This program receives and then sends 734MB data
// work with appserver.cpp
// Usage: appclient server_ip server_port

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <udt.h>

int main(int argc, char* argv[])
{
   if ((3 != argc) || (0 == atoi(argv[2])))
   {
      cout << "usage: appclient server_ip server_port" << endl;
      return 0;
   }

   CUDT* client = new CUDT;

//   char* ip = "206.220.241.13";
//   client->setOpt(UDT_ADDR, ip, 16);

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


   timeval time0, time1, time2;
   int intval, vallen;
   char* data;

   int size = 7340000;

   data = new char[size];

   gettimeofday(&time0, 0);
   for (int i = 0; i < 1000; i++)
   {
      try
      {
         gettimeofday(&time1, 0);
         client->recv(data, size);
         gettimeofday(&time2, 0);
         cout << "recv " << i << "  ";
         cout << 7.34 * 8.0 / (time2.tv_sec - time1.tv_sec + (time2.tv_usec - time1.tv_usec) / 1000000.0) << "Mbps" << endl;
      }
      catch (CUDTException e)
      {
         cout << "error msg: " << e.getErrorMessage();
         return 0;
      }
   }
   gettimeofday(&time2, 0);
   cout << "speed = " << 60000.0 / double(time2.tv_sec - time0.tv_sec + (time2.tv_usec - time0.tv_usec) / 1000000.0) << "Mbits/sec" << endl;

   delete [] data;


   gettimeofday(&time1, 0);

   for (int i = 0; i < 1000; i ++)
   {
      data = new char[size];

      try
      {
         while (client->getCurrSndBufSize() > 40960000)
            usleep(10);
         client->send(data, size);

         client->getOpt(UDT_MFLAG, &intval, vallen);
         if (0 == intval)
            delete [] data;

         cout << "sent " << i << endl;
      }
      catch(CUDTException e)
      {
         cout << "error msg: " << e.getErrorMessage();
         return 0;
      }
   }

   client->close(CUDT::WAIT_SEND);

   gettimeofday(&time2, 0);
   cout << "speed = " << 60000.0 / double(time2.tv_sec - time1.tv_sec + (time2.tv_usec - time1.tv_usec) / 1000000.0) << "Mbits/sec" << endl;

   delete client;

   return 1;
}
