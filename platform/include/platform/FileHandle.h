/* mbed Microcontroller Library
 * Copyright (c) 2017-2019 ARM Limited
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
#ifndef MBED_FILEHANDLE_H
#define MBED_FILEHANDLE_H

typedef int FILEHANDLE;

#include <cstdio>
#include "platform/Callback.h"
#include "platform/mbed_poll.h"
#include "platform/platform.h"
#include "platform/NonCopyable.h"

namespace mbed {

/**
 * \defgroup platform_FileHandle FileHandle functions
 * \ingroup platform-public-api-file
 * @{
 */


/** Class FileHandle
 *
 *  An abstract interface that represents operations on a file-like
 *  object. The core functions are read, write and seek, but only
 *  a subset of these operations can be provided.
 *
 *  @note to create a file, @see File
 *  @note Synchronization level: Set by subclass
 */
class FileHandle : private NonCopyable<FileHandle> {
public:
    virtual ~FileHandle() = default;

    /** Read the contents of a file into a buffer
     *
     *  Devices acting as FileHandles should follow POSIX semantics:
     *
     *  * if no data is available, and nonblocking set, return -EAGAIN
     *  * if no data is available, and blocking set, wait until some data is available
     *  * If any data is available, call returns immediately
     *
     *  @param buffer   The buffer to read in to
     *  @param size     The number of bytes to read
     *  @return         The number of bytes read, 0 at end of file, negative error on failure
     */
    virtual ssize_t read(void *buffer, size_t size) = 0;

    /** Write the contents of a buffer to a file
     *
     *  Devices acting as FileHandles should follow POSIX semantics:
     *
     * * if blocking, block until all data is written
     * * if no data can be written, and nonblocking set, return -EAGAIN
     * * if some data can be written, and nonblocking set, write partial
     *
     *  @param buffer   The buffer to write from
     *  @param size     The number of bytes to write
     *  @return         The number of bytes written, negative error on failure
     */
    virtual ssize_t write(const void *buffer, size_t size) = 0;

    /** Move the file position to a given offset from from a given location
     *
     *  @param offset   The offset from whence to move to
     *  @param whence   The start of where to seek
     *      SEEK_SET to start from beginning of file,
     *      SEEK_CUR to start from current position in file,
     *      SEEK_END to start from end of file
     *  @return         The new offset of the file, negative error code on failure
     */
    virtual off_t seek(off_t offset, int whence = SEEK_SET) = 0;

    /** Close a file
     *
     *  @return         0 on success, negative error code on failure
     */
    virtual int close() = 0;

    /** Synchronize the contents of a file with storage device
     *
     *  @return         0 on success, negative error code on failure
     */
    virtual int sync()
    {
        return 0;
    }

    /** Check if the file in an interactive terminal device
     *
     *  @return         True if the file is a terminal
     *  @return         False if the file is not a terminal
     *  @return         Negative error code on failure
     */
    virtual int isatty()
    {
        return false;
    }

    /** Get the file position of the file
     *
     *  @note This is equivalent to seek(0, SEEK_CUR)
     *
     *  @return         The current offset in the file, negative error code on failure
     */
    virtual off_t tell()
    {
        return seek(0, SEEK_CUR);
    }

    /** Rewind the file position to the beginning of the file
     *
     *  @note This is equivalent to seek(0, SEEK_SET)
     */
    virtual void rewind()
    {
        seek(0, SEEK_SET);
    }

    /** Get the size of the file
     *
     *  @return         Size of the file in bytes
     */
    virtual off_t size();

    /** Truncate or extend a file.
     *
     * The file's length is set to the specified value. The seek pointer is
     * not changed. If the file is extended, the extended area appears as if
     * it were zero-filled.
     *
     *  @param length   The requested new length for the file
     *
     *  @return         Zero on success, negative error code on failure
     */
    virtual int truncate(off_t length)
    {
        return -EINVAL;
    }

    /** Set blocking or nonblocking mode of the file operation like read/write.
     *  Definition depends on the subclass implementing FileHandle.
     *  The default is blocking.
     *
     *  @param blocking     true for blocking mode, false for nonblocking mode.
     *
     *  @return             0 on success
     *  @return             Negative error code on failure
     */
    virtual int set_blocking(bool blocking)
    {
        return blocking ? 0 : -ENOTTY;
    }

    /** Check current blocking or nonblocking mode for file operations.
     *
     *  @return             true for blocking mode, false for nonblocking mode.
     */
    virtual bool is_blocking() const
    {
        return true;
    }

    /** Enable or disable input
     *
     * Control enabling of device for input. This is primarily intended
     * for temporary power-saving; the overall ability of the device to operate for
     * input and/or output may be fixed at creation time, but this call can
     * allow input to be temporarily disabled to permit power saving without
     * losing device state.
     *
     *  @param enabled      true to enable input, false to disable.
     *
     *  @return             0 on success
     *  @return             Negative error code on failure
     */
    virtual int enable_input(bool enabled)
    {
        return -EINVAL;
    }

    /** Enable or disable output
     *
     * Control enabling of device for output. This is primarily intended
     * for temporary power-saving; the overall ability of the device to operate for
     * input and/or output may be fixed at creation time, but this call can
     * allow output to be temporarily disabled to permit power saving without
     * losing device state.
     *
     *  @param enabled      true to enable output, false to disable.
     *
     *  @return             0 on success
     *  @return             Negative error code on failure
     */
    virtual int enable_output(bool enabled)
    {
        return -EINVAL;
    }

    /** Check for poll event flags
     * You can use or ignore the input parameter. You can return all events
     * or check just the events listed in events.
     * Call is nonblocking - returns instantaneous state of events.
     * Whenever an event occurs, the derived class should call the sigio() callback).
     *
     * @param events        bitmask of poll events we're interested in - POLLIN/POLLOUT etc.
     *
     * @returns             bitmask of poll events that have occurred.
     */
    virtual short poll(short events) const
    {
        // Possible default for real files
        return POLLIN | POLLOUT;
    }

    /** Definition depends on the subclass implementing FileHandle.
     *  For example, if the FileHandle is of type Stream, writable() could return
     *  true when there is ample buffer space available for write() calls.
     *
     * @returns             true if the FileHandle is writable.
     */
    bool writable() const
    {
        return poll(POLLOUT) & POLLOUT;
    }

    /** Definition depends on the subclass implementing FileHandle.
     *  For example, if the FileHandle is of type Stream, readable() could return
     *  true when there is something available to read.
     *
     *  @returns            true when there is something available to read.
     */
    bool readable() const
    {
        return poll(POLLIN) & POLLIN;
    }

    /** Register a callback on state change of the file.
     *
     *  The specified callback will be called on state changes such as when
     *  the file can be written to or read from.
     *
     *  The callback may be called in an interrupt context and should not
     *  perform expensive operations.
     *
     *  Note! This is not intended as an attach-like asynchronous API, but rather
     *  as a building block for constructing such functionality.
     *
     *  The exact timing of when the registered function
     *  is called is not guaranteed and is susceptible to change. It should be used
     *  as a cue to make read/write/poll calls to find the current state.
     *
     *  @param func     Function to call on state change
     */
    virtual void sigio(Callback<void()> func)
    {
        //Default for real files. Do nothing for real files.
    }
};

/**@}*/

} // namespace mbed

#endif
