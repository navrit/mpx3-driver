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
  if (!initSpidrController(socketIPAddr, TCPPort)) {
      return false;
  }
  if (!connectToDetector()) {
      return false;
  }
  initSocket(); //! No arguments --> listens on all IP addresses
                //! Arguments --> IP address as const char *
  initDetector();
  initFileDescriptorsAndBindToPorts();

  // Make X input queues
  // These are the big fat ones that will need to be copied/shared to their
  // respective threads once they are full
  char inputQueues[number_of_chips][max_buffer_size];

  bool finished = false;
  int ret = 1;
  int received_size = 0;
  uint64_t *pixel_packet;
  uint64_t type;
  int sizeofuint64_t = sizeof(uint64_t);

  int rubbish_counter = 0;
  int pixel_depth = 12;
  int pixels_per_word = 60/pixel_depth; //! TODO get or calculate pixel_depth

  int rowPixels = 0;
  uint64_t row_counter = 0;

  int tmp = 0;

  uint64_t pSOR = 0, pEOR = 0, pSOF = 0, pEOF = 0, pMID = 0, iSOF = 0, iMID = 0, iEOF = 0, def = 0;

  time_point begin = steady_clock::now();

  startTrigger();
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

          //std::cout << std::dec << "Index: " << tmp << "\t Chip ID: " << i << "\n";
          ++tmp;

          //! This shouldn't happen but Henk has this check in his code
          //! Maybe it's a good idea to keep it
          if (received_size <= 0) break;

          /* Count the number of packets received.
             Could calculate lost packets like Henk does, is this necessary? */
          ++packets;

          //! Start processing the pixel packet
          pixel_packet = (uint64_t *) inputQueues[i];

          for( int j = 0; j < received_size/sizeofuint64_t; ++j, ++pixel_packet) {

              type = (*pixel_packet) & PKT_TYPE_MASK;

              switch (type) {
              case PIXEL_DATA_SOR:
                  ++pSOR;

                  // Henk checks for lost packets here
                  ++row_counter;
                  // Henk checks for row counter > 256, when would this ever happen?
                  rowPixels = 0;
                  rowPixels += pixels_per_word;
                  break;
              case PIXEL_DATA_EOR:
                  ++pEOR;

                  rowPixels += MPX_PIXEL_COLUMNS - (MPX_PIXEL_COLUMNS/pixels_per_word)*pixels_per_word;

                  break;
              case PIXEL_DATA_SOF:
                  ++pSOF;
                  rowPixels = 0;
                  rowPixels += pixels_per_word;

                  ++row_counter;
                  break;
              case PIXEL_DATA_EOF:
                  ++pEOF;

                  rowPixels += pixels_per_word;
                  //! Henk extracts the FLAGS word here.
                  //! Dexter doesn't use this yet, maybe revisit later

                  break;
              case PIXEL_DATA_MID:
                  ++pMID;

                  rowPixels += pixels_per_word;
                  break;
              case INFO_HEADER_SOF:
                  ++iSOF;
                  break; //! TODO Change this back to continue
              case INFO_HEADER_MID:
                  ++iMID;
                  break; //! TODO Change this back to continue
              case INFO_HEADER_EOF:
                  ++iEOF;
                  break;
              default:
                  // Rubbish packets - skip these
                  // In theory, there should be none ever
                  if (type != 0) {
                      std::cout << "Rubbish packet count: " << rubbish_counter << ": " << type << "\n";
                      ++rubbish_counter;
                  }
                  ++def;
                  break;
              }
          }
        }
      }

      frames = calculateNumberOfFrames(packets, number_of_chips, packets_per_frame);

      if (frames == nr_of_triggers) {
        //! This can never be triggered when it should if the emulator is running and not pinned
        //! to a different physical CPU core that this process.

        printDebuggingOutput(packets, packets_per_frame, number_of_chips, frames, begin);
        printEndOfRunInformation(frames, packets, begin, nr_of_triggers, trig_length_us, trig_deadtime_us);
        doEndOfRunTests(number_of_chips, pMID, pSOR, pEOR, pSOF, pEOF, iMID, iSOF, iEOF, def);

        finished = true; //! Poll loop exit condition
      } else {
        printDebuggingOutput(packets, packets_per_frame, number_of_chips, frames, begin);
      }
    } else if (ret == -1) {
      //! An error occurred, never actually seen this triggered
      return false;
    }
  } while (!finished);

  stopTrigger();
  cleanup();
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

