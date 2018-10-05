#ifndef RECEIVEUDPTHREAD_H
#define RECEIVEUDPTHREAD_H

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <math.h> /* ceil */
#include <netinet/in.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <thread>
#include <unistd.h>

#include "spdlog/spdlog.h"

#include "configs.h"

using time_point = std::chrono::steady_clock::time_point;
using steady_clock = std::chrono::steady_clock;

using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;

class receiveUDPThread : std::thread {

public:
  receiveUDPThread(); //! TODO add parent pointer
  virtual ~receiveUDPThread();
  bool initThread(const char *ipaddr, int UDP_Port);
  int run();

  int set_scheduler();
  int print_affinity();
  int set_cpu_affinity();

  void setPollTimeout(int timeout) { timeout_us = timeout; }

  bool isFinished() { return finished; }

  constexpr static int max_packet_size = 9000;
  //constexpr static int max_buffer_size =
  //    (11 * max_packet_size) +
  //    7560; //! [bytes] You can check this on Wireshark,
  //          //! by triggering 1 frame readout

  typedef struct {
      int chipIndex;
      long size;
      char data[max_packet_size];
  } PacketContainer;

  uint64_t packets = 0, frames = 0;

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
  void printDebuggingOutput(uint64_t packets, uint64_t frames, time_point begin,
                            uint64_t nr_of_triggers);
  void printEndOfRunInformation(uint64_t frames, uint64_t packets,
                                time_point begin, int triggers,
                                int trig_length_us, int trig_deadtime_us,
                                bool readoutMode_sequential);
  void doEndOfRunTests(int number_of_chips, uint64_t pMID, uint64_t pSOR,
                       uint64_t pEOR, uint64_t pSOF, uint64_t pEOF,
                       uint64_t iMID, uint64_t iSOF, uint64_t iEOF,
                       uint64_t def);

  int timeout_us;

  bool finished = false;

  Config config;
  NetworkSettings networkSettings;

  struct sockaddr_in listen_address; // My address
  struct pollfd fds[Config::number_of_chips];
};
#endif // RECEIVEUDPTHREAD_H
