/* ----------------------------------------------------------------------------
File   : mpx3defs.h

Descr  : Definitions for the Medipix3.1 and Medipix3RX devices.

History:
21JUL2013; HenkB; Created.
---------------------------------------------------------------------------- */
#ifndef MPX3DEFS_H_
#define MPX3DEFS_H_

// Device types:
//   0: NC ('Not Connected')
//   1: Medipix3.1
//   2: Medipix3RX
#define MPX_TYPE_NC             0
#define MPX_TYPE_MPX31          1
#define MPX_TYPE_MPX3RX         2
// Number of defined types
#define MPX_TYPES               3

#define MPX_PIXELS             (256*256)
#define MPX_PIXEL_ROWS          256
#define MPX_PIXEL_COLUMNS       256

// ----------------------------------------------------------------------------

// ID register size in bytes
#define MPX3_ID_SIZE           (256/8)

// DACs register size in bytes
#define MPX_DACS_SIZE          (256/8)

//#define MPX3_OMR_PRESYNC      0x000C // Not swapped
#define MPX3_OMR_PRESYNC        0x0C00
//#define MPX3_OP_PRESYNC       0x0005 // Not swapped
#define MPX3_OP_PRESYNC         0x0500
#define MPX3_LOAD_PRESYNC       0x0A00
#define MPX3_POSTSYNC           0x0000

// MPX3 pixel configuration bits in Counter0: B0 to B11
#define MPX3_CFG_CONFIGTHB_4   (1<<4)
#define MPX3_CFG_CONFIGTHA_1   (1<<5)
#define MPX3_CFG_CONFIGTHA_2   (1<<6)
#define MPX3_CFG_CONFIGTHA_4   (1<<7)
#define MPX3_CFG_GAINMODE      (1<<8)

// MPX3 pixel configuration bits in Counter1: B12 to B23
#define MPX3_CFG_TESTBIT       (1<<(15-12))
#define MPX3_CFG_CONFIGTHB_3   (1<<(16-12))
#define MPX3_CFG_CONFIGTHA_0   (1<<(17-12))
#define MPX3_CFG_CONFIGTHB_0   (1<<(18-12))
#define MPX3_CFG_CONFIGTHA_3   (1<<(19-12))
#define MPX3_CFG_CONFIGTHB_2   (1<<(20-12))
#define MPX3_CFG_MASKBIT       (1<<(21-12))
#define MPX3_CFG_CONFIGTHB_1   (1<<(22-12))

// MPX3-RX pixel configuration bits are all in Counter1: B0 to B11
#define MPX3RX_CFG_MASKBIT      0x001
#define MPX3RX_CFG_CFGDISC_L    0x03E
#define MPX3RX_CFG_CFGDISC_H    0x7C0
#define MPX3RX_CFG_TESTBIT      0x800

// ----------------------------------------------------------------------------
// Medipix3 DACs

// Number of DACs in various devices
#define MPX3_DAC_COUNT          25
#define MPX3RX_DAC_COUNT        27

// Medipix3.1 DAC codes
#define MPX3_DAC_THRESH_0       1
#define MPX3_DAC_THRESH_1       2
#define MPX3_DAC_THRESH_2       3
#define MPX3_DAC_THRESH_3       4
#define MPX3_DAC_THRESH_4       5
#define MPX3_DAC_THRESH_5       6
#define MPX3_DAC_THRESH_6       7
#define MPX3_DAC_THRESH_7       8
#define MPX3_DAC_PREAMP         9
#define MPX3_DAC_IKRUM          10
#define MPX3_DAC_SHAPER         11
#define MPX3_DAC_DISC           12
#define MPX3_DAC_DISC_LS        13
#define MPX3_DAC_THRESH_N       14
#define MPX3_DAC_PIXEL          15
#define MPX3_DAC_DELAY          16
#define MPX3_DAC_TP_BUF_IN      17
#define MPX3_DAC_TP_BUF_OUT     18
#define MPX3_DAC_RPZ            19
#define MPX3_DAC_GND            20
#define MPX3_DAC_TP_REF         21
#define MPX3_DAC_FBK            22
#define MPX3_DAC_CAS            23
#define MPX3_DAC_TP_REF_A       24
#define MPX3_DAC_TP_REF_B       25

// Medipix3RX DAC codes
#define MPX3RX_DAC_THRESH_0     1
#define MPX3RX_DAC_THRESH_1     2
#define MPX3RX_DAC_THRESH_2     3
#define MPX3RX_DAC_THRESH_3     4
#define MPX3RX_DAC_THRESH_4     5
#define MPX3RX_DAC_THRESH_5     6
#define MPX3RX_DAC_THRESH_6     7
#define MPX3RX_DAC_THRESH_7     8
#define MPX3RX_DAC_PREAMP       9
#define MPX3RX_DAC_IKRUM        10
#define MPX3RX_DAC_SHAPER       11
#define MPX3RX_DAC_DISC         12
#define MPX3RX_DAC_DISC_LS      13
#define MPX3RX_DAC_SHAPER_TEST  14
#define MPX3RX_DAC_DISC_L       15
#define MPX3RX_DAC_TEST         30
#define MPX3RX_DAC_DISC_H       31
#define MPX3RX_DAC_DELAY        16
#define MPX3RX_DAC_TP_BUF_IN    17
#define MPX3RX_DAC_TP_BUF_OUT   18
#define MPX3RX_DAC_RPZ          19
#define MPX3RX_DAC_GND          20
#define MPX3RX_DAC_TP_REF       21
#define MPX3RX_DAC_FBK          22
#define MPX3RX_DAC_CAS          23
#define MPX3RX_DAC_TP_REF_A     24
#define MPX3RX_DAC_TP_REF_B     25

// ----------------------------------------------------------------------------
#endif // MPX3DEFS_H_
