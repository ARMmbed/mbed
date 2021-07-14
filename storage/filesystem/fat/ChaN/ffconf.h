/*---------------------------------------------------------------------------/
/  FatFs Functional Configurations
/---------------------------------------------------------------------------*/

#define FFCONF_DEF	86631	/* Revision ID */

#define FFS_DBG			MBED_CONF_FAT_CHAN_FFS_DBG

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY	MBED_CONF_FAT_CHAN_FF_FS_READONLY
/* This option switches read-only configuration. (0:Read/Write or 1:Read-only)
/  Read-only configuration removes writing API functions, f_write(), f_sync(),
/  f_unlink(), f_mkdir(), f_chmod(), f_rename(), f_truncate(), f_getfree()
/  and optional writing functions as well. */


#define FF_FS_MINIMIZE	MBED_CONF_FAT_CHAN_FF_FS_MINIMIZE
/* This option defines minimization level to remove some basic API functions.
/
/   0: Basic functions are fully enabled.
/   1: f_stat(), f_getfree(), f_unlink(), f_mkdir(), f_truncate() and f_rename()
/      are removed.
/   2: f_opendir(), f_readdir() and f_closedir() are removed in addition to 1.
/   3: f_lseek() function is removed in addition to 2. */


#define FF_USE_FIND		MBED_CONF_FAT_CHAN_FF_USE_FIND
/* This option switches filtered directory read functions, f_findfirst() and
/  f_findnext(). (0:Disable, 1:Enable 2:Enable with matching altname[] too) */


#define FF_USE_MKFS		MBED_CONF_FAT_CHAN_FF_USE_MKFS
/* This option switches f_mkfs() function. (0:Disable or 1:Enable) */


#define FF_USE_FASTSEEK	MBED_CONF_FAT_CHAN_FF_USE_FASTSEEK
/* This option switches fast seek function. (0:Disable or 1:Enable) */


#define FF_USE_EXPAND	MBED_CONF_FAT_CHAN_FF_USE_EXPAND
/* This option switches f_expand function. (0:Disable or 1:Enable) */


#define FF_USE_CHMOD	MBED_CONF_FAT_CHAN_FF_USE_CHMOD
/* This option switches attribute manipulation functions, f_chmod() and f_utime().
/  (0:Disable or 1:Enable) Also FF_FS_READONLY needs to be 0 to enable this option. */


#define FF_USE_LABEL	MBED_CONF_FAT_CHAN_FF_USE_LABEL
/* This option switches volume label functions, f_getlabel() and f_setlabel().
/  (0:Disable or 1:Enable) */


#define FF_USE_FORWARD	MBED_CONF_FAT_CHAN_FF_USE_FORWARD
/* This option switches f_forward() function. (0:Disable or 1:Enable) */


#define FF_USE_STRFUNC	MBED_CONF_FAT_CHAN_FF_USE_STRFUNC
#define FF_PRINT_LLI	MBED_CONF_FAT_CHAN_FF_PRINT_LLI
#define FF_PRINT_FLOAT	MBED_CONF_FAT_CHAN_FF_PRINT_FLOAT
#define FF_STRF_ENCODE	MBED_CONF_FAT_CHAN_FF_STRF_ENCODE
/* FF_USE_STRFUNC switches string functions, f_gets(), f_putc(), f_puts() and
/  f_printf().
/
/   0: Disable. FF_PRINT_LLI, FF_PRINT_FLOAT and FF_STRF_ENCODE have no effect.
/   1: Enable without LF-CRLF conversion.
/   2: Enable with LF-CRLF conversion.
/
/  FF_PRINT_LLI = 1 makes f_printf() support long long argument and FF_PRINT_FLOAT = 1/2
   makes f_printf() support floating point argument. These features want C99 or later.
/  When FF_LFN_UNICODE >= 1 with LFN enabled, string functions convert the character
/  encoding in it. FF_STRF_ENCODE selects assumption of character encoding ON THE FILE
/  to be read/written via those functions.
/
/   0: ANSI/OEM in current CP
/   1: Unicode in UTF-16LE
/   2: Unicode in UTF-16BE
/   3: Unicode in UTF-8
*/


