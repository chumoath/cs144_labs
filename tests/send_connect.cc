#include "sender_harness.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace std;

int main() {
    try {
        auto rd = get_random_generator();

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            // invoke fill_window at init, send SYN because the state is at CLOSE
            TCPSenderTestHarness test{"SYN sent test", cfg};

            // the sender's status should be at SYN_SENT, because it sent SYN
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});

            // expect segment in queue, have SYN, no payload, have seqno with isn
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));

            // haven't be ack, so the number of  bytes in flight is 1
            test.execute(ExpectBytesInFlight{1});
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            // invoke  fill_window, switch CLOSE to SYN_SENT
            TCPSenderTestHarness test{"SYN acked test", cfg};
            // test whether at SYN_SENT status
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            // check the segment that sender have sent is valid or not, get segment from queue
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));

            // test not ackno's bytes, SYN
            test.execute(ExpectBytesInFlight{1});

            // simulate the receriver get segment's ack and win_size to sender : invoke ack_received
            // invoke fill_win, simulate the sender's action that when it receive ackno
            test.execute(AckReceived{WrappingInt32{isn + 1}});

            // check the state: it should be SYN_ACKED，bacause sender receive the ackno for SYN
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});

            // bacause the byte_stream have not input data, so no segment is expected
            test.execute(ExpectNoSegment{});

            // have no any segment is sent, so no any bytes in flight
            test.execute(ExpectBytesInFlight{0});
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            TCPSenderTestHarness test{"SYN -> wrong ack test", cfg};
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectBytesInFlight{1});

            // simulate the receiver convey the ackno and window_size that from segment
            //      invoke the fill_window function to reponse

            // expect ack is isn + 1, but provide one isn, so fill_window work nothing
            test.execute(AckReceived{WrappingInt32{isn}});

            // last ack is invalid, so still at SYN_SENT
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});

            // expect no segment to send
            test.execute(ExpectNoSegment{});

            // SYN is not acknoed, so the number of bytes in flight is 1
            test.execute(ExpectBytesInFlight{1});
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            TCPSenderTestHarness test{"SYN acked, data", cfg};
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectBytesInFlight{1});
            test.execute(AckReceived{WrappingInt32{isn + 1}});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectNoSegment{});
            test.execute(ExpectBytesInFlight{0});
            test.execute(WriteBytes{"abcdefgh"});
            test.execute(Tick{1});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectSegment{}.with_seqno(isn + 1).with_data("abcdefgh"));
            test.execute(ExpectBytesInFlight{8});
            test.execute(AckReceived{WrappingInt32{isn + 9}});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectNoSegment{});
            test.execute(ExpectBytesInFlight{0});
            test.execute(ExpectSeqno{WrappingInt32{isn + 9}});
        }

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
