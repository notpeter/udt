#ifndef WIN32
#include <arpa/inet.h>
#endif
#include <iostream>
#include <cstdlib>
#include <udt.h>

using namespace std;
using namespace UDT;

int main(int argc, char* argv[])
{
   streampos size;

   #ifdef BSD
      if ((argc != 5) || (0 == atoi(argv[2])) || (0 == (size = strtoll(argv[4], NULL, 10))))
   #elif WIN32
      if ((argc != 5) || (0 == atoi(argv[2])) || (0 == (size = _atoi64(argv[4]))))
   #else
      if ((argc != 5) || (0 == atoi(argv[2])) || (0 == (size = atoll(argv[4]))))
   #endif
   {
      cout << "usage: recvfile server_ip server_port filename filesize" << endl;
      return 0;
   }

   UDTSOCKET fhandle = UDT::socket(AF_INET, SOCK_STREAM, 0);

   sockaddr_in serv_addr;
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(atoi(argv[2]));
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

   if (UDT_ERROR == UDT::connect(fhandle, (sockaddr*)&serv_addr, sizeof(serv_addr)))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   ofstream ofs(argv[3]);
   streampos recvsize; 

   if (UDT_ERROR == (recvsize = UDT::recvfile(fhandle, ofs, 0, size)))
   {
      cout << "recvfile: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   if (recvsize < size)
      cout << "recvfile: received file size (" << recvsize << ") is less than expected (" << size << ")." << endl; 

   UDT::close(fhandle);

   return 1;
}
