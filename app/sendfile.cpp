#include <iostream>
#include <udt.h>

int main(int argc, char* argv[])
{
   //usage: sendfile "filename"

   if (2 != argc)
   {
      cout << "usage: sendfile filename" << endl;
      return 0;
   }

   CUDT* sender = new CUDT;

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

   timeval t1, t2;

   gettimeofday(&t1, 0);

   ifstream ifs(argv[1]);

   ifs.seekg(0, ios::end);
   int size = ifs.tellg();
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

   gettimeofday(&t2, 0);

   cout << "speed = " << (double(size) * 8. / 1000000.) / (t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) / 1000000.) << endl;

   return 1;
}
