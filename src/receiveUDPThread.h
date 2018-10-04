#ifndef RECEIVEUDPTHREAD_H
#define RECEIVEUDPTHREAD_H

#include <arpa/inet.h>
#include <chrono>
#include <math.h> /* ceil */
#include <netinet/in.h>
#include <sys/poll.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <thread>

#include "TestDriver.h"

class receiveUDPThread : std::thread {

public:
    receiveUDPThread(); //! TODO add parent pointer
    virtual ~receiveUDPThread();
    bool initThread(const char * ipaddr, int UDP_Port);
    int run();

    int set_scheduler();
    int print_affinity();
    int set_cpu_affinity();

    void setPollTimeout(int timeout){timeout_ms = int(timeout/1E3);}

    bool isFinished() {return finished;}

    struct sockaddr_in listen_address; // My address
    struct pollfd fds[number_of_chips];

    constexpr static int max_packet_size = 9000;
    constexpr static int max_buffer_size =
        (11 * max_packet_size) + 7560; //! [bytes] You can check this on Wireshark,
                                       //! by triggering 1 frame readout

    uint64_t packets = 0, frames = 0;

    int infoIndex = 0;
    char infoHeader[MPX_PIXEL_COLUMNS/8]; //! Single info header (Contains an OMR)
    bool isCounterhFrame = false;

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wfloat-conversion"
    const int packets_per_frame = ceil(106560. / double(max_packet_size));
    /* Packets per frame = number of bytes without network headers (106560) for
        one frame (one chip) for that acquisition (typically 1/4 of the final
        image), divided by the MTU (9000 bytes).
       Henk seems to think that this changes size even though it shouldn't unless
        the link is lossy which it isn't obviously...*/
    #pragma GCC diagnostic pop

    uint64_t last_frame_number = 0;

private:
    unsigned int inet_addr(const char *str);
    bool initSocket(const char *inetIPAddr = "");
    bool initFileDescriptorsAndBindToPorts(int UDP_Port);
    uint64_t calculateNumberOfFrames(uint64_t packets, int number_of_chips,
                                     int packets_per_frame);
    void printDebuggingOutput(uint64_t packets, uint64_t frames,
                              time_point begin, uint64_t nr_of_triggers);
    void printEndOfRunInformation(uint64_t frames, uint64_t packets,
                                  time_point begin, int triggers,
                                  int trig_length_us, int trig_deadtime_us, bool readoutMode_sequential);
    void doEndOfRunTests(int number_of_chips, uint64_t pMID, uint64_t pSOR, uint64_t pEOR, uint64_t pSOF, uint64_t pEOF, uint64_t iMID, uint64_t iSOF, uint64_t iEOF, uint64_t def);

    void hexdumpAndParsePacket(uint64_t *pixel_packet, int counter_bits, bool skip_data_packets, int chip);

    int timeout_ms;

    bool finished = false;
};
#endif // RECEIVEUDPTHREAD_H
