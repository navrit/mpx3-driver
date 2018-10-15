#include "TestDriver.h"
#include "QVector"
#include "receiveUDPThread.h"

#include <chrono>

void testDriver::run() {
  console = spdlog::stdout_color_mt("console");
  spdlog::get("console")->set_level(spdlog::level::debug);

  if (!initSpidrController(networkSettings.socketIPAddr, networkSettings.TCPPort)) {
      return;
  }
  if (!checkConnectedToDetector()) {
      return;
  }
  /* It is possible that someone didn't stop the trigger from a previous run */
  stopTrigger(config.readoutMode_sequential);
  initDetector();

  /* Instantiate receiveUDPThread. Same perfomance if it's on the stack or the heap */
  receiveUDPThread *receiveUDPthread = new receiveUDPThread();
  updateTimeout_us();
  receiveUDPthread->setPollTimeout(config.timeout_us); /* [microseconds] */

  /* Initialise and run receiveUDPThread if init succeeds */
  if (receiveUDPthread->initThread("", networkSettings.portno) == true) {
    startTrigger();
    receiveUDPthread->run();
  }

  stopTrigger(config.readoutMode_sequential);
  cleanup();
  return;
}

bool testDriver::initSpidrController(std::string IPAddr, int port) {
  int a, b, c, d;
  sscanf(IPAddr.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d);
  spidrcontrol = new SpidrController(a, b, c, d, port);
  if (spidrcontrol != nullptr) {
    return true;
  } else {
    spdlog::get("console")->error("Could not make SpidrController. Start the "
                                  "emulator or connect a SPIDR.\n");
    return false;
  }
}

/**
 * @brief Check if we are connected
 * @return true - connected. false - not connected, could be a problem with the network, network settings, firewall or the SPIDR is not on
 */
bool testDriver::checkConnectedToDetector() {
  if (!spidrcontrol->isConnected()) {
    spdlog::get("console")->error("SpidrController :(\t{}: {}, {}",
                                  spidrcontrol->ipAddressString(),
                                  spidrcontrol->connectionStateString(),
                                  spidrcontrol->connectionErrString());
    return false;
  } else {
    spdlog::get("console")->info("SpidrController :)\t{}\t{}",
                                 spidrcontrol->ipAddressString(),
                                 spidrcontrol->connectionStateString());
    return true;
  }
}

void testDriver::printReadoutMode(bool readoutMode_sequential) {
  if (readoutMode_sequential) {
    spdlog::get("console")->info("Sequential R/W readout mode");
  } else {
    spdlog::get("console")->info("Continuous R/W readout mode");
  }
}

/**
 * @brief testDriver::initDetector Get the detector ready for acquisition
 * @return true - initialisation successful. false - something failed
 *
 * @details Print readout mode.
 *          Stop the trigger and set readout mode.
 *          Initialise SPIDR acquisition - trigger, number of links etc.
 *          Suppress SPIDR debugging messages.
 *          Set default IDELAY values. For some reason the last chip needs a lower IDELAY...
 *          Initialise bias voltage at +100V.
 *          Correctly set the trigger frequency in millihertz.
 *          Submit and print the trigger configuration.
 */
