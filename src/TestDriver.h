#ifndef TESTDRIVER_H
#define TESTDRIVER_H

#include <chrono> //! Timing - used for sanity checks and debugging
#include <thread> //! For std::thread

#include "SpidrController.h" //!< Slow control to the SPIDR
#include "configs.h" //!< Contains general configuration settings (network and trigger now) which are supplied in Dexter already.
//!< Only useful for this application, not for integration with readout and control software.

#include "spdlog/spdlog.h" //!< For custom logging
#include "spdlog/sinks/stdout_color_sinks.h" //!< To allow a colourful logger

using us = std::chrono::microseconds; //!< Type definition for microseconds
using ns = std::chrono::nanoseconds; //!< Type definition for nanoseconds

/** \class testDriver
  * \authors Navrit, Bram
  * \brief Wrapper for testing and developing this Medipix3 driver
  * \return void
  *
  * This does unbelievably useful things.
  * And returns exceptionally useful results.
  * Use it everyday.
  */
class testDriver {
public:
    void run();

    /** \brief Returns a pointer to the current SpidrController */
    SpidrController *getSpidrController() { return spidrcontrol; }

    /** \brief The pointer to the global spdlog console.
      * This is accessed by spdlog::get("console")->...
      */
    std::shared_ptr<spdlog::logger> console;

private:
    bool initSpidrController(std::string IPAddr, int port);
    bool checkConnectedToDetector();
    bool initDetector();
    void printReadoutMode(bool readoutMode_sequential);
    void updateTimeout_us();
    bool startTrigger();
    void stopTrigger(bool readoutMode_sequential);
    void cleanup();

    /** \brief Effectively the pointer to the master SpidrController */
    SpidrController *spidrcontrol = nullptr;

    /** \brief Trigger configuration */
    Config config;
    /** \brief Network settings */
    NetworkSettings networkSettings;
};

#endif // TESTDRIVER_H
