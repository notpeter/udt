// This program receives a file through UDT
// Usage: recvfile server_ip server_port filename filesize

#include <iostream>
#include <stdlib.h>
#include <udt.h>

int main(int argc, char* argv[])
{
   #ifdef BSD
      if ((argc != 5) || (0 == atoi(argv[2])) || (0 == strtoll(argv[4], NULL, 10)))
   #elif WIN32
      if ((argc != 5) || (0 == atoi(argv[2])) || (0 == _atoi64(argv[4])))
   #else
      if ((argc != 5) || (0 == atoi(argv[2])) || (0 == atoll(argv[4])))
   #endif
   {
      cout << "usage: recvfile server_ip server_port filename filesize" << endl;
      return 0;
   }

   CUDT* recver = new CUDT;

   try
   {
      recver->open();
      recver->connect(argv[1], atoi(argv[2]));
   }
   catch(CUDTException e)
   {
      cout << "error message: " << e.getErrorMessage();
      return 0;
   }

   #ifdef TRACE
      recver->trace();
   #endif

   ofstream ofs(argv[3]);

   try
   {
      #ifdef BSD
         recver->recvfile(ofs, 0, strtoll(argv[4], NULL, 10));
      #elif WIN32
         recver->recvfile(ofs, 0, _atoi64(argv[4]));
      #else
         recver->recvfile(ofs, 0, atoll(argv[4]));
      #endif
   }
   catch(CUDTException e)
   {
      cout << "error message: " << e.getErrorMessage();
      return 0;
   }

   recver->close();
   delete recver;

   return 1;
}