/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE	MBED_CONF_FAT_CHAN_FF_CODE_PAGE
/* This option specifies the OEM code page to be used on the target system.
/  Incorrect code page setting can cause a file open failure.
/
/   437 - U.S.
/   720 - Arabic
/   737 - Greek
/   771 - KBL
/   775 - Baltic
/   850 - Latin 1
/   852 - Latin 2
/   855 - Cyrillic
/   857 - Turkish
/   860 - Portuguese
/   861 - Icelandic
/   862 - Hebrew
/   863 - Canadian French
/   864 - Arabic
/   865 - Nordic
/   866 - Russian
/   869 - Greek 2
/   932 - Japanese (DBCS)
/   936 - Simplified Chinese (DBCS)
/   949 - Korean (DBCS)
/   950 - Traditional Chinese (DBCS)
/     0 - Include all code pages above and configured by f_setcp()
*/


#define FF_USE_LFN		MBED_CONF_FAT_CHAN_FF_USE_LFN
#define FF_MAX_LFN		MBED_CONF_FAT_CHAN_FF_MAX_LFN
/* The FF_USE_LFN switches the support for LFN (long file name).
/
/   0: Disable LFN. FF_MAX_LFN has no effect.
/   1: Enable LFN with static  working buffer on the BSS. Always NOT thread-safe.
/   2: Enable LFN with dynamic working buffer on the STACK.
/   3: Enable LFN with dynamic working buffer on the HEAP.
/
/  To enable the LFN, ffunicode.c needs to be added to the project. The LFN function
/  requiers certain internal working buffer occupies (FF_MAX_LFN + 1) * 2 bytes and
/  additional (FF_MAX_LFN + 44) / 15 * 32 bytes when exFAT is enabled.
/  The FF_MAX_LFN defines size of the working buffer in UTF-16 code unit and it can
/  be in range of 12 to 255. It is recommended to be set it 255 to fully support LFN
/  specification.
/  When use stack for the working buffer, take care on stack overflow. When use heap
/  memory for the working buffer, memory management functions, ff_memalloc() and
/  ff_memfree() exemplified in ffsystem.c, need to be added to the project. */


#define FF_LFN_UNICODE	MBED_CONF_FAT_CHAN_FF_LFN_UNICODE
/* This option switches the character encoding on the API when LFN is enabled.
/
/   0: ANSI/OEM in current CP (TCHAR = char)
/   1: Unicode in UTF-16 (TCHAR = WCHAR)
/   2: Unicode in UTF-8 (TCHAR = char)
/   3: Unicode in UTF-32 (TCHAR = DWORD)
/
/  Also behavior of string I/O functions will be affected by this option.
/  When LFN is not enabled, this option has no effect. */


#define FF_LFN_BUF		MBED_CONF_FAT_CHAN_FF_LFN_BUF
#define FF_SFN_BUF		MBED_CONF_FAT_CHAN_FF_SFN_BUF
/* This set of options defines size of file name members in the FILINFO structure
/  which is used to read out directory items. These values should be suffcient for
/  the file names to read. The maximum possible length of the read file name depends
/  on character encoding. When LFN is not enabled, these options have no effect. */


#define FF_FS_RPATH		MBED_CONF_FAT_CHAN_FF_FS_RPATH
/* This option configures support for relative path.
/
/   0: Disable relative path and remove related functions.
/   1: Enable relative path. f_chdir() and f_chdrive() are available.
/   2: f_getcwd() function is available in addition to 1.
*/


/*---------------------------------------------------------------------------/
/ Drive/Volume Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES		MBED_CONF_FAT_CHAN_FF_VOLUMES
/* Number of volumes (logical drives) to be used. (1-10) */


