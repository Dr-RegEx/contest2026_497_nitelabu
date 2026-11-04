/****************************************************************************
 * libs/libc/stdio/lib_readwindow.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef LIBS_LIBC_STDIO_LIB_READWINDOW_H
#define LIBS_LIBC_STDIO_LIB_READWINDOW_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Private FILE flags; no FILE layout or public ABI change.  A window owns
 * fs_bufstart (UBF is clear).  The descriptor follows the valid read
 * bytes; fs_bufpos denotes the logical cursor.  The off_t trailer at
 * fs_bufend uses memcpy because the buffer size may be unaligned.
 */

#define __FS_FLAG_RWIN    (1u << 4)
#define __FS_FLAG_RNOAUTO (1u << 5)
#define __FS_FLAG_RPOS   (1u << 6)

#define LIB_STDIO_READ_WINDOW_MAX 4096

#if !defined(CONFIG_STDIO_DISABLE_BUFFERING) && CONFIG_STDIO_BUFFER_SIZE > 0

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

static inline off_t lib_rwpos(FAR FILE *stream)
{
  off_t position;
  memcpy(&position, stream->fs_bufend, sizeof(position));
  return position;
}

static inline void lib_rwsetpos(FAR FILE *stream, off_t position)
{
  memcpy(stream->fs_bufend, &position, sizeof(position));
  stream->fs_flags |= __FS_FLAG_RPOS;
}

static inline void lib_rwadvance(FAR FILE *stream, ssize_t count)
{
  off_t position;
  if ((stream->fs_flags & (__FS_FLAG_RWIN | __FS_FLAG_RPOS)) ==
      (__FS_FLAG_RWIN | __FS_FLAG_RPOS))
    {
      if (__builtin_add_overflow(lib_rwpos(stream), (off_t)count, &position))
        {
          stream->fs_flags &= ~__FS_FLAG_RPOS;
        }
      else
        {
          lib_rwsetpos(stream, position);
        }
    }
}

/* A failed read may have overwritten input and moved the descriptor even
 * when it returned no byte count.  Retain capacity, but trust neither the
 * old data mapping nor its absolute position until a successful seek.
 */

static inline void lib_rwinvalidate(FAR FILE *stream)
{
  stream->fs_bufpos = stream->fs_bufread = stream->fs_bufstart;
  stream->fs_flags &= ~__FS_FLAG_RPOS;
}

static inline void lib_rwrelease(FAR FILE *stream)
{
  lib_free(stream->fs_bufstart);
  stream->fs_bufstart = stream->fs_buffer;
  stream->fs_bufend = stream->fs_buffer + CONFIG_STDIO_BUFFER_SIZE;
  stream->fs_bufpos = stream->fs_bufread = stream->fs_bufstart;
  stream->fs_flags &= ~(__FS_FLAG_RWIN | __FS_FLAG_RPOS);
  stream->fs_flags |= __FS_FLAG_UBF;
}

static inline void lib_rwprepare(FAR FILE *stream)
{
#ifdef CONFIG_STDIO_DISABLE_AUTO_READ_WINDOW
  (void)stream;
#else
  struct stat st;
  FAR char *buffer;
  off_t position;
  size_t capacity;
  int saved_errno;
  int fd;

  /* Only the untouched default buffer of a plain regular descriptor is
   * eligible.  Explicit buffering and every cookie retain their old paths.
   * A failed probe/allocation leaves the existing 64-byte path operational.
   */

  if ((stream->fs_flags & (__FS_FLAG_RWIN | __FS_FLAG_RNOAUTO)) != 0 ||
      stream->fs_bufstart != stream->fs_buffer ||
      stream->fs_bufpos != stream->fs_bufstart ||
      stream->fs_bufread != stream->fs_bufstart)
    {
      return;
    }

  saved_errno = get_errno();
  stream->fs_flags |= __FS_FLAG_RNOAUTO;
  if (stream->fs_iofunc.read != NULL || stream->fs_iofunc.write != NULL ||
      stream->fs_iofunc.seek != NULL || stream->fs_iofunc.close != NULL)
    {
      return;
    }

  fd = (int)(intptr_t)stream->fs_cookie;
  if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_blksize > 0)
    {
      capacity = st.st_blksize > LIB_STDIO_READ_WINDOW_MAX ?
                 LIB_STDIO_READ_WINDOW_MAX : st.st_blksize;
      if (capacity > CONFIG_STDIO_BUFFER_SIZE)
        {
          position = lseek(fd, 0, SEEK_CUR);
          if (position != (off_t)-1)
            {
              buffer = lib_malloc(capacity + sizeof(off_t));
              if (buffer != NULL)
                {
                  stream->fs_bufstart = buffer;
                  stream->fs_bufend = buffer + capacity;
                  stream->fs_bufpos = stream->fs_bufread = buffer;
                  stream->fs_flags &= ~(__FS_FLAG_UBF | __FS_FLAG_RNOAUTO);
                  stream->fs_flags |= __FS_FLAG_RWIN;
                  lib_rwsetpos(stream, position);
                }
            }
        }
    }

  set_errno(saved_errno);
#endif
}

#else

#define lib_rwinvalidate(stream) ((void)0)
#define lib_rwprepare(stream) ((void)0)
#define lib_rwadvance(stream, count) ((void)0)

#endif
#endif
