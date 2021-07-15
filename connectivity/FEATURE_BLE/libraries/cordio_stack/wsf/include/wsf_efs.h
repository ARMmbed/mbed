/*************************************************************************************************/
/*!
 *  \file   wsf_efs.h
 *
 *  \brief  Embedded File System service.
 *
 *  Copyright (c) 2014-2018 Arm Ltd. All Rights Reserved.
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

#ifndef WSF_EFS_H
#define WSF_EFS_H

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup WSF_EFS_API
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief Max Number of Files and Media */
#ifndef WSF_EFS_MAX_FILES
#define WSF_EFS_MAX_FILES                   6
#endif

/*! \brief Max Number of Media */
#ifndef WSF_EFS_MAX_MEDIA
#define WSF_EFS_MAX_MEDIA                   4
#endif

/** \name Status Codes
 *
 */
/**@{*/
#define WSF_EFS_SUCCESS                     0             /*!< \brief Success */
#define WSF_EFS_FAILURE                     1             /*!< \brief Failure */
#define WSF_EFS_CBACK_REQUIRED              2             /*!< \brief File sytem callback required */
#define WSF_EFS_GET_FAILED                  0xFFFF        /*!< \brief Get operation failure */
#define WSF_EFS_PUT_FAILED                  0xFFFF        /*!< \brief PUT operation failure */
/**@}*/


/** \name  Invalid Parameter Identifiers
 *
 */
/**@{*/
#define WSF_EFS_INVALID_HANDLE              0xFFFF        /*!< \brief Invalid Handle */
#define WSF_EFS_INVALID_OFFSET              0xFFFFFFFF    /*!< \brief Invalid Offset */
#define WSF_EFS_INVALID_SIZE                0xFFFFFFFF    /*!< \brief Invalid Size */
#define WSF_EFS_INVALID_MEDIA               0xFF          /*!< \brief Invalid Media */
/**@}*/

/** \name File Types
 *
 */
/**@{*/
#define WSF_EFS_FILE_TYPE_BULK              0             /*!< \brief Bulk File Type */
#define WSF_EFS_FILE_TYPE_STREAM            1             /*!< \brief Stream File Type */
/**@}*/

/*! \brief Offset to WsfEfsAddFile indicating any file offset can be used */
#define WSF_EFS_FILE_OFFSET_ANY             0xFFFFFFFF

/** \name File Permissions
 *
 */
/**@{*/
#define WSF_EFS_REMOTE_PERMISSIONS_MASK     0xFF          /*!< \brief Remote Permissions */
#define WSF_EFS_REMOTE_GET_PERMITTED        0x01          /*!< \brief Remote Get Permitted */
#define WSF_EFS_REMOTE_PUT_PERMITTED        0x02          /*!< \brief Remote Put Permitted */
#define WSF_EFS_REMOTE_ERASE_PERMITTED      0x04          /*!< \brief Remote Erase Permitted */
#define WSF_EFS_REMOTE_VERIFY_PERMITTED     0x08          /*!< \brief Remote Verify Permitted */
#define WSF_EFS_LOCAL_GET_PERMITTED         0x0100        /*!< \brief Local Get Permitted */
#define WSF_EFS_LOCAL_PUT_PERMITTED         0x0200        /*!< \brief Local Put Permitted */
#define WSF_EFS_LOCAL_ERASE_PERMITTED       0x0400        /*!< \brief Local Erase Permitted */
#define WSF_EFS_REMOTE_VISIBLE              0x0800        /*!< \brief File Visible via Remote WDXS */
/**@}*/

/*! \brief File name length in bytes */
#define WSF_EFS_NAME_LEN                    16

/*! \brief File version length in bytes */
#define WSF_EFS_VERSION_LEN                 16

/** \name Standard Media Specific Command Identifiers
 *
 */
/**@{*/
#define WSF_EFS_WDXS_PUT_COMPLETE_CMD       0x00          /*!< \brief Put Complete */
#define WSF_EFS_VALIDATE_CMD                0x01          /*!< \brief Validate Req for the file */
#define WSF_EFS_RESTORE_ATTRS_CMD           0x02          /*!< \brief Restore file attributes */
#define WSF_EFS_GET_FS_CBACK_MODE_CMD       0x03          /*!< \brief Filesystem callback check */
#define WSF_EFS_CFG_FOTA_CMD                0x04          /*!< \brief Configure FOTA and reboot */
/**@}*/

