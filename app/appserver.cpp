// This program starts as a server and then receives data until client closes.
// work with appclient.cpp
// Usage: appserver [server_port]

//
// This simple program is a quick introdution to UDT usage.
// There is no comments in its counterpart, appclient.cpp, if you want to read clean source codes.
//

#ifndef WIN32
   #include <unistd.h>
   #include <stdlib.h>
#endif

#include <iostream>
#include <udt.h>

using namespace std;

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
      // set up UDT configurations
      // all options have default values, which works well for most networks.
      // This is only for demonstration, you may not need to change them in real applications.
      // Currently options can be set only before UDT is connected.

      intval = 9000;
      server->setOpt(UDT_PORT, &intval, sizeof(int));
      boolval = false;
      server->setOpt(UDT_PCH, &boolval, sizeof(bool));

      // Maximum flow window size, similar as the maximum TCP control window size
      //DO NOT touch this if you are not quite familiar with the protocol mechanism ;-)
      intval = 100000;
      server->setOpt(UDT_FC, &intval, sizeof(int));

      //UDT protocol buffer that temporally stores received data before you call "recv/recvfile"
      //larger is better, but not too large to affect the system performance
      intval = 91920000;
      server->setOpt(UDT_BUF, &intval, sizeof(int));

      // UDP sending and receiving buffer
      //intval = 256000;
      //server->setOpt(UDT_USB, &intval, sizeof(int));
      //server->setOpt(UDT_URB, &intval, sizeof(int));

      // Must set up MTU if the current MTU setting is not 1500
      //intval = 1500;
      //server->setOpt(UDT_MTU, &intval, sizeof(int));

      // May be useful on host with multiple IP addresses
      //char* ip = "145.146.98.41";
      //server->setOpt(UDT_ADDR, ip, 16);
   }
   catch (CUDTException e)
   {
      //UDT uses C++ exception to feedback error and exceptions.
      //The try...catch... structure should be used for error detection.
      //CUDTException class is defined in UDT.
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


      // read UDT configurations
      //can be called anywhere after step 1.

      //cout.setf(ios::boolalpha);
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
      server->getOpt(UDT_MTU, &intval, vallen);
      cout << "Maximum Transmission Unit (MTU): " << intval << endl;


   #ifdef TRACE
      server->trace();
   #endif


//step 5: data sending/receiving.
   //now you can use UDT to send/receive data
   //you may need to modify the send/recv codes to adapt to the blocking/nonblocking configuration.

   timeval time1, time2;
   int blocknum = 0;
   char* data;
   int size = 10000000;
   data = new char[size];

   gettimeofday(&time1, 0);
   while (true)
   {
      try
      {
         server->recv(data, size);
         blocknum ++;
      }
      catch (CUDTException e)
      {
         break;
      }
   }
   gettimeofday(&time2, 0);

   cout << "speed = " << blocknum * (size/1000000) * 8 / double(time2.tv_sec - time1.tv_sec + (time2.tv_usec - time1.tv_usec) / 1000000.0) << "Mbits/sec" << endl;

   delete [] data;


//step 6: close the UDT connection.
   //this is the end.

   server->close();
   delete server;

   return 1;
}
