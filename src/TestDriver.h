#ifndef TESTDRIVER_H
#define TESTDRIVER_H

#include <chrono>
#include <thread>

#include "SpidrController.h"

// Logging
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;

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
                         //! Yes, this really is [millihertz]
  int timeout_us;        //! [microseconds]
                         //! Set this depending on readoutMode_sequential later
  //! ------------------------------------------------

  int set_scheduler();
  int print_affinity();
  int set_cpu_affinity();

private:
  bool initSpidrController(std::string IPAddr, int port);
  bool connectToDetector();
  bool initDetector();
  void printReadoutMode(bool readoutMode_sequential);
  void updateTimeout_us();
  bool startTrigger();
  void stopTrigger(bool readoutMode_sequential);
  void cleanup();

  SpidrController *spidrcontrol = nullptr;
};

#endif // TESTDRIVER_H
