#ifndef SPIDRMPX3CMDS_H
#define SPIDRMPX3CMDS_H

// Command identifiers in messages to (and from) the SPIDR-MPX3 module
#define CMD_NOP                0x000

// General: module
#define CMD_GET_SOFTWVERSION   0x901
#define CMD_GET_FIRMWVERSION   0x902
#define CMD_GET_DEVICETYPE     0x903
#define CMD_SET_DEVICETYPE     0x904
#define CMD_GET_UDPPACKET_SZ   0x905
#define CMD_SET_UDPPACKET_SZ   0x906
#define CMD_RESET_MODULE       0x907
#define CMD_SET_BUSY           0x908
#define CMD_CLEAR_BUSY         0x909
#define CMD_SET_LOGLEVEL       0x90A
#define CMD_DISPLAY_INFO       0x90B
#define CMD_SET_TIMEOFDAY      0x90C
#define CMD_GET_DEVICECOUNT    0x90D
#define CMD_GET_BOARDID        0x90E
#define CMD_GET_CHIPBOARDID    0x90F

// Configuration: devices
#define CMD_GET_DEVICEID       0x110
#define CMD_GET_DEVICEIDS      0x111
#define CMD_GET_IPADDR_SRC     0x112
#define CMD_SET_IPADDR_SRC     0x113
#define CMD_GET_IPADDR_DEST    0x114
#define CMD_SET_IPADDR_DEST    0x115
//#define CMD_GET_DEVICEPORTS  0x115
//#define CMD_SET_DEVICEPORT   0x116
#define CMD_GET_DEVICEPORT     0x116
#define CMD_GET_SERVERPORT     0x117
//#define CMD_GET_SERVERPORTS  0x118
#define CMD_SET_SERVERPORT     0x119
#define CMD_GET_DAC            0x11A
#define CMD_SET_DAC            0x11B
#define CMD_SET_DACS           0x11C
//#define CMD_READ_DACS        0x11D
//#define CMD_WRITE_DACS       0x11E
#define CMD_SET_DACS_DFLT      0x11F
#define CMD_CONFIG_CTPR        0x120
#define CMD_SET_CTPR           0x121
#define CMD_GET_ACQENABLE      0x122
#define CMD_SET_ACQENABLE      0x123
#define CMD_RESET_DEVICE       0x124
#define CMD_RESET_DEVICES      0x125
#define CMD_SET_READY          0x126

// Configuration: pixels
// Medipix3.1
#define CMD_PIXCONF_MPX3_0     0x22A
#define CMD_PIXCONF_MPX3_1     0x22B
// Medipix3RX
#define CMD_PIXCONF_MPX3RX     0x22C

// Configuration: OMR
#define CMD_SET_CRW            0x330
#define CMD_SET_POLARITY       0x331
#define CMD_SET_DISCCSMSPM     0x332
#define CMD_SET_INTERNALTP     0x333
#define CMD_SET_COUNTERDEPTH   0x334
#define CMD_SET_EQTHRESHH      0x335
#define CMD_SET_COLOURMODE     0x336
#define CMD_SET_CSMSPM         0x337
#define CMD_SET_SENSEDAC       0x338
#define CMD_SET_PS             0x339

#define CMD_SET_EXTDAC         0x33A
#define CMD_WRITE_OMR          0x33B
#define CMD_SET_GAINMODE       0x33C
#define CMD_GET_OMR            0x33D

// Trigger
#define CMD_GET_TRIGCONFIG     0x440
#define CMD_SET_TRIGCONFIG     0x441
#define CMD_AUTOTRIG_START     0x442
#define CMD_AUTOTRIG_STOP      0x443
#define CMD_TRIGGER_READOUT    0x444

// Monitoring
//#define CMD_GET_ADC          0x548
#define CMD_GET_REMOTETEMP     0x549
#define CMD_GET_LOCALTEMP      0x54A
#define CMD_GET_AVDD           0x54B
#define CMD_GET_DVDD           0x54C
#define CMD_GET_AVDD_NOW       0x54D
#define CMD_GET_SPIDR_ADC      0x54E
#define CMD_GET_DVDD_NOW       0x54F

// Trigger (continued)
#define CMD_RESET_COUNTERS     0x558

// Configuration: devices (continued)
#define CMD_BIAS_SUPPLY_ENA    0x55F
#define CMD_SET_BIAS_ADJUST    0x560
#define CMD_DECODERS_ENA       0x561

// Monitoring (continued)
#define CMD_GET_FPGATEMP       0x568
#define CMD_GET_FANSPEED       0x569
#define CMD_SET_FANSPEED       0x56A
#define CMD_GET_VDD            0x56C
#define CMD_GET_VDD_NOW        0x56D
#define CMD_GET_HUMIDITY       0x56E
#define CMD_GET_PRESSURE       0x56F

