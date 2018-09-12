// Definitions concerning event and device headers

#ifndef SPIDRDATA_H
#define SPIDRDATA_H

// Format versioning
#define EVT_HEADER_VERSION  0x00000001
#define DEV_HEADER_VERSION  0x00000001
#define DEV_DATA_DECODED    0x10000000
#define DEV_DATA_COMPRESSED 0x20000000

typedef struct EvtHeader
{
  uint32_t headerId;
  uint32_t format;
  uint32_t headerSize;
  uint32_t dataSize;     // Including device headers but not this header
  uint32_t ipAddress;
  uint32_t nrOfDevices;
  uint32_t ports[4];
  uint32_t evtNr;
  uint32_t secs;         // Date-time of frame arrival (complete)
  uint32_t msecs;
  uint32_t pixelDepth;
  uint32_t triggerConfig[5];
  uint32_t unused[32-19];
} EvtHeader_t;

typedef struct DevHeader
{
  uint32_t headerId;
  uint32_t format;
  uint32_t headerSize;
  uint32_t dataSize;       // Not including this header
  uint32_t deviceId;
  uint32_t deviceType;
  uint32_t spidrHeader[3]; // Trigger/shutter counter, sequence number, time stamp
  uint32_t lostPackets;
  uint32_t unused[16-10];
} DevHeader_t;

typedef struct SpidrHeader
{
  // NB: copy of header produced by the SPIDR module; values are big-endian
  uint16_t triggerCnt;
  uint16_t shutterCnt;
  uint16_t sequenceNr;
  uint16_t timeLo;
  uint16_t timeMi;
  uint16_t timeHi;
} SpidrHeader_t;

#define EVT_HEADER_SIZE      sizeof(EvtHeader_t)
#define DEV_HEADER_SIZE      sizeof(DevHeader_t)
#define SPIDR_HEADER_SIZE    sizeof(SpidrHeader_t)

#define MPX3_12BIT_RAW_SIZE  ((256 * 256 * 12) / 8)
#define MPX3_24BIT_RAW_SIZE  ((256 * 256 * 24) / 8)
#define MPX3_MAX_FRAME_SIZE  MPX3_24BIT_RAW_SIZE

#define EVT_HEADER_ID        0x52445053 // Represents "SPDR"
#define DEV_HEADER_ID        0x3358504D // Represents "MPX3"
#define HEADER_FILLER_WORD   0xDDDDDDDD // For unused words in headers

#endif // SPIDRDATA_H
