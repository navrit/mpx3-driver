#ifndef TESTDRIVER_H
#define TESTDRIVER_H

#include <chrono>
#include <thread>

#include "SpidrController.h"
#include "configs.h"

// Logging
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;

class testDriver {
public:
  void run();

  SpidrController *getSpidrController() { return spidrcontrol; }

  std::shared_ptr<spdlog::logger> console;

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
    bool checkConnectedToDetector();

  SpidrController *spidrcontrol = nullptr;

  Config config;
  NetworkSettings networkSettings;
};

#endif // TESTDRIVER_H
