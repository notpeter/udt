#include <iostream>
#include <udt.h>

using namespace std;

int main(int argc, char* argv[])
{
   //usage: sendfile "filename"
   if (2 != argc)
   {
      cout << "usage: sendfile filename" << endl;
      return 0;
   }

   UDTSOCKET serv = UDT::socket(AF_INET, SOCK_STREAM, 0);

#ifdef WIN32
   int mss = 1052;
   UDT::setsockopt(serv, 0, UDT_MSS, &mss, sizeof(int));
#endif

   short port = 9000;

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

   CUDT::listen(serv, 1);

   int namelen;
   sockaddr_in their_addr;

   UDTSOCKET fhandle;

   if (UDT::INVALID_SOCK == (fhandle = UDT::accept(serv, (sockaddr*)&their_addr, &namelen)))
   {
      cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   UDT::close(serv);

   ifstream ifs(argv[1], ios::in | ios::binary);

   ifs.seekg(0, ios::end);
   __int64 size = ifs.tellg();
   ifs.seekg(0, ios::beg);

   UDT::TRACEINFO trace;
   UDT::perfmon(fhandle, &trace);

   if (UDT::ERROR == UDT::sendfile(fhandle, ifs, 0, size))
   {
      cout << "sendfile: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   UDT::perfmon(fhandle, &trace);
   cout << "speed = " << trace.mbpsSendRate << endl;

   UDT::close(fhandle);

   return 1;
}
