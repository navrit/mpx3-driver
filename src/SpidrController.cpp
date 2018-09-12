#include <QTcpSocket>
#include <iomanip>
using namespace std;

#ifdef WIN32
#include <winsock2.h>  // For htonl() and ntohl()
#else
#include <arpa/inet.h> // For htonl() and ntohl()
#endif

#include "SpidrController.h"
#include "mpx3defs.h"
#include "spidrmpx3cmds.h"

#include "mpx3dacsdescr.h" // Depends on mpx3defs.h to be included first

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x17080100; // setPixelDepth(): softw double-cntr read,
                                     // Bug fix: initialize _pixelDepth,
                                     // setTpSwitch() -> setTpFrequency()
//const int VERSION_ID = 0x17020800; // Remove getAdc(int, int *)
//const int VERSION_ID = 0x17020700; // Add reinitMacAddr(), getHumidity/Pressure()
//const int VERSION_ID = 0x16082900; // Add getMpx3Clock(), remove setMpx3Clock()
//const int VERSION_ID = 0x16071800; // Add resetCounters() and counter reads
//const int VERSION_ID = 0x16062900; // Add setTpSwitch(), setPs()
//const int VERSION_ID = 0x16062100; // Add open/closeShutter(), startContReadout();
                                     // bug fix in ReceiverThreadC.cpp
//const int VERSION_ID = 0x16061600; // Add setMpx3Clock()
//const int VERSION_ID = 0x16032400; // Add bias, LUT and continuous-readout functions
//const int VERSION_ID = 0x16020100; // Add getFpgaTemp(), get/setFanSpeed()
//const int VERSION_ID = 0x15092800; // Add pixelconfig read-back option
//const int VERSION_ID = 0x15082600; // Add getOmr(); optimization in
                                     // request..() functions;
                                     // add requestGetBytes()
//const int VERSION_ID = 0x15042800; // triggerSingleReadout(countl_or_h)
//const int VERSION_ID = 0x15011300; // spidrErrString(); dacMax(), dacName()
                                     // parameter is DAC code (not 'index')
//const int VERSION_ID = 0x14121200; // Reinstate loadOmr() as private member;
                                     // SPIDR loads each OMR setting immediately
//const int VERSION_ID = 0x14121100; // Remove writeOmr(), add setEnablePixelCom()
//const int VERSION_ID = 0x14120800; // Remove readDacs(), writeDacs(Dflt)()
//const int VERSION_ID = 0x14112500; // Use DAC code instead of DAC index
//const int VERSION_ID = 0x14111200; // Added setGainMode()
//const int VERSION_ID = 0x14091600; // Update to SPIDR-TPX3 'standard'
//const int VERSION_ID = 0x14012100;

// SPIDR register addresses (some of them) and register bits
#define SPIDR_SHUTTERTRIG_CTRL_I        0x0290
#define SPIDR_EXT_SHUTTER_CNTR_I        0x02A0
#define SPIDR_SHUTTER_CNTR_I            0x02A4
#define SPIDR_ENA_SHUTTER1_CNTRSEL_BIT  9
#define SPIDRMPX3_SHUTTER1_PERIOD_I     0x1008
#define SPIDRMPX3_MPX3_CTRL_I           0x1090
#define SPIDRMPX3_TP_SWITCH_BIT         3
#define SPIDRMPX3_MPX3_CLOCK_OLD_I      0x10B0
#define SPIDRMPX3_MPX3_CLOCK_I          0x10B4
#define SPIDRMPX3_SHUTTER_INH_CNTR_I    0x10B8
#define SPIDRMPX3_TPSWITCH_FREQ_I       0x10BC
#define SPIDRMPX3_TP_LENGTH_I           0x10C0

// ----------------------------------------------------------------------------
// Constructor / destructor
// ----------------------------------------------------------------------------

SpidrController::SpidrController( int ipaddr3,
				  int ipaddr2,
				  int ipaddr1,
				  int ipaddr0,
				  int port )
{
  _sock = new QTcpSocket;

  ostringstream oss;
  oss << (ipaddr3 & 0xFF) << "." << (ipaddr2 & 0xFF) << "."
      << (ipaddr1 & 0xFF) << "." << (ipaddr0 & 0xFF);

  _sock->connectToHost( QString::fromStdString( oss.str() ), port );

  _sock->waitForConnected( 5000 );

  // Initialize the local pixel configuration data array to all zeroes
  this->resetPixelConfig();

  _busyRequests = 0;
  _errId = 0;
}

// ----------------------------------------------------------------------------

SpidrController::~SpidrController()
{
  if( _sock )
    {
      _sock->close();
      delete _sock;
    }
}

// ----------------------------------------------------------------------------
// Version information
// ----------------------------------------------------------------------------

int SpidrController::classVersion()
{
  return VERSION_ID;
}

// ----------------------------------------------------------------------------

bool SpidrController::getSoftwVersion( int *version )
{
  return this->requestGetInt( CMD_GET_SOFTWVERSION, 0, version );
}

// ----------------------------------------------------------------------------

bool SpidrController::getFirmwVersion( int *version )
{
  return this->requestGetInt( CMD_GET_FIRMWVERSION, 0, version );
}

// ----------------------------------------------------------------------------

std::string SpidrController::versionToString( int version )
{
  ostringstream oss;
  oss << hex << uppercase << setfill('0') << setw(8) << version;
  return oss.str();
}

// ----------------------------------------------------------------------------
// General module configuration
// ----------------------------------------------------------------------------

bool SpidrController::isCompactSpidr()
{
  // Is the module a Compact-SPIDR module?
  // (based on the software version least-significant byte >= 0x0A)
  bool is_cspidr = false;
  int version;
  if( this->getSoftwVersion( &version ) )
    if( (version & 0xFF) != 0xFF && (version & 0xFF) > 0x09 )
      is_cspidr = true;
  return is_cspidr;
}

// ----------------------------------------------------------------------------

bool SpidrController::isConnected()
{
  return( _sock->state() == QAbstractSocket::ConnectedState );
}

// ----------------------------------------------------------------------------

std::string SpidrController::connectionStateString()
{
  QAbstractSocket::SocketState state = _sock->state();
  if( state == QAbstractSocket::UnconnectedState )
    return string( "unconnected" );
  else if( state == QAbstractSocket::HostLookupState )
    return string( "hostlookup" );
  else if( state == QAbstractSocket::ConnectingState )
    return string( "connecting" );
  else if( state == QAbstractSocket::ConnectedState )
    return string( "connected" );
  else if( state == QAbstractSocket::BoundState )
    return string( "bound" );
  else if( state == QAbstractSocket::ClosingState )
    return string( "closing" );
  else
    return string( "???" );
}

// ----------------------------------------------------------------------------

std::string SpidrController::connectionErrString()
{
  return _sock->errorString().toStdString();
}

// ----------------------------------------------------------------------------

std::string SpidrController::ipAddressString()
{
  // Return a string like: "192.168.1.10:50000"
  QString qs = _sock->peerName();
  qs += ':';
  qs += QString::number( _sock->peerPort() );
  return qs.toStdString();
}

// ----------------------------------------------------------------------------

