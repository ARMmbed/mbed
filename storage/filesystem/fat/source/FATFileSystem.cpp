/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "fat/ChaN/ffconf.h"
#include "fat/ChaN/ff.h"
#include "fat/ChaN/diskio.h"
#include "platform/mbed_debug.h"
#include "platform/mbed_critical.h"
#include "filesystem/mbed_filesystem.h"
#include "FATFileSystem.h"

#include <errno.h>
#include <stdlib.h>

namespace mbed {

using namespace mbed;

static int fat_error_remap(FRESULT res)
{
    switch (res) {
        case FR_OK:                   // (0) Succeeded
            return 0;
        case FR_DISK_ERR:             // (1) A hard error occurred in the low level disk I/O layer
            return -EIO;
        case FR_INT_ERR:              // (2) Assertion failed
            return -1;
        case FR_NOT_READY:            // (3) The physical drive cannot work
            return -EIO;
        case FR_NO_FILE:              // (4) Could not find the file
            return -ENOENT;
        case FR_NO_PATH:              // (5) Could not find the path
            return -ENOTDIR;
        case FR_INVALID_NAME:         // (6) The path name format is invalid
            return -EINVAL;
        case FR_DENIED:               // (7) Access denied due to prohibited access or directory full
            return -EACCES;
        case FR_EXIST:                // (8) Access denied due to prohibited access
            return -EEXIST;
        case FR_INVALID_OBJECT:       // (9) The file/directory object is invalid
            return -EBADF;
        case FR_WRITE_PROTECTED:      // (10) The physical drive is write protected
            return -EACCES;
        case FR_INVALID_DRIVE:        // (11) The logical drive number is invalid
            return -ENODEV;
        case FR_NOT_ENABLED:          // (12) The volume has no work area
            return -ENODEV;
        case FR_NO_FILESYSTEM:        // (13) There is no valid FAT volume
            return -EINVAL;
        case FR_MKFS_ABORTED:         // (14) The f_mkfs() aborted due to any problem
            return -EIO;
        case FR_TIMEOUT:              // (15) Could not get a grant to access the volume within defined period
            return -ETIMEDOUT;
        case FR_LOCKED:               // (16) The operation is rejected according to the file sharing policy
            return -EBUSY;
        case FR_NOT_ENOUGH_CORE:      // (17) LFN working buffer could not be allocated
            return -ENOMEM;
        case FR_TOO_MANY_OPEN_FILES:  // (18) Number of open files > FF_FS_LOCK
            return -ENFILE;
        case FR_INVALID_PARAMETER:    // (19) Given parameter is invalid
            return -EINVAL;
        default:
            return -res;
    }
}

// Helper class for deferring operations when variable falls out of scope
template <typename T>
class Deferred {
public:
    T _t;
    Callback<void(T)> _ondefer;

    Deferred(const Deferred &);
    Deferred &operator=(const Deferred &);

public:
    Deferred(T t, Callback<void(T)> ondefer = nullptr)
        : _t(t), _ondefer(ondefer)
    {
    }

    operator T()
    {
        return _t;
    }

