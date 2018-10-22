#include <QUdpSocket>
#include <QAbstractSocket>

#include "ReceiverThreadC.h"
#include "mpx3defs.h"

#define ALLOW_BIGENDIAN_OPTION

#define byteswap(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))

// ----------------------------------------------------------------------------

ReceiverThreadC::ReceiverThreadC( int *ipaddr,
				int  port,
				QObject *parent )
  : _rowCnt( 0 ),
    _rowPixels( MPX_PIXEL_COLUMNS ),
    _framePtr( (u64 *)_frameBuffer[0] ),
    _shutterCnt( 1 ),
    _bigEndian( false ),
    _infoIndex( 0 ),
    _pixelsReceived( 0 ),
    _pixelsLost( 0 ),
    ReceiverThread( ipaddr, port, parent )
{
  for( u32 i=0; i<NR_OF_FRAMEBUFS; ++i )
    {
      _frameSize[i]  = 0;
    }

  for( u32 i=0; i<NR_OF_FRAMEBUFS; ++i )
    _pixelsLostFrame[i] = 8888;
  _pixelsLostFrame[_head] = 0;

  // Maintain a 'shutter counter' in the 'SPIDR header'
  // (not produced by the Compact-SPIDR hardware)
  u16 *header = (u16 *) _headerBuffer[_head];
  header[1] = byteswap( _shutterCnt );
}

// ----------------------------------------------------------------------------

ReceiverThreadC::~ReceiverThreadC()
{
}

// ----------------------------------------------------------------------------

