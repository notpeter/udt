#include <iostream>
#include <stdlib.h>
#include <udt.h>

int main(int argc, char* argv[])
{
   if ((argc != 5) || (0 == atoi(argv[2])) || (0 == atoi(argv[4])))
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

   ofstream ofs(argv[3]);

   try
   {
      recver->recvfile(ofs, 0, atoi(argv[4]));
   }
   catch(CUDTException e)
   {
      cout << "error message: " << e.getErrorMessage();
      return 0;
   }

   recver->close();

   return 1;
}
