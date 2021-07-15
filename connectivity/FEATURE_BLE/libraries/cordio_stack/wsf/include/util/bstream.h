/*************************************************************************************************/
/*!
 *  \file   bstream.h
 *
 *  \brief  Byte stream to integer conversion macros.
 *
 *  Copyright (c) 2009-2018 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2021 Packetcraft, Inc.
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
#ifndef BSTREAM_H
#define BSTREAM_H

#include "util/bda.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup WSF_UTIL_API
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

/**
 * \name Macros for converting a little endian byte buffer to integers.
 */
/**@{*/
/*! \brief convert little endian byte buffer to int16_t. */
#define BYTES_TO_INT16(n, p)      {n = ((int16_t)(p)[0] + ((int16_t)(p)[1] << 8));}
/*! \brief convert little endian byte buffer to uint16_t. */
#define BYTES_TO_UINT16(n, p)     {n = ((uint16_t)(p)[0] + ((uint16_t)(p)[1] << 8));}
/*! \brief convert little endian byte buffer to uint24_t. */
#define BYTES_TO_UINT24(n, p)     {n = ((uint16_t)(p)[0] + ((uint16_t)(p)[1] << 8) + \
                                        ((uint16_t)(p)[2] << 16));}
/*! \brief convert little endian byte buffer to uint32_t. */
#define BYTES_TO_UINT32(n, p)     {n = ((uint32_t)(p)[0] + ((uint32_t)(p)[1] << 8) + \
                                        ((uint32_t)(p)[2] << 16) + ((uint32_t)(p)[3] << 24));}
/*! \brief convert little endian byte buffer to uint40_t. */
#define BYTES_TO_UINT40(n, p)     {n = ((uint64_t)(p)[0] + ((uint64_t)(p)[1] << 8) + \
                                        ((uint64_t)(p)[2] << 16) + ((uint64_t)(p)[3] << 24) + \
                                        ((uint64_t)(p)[4] << 32));}
/*! \brief convert little endian byte buffer to uint64_t. */
#define BYTES_TO_UINT64(n, p)     {n = ((uint64_t)(p)[0] + ((uint64_t)(p)[1] << 8) + \
                                        ((uint64_t)(p)[2] << 16) + ((uint64_t)(p)[3] << 24) + \
                                        ((uint64_t)(p)[4] << 32) + ((uint64_t)(p)[5] << 40) + \
                                        ((uint64_t)(p)[6] << 48) + ((uint64_t)(p)[7] << 56));}
/**@}*/

/**
 * \name Macros for converting a big endian byte buffer to integers.
 */
/**@{*/
/*! \brief convert big endian byte buffer to uint16_t. */
#define BYTES_BE_TO_UINT16(n, p)     {n = ((uint16_t)(p)[1] + ((uint16_t)(p)[0] << 8));}
/*! \brief convert big endian byte buffer to 24-bit uint32_t (MSB 0). */
#define BYTES_BE_TO_UINT24(n, p)     {n = ((uint16_t)(p)[2] + ((uint16_t)(p)[1] << 8) + \
                                           ((uint16_t)(p)[0] << 16));}
/*! \brief convert big endian byte buffer to uint32_t. */
#define BYTES_BE_TO_UINT32(n, p)     {n = ((uint32_t)(p)[3] + ((uint32_t)(p)[2] << 8) + \
                                           ((uint32_t)(p)[1] << 16) + ((uint32_t)(p)[0] << 24));}
/**@}*/

/**
 * \name Macros for converting little endian integers to array of bytes
 */
/**@{*/
/*! \brief convert little endian uint16_t to array of bytes. */
#define UINT16_TO_BYTES(n)        ((uint8_t) (n)), ((uint8_t)((n) >> 8))
/*! \brief convert little endian uint24_t to array of bytes. */
#define UINT24_TO_BYTES(n)        ((uint8_t) (n)), ((uint8_t)((n) >> 8)), ((uint8_t)((n) >> 16))
/*! \brief convert little endian uint32_t to array of bytes. */
#define UINT32_TO_BYTES(n)        ((uint8_t) (n)), ((uint8_t)((n) >> 8)), ((uint8_t)((n) >> 16)), ((uint8_t)((n) >> 24))
/**@}*/

/**
 * \name Macros for converting big endian integers to array of bytes
 */
