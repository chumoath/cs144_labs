#include "socket.hh"
#include "util.hh"

#include <cstdlib>
#include <iostream>
#include <sstream>

using namespace std;

void get_URL(const string &host, const string &path) {
    // Your code here.

    // Socket 自动创建 socket 文件描述符，通过调用 socket()
    TCPSocket sock;

    try {
        sock.connect(Address(host, "http"));

    } catch (unix_error &e) {
        cout << e.what() << endl;
        exit(1);
    }

    // send requests

    ostringstream request;
    request << "GET " + path + " HTTP/1.1\r\n"
            << "Host: " + host + "\r\n"
            << "Connection: close\r\n"
            << "\r\n";

    sock.write(request.str());

    // receive reponse
    string reponse;

    while (!sock.eof()) {
        reponse = sock.read(1024);
        cout << reponse;
    }

    // sock 会调用 TCPSoceket 的析构函数 => 调用 Socket 的虚构函数 => 调用 FileDescriptor 的析构函数
    //               调用 shared_ptr 的析构函数，减少对该underlying fd 的 FDWrapper 的 引用计数
    //               FDWrapper 的 引用计数为 0， 即 没有 FileDescriptor 使用它，最后一个 shared_ptr 就会调用其析构函数
    //                   真正的关闭
    //               添加了抽象层，可以复用 fd
    //               使用 socket，不需要一系列的配置，可以直接使用
    //               Address，可以直接做各种地址的转换，很方便
    //                  资源自动释放，只需要管理逻辑即可
    //               Socket 提供了 所有 socket 的公共功能(bind connect shutdown socket)，所以用作基类

    // You will need to connect to the "http" service on
    // the computer whose name is in the "host" string,
    // then request the URL path given in the "path" string.

    // Then you'll need to print out everything the server sends back,
    // (not just one call to read() -- everything) until you reach
    // the "eof" (end of file).

    // cerr << "Function called: get_URL(" << host << ", " << path << ").\n";
    // cerr << "Warning: get_URL() has not been implemented yet.\n";
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