bool initSpidrController(std::string IPAddr, int port)
{
    int a, b, c, d;
    sscanf(IPAddr.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d);
    spidrcontrol = new SpidrController(a, b, c, d, port);
    if (spidrcontrol != nullptr) {
        return true;
    } else {
        std::cout << BOLD(FRED("Could not make SpidrController, start the emulator or connect a SPIDR.\n"));
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
      spidrcontrol->setEqThreshH(i, false);
      spidrcontrol->setInternalTestPulse(i, true);
      spidrcontrol->setDiscCsmSpm(i, 0); //! In Eq mode using 0: select DiscL, 1: selects DiscH
      spidrcontrol->setCsmSpm(i, 0); //! Single pixel mode
      spidrcontrol->setPolarity(i, true); //! Use Positive polarity
      spidrcontrol->setGainMode(i, 1); //! HGM
      spidrcontrol->setPixelDepth(i, 12, false, false ); //! 12 bit, single frame readout
      spidrcontrol->setColourMode(i, false); //! Fine pitch mode
    }
    spidrcontrol->resetCounters();
    spidrcontrol->setLogLevel(0);

    QVector<uint8_t> _chipIDELAYS = {15, 15, 15, 10};
    spidrcontrol->setSpidrReg(0x10A0, _chipIDELAYS[0], true);
    spidrcontrol->setSpidrReg(0x10A4, _chipIDELAYS[1], true);
    spidrcontrol->setSpidrReg(0x10A8, _chipIDELAYS[2], true);
    spidrcontrol->setSpidrReg(0x10AC, _chipIDELAYS[3], true);

    unsigned int val1 = 0x5; // External shutter IN | Connector 1, signal 1
    val1 = val1;
    unsigned int val2 = 0x4; // Debug shutter OUT (read back) | Connector 1, signal 3
    val2 = val2 << 8;
    // mask
    unsigned int val = val1 | val2;
    //qDebug() << "HDMI Setting : " << val;
    spidrcontrol->setSpidrReg(0x0810, val, true);

    spidrcontrol->setBiasSupplyEna( true );
    spidrcontrol->setBiasVoltage( 100 );

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
    if (print) {
        std::cout << "\n[START]\n";
    }
    if (readoutMode_sequential) {
      spidrcontrol->startAutoTrigger();
    } else {
      timeout = float(1/continuousRW_frequency);
      printf("trig_freq_mhz = %d, continuousRW_frequency = %d\n", trig_freq_mhz, continuousRW_frequency);
      spidrcontrol->setSpidrReg(0x800A0298, ((1/continuousRW_frequency)/(7.8125E-9))); //! 3.0.2 Trigger Frequency Register
      spidrcontrol->startContReadout(continuousRW_frequency);
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
      printf("%9lu/%-9lu\t%-.0f%%\t%lus\t%lu packets\n", frames, nr_of_triggers,
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
    float processing_overhead = -1;
    if (readoutMode_sequential) {
        processing_overhead = 1000. * (((t / 1000) / nr_of_triggers) -
                                     trig_length_us -
                                     trig_deadtime_us);
    } else {
        std::cout << "\nt = " << t << " triggers = " << nr_of_triggers << " cont_freq = " << continuousRW_frequency << "\n";
        processing_overhead = (t / 1000 / nr_of_triggers) - 1000000 * (1/continuousRW_frequency);
    }
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

void cleanup(bool print)
{
    delete spidrcontrol;
    if (print) {
        printf("\n[END]\n");
    }
}

void doEndOfRunTests(int number_of_chips, uint64_t pMID, uint64_t pSOR, uint64_t pEOR, uint64_t pSOF, uint64_t pEOF, uint64_t iMID, uint64_t iSOF, uint64_t iEOF, uint64_t def)
{
    printf("\nPer chip packet statistics and tests:\n-------------------------------------");
    printf("\npMID\t\tpSOR\tpEOR\tpSOF\tpEOF\tiMID\tiSOF\tiEOF\tDef.\n");
    printf("%-6lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n\n", pMID/number_of_chips, pSOR/number_of_chips, pEOR/number_of_chips, pSOF/number_of_chips, pEOF/number_of_chips, iMID/number_of_chips, iSOF/number_of_chips, iEOF/number_of_chips, def/number_of_chips);

    if (pSOR != pEOR) {
        printf("[TEST] [FAIL]\tpSOR != pEOR; %lu != %lu\n", pSOR, pEOR);
    } else {
//        printf("[TEST] [PASS]\tpSOR == pEOR\n");
    }
    if (pSOF != pEOF) {
        printf("[TEST] [FAIL]\tpSOF != pEOF; %lu != %lu\n", pSOF, pEOF);
    } else {
//        printf("[TEST] [PASS]\tpSOF == pEOF\n");
    }
    if (iSOF != iEOF) {
        printf("[TEST] [FAIL]\tiSOF != iEOF; %lu != %lu\n", iSOF, iEOF);
    } else {
//        printf("[TEST] [PASS]\tiSOF == iEOF\n");
    }
    if (iMID/iSOF != 6 || iMID/iEOF != 6) {
        printf("[TEST] [FAIL]\tiMID/iSOF != 6 = %lu || iMID/iEOF != 6 = %lu\n", iMID/iSOF, iMID/iEOF);
    } else {
//        printf("[TEST] [PASS]\tiMID/iSOF == %lu && iMID/iEOF == %lu\n", iMID/iSOF, iMID/iEOF);
    }
    if (pSOF/number_of_chips != nr_of_triggers || pEOF/number_of_chips != nr_of_triggers || iSOF/number_of_chips != nr_of_triggers || iEOF/number_of_chips != nr_of_triggers) {
        printf("[TEST] [FAIL]\tnr_of_triggers*number_of_chips != pSOF, pEOF, iSOF, iEOF; %lu != %lu, %lu, %lu or %lu\n", nr_of_triggers, pSOF/number_of_chips, pEOF/number_of_chips, iSOF/number_of_chips, iEOF/number_of_chips);
    } else {
//        printf("[TEST] [PASS]\tnr_of_triggers == pSOF/number_of_chips,\n\t\t\t\tpEOF/number_of_chips,\n\t\t\t\tiSOF/number_of_chips,\n\t\t\t\tiEOF/number_of_chips\n");
    }
    if (def != 0) {
        printf("[TEST] [FAIL]\tdef != 0; def == %lu\n", def);
    } else {
//        printf("[TEST] [PASS]\tdef == 0\n");
    }
}