#define FF_STR_VOLUME_ID	MBED_CONF_FAT_CHAN_FF_STR_VOLUME_ID
#define FF_VOLUME_STRS		MBED_CONF_FAT_CHAN_FF_VOLUME_STRS
/* FF_STR_VOLUME_ID switches support for volume ID in arbitrary strings.
/  When FF_STR_VOLUME_ID is set to 1 or 2, arbitrary strings can be used as drive
/  number in the path name. FF_VOLUME_STRS defines the volume ID strings for each
/  logical drives. Number of items must not be less than FF_VOLUMES. Valid
/  characters for the volume ID strings are A-Z, a-z and 0-9, however, they are
/  compared in case-insensitive. If FF_STR_VOLUME_ID >= 1 and FF_VOLUME_STRS is
/  not defined, a user defined volume string table needs to be defined as:
/
/  const char* VolumeStr[FF_VOLUMES] = {"ram","flash","sd","usb",...
*/


#define FF_MULTI_PARTITION	MBED_CONF_FAT_CHAN_FF_MULTI_PARTITION
/* This option switches support for multiple volumes on the physical drive.
/  By default (0), each logical drive number is bound to the same physical drive
/  number and only an FAT volume found on the physical drive will be mounted.
/  When this function is enabled (1), each logical drive number can be bound to
/  arbitrary physical drive and partition listed in the VolToPart[]. Also f_fdisk()
/  funciton will be available. */


#define FF_MIN_SS		MBED_CONF_FAT_CHAN_FF_MIN_SS
#define FF_MAX_SS		MBED_CONF_FAT_CHAN_FF_MAX_SS
/* This set of options configures the range of sector size to be supported. (512,
/  1024, 2048 or 4096) Always set both 512 for most systems, generic memory card and
/  harddisk, but a larger value may be required for on-board flash memory and some
/  type of optical media. When FF_MAX_SS is larger than FF_MIN_SS, FatFs is configured
/  for variable sector size mode and disk_ioctl() function needs to implement
/  GET_SECTOR_SIZE command. */


#define FF_LBA64		0
/* This option switches support for 64-bit LBA. (0:Disable or 1:Enable)
/  To enable the 64-bit LBA, also exFAT needs to be enabled. (FF_FS_EXFAT == 1) */


#define FF_MIN_GPT		0x10000000
/* Minimum number of sectors to switch GPT as partitioning format in f_mkfs and
/  f_fdisk function. 0x100000000 max. This option has no effect when FF_LBA64 == 0. */


#define FF_USE_TRIM		MBED_CONF_FAT_CHAN_FF_USE_TRIM
/* This option switches support for ATA-TRIM. (0:Disable or 1:Enable)
/  To enable Trim function, also CTRL_TRIM command should be implemented to the
/  disk_ioctl() function. */



/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY		MBED_CONF_FAT_CHAN_FF_FS_TINY
/* This option switches tiny buffer configuration. (0:Normal or 1:Tiny)
/  At the tiny configuration, size of file object (FIL) is shrinked FF_MAX_SS bytes.
/  Instead of private sector buffer eliminated from the file object, common sector
/  buffer in the filesystem object (FATFS) is used for the file data transfer. */


#define FF_FS_EXFAT		MBED_CONF_FAT_CHAN_FF_FS_EXFAT
/* This option switches support for exFAT filesystem. (0:Disable or 1:Enable)
/  To enable exFAT, also LFN needs to be enabled. (FF_USE_LFN >= 1)
/  Note that enabling exFAT discards ANSI C (C89) compatibility. */


#define FF_FS_HEAPBUF   MBED_CONF_FAT_CHAN_FF_FS_HEAPBUF
/* This option enables the use of the heap for allocating buffers. Otherwise
/  _MAX_SS sized buffers are allocated statically in relevant structures (in
/  FATFS if _FS_TINY, otherwise in FATFS and FIL)
/  This option allows the filesystem to dynamically allocate the buffers based
/  on underlying sector size. */


