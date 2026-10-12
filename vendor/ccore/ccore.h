#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define KILOBYTE (1024ULL)
#define MEGABYTE (1024ULL * 1024ULL)

#define DEFAULT_ALIGNMENT (2 * sizeof(void*))

#define make(T, n, a) ((T*)((a)->alloc(sizeof(T) * (n), (a)->context)))

#define arena_push_array(arena, type, length)                                  \
    (type*)arena_push(arena, sizeof(type) * length)

#define array(type, cap, alloc) array_init(sizeof(type), cap, alloc)
#define array_append(a, v)                                                     \
    (assert(sizeof(v) == array_header(a)->item_size),                          \
     (a)               = array_ensure_capacity(a, 1),                          \
     (a)[array_len(a)] = (v),                                                  \
     &(a)[array_header(a)->length++])
#define array_pop_back(a) (a[--array_header(a)->length])

#define dynstr_len(str) (array_len(str) - 1)
#define dynstr_append_c(dest, src)                                             \
    {                                                                          \
        (dest)                         = array_ensure_capacity(dest, 1);       \
        (dest)[(dynstr_len(dest))]     = (src);                                \
        (dest)[(dynstr_len(dest)) + 1] = '\0';                                 \
        (array_header((dest)))->length++;                                      \
    }

typedef struct
{
    void* (*alloc)(size_t size, void* context);
    void (*free)(void* ptr, size_t size, void* context);
    void* (*realloc)(void* ptr,
                     size_t old_size,
                     size_t new_size,
                     void* context);
    void* context;
} Allocator;

typedef struct
{
    void* base;
    size_t used;
    size_t size;
    size_t alignment;
} Arena;

typedef struct
{
    void* base;
    size_t page_size;
    size_t page_count;
    size_t used;
    size_t size;
    size_t alignment;
} VArena;

typedef struct
{
    size_t capacity;
    size_t length;
    size_t item_size;
    Allocator* allocator;
} ArrayHeader;

typedef struct PoolFreeNode PoolFreeNode;
struct PoolFreeNode
{
    PoolFreeNode* next;
};

typedef struct
{
    u8* base;
    size_t capacity;
    size_t chunk_size;
    PoolFreeNode* head;
} Pool;

typedef struct BuddyBlock
{
    size_t size;
    bool is_free;
} BuddyBlock;

typedef struct BuddyAllocator
{
    BuddyBlock* head;
    BuddyBlock* tail;
    size_t alignment;
} BuddyAllocator;

void
pool_init(Pool* pool,
          void* base,
          size_t capacity,
          size_t chunk_size,
          size_t chunk_alignment);

void
pool_free_all(Pool* p);

void*
pool_allocate(Pool* p);

void
pool_free(Pool* p, void* ptr);

Allocator
pool_allocator(Pool* pool);

void
buddy_allocator_init(BuddyAllocator* b,
                     void* data,
                     size_t size,
                     size_t alignment);

void
buddy_allocator_free(BuddyAllocator* b, void* data);

void*
buddy_allocator_alloc(BuddyAllocator* b, size_t size);

Allocator
buddy_allocator(BuddyAllocator* buddy);

size_t
system_page_size();

void
arena_init(Arena* arena, void* base, size_t size);

void
arena_clear(Arena* arena);

void
arena_init_ex(Arena* arena, void* base, size_t size, size_t alignment);

void*
arena_allocate(Arena* arena, size_t size);

void
arena_push_copy(Arena* arena, const void* data, size_t size);

Allocator
arena_allocator(Arena* arena);

int
varena_init(VArena* arena, size_t size);

void
varena_clear(VArena* varena);

int
varena_init_ex(VArena* arena, size_t size, size_t page_size, size_t alignment);

void*
varena_push(VArena* varena, size_t size);

void
varena_push_copy(VArena* arena, const void* data, size_t size);

int
varena_destroy(VArena* arena);

Allocator
varena_allocator(VArena* varena);

void*
array_init(size_t item_size, size_t capacity, Allocator* allocator);

size_t
array_len(const void* a);

void
array_remove(void* arr, size_t idx);

ArrayHeader*
array_header(const void* arr);

void
array_assign(void* dest, const void* src);

void*
array_copy(const void* original, Allocator* allocator);

void*
array_ensure_capacity(void* arr, size_t added_count);

char*
cstr_from_dynstr(const char* src, Allocator* allocator);

char*
dynstr_from_cstr(const char* cstr, size_t capacity, Allocator* allocator);

char*
dynstr_init(size_t capacity, Allocator* a);

void
dynstr_append(char* dest, const char* src);

void
dynstr_clear(char* str);

void
dynstr_shrink(char* str, size_t amount);

void
dynstr_set(char* dest, const char* src);

typedef struct
{
    const char* ptr;
    size_t length;
} ByteString;

ByteString
byte_string_from_cstr(const char* str);

bool
byte_string_equals(ByteString first, ByteString second);

typedef enum
{
    HASHMAP_RECORD_FILLED,
    HASHMAP_RECORD_EMPTY,
    HASHMAP_RECORD_DELETED
} HashmapRecordType;

typedef struct
{
    HashmapRecordType type;
    void* key;
    void* value;
} HashmapRecord;

typedef struct
{
    HashmapRecord* records;
    uint64_t (*hash_fn)(const void*);
    bool (*equals_fn)(const void*, const void*);
    size_t capacity;
    size_t length;
} Hashmap;

uint64_t
cstr_hash(const char* key);

uint64_t
bytes_hash(const u8* key, size_t length);

void
hashmap_init(Hashmap* hashmap,
             uint64_t (*hash_fn)(const void*),
             bool (*equals_fn)(const void*, const void*),
             size_t capacity,
             Allocator* allocator);

void
hashmap_byte_string_init(Hashmap* hashmap,
                         size_t capacity,
                         Allocator* allocator);

void
hashmap_clear(Hashmap* hashmap);

int
hashmap_insert(Hashmap* hashmap, void* key, void* value);

void*
hashmap_get(Hashmap* hashmap, void* key);

void*
hashmap_byte_string_get(Hashmap* hashmap, ByteString key);

void*
hashmap_delete(Hashmap* hashmap, void* key);

size_t
hashmap_len(Hashmap* hashmap);
