/* mbed Microcontroller Library
 * Copyright (c) 2006-2012 ARM Limited
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "mbed.h"

#include "diskio.h"
#include "ffconf.h"
#include "mbed_debug.h"

#include "FATFileSystem.h"
#include "FATFileHandle.h"
#include "FATDirHandle.h"
#include "critical.h"


// Global access to block device from FAT driver
static BlockDevice *_ffs[_VOLUMES] = {0};
static SingletonPtr<PlatformMutex> _ffs_mutex;


// FAT driver functions
DWORD get_fattime(void) {
    time_t rawtime;
    time(&rawtime);
    struct tm *ptm = localtime(&rawtime);
    return (DWORD)(ptm->tm_year - 80) << 25
         | (DWORD)(ptm->tm_mon + 1  ) << 21
         | (DWORD)(ptm->tm_mday     ) << 16
         | (DWORD)(ptm->tm_hour     ) << 11
         | (DWORD)(ptm->tm_min      ) << 5
         | (DWORD)(ptm->tm_sec/2    );
}

// Implementation of diskio functions (see ChaN/diskio.h)
DSTATUS disk_status(BYTE pdrv) {
    debug_if(FFS_DBG, "disk_status on pdrv [%d]\n", pdrv);
    return (DSTATUS)_ffs[pdrv]->status();
}

DSTATUS disk_initialize(BYTE pdrv) {
    debug_if(FFS_DBG, "disk_initialize on pdrv [%d]\n", pdrv);
    return (DSTATUS)_ffs[pdrv]->init();
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
    debug_if(FFS_DBG, "disk_read(sector %d, count %d) on pdrv [%d]\n", sector, count, pdrv);
    int res = _ffs[pdrv]->read(buff, sector, count);
    return (res != (int)count) ? RES_PARERR : RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
    debug_if(FFS_DBG, "disk_write(sector %d, count %d) on pdrv [%d]\n", sector, count, pdrv);
    int res = _ffs[pdrv]->write(buff, sector, count);
    return (res != (int)count) ? RES_PARERR : RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    debug_if(FFS_DBG, "disk_ioctl(%d)\n", cmd);
    switch (cmd) {
        case CTRL_SYNC:
            if (_ffs[pdrv] == NULL) {
                return RES_NOTRDY;
            } else {
                int err = _ffs[pdrv]->sync();
                return err ? RES_ERROR : RES_OK;
            }
        case GET_SECTOR_COUNT:
            if (_ffs[pdrv] == NULL) {
                return RES_NOTRDY;
            } else {
                DWORD res = _ffs[pdrv]->get_block_count();
                if (res > 0) {
                    *((DWORD*)buff) = res; // minimum allowed
                    return RES_OK;
                } else {
                    return RES_ERROR;
                }
            }
        case GET_SECTOR_SIZE:
            if (_ffs[pdrv] == NULL) {
                return RES_NOTRDY;
            } else {
                DWORD res = _ffs[pdrv]->get_block_size();
                if (res > 0) {
                    *((DWORD*)buff) = res; // minimum allowed
                    return RES_OK;
                } else {
                    return RES_ERROR;
                }
            }
        case GET_BLOCK_SIZE:
            *((DWORD*)buff) = 1; // default when not known
            return RES_OK;
    }

    return RES_PARERR;
}


// Filesystem implementation (See FATFilySystem.h)
FATFileSystem::FATFileSystem(const char *n, BlockDevice *bd)
        : FileSystemLike(n), _id(-1) {
    if (bd) {
        mount(bd);
    }
}

FATFileSystem::~FATFileSystem() {
    // nop if unmounted
    unmount();
}

int FATFileSystem::mount(BlockDevice *bd) {
    return mount(bd, true);
}

int FATFileSystem::mount(BlockDevice *bd, bool force) {
    lock();
    if (_id != -1) {
        unlock();
        return -1;
    }

    for (int i = 0; i < _VOLUMES; i++) {
        if (!_ffs[i]) {
            _id = i;
            _ffs[_id] = bd;
            _fsid[0] = '0' + _id;
            _fsid[1] = '\0';
            debug_if(FFS_DBG, "Mounting [%s] on ffs drive [%s]\n", getName(), _fsid);
            FRESULT res = f_mount(&_fs, _fsid, force);
            unlock();
            return res == 0 ? 0 : -1;
        }
    }

    unlock();
    return -1;
}

int FATFileSystem::unmount() {
    lock();
    if (_id == -1) {
        unlock();
        return -1;
    }

    int err = _ffs[_id]->sync();
    if (err) {
        unlock();
        return -1;
    }

    FRESULT res = f_mount(NULL, _fsid, 0);
    _ffs[_id] = NULL;
    _id = -1;
    unlock();
    return res == 0 ? 0 : -1;
}

int FATFileSystem::sync() {
    lock();
    if (_id == -1) {
        unlock();
        return -1;
    }

    int err = _ffs[_id]->sync();
    unlock();
    return err ? -1 : 0;
}

int FATFileSystem::format(BlockDevice *bd) {
    FATFileSystem fs("");
    int err = fs.mount(bd, false);
    if (err) {
        return -1;
    }

    // Logical drive number, Partitioning rule, Allocation unit size (bytes per cluster)
    fs.lock();
    FRESULT res = f_mkfs(fs._fsid, 0, 512); 
    fs.unlock();

    err = fs.unmount();
    if (err) {
        return -1;
    }

    return res == 0 ? 0 : -1;
}

FileHandle *FATFileSystem::open(const char* name, int flags) {
    lock();
    debug_if(FFS_DBG, "open(%s) on filesystem [%s], drv [%s]\n", name, getName(), _fsid);
    char n[64];
    sprintf(n, "%s:/%s", _fsid, name);

    /* POSIX flags -> FatFS open mode */
    BYTE openmode;
    if (flags & O_RDWR) {
        openmode = FA_READ|FA_WRITE;
    } else if(flags & O_WRONLY) {
        openmode = FA_WRITE;
    } else {
        openmode = FA_READ;
    }
    if(flags & O_CREAT) {
        if(flags & O_TRUNC) {
            openmode |= FA_CREATE_ALWAYS;
        } else {
            openmode |= FA_OPEN_ALWAYS;
        }
    }

    FIL fh;
    FRESULT res = f_open(&fh, n, openmode);
    if (res) {
        debug_if(FFS_DBG, "f_open('w') failed: %d\n", res);
        unlock();
        return NULL;
    }
    if (flags & O_APPEND) {
        f_lseek(&fh, fh.fsize);
    }
    FATFileHandle *handle = new FATFileHandle(fh, _ffs_mutex.get());
    unlock();
    return handle;
}

int FATFileSystem::remove(const char *filename) {
    lock();
    FRESULT res = f_unlink(filename);
    if (res) {
        debug_if(FFS_DBG, "f_unlink() failed: %d\n", res);
        unlock();
        return -1;
    }
    unlock();
    return 0;
}

int FATFileSystem::rename(const char *oldname, const char *newname) {
    lock();
    FRESULT res = f_rename(oldname, newname);
    if (res) {
        debug_if(FFS_DBG, "f_rename() failed: %d\n", res);
        unlock();
        return -1;
    }
    unlock();
    return 0;
}

DirHandle *FATFileSystem::opendir(const char *name) {
    lock();
    FATFS_DIR dir;
    FRESULT res = f_opendir(&dir, name);
    if (res != 0) {
        unlock();
        return NULL;
    }
    FATDirHandle *handle = new FATDirHandle(dir, _ffs_mutex.get());
    unlock();
    return handle;
}

int FATFileSystem::mkdir(const char *name, mode_t mode) {
    lock();
    FRESULT res = f_mkdir(name);
    unlock();
    return res == 0 ? 0 : -1;
}

void FATFileSystem::lock() {
    _ffs_mutex->lock();
}

void FATFileSystem::unlock() {
    _ffs_mutex->unlock();
}
