// This program sends a file using UDT
// Usage: sendfile filename

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

   CUDT* sender = new CUDT;

   #ifdef WIN32
      intval = 1052;
      client->setOpt(UDT_MTU, &intval, sizeof(int));
   #endif

   try
   {
      int port = sender->open(9000);
      cout << "sendfile is ready at port: " << port << endl;

      sender->listen();
   }
   catch(CUDTException e)
   {
      cout << "error message: " << e.getErrorMessage();
      return 0;
   }

   #ifdef TRACE
      sender->trace();
   #endif

   timeval t1, t2;
   gettimeofday(&t1, 0);

   ifstream ifs(argv[1]);

   ifs.seekg(0, ios::end);
   streamsize size = ifs.tellg();
   ifs.seekg(0, ios::beg);

   try
   {
      sender->sendfile(ifs, 0, size);
   }
   catch (CUDTException e)
   {
      cout << "error message: " << e.getErrorMessage();
      return 0;
   }

   sender->close();
   delete sender;

   gettimeofday(&t2, 0);

   cout << "speed = " << (double(size) * 8. / 1000000.) / (t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) / 1000000.) << endl;

   return 1;
}
