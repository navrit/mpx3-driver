#include <QCoreApplication>

#include "SpidrController.h"
#include "SpidrDaq.h"
#include "UdpReceiver.h"
#include "FrameAssembler.h"

// Version identifier: year, month, day, release number

// - Fixes for 24-bit readout in the framebuilders' mpx3RawToPixel() and
//   processFrame() functions.
// - Fix for SPIDR-LUT decoded row counter (firmware bug) in
//   FramebuilderThreadC::mpx3RawToPixel(): just count EOR pixelpackets instead.
const int   VERSION_ID = 0x18101200;

// - Fix 24-bit bug in ReceiverThread::setPixelDepth().
// - Tolerate SOF out-of-order in ReceiverThreadC::readDatagrams().
// - Use row counter in EOR/EOF pixel packets in
//   FramebuilderThreadC::mpx3RawToPixel().
//const int VERSION_ID = 0x17020200;

//const int VERSION_ID = 0x16082900; // Add frameFlags()
//const int VERSION_ID = 0x16061400; // Add info header processing
                                     // (ReceiverThreadC)
//const int VERSION_ID = 0x16040800; // Add parameter readout_mask to c'tor
//const int VERSION_ID = 0x16032400; // Renamed disableLut() to setLutEnable()
//const int VERSION_ID = 0x16030900; // Compact-SPIDR support added
//const int VERSION_ID = 0x15101500;
//const int VERSION_ID = 0x15100100;
//const int VERSION_ID = 0x15093000;
//const int VERSION_ID = 0x15092100;
//const int VERSION_ID = 0x15051900;
//const int VERSION_ID = 0x14012400;

// At least one argument needed for QCoreApplication
//int   Argc = 1;
//char *Argv[] = { "SpidrDaq" }; 
//QCoreApplication *SpidrDaq::App = 0;
// In c'tor?: Create the single 'QCoreApplication' we need for the event loop
// in the receiver objects  ### SIGNALS STILL DO NOT WORK? Need exec() here..
//if( App == 0 ) App = new QCoreApplication( Argc, Argv );

// ----------------------------------------------------------------------------
// Constructor / destructor / info
// ----------------------------------------------------------------------------

SpidrDaq::SpidrDaq( int ipaddr3,
		    int ipaddr2,
		    int ipaddr1,
		    int ipaddr0,
		    int port,
		    int readout_mask )
{
  // Start data-acquisition with the given read-out mask
  // on the SPIDR module with the given IP address and port number
  int ipaddr[4] = { ipaddr0, ipaddr1, ipaddr2, ipaddr3 };
  int ids[4]    = { 0, 0, 0, 0 };
  int ports[4]  = { port, port+1, port+2, port+3 };
  int types[4]  = { 0, 0, 0, 0 };

  // Adjust SPIDR read-out mask if requested:
  // Reset unwanted ports/devices to 0
  for( int i=0; i<4; ++i )
    if( (readout_mask & (1<<i)) == 0 ) ports[i] = 0;
  // Read out the remaining devices
  readout_mask = 0;
  for( int i=0; i<4; ++i )
    if( ports[i] != 0 ) readout_mask |= (1<<i);

  this->init( ipaddr, ids, ports, types, 0 );
}

// ----------------------------------------------------------------------------

SpidrDaq::SpidrDaq( SpidrController *spidrctrl,
		    int              readout_mask )
{
  // If a SpidrController object is provided use it to find out the SPIDR's
  // Medipix device configuration and IP destination address, or else assume
  // a default IP address and a single device with a default port number
  int ipaddr[4] = { 1, 1, 168, 192 };
  int ids[4]    = { 0, 0, 0, 0 };
  int ports[4]  = { 8192, 0, 0, 0 };
  int types[4]  = { 0, 0, 0, 0 };
  if( spidrctrl )
    {
      // Get the IP destination address (this host network interface)
      // from the SPIDR module
      int addr = 0;
      if( spidrctrl->getIpAddrDest( 0, &addr ) )
	{
	  ipaddr[3] = (addr >> 24) & 0xFF;
	  ipaddr[2] = (addr >> 16) & 0xFF;
	  ipaddr[1] = (addr >>  8) & 0xFF;
	  ipaddr[0] = (addr >>  0) & 0xFF;
	}

      this->getIdsPortsTypes( spidrctrl, ids, ports, types );

      // Adjust SPIDR read-out mask if requested:
      // Reset unwanted ports/devices to 0
      for( int i=0; i<4; ++i )
	if( (readout_mask & (1<<i)) == 0 ) ports[i] = 0;
      // Read out the remaining devices
      readout_mask = 0;
      for( int i=0; i<4; ++i )
	if( ports[i] != 0 ) readout_mask |= (1<<i);

      // Set the new read-out mask if required
      int device_mask;
      if( spidrctrl->getAcqEnable(&device_mask) && device_mask != readout_mask )
	spidrctrl->setAcqEnable( readout_mask );
    }
  this->init( ipaddr, ids, ports, types, spidrctrl );
}

