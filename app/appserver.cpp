// This program sends and then receives 734MB data
// work with appclient.cpp
// Usage: appserver [server_port]

//
// This simple program is a quick introdution to UDT usage.
// There is no comments in its counterpart, appclient.cpp, if you 
// want to read clean source codes.
//

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <udt.h>

int main(int argc, char* argv[])
{
   if ((1 != argc) && ((2 != argc) || (0 == atoi(argv[1]))))
   {
      cout << "usage: appserver [server_port]" << endl;
      return 0;
   }

   //The order of the following step MUST NOT be changed unless it is exiplcitly explained.
   //

//step 1: declare and/or allocate a UDT class instance.

   CUDT* server = new CUDT;


//step 2: set options [optional]

   int intval;
   bool boolval;
   int vallen;

   try
   {
      //can be called anywhere after step 1.
      // read UDT configurations
      server->getOpt(UDT_PORT, &intval, vallen);
      cout << "current port number: " << intval << endl;
      server->getOpt(UDT_PCH, &boolval, vallen);
      cout << "current port number changable?: " << boolval << endl;

      server->getOpt(UDT_SNDSYN, &boolval, vallen);
      cout << "Sending is blocking?: " << boolval << endl;
      server->getOpt(UDT_RCVSYN, &boolval, vallen);
      cout << "Receiving is blocking?: " << boolval << endl;
      server->getOpt(UDT_MFLAG, &intval, vallen);
      cout << "Sent buffer is auto-released?: " << intval << endl;

      server->getOpt(UDT_FC, &intval, vallen);
      cout << "Maximum flow control window size: " << intval << endl;

      server->getOpt(UDT_BUF, &intval, vallen);
      cout << "UDT buffer size: " << intval << endl;

      server->getOpt(UDT_USB, &intval, vallen);
      cout << "UDP sending buffer size: " << intval << endl;

      server->getOpt(UDT_URB, &intval, vallen);
      cout << "UDP receiving buffer size: " << intval << endl;

      server->getOpt(UDT_IPV, &intval, vallen);
      cout << "Using IP version: " << "IPv" << intval << endl;


      // set up UDT configurations
      // all options have default values, which works well for most networks.
      // This is only for demonstration, you may not need to change them in real applications.
      // Currently options can be set only before UDT is connected.

      intval = 9000;
      server->setOpt(UDT_PORT, &intval, sizeof(int));
      boolval = false;
      server->setOpt(UDT_PCH, &boolval, sizeof(bool));

      // blocking or non-blocking, for sending and receiving, respectively
      boolval = false;
      server->setOpt(UDT_SNDSYN, &boolval, sizeof(bool));
      boolval = true;
      server->setOpt(UDT_RCVSYN, &boolval, sizeof(bool));
      // UDT can auto delete the sent buffer if necessary
      intval = 1;
      server->setOpt(UDT_MFLAG, &intval, sizeof(int));
   
      // Maximum flow window size, similar as the maximum TCP control window size
      //DO NOT touch this if you are not quite familiar with the protocol mechanism ;-)
      intval = 25600;
      //server->setOpt(UDT_FC, &intval, sizeof(int));

      //UDT protocol buffer that temporally stores received data before you call "recv/recvfile"
      //larger is better, but not too large to affect the system performance
      intval = 40960000;
      server->setOpt(UDT_BUF, &intval, sizeof(int));

      // UDP sending and receiving buffer
      intval = 256000;
      //server->setOpt(UDT_USB, &intval, sizeof(int));
      //server->setOpt(UDT_URB, &intval, sizeof(int));

      // IP version, 4 or 6
      intval = 4;
      server->setOpt(UDT_IPV, &intval, sizeof(int));

      // May be useful on host with multiple IP addresses
      //char* ip = "145.146.98.41";
      //server->setOpt(UDT_ADDR, ip, 16);
   }
   catch (CUDTException e)
   {
      cout << "error msg: " << e.getErrorMessage();
      return 0;
   }


//step 3: Initialize the UDT instance.

   int port;
   try
   {
      if (argc > 1)
         port = server->open(atoi(argv[1]));
      else
         port = server->open();
   }
   catch(CUDTException e)
   {
      cout << "error msg: " << e.getErrorMessage();
      return 0;
   }
   cout << "server is ready at port: " << port << endl;


//step 4: server listen(); client connect().

   try
   {
      server->listen();
   }
   catch(CUDTException e)
   {
      cout << "error msg: " << e.getErrorMessage();
      return 0;
   }


//step 5: data sending/receiving.
   //now you can use UDT to send/receive data
   //you may need to modify the send/recv codes to adapt to the blocking/nonblocking configuration.

   timeval time1, time2;

   char* data;
   int size = 7340000;

   gettimeofday(&time1, 0);

   for (int i = 0; i < 1000; i ++)
   {
      data = new char[size];

      try
      {
         while (server->getCurrSndBufSize() > 40960000)
            usleep(10);
         server->send(data, size);

         server->getOpt(UDT_MFLAG, &intval, vallen);
         if (0 == intval)
            delete [] data;

         cout << "sent block" << i << endl;
      }
      catch(CUDTException e)
      {
         cout << "error msg: " << e.getErrorMessage();
         return 0;
      }
   }

   //UDT uses C++ exception to feedback error and exceptions.
   //The try...catch... structure should be used for error detection.
   //CUDTException class is defined in UDT.

   //wait until all data are sent out
   try
   {
      while (server->getCurrSndBufSize() > 0)
         usleep(10);
   }
   catch(CUDTException e)
   {
      cout << "error msg: " << e.getErrorMessage();
      return 0;
   }

   gettimeofday(&time2, 0);
   cout << "speed = " << 60000.0 / double(time2.tv_sec - time1.tv_sec + (time2.tv_usec - time1.tv_usec) / 1000000.0) << "Mbits/sec" << endl;


   data = new char[size];

   gettimeofday(&time1, 0);
   for (int i = 0; i < 1000; i++)
   {
      try
      {
         server->recv(data, size);
         
         cout << "recv block " << i << endl;
      }
      catch (CUDTException e)
      {
         cout << "error msg: " << e.getErrorMessage();
         return 0;
      }
   }
   gettimeofday(&time2, 0);

   cout << "speed = " << 60000.0 / double(time2.tv_sec - time1.tv_sec + (time2.tv_usec - time1.tv_usec) / 1000000.0) << "Mbits/sec" << endl;

   delete [] data;


//step 6: close the UDT connection.
   //that is the end.

   server->close();
   delete server;

   return 1;
}
