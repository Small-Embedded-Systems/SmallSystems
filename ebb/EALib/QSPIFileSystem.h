/*
 *  Copyright 2013 Embedded Artists AB
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef QSPIFILESYSTEM_H
#define QSPIFILESYSTEM_H

#include "mbed.h"
#include "FileSystemLike.h"

/** Access the filesystem on an QSPI flash using SPIFI
 *
 * One way to utilize the 8MByte of QSPI FLASH on the LPC4088 QuickStart Board 
 * is to place a file system on it. The QSPIFileSystem is using the
 * FileSystemLike interface making using it is as simple as:
 *
 * @code
 * #include "mbed.h"
 * #include "QSPIFileSystem.h"
 *
 * QSPIFileSystem qspifs("qspi");
 *  
 * int main() {
 *     if (!qspifs.isformatted()) {
 *         qspifs.format();
 *     }
 *
 *     FILE *fp = fopen("/qspi/myfile.txt", "w");
 *     if (fp != NULL) {
 *         fwrite("Hello World!", 12, 1, fp);
 *         fclose(fp);
 *     }
 * }
 * @endcode
 *
 * The file system can be formatted to a specific size in increments of 1MByte 
 * and will allways be placed at the top of the address range. For the 8 MByte 
 * memory on the LPC4088 QuickStart Board this means:
 * 
 * <pre>
 *                    0x28000000                                               0x28800000
 *                         |------|------|------|------|------|------|------|------|
 * qspifs.format(1)        |   available for program                        |  FS  |
 *                         |------|------|------|------|------|------|------|------|
 *                                                                                  </pre><pre>
 *                         |------|------|------|------|------|------|------|------|
 * qspifs.format(2)        |   available for program                 | FileSystem  |
 *                         |------|------|------|------|------|------|------|------|
 *                                                                                  </pre><pre>
 *                         |------|------|------|------|------|------|------|------|
 * qspifs.format(7)        | PROG |                 FileSystem                     |
 *                         |------|------|------|------|------|------|------|------|
 *                                                                                  </pre><pre>
 *                         |------|------|------|------|------|------|------|------|
 * qspifs.format(8)        |                       FileSystem                      |
 *                         |------|------|------|------|------|------|------|------|
 * </pre>
 * The file system must be placed at the top of the memory because the linker scripts 
 * places your program at the bottom of the memory (if needed).
 *
 * The file system is using one or more blocks (each the size of one erase block) per 
 * file. Each file is stored in a sequence of blocks. If a file is written to so that 
 * it's size will occupy more than one block and the block after is already used by
 * a different file the entire file will be moved, like this:
 *
 * <pre>
 *                   |-------|-------|-------|-------|-------|-------|-------|
 * Before            |///////| FILE1 | FILE2 | FILE3 |///////|///////|///////|
 *                   |-------|-------|-------|-------|-------|-------|-------|
 *                                                                            </pre><pre>
 * After writing     |-------|-------|-------|-------|-------|-------|-------|
 * to FILE2          |///////| FILE1 |///////| FILE3 |     FILE2     |///////|
 *                   |-------|-------|-------|-------|-------|-------|-------|
 * </pre>
 *
 * <b>Note: </b>As each file takes up at least one block it will limit the total number of files that
 * can be stored on the file system. The formula for this is roughly 
 * <pre>
 *          max_num_files = fs_size_in_bytes / erase_block_size
 * </pre>
 * For the SPI flash on the LPC4088 QuickStart Board it means
 * <pre>
 *          max_num_files = 8MByte / 4KByte = 2048
 * </pre>
 *
 * Some of the blocks are used to store the table of content (TOC) at the very end of
 * the spi flash. The TOC will contain a list of all blocks on the flash (even those
 * not used by the file system) and will mark each block as "reserved","in use" or "free".
 * For a file system that is 4MByte on a 8MByte flash half of the blocks will be marked
 * as "reserved".
 * 
 * <b>Note: </b>The file system will not store any date/time information.
 *
 * <b>Note: </b>The file system will not store any file attributes (hidden/system/read only).
 *
 * <b>Note: </b>The file system stores the absolute path of each file (path + file name) in 
 * the file's first block's first 256 bytes. Folders are never stored themselves. This has 
 * the drawback that the file system cannot hold empty folders. 
 */
class QSPIFileSystem : public FileSystemLike {
public:

    /** Create the File System for accessing a QSPI Flash
     *
     * @param name The name used to access the virtual filesystem
     */
    QSPIFileSystem(const char* name);

    virtual FileHandle *open(const char *filename, int flags);
    virtual int remove(const char *filename);
    virtual int rename(const char *oldname, const char *newname);
    virtual DirHandle *opendir(const char *name);
    virtual int mkdir(const char *name, mode_t mode);

    /** Creates a new file system on the QSPI flash.
     * The file system will have the specified size and will always
     * be positioned at the end of the QSPI flash. If the fsSizeInMB is
     * less than the size of the QSPI flash the lower end of the flash
     * will be left untouched.
     *
     * @param fsSizeInMB The size of the file system
     *
     * @returns
     *    0 on success,
     *   -1 on failure.
     */
    int format(unsigned int fsSizeInMB = 8);

    /** Tests if there is a file system present on the QSPI flash
     *
     * @returns
     *   True if a valid file system could be found,
     *   False on failure.
     */
    bool isformatted();

    /** Retrieves the start and end addresses for the file system.
     * The pStartAddr and pEndAddr will only be assigned values if the
     * function returns true.
     *
     * @param pStartAddr Will return the start of the file system area
     * @param pEndAddr   Will return the end of the file system area
     *
     * @returns
     *   True if there is a file system,
     *   False on failure.
     */
    bool getMemoryBoundaries(uint32_t* pStartAddr, uint32_t* pEndAddr);
};

#endif