bool testDriver::initDetector() {
  printReadoutMode(config.readoutMode_sequential);

  /* Stop the trigger and set readout mode */
  if (config.readoutMode_sequential) {
    spidrcontrol->stopAutoTrigger();

    for (int i = 0; i < config.number_of_chips; i++) {
      spidrcontrol->setContRdWr(i, false);
    }
  } else {
    spidrcontrol->stopContReadout();

    for (int i = 0; i < config.number_of_chips; i++) {
      spidrcontrol->setContRdWr(i, true);
    }
  }

  /* Initialise SPIDR acquisition - trigger, number of links etc. */
  for (int i = 0; i < config.number_of_chips; i++) {
    spidrcontrol->setPs(i, 3);
    spidrcontrol->setEqThreshH(i, false);
    spidrcontrol->setInternalTestPulse(i, true);
    spidrcontrol->setDiscCsmSpm(
        i, 0); /* In Eq mode using 0: select DiscL, 1: selects DiscH */
    spidrcontrol->setCsmSpm(i, 0);      /* Single pixel mode */
    spidrcontrol->setPolarity(i, true); /* Use Positive polarity */
    spidrcontrol->setGainMode(i, 1);    /* HGM */
    spidrcontrol->setPixelDepth(i, 12, false, false); /* 12 bit, single frame readout */
    spidrcontrol->setColourMode(i, false); /* Fine pitch mode */
  }
  spidrcontrol->resetCounters();
  /* Suppress SPIDR debugging messages */
  spidrcontrol->setLogLevel(0);

  /* Set default IDELAY values. For some reason the last chip needs a lower IDELAY... */
  QVector<uint8_t> _chipIDELAYS = {15, 15, 15, 10};
  spidrcontrol->setSpidrReg(0x10A0, _chipIDELAYS[0], true);
  spidrcontrol->setSpidrReg(0x10A4, _chipIDELAYS[1], true);
  spidrcontrol->setSpidrReg(0x10A8, _chipIDELAYS[2], true);
  spidrcontrol->setSpidrReg(0x10AC, _chipIDELAYS[3], true);

  spdlog::get("console")->info("HDMI unmodified");

  /* Initialise bias voltage at +100V */
  spidrcontrol->setBiasSupplyEna(true);
  spidrcontrol->setBiasVoltage(100);

  /* Correctly set the trigger frequency in millihertz */
  if (config.readoutMode_sequential) {
    config.trig_freq_mhz =
        int(1E3 * (1. / (double((config.trig_length_us + config.trig_deadtime_us)) / 1E6)));
  } else {
    config.trig_freq_mhz = int(1E3 * (1. / (1.0 * (double(config.trig_length_us / 1E6)))));
  }

  /* Submit and print the trigger configuration */
  spidrcontrol->setShutterTriggerConfig(config.trig_mode, config.trig_length_us,
                                        config.trig_freq_mhz, config.nr_of_triggers);
  spdlog::get("console")->info("Trigger data:");
  spdlog::get("console")->info("\ttrig_mode = {}", config.trig_mode);
  spdlog::get("console")->info("\ttrig_length_us = {}", config.trig_length_us);
  spdlog::get("console")->info("\ttrig_freq_mHz = {}", config.trig_freq_mhz);
  spdlog::get("console")->info("\tnr_of_triggers ={}", config.nr_of_triggers);

  return true;
}

void testDriver::updateTimeout_us() {
  if (config.readoutMode_sequential) {
    config.timeout_us = config.trig_length_us + config.trig_deadtime_us; //! [us]
  } else {
    config.timeout_us = int(1E6 / config.continuousRW_frequency);
  }
  spdlog::get("console")->info("Updated timeout to {} ms", config.timeout_us / 1000.);
}

bool testDriver::startTrigger() {
  spdlog::get("console")->info("[START]");

  if (config.readoutMode_sequential) {
    spidrcontrol->startAutoTrigger();
  } else {
    config.trig_freq_mhz = int(config.continuousRW_frequency * 1E3);
    spdlog::get("console")->info(
        "trig_freq_mhz = {}, continuousRW_frequency = {}", config.trig_freq_mhz,
        config.continuousRW_frequency);
    spidrcontrol->startContReadout(config.continuousRW_frequency);
  }

  return true;
}

void testDriver::stopTrigger(bool readoutMode_sequential) {
  if (readoutMode_sequential) {
    spidrcontrol->stopAutoTrigger();
  } else {
    spidrcontrol->stopContReadout();
  }
}

void testDriver::cleanup() {
  delete spidrcontrol;
  spdlog::get("console")->info("[END]");
}

int main() {
  testDriver td;
  td.run();
  return 0;
}
