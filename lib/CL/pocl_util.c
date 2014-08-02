/* OpenCL runtime library: pocl_util utility functions

   Copyright (c) 2012 Pekka Jääskeläinen / Tampere University of Technology
   
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "pocl_util.h"
#include "pocl_cl.h"
#include "utlist.h"
#include "common.h"
#include "pocl_mem_management.h"

#define TEMP_DIR_PATH_CHARS 512

struct list_item;

typedef struct list_item
{
  void *value;
  struct list_item *next;
} list_item;

void 
pocl_remove_directory (const char *path_name)
{
  int str_size = 10 + strlen(path_name) + 1;
  char *cmd = (char*)malloc(str_size);
  snprintf(cmd, str_size, "rm -fr '%s'", path_name);
  system(cmd);
  free(cmd);
}

void
pocl_remove_file (const char *file_path)
{
  int str_size = 10 + strlen(file_path) + 1;
  char *cmd = (char*)malloc(str_size);
  snprintf(cmd, str_size, "rm -f '%s'", file_path);
  system(cmd);
  free(cmd);
}

void
pocl_make_directory (const char *path_name)
{
  int str_size = 12 + strlen(path_name) + 1;
  char *cmd = (char*)malloc(str_size);
  snprintf(cmd, str_size, "mkdir -p '%s'", path_name);
  system(cmd);
  free(cmd);
}

void
pocl_create_or_append_file (const char* file_name, char* content)
{
  FILE *fp = fopen(file_name, "a");
  if ((fp == NULL) || (content == NULL))
    return;

  fprintf(fp, "%s", content);

  fclose(fp);
}

char*
pocl_read_text_file (const char* file_name)
{
  char *str;
  FILE *fp;
  struct stat st;
  int file_size;

  stat(file_name, &st);
  file_size = (int)st.st_size;

  fp = fopen(file_name, "r");
  if (fp == NULL)
    return NULL;

  str = (char*) malloc((file_size + 1) * sizeof(char));
  fread(str, sizeof(char), file_size, fp);
  str[file_size] = '\0';

  return str;
}

unsigned char*
pocl_read_binary_file (const char* file_name)
{
  unsigned char *bin;
  FILE *fp;
  struct stat st;
  int file_size;

  stat(file_name, &st);
  file_size = (int)st.st_size;

  fp = fopen(file_name, "rb");
  if (fp == NULL)
    return NULL;

  bin = (unsigned char*) malloc(file_size * sizeof(unsigned char));
  fread(bin, file_size, sizeof(unsigned char), fp);

  return bin;
}

#define POCL_TEMPDIR_ENV "POCL_TEMP_DIR"

char*
pocl_generate_temp_dir_name ()
{  
  char *path_name, *process_name;

  path_name = (char*)malloc(TEMP_DIR_PATH_CHARS);
  process_name = pocl_get_process_name();

#ifdef ANDROID
  if ((getenv(POCL_TEMPDIR_ENV) != NULL)
          && (access(getenv (POCL_TEMPDIR_ENV), W_OK) == 0))
    {
      /* Optionally, applications can set POCL_TEMP_DIR to their cache folder */
      sprintf(path_name, "%s/pocl", getenv(POCL_TEMPDIR_ENV));
    }
  else
    {
      sprintf(path_name, "/data/data/%s/cache", process_name);
      if (access(path_name, W_OK) == 0)
        {
          strcat(path_name, "/pocl");
        }
      else
        {
          sprintf(path_name, "/sdcard/pocl/kcache/%s", process_name);
        }
    }
#else
  if ((getenv(POCL_TEMPDIR_ENV) != NULL)
          && (access(getenv(POCL_TEMPDIR_ENV), W_OK) == 0))
    {
      strcpy(path_name, getenv(POCL_TEMPDIR_ENV));
    }
  else
    {
    #ifdef POCL_KERNEL_CACHE
      sprintf(path_name, "%s/.pocl/kcache/%s", getenv("HOME"), process_name);
    #else
      sprintf(path_name, "/tmp/pocl/%s", process_name);
    #endif
    }
#endif

    /* TODO : delete contents if size exceeds some limit */

    return path_name;
}

