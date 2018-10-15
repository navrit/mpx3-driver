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

/** @class testDriver
  * @authors Navrit, Bram
  * @brief Wrapper for testing and developing this Medipix3 driver
  * @return void
  *
  * This does unbelievably useful things.
  * And returns exceptionally useful results.
  * Use it everyday.
  */
class testDriver {
public:
    /**
     * @brief The entry point to the class. Basically main()
     * @return void
     */
    void run();

    /** @brief Returns a pointer to the current SpidrController */
    SpidrController *getSpidrController() { return spidrcontrol; }

    /** @brief The pointer to the global spdlog console.
      * This is accessed by spdlog::get("console")->...
      */
    std::shared_ptr<spdlog::logger> console;

private:
    /**
     * @brief Initialise the SpidrController and connect
     * @param IPAddr - The IPv4 address as a string, usually 192.168.100.10 or 192.168.1.10
     * @param port - The TCP port to bind to for slow control, default: 50000
     * @return true - Initialised and connected
     * @return false - Could not initialise or connect
     */
    bool initSpidrController(std::string IPAddr, int port);
    /**
     * @brief Check if we are connected
     * @return true - connected
     * @return false - not connected, could be a problem with the network, network settings, firewall or the SPIDR is not on
     */
    bool checkConnectedToDetector();
    /**
     * @brief Say which readout mode this is running in
     * @param readoutMode_sequential - Sequential (SRW) or continuous (CRW) readout mode
     * @return void
     */
    void printReadoutMode(bool readoutMode_sequential);
    /**
     * @brief testDriver::initDetector Get the detector ready for acquisition
     * @return true - initialisation successful. false - something failed
     *
     * @details Print readout mode.
     * @details Stop the trigger and set readout mode.
     * @details Initialise SPIDR acquisition - trigger, number of links etc.
     * @details Suppress SPIDR debugging messages.
     * @details Set default IDELAY values. For some reason the last chip needs a lower IDELAY...
     * @details Initialise bias voltage at +100V.
     * @details Correctly set the trigger frequency in millihertz.
     * @details Submit and print the trigger configuration.
     */
    bool initDetector();
    /**
     * @brief Update the timeout in microseconds for receiveUDPThread, used for the epoll timeout
     * @return void
     */
    void updateTimeout_us();
    /**
     * @brief Prints [START] and the trigger frequency in millihertz, updates the trigger frequency and starts the trigger
     * @return true - trigger has started
     */
    bool startTrigger();
    /**
     * @brief Stops the trigger depending on which readout mode we are in
     * @param readoutMode_sequential - Is it sequential (SRW) or continuous (CRW) readout mode?
     * @return void
     */
    void stopTrigger(bool readoutMode_sequential);
    /**
     * @brief Delete spidrcontrol pointer and print [END]
     * @return void
     */
    void cleanup();

    /** @brief Effectively the pointer to the master SpidrController */
    SpidrController *spidrcontrol = nullptr;

    /** @brief Trigger configuration */
    Config config;
    /** @brief Network settings */
    NetworkSettings networkSettings;
};

#endif // TESTDRIVER_H
