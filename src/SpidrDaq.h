#ifndef SPIDRDAQ_H
#define SPIDRDAQ_H

#ifdef WIN32
 // On Windows differentiate between building the DLL or using it
 #ifdef MY_LIB_EXPORT
 #define MY_LIB_API __declspec(dllexport)
 #else
 #define MY_LIB_API __declspec(dllimport)
 #endif
#else
 // Linux
 #define MY_LIB_API
#endif // WIN32

#include <string>
#include <vector>
#include <thread>

#include "FrameSet.h"
#include "FrameSetManager.h"

class SpidrController;
class UdpReceiver;
class FrameAssembler;
//class QCoreApplication;

typedef void (*CallbackFunc)( int id );

class MY_LIB_API SpidrDaq
{
 public:
  // C'tor, d'tor
  SpidrDaq( int ipaddr3, int ipaddr2, int ipaddr1, int ipaddr0,
	    int port, int readout_mask = 0xF );
  SpidrDaq( SpidrController *spidrctrl, int readout_mask = 0xF );
  ~SpidrDaq();

  // General
  void        stop              ( ); // To be called before exiting/deleting
  int         classVersion      ( ); // Version of this class
  std::string ipAddressString   ( int index );
  std::string errorString       ( );
  bool        hasError          ( );

  // Configuration
  void setPixelDepth            ( int nbits );
  //void setDecodeFrames          ( bool decode );
  //void setCompressFrames        ( bool compress );
  void setLutEnable             ( bool enable );
  //bool openFile                 ( std::string filename,
  //                                bool overwrite = false );
  //bool closeFile                ( );

  // Acquisition
  //int       numberOfDevices     ( ) { return (int) _frameReceivers.size(); }
  bool      hasFrame            ( unsigned long timeout_ms = 0 );
  FrameSet  *getFrameSet           ();
  void      releaseFrame        (FrameSet *fs = nullptr);
  //int       frameShutterCounter ( int index = -1 );
  //bool      isCounterhFrame     ( int index = -1 );
  //int       frameFlags          ( int index );
  //long long frameTimestamp      ( );
  //long long frameTimestamp      ( int buf_i );        // For debugging
  //long long frameTimestampSpidr ( );
  //double    frameTimestampDouble( );                  // For Pixelman
  //void      setCallbackId       ( int id );           // For Pixelman
  //void      setCallback         ( CallbackFunc cbf ); // For Pixelman

  // Statistics and info
  //int  framesWrittenCount       ( );
  //int  framesProcessedCount     ( );
  //int  framesCount              ( int index );
  int  framesCount              ( );
  //int  framesLostCount          ( int index );
  int  framesLostCount          ( );
  //int  packetsReceivedCount     ( int index );
  //int  packetsReceivedCount     ( );
  //int  lostCount                ( int index );
  //int  lostCount                ( );
  void resetLostCount           ( );
  //int  lostCountFile            ( );
  //int  lostCountFrame           ( );

  //int  packetsLostCountFrame    ( int index, int buf_i ); // For debugging
  //int  packetSize               ( int index );            // For debugging
  //int  expSequenceNr            ( int index );            // For debugging

  //int  pixelsReceivedCount      ( int index );
  //int  pixelsReceivedCount      ( );
  //int  pixelsLostCount          ( int index );
  //int  pixelsLostCount          ( );
  //int  pixelsLostCountFrame     ( int index, int buf_i ); // For debugging

 private:
  FrameSetManager *frameSetManager;
  UdpReceiver * udpReceiver;
  FrameAssembler *_frameBuilder;
  std::thread th;

  // Functions used in c'tors
  void getIdsPortsTypes( SpidrController *spidrctrl,
                         int             *ids,
                         int             *ports,
                         int             *types );
  void init( int             *ipaddr,
             int             *ids,
             int             *ports,
             int             *types,
             SpidrController *spidrctrl );
};

#endif // SPIDRDAQ_H