void
pocl_create_source_dirs (cl_program program, int size)
{

  char s1[1024], s2[1024];
  char *path_name = program->temp_dir;
  char *source = program->source;

  /* Simple hash-function based on source length. SHA is an overkill */
  int found_in_cache = 0;

  sprintf(s1, "%s/s/%d", path_name, size);

#ifdef POCL_KERNEL_CACHE
  DIR *dp;
  struct dirent *ep;

  dp = opendir(s1);
  if (dp)
    {
      while ((ep = readdir(dp)) && (!found_in_cache))
        {
          char *content;
          sprintf(s2, "%s/%s/program.cl", s1, ep->d_name);
          content = pocl_read_text_file(s2);
          if (content && (strcmp(content, source) == 0))
            {               /* Voila, found same program source in cache */
              sprintf(path_name, "%s/%s", s1, ep->d_name);
              found_in_cache = 1;
            }
          if (content) free(content);
        }
      closedir(dp);
    }
#endif

  if (!found_in_cache)
    {

      if (access(s1, F_OK) != 0)
        pocl_make_directory(s1);

      sprintf(path_name, "%s/XXXXXX\0", s1);
      mkdtemp(path_name);
    }

  program->temp_dir = path_name;
}

void
pocl_create_binary_dirs (cl_program program, int size)
{
  char s1[1024], s2[1024];
  char *path_name = program->temp_dir;
  unsigned char **binaries = program->binaries;
  int device_i;

  /* Simple hash-function based on binary length. SHA is an overkill */
  int found_in_cache = 0;

  sprintf(s1, "%s/b/%d", path_name, size);

#ifdef POCL_KERNEL_CACHE
  DIR *dp;
  struct dirent *ep;

  dp = opendir(s1);
  if (dp)
    {
      while (ep = readdir(dp))
        {
          int found_cache_in_all = 1;
          for (device_i = 0; device_i < program->num_devices; device_i++)
            {
              unsigned char *bin;

              sprintf(s2, "%s/%s/%s/program.bc", s1, ep->d_name,
                            program->devices[device_i]->short_name);
              bin = pocl_read_binary_file(s2);

              if (!(bin && (memcmp(bin, binaries[device_i], program->binary_sizes[device_i]) == 0)))
                found_cache_in_all = 0;

              if(bin) free(bin);
            }

          if (found_cache_in_all)
            {
              found_in_cache = 1;
              sprintf(path_name, "%s/%s", s1, ep->d_name);
              break;
            }
        }
      closedir(dp);
    }
#endif

  if (!found_in_cache)
    {
      if (access(s1, F_OK) != 0)
        pocl_make_directory(s1);

      sprintf(path_name, "%s/XXXXXX\0", s1);
      mkdtemp(path_name);
    }

  program->temp_dir = path_name;
}

uint32_t
byteswap_uint32_t (uint32_t word, char should_swap) 
{
    union word_union 
    {
        uint32_t full_word;
        unsigned char bytes[4];
    } old, neww;
    if (!should_swap) return word;

    old.full_word = word;
    neww.bytes[0] = old.bytes[3];
    neww.bytes[1] = old.bytes[2];
    neww.bytes[2] = old.bytes[1];
    neww.bytes[3] = old.bytes[0];
    return neww.full_word;
}

float
byteswap_float (float word, char should_swap) 
{
    union word_union 
    {
        float full_word;
        unsigned char bytes[4];
    } old, neww;
    if (!should_swap) return word;

    old.full_word = word;
    neww.bytes[0] = old.bytes[3];
    neww.bytes[1] = old.bytes[2];
    neww.bytes[2] = old.bytes[1];
    neww.bytes[3] = old.bytes[0];
    return neww.full_word;
}

size_t
pocl_size_ceil2(size_t x) {
  /* Rounds up to the next highest power of two without branching and
   * is as fast as a BSR instruction on x86, see:
   *
   * http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
   */
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
#if SIZE_MAX > 0xFFFFFFFF
  x |= x >> 32;
#endif
  return ++x;
}