// ----------------------------------------------------------------------------

void SpidrDaq::getIdsPortsTypes( SpidrController *spidrctrl,
				 int             *ids,
				 int             *ports,
				 int             *types )
{
  if( !spidrctrl ) return;

  // Get the device IDs from the SPIDR module
  spidrctrl->getDeviceIds( ids );

  // Get the device port numbers from the SPIDR module
  // but only for devices whose ID could be determined (i.e. is unequal to 0)
  for( int i=0; i<4; ++i )
    {
      ports[i] = 0;
      types[i] = 0;
      if( ids[i] != 0 )
	{
	  spidrctrl->getServerPort( i, &ports[i] );
	  spidrctrl->getDeviceType( i, &types[i] );
	}
    }
}

// ----------------------------------------------------------------------------

void SpidrDaq::init( int             *ipaddr,
		     int             *ids,
		     int             *ports,
		     int             *types,
		     SpidrController *spidrctrl )
{
    int fwVersion;
    spidrctrl->getFirmwVersion(&fwVersion);
    udpReceiver = new UdpReceiver(fwVersion < 0x18100100);
    if (udpReceiver->initThread("", ports[0]) == true) {
        th = udpReceiver->spawn();
    }
    frameSetManager = udpReceiver->getFrameSetManager();
}

// ----------------------------------------------------------------------------

SpidrDaq::~SpidrDaq()
{
  this->stop();
}

// ----------------------------------------------------------------------------

void SpidrDaq::stop()
{
    /*
  if( _frameBuilder )
    {
      _frameBuilder->stop();
      delete _frameBuilder;
      _frameBuilder = 0;
    }
  for( unsigned int i=0; i<_frameReceivers.size(); ++i )
    {
      _frameReceivers[i]->stop();
      delete _frameReceivers[i];
    }
  _frameReceivers.clear();
  */
}

// ----------------------------------------------------------------------------
// General
// ----------------------------------------------------------------------------

int SpidrDaq::classVersion()
{
  return VERSION_ID;
}

// ----------------------------------------------------------------------------

std::string SpidrDaq::ipAddressString( int index )
{
  //if( index < 0 || index >= (int) _frameReceivers.size() )
    return std::string( "" );
  //return _frameReceivers[index]->ipAddressString();
}

// ----------------------------------------------------------------------------

std::string SpidrDaq::errorString()
{
  std::string str;
  /*
  for( unsigned int i=0; i<_frameReceivers.size(); ++i )
    {
      if( !str.empty() && !_frameReceivers[i]->errString().empty() )
	str += std::string( ", " );
      str += _frameReceivers[i]->errString();
    }
  if( !str.empty() && !_frameBuilder->errString().empty() )
    str += std::string( ", " );
  str += _frameBuilder->errString();

  // Clear the error strings
  for( unsigned int i=0; i<_frameReceivers.size(); ++i )
    _frameReceivers[i]->clearErrString();
  _frameBuilder->clearErrString();
 */
  return str;
}

// ----------------------------------------------------------------------------

bool SpidrDaq::hasError()
{ /*
  for( unsigned int i=0; i<_frameReceivers.size(); ++i )
    if( !_frameReceivers[i]->errString().empty() )
      return true;
  if( !_frameBuilder->errString().empty() )
    return true; */
  return false;
}

// ----------------------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------------------

void SpidrDaq::setPixelDepth( int nbits )
{ /*
  for( unsigned int i=0; i<_frameReceivers.size(); ++i )
    _frameReceivers[i]->setPixelDepth( nbits );
  _frameBuilder->setPixelDepth( nbits ); */
}

// ----------------------------------------------------------------------------

void SpidrDaq::setLutEnable( bool enable )
{
  //_frameBuilder->setLutEnable( enable );
}

// ----------------------------------------------------------------------------
// Acquisition
// ----------------------------------------------------------------------------

bool SpidrDaq::hasFrame( unsigned long timeout_ms )
{
  return frameSetManager->wait(timeout_ms);
}

// ----------------------------------------------------------------------------

FrameSet *SpidrDaq::getFrameSet()
{
  return frameSetManager->getFrameSet();
}

// ----------------------------------------------------------------------------

void SpidrDaq::releaseFrame(FrameSet *fs)
{
  frameSetManager->releaseFrameSet(fs);
}

// ----------------------------------------------------------------------------
// Statistics
// ----------------------------------------------------------------------------

int SpidrDaq::framesCount()
{
  return frameSetManager->_framesReceived;
}

// ----------------------------------------------------------------------------

int SpidrDaq::framesLostCount()
{
  return frameSetManager->_framesLost;
}

// ----------------------------------------------------------------------------

void SpidrDaq::resetLostCount()
{
  frameSetManager->_framesLost = 0;
}
