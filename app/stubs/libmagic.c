/*
 * Copyright (C) 2015 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <magic.h>
#include <stdlib.h>
#include <dlfcn.h>

#define MAGIC_FUNCTIONS(f) \
    f(void, magic_close, \
     (magic_t cookie), (cookie), (void)0) \
    f(int, magic_load, \
     (magic_t cookie, const char* path), (cookie, path), -1) \
    f(const char*, magic_file, \
     (magic_t cookie, const char* path), (cookie, path), NULL)

static struct {
    void* handle;
    union {
        struct magic_proc {
            magic_t (*magic_open)(int flags);
            #define FN_POINTER(type,name,params,args,fail) type (* name) params;
            MAGIC_FUNCTIONS(FN_POINTER)
        } magic;
        void* entry[1];
    } fn;
} magic;

static const char* magic_names[] = {
    "magic_open",
    #define FN_NAME(type,name,params,args,fail) #name,
    MAGIC_FUNCTIONS(FN_NAME)
};

#define MAGIC_NUM_FUNCTIONS (sizeof(magic_names)/sizeof(magic_names[0]))
#define MAGIC_NO_HANDLE     ((void*)-1)
#define MAGIC_SO            "/usr/lib/libmagic.so.1"

/* magic_open() is the special function where we load the library */
magic_t magic_open(int flags)
{
    if (!magic.handle) {
        magic.handle = dlopen(MAGIC_SO, RTLD_LAZY);
        if (magic.handle) {
            unsigned int i;
            for (i=0; i<MAGIC_NUM_FUNCTIONS; i++) {
                magic.fn.entry[i] = dlsym(magic.handle, magic_names[i]);
            }
        } else {
            magic.handle = MAGIC_NO_HANDLE;
        }
    }
    return magic.fn.magic.magic_open ? magic.fn.magic.magic_open(flags) : NULL;
}

/* All other wrappers are generated by the preprocessor */
#define FN_IMPL(type,name,params,args,fail) type name params \
  { return magic.fn.magic.name ? magic.fn.magic.name args : fail; }
MAGIC_FUNCTIONS(FN_IMPL)