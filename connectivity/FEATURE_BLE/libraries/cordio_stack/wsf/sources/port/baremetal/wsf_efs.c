/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Embedded File System service.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_efs.h"

#include "wsf_assert.h"
#include "wsf_cs.h"
#include "wsf_trace.h"
#include "util/bstream.h"

/* Media Control Block */
static pWsfEfsMedia_t wsfEfsMediaTbl[WSF_EFS_MAX_MEDIA];

/* File Control Block */
static wsfEfsControl_t wsfEfsFileTbl[WSF_EFS_MAX_FILES];

/*************************************************************************************************/
/*!
 *  \brief  Checks if the given physical media identifier has been registered with the file system.
 *
 *  \param  media   The physical media identifier
 *
 *  \return TRUE if valid, else FALSE.
 */
/*************************************************************************************************/
static bool_t wsfEfsMediaValid(uint8_t media)
{
  if (media < WSF_EFS_MAX_MEDIA && wsfEfsMediaTbl[media])
  {
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Returns the file control block for the given handle.
 *
 *  \param  handle   Handle of the file
 *
 *  \return File control block, or NULL.
 */
/*************************************************************************************************/
wsfEfsControl_t *WsfEfsGetFileByHandle(wsfEfsHandle_t handle)
{
  if ((handle < WSF_EFS_MAX_FILES) &&
      (wsfEfsFileTbl[handle].maxSize != WSF_EFS_INVALID_SIZE))
  {
    return &wsfEfsFileTbl[handle];
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Checks if the given offset and size overlaps with a given file.
 *
 *  \param  handle  The handle of the file to check against
 *  \param  media   The physical medium the new file will be in
 *  \param  address The starting address in the physical medium
 *  \param  size    The max size of the file
 *
 *  \return TRUE if overlap, else FALSE.
 */
/*************************************************************************************************/
static bool_t wsfEfsFileOverlap(wsfEfsHandle_t handle, uint8_t media, efsAddr_t address, uint32_t size)
{
  if (wsfEfsMediaValid(media))
  {
    /* Check if the table entry is in use and the physical media are the same */
    if (size && (wsfEfsFileTbl[handle].maxSize) &&
        (wsfEfsFileTbl[handle].maxSize != WSF_EFS_INVALID_SIZE) &&
        (wsfEfsFileTbl[handle].media == media))
    {
      /* Calculate the top and bottom addresses of the files */
      efsAddr_t top1 = wsfEfsFileTbl[handle].address;
      efsAddr_t bottom1 = wsfEfsFileTbl[handle].address + wsfEfsFileTbl[handle].maxSize - 1;
      efsAddr_t top2 = address;
      efsAddr_t bottom2 = address + size - 1;

      /* Check for overlap */
      if ((top2 >= top1) && (top2 <= bottom1))
      {
        return TRUE;
      }

      if ((bottom2 >= top1) && (bottom2 <= bottom1))
      {
        return TRUE;
      }
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Finds the offset of an unused area of the physical media big enough for a file
 *          of the given size.
 *
 *  \param  media   The physical medium
 *  \param  size    The max size of the file
 *
 *  \return The next available offset.
 */
/*************************************************************************************************/
static efsAddr_t wsfEfsFindAvailableOffset(uint8_t media, uint32_t size)
{
  if (wsfEfsMediaValid(media))
  {
    int8_t i;
    efsAddr_t address = wsfEfsMediaTbl[media]->startAddress;

    for (i=0; i<WSF_EFS_MAX_FILES; i++)
    {
      if (wsfEfsFileOverlap(i, media, address, size))
      {
        /* The offset overlaps another file.  Move the offset to the end of the overlapping */
        /* file and retest. */
        address = wsfEfsFileTbl[i].address + wsfEfsFileTbl[i].maxSize;
        address += address % wsfEfsMediaTbl[media]->pageSize;
        i = -1;

        if (address + size > wsfEfsMediaTbl[media]->endAddress)
        {
          return WSF_EFS_INVALID_OFFSET;
        }
      }
    }

    return address;
  }

  return WSF_EFS_INVALID_OFFSET;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the embedded file system.
 */
/*************************************************************************************************/
void WsfEfsInit(void)
{
  int8_t i;

  /* Clear control structures */
  memset((void *) wsfEfsMediaTbl, 0, sizeof(wsfEfsMediaTbl));

  /* Set maxSize to invalid indicating file is unused */
  for (i=0; i<WSF_EFS_MAX_FILES; i++)
  {
    wsfEfsFileTbl[i].maxSize = WSF_EFS_INVALID_SIZE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Create a file in the embedded file system.
 *
 *  \param  maxSize   Number of bytes to reserve for the file.
 *  \param  media     Identifier of the media where the file is stored.
 *  \param  pAttr     Attributes of the file
 *  \param  offset    Offset address of the file in the memory space.
 *
 *  \return File Handle, or WSF_EFS_INVALID_HANDLE if the operation failed.
 */
/*************************************************************************************************/
wsfEfsHandle_t WsfEfsAddFile(uint32_t maxSize, uint8_t media, wsfEsfAttributes_t *pAttr, uint32_t offset)
{
  wsfEfsHandle_t handle;
  efsAddr_t address = offset;
  wsfEfsControl_t *pFile = NULL;

  if (wsfEfsMediaValid(media))
  {
    /* Find an available slot in the file table (wsfEfsFileTbl) */
    for (handle = 0; handle < WSF_EFS_MAX_FILES; handle++)
    {
      if (wsfEfsFileTbl[handle].maxSize == WSF_EFS_INVALID_SIZE)
      {
        pFile = &wsfEfsFileTbl[handle];
        break;
      }
    }

    if (pFile)
    {
      if (address == WSF_EFS_FILE_OFFSET_ANY)
      {
        address = wsfEfsFindAvailableOffset(media, maxSize);

        if (address == WSF_EFS_INVALID_OFFSET)
        {
          return WSF_EFS_INVALID_HANDLE;
        }
      }
      else
      {
        int i;

        /* The user specifies a zero based offset when adding a file */
        /* This offset must be incremented by the media StartAddress */
        /* to become the file's address in the wsfEfsMediaTbl. */
        address += wsfEfsMediaTbl[media]->startAddress;

        /* Increment the address to a page boundry */
        if (wsfEfsMediaTbl[media]->pageSize)
        {
          address += address % wsfEfsMediaTbl[media]->pageSize;
        }

        /* Verify the given offset doesn't overlap another file */
        for (i=0; i<WSF_EFS_MAX_FILES; i++)
        {
          if (wsfEfsFileOverlap(i, media, address, maxSize) == TRUE)
          {
            return WSF_EFS_INVALID_HANDLE;
          }
        }
      }

      /* Fill in the file control structure */
      pFile->size = 0;
      pFile->media = media;
      pFile->address = address;
      memcpy(&pFile->attributes, pAttr, sizeof(wsfEsfAttributes_t));

      /* Set the maxSize, and increment the maxSize such that it */
      /* occupies a full page in the physical medium */
      pFile->maxSize = maxSize;
      if (wsfEfsMediaTbl[media]->pageSize)
      {
        pFile->maxSize += maxSize % wsfEfsMediaTbl[media]->pageSize;
      }

      return handle;
    }
  }

  return WSF_EFS_INVALID_HANDLE;
}

/*************************************************************************************************/
/*!
 *  \brief  Deletes a file in the embedded file system.
 *
 *  \param  handle    Handle identifying the file.
 *
 *  \return WSF_EFS_SUCCESS or WSF_EFS_FAILURE.
 */
/*************************************************************************************************/
uint8_t WsfEfsRemoveFile(wsfEfsHandle_t handle)
{
  if (handle < WSF_EFS_MAX_FILES)
  {
    wsfEfsFileTbl[handle].maxSize = WSF_EFS_INVALID_SIZE;
    return WSF_EFS_SUCCESS;
  }

  return WSF_EFS_FAILURE;
}

/*************************************************************************************************/
/*!
 *  \brief  Clears the contents of a file without deleting the file.
 *
 *  \param  handle    Handle identifying the file.
 *
 *  \return WSF_EFS_SUCCESS or WSF_EFS_FAILURE.
 */
/*************************************************************************************************/
uint8_t WsfEfsErase(wsfEfsHandle_t handle)
{
  uint8_t status = WSF_EFS_FAILURE;
  wsfEfsControl_t *pFile;

  if ((pFile = WsfEfsGetFileByHandle(handle)) != NULL)
  {
    if (pFile->attributes.permissions & (WSF_EFS_REMOTE_ERASE_PERMITTED | WSF_EFS_LOCAL_ERASE_PERMITTED))
    {
      uint8_t media = pFile->media;

      if (wsfEfsMediaTbl[media]->erase)
      {
        efsAddr_t address = pFile->address;
        uint32_t size = pFile->maxSize;

        status = wsfEfsMediaTbl[media]->erase(address, size);
        pFile->size = 0;
      }
    }
  }

  return status;
}

/*************************************************************************************************/
/*!
 *  \brief  Gets the attributes of a file.
 *
 *  \param  handle    Handle identifying the file.
 *  \param  pAttr     Pointer to memory to store the attributes.
 *
 *  \return WSF_EFS_SUCCESS or WSF_EFS_FAILURE.
 */
/*************************************************************************************************/
uint8_t WsfEfsGetAttributes(wsfEfsHandle_t handle, wsfEsfAttributes_t *pAttr)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL) && pAttr)
  {
    memcpy(pAttr, &pFile->attributes, sizeof(wsfEsfAttributes_t));
    return WSF_EFS_SUCCESS;
  }

  return WSF_EFS_FAILURE;
}

/*************************************************************************************************/
/*!
 *  \brief  Updates the attributes of a file.
 *
 *  \param  handle    Handle identifying the file.
 *  \param  pAttr     Pointer to memory to with the new attributes.
 *
 *  \return WSF_EFS_SUCCESS or WSF_EFS_FAILURE.
 */
/*************************************************************************************************/
uint8_t WsfEfsSetAttributes(wsfEfsHandle_t handle, wsfEsfAttributes_t *pAttr)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL) && pAttr)
  {
    memcpy(&pFile->attributes, pAttr, sizeof(wsfEsfAttributes_t));
    return WSF_EFS_SUCCESS;
  }

  return WSF_EFS_FAILURE;
}

/*************************************************************************************************/
/*!
 *  \brief  Copies data from a file.
 *
 *  \param  handle    Handle identifying the file.
 *  \param  offset    Offset into the file to begin copying from.
 *  \param  pBuffer   Location to copy the data to.
 *  \param  len       Number of bytes to copy into pBuffer.
 *
 *  \return The number of bytes read from the file.
 */
/*************************************************************************************************/
uint16_t WsfEfsGet(wsfEfsHandle_t handle, uint32_t offset, uint8_t *pBuffer, uint16_t len)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL) && pBuffer)
  {
    if (pFile->attributes.permissions & (WSF_EFS_REMOTE_GET_PERMITTED | WSF_EFS_LOCAL_GET_PERMITTED))
    {
      uint8_t media = pFile->media;

      if (wsfEfsMediaTbl[media]->read)
      {
        if (pFile->attributes.type == WSF_EFS_FILE_TYPE_STREAM)
        {
          wsfEfsMediaTbl[media]->read(pBuffer, 0, len);
          return len;
        }
        else if (offset < pFile->size)
        {
          efsAddr_t address = pFile->address + offset;

          if (offset + len > pFile->size)
          {
            len = (uint16_t) (pFile->size - offset);
          }

          wsfEfsMediaTbl[media]->read(pBuffer, address, len);

          return len;
        }
      }
    }
  }

  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Writes data to a file.
 *
 *  \param  handle    Handle identifying the file.
 *  \param  offset    Offset into the file to begin writing to.
 *  \param  pBuffer   Data to write to the file.
 *  \param  len       Number of bytes to write to the file.
 *
 *  \return The number of bytes written to the file
 */
/*************************************************************************************************/
uint16_t WsfEfsPut(wsfEfsHandle_t handle, uint32_t offset, const uint8_t *pBuffer, uint16_t len)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL) && pBuffer)
  {
    if (pFile->attributes.permissions & (WSF_EFS_REMOTE_PUT_PERMITTED | WSF_EFS_LOCAL_PUT_PERMITTED))
    {
      uint8_t media = pFile->media;

      if (wsfEfsMediaTbl[media]->write)
      {
        if (pFile->attributes.type == WSF_EFS_FILE_TYPE_STREAM)
        {
          wsfEfsMediaTbl[media]->write(pBuffer, 0, len);
          return len;
        }
        else if (offset < pFile->maxSize)
        {
          efsAddr_t address = pFile->address + offset;

          if (offset + len > pFile->maxSize)
          {
            len = (uint16_t) (pFile->maxSize - offset);
          }

          wsfEfsMediaTbl[media]->write(pBuffer, address, len);

          /* If writing to the end of the file, update the file size */
          if (offset + len > pFile->size)
          {
            pFile->size = offset + len;
          }

          return len;
        }
      }
    }
  }

  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the name of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Filename string of a file.
 */
