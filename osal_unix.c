/*
 * osal_unix.c
 * $Id: osal_unix.c,v 1.1 2004/08/20 12:35:17 b081 Exp $
 *
 * Copyright 2004 Bobi B., w1zard0f07@yahoo.com
 *
 * This file is part of hdl_dump.
 *
 * hdl_dump is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * hdl_dump is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hdl_dump; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "retcodes.h"
#include "osal.h"
#include "apa.h"


/**************************************************************/
unsigned long
osal_get_last_error_code (void)
{
  return (errno);
}


/**************************************************************/
char*
osal_get_error_msg (unsigned long err)
{
  return (strerror (err));
}


/**************************************************************/
char*
osal_get_last_error_msg (void)
{
  return (osal_get_error_msg (osal_get_last_error_code ()));
}


/**************************************************************/
void
osal_dispose_error_msg (char *msg)
{
  /* this point should not be freed */
  msg = NULL;
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_open (const char *name,
	   osal_handle_t *handle,
	   int no_cache)
{
  /* TODO: remove buffering */
  handle->desc = open64 (name, O_RDONLY | O_LARGEFILE, S_IRUSR | S_IWUSR);
  return (handle->desc == -1 ? OSAL_ERR : OSAL_OK);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_open_device_for_writing (const char *device_name,
			      osal_handle_t *handle)
{
  /* TODO: remove buffering */
  handle->desc = open (device_name, O_RDWR | O_LARGEFILE, S_IRUSR | S_IWUSR);
  return (handle->desc == -1 ? OSAL_ERR : OSAL_OK);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_create_file (const char *path,
		  osal_handle_t *handle,
		  bigint_t estimated_size)
{
  int result = RET_ERR;
  handle->desc = open64 (path, O_CREAT | O_EXCL | O_LARGEFILE, S_IRUSR | S_IWUSR);
  if (handle->desc != -1)
    { /* success */
      off64_t offs = lseek64 (handle->desc, estimated_size - 1, SEEK_END);
      if (offs != -1)
	{
	  char dummy = '\0';
	  size_t bytes = write (handle->desc, &dummy, 1);
	  if (bytes == 1)
	    {
	      offs = lseek64 (handle->desc, 0, SEEK_SET);
	      if (offs == 0)
		{ /* success */
		  result = RET_OK;
		}
	    }
	}

      if (result != RET_OK)
	{ /* delete file on error */
	  close (handle->desc);
	  handle->desc = -1; /* make sure "is file opened" test would fail */
	  unlink (path);
	}
    }
  else
    result = RET_ERR;
  return (result);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_get_estimated_device_size (osal_handle_t handle,
				bigint_t *size_in_bytes)
{
  struct stat64 st;
  int result;
  memset (&st, 0, sizeof (struct stat64)); /* play on the safe side */
  result = fstat64 (handle.desc, &st);
  if (result == 0)
    { /* success */
      *size_in_bytes = st.st_size; /* TODO: might be 0 for block devices? */
      result = RET_OK;
    }
  else
    result = RET_ERR;
  return (result);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_get_device_size (osal_handle_t handle,
		      bigint_t *size_in_bytes)
{
  /* TODO: check if stat returns the *real* device size or works as Windows' one */
  return (osal_get_estimated_device_size (handle, size_in_bytes));
}


/**************************************************************/
int
osal_get_device_sect_size (osal_handle_t handle,
			   size_t *size_in_bytes)
{
  /* TODO: */
  return (OSAL_ERR);
}


/**************************************************************/
int
osal_get_volume_sect_size (const char *volume_root,
			   size_t *size_in_bytes)
{
  /* TODO: */
  return (OSAL_ERR);
}


/**************************************************************/
int
osal_get_file_size (osal_handle_t handle,
		    bigint_t *size_in_bytes)
{
  off64_t offs_curr = lseek64 (handle.desc, 0, SEEK_CUR);
  if (offs_curr != -1)
    {
      off64_t offs_end = lseek64 (handle.desc, 0, SEEK_END);
      if (offs_end != -1)
	{
	  if (lseek64 (handle.desc, offs_curr, SEEK_SET) == offs_curr)
	    { /* success */
	      *size_in_bytes = offs_end;
	      return (RET_OK);
	    }
	}
    }
  return (RET_ERR);
}


/**************************************************************/
int
osal_get_file_size_ex (const char *path,
		       bigint_t *size_in_bytes)
{
  osal_handle_t in;
  int result = osal_open (path, &in, 1);
  if (result == OSAL_OK)
    {
      result = osal_get_file_size (in, size_in_bytes);
      osal_close (in);
    }
  return (result);
}


/**************************************************************/
int
osal_seek (osal_handle_t handle,
	   bigint_t abs_pos)
{
  return (lseek64 (handle.desc, abs_pos, SEEK_SET) == -1 ? OSAL_ERR : OSAL_OK);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_read (osal_handle_t handle,
	   void *out,
	   size_t bytes,
	   size_t *stored)
{
  int n = read (handle.desc, &out, bytes);
  if (n != -1)
    { /* success */
      *stored = n;
      return (OSAL_OK);
    }
  else
    return (OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_write (osal_handle_t handle,
	    const void *in,
	    size_t bytes,
	    size_t *stored)
{
  int n = write(handle.desc, &in, bytes);
  if (n != -1)
    { /* success */
      *stored = n;
      return (OSAL_OK);
    }
  else
    return (OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_close (osal_handle_t handle)
{
  return (close (handle.desc) == -1 ? OSAL_ERR : OSAL_OK);
}


/**************************************************************/
void*
osal_alloc (size_t bytes)
{
  return (malloc (bytes));
}


/**************************************************************/
void
osal_free (void *ptr)
{
  if (ptr != NULL)
    free (ptr);
}


/**************************************************************/
int
osal_query_hard_drives (osal_dlist_t **hard_drives)
{
  /* TODO: under unix, they are like files */
  return (OSAL_ERR);
}


/**************************************************************/
int
osal_query_optical_drives (osal_dlist_t **optical_drives)
{
  /* TODO: under unix, they are like files */
  return (OSAL_ERR);
}


/**************************************************************/
int
osal_query_devices (osal_dlist_t **hard_drives,
		    osal_dlist_t **optical_drives)
{
  int result = osal_query_hard_drives (hard_drives);
  if (result == RET_OK)
    result = osal_query_optical_drives (optical_drives);
  return (result);
}


/**************************************************************/
void
osal_dlist_free (osal_dlist_t *dlist)
{
  if (dlist != NULL)
    {
      if (dlist->device != NULL)
	osal_free (dlist->device);
      osal_free (dlist);
    }
}


/**************************************************************/
int /* RET_OK, RET_BAD_FORMAT, RET_BAD_DEVICE */
osal_map_device_name (const char *input,
		      char output [30])
{
  /* TODO: map hddX to /dev/hdaX or /dev/ide/host/... */
  if (memcmp (input, "hdd", 3) == 0)
    {
      char *endp;
      long index = strtol (input + 3, &endp, 10);
      if (endp == input + 3)
	return (RET_BAD_FORMAT); /* bad format: no number after hdd */
      if (endp [0] == ':' &&
	  endp [1] == '\0')
	{
	  sprintf (output, "\\\\.\\PhysicalDrive%ld", index);
	  return (RET_OK);
	}
      else
	return (RET_BAD_FORMAT);
    }
  else if (memcmp (input, "cd", 2) == 0)
    {
      char *endp;
      long index = strtol (input + 2, &endp, 10);
      if (endp == input + 2)
	return (RET_BAD_FORMAT); /* bad format: no number after hdd */
      if (endp [0] == ':' &&
	  endp [1] == '\0')
	{
	  sprintf (output, "\\\\.\\CdRom%ld", index);
	  return (RET_OK);
	}
      else
	return (RET_BAD_FORMAT);
    }
  else
    return (RET_BAD_DEVICE);
}