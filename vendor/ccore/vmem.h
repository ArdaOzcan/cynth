#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#if defined(MADV_DONTNEED)
#elif defined(__linux__)
#define MADV_DONTNEED 4
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
#define MADV_DONTNEED 4
#elif defined(__APPLE__)
#define MADV_DONTNEED 4
#endif
#endif

static void*
vmem_reserve(size_t size)
{
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
#else
    void* ptr = mmap(NULL,
                     size,
                     PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                     -1,
                     0);
    return ptr == MAP_FAILED ? NULL : ptr;
#endif
}

static int
vmem_commit(void* ptr, size_t size)
{
#ifdef _WIN32
    return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
#else
    return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
#endif
}

static int
vmem_decommit(void* ptr, size_t size)
{
#ifdef _WIN32
    return VirtualFree(ptr, size, MEM_DECOMMIT) != 0;
#else
#if defined(MADV_DONTNEED)
    return madvise(ptr, size, MADV_DONTNEED) == 0;
#else
    /* Fallback: do nothing */
    (void)ptr;
    (void)size;
    return 0;
#endif
#endif
}

static void
vmem_release(void* ptr, size_t size)
{
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}
