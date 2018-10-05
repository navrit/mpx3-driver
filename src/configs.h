#ifndef CONFIGS_H
#define CONFIGS_H

#include <stdint.h>
#include <string>

struct Config{
    const static int number_of_chips = 4;

    const int trig_mode = 4;          //! Auto-trigger mode
    const int trig_length_us = 250;   //! [us]
    const int trig_deadtime_us = 250; //! [us]

    const uint64_t nr_of_triggers = 10000;
    const int continuousRW_frequency = 2000; //! [Hz]
    const bool readoutMode_sequential = false;

    int trig_freq_mhz = 0; //! Set this depending on readoutMode_sequential later
                           //! Yes, this really is [millihertz]
    int timeout_us;        //! [microseconds]
                           //! Set this depending on readoutMode_sequential later

};

struct NetworkSettings{
    // const std::string socketIPAddr = "127.0.0.1";
    const std::string socketIPAddr = "192.168.100.10";
    // const std::string socketIPAddr = "192.168.1.10";
    const int TCPPort = 50000;
    const int portno = 8192;
};

#endif // CONFIGS_H