#ifndef HAVE_ALIGNED_ALLOC
void *
pocl_aligned_malloc (size_t alignment, size_t size)
{
# ifdef HAVE_POSIX_MEMALIGN
  
  /* make sure that size is a multiple of alignment, as posix_memalign
   * does not perform this test, whereas aligned_alloc does */
  if ((size & (alignment - 1)) != 0)
    {
      errno = EINVAL;
      return NULL;
    }

  /* posix_memalign requires alignment to be at least sizeof(void *) */
  if (alignment < sizeof(void *))
    alignment = sizeof(void* );

  void* result;
  int err;
  
  result = pocl_memalign_alloc(alignment, size);
  if (result == NULL)
    {
      errno = -1;
      return NULL;
    }

  return result;

# else
  
  /* allow zero-sized allocations, force alignment to 1 */
  if (!size)
    alignment = 1;

  /* make sure alignment is a non-zero power of two and that
   * size is a multiple of alignment */
  size_t mask = alignment - 1;
  if (!alignment || ((alignment & mask) != 0) || ((size & mask) != 0))
    {
      errno = EINVAL;
      return NULL;
    }

  /* allocate memory plus space for alignment header */
  uintptr_t address = (uintptr_t)malloc(size + mask + sizeof(void *));
  if (!address)
    return NULL;

  /* align the address, and store original pointer for future use
   * with free in the preceeding bytes */
  uintptr_t aligned_address = (address + mask + sizeof(void *)) & ~mask;
  void** address_ptr = (void **)(aligned_address - sizeof(void *));
  *address_ptr = (void *)address;
  return (void *)aligned_address;

#endif
}
#endif

#if !defined HAVE_ALIGNED_ALLOC && !defined HAVE_POSIX_MEMALIGN
void
pocl_aligned_free (void *ptr)
{
  /* extract pointer from original allocation and free it */
  if (ptr)
    free(*(void **)((uintptr_t)ptr - sizeof(void *)));
}
#endif

cl_int pocl_create_event (cl_event *event, cl_command_queue command_queue, 
                          cl_command_type command_type)
{
  if (event != NULL)
    {
      *event = pocl_mem_manager_new_event ();
      if (event == NULL)
        return CL_OUT_OF_HOST_MEMORY;
      
      (*event)->queue = command_queue;
      POname(clRetainCommandQueue) (command_queue);
      (*event)->command_type = command_type;
      (*event)->callback_list = NULL;
      (*event)->next = NULL;
    }
  return CL_SUCCESS;
}

cl_int pocl_create_command (_cl_command_node **cmd,
                            cl_command_queue command_queue, 
                            cl_command_type command_type, cl_event *event_p, 
                            cl_int num_events, const cl_event *wait_list)
{
  int i;
  int err;
  cl_event *event = NULL;

  if ((wait_list == NULL && num_events != 0) ||
      (wait_list != NULL && num_events == 0))
    return CL_INVALID_EVENT_WAIT_LIST;
  
  for (i = 0; i < num_events; ++i)
    {
      if (wait_list[i] == NULL)
        return CL_INVALID_EVENT_WAIT_LIST;
    }
  
  *cmd = pocl_mem_manager_new_command ();

  if (*cmd == NULL)
    return CL_OUT_OF_HOST_MEMORY;
  
  /* if user does not provide event pointer, create event anyway */
  event = &((*cmd)->event);
  err = pocl_create_event(event, command_queue, command_type);
  if (err != CL_SUCCESS)
    {
      free (*cmd);
      return err;
    }
  if (event_p)
    *event_p = *event;
  
  /* if in-order command queue and queue is not empty, add event from 
     previous command to new commands event_waitlist */
  if (!(command_queue->properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) 
      && command_queue->root != NULL)
    {
      _cl_command_node *prev_command;
      for (prev_command = command_queue->root; prev_command->next != NULL;
           prev_command = prev_command->next){}
      //printf("create_command: prev_com=%d prev_com->event = %d \n",prev_command, prev_command->event);
      cl_event *new_wl = (cl_event*)malloc ((num_events +1)*sizeof (cl_event));
      for (i = 0; i < num_events; ++i)
        {
          new_wl[i] = wait_list[i];
        }
      new_wl[i] = prev_command->event;
      (*cmd)->event_wait_list = new_wl;
      (*cmd)->num_events_in_wait_list = num_events + 1;
      for (i = 0; i < num_events + 1; ++i)
        {
          //printf("create-command: new_wl[%i]=%d\n", i, new_wl[i]);
        }
    }
  else
    {
      (*cmd)->event_wait_list = wait_list;  
      (*cmd)->num_events_in_wait_list = num_events;
    }
  (*cmd)->type = command_type;
  (*cmd)->next = NULL;
  (*cmd)->device = command_queue->device;

  //printf("create_command (end): event=%d new_event=%d cmd->event=%d cmd=%d\n", event, new_event, (*cmd)->event, *cmd);
  

  return CL_SUCCESS;
}