/**@{*/
/*! \brief convert big endian uint16_t to array of bytes. */
#define UINT16_TO_BE_BYTES(n)       ((uint8_t)((n) >> 8)), ((uint8_t) (n))
/*! \brief convert 24-bit big endian uint32_t (MSB 0) to array of bytes. */
#define UINT24_TO_BE_BYTES(n)       ((uint8_t)((n) >> 16)), ((uint8_t)((n) >> 8)), ((uint8_t) (n))
/*! \brief convert big endian uint32_t to array of bytes. */
#define UINT32_TO_BE_BYTES(n)       ((uint8_t)((n) >> 24)), ((uint8_t)((n) >> 16)), ((uint8_t)((n) >> 8)), ((uint8_t) (n))
/**@}*/

/**
 * \name Macros for converting little endian integers to single bytes
 */
/**@{*/
/*! \brief convert little endian uint16_t to byte 0. */
#define UINT16_TO_BYTE0(n)        ((uint8_t) (n))
/*! \brief convert little endian uint16_t to byte 1. */
#define UINT16_TO_BYTE1(n)        ((uint8_t) ((n) >> 8))

/*! \brief convert little endian uint32_t to byte 0. */
#define UINT32_TO_BYTE0(n)        ((uint8_t) (n))
/*! \brief convert little endian uint32_t to byte 1. */
#define UINT32_TO_BYTE1(n)        ((uint8_t) ((n) >> 8))
/*! \brief convert little endian uint32_t to byte 2. */
#define UINT32_TO_BYTE2(n)        ((uint8_t) ((n) >> 16))
/*! \brief convert little endian uint32_t to byte 3. */
#define UINT32_TO_BYTE3(n)        ((uint8_t) ((n) >> 24))
/**@}*/

/**
 * \name Macros for converting a little endian byte stream to integers, with increment.
 */
/**@{*/
/*! \brief convert little endian byte stream to uint8_t, incrementing one byte. */
#define BSTREAM_TO_INT8(n, p)     {n = (int8_t)(*(p)++);}
/*! \brief convert little endian byte stream to int8_t, incrementing one byte. */
#define BSTREAM_TO_UINT8(n, p)    {n = (uint8_t)(*(p)++);}
/*! \brief convert little endian byte stream to int16_t, incrementing two bytes. */
#define BSTREAM_TO_INT16(n, p)    {BYTES_TO_INT16(n, p); p += 2;}
/*! \brief convert little endian byte stream to uint16_t, incrementing two bytes. */
#define BSTREAM_TO_UINT16(n, p)   {BYTES_TO_UINT16(n, p); p += 2;}
/*! \brief convert little endian byte stream to uint24_t, incrementing three bytes. */
#define BSTREAM_TO_UINT24(n, p)   {BYTES_TO_UINT24(n, p); p += 3;}
/*! \brief convert little endian byte stream to uint32_t, incrementing four bytes. */
#define BSTREAM_TO_UINT32(n, p)   {BYTES_TO_UINT32(n, p); p += 4;}
/*! \brief convert little endian byte stream to uint40_t, incrementing five bytes. */
#define BSTREAM_TO_UINT40(n, p)   {BYTES_TO_UINT40(n, p); p += 5;}
/*! \brief convert little endian byte stream to uint64_t, incrementing eigth bytes. */
#define BSTREAM_TO_UINT64(n, p)   {n = BstreamToUint64(p); p += 8;}
/*! \brief convert little endian byte stream to six byte Bluetooth device address, incrementing six bytes. */
#define BSTREAM_TO_BDA(bda, p)    {BdaCpy(bda, p); p += BDA_ADDR_LEN;}
/*! \brief convert little endian byte stream to eight byte Bluetooth device address, incrementing eight bytes. */
#define BSTREAM_TO_BDA64(bda, p)  {bda = BstreamToBda64(p); p += BDA_ADDR_LEN;}
/**@}*/

/**
 * \name Macros for converting a big endian byte stream to integers, with increment.
 */
/**@{*/
/*! \brief convert big endian byte stream to uint16_t, incrementing one byte. */
#define BSTREAM_BE_TO_UINT16(n, p)   {BYTES_BE_TO_UINT16(n, p); p += 2;}
/*! \brief convert big endian byte stream to 24-bit uint32_t (MSB 0), incrementing one byte. */
#define BSTREAM_BE_TO_UINT24(n, p)   {BYTES_BE_TO_UINT24(n, p); p += 3;}
/**@}*/

