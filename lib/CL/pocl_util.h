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

#ifndef POCL_UTIL_H
#define POCL_UTIL_H

#include <stdint.h>
#include <stdlib.h>
#include "pocl_cl.h"

#pragma GCC visibility push(hidden)
#ifdef __cplusplus
extern "C" {
#endif

/* Return a tmp/cache folder name for the current program
 */
char *pocl_choose_dir();
void pocl_create_binary_dirs(cl_program program, int size);
void pocl_create_source_dirs(cl_program program, int size);

void remove_directory (const char *path_name);
void remove_file(const char *file_path);
void make_directory (const char *path_name);

uint32_t byteswap_uint32_t (uint32_t word, char should_swap);
float byteswap_float (float word, char should_swap);

/* Finds the next highest power of two of the given value. */
size_t pocl_size_ceil2(size_t x);

/* Allocates aligned blocks of memory.
 *
 * Uses posix_memalign when available. Otherwise, uses
 * malloc to allocate a block of memory on the heap of the desired
 * size which is then aligned with the given alignment. The resulting
 * pointer must be freed with a call to pocl_aligned_free. Alignment
 * must be a non-zero power of 2.
 */
#if defined HAVE_POSIX_MEMALIGN
void *pocl_aligned_malloc(size_t alignment, size_t size);
# define pocl_aligned_free free
#else
void *pocl_aligned_malloc(size_t alignment, size_t size);
void pocl_aligned_free(void* ptr);
#endif

#ifdef __cplusplus
}
#endif
#pragma GCC visibility pop

/* Common macro for cleaning up "*GetInfo" API call implementations.
 * All the *GetInfo functions have been specified to look alike, 
 * and have been implemented to use the same variable names, so this
 * code can be shared.
 */
#define POCL_RETURN_GETINFO(__TYPE__, __VALUE__)                \
  {                                                                 \
    size_t const value_size = sizeof(__TYPE__);                     \
    if (param_value)                                                \
      {                                                             \
        if (param_value_size < value_size) return CL_INVALID_VALUE; \
        *(__TYPE__*)param_value = __VALUE__;                        \
      }                                                             \
    if (param_value_size_ret)                                       \
      *param_value_size_ret = value_size;                           \
    return CL_SUCCESS;                                              \
  } 


/* Function for creating events */
cl_int pocl_create_event (cl_event *event, cl_command_queue command_queue, 
                          cl_command_type command_type);

cl_int pocl_create_command (_cl_command_node **cmd, 
                            cl_command_queue command_queue, 
                            cl_command_type command_type, cl_event *event, 
                            cl_int num_events, const cl_event *wait_list);
  

void pocl_command_enqueue(cl_command_queue command_queue, 
                          _cl_command_node *node);

// Functions to get process info
char* pocl_get_process_name();


void create_or_update_file(const char* file_name, char* content);

char* get_file_contents(const char* file_name);

unsigned char* get_binfile_contents(const char* file_name);

void check_and_invalidate_cache(cl_program program, int device_i, const char* device_tmpdir);

#endif
