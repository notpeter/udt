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

   int intval;
   bool boolval;
   int vallen;


   boolval = false;
   client->setOpt(UDT_SNDSYN, &boolval, sizeof(bool));
   boolval = true;
   client->setOpt(UDT_RCVSYN, &boolval, sizeof(bool));
   intval = 1;
   client->setOpt(UDT_MFLAG, &intval, sizeof(int));
   
   intval = 1;
   //client->setOpt(UDT_TF, &intval, sizeof(int));

   intval = 25600;
   //client->setOpt(UDT_FC, &intval, sizeof(int));

   intval = 40960000;
   client->setOpt(UDT_BUF, &intval, sizeof(int));

   intval = 256000;
//   client->setOpt(UDT_USB, &intval, sizeof(int));
//   client->setOpt(UDT_URB, &intval, sizeof(int));

   intval = 4;
   client->setOpt(UDT_IPV, &intval, sizeof(int));

//   char* ip = "206.220.241.13";
//   server->setOpt(UDT_ADDR, ip, 16);

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
            usleep(100);
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

   try
   {
      while (client->getCurrSndBufSize())
         usleep(10);
   }
   catch(CUDTException e)
   {
      cout << "error msg: " << e.getErrorMessage();
      return 0;
   }

   gettimeofday(&time2, 0);
   cout << "speed = " << 60000.0 / double(time2.tv_sec - time1.tv_sec + (time2.tv_usec - time1.tv_usec) / 1000000.0) << "Mbits/sec" << endl;


   client->close();

   return 1;
}
