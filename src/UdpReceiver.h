#ifndef RECEIVEUDPTHREAD_H
#define RECEIVEUDPTHREAD_H

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <math.h> /* ceil */
#include <netinet/in.h>
#include <sys/epoll.h>
#include <signal.h>
#include <stdio.h>
#include <thread>
#include <unistd.h>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "FrameAssembler.h"
#include "PacketContainer.h"
#include "configs.h"

using time_point = std::chrono::steady_clock::time_point;
using steady_clock = std::chrono::steady_clock;

using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;

class FrameAssembler;

struct peer_t {
    int fd;
    int chipIndex;
};

class UdpReceiver {

public:
  UdpReceiver(bool lutBug); //! TODO add parent pointer
  virtual ~UdpReceiver();
  bool initThread(const char *ipaddr="", int UDP_Port=50000);
  void run();
  std::thread spawn() {
      return std::thread(&UdpReceiver::run, this);
  }

  int set_scheduler();
  int print_affinity();
  int set_cpu_affinity();

  void setPollTimeout(int timeout) { timeout_us = timeout; }

  bool isFinished() { return finished; }

  constexpr static int max_packet_size = 9000;
  // constexpr static int max_buffer_size =
  //    (11 * max_packet_size) +
  //    7560; //! [bytes] You can check this on Wireshark,
  //          //! by triggering 1 frame readout

  uint64_t packets = 0, frames = 0;

  FrameSetManager *getFrameSetManager() { return fsm; }
  uint64_t last_frame_number = 0;

private:
  unsigned int inet_addr(const char *str);
  bool initSocket(const char *inetIPAddr = "");
  bool initFileDescriptorsAndBindToPorts(int UDP_Port);

  std::shared_ptr<spdlog::logger> console;

  int timeout_us = 10000;

  bool finished = false;

  Config config;
  NetworkSettings networkSettings;

  struct sockaddr_in listen_address; // My address
  int epfd = -1;
  peer_t peers[Config::number_of_chips];

  bool lutBug = false;
  FrameSetManager *fsm = new FrameSetManager();
  PacketContainer inputQueues[Config::number_of_chips];
  FrameAssembler *frameAssembler[Config::number_of_chips];
};
#endif // RECEIVEUDPTHREAD_H