    ~Deferred()
    {
        if (_ondefer) {
            _ondefer(_t);
        }
    }
};

static void dodelete(const char *data)
{
    delete[] data;
}

// Adds prefix needed internally by fatfs, this can be avoided for the first fatfs
// (id 0) otherwise a prefix of "id:/" is inserted in front of the string.
static Deferred<const char *> fat_path_prefix(int id, const char *path)
{
    // We can avoid dynamic allocation when only on fatfs is in use
    if (id == 0) {
        return path;
    }

    // Prefix path with id, will look something like 2:/hi/hello/filehere.txt
    char *buffer = new char[strlen("0:/") + strlen(path) + 1];
    if (!buffer) {
        return NULL;
    }

    buffer[0] = '0' + id;
    buffer[1] = ':';
    buffer[2] = '/';
    strcpy(buffer + strlen("0:/"), path);
    return Deferred<const char *>(buffer, dodelete);
}

////// Disk operations //////

// Global access to block device from FAT driver
static mbed::BlockDevice *_ffs[FF_VOLUMES] = {0};
static SingletonPtr<PlatformMutex> _ffs_mutex;

// FAT driver functions
extern "C" DWORD get_fattime(void)
{
    time_t rawtime;
    time(&rawtime);
    struct tm *ptm = localtime(&rawtime);
    return (DWORD)(ptm->tm_year - 80) << 25
           | (DWORD)(ptm->tm_mon + 1) << 21
           | (DWORD)(ptm->tm_mday) << 16
           | (DWORD)(ptm->tm_hour) << 11
           | (DWORD)(ptm->tm_min) << 5
           | (DWORD)(ptm->tm_sec / 2);
}

extern "C" void *ff_memalloc(UINT size)
{
    return malloc(size);
}

extern "C" void ff_memfree(void *p)
{
    free(p);
}

// Implementation of diskio functions (see ChaN/diskio.h)
static WORD disk_get_sector_size(BYTE pdrv)
{
    bd_size_t sector_size = _ffs[pdrv]->get_erase_size();
    MBED_ASSERT(sector_size <= WORD(-1));

    WORD ssize = sector_size;
    if (ssize < 512) {
        ssize = 512;
    }

    MBED_ASSERT(ssize >= FF_MIN_SS && ssize <= FF_MAX_SS);
    MBED_ASSERT(_ffs[pdrv]->get_read_size() <= _ffs[pdrv]->get_erase_size());
    MBED_ASSERT(_ffs[pdrv]->get_program_size() <= _ffs[pdrv]->get_erase_size());
    return ssize;
}

static DWORD disk_get_sector_count(BYTE pdrv)
{
    DWORD scount = _ffs[pdrv]->size() / disk_get_sector_size(pdrv);
    MBED_ASSERT(scount >= 64);
    return scount;
}

extern "C" DSTATUS disk_status(BYTE pdrv)
{
    debug_if(FFS_DBG, "disk_status on pdrv [%d]\n", pdrv);
    return RES_OK;
}

extern "C" DSTATUS disk_initialize(BYTE pdrv)
{
    debug_if(FFS_DBG, "disk_initialize on pdrv [%d]\n", pdrv);
    return (DSTATUS)_ffs[pdrv]->init();
}

extern "C" DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    debug_if(FFS_DBG, "disk_read(sector %lu, count %u) on pdrv [%d]\n", sector, count, pdrv);
    DWORD ssize = disk_get_sector_size(pdrv);
    mbed::bd_addr_t addr = (mbed::bd_addr_t)sector * ssize;
    mbed::bd_size_t size = (mbed::bd_size_t)count * ssize;
    int err = _ffs[pdrv]->read(buff, addr, size);
    return err ? RES_PARERR : RES_OK;
}

extern "C" DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    debug_if(FFS_DBG, "disk_write(sector %lu, count %u) on pdrv [%d]\n", sector, count, pdrv);
    DWORD ssize = disk_get_sector_size(pdrv);
    mbed::bd_addr_t addr = (mbed::bd_addr_t)sector * ssize;
    mbed::bd_size_t size = (mbed::bd_size_t)count * ssize;

    int err = _ffs[pdrv]->erase(addr, size);
    if (err) {
        return RES_PARERR;
    }

    err = _ffs[pdrv]->program(buff, addr, size);
    if (err) {
        return RES_PARERR;
    }

    return RES_OK;
}

extern "C" DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    debug_if(FFS_DBG, "disk_ioctl(%d)\n", cmd);
    switch (cmd) {
        case CTRL_SYNC:
            if (_ffs[pdrv] == NULL) {
                return RES_NOTRDY;
            } else {
                return RES_OK;
            }
        case GET_SECTOR_COUNT:
            if (_ffs[pdrv] == NULL) {
                return RES_NOTRDY;
            } else {
                *((DWORD *)buff) = disk_get_sector_count(pdrv);
                return RES_OK;
            }
        case GET_SECTOR_SIZE:
            if (_ffs[pdrv] == NULL) {
                return RES_NOTRDY;
            } else {
                *((WORD *)buff) = disk_get_sector_size(pdrv);
                return RES_OK;
            }
        case GET_BLOCK_SIZE:
            *((DWORD *)buff) = 1; // default when not known
            return RES_OK;
        case CTRL_TRIM:
            if (_ffs[pdrv] == NULL) {
                return RES_NOTRDY;
            } else {
                DWORD *sectors = (DWORD *)buff;
                DWORD ssize = disk_get_sector_size(pdrv);
                mbed::bd_addr_t addr = (mbed::bd_addr_t)sectors[0] * ssize;
                mbed::bd_size_t size = (mbed::bd_size_t)(sectors[1] - sectors[0] + 1) * ssize;
                int err = _ffs[pdrv]->trim(addr, size);
                return err ? RES_PARERR : RES_OK;
            }
    }

    return RES_PARERR;
}

////// Generic filesystem operations //////

// Filesystem implementation (See FATFilySystem.h)
FATFileSystem::FATFileSystem(const char *name, BlockDevice *bd)
    : FileSystem(name), _fs(), _id(-1)
{
    if (bd) {
        mount(bd);
    }
}

