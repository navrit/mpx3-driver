#include "receiveUDPThread.h"

#include <iomanip> // For pretty column printing --> std::setw()
#include <chrono>
#include <thread>

using time_point = std::chrono::steady_clock::time_point;
using steady_clock = std::chrono::steady_clock;

using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;

int main() {
  print_affinity();
  set_cpu_affinity();
  print_affinity();
  if (set_scheduler() != 0) {
      return 1;
  }
  if (!initSpidrController(socketIPAddr, TCPPort)) {
      return 1;
  }
  if (!connectToDetector()) {
      return 1;
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
  long received_size = 0;
  uint64_t *pixel_packet;
  uint64_t type;
  int sizeofuint64_t = sizeof(uint64_t);

  int rubbish_counter = 0;
  int counter_depth = 12;
  int pixels_per_word = 60/counter_depth; //! TODO get or calculate pixel_depth

  int rowPixels = 0;
  uint64_t row_counter = 0;

  int tmp = 0;
  char byt;

  uint64_t pSOR = 0, pEOR = 0, pSOF = 0, pEOF = 0, pMID = 0, iSOF = 0, iMID = 0, iEOF = 0, def = 0;

  startTrigger();
  std::this_thread::sleep_for(std::chrono::microseconds(timeout));
  const int timeout_ms = int(timeout/1E3);

  time_point begin = steady_clock::now();
  printDebuggingOutput(packets, calculateNumberOfFrames(packets, number_of_chips, packets_per_frame), begin);

  do {
    ret = poll(fds, number_of_chips, timeout_ms);

    // Success
    if (ret > 0) {
      // An event on one of the fds has occurred.
      for (int i = 0; i < number_of_chips; ++i) {
        if (fds[i].revents & POLLIN) {
          /* This consists of 12 (packets_per_frame) packets (MTU = 9000 bytes).
             First 11 are 9000 bytes, the last one is 7560 bytes.
             Assuming no packet loss, extra fragmentation or MTU changing size*/
          received_size = recv(fds[i].fd, inputQueues[i], max_packet_size, 0);

          //std::cout << std::dec << "Index: " << tmp << "\t Chip ID: " << i << "\t Received size: " << received_size << "\n";
          ++tmp;

          //! This shouldn't happen but Henk has this check in his code
          //! Maybe it's a good idea to keep it
          //if (received_size <= 0) break;

          /* Count the number of packets received.
             Could calculate lost packets like Henk does, is this necessary? */
          ++packets;

          //! Start processing the pixel packet
          pixel_packet = (uint64_t *) inputQueues[i];

          //row_number_from_packet = -1;
          int rownr_EOR = -1, rownr_SOR = -1;

          for( int j = 0; j < received_size/sizeofuint64_t; ++j, ++pixel_packet) {
              type = (*pixel_packet) & PKT_TYPE_MASK;

              /*if (i == 0) {
                  hexdumpAndParsePacket(pixel_packet, counter_depth, true, i);
              }*/

              switch (type) {
              case PIXEL_DATA_SOR:
                  ++pSOR;
                  // Henk checks for lost packets here
                  // Henk checks for row counter > 256, when would this ever happen?
                  ++row_counter;

                  rowPixels = 0;
                  rowPixels += pixels_per_word;

                  ++rownr_SOR;

                  break;
              case PIXEL_DATA_EOR:
                  ++pEOR;
                  // Henk checks for lost pixels again
                  // Henk checks for row counter > 256, when would this ever happen?
                  rowPixels += MPX_PIXEL_COLUMNS - (MPX_PIXEL_COLUMNS/pixels_per_word)*pixels_per_word;

                  ++rownr_EOR;

                  /*if (rownr_SOR+1 != rownr_EOR) {
                      std::cout << "Row # SOR: " << rownr_SOR << " " << pSOF << " - Row # EOR: " << rownr_EOR << " " << pEOF << "\n";
                  }*/

                  break;
              case PIXEL_DATA_SOF:
                  ++pSOF;
                  // Henk checks for lost pixels again
                  rowPixels = 0;
                  rowPixels += pixels_per_word;

                  ++row_counter;

                  break;
              case PIXEL_DATA_EOF:
                  ++pEOF;
                  // Henk checks for lost pixels again
                  rowPixels += pixels_per_word;
                  //! Henk extracts the FLAGS word here.
                  //! Dexter doesn't use this yet, maybe revisit later

                  /*row_number_from_packet = int(((*pixel_packet & ROW_COUNT_MASK) >> ROW_COUNT_SHIFT));
                  if (row_number_from_packet != 255) {
                      std::cout << ">> " << row_number_from_packet << "\n";
                  }*/

                  break;
              case PIXEL_DATA_MID:
                  ++pMID;

                  rowPixels += pixels_per_word;
                  if (rowPixels > MPX_PIXEL_COLUMNS) {
                      ++row_counter;
                  }
                  break;
              case INFO_HEADER_SOF:
                  //! This is really iSOF (N*1) + iMID (N*6) + iEOF (N*1) = 8*N
                  ++iSOF;
                  [[fallthrough]];
              case INFO_HEADER_MID:
                  //! This is really iMID (N*6) + iEOF (N*1) = 7*N
                  ++iMID;
                  [[fallthrough]];
              case INFO_HEADER_EOF:
                  ++iEOF;

                  //! TODO Come back to this logic, HOW DOES THIS EVER GET TRIGGERED?!
                  if (type == INFO_HEADER_SOF) {
                      infoIndex = 0;
                  }
                  if (infoIndex <= MPX_PIXEL_COLUMNS / 8 - 4) {
                      for (int i = 0; i < 4; ++i, ++infoIndex) {
                          infoHeader[infoIndex] = char((*pixel_packet >> (i*8)) & 0xFF); // Same as infoHeader[infoIndex] = p[i] where p = (char *) pixel_packet;
                      }
                  }
                  if (type == INFO_HEADER_EOF) {
                      // Format and interpret:
                      // e.g. OMR has to be mirrored per byte;
                      // in order to distinguish CounterL from CounterH readout
                      // it's sufficient to look at the last byte, containing
                      // the three operation mode bits
                      byt = infoHeader[infoIndex - 1];
                      // Mirroring of byte with bits 'abcdefgh':
                      byt = ((byt & 0xF0) >> 4) | ((byt & 0x0F) << 4); // efghabcd
                      byt = ((byt & 0xCC) >> 2) | ((byt & 0x33) << 2); // ghefcdab
                      byt = ((byt & 0xAA) >> 1) | ((byt & 0x55) << 1); // hgfedcba
                      if ((byt & 0x7) == 0x4) {                        // 'Read CounterH'
                          isCounterhFrame = true;
                      } else {
                          isCounterhFrame = false;
                      }
                  }
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
        //! to a different physical CPU core than this process.

        printDebuggingOutput(packets, frames, begin);
        printEndOfRunInformation(frames, packets, begin, nr_of_triggers, trig_length_us, trig_deadtime_us);
        doEndOfRunTests(number_of_chips, pMID, pSOR, pEOR, pSOF, pEOF, iMID, iSOF, iEOF, def);

        finished = true; //! Poll loop exit condition
      } else {
        printDebuggingOutput(packets, frames, begin);
      }
    } else if (ret == -1) {
      //! An error occurred, never actually seen this triggered
      return 1;
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
        console->error("Sched_setscheduler, ret = {}", ret);
        return 1;
    }

    policy = sched_getscheduler(0);

    switch (policy) {
    case SCHED_OTHER:
        console->info("Policy is normal");
        break;

    case SCHED_RR:
        console->info("Policy is round-robin");
        break;

    case SCHED_FIFO:
        console->info("Policy is first-in, first-out");
        break;

    case -1:
        console->error("Sched_getscheduler");
        break;

    default:
        console->error("Unknown policy!");
    }

    ret = sched_getparam(0, &sp);

    if (ret == -1) {
        console->error("Sched_getparam, ret = {}", ret);
        return 1;
    }

    console->info("Our priority is {}", sp.sched_priority);

    ret = sched_rr_get_interval(0, &tp);

    if (ret == -1) {
        console->error("Sched_rr_get_interval, ret = {}", ret);
        return 1;
    }

    console->info("Our time quantum is {} milliseconds", (tp.tv_sec * 1000.0f) + (tp.tv_nsec / 1000000.0f));

    return 0;
}

int print_affinity() {
  cpu_set_t mask;
  long nproc, i;

  int ret = sched_getaffinity(0, sizeof(cpu_set_t), &mask);

  if (ret == -1) {
    console->error("Sched_getaffinity, ret = {}", ret);
    return 1;
  } else {
    nproc = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.

    for (i = 0; i < nproc; i++) {
      console->info("Sched_getaffinity --> Core {} = {}", i, CPU_ISSET(i, &mask));
    }
  }
  return 0;
}

// ! http://man7.org/linux/man-pages/man3/CPU_SET.3.html
int set_cpu_affinity()
{
    cpu_set_t set;                             /* Define your cpu_set
                                                bit mask. */
    int ret;
    long nproc = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.

    CPU_ZERO(&set);                                      /* Clears set, so that it
                                                          contains no CPUs */
    CPU_SET(0, &set);                                    /* Add CPU cpu to set */
    //CPU_CLR(1, &set);                               /* Remove CPU cpu from set */
    ret = sched_setaffinity(0, sizeof(cpu_set_t), &set); /* Set affinity of this
                                                          process to the defined
                                                          mask, i.e. only N */

    if (ret == -1) {
        console->error("Sched_setaffinity");
        return -1;
    }

    for (long i = 0; i < nproc; i++) {
        int cpu;

        cpu = CPU_ISSET(i, &set);
        console->info("cpu {} is {}", i, (cpu ? "SET" : "unset"));
    }

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
        console->error("Could not make SpidrController, start the emulator or connect a SPIDR.\n");
        return false;
    }
}

bool connectToDetector()
{
    // Are we connected ?
    if (!spidrcontrol->isConnected()) {
        console->error("SpidrController :(\t{}: {}, {}",
                       spidrcontrol->ipAddressString(),
                       spidrcontrol->connectionStateString(),
                       spidrcontrol->connectionErrString());
                return false;
    } else {
        console->info("SpidrController :)\t{}\t{}",
                      spidrcontrol->ipAddressString(),
                      spidrcontrol->connectionStateString());
        return true;
    }
}

unsigned int inet_addr(char *str)
{
    int a, b, c, d;
    char arr[4];
    sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d);
    arr[0] = char(a); arr[1] = char(b); arr[2] = char(c); arr[3] = char(d);
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
        console->info("Listening on {}", inetIPAddr);
    } else {
        listen_address.sin_addr.s_addr = htonl(INADDR_ANY); //! Address to accept any incoming messages
        console->info("Listening on ANY ADDRESS");
        //! Eg. inet_addr("127.0.0.1");
    }
    std::memset(fds, 0, sizeof(fds));
    return true;
}

void printReadoutMode()
{
    if (readoutMode_sequential) {
      console->info("Sequential R/W readout mode");
    } else {
      console->info("Continuous R/W readout mode");
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

    if (readoutMode_sequential) {
        trig_freq_mhz = int( 1E3 * ( 1. / (double(( trig_length_us + trig_deadtime_us ))/1E6)));
    } else {
        trig_freq_mhz = int( 1E3 * ( 1. / (1.0*(double(trig_length_us/1E6))) ));
    }
    spidrcontrol->setShutterTriggerConfig(trig_mode,
                                          trig_length_us,
                                          trig_freq_mhz,
                                          nr_of_triggers);
    console->info("Trigger data:");
    console->info("\ttrig_mode = {}", trig_mode);
    console->info("\ttrig_length_us = {}", trig_length_us);
    console->info("\ttrig_freq_mhz = {}", trig_freq_mhz);
    console->info("\tnr_of_triggers ={}", nr_of_triggers);

    return true;
}

bool initFileDescriptorsAndBindToPorts()
{
    for (int i = 0; i < number_of_chips; i++) {
      fds[i].fd     = socket(AF_INET, SOCK_DGRAM, 0);
      fds[i].events = POLLIN;

      if (fds[i].fd < 0) {
        console->error("Could not create socket");
        return false;
      }

      listen_address.sin_port = htons(portno + i);

      if (bind(fds[i].fd,
               (struct sockaddr *)&listen_address,
               (socklen_t)sizeof(listen_address)) < 0) {
        console->error("Could not bind, port =", portno + i);
        return false;
      }
      console->info("Socket created: {}\tBound port: {}", i, portno + i);
    }
    return true;
}

bool startTrigger()
{
    console->info("\t[START]");

    if (readoutMode_sequential) {
      timeout = trig_length_us + trig_deadtime_us; //! [us]
      spidrcontrol->startAutoTrigger();
    } else {
      timeout = int(1E6/continuousRW_frequency);
      trig_freq_mhz = int(continuousRW_frequency*1E3);
      console->info("trig_freq_mhz = {}, continuousRW_frequency = {}", trig_freq_mhz, continuousRW_frequency);
      //spidrcontrol->setSpidrReg(0x800A0298, ((1/continuousRW_frequency)/(7.8125E-9))); //! 3.0.2 Trigger Frequency Register
      //! This may have crashed my SPIDR...
      spidrcontrol->startContReadout(continuousRW_frequency);
    }

    return true;
}

uint64_t calculateNumberOfFrames(uint64_t packets, int number_of_chips, int packets_per_frame)
{
    return uint64_t(packets / uint64_t(number_of_chips) / uint64_t(packets_per_frame));
}

void printDebuggingOutput(uint64_t packets, uint64_t frames, time_point begin)
{
    // Some debugging output during a run
    // Print every 2%

    uint number_of_prints = 50;

    if ((frames % (nr_of_triggers / number_of_prints) == 0) && (frames != last_frame_number)) {
      last_frame_number = frames;
      time_point end = steady_clock::now();
      auto t = std::chrono::duration_cast<us>(end - begin).count();

      console->info("{:>10}/{:<10} {:>4}%  {:>.1f}s {:>10} pkts", frames, nr_of_triggers, float(frames * 100. / nr_of_triggers), t / 1E6, packets);
    }
    return;
}

void printEndOfRunInformation(uint64_t frames, uint64_t packets, time_point begin, int nr_of_triggers, int trig_length_us, int trig_deadtime_us)
{
    console->info("Frames processed = {}", frames);
    steady_clock::time_point end = steady_clock::now();
    auto t = std::chrono::duration_cast<ns>(end - begin).count();
    console->info("Time to process frames = {} μs = {} s", t / 1E3, t / 1E9);
    float time_per_frame      = float(t) / nr_of_triggers / 1E6;
    float processing_overhead = -1;
    if (readoutMode_sequential) {
        processing_overhead = 1E3 * ((float(t / 1E3) / nr_of_triggers) -
                                     trig_length_us -
                                     trig_deadtime_us);
    } else {
        //std::cout << "\nt = " << t << " triggers = " << nr_of_triggers << " cont_freq = " << continuousRW_frequency << " _ " << (double(t) / 1E3 / double(nr_of_triggers)) << " _ " << (1E6 * (1/double(continuousRW_frequency))) << "\n";
        processing_overhead = 1E3 * ((double(t) / 1E3 / double(nr_of_triggers)) - (1E6 * (1/double(continuousRW_frequency))));
    }
    console->info("Time per frame = {} ms", time_per_frame );
    console->info("Processing overhead = {} ns", processing_overhead);
    console->info("Packets received = {}", packets);
}

void stopTrigger()
{
    if (readoutMode_sequential) {
      spidrcontrol->stopAutoTrigger();
    } else {
      spidrcontrol->stopContReadout();
    }
}

void cleanup()
{
    delete spidrcontrol;
    console->info("[END]");
}

void doEndOfRunTests(int number_of_chips, uint64_t pMID, uint64_t pSOR, uint64_t pEOR, uint64_t pSOF, uint64_t pEOF, uint64_t iMID, uint64_t iSOF, uint64_t iEOF, uint64_t def)
{
    console->info("Per chip packet statistics and tests:");
    console->info("----------------------------------------------------------------");
    console->info("{:<12} {:<12} {:<12} {:<12} {:<12}", "pMID", "pSOR", "pEOR", "pSOF", "pEOF");
    console->info("{:<12} {:<12} {:<12} {:<12} {:<12}", pMID/number_of_chips, pSOR/number_of_chips, pEOR/number_of_chips, pSOF/number_of_chips, pEOF/number_of_chips);

    console->info("{:<12} {:<12} {:<12} {:<12}", "iSOF", "iMID", "iEOF", "Def.");
    console->info("{:<12} {:<12} {:<12} {:<12}", iSOF/number_of_chips, iMID/number_of_chips, iEOF/number_of_chips, def/number_of_chips);

    if (pSOR != pEOR) {
        console->warn("[TEST] [FAIL]\tpSOR != pEOR; {} != {}", pSOR, pEOR);
    } else {
        console->info("[TEST] [PASS]\tpSOR == pEOR");
    }
    if (pSOF != pEOF) {
        console->warn("[TEST] [FAIL]\tpSOF != pEOF; {} != {}", pSOF, pEOF);
    } else {
        console->info("[TEST] [PASS]\tpSOF == pEOF");
    }
    if (iEOF - iMID - iSOF != 0) {
        console->warn("[TEST] [FAIL]\tiEOF - iMID - iSOF != 0; {} - {} - {} == {}", iEOF, iMID, iSOF, (iEOF - iMID - iSOF));
    } else {
        console->info("[TEST] [PASS]\tiEOF - iMID - iSOF == 0");
    }
    if (iMID%7 != 0) {
        printf("[TEST] [FAIL]\t iMID%%7 != 0; {} %% 7 != 0", iMID);
    } else {
        console->info("[TEST] [PASS] \t iMID%7 == 0");
    }
    if (pSOF/number_of_chips != nr_of_triggers || pEOF/number_of_chips != nr_of_triggers || iSOF/number_of_chips != nr_of_triggers || iEOF/number_of_chips/8 != nr_of_triggers) {
        console->warn("[TEST] [FAIL]\tnr_of_triggers*number_of_chips != pSOF, pEOF, iSOF, iEOF; {} != {}, {}, {} or {}", nr_of_triggers, pSOF/number_of_chips, pEOF/number_of_chips, iSOF/number_of_chips, iEOF/number_of_chips/8);
    } else {
//        console->info("[TEST] [PASS]\tnr_of_triggers == pSOF/number_of_chips,\n\t\t\t\tpEOF/number_of_chips,\n\t\t\t\tiSOF/number_of_chips,\n\t\t\t\tiEOF/number_of_chips");
    }
    if (def != 0) {
        console->warn("[TEST] [FAIL]\tdef != 0; def == {}", def);
    } else {
//        console->info("[TEST] [PASS]\tdef == 0");
    }
}

void hexdumpAndParsePacket(uint64_t* pixel_packet, int counter_bits, bool skip_data_packets, int chip)
{
    //! ONLY DEBUGGING USE
    //! This is slow and affects the number of packets received somehow...

    int pix_per_word = 60/counter_bits; // 60 bytes = sizeof(uint64_t) - sizeof(int)
    uint64_t pixelword = *pixel_packet;
    std::stringstream ss;
    int row = 0;

    uint64_t type = pixelword & PKT_TYPE_MASK;

    switch( type ) {
    case PIXEL_DATA_SOR:
        ss << chip << " pSOR " << std::hex << pixelword << "\t";

        for (int i = 0; i < pix_per_word; ++i) {
            ss << std::dec << std::setw(5) << ((pixelword >> (i*counter_bits)) & 0xFFF) << " ";
        }
        break;
    case PIXEL_DATA_EOR:
        row = int((pixelword & ROW_COUNT_MASK) >> ROW_COUNT_SHIFT);
        ss << chip << " pEOR Row = " << row << " " << "\n";
        ss << chip << " pEOR " << std::hex << pixelword << "\t";
        for (int i = 0; i < pix_per_word; ++i) {
            ss << std::dec << std::setw(5) << ((pixelword << (i*counter_bits)) & 0xFFF) << " ";
        }

        break;
    case PIXEL_DATA_SOF:
        ss << chip << " pSOF " << std::hex << pixelword << "\t";
        for (int i = 0; i < pix_per_word; ++i) {
            ss << std::dec << std::setw(5) << ((pixelword >> (i*counter_bits)) & 0xFFF) << " ";
        }

        break;
    case PIXEL_DATA_EOF: {
        //! Extract the row number, flags (something then frame ID), last pixel,
        ss << chip << " pEOF Row = " << row << " " << std::hex << pixelword << "\t";

        ss << std::setw(5) << std::dec << (pixelword & 0xFFF) << " ";

        uint64_t flags = (((pixelword) & FRAME_FLAGS_MASK) >> FRAME_FLAGS_SHIFT);
        ss << std::setw(5) << std::hex << flags << " ";
        //! TODO Implement this if necessary
        //! NOTES on FLAGS
        /* variable FLAGS: std_logic_vector(15 downto 0); --flags are shifted in the last packet (EOF) of every frame, to show some status bits / frame counter.
         --Flag a warning when the fifo is full, this means that the MPX3 clock will be stopped.
         if(fe_fifo_full_flag = '0') then
           FLAGS(8) := '1';
         end if;
         -Flag a warning when Continuousread write mode is 0, and the shutter is open while readout is still busy
        if(mpx_readout_state /= IDLE and (Shutter='1' and OMRClkOut(3) = '0')) then
            FLAGS(9) := '1';
        end if;
        case (mpx_readout_state) is
          when IDLE => --wait for a rising edge on RxFrame
              FLAGS := x"00"&framecnt;
        */
        row = (((pixelword) & ROW_COUNT_MASK) >> ROW_COUNT_SHIFT);
        ss << std::setw(5) << std::dec << row;

        break;
    }
    case PIXEL_DATA_MID:
        if (skip_data_packets) {
            return;
        } else {
            ss << chip << " pMID " << std::hex << pixelword << "\t";
            for (int i = 1; i < pix_per_word; ++i) {
                ss << std::setw(5) << std::dec << ((pixelword >> (i*counter_bits)) & 0xFFF) << " ";
            }
        }
        break;
    case INFO_HEADER_SOF:
        //! This is iSOF (N*1) = N
        ss << chip << " iSOF " << std::hex << pixelword << "\t";
        break;
        //[[fallthrough]];
    case INFO_HEADER_MID: {
        //! This is iMID (N*6) = 6*N
        int low = (pixelword & 0xFFFFFFFF);
        ss << chip << " iMID " << std::hex << pixelword << "\t" << low;
        break;
        //[[fallthrough]];
    }
    case INFO_HEADER_EOF: {
        //! This is iEOF (N*1) = N
        //!
        //! OMR Reconstruction notes:
        //! ------------------------------------
        //! There's one OMR per frame
        //! With some bits in iMID and iEOF
        //! The bits are all reversed
        //! We have the mode, crw/srw, countL, colormode, csm/spm there
        //!
        //! Henk makes the assumption that the SPIDR is in the state that he just set. This isn't unreasonable but it may not always be the case. Actually knowing what state the SPIDR is in will be useful for debugging at least.
        //!
        //! TODO Read OMR from the packets

        ss << chip << " iEOF " << std::hex << pixelword << "\n";

        if( type == INFO_HEADER_SOF )
            infoIndex = 0;
        if( infoIndex <= 256/8-4 )
            for( int i=0; i<4; ++i, ++infoIndex )
                infoHeader[infoIndex] = char((*pixel_packet >> (i*8)) & 0xFF); // Same as infoHeader[infoIndex] = p[i] where p = (char *) pixel_packet;
        if( type == INFO_HEADER_EOF ) {
            // Format and interpret:
            // e.g. OMR has to be mirrored per byte;
            // in order to distinguish CounterL from CounterH readout
            // it's sufficient to look at the last byte, containing
            // the three operation mode bits
            char byt = infoHeader[infoIndex-1];
            // Mirroring of byte with bits 'abcdefgh':
            byt = ((byt & 0xF0)>>4) | ((byt & 0x0F)<<4);// efghabcd
            byt = ((byt & 0xCC)>>2) | ((byt & 0x33)<<2);// ghefcdab
            byt = ((byt & 0xAA)>>1) | ((byt & 0x55)<<1);// hgfedcba
            if( (byt & 0x7) == 0x4 ) // 'Read CounterH'
                isCounterhFrame = true;
            else
                isCounterhFrame = false;
            ss << chip << " iEOF isCounterHFrame = " << isCounterhFrame;
        }

        break;
    }
    default:
        ss << std::dec << chip << std::hex << pixelword << " ";
        break;
    }

    ss << ";";
    std::string mystr = ss.str();
    std::cout << mystr << "\n";
}
