/*

    \file                          Definition.h
    \brief                         Definition File, listing the registers
    \author                        Nicolas PIERRE
    \version                       1.0
    \date                          07/06/14
    Support :                      mail to : nico.pierre@icloud.com

 */
#ifndef _DEFINITION_H__
#define _DEFINITION_H__

#include <iostream>
#include <map>

//-----------------------------------------------------------------------------
// Glib Config Files

// Time out for stack writing
// #define TIME_OUT         5

//------------------------------------------------------------------------------
#define NCHANNELS 254
#define NSSACHANNELS 120
#define NMPACHANNELS 1920
#define NMPACOLS 16
#define NCHIPS_OT 8
#define HYBRID_CHANNELS_OT NCHIPS_OT* NCHANNELS
#define TOTAL_CHANNELS_OT NCHIPS_OT* NCHANNELS * 2

// Fix issue if HOST_NAME_MAX is not declared
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

// Events

// CBC2
// in uint32_t words
#define CBC_EVENT_SIZE_32 9        // 9 32bit words per CBC
#define EVENT_HEADER_TDC_SIZE_32 6 // total of 6 32 bit words for HEADER + TDC
#define EVENT_HEADER_SIZE_32 5     // 5 words for the header

#define MPAlight_HEADER_SIZE_32 4099
#define MPAlight_EVENT_SIZE_32 240

#define MPA_HEADER_SIZE_32 1
#define MPA_EVENT_SIZE_32 5

#define SSA_HEADER_SIZE_32 1
#define SSA_EVENT_SIZE_32 8

// Event
#define OFFSET_BUNCH 8
#define WIDTH_BUNCH 24
#define OFFSET_ORBIT 1 * 32 + 8
#define WIDTH_ORBIT 24
#define OFFSET_LUMI 2 * 32 + 8
#define WIDTH_LUMI 24
#define OFFSET_EVENT_COUNT 3 * 32 + 8
#define WIDTH_EVENT_COUNT 24
#define OFFSET_EVENT_COUNT_CBC 4 * 32 + 8
#define WIDTH_EVENT_COUNT_CBC 3 * 8

// Cbc Event
#define OFFSET_ERROR 0
#define WIDTH_ERROR 2
#define OFFSET_PIPELINE_ADDRESS 2 // OFFSET_ERROR + WIDTH_ERROR
#define WIDTH_PIPELINE_ADDRESS 8
#define OFFSET_CBCDATA 2 + 8     // OFFSET_PIPELINE_ADDRESS + WIDTH_PIPELINE_ADDRESS
#define WIDTH_CBCDATA 254        // NCHANNELS
#define OFFSET_GLIBFLAG 10 + 254 // OFFSET_CBCDATA + WIDTH_CBCDATA
#define WIDTH_GLIBFLAG 12
#define OFFSET_CBCSTUBDATA 264 + 23 // LAST BIT
#define IC_OFFSET_CBCSTUBDATA 276   // BIT 12
#define WIDTH_CBCSTUBDATA 12

// CBC3
// in uint32_t words
#define CBC_EVENT_SIZE_32_CBC3 11       // 11 32bit words per CBC
#define EVENT_HEADER_TDC_SIZE_32_CBC3 3 // total of 6 32 bit words for HEADER + TDC
#define EVENT_HEADER_SIZE_32_CBC3 3     // 5 words for the header

// D19C event header size (CIC)
// in uint32_t words
#define D19C_EVENT_HEADER1_SIZE_32_CIC 4

// D19C event header size (CBC)
#define D19C_EVENT_HEADER1_SIZE_32_CBC3 4
#define D19C_EVENT_SIZE_32_CBC3 16

// SSA
// in uint32_t words
#define D19C_EVENT_HEADER1_SIZE_32_SSA 4
#define D19C_EVENT_SIZE_32_SSA 12 // FIXME??

// SSA2 
// in float
#define SSA2_VBG_EXPECTED 0.275 // [V] this will need to be taken from database
#define SSA2_VREF_EXPECTED 0.850 // [V] this is true if ADC_VREF is tuned 
#define SSA2_VREF_MIN   0.750 // [V] should be verified once we have numbers/manual is updated
#define SSA2_VREF_MAX   1.0 // [V] should be verified once we have numbers/manual is updated
#define SSA2_ADC_PRECISION 0.010 // [V] from skeleton testing: changing by 1 bit ADC_VREF, VREF measured on skeleton changes by 7-8 mV. Here we are rounding up the precision.
#define SSA2_ELECTRON_CALDAC 243. // 1 CalDAC = 0.039 fC = 243 electrons - confirmed by Davide  
#define SSA2_ELECTRON_THDAC  250. // 1 ThDAC  = 0.040 fC = 250 electrons - confirmed by Davide 

