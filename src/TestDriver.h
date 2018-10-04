#ifndef TESTDRIVER_H
#define TESTDRIVER_H

#include <chrono>
#include <thread>

#include "SpidrController.h"

// Logging
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

using time_point = std::chrono::steady_clock::time_point;
using steady_clock = std::chrono::steady_clock;

using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;

//! See Table 54 (MPX3 Packet Format) - SPIDR Register Map
//! 64 bit masks because of the uint64_t data type
//! Same order as switch, case statement
const static uint64_t PKT_TYPE_MASK = 0xF000000000000000;

const static uint64_t PIXEL_DATA_SOR = 0xA000000000000000;  //! #2
const static uint64_t PIXEL_DATA_EOR = 0xE000000000000000;  //! #3
const static uint64_t PIXEL_DATA_SOF = 0xB000000000000000;  //! #7
const static uint64_t PIXEL_DATA_EOF = 0x7000000000000000;  //! #5
const static uint64_t PIXEL_DATA_MID = 0x3000000000000000;  //! #1 Most frequent
const static uint64_t INFO_HEADER_SOF = 0x9000000000000000; //! #6
const static uint64_t INFO_HEADER_MID = 0x1000000000000000; //! #4
const static uint64_t INFO_HEADER_EOF = 0xD000000000000000; //! #8
//! End of Table 54
const static uint64_t ROW_COUNT_MASK = 0x0FF0000000000000;
const static uint64_t ROW_COUNT_SHIFT = 52;
const static uint64_t FRAME_FLAGS_MASK = 0x000FFFF000000000;
const static uint64_t FRAME_FLAGS_SHIFT = 36;

const static int MPX_PIXEL_COLUMNS = 256;

//! -------------- Networking ----------------------
// const static std::string socketIPAddr = "127.0.0.1";
const static std::string socketIPAddr = "192.168.100.10";
// const static std::string socketIPAddr = "192.168.1.10";
const static int TCPPort = 50000;
const static int portno = 8192;
//! ------------------------------------------------

//! ------------- Trigger etc. ---------------------
const static int number_of_chips = 4;

const static int trig_mode = 4;          //! Auto-trigger mode
const static int trig_length_us = 250;   //! [us]
const static int trig_deadtime_us = 250; //! [us]

const static uint64_t nr_of_triggers = 10000;
const static int continuousRW_frequency = 2000; //! [Hz]
const static bool readoutMode_sequential = false;
//! ------------------------------------------------

class testDriver {
public:
  void run();

  SpidrController *getSpidrController() { return spidrcontrol; }

  std::shared_ptr<spdlog::logger> console;

  //! ------------- Trigger etc. ---------------------
  int trig_freq_mhz = 0; //! Set this depending on readoutMode_sequential later
                         //! Yes, this really is millihertz
  int timeout;           //! Set this depending on readoutMode_sequential later
  //! ------------------------------------------------

  int set_scheduler();
  int print_affinity();
  int set_cpu_affinity();

private:
  bool initSpidrController(std::string IPAddr, int port);
  bool connectToDetector();
  bool initDetector();
  void printReadoutMode(bool readoutMode_sequential);
  void updateTimeout();
  bool startTrigger();
  void stopTrigger(bool readoutMode_sequential);
  void cleanup();

  SpidrController *spidrcontrol = nullptr;
};

#endif // TESTDRIVER_H
