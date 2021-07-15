/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Operation list maintenance implementation file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

/*************************************************************************************************/
/*
 *  This resource manager consists of the following features:
 *
 *      * Verification of reservation capacity
 *      * Selection of intervals given a range
 *      * Selection of txWindowOffset for adding or updating a reservation
 *
 *  Selection of txWindowOffset is optimized to allow the most medium time between each
 *  reservation's CE. It attempts to place the offset at 1/2 the largest gap among the largest
 *  reservation intervals.
 *
 *  For example, consider adding 3 reservations as illustrated below. Each reservation is added
 *  in order of its handle value. The first reservation added, handle=0, is given the maximum
 *  txWindowOffset to maximize the scan opportunity. Next, handle=1 is placed 1/2 the interval
 *  of handle=0 to maximize the CE duration between both handle=0 and handle=1. And finally,
 *  handle=2 and handle=3 split the remaining gaps.
 *
 *  rsvn[0] = { .handle=3, .offsUsec=30000, .interUsec=120000 }
 *  rsvn[1] = { .handle=1, .offsUsec=60000, .interUsec=120000 }
 *  rsvn[2] = { .handle=2, .offsUsec=90000, .interUsec=120000 }
 *  rsvn[3] = { .handle=0, .offsUsec=120000, .interUsec=120000 }
 *  |--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+-
 *  :...........0       :   0     :     0   :       0 :         0         : 0       :   0     :     0   :
 *  :.....1   :       1 :         1         : 1       :   1     :     1   :       1 :         1         :
 *  :........2:         :2        :  2      :    2    :      2  :        2:         :2        :  2      :
 *  :..3      :    3    :      3  :        3:         :3        :  3      :    3    :      3  :        3:
 *  |---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|
 *  0         1         2         3         4         5         6         7         8         9        10-ms
 */
/*************************************************************************************************/

#include "sch_int_rm.h"
#include "sch_int_tm.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include <string.h>

/*! \brief      Assert table doesn't exceed 32 entries addressable by RM 32-bit descriptor masks. */
WSF_CT_ASSERT((SCH_RM_MAX_RSVN <= 32));

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Total number of reservation bins. */
#define SCH_RM_MAX_RSVN_BINS      ((sizeof(schRmRsvnRatio)/sizeof(schRmRsvnRatio[0])) - 1)

/*! \brief      Maximum number of attempts to add reservation. */
#define SCH_RM_ADD_MAX_ATTEMPTS   4

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Scheduler resource manager control block. */
SchRmCb_t schRmCb;

/*! \brief      Reservation ratio for each reservation bin. One reservation with 10ms takes 50%, one with 20ms takes 25% and so on. */
const uint16_t schRmRsvnRatio[] = {50, 25, 17, 13, 10, 5};

/*************************************************************************************************/
/*!
 *  \brief      Select the preferred interval.
 *
 *  \param      minUsec     Minimum interval in microseconds.
 *  \param      maxUsec     Maximum interval in microseconds.
 *  \param      prefPerUsec Preferred periodicity in microseconds.
 *  \param      pref        Preference for selecting the interval.
 *  \param      exclude     Handle to exclude.
 *
 *  \return     Preferred interval in microseconds.
 */
/*************************************************************************************************/
static uint32_t schRmSelectPreferredIntervalUsec(uint32_t minUsec, uint32_t maxUsec, uint32_t prefPerUsec, uint8_t pref, uint8_t exclude)
{
  uint32_t prefInterUsec;
  uint32_t numInter;

  if (pref == SCH_RM_PREF_CAPACITY)   /* Search from upper bound */
  {
    numInter = (prefPerUsec > SCH_RM_PREF_PER_USEC) ? SCH_RM_DIV_PREF_PER(maxUsec) / SCH_RM_DIV_PREF_PER(prefPerUsec) : SCH_RM_DIV_PREF_PER(maxUsec);
    prefInterUsec = numInter * prefPerUsec;

    /* Use maxUsec if we can not find any interval between min and max with our preferred periodicity. */
    if (prefInterUsec < minUsec)
    {
      prefInterUsec = maxUsec;
    }
  }
  else  /* Search from lower bound */
  {
    numInter = (prefPerUsec > SCH_RM_PREF_PER_USEC) ? SCH_RM_DIV_PREF_PER(minUsec) / SCH_RM_DIV_PREF_PER(prefPerUsec) : SCH_RM_DIV_PREF_PER(minUsec);
    prefInterUsec = numInter * prefPerUsec;

    if (prefInterUsec < minUsec)
    {
      /* Round up to next periodic interval. */
      prefInterUsec = (numInter + 1) * prefPerUsec;
    }
  }

  /*** Prefer existing intervals ***/

  uint32_t intervUsec[SCH_RM_MAX_RSVN];
  uint8_t numIntervUsec = 0;

  for (unsigned int i = 0; i < SCH_RM_MAX_RSVN; i++)
  {
    if ((i != exclude) &&
        schRmCb.rsvnInterUsec[i])
    {
      intervUsec[numIntervUsec++] = schRmCb.rsvnInterUsec[i];
    }
  }

  /* Use larger intervals first to avoid capacity limitation. */
  schRmSortListDescending(intervUsec, numIntervUsec);

  for (unsigned int i = 0; i < numIntervUsec; i++)
  {
    if ((intervUsec[i] >= prefInterUsec) &&
        (intervUsec[i] <= maxUsec))
    {
      return intervUsec[i];
    }
  }

  /*** Limit upper bound ***/

  if (prefInterUsec > maxUsec)
  {
    /* Limit to max value. */
    prefInterUsec = maxUsec;
  }

  return prefInterUsec;
}

