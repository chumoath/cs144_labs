#include "stream_reassembler.hh"
#include <iostream>
using namespace std;

int
main(void){
    StreamReassembler sr(65000);
    sr.push_substring("b", 1, false);
    cout << "reassemble bytes: " << sr.stream_out().bytes_written() << endl;
    cout << sr.stream_out().eof() << endl;
    sr.push_substring("d", 3, false);
    cout << "reassemble bytes: " << sr.stream_out().bytes_written() << endl;
    cout << sr.stream_out().eof() << endl;
    sr.push_substring("c", 2, false);
    cout << "reassemble bytes: " << sr.stream_out().bytes_written() << endl;
    cout << sr.stream_out().read(0) << endl;
    cout << sr.stream_out().eof() << endl;


    sr.push_substring("a", 0, false);
    cout << "reassemble bytes: " << sr.stream_out().bytes_written() << endl;
    // cout << sr.stream_out().read(8) << endl;
    cout << sr.stream_out().eof() << endl;


        // Initialized (capacity = 65000)
        // Action:      substring submitted with data "b", index `1`, eof `0`
        // Expectation: net bytes assembled = 0
        // Expectation: stream_out().buffer_size() returned 0, and stream_out().read(0) returned the string ""
        // Expectation: not at EOF
        // Action:      substring submitted with data "d", index `3`, eof `0`
        // Expectation: net bytes assembled = 0
        // Expectation: stream_out().buffer_size() returned 0, and stream_out().read(0) returned the string ""
        // Expectation: not at EOF
        // Action:      substring submitted with data "c", index `2`, eof `0`
        // Expectation: net bytes assembled = 0
        // Expectation: stream_out().buffer_size() returned 0, and stream_out().read(0) returned the string ""
        // Expectation: not at EOF
        // Action:      substring submitted with data "a", index `0`, eof `0`
}