FATFileSystem::~FATFileSystem()
{
    // nop if unmounted
    unmount();
}

int FATFileSystem::mount(BlockDevice *bd)
{
    // requires duplicate definition to allow virtual overload to work
    return mount(bd, true);
}

int FATFileSystem::mount(BlockDevice *bd, bool mount)
{
    lock();
    if (_id != -1) {
        unlock();
        return -EINVAL;
    }

    for (int i = 0; i < FF_VOLUMES; i++) {
        if (!_ffs[i]) {
            _id = i;
            _ffs[_id] = bd;
            _fsid[0] = '0' + _id;
            _fsid[1] = ':';
            _fsid[2] = '\0';
            debug_if(FFS_DBG, "Mounting [%s] on ffs drive [%s]\n", getName(), _fsid);
            FRESULT res = f_mount(&_fs, _fsid, mount);
            unlock();
            return fat_error_remap(res);
        }
    }

    unlock();
    return -ENOMEM;
}

int FATFileSystem::unmount()
{
    lock();
    if (_id == -1) {
        unlock();
        return -EINVAL;
    }

    FRESULT res = f_mount(NULL, _fsid, 0);
    int err = _ffs[_id]->deinit();
    if (err) {
        unlock();
        return err;
    }
    _ffs[_id] = NULL;
    _id = -1;
    unlock();
    return fat_error_remap(res);
}

/* See http://elm-chan.org/fsw/ff/en/mkfs.html for details of f_mkfs() and
 * associated arguments. */
int FATFileSystem::format(BlockDevice *bd, bd_size_t cluster_size)
{
    FATFileSystem fs;
    fs.lock();

    int err = bd->init();
    if (err) {
        fs.unlock();
        return err;
    }

    // erase first handful of blocks
    bd_size_t header = 2 * bd->get_erase_size();
    err = bd->erase(0, header);
    if (err) {
        bd->deinit();
        fs.unlock();
        return err;
    }

    if (bd->get_erase_value() < 0) {
        // erase is unknown, need to write 1s
        bd_size_t program_size = bd->get_program_size();
        void *buf = malloc(program_size);
        if (!buf) {
            bd->deinit();
            fs.unlock();
            return -ENOMEM;
        }

        memset(buf, 0xff, program_size);

        for (bd_addr_t i = 0; i < header; i += program_size) {
            err = bd->program(buf, i, program_size);
            if (err) {
                free(buf);
                bd->deinit();
                fs.unlock();
                return err;
            }
        }

        free(buf);
    }

    // trim entire device to indicate it is unneeded
    err = bd->trim(0, bd->size());
    if (err) {
        bd->deinit();
        fs.unlock();
        return err;
    }

    err = bd->deinit();
    if (err) {
        fs.unlock();
        return err;
    }

    err = fs.mount(bd, false);
    if (err) {
        fs.unlock();
        return err;
    }

    // Logical drive number, Partitioning rule, Allocation unit size (bytes per cluster)
    MKFS_PARM opt;
    FRESULT   res;
    opt.fmt     = (FM_ANY | FM_SFD);
    opt.n_fat   = 0U;
    opt.align   = 0U;
    opt.n_root  = 0U;
    opt.au_size = (DWORD)cluster_size;

    res = f_mkfs((const TCHAR*)fs._fsid, &opt, NULL, FF_MAX_SS);
    if (res != FR_OK) {
        fs.unmount();
        fs.unlock();
        return fat_error_remap(res);
    }

    err = fs.unmount();
    if (err) {
        fs.unlock();
        return err;
    }

    fs.unlock();
    return 0;
}

int FATFileSystem::reformat(BlockDevice *bd, int allocation_unit)
{
    lock();
    if (_id != -1) {
        if (!bd) {
            bd = _ffs[_id];
        }

        int err = unmount();
        if (err) {
            unlock();
            return err;
        }
    }

    if (!bd) {
        unlock();
        return -ENODEV;
    }

    int err = FATFileSystem::format(bd, allocation_unit);
    if (err) {
        unlock();
        return err;
    }

    err = mount(bd);
    unlock();
    return err;
}

int FATFileSystem::remove(const char *path)
{
    Deferred<const char *> fpath = fat_path_prefix(_id, path);

    lock();
    FRESULT res = f_unlink(fpath);
    unlock();

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_unlink() failed: %d\n", res);
        if (res == FR_DENIED) {
            return -ENOTEMPTY;
        }
    }
    return fat_error_remap(res);
}