/*************************************************************************************************/
char *WsfEfsGetFileName(wsfEfsHandle_t handle)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL))
  {
    return pFile->attributes.name;
  }

  return "Unknown";
}

/*************************************************************************************************/
/*!
 *  \brief  Get the version of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Version string of a file.
 */
/*************************************************************************************************/
char *WsfEfsGetFileVersion(wsfEfsHandle_t handle)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL))
  {
    return pFile->attributes.version;
  }

  return "Unknown";
}

/*************************************************************************************************/
/*!
 *  \brief  Get the number of bytes of memory reserved for use by a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Max size of the file.
 */
/*************************************************************************************************/
uint32_t WsfEfsGetFileMaxSize(wsfEfsHandle_t handle)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL))
  {
    return pFile->maxSize;
  }

  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the size of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Size of the file.
 */
/*************************************************************************************************/
uint32_t WsfEfsGetFileSize(wsfEfsHandle_t handle)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL))
  {
    return pFile->size;
  }

  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the type of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Type of file (bulk or stream).
 */
/*************************************************************************************************/
uint8_t WsfEfsGetFileType(wsfEfsHandle_t handle)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL))
  {
    return pFile->attributes.type;
  }

  return WSF_EFS_FILE_TYPE_BULK;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the permissions of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Permissions of the file.
 */