std::string SpidrController::errorString()
{
  return _errString.str();
}

// ----------------------------------------------------------------------------

void SpidrController::clearErrorString()
{
  _errString.str( "" );
}

// ----------------------------------------------------------------------------

int SpidrController::errorId()
{
  return _errId;
}

// ----------------------------------------------------------------------------

bool SpidrController::getMaxPacketSize( int *size )
{
  // The returned size is the total size of the Ethernet packets
  // with Medipix3 frame data, including all headers
  // (since this is the value limited/set by a network interface card)
  *size = 0;
  if( this->requestGetInt( CMD_GET_UDPPACKET_SZ, 0, size ) )
    {
      // Adjust the received value for the additional header sizes
      // not included by the SPIDR register in question, i.e.
      // SPIDR header   : 12 bytes
      // UDP header     :  8 bytes
      // IP  header     : 20 bytes
      // Ethernet header: 14 bytes
      //                 ----
      //                  54 bytes
      *size += 54;
      return true;
    }
  return false;
}

// ----------------------------------------------------------------------------

bool SpidrController::setMaxPacketSize( int size )
{
  // Set the maximum packet size of the Ethernet packets
  // carrying Medipix3 frame data, so including all headers (see above)
  if( size < 54 )
    size = 0;
  else
    size -= 54;
  return this->requestSetInt( CMD_SET_UDPPACKET_SZ, 0, size );
}

// ----------------------------------------------------------------------------

bool SpidrController::reset( int *errorstat )
{
  return this->requestGetInt( CMD_RESET_MODULE, 0, errorstat );
}

// ----------------------------------------------------------------------------