int FATFileSystem::rename(const char *oldpath, const char *newpath)
{
    Deferred<const char *> oldfpath = fat_path_prefix(_id, oldpath);
    Deferred<const char *> newfpath = fat_path_prefix(_id, newpath);

    lock();
    FRESULT res = f_rename(oldfpath, newfpath);
    unlock();

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_rename() failed: %d\n", res);
    }
    return fat_error_remap(res);
}

int FATFileSystem::mkdir(const char *path, mode_t mode)
{
    Deferred<const char *> fpath = fat_path_prefix(_id, path);

    lock();
    FRESULT res = f_mkdir(fpath);
    unlock();

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_mkdir() failed: %d\n", res);
    }
    return fat_error_remap(res);
}

int FATFileSystem::stat(const char *path, struct stat *st)
{
    Deferred<const char *> fpath = fat_path_prefix(_id, path);

    lock();
    FILINFO f;
    memset(&f, 0, sizeof(f));

    FRESULT res = f_stat(fpath, &f);
    if (res != FR_OK) {
        unlock();
        return fat_error_remap(res);
    }

    st->st_size = f.fsize;
    st->st_mode = 0;
    st->st_mode |= (f.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
    st->st_mode |= (f.fattrib & AM_RDO) ?
                   (S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) :
                   (S_IRWXU | S_IRWXG | S_IRWXO);
    unlock();

    return 0;
}

int FATFileSystem::statvfs(const char *path, struct statvfs *buf)
{

    memset(buf, 0, sizeof(struct statvfs));
    FATFS *fs;
    DWORD fre_clust;

    lock();
    FRESULT res = f_getfree(_fsid, &fre_clust, &fs);
    if (res != FR_OK) {
        unlock();
        return fat_error_remap(res);
    }

    buf->f_bsize = fs->ssize;
    buf->f_frsize = fs->ssize;
    buf->f_blocks = (fs->n_fatent - 2) * fs->csize;
    buf->f_bfree = fre_clust * fs->csize;
    buf->f_bavail = buf->f_bfree;
#if FF_USE_LFN
    buf->f_namemax = FF_LFN_BUF;
#else
    buf->f_namemax = FF_SFN_BUF;
#endif

    unlock();
    return 0;
}

void FATFileSystem::lock()
{
    _ffs_mutex->lock();
}

void FATFileSystem::unlock()
{
    _ffs_mutex->unlock();
}


////// File operations //////
int FATFileSystem::file_open(fs_file_t *file, const char *path, int flags)
{
    debug_if(FFS_DBG, "open(%s) on filesystem [%s], drv [%d]\n", path, getName(), _id);

    FIL *fh = new FIL;
    Deferred<const char *> fpath = fat_path_prefix(_id, path);

    /* POSIX flags -> FatFS open mode */
    BYTE openmode;
    if (flags & O_RDWR) {
        openmode = FA_READ | FA_WRITE;
    } else if (flags & O_WRONLY) {
        openmode = FA_WRITE;
    } else {
        openmode = FA_READ;
    }

    if (flags & O_CREAT) {
        if (flags & O_TRUNC) {
            openmode |= FA_CREATE_ALWAYS;
        } else {
            openmode |= FA_OPEN_ALWAYS;
        }
    }

    if (flags & O_APPEND) {
        openmode |= FA_OPEN_APPEND;
    }

    lock();
    FRESULT res = f_open(fh, fpath, openmode);

    if (res != FR_OK) {
        unlock();
        debug_if(FFS_DBG, "f_open('w') failed: %d\n", res);
        delete fh;
        return fat_error_remap(res);
    }

    unlock();

    *file = fh;
    return 0;
}

int FATFileSystem::file_close(fs_file_t file)
{
    FIL *fh = static_cast<FIL *>(file);

    lock();
    FRESULT res = f_close(fh);
    unlock();

    delete fh;
    return fat_error_remap(res);
}

ssize_t FATFileSystem::file_read(fs_file_t file, void *buffer, size_t len)
{
    FIL *fh = static_cast<FIL *>(file);

    lock();
    UINT n;
    FRESULT res = f_read(fh, buffer, len, &n);
    unlock();

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_read() failed: %d\n", res);
        return fat_error_remap(res);
    } else {
        return n;
    }
}

ssize_t FATFileSystem::file_write(fs_file_t file, const void *buffer, size_t len)
{
    FIL *fh = static_cast<FIL *>(file);

    lock();
    UINT n;
    FRESULT res = f_write(fh, buffer, len, &n);
    unlock();

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_write() failed: %d", res);
        return fat_error_remap(res);
    } else {
        return n;
    }
}