/**
 * \name Macros for converting integers to a little endian byte stream, with increment.
 */
/**@{*/
/*! \brief convert uint8_t to little endian byte stream, incrementing one byte. */
#define UINT8_TO_BSTREAM(p, n)    {*(p)++ = (uint8_t)(n);}
/*! \brief convert uint16_t to little endian byte stream, incrementing two bytes. */
#define UINT16_TO_BSTREAM(p, n)   {*(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8);}
/*! \brief convert uint24_t to little endian byte stream, incrementing three bytes. */
#define UINT24_TO_BSTREAM(p, n)   {*(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                  *(p)++ = (uint8_t)((n) >> 16);}
/*! \brief convert uint32_t to little endian byte stream, incrementing four bytes. */
#define UINT32_TO_BSTREAM(p, n)   {*(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                  *(p)++ = (uint8_t)((n) >> 16); *(p)++ = (uint8_t)((n) >> 24);}
/*! \brief convert uint40_t to little endian byte stream, incrementing five bytes. */
#define UINT40_TO_BSTREAM(p, n)   {*(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                  *(p)++ = (uint8_t)((n) >> 16); *(p)++ = (uint8_t)((n) >> 24); \
                                  *(p)++ = (uint8_t)((n) >> 32);}
/*! \brief convert uint64_t to little endian byte stream, incrementing eight bytes. */
#define UINT64_TO_BSTREAM(p, n)   {Uint64ToBstream(p, n); p += sizeof(uint64_t);}
/*! \brief convert six byte Bluetooth device address to little endian byte stream, incrementing six bytes. */
#define BDA_TO_BSTREAM(p, bda)    {BdaCpy(p, bda); p += BDA_ADDR_LEN;}
/*! \brief convert eight byte Bluetooth device address to little endian byte stream, incrementing eigth bytes. */
#define BDA64_TO_BSTREAM(p, bda)  {Bda64ToBstream(p, bda); p += BDA_ADDR_LEN;}
/**@}*/

/**
 * \name Macros for converting integers to a big endian byte stream, with increment.
 */
/**@{*/
/*! \brief convert uint16_t to big endian byte stream, incrementing one byte. */
#define UINT16_TO_BE_BSTREAM(p, n)   {*(p)++ = (uint8_t)((n) >> 8); *(p)++ = (uint8_t)(n);}
/*! \brief convert uint32_t to big endian byte stream, incrementing one byte. */
#define UINT32_TO_BE_BSTREAM(p, n)   {*(p)++ = (uint8_t)((n) >> 24); *(p)++ = (uint8_t)((n) >> 16);\
                                      *(p)++ = (uint8_t)((n) >> 8); *(p)++ = (uint8_t)(n);}
/**@}*/

/**
 * \name Macros for converting integers to a little endian byte stream, without increment.
 */
/**@{*/
/*! \brief convert uint16_t to little endian byte stream. */
#define UINT16_TO_BUF(p, n)       {(p)[0] = (uint8_t)(n); (p)[1] = (uint8_t)((n) >> 8);}
/*! \brief convert uint24_t to little endian byte stream. */
#define UINT24_TO_BUF(p, n)       {(p)[0] = (uint8_t)(n); (p)[1] = (uint8_t)((n) >> 8); \
                                  (p)[2] = (uint8_t)((n) >> 16);}
/*! \brief convert uint32_t to little endian byte stream. */
#define UINT32_TO_BUF(p, n)       {(p)[0] = (uint8_t)(n); (p)[1] = (uint8_t)((n) >> 8); \
                                  (p)[2] = (uint8_t)((n) >> 16); (p)[3] = (uint8_t)((n) >> 24);}
/*! \brief convert uint40_t to little endian byte stream. */
#define UINT40_TO_BUF(p, n)       {(p)[0] = (uint8_t)(n); (p)[1] = (uint8_t)((n) >> 8); \
                                  (p)[2] = (uint8_t)((n) >> 16); (p)[3] = (uint8_t)((n) >> 24);\
                                  (p)[4] = (uint8_t)((n) >> 32);}
/**@}*/

/**
 * \name Macros for converting integers to a big endian byte stream, without increment.
 */