/*! \brief Media Specific Command Identifiers reserved for applications begin at 0x80 */
#define WSF_EFS_USER_CMD                    0x80

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* EFS address data type */
#if (defined CPU_64BIT) && (CPU_64BIT == 1)
typedef uint64_t efsAddr_t;
#else
typedef uint32_t efsAddr_t;
#endif

/*! \brief File handle data type */
typedef uint16_t wsfEfsHandle_t;

/*! \brief File attributes data type */
typedef struct
{
  char     name[WSF_EFS_NAME_LEN];       /*!< \brief File name string. */
  char     version[WSF_EFS_VERSION_LEN]; /*!< \brief File version string. */
  uint16_t permissions;                  /*!< \brief File permissions. */
  uint8_t  type;                         /*!< \brief File type. */
} wsfEsfAttributes_t;

/*! \brief File control block data type */
typedef struct
{
  uint32_t           maxSize;    /*!< \brief File maximum size. */
  efsAddr_t          address;    /*!< \brief File storage address. */
  uint8_t            media;      /*!< \brief File media. */
  uint32_t           size;       /*!< \brief File size. */
  wsfEsfAttributes_t attributes; /*!< \brief File attributes. */
} wsfEfsControl_t;

/*! \brief File Listing Information */
typedef struct
{
  wsfEfsHandle_t     handle;     /*!< \brief File handle. */
  uint32_t           size;       /*!< \brief File size. */
  wsfEsfAttributes_t attributes; /*!< \brief File attributes. */
} wsfEfsFileInfo_t;

/*************************************************************************************************/
/*!
 *  \brief  Media Init function, called when media is registered.
 *
 *  \return Status of the operation.
 */
/*************************************************************************************************/
typedef uint8_t wsfMediaInitFunc_t(void);

/*************************************************************************************************/
/*!
 *  \brief  Media Erase function.
 *
 *  \param  pAddress Address in media to start erasing.
 *  \param  size     Number of bytes to erase.
 *
 *  \return Status of the operation.
 */
/*************************************************************************************************/
typedef uint8_t wsfMediaEraseFunc_t(efsAddr_t pAddress, uint32_t size);

/*************************************************************************************************/
/*!
 *  \brief  Media Read function.
 *
 *  \param  pBuf     Buffer to hold data.
 *  \param  pAddress Address in media to read from.
 *  \param  size     Size of pBuf in bytes.
 *
 *  \return Status of the operation.
 */
/*************************************************************************************************/
typedef uint8_t wsfMediaReadFunc_t(uint8_t *pBuf, efsAddr_t pAddress, uint32_t size);

/*************************************************************************************************/
/*!
 *  \brief  Media Write function.
 *
 *  \param  pBuf     Buffer with data to be written.
 *  \param  pAddress Address in media to write to.
 *  \param  size     Size of pBuf in bytes.
 *
 *  \return Status of the operation.
 */
/*************************************************************************************************/
typedef uint8_t wsfMediaWriteFunc_t(const uint8_t *pBuf, efsAddr_t pAddress, uint32_t size);

/*************************************************************************************************/
/*!
 *  \brief  Media Specific Command handler.
 *
 *  \param  cmd    Identifier of the media specific command.
 *  \param  param  Optional Parameter to the command.
 *
 *  \return Status of the operation.
 */
/*************************************************************************************************/
typedef uint8_t wsfMediaHandleCmdFunc_t(uint8_t cmd, uint32_t param);

/*! \brief Media Control data type */
typedef struct
{
  efsAddr_t               startAddress;  /*!< \brief Start address. */
  efsAddr_t               endAddress;    /*!< \brief End address. */
  efsAddr_t               pageSize;      /*!< \brief Page size. */
  wsfMediaInitFunc_t      *init;         /*!< \brief Media intialization callback. */
  wsfMediaEraseFunc_t     *erase;        /*!< \brief Media erase callback. */
  wsfMediaReadFunc_t      *read;         /*!< \brief Media read callback. */
  wsfMediaWriteFunc_t     *write;        /*!< \brief Media write callback. */
  wsfMediaHandleCmdFunc_t *handleCmd;    /*!< \brief Media command handler callback. */
} wsfEfsMedia_t;

/*! \brief Pointer to Media Control data type */
typedef const wsfEfsMedia_t *pWsfEfsMedia_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Initialise the embedded file system.
 *
 *  \return none.
 */
/*************************************************************************************************/
void WsfEfsInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Create a file in the embedded file system.
 *
 *  \param  maxSize   Max length in bytes of of the file.
 *  \param  media     Identifier of the media where the file is stored.
 *  \param  pAttr     Attributes of the file
 *  \param  offset    Offset address of the file in the memory space.
 *
 *  \return File Handle, or WSF_EFS_INVALID_HANDLE.
 */