bool SpidrController::setBusy()
{
  return this->requestSetInt( CMD_SET_BUSY, 0, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::clearBusy()
{
  return this->requestSetInt( CMD_CLEAR_BUSY, 0, 0 );
}

// ----------------------------------------------------------------------------

void SpidrController::setBusyRequest()
{
  // To be used by the receiver threads
  this->setBusy();
  ++_busyRequests;
}

// ----------------------------------------------------------------------------

void SpidrController::clearBusyRequest()
{
  // To be used by the receiver threads
  --_busyRequests;
  if( _busyRequests == 0 ) this->clearBusy();
}

// ----------------------------------------------------------------------------

bool SpidrController::setLogLevel( int level )
{
  return this->requestSetInt( CMD_SET_LOGLEVEL, 0, level );
}

// ----------------------------------------------------------------------------

bool SpidrController::displayInfo()
{
  return this->requestSetInt( CMD_DISPLAY_INFO, 0, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::getDeviceCount( int *count )
{
  return this->requestGetInt( CMD_GET_DEVICECOUNT, 0, count );
}

// ----------------------------------------------------------------------------

bool SpidrController::getChipboardId( int *id )
{
  return this->requestGetInt( CMD_GET_CHIPBOARDID, 0, id );
}

// ----------------------------------------------------------------------------

bool SpidrController::setChipboardId( int id )
{
  return this->requestSetInt( CMD_SET_CHIPBOARDID, 0, id );
}

// ----------------------------------------------------------------------------
// Configuration: module/device interface
// ----------------------------------------------------------------------------

bool SpidrController::getIpAddrSrc( int index, int *ipaddr )
{
  return this->requestGetInt( CMD_GET_IPADDR_SRC, index, ipaddr );
}

// ----------------------------------------------------------------------------

bool SpidrController::setIpAddrSrc( int index, int ipaddr )
{
  return this->requestSetInt( CMD_SET_IPADDR_SRC, index, ipaddr );
}

// ----------------------------------------------------------------------------

bool SpidrController::getIpAddrDest( int index, int *ipaddr )
{
  return this->requestGetInt( CMD_GET_IPADDR_DEST, index, ipaddr );
}

// ----------------------------------------------------------------------------

bool SpidrController::setIpAddrDest( int index, int ipaddr )
{
  return this->requestSetInt( CMD_SET_IPADDR_DEST, index, ipaddr );
}

// ----------------------------------------------------------------------------

bool SpidrController::getDevicePort( int index, int *port_nr )
{
  return this->requestGetInt( CMD_GET_DEVICEPORT, index, port_nr );
}

// ----------------------------------------------------------------------------
/*
### Probably won't be used, so outcommented, at least for now (Jan 2014)
bool SpidrController::getDevicePorts( int *port_nrs )
{
  return this->requestGetInts( CMD_GET_DEVICEPORTS, 0, 4, port_nrs );
}

// ----------------------------------------------------------------------------

bool SpidrController::setDevicePort( int dev_nr, int port_nr )
{
  return this->requestSetInt( CMD_SET_DEVICEPORT, dev_nr, port_nr );
}
*/
// ----------------------------------------------------------------------------

bool SpidrController::getServerPort( int dev_nr, int *port_nr )
{
  return this->requestGetInt( CMD_GET_SERVERPORT, dev_nr, port_nr );
}

// ----------------------------------------------------------------------------
/*
bool SpidrController::getServerPorts( int *port_nrs )
{
  return this->requestGetInts( CMD_GET_SERVERPORTS, 0, 4, port_nrs );
}
*/
// ----------------------------------------------------------------------------

bool SpidrController::setServerPort( int dev_nr, int port_nr )
{
  return this->requestSetInt( CMD_SET_SERVERPORT, dev_nr, port_nr );
}

// ----------------------------------------------------------------------------

bool SpidrController::reinitMacAddr()
{
  // Let SPIDR reinitialize the MAC addresses associated with
  // the UDP device data streams
  return this->requestSetInt( CMD_REINIT_MACADDR, 0, 0 );
}

// ----------------------------------------------------------------------------
// Configuration: devices
// ----------------------------------------------------------------------------

bool SpidrController::getDeviceId( int dev_nr, int *id )
{
  return this->requestGetInt( CMD_GET_DEVICEID, dev_nr, id );
}

// ----------------------------------------------------------------------------

bool SpidrController::getDeviceIds( int *ids )
{
  return this->requestGetInts( CMD_GET_DEVICEIDS, 0, 4, ids );
}

// ----------------------------------------------------------------------------

bool SpidrController::getDeviceType( int dev_nr, int *type )
{
  return this->requestGetInt( CMD_GET_DEVICETYPE, dev_nr, type );
}

// ----------------------------------------------------------------------------

bool SpidrController::setDeviceType( int dev_nr, int type )
{
  return this->requestSetInt( CMD_SET_DEVICETYPE, dev_nr, type );
}

// ----------------------------------------------------------------------------

bool SpidrController::getDac( int dev_nr, int dac_code, int *dac_val )
{
  int dac = dac_code;
  if( this->requestGetInt( CMD_GET_DAC, dev_nr, &dac ) )
    {
      // Extract dac_code and dac_val
      if( (dac >> 16) != dac_code )
	{
	  this->clearErrorString();
	  _errString << "DAC code mismatch in reply";
	  return false;
	}
      *dac_val = dac & 0xFFFF;
      return true;
    }
  return false;
}

// ----------------------------------------------------------------------------

bool SpidrController::setDac( int dev_nr, int dac_code, int dac_val )
{
  // Combine dac_code and dac_val into a single int
  int dac_data = ((dac_code & 0xFFFF) << 16) | (dac_val & 0xFFFF);
  return this->requestSetInt( CMD_SET_DAC, dev_nr, dac_data );
}

// ----------------------------------------------------------------------------

bool SpidrController::setDacs( int dev_nr, int nr_of_dacs, int *dac_val )
{
  return this->requestSetInts( CMD_SET_DACS, dev_nr, nr_of_dacs, dac_val );
}

// ----------------------------------------------------------------------------

bool SpidrController::setDacsDflt( int dev_nr )
{
  return this->requestSetInt( CMD_SET_DACS_DFLT, dev_nr, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::configCtpr( int dev_nr, int column, int val )
{
  // Combine column and val into a single int
  int ctpr = ((column & 0xFFFF) << 16) | (val & 0x0001);
  return this->requestSetInt( CMD_CONFIG_CTPR, dev_nr, ctpr );
}

// ----------------------------------------------------------------------------

bool SpidrController::setCtpr( int dev_nr )
{
  return this->requestSetInt( CMD_SET_CTPR, dev_nr, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::getAcqEnable( int *mask )
{
  return this->requestGetInt( CMD_GET_ACQENABLE, 0, mask );
}

// ----------------------------------------------------------------------------

bool SpidrController::setAcqEnable( int mask )
{
  return this->requestSetInt( CMD_SET_ACQENABLE, 0, mask );
}

// ----------------------------------------------------------------------------

bool SpidrController::resetDevice( int dev_nr )
{
  return this->requestSetInt( CMD_RESET_DEVICE, dev_nr, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::resetDevices()
{
  return this->requestSetInt( CMD_RESET_DEVICES, 0, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::setReady()
{
  return this->requestSetInt( CMD_SET_READY, 0, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::setBiasSupplyEna( bool enable )
{
  return this->requestSetInt( CMD_BIAS_SUPPLY_ENA, 0, (int) enable );
}

// ----------------------------------------------------------------------------

bool SpidrController::setBiasVoltage( int volts )
{
  // Parameter 'volts' should be between 12 and 104 Volts
  // (which is the range SPIDR-TPX3 can set)
  if( volts < 12 ) volts = 12;
  if( volts > 104 ) volts = 104;

  // Convert the volts to the appropriate DAC value
  int dac_val = ((volts - 12)*4095)/(104 - 12);

  return this->requestSetInt( CMD_SET_BIAS_ADJUST, 0, dac_val );
}

// ----------------------------------------------------------------------------

bool SpidrController::setLutEnable( bool enable )
{
  return this->requestSetInt( CMD_DECODERS_ENA, 0, (int) enable );
}

// ----------------------------------------------------------------------------

bool SpidrController::getMpx3Clock( int *megahertz )
{
  return this->getSpidrReg( SPIDRMPX3_MPX3_CLOCK_I, megahertz );
}

// ----------------------------------------------------------------------------
/*
bool SpidrController::setMpx3Clock( int megahertz )
{
  int id;
  if( megahertz == 64 )
    id = 0;
  else if( megahertz == 100 )
    id = 1;
  else if( megahertz == 128 )
    id = 2;
  else if( megahertz == 200 )
    id = 3;
  else
    {
      this->clearErrorString();
      _errString << "Invalid frequency parameter";
      return false;
    }
  // Write and verify
  return this->setSpidrReg( SPIDRMPX3_MPX3_CLOCK_I, id, true );
}
*/
// ----------------------------------------------------------------------------

bool SpidrController::setTpFrequency( bool enable, int freq_mhz,
                                      int pulse_width )
{
  // Configure the TP_Switch period (frequency): 25ns per count
  if( freq_mhz > 0 )
    this->setSpidrReg( SPIDRMPX3_TPSWITCH_FREQ_I,
		       (int) (((double) 40000000.0/(double) freq_mhz)*1000.0) );

  // Configure the TP pulse length: in units of 25ns
  if( pulse_width > 0 )
    this->setSpidrReg( SPIDRMPX3_TP_LENGTH_I, pulse_width );

  return this->setSpidrRegBit( SPIDRMPX3_MPX3_CTRL_I, SPIDRMPX3_TP_SWITCH_BIT,
			       enable, true );
}

// ----------------------------------------------------------------------------
// Configuration: pixels
// ----------------------------------------------------------------------------

void SpidrController::resetPixelConfig()
{
  // Set the local pixel configuration data array to all zeroes
  memset( static_cast<void *> (_pixelConfig), 0, sizeof(_pixelConfig) );
}

// ----------------------------------------------------------------------------

bool SpidrController::configPixelMpx3( int  x,
				       int  y,
				       int  configtha,
				       int  configthb,
				       bool configtha4,
				       bool configthb4,
				       bool gainmode,
				       bool testbit )
{
  int xstart, xend;
  int ystart, yend;
  if( !this->validXandY( x, y, &xstart, &xend, &ystart, &yend ) )
    return false;

  // Check other parameters
  bool invalid_parameter = false;
  if( configtha < 0 || configtha > 15 ) invalid_parameter = true;
  if( configthb < 0 || configthb > 15 ) invalid_parameter = true;
  if( invalid_parameter )
    {
      this->clearErrorString();
      _errString << "Invalid pixel config parameter";
      return false;
    }

  // Set or reset the configuration bits in the requested pixels
  int xi, yi;
  unsigned int *pcfg;
  for( yi=ystart; yi<yend; ++yi )
    for( xi=xstart; xi<xend; ++xi )
      {
	pcfg = &_pixelConfig[yi][xi];
	*pcfg &= MPX3_CFG_MASKBIT; // Set/reset elsewhere
	if( configtha & 0x1 ) *pcfg |= MPX3_CFG_CONFIGTHA_0;
	if( configtha & 0x2 ) *pcfg |= MPX3_CFG_CONFIGTHA_1;
	if( configtha & 0x4 ) *pcfg |= MPX3_CFG_CONFIGTHA_2;
	if( configtha & 0x8 ) *pcfg |= MPX3_CFG_CONFIGTHA_3;
	if( configtha4 )      *pcfg |= MPX3_CFG_CONFIGTHA_4;
	if( configthb & 0x1 ) *pcfg |= MPX3_CFG_CONFIGTHB_0;
	if( configthb & 0x2 ) *pcfg |= MPX3_CFG_CONFIGTHB_1;
	if( configthb & 0x4 ) *pcfg |= MPX3_CFG_CONFIGTHB_2;
	if( configthb & 0x8 ) *pcfg |= MPX3_CFG_CONFIGTHB_3;
	if( configthb4 )      *pcfg |= MPX3_CFG_CONFIGTHB_4;
	if( gainmode )        *pcfg |= MPX3_CFG_GAINMODE;
	if( testbit )         *pcfg |= MPX3_CFG_TESTBIT;
      }

  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::setPixelMaskMpx3( int x, int y, bool b )
{
  return this->setPixelBit( x, y, MPX3_CFG_MASKBIT, b );
}

// ----------------------------------------------------------------------------

bool SpidrController::setPixelConfigMpx3( int  dev_nr,
					  bool with_replies )
{
  // To be done in 2 stages:
  // 12 bits of configuration (bits 0-11) per pixel to 'Counter0', then
  // 12 bits (bits 12-23) to 'Counter1' (using variable 'counter' for that)

  // Space for one row (256 pixels) of Medipix-formatted
  // pixel configuration data
  unsigned char pixelrow[(256*12)/8];

  unsigned int   *pconfig;
  int             counter, row, column, pixelbit, pixelbitmask, bit;
  unsigned char  *prow;
  unsigned char   byte, bitmask;
  for( counter=0; counter<2; ++counter )
    {
      int hi_bit = 11 + counter*12;
      int lo_bit = counter*12;
      int cmd    = CMD_PIXCONF_MPX3_0 + counter;
      // ### Without replies doesn't work (yet) because -I think- TCP packets
      //     are concatenated at the receiver end and need to be interpreted;
      //     to be investigated...
      //if( !with_replies ) cmd |= CMD_NOREPLY;

      // Convert the data in _pixelConfig row-by-row into the format
      // required by the Medipix device
      for( row=0; row<256; ++row )
	{
	  // Next row
	  pconfig = &_pixelConfig[row][0];
	  // Refill pixelrow[]
	  memset( static_cast<void *> (pixelrow), 0, sizeof(pixelrow) );
	  // 12 bits of configuration data, starting from MSB
	  prow = pixelrow;
	  for( pixelbit=hi_bit; pixelbit>=lo_bit; --pixelbit )
	    {
	      pixelbitmask = (1 << pixelbit);
	      for( column=0; column<256; column+=8 )
		{
		  // Fill a byte
		  byte = 0;
		  bitmask = 0x01;
		  for( bit=0; bit<8; ++bit )
		    {
		      if( pconfig[column + bit] & pixelbitmask )
			byte |= bitmask;
		      bitmask <<= 1;
		    }
		  *prow = byte;
		  ++prow;
		}
	    }

	  // Even with 'with_replies' false, acknowledge first and last message
	  /*
	  if( row == 0 || row == 255 )
	    cmd &= ~CMD_NOREPLY;
	  else if( !with_replies )
	    cmd |= CMD_NOREPLY;
	  */
	  // Send this row of formatted configuration data to the SPIDR module
	  if( this->requestSetIntAndBytes( cmd, dev_nr,
					   row, // Sequence number
					   sizeof( pixelrow ),
					   pixelrow ) == false )
	    return false;
	}
    }
  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::configPixelMpx3rx( int  x,
					 int  y,
					 int  discl,
					 int  disch,
					 bool testbit )
{
  int xstart, xend;
  int ystart, yend;
  if( !this->validXandY( x, y, &xstart, &xend, &ystart, &yend ) )
    return false;

  // Check other parameters
  bool invalid_parameter = false;
  if( discl < 0 || discl > 31 ) invalid_parameter = true;
  if( disch < 0 || disch > 31 ) invalid_parameter = true;
  if( invalid_parameter )
    {
      this->clearErrorString();
      _errString << "Invalid pixel config parameter";
      return false;
    }

  // Set or reset the configuration bits in the requested pixels
  int xi, yi;
  unsigned int *pcfg;
  for( yi=ystart; yi<yend; ++yi )
    for( xi=xstart; xi<xend; ++xi )
      {
	pcfg = &_pixelConfig[yi][xi];
	*pcfg &= MPX3RX_CFG_MASKBIT; // Set/reset elsewhere
	*pcfg |= (discl << 1);
	*pcfg |= (disch << 6);
	if( testbit ) *pcfg |= MPX3RX_CFG_TESTBIT;
      }

  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::setPixelMaskMpx3rx( int x, int y, bool b )
{
  return this->setPixelBit( x, y, MPX3RX_CFG_MASKBIT, b );
}

// ----------------------------------------------------------------------------

bool SpidrController::setPixelConfigMpx3rx( int  dev_nr,
					    bool readback,
					    bool with_replies )
{
  // The SPIDR software will see to it that it gets done in 2 stages:
  // the first 128 rows, then the second 128 rows...
  // (due to hardware bug in Medipix3RX device)

  // Space for one row (256 pixels) of Medipix-formatted
  // pixel configuration data
  unsigned char pixelrow[(256*12)/8];

  unsigned int   *pconfig;
  int             row, column, pixelbit, pixelbitmask, bit;
  unsigned char  *prow;
  unsigned char   byte, bitmask;
  int             cmd = CMD_PIXCONF_MPX3RX;
  // ### Without replies doesn't work (yet) because -I think- TCP packets
  //     are concatenated at the receiver end and need to be interpreted
  //     to be investigated...
  //if( !with_replies ) cmd |= CMD_NOREPLY;

  // Convert the data in _pixelConfig row-by-row into the format
  // required by the Medipix device
  for( row=0; row<256; ++row )
    {
      // Next row
      pconfig = &_pixelConfig[row][0];
      // Refill pixelrow[]
      memset( static_cast<void *> (pixelrow), 0, sizeof(pixelrow) );
      // 12 bits of configuration data, starting from MSB
      prow = pixelrow;
      for( pixelbit=11; pixelbit>=0; --pixelbit )
	{
	  pixelbitmask = (1 << pixelbit);
	  for( column=0; column<256; column+=8 )
	    {
	      // Fill a byte
	      byte = 0;
	      bitmask = 0x01;
	      for( bit=0; bit<8; ++bit )
		{
		  if( pconfig[column + bit] & pixelbitmask )
		    byte |= bitmask;
		  bitmask <<= 1;
		}
	      *prow = byte;
	      ++prow;
	    }
	}

      // Even with 'with_replies' false, acknowledge first and last message
      /*
      if( row == 0 || row == 255 )
	cmd &= ~CMD_NOREPLY;
      else if( !with_replies )
	cmd |= CMD_NOREPLY;
      */

      // Trigger read-out to read back the pixel config uploaded just now
      if( row == 255 && readback ) row |= 0x10000;

      // Send this row of formatted configuration data to the SPIDR module
      if( this->requestSetIntAndBytes( cmd, dev_nr,
				       row, // Sequence number
				       sizeof( pixelrow ),
				       pixelrow ) == false )
	return false;
    }
  return true;
}

// ----------------------------------------------------------------------------

unsigned int *SpidrController::pixelConfig()
{
  return &_pixelConfig[0][0];
}

// ----------------------------------------------------------------------------
// Configuration: OMR
// ----------------------------------------------------------------------------

bool SpidrController::setContRdWr( int dev_nr, bool crw )
{
  int val = 0;
  if( crw ) val = 1;
  return this->requestSetInt( CMD_SET_CRW, dev_nr, val );
}

// ----------------------------------------------------------------------------

bool SpidrController::setPolarity( int dev_nr, bool polarity )
{
  int val = 0;
  if( polarity ) val = 1;
  return this->requestSetInt( CMD_SET_POLARITY, dev_nr, val );
}

// ----------------------------------------------------------------------------

bool SpidrController::setPs( int dev_nr, int ps )
{
  return this->requestSetInt( CMD_SET_PS, dev_nr, ps );
}

// ----------------------------------------------------------------------------

bool SpidrController::setDiscCsmSpm( int dev_nr, int disc )
{
  // For Medipix3RX only
  return this->requestSetInt( CMD_SET_DISCCSMSPM, dev_nr, disc );
}

// ----------------------------------------------------------------------------

bool SpidrController::setInternalTestPulse( int dev_nr, bool internal )
{
  int val = 0;
  if( internal ) val = 1;
  return this->requestSetInt( CMD_SET_INTERNALTP, dev_nr, val );
}

// ----------------------------------------------------------------------------

bool SpidrController::setPixelDepth( int dev_nr, int bits,
                                     bool two_counter_readout,
                                     bool two_counter_readout_softw )
{
  int pixelcounterdepth_id = 2; // 12-bit
  if( bits == 1 )
    pixelcounterdepth_id = 0;   // 1-bit
  else if( bits == 4 || bits == 6 )
    pixelcounterdepth_id = 1;   // 6-bit (MPX3RX) or 4-bit (MPX3)
  else if( bits == 24 )
    pixelcounterdepth_id = 3;   // 24-bit

  // Will both counters (per pixel) be read out ?
  if( two_counter_readout )       pixelcounterdepth_id |= 0x10000;
  if( two_counter_readout_softw ) pixelcounterdepth_id |= 0x20000;

  return this->requestSetInt( CMD_SET_COUNTERDEPTH, dev_nr,
			      pixelcounterdepth_id );
}

// ----------------------------------------------------------------------------

bool SpidrController::setEqThreshH( int dev_nr, bool equalize )
{
  // Matches 'Equalization' bit for Medipix3RX
  int val = 0;
  if( equalize ) val = 1;
  return this->requestSetInt( CMD_SET_EQTHRESHH, dev_nr, val );
}

// ----------------------------------------------------------------------------

bool SpidrController::setColourMode( int dev_nr, bool colour )
{
  int val = 0;
  if( colour ) val = 1;
  return this->requestSetInt( CMD_SET_COLOURMODE, dev_nr, val );
}

// ----------------------------------------------------------------------------

bool SpidrController::setCsmSpm( int dev_nr, int csm )
{
  // For Medipix3RX only
  return this->requestSetInt( CMD_SET_CSMSPM, dev_nr, csm );
}

// ----------------------------------------------------------------------------

bool SpidrController::setEnablePixelCom( int dev_nr, bool enable )
{
  // For Medipix3 only: corresponds to Medipix3RX CSM_SPM OMR bit
  int val = 0;
  if( enable ) val = 1;
  return this->requestSetInt( CMD_SET_CSMSPM, dev_nr, val );
}

// ----------------------------------------------------------------------------

bool SpidrController::setGainMode( int dev_nr, int mode )
{
  // For Medipix3RX only
  return this->requestSetInt( CMD_SET_GAINMODE, dev_nr, mode );
}

// ----------------------------------------------------------------------------

bool SpidrController::setSenseDac( int dev_nr, int dac_code )
{
  return this->requestSetInt( CMD_SET_SENSEDAC, dev_nr, dac_code );
}

// ----------------------------------------------------------------------------

bool SpidrController::setExtDac( int dev_nr, int dac_code, int dac_val )
{
  // Combine dac_code and dac_val into a single int
  // (the DAC to set is the SPIDR-MPX3 DAC)
  int dac_data = ((dac_code & 0xFFFF) << 16) | (dac_val & 0xFFFF);
  return this->requestSetInt( CMD_SET_EXTDAC, dev_nr, dac_data );
}

// ----------------------------------------------------------------------------

bool SpidrController::getOmr( int dev_nr, unsigned char *omr )
{
  return this->requestGetBytes( CMD_GET_OMR, dev_nr, 48/8, omr );
}

// ----------------------------------------------------------------------------
// Configuration: non-volatile storage
// ----------------------------------------------------------------------------

bool SpidrController::storeAddrAndPorts( int ipaddr_src,
					 int ipport )
{
  // Store SPIDR controller and devices addresses and ports
  // to onboard non-volatile memory; at the same time changes
  // the controller's IP-address and/or port if the corresponding
  // parameter values are unequal to zero, but these values become
  // current only *after* the next hard reset or power-up
  int datawords[2];
  datawords[0] = ipaddr_src;
  datawords[1] = ipport;
  return this->requestSetInts( CMD_STORE_ADDRPORTS, 0, 2, datawords );
}

// ----------------------------------------------------------------------------

bool SpidrController::eraseAddrAndPorts()
{
  return this->requestSetInt( CMD_ERASE_ADDRPORTS, 0, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::validAddrAndPorts( bool *valid )
{
  int result = 0;
  *valid = false;
  if( this->requestGetInt( CMD_VALID_ADDRPORTS, 0, &result ) )
    {
      if( result ) *valid = true;
      return true;
    }
  return false;
}

// ----------------------------------------------------------------------------

bool SpidrController::storeDacs( int dev_nr )
{
  return this->requestSetInt( CMD_STORE_DACS, dev_nr, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::eraseDacs( int dev_nr )
{
  return this->requestSetInt( CMD_ERASE_DACS, dev_nr, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::validDacs( int dev_nr, bool *valid )
{
  int result = 0;
  *valid = false;
  if( this->requestGetInt( CMD_VALID_DACS, dev_nr, &result ) )
    {
      if( result ) *valid = true;
      return true;
    }
  return false;
}

// ----------------------------------------------------------------------------

bool SpidrController::readFlash( int            flash_id,
				 int            address,
				 int           *nbytes,
				 unsigned char *databytes )
{
  int addr = (address & 0x00FFFFFF) | (flash_id << 24);
  *nbytes = 0;
  if( !this->requestGetIntAndBytes( CMD_READ_FLASH, 0,
				    &addr, 1024, databytes ) )
    return false;

  // Returned address should match
  if( addr != ((address & 0x00FFFFFF) | (flash_id << 24)) ) return false;

  *nbytes = 1024;
  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::writeFlash( int            flash_id,
				  int            address,
				  int            nbytes,
				  unsigned char *databytes )
{
  int addr = (address & 0x00FFFFFF) | (flash_id << 24);
  return this->requestSetIntAndBytes( CMD_WRITE_FLASH, 0,
				      addr, nbytes, databytes );
}

// ----------------------------------------------------------------------------
// Shutter Trigger
// ----------------------------------------------------------------------------

bool SpidrController::setShutterTriggerConfig( int trigger_mode,
					       int trigger_width_us,
					       int trigger_freq_mhz,
					       int nr_of_triggers,
					       int trigger_pulse_count )
{
  int datawords[5];
  datawords[0] = trigger_mode;
  datawords[1] = trigger_width_us;
  datawords[2] = trigger_freq_mhz;
  datawords[3] = nr_of_triggers;
  datawords[4] = trigger_pulse_count;
  return this->requestSetInts( CMD_SET_TRIGCONFIG, 0, 5, datawords );
}

// ----------------------------------------------------------------------------

bool SpidrController::getShutterTriggerConfig( int *trigger_mode,
					       int *trigger_width_us,
					       int *trigger_freq_mhz,
					       int *nr_of_triggers,
					       int *trigger_pulse_count )
{
  int data[5];
  if( !this->requestGetInts( CMD_GET_TRIGCONFIG, 0, 5, data ) )
    return false;
  *trigger_mode        = data[0];
  *trigger_width_us    = data[1];
  *trigger_freq_mhz    = data[2];
  *nr_of_triggers      = data[3];
  if( trigger_pulse_count )
    *trigger_pulse_count = data[4];
  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::startAutoTrigger()
{
  return this->requestSetInt( CMD_AUTOTRIG_START, 0, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::stopAutoTrigger()
{
  return this->requestSetInt( CMD_AUTOTRIG_STOP, 0, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::openShutter()
{
  // It is sufficient to set the trigger period to zero (June 2014)
  if( !this->setShutterTriggerConfig( SHUTTERMODE_AUTO, 0, 10000, 1 ) )
    return false;
  return this->startAutoTrigger();
}

// ----------------------------------------------------------------------------

bool SpidrController::closeShutter()
{
  return this->stopAutoTrigger();
}

// ----------------------------------------------------------------------------

bool SpidrController::triggerSingleReadout( int countl_or_h )
{
  return this->requestSetInt( CMD_TRIGGER_READOUT, 0, countl_or_h );
}

// ----------------------------------------------------------------------------

bool SpidrController::startContReadout( int freq_hz )
{
  if( !this->setReady() )
    return false;

  for( int devnr=0; devnr<4; ++devnr )
    this->setContRdWr( devnr, true );

  // Configure read-out frequency (period)
  int period_25ns;
  if( freq_hz < 1 ) freq_hz = 1;
  period_25ns = 40000000/freq_hz;
  if( !this->setSpidrReg( SPIDRMPX3_SHUTTER1_PERIOD_I, period_25ns ) )
    return false;

  if( !this->setSpidrRegBit( SPIDR_SHUTTERTRIG_CTRL_I,
			     SPIDR_ENA_SHUTTER1_CNTRSEL_BIT, true ) )
    return false;

  // Open shutter
  if( !this->openShutter() )
    return false;

  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::stopContReadout()
{
  for( int devnr=0; devnr<4; ++devnr )
    this->setContRdWr( devnr, false );

  this->setSpidrRegBit( SPIDR_SHUTTERTRIG_CTRL_I,
			SPIDR_ENA_SHUTTER1_CNTRSEL_BIT, false );
  return this->closeShutter();
}

// ----------------------------------------------------------------------------

bool SpidrController::getExtShutterCounter( int *cntr )
{
  return this->getSpidrReg( SPIDR_EXT_SHUTTER_CNTR_I, cntr );
}

// ----------------------------------------------------------------------------

bool SpidrController::getShutterCounter( int *cntr )
{
  return this->getSpidrReg( SPIDR_SHUTTER_CNTR_I, cntr );
}

// ----------------------------------------------------------------------------

bool SpidrController::getShutterInhibitCounter( int *cntr )
{
  return this->getSpidrReg( SPIDRMPX3_SHUTTER_INH_CNTR_I, cntr );
}

// ----------------------------------------------------------------------------

bool SpidrController::resetCounters()
{
  return this->requestSetInt( CMD_RESET_COUNTERS, 0, 0 );
}

// ----------------------------------------------------------------------------
// Monitoring
// ----------------------------------------------------------------------------

bool SpidrController::getAdc( int *adc_val, int chan, int nr_of_samples )
{
  // Get the sum of a number of ADC samples of the selected SPIDR ADC channel
  *adc_val = (chan & 0xFFFF) | ((nr_of_samples & 0xFFFF) << 16);
  return this->requestGetInt( CMD_GET_SPIDR_ADC, 0, adc_val );
}

// ----------------------------------------------------------------------------

bool SpidrController::getDacOut( int  dev_nr,
				 int *dacout_val,
				 int  nr_of_samples )
{
  // Get (an) ADC sample(s) of a Medipix3 device's 'DACOut' output
  int chan = dev_nr; // Assume this is how they are connected to the ADC
  return this->getAdc( dacout_val, chan, nr_of_samples );
}

// ----------------------------------------------------------------------------

bool SpidrController::getRemoteTemp( int *mdegrees )
{
  return this->requestGetInt( CMD_GET_REMOTETEMP, 0, mdegrees );
}

// ----------------------------------------------------------------------------

bool SpidrController::getLocalTemp( int *mdegrees )
{
  return this->requestGetInt( CMD_GET_LOCALTEMP, 0, mdegrees );
}

// ----------------------------------------------------------------------------

bool SpidrController::getFpgaTemp( int *mdegrees )
{
  return this->requestGetInt( CMD_GET_FPGATEMP, 0, mdegrees );
}

// ----------------------------------------------------------------------------

bool SpidrController::getAvdd( int *mvolt, int *mamp, int *mwatt )
{
  return this->get3Ints( CMD_GET_AVDD, mvolt, mamp, mwatt );
}

// ----------------------------------------------------------------------------

bool SpidrController::getDvdd( int *mvolt, int *mamp, int *mwatt )
{
  return this->get3Ints( CMD_GET_DVDD, mvolt, mamp, mwatt );
}

// ----------------------------------------------------------------------------

bool SpidrController::getVdd( int *mvolt, int *mamp, int *mwatt )
{
  return this->get3Ints( CMD_GET_VDD, mvolt, mamp, mwatt );
}

// ----------------------------------------------------------------------------

bool SpidrController::getAvddNow( int *mvolt, int *mamp, int *mwatt )
{
  return this->get3Ints( CMD_GET_AVDD_NOW, mvolt, mamp, mwatt );
}

// ----------------------------------------------------------------------------

bool SpidrController::getDvddNow( int *mvolt, int *mamp, int *mwatt )
{
  return this->get3Ints( CMD_GET_DVDD_NOW, mvolt, mamp, mwatt );
}

// ----------------------------------------------------------------------------

bool SpidrController::getVddNow( int *mvolt, int *mamp, int *mwatt )
{
  return this->get3Ints( CMD_GET_VDD_NOW, mvolt, mamp, mwatt );
}

// ----------------------------------------------------------------------------

bool SpidrController::getBiasVoltage( int *volts )
{
  int chan = 4; // SPIDR-MPX3Q ADC input
  int adc_data = chan;
  if( this->requestGetInt( CMD_GET_SPIDR_ADC, 0, &adc_data ) )
    {
      // Full-scale is 1.5V = 1500mV
      // and 0.01V represents approximately 1V bias voltage
      *volts = (((adc_data & 0xFFF)*1500 + 4095) / 4096) / 10;
      return true;
    }
  return false;
}

// ----------------------------------------------------------------------------

bool SpidrController::getFanSpeed( int index, int *rpm )
{
  // Index indicates fan speed to return (chipboard or SPIDR resp.)
  *rpm = index;
  return this->requestGetInt( CMD_GET_FANSPEED, 0, rpm );
}

// ----------------------------------------------------------------------------

bool SpidrController::setFanSpeed( int index, int percentage )
{
  // Index indicates fan speed to set (chipboard or SPIDR resp.)
  return this->requestSetInt( CMD_SET_FANSPEED, 0, (index << 16) | percentage );
}

// ----------------------------------------------------------------------------

bool SpidrController::getHumidity( int *percentage )
{
  return this->requestGetInt( CMD_GET_HUMIDITY, 0, percentage );
}

// ----------------------------------------------------------------------------

bool SpidrController::getPressure( int *mbar )
{
  return this->requestGetInt( CMD_GET_PRESSURE, 0, mbar );
}

// ----------------------------------------------------------------------------
// Other
// ----------------------------------------------------------------------------

bool SpidrController::getSpidrReg( int addr, int *val )
{
  int data[2];
  data[0] = addr;
  if( !this->requestGetInts( CMD_GET_SPIDRREG, 0, 2, data ) )
    return false;
  if( data[0] != addr )
    return false;
  *val = data[1];
  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::setSpidrReg( int addr, int val, bool verify )
{
  int data[2];
  data[0] = addr;
  data[1] = val;
  if( this->requestSetInts( CMD_SET_SPIDRREG, 0, 2, data ) )
    {
      if( verify )
	{
	  int readval;
	  if( !this->getSpidrReg( addr, &readval ) )
	    return false;
	  if( readval != val )
	    {
	      this->clearErrorString();
	      _errString << "Verify failed: wrote " << hex
			 << val << ", read " << readval << dec;
	      return false;
	    }
	}
      return true;
    }
  return false;
}

// ----------------------------------------------------------------------------

 bool SpidrController::setSpidrRegBit( int addr, int bitnr, bool set,
				       bool verify )
{
  if( bitnr < 0 || bitnr > 31 ) return false;
  int reg;
  if( !this->getSpidrReg( addr, &reg ) ) return false;
  // Set or reset bit 'bitnr' of the register...
  if( set )
    reg |= (1 << bitnr);
  else
    reg &= ~(1 << bitnr);
  return this->setSpidrReg( addr, reg, verify );
}

// ----------------------------------------------------------------------------

std::string SpidrController::dacNameMpx3( int dac_code )
{
  int index = this->dacIndexMpx3( dac_code );
  if( index < 0 ) return string( "????" ); 
  return string( MPX3_DAC_TABLE[index].name );
}

// ----------------------------------------------------------------------------

std::string SpidrController::dacNameMpx3rx( int dac_code )
{
  int index = this->dacIndexMpx3rx( dac_code );
  if( index < 0 ) return string( "????" ); 
  return string( MPX3RX_DAC_TABLE[index].name );
}

// ----------------------------------------------------------------------------

int SpidrController::dacMaxMpx3( int dac_code )
{
  int index = this->dacIndexMpx3( dac_code );
  if( index < 0 ) return 0;
  return( (1 << MPX3_DAC_TABLE[index].bits) - 1 );
}

// ----------------------------------------------------------------------------

int SpidrController::dacMaxMpx3rx( int dac_code )
{
  int index = this->dacIndexMpx3rx( dac_code );
  if( index < 0 || index >= MPX3RX_DAC_COUNT ) return 0;
  return( (1 << MPX3RX_DAC_TABLE[index].bits) - 1 );
}

// ----------------------------------------------------------------------------
// Private functions
// ----------------------------------------------------------------------------

bool SpidrController::loadOmr( int dev_nr )
{
  // Load the actual OMR with the onboard data structure
  return this->requestSetInt( CMD_WRITE_OMR, dev_nr, 0 );
}

// ----------------------------------------------------------------------------

bool SpidrController::setPixelBit( int x, int y, unsigned int bitmask, bool b )
{
  int xstart, xend;
  int ystart, yend;
  if( !this->validXandY( x, y, &xstart, &xend, &ystart, &yend ) )
    return false;

  // Set or unset the bit(s) in the requested pixels
  int xi, yi;
  if( b )
    {
      for( yi=ystart; yi<yend; ++yi )
	for( xi=xstart; xi<xend; ++xi )
	  _pixelConfig[yi][xi] |= bitmask;
    }
  else
    {
      for( yi=ystart; yi<yend; ++yi )
	for( xi=xstart; xi<xend; ++xi )
	  _pixelConfig[yi][xi] &= ~bitmask;
    }

  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::get3Ints( int cmd, int *data0, int *data1, int *data2 )
{
  int data[3];
  int dummy = 0;
  if( !this->requestGetInts( cmd, dummy, 3, data ) ) return false;
  *data0 = data[0];
  *data1 = data[1];
  *data2 = data[2];
  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::validXandY( int x,       int y,
				  int *xstart, int *xend,
				  int *ystart, int *yend )
{
  if( x == ALL_PIXELS )
    {
      *xstart = 0;
      *xend   = 256;
    }
  else
    {
      if( x >= 0 && x <= 255 )
	{
	  *xstart = x;
	  *xend   = x+1;
	}
      else
	{
	  this->clearErrorString();
	  _errString << "Invalid x coordinate: " << x;
	  return false;
	}
    }
  if( y == ALL_PIXELS )
    {
      *ystart = 0;
      *yend   = 256;
    }
  else
    {
      if( y >= 0 && y <= 255 )
	{
	  *ystart = y;
	  *yend   = y+1;
	}
      else
	{
	  this->clearErrorString();
	  _errString << "Invalid y coordinate: " << y;
	  return false;
	}
    }
  return true;
}

// ----------------------------------------------------------------------------

bool SpidrController::requestGetInt( int cmd, int dev_nr, int *dataword )
{
  int req_len = (4+1)*4;
  _reqMsg[4] = htonl( *dataword ); // May contain an additional parameter!
  int expected_len = (4+1)*4;
  if( this->request( cmd, dev_nr, req_len, expected_len ) )
    {
      *dataword = ntohl( _replyMsg[4] );
      return true;
    }
  else
    {
      *dataword = 0;
    }
  return false;
}

// ----------------------------------------------------------------------------

bool SpidrController::requestGetInts( int cmd, int dev_nr,
				      int expected_ints, int *datawords )
{
  int req_len = (4+1)*4;
  _reqMsg[4] = htonl( *datawords ); // May contain an additional parameter!
  int expected_len = (4 + expected_ints) * 4;
  if( this->request( cmd, dev_nr, req_len, expected_len ) )
    {
      int i;
      for( i=0; i<expected_ints; ++i )
	datawords[i] = ntohl( _replyMsg[4+i] );
      return true;
    }
  else
    {
      int i;
      for( i=0; i<expected_ints; ++i ) datawords[i] = 0;
    }
  return false;
}

// ----------------------------------------------------------------------------

bool SpidrController::requestGetBytes( int cmd, int dev_nr,
				       int expected_bytes,
				       unsigned char *databytes )
{
  int req_len = (4+1)*4;
  _reqMsg[4] = 0;
  int expected_len = (4*4) + expected_bytes;
  if( this->request( cmd, dev_nr, req_len, expected_len ) )
    {
      memcpy( static_cast<void *> (databytes),
	      static_cast<void *> (&_replyMsg[4]), expected_bytes );
      return true;
    }
  else
    {
      int i;
      for( i=0; i<expected_bytes; ++i ) databytes[i] = 0;
    }
  return false;
}

// ----------------------------------------------------------------------------

bool SpidrController::requestGetIntAndBytes( int cmd, int dev_nr,
					     int *dataword,
					     int expected_bytes,
					     unsigned char *databytes )
{
  // Send a message with 1 dataword, expect a reply with a dataword
  // and a number of bytes
  int req_len = (4+1)*4;
  _reqMsg[4] = htonl( *dataword ); // May contain an additional parameter!
  int expected_len = (4+1)*4 + expected_bytes;
  if( this->request( cmd, dev_nr, req_len, expected_len ) )
    {
      *dataword = ntohl( _replyMsg[4] );
      memcpy( static_cast<void *> (databytes),
	      static_cast<void *> (&_replyMsg[5]), expected_bytes );
      return true;
    }
  else
    {
      int i;
      for( i=0; i<expected_bytes; ++i ) databytes[i] = 0;
    }
  return false;
}

// ----------------------------------------------------------------------------

bool SpidrController::requestSetInt( int cmd, int dev_nr, int dataword )
{
  int req_len = (4+1)*4;
  _reqMsg[4] = htonl( dataword );
  int expected_len = (4+1)*4;
  return this->request( cmd, dev_nr, req_len, expected_len );
}

// ----------------------------------------------------------------------------

bool SpidrController::requestSetInts( int cmd, int dev_nr,
				      int nwords, int *datawords )
{
  int req_len = (4 + nwords)*4;
  for( int i=0; i<nwords; ++i )
    _reqMsg[4+i] = htonl( datawords[i] );
  int expected_len = (4+1)*4;
  return this->request( cmd, dev_nr, req_len, expected_len );
}

// ----------------------------------------------------------------------------

bool SpidrController::requestSetIntAndBytes( int cmd, int dev_nr,
					     int dataword,
					     int nbytes,
					     unsigned char *bytes )
{
  int req_len = (4+1)*4 + nbytes;
  _reqMsg[4] = htonl( dataword );
  memcpy( static_cast<void *> (&_reqMsg[5]),
	  static_cast<void *> (bytes), nbytes );
  int expected_len = (4+1)*4;
  return this->request( cmd, dev_nr, req_len, expected_len );
}

// ----------------------------------------------------------------------------

bool SpidrController::request( int cmd,     int dev_nr,
			       int req_len, int exp_reply_len )
{
  _reqMsg[0] = htonl( cmd );
  _reqMsg[1] = htonl( req_len );
  _reqMsg[2] = 0; // Dummy for now; reply uses it to return an error status
  _reqMsg[3] = htonl( dev_nr );

  _sock->write( (const char *) _reqMsg, req_len );
  if( !_sock->waitForBytesWritten( 400 ) )
    {
      this->clearErrorString();
      _errString << "Time-out sending command";
      return false;
    }

  // Reply expected ?
  if( cmd & CMD_NOREPLY ) return true;

  if( !_sock->waitForReadyRead( 1000 ) )
    {
      this->clearErrorString();
      _errString << "Time-out receiving reply";
      return false;
    }

  int reply_len = _sock->read( (char *) _replyMsg, sizeof(_replyMsg) );
  if( reply_len < 0 )
    {
      this->clearErrorString();
      _errString << "Failed to read reply";
      return false;
    }

  // Various checks on the received reply
  if( reply_len < exp_reply_len )
    {
      this->clearErrorString();
      _errString << "Unexpected reply length, got "
		 << reply_len << ", expected at least " << exp_reply_len;
      return false;
    }
  int err = ntohl( _replyMsg[2] ); // (Check 'err' before 'reply')
  _errId = err;
  if( err != 0 )
    {
      this->clearErrorString();
      _errString << "Error from SPIDR: " << this->spidrErrString( err )
		 << " (0x" << hex << err << ")";
      return false;
    }
  int reply = ntohl( _replyMsg[0] );
  if( reply != (cmd | CMD_REPLY) )
    {
      this->clearErrorString();
      _errString << "Unexpected reply: 0x" << hex << reply;
      return false;
    }
  if( ntohl( _replyMsg[3] ) != (unsigned int) dev_nr )
    {
      this->clearErrorString();
      _errString << "Unexpected device number in reply: " << _replyMsg[3];
      return false;
    }
  return true;
}

// ----------------------------------------------------------------------------

static const char *MPX3_ERR_STR[] =
  {
    "no error"
    "MPX3_ERR_FLUSHING",
    "MPX3_ERR_EMPTY",
    "MPX3_ERR_UNEXP_SIZE"
  };

static const char *SPIDR_ERR_STR[] =
  {
    "SPIDR_ERR_I2C_INIT",
    "SPIDR_ERR_I2C",
    "<unknown>",
    "<unknown>",
    "MON_ERR_MAX6663_INIT",
    "MON_ERR_INA219_0_INIT",
    "MON_ERR_INA219_1_INIT",
    "MON_ERR_INA219_2_INIT"
  };

static const char *STORE_ERR_STR[] =
  {
    "no error",
    "STORE_ERR_MPX",
    "STORE_ERR_WRITE",
    "STORE_ERR_WRITE_CHECK",
    "STORE_ERR_READ",
    "STORE_ERR_UNMATCHED_ID",
    "STORE_ERR_NOFLASH"
  };

static const char *MONITOR_ERR_STR[] =
  {
    "MON_ERR_TEMP_DAQ",
    "MON_ERR_POWER_DAQ",
  };

std::string SpidrController::spidrErrString( int err )
{
  std::string errstr;
  unsigned int errid = err & 0xFF;
  
  if( errid >= (sizeof(ERR_STR)/sizeof(char*)) )
    errstr = "<unknown>";
  else
    errstr = ERR_STR[errid];

  if( errid == ERR_MPX3_HARDW )
    {
      errid = (err & 0xFF00) >> 8;
      errstr += ", ";
      // Error identifier is a number
      if( errid >= (sizeof(MPX3_ERR_STR)/sizeof(char*)) )
	errstr += "<unknown>";
      else
	errstr += MPX3_ERR_STR[errid];
    }
  else if( errid == ERR_MON_HARDW )
    {
      errid = (err & 0xFF00) >> 8;
      errstr += ", ";
      // Error identifier is a bitmask
      for( int bit=0; bit<8; ++bit )
	if( errid & (1<<bit) )
	  {
	    errstr += SPIDR_ERR_STR[bit];
	    errstr += " ";
	  }
    }
  else if( errid == ERR_FLASH_STORAGE )
    {
      errid = (err & 0xFF00) >> 8;
      errstr += ", ";
      // Error identifier is a number
      if( errid >= (sizeof(STORE_ERR_STR)/sizeof(char*)) )
	errstr += "<unknown>";
      else
	errstr += STORE_ERR_STR[errid];
    }

  return errstr;
}

// ----------------------------------------------------------------------------

int SpidrController::dacIndexMpx3( int dac_code )
{
  int i;
  for( i=0; i<MPX3_DAC_COUNT; ++i )
    if( MPX3_DAC_TABLE[i].code == dac_code ) return i;
  return -1;
}

// ----------------------------------------------------------------------------

int SpidrController::dacIndexMpx3rx( int dac_code )
{
  int i;
  for( i=0; i<MPX3RX_DAC_COUNT; ++i )
    if( MPX3RX_DAC_TABLE[i].code == dac_code ) return i;
  return -1;
}

// ----------------------------------------------------------------------------
