#include "UdpReceiver.h"
#include "FrameAssembler.h"

#include <chrono>
#include <errno.h>

UdpReceiver::UdpReceiver(bool lutBug) {
  console = spdlog::stdout_color_mt("console");
  spdlog::get("console")->set_level(spdlog::level::debug);
  this->lutBug = lutBug;
}

UdpReceiver::~UdpReceiver() {
  // stop(); //! TODO implement, delete some pointers?
}

bool UdpReceiver::initThread(const char *ipaddr, int UDP_Port) {
  print_affinity();
  set_cpu_affinity();
  print_affinity();
  if (set_scheduler() != 0) {
    spdlog::get("console")->error("Could not set scheduler");
  }

  (void) ipaddr; //! Purely to suppress the warning about ipaddr not being used.

  initSocket(); //! No arguments --> listens on all IP addresses
                //! Arguments --> IP address as const char *
  initFileDescriptorsAndBindToPorts(UDP_Port);

  FrameAssembler::lutInit(lutBug);

  for (int i = 0; i < config.number_of_chips; ++i) {
    frameAssembler[i] = new FrameAssembler(i);
    frameAssembler[i]->setFrameSetManager(fsm);
  }

  return true;
}

void UdpReceiver::run() {

  spdlog::get("console")->debug("Run started");

  int timeout_ms = int((timeout_us+0.5)/1000.); //! Round up
  spdlog::get("console")->debug("Poll timeout = {} us = {} ms", timeout_us, timeout_ms);

  struct epoll_event events[1024];

  long poll_count = 0, ev_count = 0, sum_p = 0, sum_r = 0, sum_a = 0;
  do {

    time_point begin = steady_clock::now();

    int ret = epoll_wait(epfd, events, 1024, timeout_ms);
    time_point polled = steady_clock::now();
    poll_count++;
    sum_p += std::chrono::duration_cast<us>(polled - begin).count();

    // Success
    if (ret > 0) {
        ev_count += ret;
      // An event on one of the fds has occurred.
        time_point l0 = polled;
      for (int j = 0; j < ret; j++) {
          uint32_t etype = events[j].events;
          if (! (etype & EPOLLIN)) {
            continue;
          }

          peer_t *peer = (peer_t*) events[j].data.ptr; //! Does this have to be an old style cast?
          int i = peer->chipIndex;
          /* This consists of 12 (packets_per_frame) packets (MTU = 9000 bytes).
             First 11 are 9000 bytes, the last one is 7560 bytes.
             Assuming no packet loss, extra fragmentation or MTU changing size*/
          long received_size =
              recv(peer->fd, inputQueues[i].data, max_packet_size, 0);
          time_point lr = steady_clock::now();
          inputQueues[i].chipIndex = i;
          inputQueues[i].size = received_size;

          ++packets; //! Count the number of packets received.

          frameAssembler[i]->onEvent(inputQueues[i]);
          time_point la = steady_clock::now();

          {
              sum_r += std::chrono::duration_cast<us>(lr - l0).count();
              sum_a += std::chrono::duration_cast<us>(la - lr).count();
              l0 = la;
          }
      }

    } else if (ret == -1 && errno != EINTR) {
      spdlog::get("console")->error("epoll_wait: ret = {}", ret);
    }

    if (poll_count == 1000) {
      spdlog::get("console")->info("Polls {}, time {}, evts {}, rcv {}, ass {} ", poll_count, sum_p, ev_count, sum_r, sum_a);
      poll_count = 0; ev_count = 0; sum_p = 0; sum_r = 0; sum_a = 0;
    }
  } while (!finished);
}

int UdpReceiver::set_scheduler() {
  int policy;
  struct sched_param sp = {.sched_priority = 99};
  struct timespec tp;
  int ret = sched_setscheduler(0, SCHED_FIFO, &sp);

  if (ret == -1) {
    spdlog::get("console")->error("Sched_setscheduler, ret = {}", ret);
    return 1;
  }

  policy = sched_getscheduler(0);

  switch (policy) {
  case SCHED_OTHER:
    spdlog::get("console")->debug("Policy is normal");
    break;

  case SCHED_RR:
    spdlog::get("console")->debug("Policy is round-robin");
    break;

  case SCHED_FIFO:
    spdlog::get("console")->debug("Policy is first-in, first-out");
    break;

  case -1:
    spdlog::get("console")->error("Sched_getscheduler");
    break;

  default:
    spdlog::get("console")->error("Unknown policy!");
  }

  ret = sched_getparam(0, &sp);

  if (ret == -1) {
    spdlog::get("console")->error("Sched_getparam, ret = {}", ret);
    return 1;
  }

  spdlog::get("console")->debug("Our priority is {}", sp.sched_priority);

  ret = sched_rr_get_interval(0, &tp);

  if (ret == -1) {
    spdlog::get("console")->error("Sched_rr_get_interval, ret = {}", ret);
    return 1;
  }

  spdlog::get("console")->debug("Our time quantum is {} milliseconds",
                                (tp.tv_sec * 1000.0f) +
                                    (tp.tv_nsec / 1000000.0f));

  return 0;
}