// Configuration: non-volatile onboard storage
#define CMD_STORE_ADDRPORTS    0x670
#define CMD_STORE_DACS         0x671
#define CMD_STORE_REGISTERS    0x672
#define CMD_STORE_PIXCONF      0x673
#define CMD_ERASE_ADDRPORTS    0x674
#define CMD_ERASE_DACS         0x675
#define CMD_ERASE_REGISTERS    0x676
#define CMD_ERASE_PIXCONF      0x677
#define CMD_VALID_ADDRPORTS    0x678
#define CMD_VALID_DACS         0x679
#define CMD_VALID_REGISTERS    0x67A
#define CMD_VALID_PIXCONF      0x67B

// Firmware update
#define CMD_READ_FLASH         0x67E
#define CMD_WRITE_FLASH        0x67F

// Other
#define CMD_GET_SPIDRREG       0x783
#define CMD_SET_SPIDRREG       0x784
#define CMD_SET_CHIPBOARDID    0x785
#define CMD_SET_BOARDID        0x786
#define CMD_REINIT_MACADDR     0x787

// Short strings describing the commands
// (indexed by the lower byte of the command identifier)
static const char *CMD_STR[] =
  {
    "<no operation>   ", // 0x900
    "GET_SOFTWVERSION ", // 0x901
    "GET_FIRMWVERSION ", // 0x902
    "GET_DEVICETYPE   ", // 0x903
    "SET_DEVICETYPE   ", // 0x904
    "GET_UDPPACKET_SZ ", // 0x905
    "SET_UDPPACKET_SZ ", // 0x906
    "RESET_MODULE     ", // 0x907
    "SET_BUSY         ", // 0x908
    "CLEAR_BUSY       ", // 0x909
    "SET_LOGLEVEL     ", // 0x90A
    "DISPLAY_INFO     ", // 0x90B
    "SET_TIMEOFDAY    ", // 0x90C
    "GET_DEVICECOUNT  ", // 0x90D
    "GET_BOARDID      ", // 0x90E
    "GET_CHIPBOARDID  ", // 0x90F

    "GET_DEVICEID     ", // 0x110
    "GET_DEVICEIDS    ", // 0x111
    "GET_IPADDR_SRC   ", // 0x112
    "SET_IPADDR_SRC   ", // 0x113
    "GET_IPADDR_DEST  ", // 0x114
    "SET_IPADDR_DEST  ", // 0x115
    "GET_DEVICEPORT   ", // 0x116
    "GET_SERVERPORT   ", // 0x117
    "-----",             // 0x118
    "SET_SERVERPORT   ", // 0x119
    "GET_DAC          ", // 0x11A
    "SET_DAC          ", // 0x11B
    "SET_DACS         ", // 0x11C
    "-----",             // 0x11D
    "-----",             // 0x11E
    "SET_DACS_DFLT    ", // 0x11F
    "CONFIG_CTPR      ", // 0x120
    "SET_CTPR         ", // 0x121
    "GET_ACQENABLE    ", // 0x122
    "SET_ACQENABLE    ", // 0x123
    "RESET_DEVICE     ", // 0x124
    "RESET_DEVICES    ", // 0x125
    "SET_READY        ", // 0x126
    "-----",             // 0x127
    "-----",             // 0x128
    "-----",             // 0x129

    "PIXCONF_MPX3_0   ", // 0x22A
    "PIXCONF_MPX3_1   ", // 0x22B
    "PIXCONF_MPX3RX   ", // 0x22C
    "-----",             // 0x22D
    "-----",             // 0x22E
    "-----",             // 0x22F

    "SET_CRW          ", // 0x330
    "SET_POLARITY     ", // 0x331
    "SET_DISCCSMSPM   ", // 0x332
    "SET_INTERNALTP   ", // 0x333
    "SET_COUNTERDEPTH ", // 0x334
    "SET_EQTHRESHH    ", // 0x335
    "SET_COLOURMODE   ", // 0x336
    "SET_CSMSPM       ", // 0x337
    "SET_SENSEDAC     ", // 0x338
    "SET_PS           ", // 0x339
    "SET_EXTDAC       ", // 0x33A
    "WRITE_OMR        ", // 0x33B
    "SET_GAINMODE     ", // 0x33C
    "-----",             // 0x33D
    "-----",             // 0x33E
    "-----",             // 0x33F

    "GET_TRIGCONFIG   ", // 0x440
    "SET_TRIGCONFIG   ", // 0x441
    "AUTOTRIG_START   ", // 0x442
    "AUTOTRIG_STOP    ", // 0x443
    "TRIGGER_READOUT  ", // 0x444
    "-----",             // 0x445
    "-----",             // 0x446
    "-----",             // 0x447

    "-----",             // 0x548
    "GET_REMOTETEMP   ", // 0x549
    "GET_LOCALTEMP    ", // 0x54A
    "GET_AVDD         ", // 0x54B
    "GET_DVDD         ", // 0x54C
    "GET_AVDD_NOW     ", // 0x54D
    "GET_SPIDR_ADC    ", // 0x54E
    "GET_DVDD_NOW     ", // 0x54F

    "-----",             // 0x550
    "-----",             // 0x551
    "-----",             // 0x552
    "-----",             // 0x553
    "-----",             // 0x554
    "-----",             // 0x555
    "-----",             // 0x556
    "-----",             // 0x557
    "RESET_COUNTERS   ", // 0x558
    "-----",             // 0x559
    "-----",             // 0x55A
    "-----",             // 0x55B
    "-----",             // 0x55C
    "-----",             // 0x55D
    "-----",             // 0x55E
    "BIAS_SUPPLY_ENA  ", // 0x55F

    "SET_BIAS_ADJUST  ", // 0x560
    "DECODERS_ENA     ", // 0x561
    "-----",             // 0x562
    "-----",             // 0x563
    "-----",             // 0x564
    "-----",             // 0x565
    "-----",             // 0x566
    "-----",             // 0x567
    "GET_FPGATEMP     ", // 0x568
    "GET_FANSPEED     ", // 0x569
    "SET_FANSPEED     ", // 0x56A
    "-----",             // 0x56B
    "GET_VDD          ", // 0x56C
    "GET_VDD_NOW      ", // 0x56D
    "GET_HUMIDITY     ", // 0x56E
    "GET_PRESSURE     ", // 0x56F

    "STORE_ADDRPORTS  ", // 0x670
    "STORE_DACS       ", // 0x671
    "STORE_REGISTERS  ", // 0x672
    "STORE_PIXCONF    ", // 0x673
    "ERASE_ADDRPORTS  ", // 0x674
    "ERASE_DACS       ", // 0x675
    "ERASE_REGISTERS  ", // 0x676
    "ERASE_PIXCONF    ", // 0x677
    "VALID_ADDRPORTS  ", // 0x678
    "VALID_DACS       ", // 0x679
    "VALID_REGISTERS  ", // 0x67A
    "VALID_PIXCONF    ", // 0x67B
    "-----",             // 0x67C
    "-----",             // 0x67D
    "READ_FLASH       ", // 0x67E
    "WRITE_FLASH      ", // 0x67F

    "-----",             // 0x780
    "-----",             // 0x781
    "-----",             // 0x782
    "GET_SPIDRREG     ", // 0x783
    "SET_SPIDRREG     ", // 0x784
    "SET_CHIPBOARDID  ", // 0x785
    "SET_BOARDID      ", // 0x786
    "REINIT_MACADDR   "  // 0x787
  };

