#include "byte_stream.hh"

#include <iostream>
using namespace std;

int main(int argc, const char *argv[]) {
    cout << argc << endl;
    cout << argv[0] << endl;
    ByteStream bs(2000);
    cout << "isEmpty: " << bs.buffer_empty() << endl;
    bs.write("hello world");
    cout << "size: " << bs.buffer_size() << endl;

    cout << "read: " << bs.read(2) << endl;

    cout << "size: " << bs.buffer_size() << endl;

    cout << "eof: " << bs.eof() << endl;
}