int UdpReceiver::print_affinity() {
  cpu_set_t mask;
  long nproc, i;

  int ret = sched_getaffinity(0, sizeof(cpu_set_t), &mask);

  if (ret == -1) {
    console->error("Sched_getaffinity, ret = {}", ret);
    return 1;
  } else {
    nproc = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.

    for (i = 0; i < nproc; i++) {
      console->debug("Sched_getaffinity --> Core {} = {}", i,
                                    CPU_ISSET(i, &mask));
    }
  }
  return 0;
}

// ! http://man7.org/linux/man-pages/man3/CPU_SET.3.html
int UdpReceiver::set_cpu_affinity() {
  cpu_set_t set; /* Define your cpu_set
                  bit mask. */
  int ret;
  long nproc = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.

  CPU_ZERO(&set);   /* Clears set, so that it
                     contains no CPUs */
  CPU_SET(0, &set); /* Add CPU cpu to set */
  // CPU_CLR(1, &set);                               /* Remove CPU cpu from set
  // */
  ret = sched_setaffinity(0, sizeof(cpu_set_t), &set); /* Set affinity of this
                                                        process to the defined
                                                        mask, i.e. only N */

  if (ret == -1) {
    spdlog::get("console")->error("Sched_setaffinity");
    return -1;
  }

  for (long i = 0; i < nproc; i++) {
    int cpu;

    cpu = CPU_ISSET(i, &set);
    spdlog::get("console")->debug("cpu {} is {}", i, (cpu ? "SET" : "unset"));
  }

  return 0;
}

unsigned int UdpReceiver::inet_addr(const char *str) {
  int a, b, c, d;
  char arr[4];
  sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d);
  arr[0] = char(a);
  arr[1] = char(b);
  arr[2] = char(c);
  arr[3] = char(d);
  return *(unsigned int *)arr;
}

bool UdpReceiver::initSocket(const char *inetIPAddr) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  listen_address = {0};
#pragma GCC diagnostic pop
  listen_address.sin_family = AF_INET;
  if (strcmp(inetIPAddr, "")) {
    listen_address.sin_addr.s_addr = inet_addr(inetIPAddr);
    spdlog::get("console")->info("Listening on {}", inetIPAddr);
  } else {
    listen_address.sin_addr.s_addr =
        htonl(INADDR_ANY); //! Address to accept any incoming messages
    spdlog::get("console")->info("Listening on ANY ADDRESS");
    //! Eg. inet_addr("127.0.0.1");
  }
  std::memset(peers, 0, sizeof(peers));
  return true;
}

bool UdpReceiver::initFileDescriptorsAndBindToPorts(int UDP_Port) {
  if ((epfd = epoll_create1(EPOLL_CLOEXEC)) == -1) {
     spdlog::get("console")->error("Make_epoll_fd");
     return false;
  }

  for (int i = 0; i < config.number_of_chips; i++) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
      spdlog::get("console")->error("Could not create socket");
      return false;
    }

    listen_address.sin_port = htons(UDP_Port + i);

    int ret = bind(fd, (struct sockaddr *)&listen_address,
                   socklen_t(sizeof(listen_address)));

    if (ret < 0) {
      spdlog::get("console")->error("Could not bind, port {}. Error code = {}",
                                    UDP_Port + i, strerror(errno));

      return false;
    }
    spdlog::get("console")->info("Socket created: {}\tBound port: {}", i,
                                 UDP_Port + i);
    peers[i].fd = fd;
    peers[i].chipIndex = i;
    struct epoll_event ee = { EPOLLIN, { &peers[i] } };
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ee)) {
        spdlog::get("console")->error("epoll_ctl");
        return false;
    }
  }
  return true;
}