int FATFileSystem::file_sync(fs_file_t file)
{
    FIL *fh = static_cast<FIL *>(file);

    lock();
    FRESULT res = f_sync(fh);
    unlock();

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_sync() failed: %d\n", res);
    }
    return fat_error_remap(res);
}

off_t FATFileSystem::file_seek(fs_file_t file, off_t offset, int whence)
{
    FIL *fh = static_cast<FIL *>(file);

    lock();
    if (whence == SEEK_END) {
        offset += f_size(fh);
    } else if (whence == SEEK_CUR) {
        offset += f_tell(fh);
    }

    FRESULT res = f_lseek(fh, offset);
    off_t noffset = fh->fptr;
    unlock();

    if (res != FR_OK) {
        debug_if(FFS_DBG, "lseek failed: %d\n", res);
        return fat_error_remap(res);
    } else {
        return noffset;
    }
}

off_t FATFileSystem::file_tell(fs_file_t file)
{
    FIL *fh = static_cast<FIL *>(file);

    lock();
    off_t res = f_tell(fh);
    unlock();

    return res;
}

off_t FATFileSystem::file_size(fs_file_t file)
{
    FIL *fh = static_cast<FIL *>(file);

    lock();
    off_t res = f_size(fh);
    unlock();

    return res;
}

int FATFileSystem::file_truncate(fs_file_t file, off_t length)
{
    FIL *fh = static_cast<FIL *>(file);

    lock();
    // save current position
    FSIZE_t oldoff = f_tell(fh);

    // seek to new file size and truncate
    FRESULT res = f_lseek(fh, length);
    if (res) {
        unlock();
        return fat_error_remap(res);
    }

    res = f_truncate(fh);
    if (res) {
        unlock();
        return fat_error_remap(res);
    }

    // restore old position
    res = f_lseek(fh, oldoff);
    if (res) {
        unlock();
        return fat_error_remap(res);
    }

    return 0;
}


////// Dir operations //////
int FATFileSystem::dir_open(fs_dir_t *dir, const char *path)
{
    FATFS_DIR *dh = new FATFS_DIR;
    Deferred<const char *> fpath = fat_path_prefix(_id, path);

    lock();
    FRESULT res = f_opendir(dh, fpath);
    unlock();

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_opendir() failed: %d\n", res);
        delete dh;
        return fat_error_remap(res);
    }

    *dir = dh;
    return 0;
}

int FATFileSystem::dir_close(fs_dir_t dir)
{
    FATFS_DIR *dh = static_cast<FATFS_DIR *>(dir);

    lock();
    FRESULT res = f_closedir(dh);
    unlock();

    delete dh;
    return fat_error_remap(res);
}

ssize_t FATFileSystem::dir_read(fs_dir_t dir, struct dirent *ent)
{
    FATFS_DIR *dh = static_cast<FATFS_DIR *>(dir);
    FILINFO finfo;

    lock();
    FRESULT res = f_readdir(dh, &finfo);
    unlock();

    if (res != FR_OK) {
        return fat_error_remap(res);
    } else if (finfo.fname[0] == 0) {
        return 0;
    }

    ent->d_type = (finfo.fattrib & AM_DIR) ? DT_DIR : DT_REG;

#if FF_USE_LFN
    if (ent->d_name[0] == 0) {
        // No long filename so use short filename.
        strncpy(ent->d_name, finfo.fname, FF_LFN_BUF);
    }
#else
    strncpy(ent->d_name, finfo.fname, FF_SFN_BUF);
#endif

    return 1;
}

void FATFileSystem::dir_seek(fs_dir_t dir, off_t offset)
{
    FATFS_DIR *dh = static_cast<FATFS_DIR *>(dir);
    off_t dptr = static_cast<off_t>(dh->dptr);

    lock();

    if (offset < dptr) {
        f_rewinddir(dh);
    }
    while (dptr < offset) {
        FILINFO finfo;
        FRESULT res;

        res = f_readdir(dh, &finfo);
        dptr = dh->dptr;
        if (res != FR_OK) {
            break;
        } else if (finfo.fname[0] == 0) {
            break;
        }
    }
    unlock();
}

off_t FATFileSystem::dir_tell(fs_dir_t dir)
{
    FATFS_DIR *dh = static_cast<FATFS_DIR *>(dir);

    lock();
    off_t offset = dh->dptr;
    unlock();

    return offset;
}

void FATFileSystem::dir_rewind(fs_dir_t dir)
{
    FATFS_DIR *dh = static_cast<FATFS_DIR *>(dir);

    lock();
    f_rewinddir(dh);
    unlock();
}

} // namespace mbed
