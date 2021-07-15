/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Codec interface file.
 *
 *  Copyright (c) 2019-2020 Packetcraft, Inc.
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/

#ifndef PAL_CODEC_H
#define PAL_CODEC_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *  \addtogroup PAL_CODEC
 *  \{
 */

/**************************************************************************************************'
  Data Types
**************************************************************************************************/

/*! \brief      Audio data path direction. */
typedef enum
{
  PAL_CODEC_DIR_INPUT   = 0,    /*!< Input data path. */
  PAL_CODEC_DIR_OUTPUT  = 1     /*!< Output data path. */
} PalCodecDir_t;

/*! \brief      Audio Channel. */
typedef enum
{
  PAL_CODEC_CH_LEFT     = 0,    /*!< Left channel. */
  PAL_CODEC_CH_RIGHT    = 1,    /*!< Right channel. */
  PAL_CODEC_NUM_CH              /*!< Total number of channel. */
} PalCodecChan_t;

/*! \brief      Codec IDs. */
typedef enum
{
  /* Standard codecs. */
  PAL_CODEC_ID_LC3_10000US_48KHZ_16B_L    = 0x0000,
  PAL_CODEC_ID_LC3_10000US_48KHZ_16B_R    = 0x0001,
  PAL_CODEC_ID_LC3_10000US_16KHZ_16B_L    = 0x0002,
  PAL_CODEC_ID_LC3_10000US_16KHZ_16B_R    = 0x0003,
  PAL_CODEC_ID_LC3_7500US_48KHZ_16B_L     = 0x0004,
  PAL_CODEC_ID_LC3_7500US_48KHZ_16B_R     = 0x0005,
  /* Custom codecs. */
  PAL_CODEC_ID_SBC_10000US_16KHZ_16B_JS   = 0xF000,
  PAL_CODEC_ID_OPUS_10000US_48KHZ_16B_JS  = 0xF100,
  PAL_CODEC_ID_BV32_15000US_16KHZ_16B_M   = 0xF200,
} PalCodecId_t;

/*! \brief      Codec info block. */
typedef struct
{
  uint8_t codecFmt;             /*!< Coding format. */
  uint16_t codecId;             /*!< Codec ID. */
} PalCodecInfo_t;

/*! \brief      Standard codec info block. */
typedef struct
{
  uint8_t codecId;              /*!< Codec ID. */
  uint8_t supTrans;             /*!< Supported transport mask. */
} PalCodecStdInfo_t;

/*! \brief      VS codec info block. */
typedef struct
{
  uint16_t compId;              /*!< Company ID. */
  uint16_t codecId;             /*!< Codec ID. */
  uint8_t  supTrans;            /*!< Supported transport mask. */
} PalCodecVsInfo_t;

/*! \brief      Buffer available call signature. */
typedef void (*PalCodecDataInCback_t)(uint16_t id, uint32_t pktCtr, uint32_t ts, uint8_t *pData, uint16_t actLen);

/*! \brief      Stream parameters. */
typedef struct
{
  PalCodecDir_t dir;            /*!< Stream data direction. */
  uint32_t      pktCtr;         /*!< Initial packet counter value. */
  uint16_t      chMask;         /*!< Channel mask. */
  uint16_t      codecId;        /*!< Codec ID. */
  PalCodecDataInCback_t inCback;/*!< Data input callback. */
} PalCodecStreamParam_t;

/*! \brief      Codec capability. */
typedef struct
{
  uint32_t sampFreq;            /*!< Sampling frequency. */
  uint32_t frmDurUsec;          /*!< Frame duration in microseconds. */
  uint16_t numChan;             /*!< Channel count. */
  uint16_t minFrmLen;           /*!< Minimum frame length. */
  uint16_t maxFrmLen;           /*!< Maximum frame length. */
  uint32_t minDlyOutUsec;       /*!< Minimum output delay in microseconds. */
  uint32_t maxDlyOutUsec;       /*!< Maximum output delay in microseconds. */
  uint32_t minDlyInUsec;        /*!< Minimum input delay in microseconds. */
  uint32_t maxDlyInUsec;        /*!< Maximum input delay in microseconds. */
} PalCodecCap_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Codec Information */
void PalCodecGetSupported(uint8_t codingFmt, uint8_t *pNumInfo, PalCodecInfo_t info[]);
bool_t PalCodecGetCapability(PalCodecId_t codecId, PalCodecCap_t *pCap);

// TODO remove
void PalCodecReadLocalSupportedCodecs(uint8_t *pNumStd, PalCodecStdInfo_t stdCodecs[],
                                      uint8_t *pNumVs, PalCodecVsInfo_t vsCodecs[]);
bool_t PalCodecReadLocalSupportedCodecCapabilities(uint8_t codingFmt, uint16_t compId, uint16_t vsCodecId, PalCodecDir_t dir, uint8_t transType,
                                                   uint8_t *pCapLen, uint8_t *pCap);
bool_t PalCodecReadLocalSupportedControllerDelay(uint8_t codingFmt, uint16_t compId, uint16_t vsCodecId, PalCodecDir_t dir, uint8_t transType,
                                                 uint32_t *pMinDly, uint32_t *pMaxDly);
bool_t PalCodecConfigureDataPath(PalCodecDir_t dir, uint8_t vsCfgLen, const uint8_t *pVsCfg);

/* Audio Amplifier */
void PalCodecAmpInit(void);
uint8_t PalCodecAmpGetVolume(void);
void PalCodecAmpSetVolume(uint8_t vol);
void PalCodecAmpVolumeUp(void);
void PalCodecAmpVolumeDown(void);
void PalCodecAmpMute(void);
void PalCodecAmpUnmute(void);

/* Data Path */
void PalCodecDataInit(void);
bool_t PalCodecDataStartStream(uint16_t id, PalCodecStreamParam_t *pParam);
void PalCodecDataStopStream(uint16_t id, PalCodecDir_t dir);
void PalCodecDataStreamIn(uint16_t id, uint8_t *pBuf, uint16_t len);
void PalCodecDataStreamOut(uint16_t id, const uint8_t *pBuf, uint16_t len, uint32_t sduRef);

/*! \} */    /* PAL_CODEC */

#ifdef __cplusplus
};
#endif

#endif /* PAL_CODEC_H */
