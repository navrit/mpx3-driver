#include "UdpReceiver.h"
#include "SpidrController.h"

/**
     * @brief Initialise the SpidrController and connect
     * @param IPAddr - The IPv4 address as a string, usually 192.168.100.10 or 192.168.1.10
     * @param port - The TCP port to bind to for slow control, default: 50000
     * @return true - Initialised and connected
     * @return false - Could not initialise or connect
     */
SpidrController* initSpidrController(SpidrController *spidrcontrol, std::string IPAddr, int port) {
  int a, b, c, d;
  sscanf(IPAddr.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d);
  spidrcontrol = new SpidrController(a, b, c, d, port);
  if (spidrcontrol != nullptr) {
    spdlog::get("console")->info("Made a SPIDR Controller");
    return spidrcontrol;
  } else {
    spdlog::get("console")->error("Could not make SpidrController. Start the "
                                  "emulator or connect a SPIDR");
    return nullptr;
  }
}

/**
     * @brief Check if we are connected
     * @return true - connected
     * @return false - not connected, could be a problem with the network, network settings, firewall or the SPIDR is not on
     */
bool checkConnectedToDetector(SpidrController *spidrcontrol) {
  if (spidrcontrol->isConnected()) {
    spdlog::get("console")->info("SpidrController :)\t{}\t{}",
                                   spidrcontrol->ipAddressString(),
                                   spidrcontrol->connectionStateString());
    return true;

  } else {
    spdlog::get("console")->error("SpidrController :(\t{}: {}, {}",
                                    spidrcontrol->ipAddressString(),
                                    spidrcontrol->connectionStateString(),
                                    spidrcontrol->connectionErrString());
    return false;
  }
}

/**
     * @brief Say which readout mode this is running in
     * @param readoutMode_sequential - Sequential (SRW) or continuous (CRW) readout mode
     * @return void
     */
void printReadoutMode(bool readoutMode_sequential) {
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
     * @details Stop the trigger and set readout mode.
     * @details Initialise SPIDR acquisition - trigger, number of links etc.
     * @details Suppress SPIDR debugging messages.
     * @details Set default IDELAY values. For some reason the last chip needs a lower IDELAY...
     * @details Initialise bias voltage at +100V.
     * @details Correctly set the trigger frequency in millihertz.
     * @details Submit and print the trigger configuration.
     */
bool initDetector(SpidrController *spidrcontrol, Config config) {
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
  std::vector<int> _chipIDELAYS = {15, 15, 15, 10};
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

/**
     * @brief Update the timeout in microseconds for receiveUDPThread, used for the epoll timeout
     * @return void
     */
void updateTimeout_us(Config config) {
  if (config.readoutMode_sequential) {
    config.timeout_us = config.trig_length_us + config.trig_deadtime_us; //! [us]
  } else {
    config.timeout_us = int(1E6 / config.continuousRW_frequency);
  }
  spdlog::get("console")->info("Updated timeout to {} ms", config.timeout_us / 1000.);
}

/**
     * @brief Prints [START] and the trigger frequency in millihertz, updates the trigger frequency and starts the trigger
     * @return true - trigger has started
     */
bool startTrigger(SpidrController *spidrcontrol, Config config) {
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

/**
     * @brief Stops the trigger depending on which readout mode we are in
     * @param readoutMode_sequential - Is it sequential (SRW) or continuous (CRW) readout mode?
     * @return void
     */
void stopTrigger(SpidrController *spidrcontrol, bool readoutMode_sequential) {
  if (readoutMode_sequential) {
    spidrcontrol->stopAutoTrigger();
  } else {
    spidrcontrol->stopContReadout();
  }
}

/**
 * @brief main, initialise the testDriver class and run
 * @return 0
 */
int main() {
  std::shared_ptr<spdlog::logger> console;
  SpidrController *spidrcontrol = nullptr;
  NetworkSettings networkSettings;
  Config config;

  console = spdlog::stdout_color_mt("console");
  spdlog::get("console")->set_level(spdlog::level::debug);
  spdlog::get("console")->info("[INIT]");

  /* Exit now if you cannot connect to the SPIDR*/
  spidrcontrol = initSpidrController(spidrcontrol, networkSettings.socketIPAddr, networkSettings.TCPPort);
  if (spidrcontrol == nullptr) {
      spdlog::get("console")->error("Could not connect to SPIDR");
      return -2;
  }

  /* Exit now if you are not connected to the SPIDR*/
  if (!checkConnectedToDetector(spidrcontrol)) {
      spdlog::get("console")->error("Not connected to SPIDR");
      return -3;
  }

  /* It is possible that someone didn't stop the trigger from a previous run */
  stopTrigger(spidrcontrol, config.readoutMode_sequential);
  initDetector(spidrcontrol, config);

  int fwVersion;
  spidrcontrol->getFirmwVersion(&fwVersion);
  UdpReceiver *udpReceiver = new UdpReceiver(fwVersion < 0x18100100);
  FrameSetManager *frameSetManager = nullptr;
  std::thread th;
  updateTimeout_us(config);
  udpReceiver->setPollTimeout(config.timeout_us); /* [microseconds] */

  if (udpReceiver->initThread("", networkSettings.portno)) {
      th = udpReceiver->spawn();
      frameSetManager = udpReceiver->getFrameSetManager();
      startTrigger(spidrcontrol, config);
      th.join();
      stopTrigger(spidrcontrol, config.readoutMode_sequential);
      delete spidrcontrol;
      spdlog::get("console")->info("[END]");
      return 0;
  } else {
      return -1;
  }
}