#define FF_FS_NORTC		MBED_CONF_FAT_CHAN_FF_FS_NORTC
#define FF_NORTC_MON	MBED_CONF_FAT_CHAN_FF_NORTC_MON
#define FF_NORTC_MDAY	MBED_CONF_FAT_CHAN_FF_NORTC_MDAY
#define FF_NORTC_YEAR	MBED_CONF_FAT_CHAN_FF_NORTC_YEAR
/* The option FF_FS_NORTC switches timestamp functiton. If the system does not have
/  any RTC function or valid timestamp is not needed, set FF_FS_NORTC = 1 to disable
/  the timestamp function. Every object modified by FatFs will have a fixed timestamp
/  defined by FF_NORTC_MON, FF_NORTC_MDAY and FF_NORTC_YEAR in local time.
/  To enable timestamp function (FF_FS_NORTC = 0), get_fattime() function need to be
/  added to the project to read current time form real-time clock. FF_NORTC_MON,
/  FF_NORTC_MDAY and FF_NORTC_YEAR have no effect.
/  These options have no effect in read-only configuration (FF_FS_READONLY = 1). */


#define FF_FS_NOFSINFO	MBED_CONF_FAT_CHAN_FF_FS_NOFSINFO
/* If you need to know correct free space on the FAT32 volume, set bit 0 of this
/  option, and f_getfree() function at first time after volume mount will force
/  a full FAT scan. Bit 1 controls the use of last allocated cluster number.
/
/  bit0=0: Use free cluster count in the FSINFO if available.
/  bit0=1: Do not trust free cluster count in the FSINFO.
/  bit1=0: Use last allocated cluster number in the FSINFO if available.
/  bit1=1: Do not trust last allocated cluster number in the FSINFO.
*/


#define FF_FS_LOCK		MBED_CONF_FAT_CHAN_FF_FS_LOCK
/* The option FF_FS_LOCK switches file lock function to control duplicated file open
/  and illegal operation to open objects. This option must be 0 when FF_FS_READONLY
/  is 1.
/
/  0:  Disable file lock function. To avoid volume corruption, application program
/      should avoid illegal open, remove and rename to the open objects.
/  >0: Enable file lock function. The value defines how many files/sub-directories
/      can be opened simultaneously under file lock control. Note that the file
/      lock control is independent of re-entrancy. */


#define FF_FS_REENTRANT	MBED_CONF_FAT_CHAN_FF_FS_REENTRANT
#define FF_FS_TIMEOUT	MBED_CONF_FAT_CHAN_FF_FS_TIMEOUT
#define FF_SYNC_t		MBED_CONF_FAT_CHAN_FF_SYNC_t
/* The option FF_FS_REENTRANT switches the re-entrancy (thread safe) of the FatFs
/  module itself. Note that regardless of this option, file access to different
/  volume is always re-entrant and volume control functions, f_mount(), f_mkfs()
/  and f_fdisk() function, are always not re-entrant. Only file/directory access
/  to the same volume is under control of this function.
/
/   0: Disable re-entrancy. FF_FS_TIMEOUT and FF_SYNC_t have no effect.
/   1: Enable re-entrancy. Also user provided synchronization handlers,
/      ff_req_grant(), ff_rel_grant(), ff_del_syncobj() and ff_cre_syncobj()
/      function, must be added to the project. Samples are available in
/      option/syscall.c.
/
/  The FF_FS_TIMEOUT defines timeout period in unit of time tick.
/  The FF_SYNC_t defines O/S dependent sync object type. e.g. HANDLE, ID, OS_EVENT*,
/  SemaphoreHandle_t and etc. A header file for O/S definitions needs to be
/  included somewhere in the scope of ff.h. */

/* #include <windows.h>	// O/S definitions  */

#define FLUSH_ON_NEW_CLUSTER    MBED_CONF_FAT_CHAN_FLUSH_ON_NEW_CLUSTER   /* Sync the file on every new cluster */
#define FLUSH_ON_NEW_SECTOR     MBED_CONF_FAT_CHAN_FLUSH_ON_NEW_SECTOR   /* Sync the file on every new sector */
/* Only one of these two defines needs to be set to 1. If both are set to 0
   the file is only sync when closed.
   Clusters are group of sectors (eg: 8 sectors). Flushing on new cluster means
   it would be less often than flushing on new sector. Sectors are generally
   512 Bytes long. */


/*--- End of configuration options ---*/