void pocl_command_enqueue (cl_command_queue command_queue,
                          _cl_command_node *node)
{
  POCL_LOCK_OBJ(command_queue);
  LL_APPEND (command_queue->root, node);
  POCL_UNLOCK_OBJ(command_queue);
  #ifdef POCL_DEBUG_BUILD
  if (pocl_is_option_set("POCL_IMPLICIT_FINISH"))
    POclFinish (command_queue);
  #endif
  POCL_UPDATE_EVENT_QUEUED (&node->event, command_queue);

}

char* pocl_get_process_name ()
{
  char tmpStr[64], cmdline[512], *processName = NULL;
  FILE *statusFile;
  int len, i, begin;

  sprintf(tmpStr, "/proc/%d/cmdline", getpid());
  statusFile = fopen(tmpStr, "r");
  if (statusFile == NULL)
    return NULL;

  if (fgets(cmdline, 511, statusFile) != NULL)
    {
      len = strlen(cmdline);
      begin = 0;
      for (i=len-1; i>=0; i--)     /* Extract program-name after last '/' */
        {
          if (cmdline[i] == '/')
            {
              begin = i + 1;
              break;
            }
        }
      processName = strdup(cmdline + begin);
    }

  fclose(statusFile);
  return processName;
}

static int cache_lock_initialized = 0;
static pocl_lock_t cache_lock;

void
pocl_check_and_invalidate_cache (cl_program program,
                  int device_i, const char* device_tmpdir)
{
  int cache_dirty = 0;
  char version_file[TEMP_DIR_PATH_CHARS];
  char options_file[TEMP_DIR_PATH_CHARS], *content;

  if (!cache_lock_initialized)
    {
      cache_lock_initialized = 1;
      POCL_INIT_LOCK(cache_lock);
    }

  sprintf(version_file, "%s/pocl_build_id", device_tmpdir);
  sprintf(options_file, "%s/program_build_options", device_tmpdir);

  POCL_LOCK(cache_lock);

  if (pocl_get_bool_option("POCL_CLEAR_KERNEL_CACHE", 0))
    {
      cache_dirty = 1;
      goto bottom;
    }

  /* Check for driver version match */
  if (access (version_file, F_OK) == 0)
    {
      content = pocl_read_text_file(version_file);
      if(content && (strcmp(content, POCL_BUILD_TIMESTAMP) != 0))
        {
          cache_dirty = 1;
        }

      if (content)
        free(content);
    }
  else
    {
      pocl_create_or_append_file(version_file, POCL_BUILD_TIMESTAMP);
    }
    if (cache_dirty)  goto bottom;

  /* Check for build option match */
  if (access(options_file, F_OK) == 0)
    {
      content = pocl_read_text_file(options_file);
      if (content && (program->compiler_options)
          && (strcmp(content, program->compiler_options) != 0))
        {
          cache_dirty = 1;
        }

      if (content)
        free(content);
    }
  else
    {
      pocl_create_or_append_file(options_file,
                          program->compiler_options);
    }
    if (cache_dirty)  goto bottom;

  /* If program contains "#include", disable caching
     Included headers might get modified, force recompilation in all the cases
   */
  if (strstr(program->source, "#include"))
    cache_dirty = 1;

  bottom:
  if (cache_dirty)
    {
      pocl_remove_directory(device_tmpdir);
      mkdir(device_tmpdir, S_IRWXU);

      pocl_create_or_append_file(version_file, POCL_BUILD_TIMESTAMP);
      pocl_create_or_append_file(options_file, program->compiler_options);
    }

  POCL_UNLOCK(cache_lock);
}