// MPA
// in uint32_t words
#define D19C_EVENT_HEADER1_SIZE_32_MPA 4
#define D19C_EVENT_SIZE_32_MPA 32 // FIXME??

// MPA2 
// in float
#define MPA2_VBG_EXPECTED 0.280 // FIXMEEEE
#define MPA2_VREF_EXPECTED 0.850 // FIXMEEEE
#define MPA2_VREF_MIN   0.750 // FIXMEEEE
#define MPA2_VREF_MAX   1.0 // FIXMEEE
#define MPA2_ADC_PRECISION 0.010 // FIXME
#define MPA2_ELECTRON_CALDAC 220. // 1 CalDAC = 0.035 fC = 220 electrons - confirmed by Davide  
#define MPA2_ELECTRON_THDAC   94. // 1 ThDAC  = 0.015 fC =  94 electrons - confirmed by Davide 

// points to bufferoverlow
#define D19C_OFFSET_ERROR_CBC3 2 * 32 + 0

// D19C (MPA/SSA)
// D19C event header size
#define D19C_EVENT_HEADER1_SIZE_32 5
#define D19C_EVENT_HEADER2_SIZE_32 1

#define CBC_CHANNEL_GROUP_BITSET                                                                                                                                                                       \
    std::string("0000000000001100000000000000110000000000000011000000000000001100000000000000110000000000000011000000"                                                                                 \
                "0000000011000000000000001100000000000000110000000000000011000000000000001100000000000000110000000000"                                                                                 \
                "000011000000000000001100000000000000110000000000000011")

#define D19C_PCluster_SIZE_32_MPA 14
#define D19C_SCluster_SIZE_32_MPA 11

// Event
//#define OFFSET_BUNCH               8
//#define WIDTH_BUNCH                24
//#define OFFSET_ORBIT               1*32+8
//#define WIDTH_ORBIT                24
//#define OFFSET_LUMI                2*32+8
//#define WIDTH_LUMI                 24
#define OFFSET_EVENT_COUNT_CBC3 2 * 32 + 3
#define WIDTH_EVENT_COUNT_CBC3 29

// Cbc Event
#define OFFSET_EVENT_COUNT_CBC_CBC3 2 * 32 + 4
#define WIDTH_EVENT_COUNT_CBC_CBC3 9
#define OFFSET_ERROR_CBC3 2 * 32 + 22
#define WIDTH_ERROR_CBC3 2
#define OFFSET_PIPELINE_ADDRESS_CBC3 2 * 32 + 13 // OFFSET_ERROR + WIDTH_ERROR
#define WIDTH_PIPELINE_ADDRESS_CBC3 9
#define OFFSET_CBCDATA_CBC3 2 * 32 + 4 // OFFSET_PIPELINE_ADDRESS + WIDTH_PIPELINE_ADDRESS
#define WIDTH_CBCDATA_CBC3 254         // NCHANNELS
#define OFFSET_GLIBFLAG_CBC3 10 + 254  // OFFSET_CBCDATA + WIDTH_CBCDATA
#define WIDTH_GLIBFLAG_CBC3 12
#define OFFSET_CBCSTUBDATA_CBC3 264 + 23 // LAST BIT
#define WIDTH_CBCSTUBDATA 12

// number of bend codes
#define BENDBINS 30
// Latency Scan
#define TDCBINS 8
#define VECSIZE 1000
//------------------------------------------------------------------------------

// OT Physics parameters
#define MAX_NUMBER_OF_STRIP_CLUSTERS 5
#define MAX_NUMBER_OF_PIXEL_CLUSTERS 5
#define MAX_NUMBER_OF_STUB_CLUSTERS_PS 5
#define MAX_NUMBER_OF_STUB_CLUSTERS_2S 3

// LpGBT conversion factors
#define VREF_LPGBT 1.0
#define CONVERSION_FACTOR (VREF_LPGBT / 1024.)

enum class BoardType
{
    UNDEFINED,
    D19C,
    RD53
};
enum class FrontEndType
{
    UNDEFINED = 0,
    HYBRID,
    CBC3,
    MPA,
    MPA2,
    SSA,
    SSA2,
    RD53A,
    RD53B,
    CIC,
    CIC2,
    OuterTracker2S,
    OuterTrackerPS,
    InnerTrackerDouble,
    InnerTrackerQuad,
    HYBRID2S,
    HYBRIDPS,
    LpGBT
};
enum class SLinkDebugMode
{
    SUMMARY = 0,
    FULL    = 1,
    ERROR   = 2
};
enum class EventType
{
    ZS    = 1,
    VR    = 2,
    SSA   = 3,
    MPA   = 4,
    SCAS  = 5,
    SSA2  = 6,
    SSAAS = 7,
    MPAAS = 8,
    PSAS  = 9,
    VR2S  = 10
};

// Monitoring parameters
#define MAX_LENGHT_PARAMETER_STRING 50

#endif
