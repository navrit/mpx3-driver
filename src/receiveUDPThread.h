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
#include <cstring>
#include <QVector>

#include "colors.h" // Just for pretty terminal colour printing

#include "SpidrController.h"

typedef std::chrono::steady_clock::time_point time_point;
typedef std::chrono::steady_clock steady_clock;
typedef std::chrono::microseconds us;
typedef std::chrono::nanoseconds ns;

//! See Table 54 (MPX3 Packet Format) - SPIDR Register Map
//! 64 bit masks because of the uint64_t data type
//! Same order as switch, case statement
const uint64_t PKT_TYPE_MASK     = 0xF000000000000000;

const uint64_t PIXEL_DATA_SOR    = 0xA000000000000000; //! #2
const uint64_t PIXEL_DATA_EOR    = 0xE000000000000000; //! #3
const uint64_t PIXEL_DATA_SOF    = 0xB000000000000000; //! #7
const uint64_t PIXEL_DATA_EOF    = 0x7000000000000000; //! #5
const uint64_t PIXEL_DATA_MID    = 0x3000000000000000; //! #1 Most frequent
const uint64_t INFO_HEADER_SOF   = 0x9000000000000000; //! #6
const uint64_t INFO_HEADER_MID   = 0x1000000000000000; //! #4
const uint64_t INFO_HEADER_EOF   = 0xD000000000000000; //! #8
//! End of Table 54
const uint64_t ROW_COUNT_MASK    = 0x0FF0000000000000;
const uint64_t ROW_COUNT_SHIFT   = 52;


const int MPX_PIXEL_COLUMNS      = 256;

const int number_of_chips = 4;
const int TCPPort = 50000;
//const std::string socketIPAddr = "127.0.0.1";
const std::string socketIPAddr = "192.168.100.10";
//const std::string socketIPAddr = "192.168.1.10";

SpidrController *spidrcontrol = nullptr;
struct sockaddr_in listen_address; // My address
struct pollfd fds[number_of_chips];

const int max_packet_size = 9000;
const int max_buffer_size =
    (11 * max_packet_size) + 7560; //! [bytes] You can check this on Wireshark,
                                   //! by triggering 1 frame readout
const int portno = 8192;

const int trig_mode = 4;          //! Auto-trigger mode
const int trig_length_us = 500;   //! [us]
const int trig_deadtime_us = 500; //! [us]
int trig_freq_mhz = 0; //! Set this depending on readoutMode_sequential later

const uint64_t nr_of_triggers = 100;
const int continuousRW_frequency = 2000;                        // Hz?
int timeout = (trig_length_us + trig_deadtime_us) / 1000; // ms?
const bool readoutMode_sequential = true;

int infoIndex = 0;
char infoHeader[MPX_PIXEL_COLUMNS/8]; //! Single info header (OMR)
bool isCounterhFrame = false;

uint64_t packets = 0, frames = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
const int packets_per_frame = ceil(106560. / 9000.);
/* Packets per frame = number of bytes without network headers (106560) for
    one frame (one chip) for that acquisition (typically 1/4 of the final
    image), divided by the MTU (9000 bytes).
   Henk seems to think that this changes size even though it shouldn't unless
    the link is lossy which it isn't obviously...*/
#pragma GCC diagnostic pop

int set_scheduler();
int print_affinity();
int set_cpu_affinity();

bool initSpidrController(std::string IPAddr, int port);
bool connectToDetector();
unsigned int inet_addr(char *str);
bool initSocket(const char *inetIPAddr = "");
void printReadoutMode();
bool initDetector();
bool initFileDescriptorsAndBindToPorts();
bool startTrigger(bool print = true);
uint64_t calculateNumberOfFrames(uint64_t packets, int number_of_chips,
                                 int packets_per_frame);
void printDebuggingOutput(uint64_t packets, int packets_per_frame,
                          int number_of_chips, uint64_t frames,
                          time_point begin);
void printEndOfRunInformation(uint64_t frames, uint64_t packets,
                              time_point begin, int triggers,
                              int trig_length_us, int trig_deadtime_us);
void doEndOfRunTests(int number_of_chips, uint64_t pMID, uint64_t pSOR, uint64_t pEOR, uint64_t pSOF, uint64_t pEOF, uint64_t iMID, uint64_t iSOF, uint64_t iEOF, uint64_t def);
void stopTrigger();
void cleanup(bool print = true);

#endif // RECEIVEUDPTHREAD_H
