#include "receiveUDPThread.h"

using time_point = std::chrono::steady_clock::time_point;
using steady_clock = std::chrono::steady_clock;

using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;

int main() {
  print_affinity();
  set_cpu_affinity();
  print_affinity();
  if (set_scheduler() != 0) {
      return false;
  }

  initSpidrController("127.0.0.1", TCPPort);
  connectToDetector();
  initSocket(); //! No arguments --> listens on all IP addresses
                //! Arguments --> IP address as const char *
  initDetector();
  initFileDescriptorsAndBindToPorts();
  startTrigger();

  // Make X input queues
  // These are the big fat ones that will need to be copied/shared to their
  // respective threads once they are full
  char inputQueues[number_of_chips][max_buffer_size];

  bool finished = false;
  int ret = 1;
  int received_size = 0;
  uint64_t *pixel_packet, pixel_word;
  uint64_t type;
  int sizeofuint64_t = sizeof(uint64_t);

  int tmp = 0;

  uint64_t pSOR = 0, pEOR = 0, pSOF = 0, pEOF = 0, pMID = 0, iEOF = 0, def = 0, bram = 0;

  time_point begin = steady_clock::now();
  printDebuggingOutput(packets, packets_per_frame, number_of_chips, calculateNumberOfFrames(packets, number_of_chips, packets_per_frame), begin);

  do {
    ret = poll(fds, number_of_chips, timeout);

    // Success
    if (ret > 0) {
      // An event on one of the fds has occurred.
      for (int i = 0; i < number_of_chips; ++i) {
        if (fds[i].revents & POLLIN) {
          /* This consists of 12 (packets_per_frame) packets (MTU = 9000 bytes).
             First 11 are 9000 bytes, the last one is 7560 bytes.
             Assuming no packet loss, extra fragmentation or MTU changing size*/
          received_size = recv(fds[i].fd, inputQueues[i], max_packet_size, 0);

          //! This shouldn't happen but Henk has this check in his code
          //! Maybe it's a good idea to keep it
          if (received_size <= 0) break;

          /* Count the number of packets received.
             Could calculate lost packets like Henk does, is this necessary? */
          ++packets;

          pixel_packet = (uint64_t *) inputQueues[i];
          for( int i = 0; i < received_size/sizeofuint64_t; ++i, ++pixel_packet) {
              // Reverse the byte order
              char *bytes = (char *) &pixel_word;
              char *p     = (char *) pixel_packet;
              for( int i=0; i < 8; ++i ) {
                  bytes[i] = p[7-i];
              }
              *pixel_packet = pixel_word;

              type = (*pixel_packet) & PKT_TYPE_MASK;

              switch (type) {
              case INFO_HEADER_SOF:
                  continue;
              case INFO_HEADER_MID:
                  continue;
              case INFO_HEADER_EOF:
                  ++iEOF;
                  break;
              case BRAM_SPECIAL_PKT:
                  /* The number of Bram packets (0x2000000000000000) is
                     always the same as pixel MID.
                    They must be related somehow... */
                  // DEV_DATA_COMPRESSED?
                  ++bram;
                  break;
              case PIXEL_DATA_SOR:
                  ++pSOR;
                  break;
              case PIXEL_DATA_EOR:
                  ++pEOR;
                  break;
              case PIXEL_DATA_SOF:
                  ++pSOF;
                  break;
              case PIXEL_DATA_EOF:
                  ++pEOF;
                  break;
              case PIXEL_DATA_MID:
                  ++pMID;
                  break;
              default:
                  // Some kind of fucking rubbish??
                  // Henk doesn't bother explaining what this is
                  // Maybe it's data, who knows
                  if (type != 0) {
                      std::cout << tmp << ": " << type << "\n";
                      ++tmp;
                  }
                  ++def;
                  break;
              }
          }
        }
      }

      frames = calculateNumberOfFrames(packets, number_of_chips, packets_per_frame);

      if (frames == nr_of_triggers) {
        printDebuggingOutput(packets, packets_per_frame, number_of_chips, frames, begin);
        printEndOfRunInformation(frames, packets, begin, nr_of_triggers, trig_length_us, trig_deadtime_us);
        printf("\nDefault\t\tiEOF\t\tpMID\tBram\tpSOR\tpEOR\tpSOF\tpEOF\n");
        printf("%-10lu\t%-10lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n\n", def/4, iEOF/4, pMID/4, bram/4, pSOR/4, pEOR/4, pSOF/4, pEOF/4);
        finished = true;
      } else {
        printDebuggingOutput(packets, packets_per_frame, number_of_chips, frames, begin);
      }
    } else if (ret == -1) {
      // An error occurred, never actually seen this triggered
      return false;
    }
  } while (!finished);

  return 0;
}