/*************************************************************************************************/
uint16_t WsfEfsGetFilePermissions(wsfEfsHandle_t handle)
{
  wsfEfsControl_t *pFile;

  if (((pFile = WsfEfsGetFileByHandle(handle)) != NULL))
  {
    return pFile->attributes.permissions;
  }

  return 0;
}

/*************************************************************************************************/
/*!
 *  \brief  Execute a media specific command on a file.
 *
 *  \param  handle    File Handle.
 *  \param  cmd       Media specific command identifier.
 *  \param  param     Ccommand specific parameter.
 *
 *  \return Status of the operation.
 */
/*************************************************************************************************/
uint8_t WsfEfsMediaSpecificCommand(wsfEfsHandle_t handle, uint8_t cmd, uint32_t param)
{
  if (WsfEfsGetFileByHandle(handle))
  {
    uint8_t media = wsfEfsFileTbl[handle].media;

    /* Call the command handler if it exists */
    if (wsfEfsMediaTbl[media]->handleCmd)
    {
      return wsfEfsMediaTbl[media]->handleCmd(cmd, param);
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Registers a File Storage Medium with the Embedded File System.
 *
 *  \param  pMedia    Pointer to the media control structure.
 *  \param  mediaID   Identifier of the media.
 *
 *  \return WSF_EFS_SUCCESS or WSF_EFS_FAILURE.
 */
/*************************************************************************************************/
uint8_t WsfEfsRegisterMedia(const wsfEfsMedia_t *pMediaCtrl, uint8_t mediaID)
{
  if (mediaID < WSF_EFS_MAX_MEDIA && pMediaCtrl)
  {
    wsfEfsMediaTbl[mediaID] = pMediaCtrl;

    /* Call the initialization function for the media */
    if (pMediaCtrl->init)
    {
      pMediaCtrl->init();
    }

    return WSF_EFS_SUCCESS;
  }

  return WSF_EFS_FAILURE;
}