/*************************************************************************************************/
/*!
 *  \brief      Check reservation is within capacity.
 *
 *  \param      handle      Reservation handle.
 *  \param      interUsec   Interval in microseconds.
 *
 *  \return     TRUE if new reservation does not exceed capacity, FALSE otherwise.
 */
/*************************************************************************************************/
static bool_t schRmCheckRsvnCapacity(uint8_t handle, uint32_t interUsec)
{
  unsigned int maxBinIdx = 0;

  /* Number of reservations by interval size; zero index is SCH_RM_PREF_PER_USEC. */
  uint8_t numRsvnPerInter[SCH_RM_MAX_RSVN_BINS + 1] = { 0 };
  for (unsigned int i = 0; i < SCH_RM_MAX_RSVN; i++)
  {
    unsigned int rsvnIntervUsec;
    if (i != handle)
    {
      rsvnIntervUsec = schRmCb.rsvnInterUsec[i];
    }
    else
    {
      rsvnIntervUsec = interUsec;
    }

    if (!rsvnIntervUsec)
    {
      /* Inactive reservation. */
      continue;
    }

    /* Bin interval value inclusive of max value. */
    unsigned int binIdx = SCH_RM_DIV_PREF_PER(rsvnIntervUsec - 1);

    binIdx = (binIdx > SCH_RM_MAX_RSVN_BINS) ? SCH_RM_MAX_RSVN_BINS : binIdx;
    numRsvnPerInter[binIdx]++;
    maxBinIdx = WSF_MAX(binIdx, maxBinIdx);
  }

  /* Check capacity. */

  uint16_t rsvnPerc = 0;    /* Reservation percentage */
  uint16_t numRsvn;
  bool_t isMaxCap = FALSE;

  for (unsigned int i = 0; i < (maxBinIdx + 1); i++)
  {
    numRsvn = numRsvnPerInter[i];
    while (numRsvn)
    {
      if (!isMaxCap)
      {
        rsvnPerc += schRmRsvnRatio[i];
        numRsvn--;

        if (rsvnPerc >= 100)
        {
          isMaxCap = TRUE;
        }
      }

      if (isMaxCap && (numRsvn > 0))
      {
        /* Capacity exceeded. */
        return FALSE;
      }
    }
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Calculate depth between interval.
 *
 *  \param      intLarge    Larger interval to be compared.
 *  \param      intSmall    Smaller interval to be compared.
 *
 *  \return     Depth between intervals.
 */
/*************************************************************************************************/
uint8_t schRmIntCalculateDepth(uint32_t intLarge, uint32_t intSmall)
{
  uint8_t x;
  uint8_t depth = 0;
  uint32_t tmpInt = intLarge;

  WSF_ASSERT(intLarge > intSmall);

  for (x = 0; (x < SCH_RM_MAX_SEARCH_DEPTH) && (tmpInt > SCH_RM_MIN_OFFSET_UNIT_US); x++)
  {
    tmpInt >>= 1;
    if (tmpInt == intSmall)
    {
      depth = x + 1;
      break;
    }
  }

  return depth;
}

/*************************************************************************************************/
/*!
 *  \brief      Add resource manager offset for the specified handle.
 *
 *  \param      handle      Client defined reservation handle.
 *  \param      depth       Offset depth of the interval of the handle compared to maxConInt.
 *  \param      interUsec   Interval of the reservation in microseconds.
 *  \param      durUsec     Duration of the reservation in microseconds.
 *
 *  \return     TRUE if update was successful, FALSE otherwise..
 */
/*************************************************************************************************/
static bool_t schRmIntAddRmOffset(uint8_t handle, uint8_t depth, uint32_t interUsec, uint32_t durUsec)
{
  bool_t updated = FALSE;
  uint32_t offsetUnitUs = schRmCb.cmnInter >> schRmCb.offsetDepth;
  uint32_t tmpRmStatus = schRmCb.rmStatus;
  uint8_t numBitsPerRes = 1;    /* Number of bit offsets to be occupied by the reservation. */
  uint8_t x, y, z, numBitsOn;

  WSF_ASSERT(schRmCb.offsetDepth >= depth);
  WSF_ASSERT(durUsec < schRmCb.cmnInter);

  /* If duration is 12ms while offset unit is 5ms, the reservation will occupy 3 consecutive offset bits. */
  while (durUsec >= offsetUnitUs)
  {
    numBitsPerRes++;
    durUsec -= offsetUnitUs;
  }

  if (numBitsPerRes > 1)
  {
    /* Need to duplicate rmStatus to check if the reservation has conflict with lowest bit offset. */
    tmpRmStatus |= (tmpRmStatus <<  (1 << schRmCb.offsetDepth));
  }

  for (x = 0; x < (1 << (schRmCb.offsetDepth - depth)); x++)
  {
    /* Depth 0 means that the interval of the handle is same as common interval. Depth 1 means that it is half of common interval. */
    /* When offsetDepth is 3(8 bits) and depth is 1, we will cut rmStatus in half and check if the bits in same location are all zero. */
    numBitsOn = 0;
    for (y = 0; y < (1 << depth); y++)
    {
      for (z = 0; z < numBitsPerRes; z++)
      {
        if ((tmpRmStatus >> ((1 << (schRmCb.offsetDepth - depth)) * y + x + z)) & 0x01)
        {
          numBitsOn++;
        }
      }
    }

    /* Found available offset bit(s). */
    if (numBitsOn == 0)
    {
      /* Check conflict with TM links. */
      if (schRmCb.rsvn[schRmCb.refHandle].refTimeCb != NULL)
      {
        uint32_t rmRefTime = schRmCb.rsvn[schRmCb.refHandle].refTimeCb(schRmCb.refHandle, NULL);
        uint32_t targetTime = rmRefTime + (offsetUnitUs * x);

        if (SchTmCheckConflict(targetTime, interUsec, durUsec))
        {
          /* Conflict is found, move on to next offset bits. */
          continue;
        }
      }

      /* Set all the bits */
      for (y = 0; y < (1 << depth); y++)
      {
        for (z = 0; z < numBitsPerRes; z++)
        {
          schRmCb.rmStatus |= (0x01 << ((1 << (schRmCb.offsetDepth - depth)) * y + x + z));
        }
      }

      schRmCb.rsvn[handle].offsetBit = x;
      schRmCb.rsvn[handle].cmdInterUsed = TRUE;
      updated = TRUE;

      LL_TRACE_INFO2("schRmIntAddRmOffset, handle=%u, offsetDepth=%u", handle, schRmCb.offsetDepth);
      LL_TRACE_INFO1("                     rmStatus=0x%x", schRmCb.rmStatus);
      LL_TRACE_INFO1("                     cmnInter=%u", schRmCb.cmnInter);
      LL_TRACE_INFO1("                     offsetBit=%u", schRmCb.rsvn[handle].offsetBit);
      break;
    }
  }

  return updated;
}

/*************************************************************************************************/
/*!
 *  \brief      Increase reservation manager offset depth.
 *
 *  \param      depth     Offset depth to be increased.
 *
 *  \return     TRUE if offset depth was increased successfully, FALSE otherwise..
 */
/*************************************************************************************************/
static bool_t schRmIntIncOffsetDepth(uint8_t depth)
{
  uint8_t y, z, handle;
  uint8_t numBitsPerRes, rsvnDepth;
  uint32_t durUsec;
  uint32_t offsetUnitUs = schRmCb.cmnInter >> (schRmCb.offsetDepth + depth);

  /* Check if the offsetDepth will be valid after the increase. */
  if (offsetUnitUs < SCH_RM_MIN_OFFSET_UNIT_US)
  {
    return FALSE;
  }

  schRmCb.rmStatus = 0;
  schRmCb.offsetDepth += depth;

  /* Update database according to the new offsetDepth. */
  /* rmStatus b'11 becomes b'00010001, when depth is 2. */
  for (handle = 0; handle < SCH_RM_MAX_RSVN; handle++)
  {
    if (schRmCb.rsvn[handle].cmdInterUsed)
    {
      numBitsPerRes = 1;    /* Number of bit offsets to be occupied by the reservation. */
      durUsec = schRmCb.rsvn[handle].durUsec;
      WSF_ASSERT(durUsec < schRmCb.cmnInter);

      if (schRmCb.cmnInter == schRmCb.rsvn[handle].interUsec)
      {
        rsvnDepth = 0;
      }
      else
      {
        rsvnDepth = schRmIntCalculateDepth(schRmCb.cmnInter, schRmCb.rsvn[handle].interUsec);
        WSF_ASSERT(rsvnDepth > 0);
      }

      while (durUsec >= offsetUnitUs)
      {
        numBitsPerRes++;
        durUsec -= offsetUnitUs;
      }

      /* For example, with depth 2, bit1 will move into bit4. */
      schRmCb.rsvn[handle].offsetBit *= (1 << depth);

      /* Re-construct rmStatus */
      for (y = 0; y < (1 << rsvnDepth); y++)
      {
        for (z = 0; z < numBitsPerRes; z++)
        {
          schRmCb.rmStatus |= (0x01 << ((0x01 << (schRmCb.offsetDepth - rsvnDepth)) * y + schRmCb.rsvn[handle].offsetBit + z));
        }
      }
    }
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Decrease reservation manager offset depth by one if necessary.
 */
/*************************************************************************************************/
static void schRmIntCheckDecOffsetDepth(void)
{
  uint32_t tmpStatus = schRmCb.rmStatus;
  uint8_t x, evenOdd, other, handle, numBitsOn;
  uint8_t depth = schRmCb.offsetDepth - 1;

  if (schRmCb.offsetDepth < 2)
  {
    /* nothing to do. */
    return;
  }

  /* b'1010 with depth 2 is same as b'11 with depth 1. */
  /* Check if all even or odd bits are zero. */
  for (evenOdd = 0; evenOdd < 2; evenOdd++)
  {
    numBitsOn = 0;
    other = (evenOdd == 0) ? 1 : 0; /* When checking even bits, other is 1. */

    for (x = 0; x < (1 << depth); x++)
    {
      if ((tmpStatus >> (x * 2 + evenOdd)) & 0x01)
      {
        numBitsOn++;
      }
    }

    if (numBitsOn == 0)
    {
      schRmCb.offsetDepth--;
      schRmCb.rmStatus = 0;

      for (x = 0; x < (1 << depth); x++)
      {
        if ((tmpStatus >> (x * 2 + other)) & 0x01)
        {
          schRmCb.rmStatus |= (0x01 << (x));
        }
      }

      /* Update database according to the new offsetDepth. */
      for (handle = 0; handle < SCH_RM_MAX_RSVN; handle++)
      {
        if (schRmCb.rsvn[handle].cmdInterUsed)
        {
          schRmCb.rsvn[handle].offsetBit >>= 1;

          /* The handle for Bit0 is updated to reference handle. */
          if (schRmCb.rsvn[handle].offsetBit == 0)
          {
            schRmCb.refHandle = handle;
          }
        }
      }

      return;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Remove resource manager offset for the specified handle.
 *
 *  \param      handle      Client defined reservation handle.
 *
 *  \return     TRUE if update was successful, FALSE otherwise..
 */
/*************************************************************************************************/
static void schRmIntRemoveRmOffset(uint8_t handle)
{
  uint8_t x, y, depth;
  uint8_t numBitsPerRes = 1;    /* Number of consecutive offset bits occupied by the reservation. */
  uint32_t durUsec;
  uint32_t offsetUnitUs = schRmCb.cmnInter >> schRmCb.offsetDepth;

  if (schRmCb.rsvn[handle].cmdInterUsed)
  {
    WSF_ASSERT(schRmCb.rsvn[handle].durUsec < schRmCb.cmnInter);

    /* Check how many continuous bit offsets were occupied by this reservation. */
    durUsec = schRmCb.rsvn[handle].durUsec;
    while (durUsec >= offsetUnitUs)
    {
      numBitsPerRes++;
      durUsec -= offsetUnitUs;
    }

    /* The interval of this handle must be equal to or smaller than the common interval. */
    if (schRmCb.cmnInter == schRmCb.rsvn[handle].interUsec)
    {
      for (y = 0; y < numBitsPerRes; y++)
      {
        schRmCb.rmStatus &= ~(1 << (schRmCb.rsvn[handle].offsetBit + y));
      }
    }
    else
    {
      depth = schRmIntCalculateDepth(schRmCb.cmnInter, schRmCb.rsvn[handle].interUsec);

      if (depth)
      {
        for (x = 0; x < (1 << depth); x++)
        {
          for (y = 0; y < numBitsPerRes; y++)
          {
            schRmCb.rmStatus &= ~(1 << ((1 << (schRmCb.offsetDepth - depth)) * x + schRmCb.rsvn[handle].offsetBit + y));
          }
        }
      }
    }

    schRmCb.rsvn[handle].cmdInterUsed = FALSE;

    /* Adjust offsetDepth if necessary. */
    schRmIntCheckDecOffsetDepth();

    /* All the reservations are removed. */
    if (schRmCb.rmStatus == 0)
    {
      schRmCb.cmnInter = 0;
      schRmCb.offsetDepth = 0;
    }

    LL_TRACE_INFO2("schRmIntRemoveRmOffset, handle=%u, offsetDepth=%u", handle, schRmCb.offsetDepth);
    LL_TRACE_INFO1("                        rmStatus=0x%x", schRmCb.rmStatus);
    LL_TRACE_INFO1("                        cmnInter=%u", schRmCb.cmnInter);
  }
  else
  {
    if (schRmCb.indexUncommon > 0)
    {
      /* Uncommon handle by reservation manager. */
      schRmCb.indexUncommon--;
      LL_TRACE_INFO2("schRmIntRemoveRmOffset, deleted uncommon index %u, handle=%u", schRmCb.indexUncommon, handle);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Increase common interval of reservation manager.
 *
 *  \param      depth     Depth between current interval and new interval.
 */
/*************************************************************************************************/
static void schRmIntIncCommInterval(uint8_t depth)
{
  uint8_t x, y;
  uint32_t tmpStatus = schRmCb.rmStatus;

  WSF_ASSERT(depth > 0);

  schRmCb.cmnInter = schRmCb.cmnInter << depth;

  /* Reconstruct rmStatus as to the new common interval. */
  schRmCb.rmStatus = 0;
  for (x = 0; x < (1 << schRmCb.offsetDepth); x++)
  {
    if ((tmpStatus >> x) & 0x01)
    {
      for (y = 0; y < (1 << depth); y++)
      {
        schRmCb.rmStatus |= (0x01 << (y * (1 << schRmCb.offsetDepth) + x));
      }
    }
  }

  schRmCb.offsetDepth += depth;

  /* Offset bit location(offsetBit) stays same for existing handles. */
}

/*************************************************************************************************/
/*!
 *  \brief      Handle adding resource manager offset for the specified handle.
 *
 *  \param      handle        Client defined reservation handle.
 *  \param      newInterUsec  Interval of the reservation in microseconds.
 *  \param      durUsec       Duration of the reservation in microseconds.
 *
 *  \return     TRUE if update was successful, FALSE otherwise..
 */
/*************************************************************************************************/
static bool_t schRmIntHandleAddRmOffset(uint8_t handle, uint32_t newInterUsec, uint32_t durUsec)
{
  bool_t status = TRUE;
  uint8_t numAttempt = 0;

  /* Let's assume all connections are with common multiples. */
  /* Will add function later to determine if 2 intervals are with common intervals. */
  if (schRmCb.cmnInter == 0)
  {
    schRmCb.cmnInter = newInterUsec;
    schRmCb.indexUncommon = 0;
    schRmCb.offsetDepth = 0;  /* Offset unit is same as cmnInter. */
    schRmCb.rmStatus = 0x01;  /* 100 percent reserved. */

    schRmCb.rsvn[handle].offsetBit = 0;
    schRmCb.rsvn[handle].cmdInterUsed = TRUE;
    schRmCb.refHandle = handle;
  }
  else if (schRmCb.cmnInter == newInterUsec)
  {
    while (status == TRUE)
    {
      numAttempt++;
      if (schRmIntAddRmOffset(handle, 0, newInterUsec, durUsec))
      {
        break;
      }
      else if (numAttempt <= SCH_RM_ADD_MAX_ATTEMPTS)
      {
        /* Offset depth needs to be increased if RM status is full with the current depth. */
        status = schRmIntIncOffsetDepth(1);
      }
      else
      {
        /* Failed with maximum attempts. */
        status = FALSE;
      }
    }
  }
  else if (schRmCb.cmnInter < newInterUsec)   /* new interval is multiple of cmnInter. */
  {
    uint8_t depth = schRmIntCalculateDepth(newInterUsec, schRmCb.cmnInter);

    if (depth)
    {
      schRmIntIncCommInterval(depth);

      while (status == TRUE)
      {
        numAttempt++;
        if (schRmIntAddRmOffset(handle, 0, newInterUsec, durUsec))
        {
          break;
        }
        else if (numAttempt <= SCH_RM_ADD_MAX_ATTEMPTS)
        {
          /* Offset depth needs to be increased if RM status is full with the current depth. */
          status = schRmIntIncOffsetDepth(1);
        }
        else
        {
          /* Failed with maximum attempts. */
          status = FALSE;
        }
      }
    }
    else
    {
      /* Uncommon handle by reservation manager. */
      schRmCb.indexUncommon++;
      LL_TRACE_INFO2("Adding uncommon index %u, handle=%u", schRmCb.indexUncommon, handle);
    }
  }
  else if (schRmCb.cmnInter > newInterUsec)
  {
    uint8_t depth = schRmIntCalculateDepth(schRmCb.cmnInter, newInterUsec);

    if (depth)
    {
      /* First update offset depth so that offset unit would be same as new interval. */
      if (schRmCb.offsetDepth < depth)
      {
        status = schRmIntIncOffsetDepth(depth - schRmCb.offsetDepth);
      }

      while (status == TRUE)
      {
        numAttempt++;
        if (schRmIntAddRmOffset(handle, depth, newInterUsec, durUsec))
        {
          break;
        }
        else if (numAttempt <= SCH_RM_ADD_MAX_ATTEMPTS)
        {
          /* Offset depth needs to be increased if RM status is full with the current depth. */
          status = schRmIntIncOffsetDepth(1);
        }
        else
        {
          /* Failed with maximum attempts. */
          status = FALSE;
        }
      }
    }
    else
    {
      /* Uncommon handle by reservation manager. */
      schRmCb.indexUncommon++;
      LL_TRACE_INFO2("Adding uncommon index %u, handle=%u", schRmCb.indexUncommon, handle);
    }
  }

  if (status == TRUE)
  {
    schRmCb.rsvn[handle].handle = handle;
    schRmCb.rsvn[handle].interUsec = newInterUsec;
    schRmCb.rsvn[handle].durUsec = durUsec;
  }
  else
  {
    LL_TRACE_WARN2("schRmIntHandleAddRmOffset, Add RM failed, handle=%u, interval=%u", handle, newInterUsec);
  }

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      In-place descending sort of a list of numbers.
 *
 *  \param      item        List of numbers.
 *  \param      numItems    Number of numbers in item[].
 * Descending
 */
/*************************************************************************************************/
void schRmSortListDescending(uint32_t item[], uint8_t numItems)
{
  if (numItems <= 1)
  {
    return;
  }

  unsigned int numUnsorted = numItems;

  do
  {
    unsigned int swapIdx = 0;

    for (unsigned int i = 1; i < numUnsorted; i++)
    {
      if (item[i-1] < item[i])
      {
        uint32_t tmp;
        tmp = item[i-1];
        item[i-1] = item[i];
        item[i] = tmp;
        swapIdx = i;
      }
    }

    /* Items after swapIdx are sorted. */
    numUnsorted = swapIdx;

  } while (numUnsorted > 1);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize the resource manager.
 */
/*************************************************************************************************/
void SchRmInit(void)
{
  memset(&schRmCb, 0, sizeof(schRmCb));
}

/*************************************************************************************************/
/*!
 *  \brief      Calculate the common periodicity.
 *
 *  \param      peerPerUsec Peer's periodicity in microseconds.
 *
 *  \return     Common periodicity in microseconds.
 */
/*************************************************************************************************/
uint32_t SchRmCalcCommonPeriodicityUsec(uint32_t peerPerUsec)
{
  /* Is peer periodicity a multiple of local periodicity? */
  if ((SCH_RM_DIV_PREF_PER(peerPerUsec) * SCH_RM_PREF_PER_USEC) == peerPerUsec)
  {
    /* Accommodate the peer's periodicity. */
    return WSF_MAX(SCH_RM_PREF_PER_USEC, peerPerUsec);
  }

  /* Default to local preference. */
  return SCH_RM_PREF_PER_USEC;
}

/*************************************************************************************************/
/*!
 *  \brief      Validate and commit a new reservation against collision.
 *
 *  \param      handle      Client defined reservation handle.
 *  \param      pref        Preference for selecting the interval.
 *  \param      minUsec     Minimum interval in microseconds.
 *  \param      maxUsec     Maximum interval in microseconds.
 *  \param      durUsec     Duration of the reservation in microseconds.
 *  \param      pInterUsec  Actual interval return value in microseconds.
 *  \param      refTimeCb   Get reference time callback function.
 *
 *  \return     TRUE if reservation available, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t SchRmAdd(uint8_t handle, uint8_t pref, uint32_t minUsec, uint32_t maxUsec, uint32_t durUsec, uint32_t *pInterUsec, GetRefTimeCb_t refTimeCb)
{
  bool_t status = FALSE;
  uint32_t perfPerUsec = SCH_RM_PREF_PER_CONN_USEC;

  WSF_ASSERT(handle < SCH_RM_MAX_RSVN);
  WSF_ASSERT(maxUsec >= minUsec);

  if (pref == SCH_RM_PREF_CAPACITY)
  {
    perfPerUsec = SCH_RM_PREF_PER_SYNC_USEC;
  }

  /*** Select reservation's preferred interval. ***/

  uint32_t prefInterUsec = schRmSelectPreferredIntervalUsec(minUsec, maxUsec, perfPerUsec, pref, handle);

  /*** Check reservation capacity. ***/

  if (!schRmCheckRsvnCapacity(handle, prefInterUsec))
  {
    return FALSE;
  }

  /*** Commit reservation. ***/

  status = schRmIntHandleAddRmOffset(handle, prefInterUsec, durUsec);
  if (status == TRUE)
  {
    schRmCb.rsvn[handle].refTimeCb = refTimeCb;

    schRmCb.rsvnInterUsec[handle] = prefInterUsec;
    schRmCb.numRsvn++;
    if (pInterUsec)
    {
      *pInterUsec = prefInterUsec;
    }
  }

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief      Start update to a reservation.
 *
 *  \param      handle      Client defined reservation handle.
 *  \param      minUsec     Minimum interval in microseconds.
 *  \param      maxUsec     Maximum interval in microseconds.
 *  \param      perfPerUsec Preferred periodicity in microseconds.
 *  \param      durUsec     Duration of the connection in microseconds.
 *  \param      pInterUsec  Actual interval return value in microseconds.
 *
 *  \return     TRUE if resource is can be updated, FALSE otherwise.
 *
 *  \note       For BLE, connection belonging to handle must be a master only.
 */
/*************************************************************************************************/
bool_t SchRmStartUpdate(uint8_t handle, uint32_t minUsec, uint32_t maxUsec, uint32_t perfPerUsec, uint32_t durUsec, uint32_t *pInterUsec)
{
  WSF_ASSERT(handle < SCH_RM_MAX_RSVN);
  WSF_ASSERT(maxUsec >= minUsec);

  /*** Select reservation's preferred interval. ***/

  uint32_t prefInterUsec = schRmSelectPreferredIntervalUsec(minUsec, maxUsec, perfPerUsec, SCH_RM_PREF_PERFORMANCE, handle);

  /*** Check reservation capacity. ***/

  if (!schRmCheckRsvnCapacity(handle, prefInterUsec))
  {
    return FALSE;
  }

  /*** Commit reservation. ***/

  schRmCb.rsvnInterUsec[handle] = prefInterUsec;
  *pInterUsec = prefInterUsec;

  schRmIntRemoveRmOffset(handle);
  (void)schRmIntHandleAddRmOffset(handle, prefInterUsec, durUsec);

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Commit an update to a reservation.
 *
 *  \param      handle      Client defined reservation handle.
 */
/*************************************************************************************************/
void SchRmCommitUpdate(uint8_t handle)
{
  /* No action required; already committed in SchRmStartUpdate(). */
}

/*************************************************************************************************/
/*!
 *  \brief      Remove an existing reservation.
 *
 *  \param      handle      Client defined reservation handle.
 */
/*************************************************************************************************/
void SchRmRemove(uint8_t handle)
{
  WSF_ASSERT(handle < SCH_RM_MAX_RSVN);
  WSF_ASSERT(schRmCb.rsvnInterUsec[handle]);

  schRmCb.rsvnInterUsec[handle] = 0;
  schRmCb.numRsvn--;

  schRmIntRemoveRmOffset(handle);
}

/*************************************************************************************************/
/*!
 *  \brief      Get the next available offset in microseconds.
 *
 *  \param      defOffsUsec Default offset in microseconds.
 *  \param      handle      Reservation handle.
 *  \param      refTime     Reference time in microseconds.
 *
 *  \return     Offset in microseconds.
 */
/*************************************************************************************************/
uint32_t SchRmGetOffsetUsec(uint32_t defOffsUsec, uint8_t handle, uint32_t refTime)
{
  uint32_t rmRefTime = refTime;
  uint32_t targetTime, offsUsec;
  uint32_t offsetUnitUs = schRmCb.cmnInter >> schRmCb.offsetDepth;

  /* Out of sequence. */
  WSF_ASSERT(schRmCb.numRsvn);

  /* Ensure no division by 0. */
  WSF_ASSERT(schRmCb.rsvn[handle].interUsec > 0);

  if (schRmCb.numRsvn <= 1)
  {
    /* We do not have reference anchor point yet for the 1st reservation. */
    /* First anchor point will be chosen to avoid conflict with TM links. */
    return SchTmGetFirstAnchor(refTime, defOffsUsec, schRmCb.rsvn[handle].interUsec, schRmCb.rsvn[handle].durUsec);
  }

  /* rmRefTime is the time for offset bit 0. */
  if (schRmCb.rsvn[schRmCb.refHandle].refTimeCb != NULL)
  {
    rmRefTime = schRmCb.rsvn[schRmCb.refHandle].refTimeCb(schRmCb.refHandle, NULL);
  }

  if ((schRmCb.cmnInter != 0) && (schRmCb.rsvn[handle].cmdInterUsed == TRUE))
  {
    uint32_t rmOffsUsec;

    /* Time of Offset bit n = rmRefTime + offsetUnit * n. */
    rmOffsUsec = offsetUnitUs * schRmCb.rsvn[handle].offsetBit;
    if (rmOffsUsec == 0)
    {
      rmOffsUsec = schRmCb.rsvn[handle].interUsec;
    }

    targetTime = rmRefTime + rmOffsUsec;
    offsUsec = BbGetTargetTimeDelta(targetTime, refTime);

    LL_TRACE_INFO2("SchRmGetOffsetUsec, handle=%u, refhandle=%u", handle, schRmCb.refHandle);
    LL_TRACE_INFO1("                    rmOffsUsec=%u", rmOffsUsec);
  }
  else
  {
    uint8_t offsetBit = 0;
    uint8_t numUncommon = schRmCb.indexUncommon;
    uint32_t uncommonOffsetUs = SCH_RM_OFFSET_UNCOMMON_US; /* Offset in microseconds for uncommon handle */

    numUncommon--;
    while (numUncommon)
    {
      offsetBit++;
      numUncommon--;
      offsetBit = (offsetBit >= (1 << schRmCb.offsetDepth)) ? 0 : offsetBit;
    }

    /* Find the duration of BOD of the handle which occupies the offset bit. */
    for (unsigned int i = 0; i < SCH_RM_MAX_RSVN; i++)
    {
      if ((schRmCb.rsvn[i].offsetBit == offsetBit) && (schRmCb.rsvn[i].cmdInterUsed == TRUE))
      {
        if (schRmCb.rsvn[i].refTimeCb != NULL)
        {
          (void)schRmCb.rsvn[i].refTimeCb(i, &uncommonOffsetUs);
          uncommonOffsetUs += BbGetSchSetupDelayUs() + SCH_RM_MARGIN_UNCOMMON_US;
        }
      }
    }

    /* Place the uncommon handle away from each common ones to avoid conflicts. */
    targetTime = rmRefTime + (offsetBit * offsetUnitUs) + uncommonOffsetUs;
    offsUsec = BbGetTargetTimeDelta(targetTime, refTime);

    LL_TRACE_INFO2("SchRmGetOffsetUsec, uncommonHandle=%u, refHandle=%u", handle, schRmCb.refHandle);
  }

  /* Ensure "targetTime > refTime". */
  if (offsUsec == 0)
  {
    unsigned int numInter = 1;

    if (schRmCb.rsvn[handle].interUsec)   /* Protect against division by 0. */
    {
      numInter = (BbGetTargetTimeDelta(refTime, targetTime) / schRmCb.rsvn[handle].interUsec) + 1;
    }

    targetTime += numInter * schRmCb.rsvn[handle].interUsec;
    offsUsec = BbGetTargetTimeDelta(targetTime, refTime);
  }

  if (schRmCb.rsvn[handle].interUsec)     /* Protect against division by 0. */
  {
    /* Ensure "offset <= interval". */
    offsUsec %= schRmCb.rsvn[handle].interUsec;
  }

  LL_TRACE_INFO1("                    targetTime=%u", (refTime + offsUsec));
  return offsUsec;
}