int set_scheduler() {
  int policy;
  struct sched_param sp = { .sched_priority = 99 };
  struct timespec    tp;
  int ret;

  ret = sched_setscheduler(0, SCHED_FIFO, &sp);

  if (ret == -1) {
    perror("sched_setscheduler");
    return 1;
  }

  policy = sched_getscheduler(0);

  switch (policy) {
  case SCHED_OTHER:
    printf("policy is normal\n");
    break;

  case SCHED_RR:
    printf("policy is round-robin\n");
    break;

  case SCHED_FIFO:
    printf("policy is first-in, first-out\n");
    break;

  case -1:
    perror("sched_getscheduler");
    break;

  default:
    fprintf(stderr, "Unknown policy!\n");
  }

  ret = sched_getparam(0, &sp);

  if (ret == -1) {
    perror("sched_getparam");
    return 1;
  }

  printf("Our priority is %d\n", sp.sched_priority);

  ret = sched_rr_get_interval(0, &tp);

  if (ret == -1) {
    perror("sched_rr_get_interval");
    return 1;
  }

  printf("Our time quantum is %.2lf milliseconds\n",
         (tp.tv_sec * 1000.0f) + (tp.tv_nsec / 1000000.0f));

  std::cout << "\n\n";

  return 0;
}

int print_affinity() {
  cpu_set_t mask;
  long nproc, i;

  if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
    perror("sched_getaffinity");
    return 1;
  } else {
    nproc = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.
    printf("sched_getaffinity = ");

    for (i = 0; i < nproc; i++) {
      printf("%d ", CPU_ISSET(i, &mask));
    }
    printf("\n");
  }
  std::cout << "\n\n";
  return 0;
}

// ! http://man7.org/linux/man-pages/man3/CPU_SET.3.html
int set_cpu_affinity()
{
  cpu_set_t set;                             /* Define your cpu_set
                                                bit mask. */
  int ret, i;
  int nproc = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.

  CPU_ZERO(&set);                                      /* Clears set, so that it
                                                          contains no CPUs */
  CPU_SET(4, &set);                                    /* Add CPU cpu to set */
  //CPU_CLR(1, &set);                               /* Remove CPU cpu from set */
  ret = sched_setaffinity(0, sizeof(cpu_set_t), &set); /* Set affinity of this
                                                          process to the defined
                                                          mask, i.e. only N */

  if (ret == -1) {
    perror("sched_setaffinity");
    return -1;
  }

  for (i = 0; i < nproc; i++) {
    int cpu;

    cpu = CPU_ISSET(i, &set);
    printf("cpu=%i is %s\n", i, cpu ? "set" : "unset");
  }

  std::cout << "\n";

  return 0;
}

bool initSpidrController(const char * IPAddr, int port)
{
    int a, b, c, d;
    sscanf(IPAddr, "%d.%d.%d.%d", &a, &b, &c, &d);
    spidrcontrol = new SpidrController(a, b, c, d, port);
    if (spidrcontrol != nullptr) {
        return true;
    } else {
        return false;
    }
}

bool connectToDetector()
{
    // Are we connected ?
    if (!spidrcontrol->isConnected()) {
      std::cout << BOLD(FRED("SpidrController :(\t"
                             << spidrcontrol->ipAddressString()
                             << ": "
                             << spidrcontrol->connectionStateString()
                             << ", "
                             << spidrcontrol->connectionErrString()
                             << "\n"));
      return false;
    } else {
      std::cout << BOLD(FGRN("SpidrController :)\t"));
      std::cout << spidrcontrol->ipAddressString()
                << "\t"
                << spidrcontrol->connectionStateString()
                << "\n";
      return true;
    }
}

unsigned int inet_addr(char *str)
{
    int a, b, c, d;
    char arr[4];
    sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d);
    arr[0] = a; arr[1] = b; arr[2] = c; arr[3] = d;
    return *(unsigned int *)arr;
}

bool initSocket(const char * inetIPAddr)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    listen_address = { 0 };
#pragma GCC diagnostic pop
    listen_address.sin_family = AF_INET;
    if (strcmp(inetIPAddr, "")) {
        listen_address.sin_addr.s_addr = inet_addr(inetIPAddr);
        std::cout << "\nListening on " << inetIPAddr << "\n";
    } else {
        listen_address.sin_addr.s_addr = htonl(INADDR_ANY); //! Address to accept any incoming messages
        std:: cout << "\nListening on ANY ADDRESS\n";
        //! Eg. inet_addr("127.0.0.1");
    }
    std::memset(fds, 0, sizeof(fds));
    return true;
}