void ReceiverThreadC::readDatagrams()
{
  int  i, recvd_sz;
  bool copy;
  u64 *pixelpkt, pixelword;
  u64  type;
  int  pix_per_word = 60/_pixelDepth;

#ifndef USE_NATIVE_SOCKET
  while( _sock->hasPendingDatagrams() )
    {
      recvd_sz = _sock->readDatagram( _recvBuffer, RECV_BUF_SIZE );
      if( recvd_sz <= 0 ) continue;
#else
    {
      recvd_sz = _sock->read( _recvBuffer, RECV_BUF_SIZE );
      if( recvd_sz <= 0 ) return;
#endif // USE_NATIVE_SOCKET
      ++_packetsReceived;

      // Parse the data in the received UDP packet
      pixelpkt = (u64 *) _recvBuffer;
      for( i=0; i<recvd_sz/sizeof(u64); ++i, ++pixelpkt )
	{
#ifdef ALLOW_BIGENDIAN_OPTION
	  if( _bigEndian )
	    {
	      // Reverse the byte order
	      char *bytes = (char *) &pixelword;
	      char *p     = (char *) pixelpkt;
	      for( int i=0; i<8; ++i )
		bytes[i] = p[7-i];
	      *pixelpkt = pixelword;
	    }
#endif // ALLOW_BIGENDIAN_OPTION

	  type = (*pixelpkt) & PKT_TYPE_MASK;
	  copy = true;

	  switch( type )
	    {
	    case PIXEL_DATA_SOR:
	      if( _rowPixels < MPX_PIXEL_COLUMNS )
		{
		  // Lost some pixels of the previous row?
		  _pixelsLost             += MPX_PIXEL_COLUMNS - _rowPixels;
		  _pixelsLostFrame[_head] += MPX_PIXEL_COLUMNS - _rowPixels;
		}
	      // Assume SOF may come in a next packet (so out of order),
	      // so outcommented the following if-statement
	      // (seen 2 Feb 2017, Compact-SPIDR with MPX3 quadboard:
	      //  SPIDR: Firmware 0019A6 (Date 1608231422), Software 1607200A)
	      //if( _rowCnt == 0 )
	      //  ++_rowCnt; // Must've missed SOF
	      ++_rowCnt;
	      if( _rowCnt > MPX_PIXEL_ROWS )
		{
		  // Can't be correct.. what to do?
		  // Start filling a next frame
		  this->nextFrame();
		}
	      _rowPixels = 0;
	      _rowPixels += pix_per_word;
	      break;

	    case PIXEL_DATA_EOR:
	      _rowPixels += MPX_PIXEL_COLUMNS - (MPX_PIXEL_COLUMNS/pix_per_word)*pix_per_word;
	      if( _rowPixels < MPX_PIXEL_COLUMNS )
		{
		  // Lost some pixels of this row?
		  _pixelsLost             += MPX_PIXEL_COLUMNS - _rowPixels;
		  _pixelsLostFrame[_head] += MPX_PIXEL_COLUMNS - _rowPixels;
		  // Don't count them again at next SOR
		  _rowPixels = MPX_PIXEL_COLUMNS;
		}
	      if( _rowCnt >= MPX_PIXEL_ROWS )
		{
		  // Can't be correct.. what to do?
		  _rowPixels = 0;
		  _rowPixels += pix_per_word;

		  // Start filling a next frame
		  this->nextFrame();
		}
	      break;

	    case PIXEL_DATA_SOF:
	      if( _rowPixels < MPX_PIXEL_COLUMNS )
		{
		  // Lost some pixels ?
		  _pixelsLost             += MPX_PIXEL_COLUMNS - _rowPixels;
		  _pixelsLostFrame[_head] += MPX_PIXEL_COLUMNS - _rowPixels;
		}
	      ++_rowCnt;
	      _rowPixels = 0;
	      _rowPixels += pix_per_word;
	      break;

	    case PIXEL_DATA_EOF:
	      _rowPixels += pix_per_word;
	      if( _rowPixels < MPX_PIXEL_COLUMNS )
		{
		  // Lost some pixels ?
		  _pixelsLost             += MPX_PIXEL_COLUMNS - _rowPixels;
		  _pixelsLostFrame[_head] += MPX_PIXEL_COLUMNS - _rowPixels;
		  // Don't count them again at next SOF/SOR
		  _rowPixels = MPX_PIXEL_COLUMNS;
		}
	      break;

	    case PIXEL_DATA_MID:
	      _rowPixels += pix_per_word;
	      if( _rowPixels >= MPX_PIXEL_COLUMNS )
		{
		  // Unexpected: did we miss a SOR ?
		  ++_rowCnt;
		  if( _rowCnt > MPX_PIXEL_ROWS )
		    {
		      // Can't be correct.. what to do?
		      // Start filling a next frame
		      this->nextFrame();
		    }
		  _rowPixels = 0;
		  _rowPixels += pix_per_word;
		}
	      break;

	    case INFO_HEADER_SOF:
	    case INFO_HEADER_MID:
	    case INFO_HEADER_EOF:
	      {
		char *p = (char *) pixelpkt;
		if( type == INFO_HEADER_SOF )
		  _infoIndex = 0;
		if( _infoIndex <= 256/8-4 )
		  for( int i=0; i<4; ++i, ++_infoIndex )
		    _infoHeader[_infoIndex] = p[i];
		if( type == INFO_HEADER_EOF )
		  {
		    // Format and interpret:
		    // e.g. OMR has to be mirrored per byte;
		    // in order to distinguish CounterL from CounterH readout
		    // it's sufficient to look at the last byte, containing
		    // the three operation mode bits
		    char byt = _infoHeader[_infoIndex-1];
		    // Mirroring of byte with bits 'abcdefgh':
		    byt = ((byt & 0xF0)>>4) | ((byt & 0x0F)<<4);// efghabcd
		    byt = ((byt & 0xCC)>>2) | ((byt & 0x33)<<2);// ghefcdab
		    byt = ((byt & 0xAA)>>1) | ((byt & 0x55)<<1);// hgfedcba
		    if( (byt & 0x7) == 0x4 ) // 'Read CounterH'
		      _isCounterhFrame[_head] = true;
		    else
		      _isCounterhFrame[_head] = false;
		    // Mirror all bytes...
		    //for( int i=0; i<256/8; ++i )
		    //  {
			//char byt = _infoHeader[i];
			// Mirroring of byte with bits 'abcdefgh':
			//byt = ((byt & 0xF0)>>4) | ((byt & 0x0F)<<4);// efghabcd
			//byt = ((byt & 0xCC)>>2) | ((byt & 0x33)<<2);// ghefcdab
			//byt = ((byt & 0xAA)>>1) | ((byt & 0x55)<<1);// hgfedcba
			//_infoHeader[i] = byt;
		    //  }
		  }
	      }
          // KEEP this packet
          copy = true;
	      break;

	    default:
	      // Skip this packet
	      copy = false;
	      break;
	    }

	  if( copy )
	    {
	      *_framePtr = *pixelpkt;
	      ++_framePtr;
	      _frameSize[_head] += 8;
	      // Don't count 'padding' (empty/unused) pixel data
	      if( _rowPixels > MPX_PIXEL_COLUMNS )
		_pixelsReceived += (pix_per_word -
				    (_rowPixels-MPX_PIXEL_COLUMNS));
	      else
		_pixelsReceived += pix_per_word;

	      if( type == PIXEL_DATA_EOF )
		// Start filling a next frame
		this->nextFrame();
	    }
	}
    }
}

// ----------------------------------------------------------------------------

void ReceiverThreadC::nextFrame()
{
  // Count our losses, if any...
  if( _rowCnt < MPX_PIXEL_ROWS )
    {
      // Lost whole pixel rows ?
      _pixelsLost += (MPX_PIXEL_ROWS-_rowCnt) * MPX_PIXEL_COLUMNS;
      _pixelsLostFrame[_head] +=
	(MPX_PIXEL_ROWS-_rowCnt) * MPX_PIXEL_COLUMNS;
      _rowCnt = MPX_PIXEL_ROWS;
    }

  // Start filling next frame
  ++_framesReceived;
  this->nextFrameBuffer();

  // Additional initializations
  _framePtr               = (u64 *) _frameBuffer[_head];
  _frameSize[_head]       = 0;
  _rowCnt                 = 0;
  _pixelsLostFrame[_head] = 0;

  // Maintain a 'shutter counter' in the 'SPIDR header'
  // (not produced by the Compact-SPIDR hardware)
  ++_shutterCnt;
  u16 *header = (u16 *) _headerBuffer[_head];
  header[1] = byteswap( _shutterCnt );
}

// ----------------------------------------------------------------------------