// Reply bit: set in the reply message in the command identifier
#define CMD_REPLY            0x00010000

// No-reply bit: set in the command message in the command identifier
// indicating to the SPIDR server that no reply is expected
// (to speed up certain operations, such as pixel configuration uploads)
#define CMD_NOREPLY          0x00080000

#define CMD_MASK             0x0000FFFF

// Error identifiers in replies from the SPIDR module
// (in first byte; 2nd to 4th byte can be used for additional info)
#define ERR_NONE             0x00000000
#define ERR_UNKNOWN_CMD      0x00000001
#define ERR_MSG_LENGTH       0x00000002
#define ERR_SEQUENCE         0x00000003
#define ERR_ILLEGAL_PAR      0x00000004
#define ERR_NOT_IMPLEMENTED  0x00000005
#define ERR_MPX3_HARDW       0x00000006
#define ERR_ADC_HARDW        0x00000007
#define ERR_DAC_HARDW        0x00000008
#define ERR_MON_HARDW        0x00000009
#define ERR_FLASH_STORAGE    0x0000000A
#define ERR_MONITOR          0x0000000B

// Short strings describing the errors
// (indexed by the lower byte of the error identifier)
static const char *ERR_STR[] =
  {
    "no error",
    "ERR_UNKNOWN_CMD",
    "ERR_MSG_LENGTH",
    "ERR_SEQUENCE",
    "ERR_ILLEGAL_PAR",
    "ERR_NOT_IMPLEMENTED",
    "ERR_MPX3_HARDW",
    "ERR_ADC_HARDW",
    "ERR_DAC_HARDW",
    "ERR_MON_HARDW",
    "ERR_FLASH_STORAGE"
  };

#endif // SPIDRMPX3CMDS_H