void printReadoutMode()
{
    if (readoutMode_sequential) {
      std::cout << BOLD("\nSequential R/W readout mode\n");
    } else {
      std::cout << BOLD("\nContinuous R/W readout mode\n");
    }
}

bool initDetector()
{
    printReadoutMode();

    if (readoutMode_sequential) {
      spidrcontrol->stopAutoTrigger();

      for (int i = 0; i < number_of_chips; i++) {
        spidrcontrol->setContRdWr(i, false);
      }
    } else {
      spidrcontrol->stopContReadout();

      for (int i = 0; i < number_of_chips; i++) {
        spidrcontrol->setContRdWr(i, true);
      }
    }

    // Initialise SPIDR acquisition - trigger, number of links etc.
    for (int i = 0; i < number_of_chips; i++) {
      spidrcontrol->setPs(i, 3);
    }
    spidrcontrol->resetCounters();
    spidrcontrol->setLogLevel(0);
    spidrcontrol->setShutterTriggerConfig(trig_mode,
                                          trig_length_us,
                                          trig_freq_mhz,
                                          nr_of_triggers);
    std::cout << "\nTrigger data:" <<
      "\n\ttrig_mode = " << trig_mode << " " <<
      "\n\ttrig_length_us = " << trig_length_us << " " <<
      "\n\ttrig_freq_mhz = " << trig_freq_mhz << " " <<
      "\n\tnr_of_triggers = " << nr_of_triggers << "\n\n";

    return true;
}

bool initFileDescriptorsAndBindToPorts()
{
    for (int i = 0; i < number_of_chips; i++) {
      fds[i].fd     = socket(AF_INET, SOCK_DGRAM, 0);
      fds[i].events = POLLIN;

      if (fds[i].fd < 0) {
        std::cout << BOLD(FRED("Could not create socket"));
        return false;
      } else {
        std::cout << FGRN("Socket created: ") << i << "\t";
      }

      listen_address.sin_port = htons(portno + i);

      if (bind(fds[i].fd,
               (struct sockaddr *)&listen_address,
               (socklen_t)sizeof(listen_address)) < 0) {
        std::cout << BOLD(FRED("Could not bind"));
        return false;
      }
      std::cout << FGRN("Bound port:\t") << portno + i << "\n";
    }
    return true;
}

bool startTrigger(bool print)
{
    if (readoutMode_sequential) {
      spidrcontrol->startAutoTrigger();
    } else {
      spidrcontrol->startContReadout(continuousRW_frequency);
    }

    if (print) {
        std::cout << "\n[START]\n";
    }
    return true;
}

uint64_t calculateNumberOfFrames(uint64_t packets, int number_of_chips, int packets_per_frame)
{
    return (packets / number_of_chips / packets_per_frame);
}

void printDebuggingOutput(uint64_t packets, int packets_per_frame, int number_of_chips, uint64_t frames, time_point begin)
{
    // Some debugging output during a run
    // Print every 2%

    if ((packets % (packets_per_frame * number_of_chips) == 0) &&
        (frames % (nr_of_triggers / 50) == 0)) {
      time_point end = steady_clock::now();
      auto t = std::chrono::duration_cast<us>(end - begin).count();
      printf("%9lu/%-9d\t%-.0f%%\t%lus\t%lu packets\n", frames, nr_of_triggers,
             float(frames * 100. / nr_of_triggers), t / 1000000, packets);
    }
}

void printEndOfRunInformation(uint64_t frames, uint64_t packets, time_point begin, int nr_of_triggers, int trig_length_us, int trig_deadtime_us)
{
    printf("\nFrames processed = %lu\n", frames);
    steady_clock::time_point end = steady_clock::now();
    auto t = std::chrono::duration_cast<ns>(end - begin).count();
    printf("Time to process frames = %.6e μs = %f s\n", t / 1000., t /
           1000000000.);
    float time_per_frame      = float(t) / nr_of_triggers / 1000000.;
    float processing_overhead = 1000. *
                                ((float(t / 1000.) / nr_of_triggers) -
                                 trig_length_us -
                                 trig_deadtime_us);
    printf("Time per frame = %.5f ms \nProcessing overhead = %.1f ns\n",
           time_per_frame,
           processing_overhead);
    printf("Packets received = %lu\t\n", packets);
}

void stopTrigger()
{
    if (readoutMode_sequential) {
      spidrcontrol->stopAutoTrigger();
    } else {
      spidrcontrol->stopContReadout();
    }
}

void cleanup(bool print = true)
{
    delete spidrcontrol;
    if (print) {
        printf("\n[END]\n");
    }
}