/*************************************************************************************************/
wsfEfsHandle_t WsfEfsAddFile(uint32_t maxSize, uint8_t media, wsfEsfAttributes_t *pAttr, uint32_t offset);

/*************************************************************************************************/
/*!
 *  \brief  Deletes a file in the embedded file system.
 *
 *  \param  handle    Handle identifying the file.
 *
 *  \return WSF_EFS_SUCCESS or WSF_EFS_FAILURE.
 */
/*************************************************************************************************/
uint8_t WsfEfsRemoveFile(wsfEfsHandle_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Clears the contents of a file without deleting the file.
 *
 *  \param  handle    Handle identifying the file.
 *
 *  \return WSF_EFS_SUCCESS or WSF_EFS_FAILURE.
 */
/*************************************************************************************************/
uint8_t WsfEfsErase(wsfEfsHandle_t handle);

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
uint8_t WsfEfsGetAttributes(wsfEfsHandle_t handle, wsfEsfAttributes_t *pAttr);

/*************************************************************************************************/
/*!
 *  \brief  Updates the attributes of a file.
 *
 *  \param  handle    Handle identifying the file.
 *  \param  pInfo     Pointer to memory to with the updated attributes.
 *
 *  \return WSF_EFS_SUCCESS or WSF_EFS_FAILURE.
 */
/*************************************************************************************************/
uint8_t WsfEfsSetAttributes(wsfEfsHandle_t handle, wsfEsfAttributes_t *pInfo);

/*************************************************************************************************/
/*!
 *  \brief  Copies data from a file.
 *
 *  \param  handle    Handle identifying the file.
 *  \param  offset    Offset into the file to begin copying from.
 *  \param  pBuffer   Location to copy the data to.
 *  \param  len       Number of bytes to copy into pBuffer.
 *
 *  \return The number of bytes read from the file
 */
/*************************************************************************************************/
uint16_t WsfEfsGet(wsfEfsHandle_t handle, uint32_t offset, uint8_t *pBuffer, uint16_t len);

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
uint16_t WsfEfsPut(wsfEfsHandle_t handle, uint32_t offset, const uint8_t *pBuffer, uint16_t len);

/*************************************************************************************************/
/*!
 *  \brief  Registers a File Storage Medium with the Embedded File System.
 *
 *  \param  pMediaCtrl    Pointer to the media control structure.
 *  \param  mediaID       User specified identifier of the media.
 *
 *  \return WSF_EFS_SUCCESS or WSF_EFS_FAILURE.
 */
/*************************************************************************************************/
uint8_t WsfEfsRegisterMedia(const wsfEfsMedia_t *pMediaCtrl, uint8_t mediaID);

/*************************************************************************************************/
/*!
 *  \brief  Returns the file control block for the given handle.
 *
 *  \param  handle   Handle of the file
 *
 *  \return File control block, or NULL.
 */
/*************************************************************************************************/
wsfEfsControl_t *WsfEfsGetFileByHandle(wsfEfsHandle_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get the name of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Filename string of a file.
 */
/*************************************************************************************************/
char *WsfEfsGetFileName(wsfEfsHandle_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get the version of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Version string of a file.
 */
/*************************************************************************************************/
char *WsfEfsGetFileVersion(wsfEfsHandle_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get the size of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Size of the file.
 */
/*************************************************************************************************/
uint32_t WsfEfsGetFileSize(wsfEfsHandle_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get the number of bytes of memory reserved for use by a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Max size of the file.
 */
/*************************************************************************************************/
uint32_t WsfEfsGetFileMaxSize(wsfEfsHandle_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get the type of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Type of file (bulk or stream).
 */
/*************************************************************************************************/
uint8_t WsfEfsGetFileType(wsfEfsHandle_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get the permissions of a file.
 *
 *  \param  handle    File Handle.
 *
 *  \return Permissions of the file.
 */
/*************************************************************************************************/
uint16_t WsfEfsGetFilePermissions(wsfEfsHandle_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Execute a media specific command on a file.
 *
 *  \param  handle    File Handle.
 *  \param  cmd       Media specific command identifier.
 *  \param  param     Command specific parameter.
 *
 *  \return Status of the operation.
 */
/*************************************************************************************************/
uint8_t WsfEfsMediaSpecificCommand(wsfEfsHandle_t handle, uint8_t cmd, uint32_t param);

/*! \} */    /* WSF_EFS_API */

#ifdef __cplusplus
}
#endif

#endif /* WSF_EFS_H */