/**@{*/
/*! \brief convert uint16_t to big endian byte stream. */
#define UINT16_TO_BE_BUF(p, n)    {(p)[0] = (uint8_t)((n) >> 8); (p)[1] = (uint8_t)(n);}
/*! \brief convert 24-bit uint32_t (MSB 0) to big endian byte stream. */
#define UINT24_TO_BE_BUF(p, n)    {(p)[0] = (uint8_t)((n) >> 16); (p)[1] = (uint8_t)((n) >> 8); \
                                  (p)[2] = (uint8_t)(n);}
/*! \brief convert uint32_t to big endian byte stream. */
#define UINT32_TO_BE_BUF(p, n)    {(p)[0] = (uint8_t)((n) >> 24); (p)[1] = (uint8_t)((n) >> 16); \
                                  (p)[2] = (uint8_t)((n) >> 8); (p)[3] = (uint8_t)(n);}
/**@}*/

/**
 * \name Macros for comparing a little endian byte buffer to integers.
 */
/**@{*/
/*! \brief compare 2 byte little endian buffer with a uint16_t. */
#define BYTES_UINT16_CMP(p, n)    ((p)[1] == UINT16_TO_BYTE1(n) && (p)[0] == UINT16_TO_BYTE0(n))
/**@}*/

/**
 * \name Macros for IEEE FLOAT type:  exponent = byte 3, mantissa = bytes 2-0
 */
/**@{*/
/*! \brief Convert float to uint32. */
#define FLT_TO_UINT32(m, e)       ((m) | ((int32_t)(e) * 16777216))
/*! \brief Convert uint32_t to float. */
#define UINT32_TO_FLT(m, e, n)    {m = UINT32_TO_FLT_M(n); e = UINT32_TO_FLT_E(n);}
/*! \brief Convert uint32_t to float mantissa component */
#define UINT32_TO_FLT_M(n)        ((((n) & 0x00FFFFFF) >= 0x00800000) ? \
                                   ((int32_t)(((n) | 0xFF000000))) : ((int32_t)((n) & 0x00FFFFFF)))
/*! \brief Convert uint32_t to float exponent component. */
#define UINT32_TO_FLT_E(n)        ((int8_t)(n >> 24))
/**@}*/

/**
 * \name Macros for IEEE SFLOAT type:  exponent = bits 15-12, mantissa = bits 11-0
 */
/**@{*/
/*! \brief Convert sfloat to uint16_t */
#define SFLT_TO_UINT16(m, e)      ((m) | (0xF000 & ((int16_t)(e) * 4096)))
/*! \brief Convert uint16_t to sfloat */
#define UINT16_TO_SFLT(m, e, n)   {m = UINT16_TO_SFLT_M(n); e = UINT16_TO_SFLT_E(n);}
/*! \brief Convert uint16_T to sfloat mantissa component */
#define UINT16_TO_SFLT_M(n)       ((((n) & 0x0FFF) >= 0x0800) ? \
                                   ((int16_t)(((n) | 0xF000))) : ((int16_t)((n) & 0x0FFF)))
/*! \brief Convert uint16_T to sfloat exponent component */
#define UINT16_TO_SFLT_E(n)       (((n >> 12) >= 0x0008) ? \
                                   ((int8_t)(((n >> 12) | 0xF0))) : ((int8_t)(n >> 12)))
/**@}*/

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Convert bstream to BDA64.
 *
 *  \param  p       Bstream pointer.
 *
 *  \return Resulting BDA64 number.
 */
/*************************************************************************************************/
uint64_t BstreamToBda64(const uint8_t *p);

/*************************************************************************************************/
/*!
 *  \brief  Convert bstream to uint64_t.
 *
 *  \param  p       Bstream pointer.
 *
 *  \return Resulting uint64_t number.
 */
/*************************************************************************************************/
uint64_t BstreamToUint64(const uint8_t *p);

/*************************************************************************************************/
/*!
 *  \brief  Convert BDA64 to bstream.
 *
 *  \param  p       Bstream pointer.
 *  \param  bda     uint64_t BDA.
 */
/*************************************************************************************************/
void Bda64ToBstream(uint8_t *p, uint64_t bda);

/*************************************************************************************************/
/*!
 *  \brief  Convert uint64_t to bstream.
 *
 *  \param  p       Bstream pointer.
 *  \param  n       uint64_t number.
 */
/*************************************************************************************************/
void Uint64ToBstream(uint8_t *p, uint64_t n);

/*! \} */    /* WSF_UTIL_API */

#ifdef __cplusplus
};
#endif

#endif /* BSTREAM_H */
