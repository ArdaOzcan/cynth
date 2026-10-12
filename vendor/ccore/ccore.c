#include "ccore.h"
#include "vmem.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

/* CSV Header:
 * Type,Function,BaseAddress,AllocAddress,Size,Used,Committed,Capacity,ExtraInfo
 */
#define CSV_LOG_POOL(pool_, func_, alloc_ptr_, alloc_size_, extra_)            \
    printf("POOL,%s,%p,%p,%zu,%zu,%zu,%zu,%s\n",                               \
           (func_),                                                            \
           (pool_)->base,                                                      \
           (alloc_ptr_),                                                       \
           (size_t)(alloc_size_),                                              \
           (size_t)0,                                                          \
           (pool_)->capacity,                                                  \
           (pool_)->capacity,                                                  \
           (extra_))
#define CSV_LOG_BUDDY(buddy_, func_, alloc_ptr_, alloc_size_, extra_)          \
    printf("BUDDY,%s,%p,%p,%zu,%zu,%zu,%zu,%s\n",                              \
           (func_),                                                            \
           (buddy_)->head,                                                     \
           (alloc_ptr_),                                                       \
           (size_t)(alloc_size_),                                              \
           (size_t)0,                                                          \
           (intptr_t)(buddy_)->tail - (intptr_t)(buddy_)->head,                \
           (intptr_t)(buddy_)->tail - (intptr_t)(buddy_)->head,                \
           (extra_))
#define CSV_LOG_VARENA(func, alloc_ptr, size_, extra)                          \
    printf("VARENA,%s,%p,%p,%zu,%zu,%zu,%zu,%s\n",                             \
           (func),                                                             \
           varena->base,                                                       \
           (alloc_ptr),                                                        \
           (size_t)(size_),                                                    \
           varena->used,                                                       \
           varena->page_count * varena->page_size,                             \
           varena->size,                                                       \
           (extra))

size_t
system_page_size()
{
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return (size_t)sysconf(_SC_PAGESIZE);
#endif
}

int
varena_destroy(VArena* varena)
{
    vmem_release(varena->base, varena->size);

#ifdef CCORE_VERBOSE
    CSV_LOG_VARENA("DESTROY", varena->base, 0, "Memory Released");
#endif
    varena->base       = NULL;
    varena->used       = 0;
    varena->page_count = 0;
    varena->size       = 0;
    return 0;
}

void
varena_clear(VArena* varena)
{
    varena->used = 0;
#ifdef CCORE_VERBOSE
    CSV_LOG_VARENA("CLEAR", varena->base, 0, "Pointer Reset");
#endif
}

int
varena_init(VArena* arena, size_t size)
{
    return varena_init_ex(arena, size, system_page_size(), DEFAULT_ALIGNMENT);
}

int
varena_init_ex(VArena* varena, size_t size, size_t page_size, size_t alignment)
{
    assert(page_size % system_page_size() == 0);
    void* base = vmem_reserve(size);

    varena->base       = base;
    varena->used       = 0;
    varena->page_count = 0;
    varena->page_size  = page_size;
    varena->size       = size;
    varena->alignment  = alignment;
#ifdef CCORE_VERBOSE
    CSV_LOG_VARENA("INIT", base, size, "Reserved Virtual Space");
#endif

    return 0;
}

static int
varena_commit_pages(VArena* varena, size_t amount)
{
    size_t committed = varena->page_count * varena->page_size;
    if (committed + varena->page_size * amount >= varena->size) {
        fprintf(stderr, "Allocation exceeds varena usable size!\n");
        return 1;
    }

    void* start = (uint8_t*)varena->base + committed;

    vmem_commit(start, varena->page_size * amount);

#ifdef CCORE_VERBOSE
    char info[64];
    sprintf(info, "Committed %zu new pages", amount);
    CSV_LOG_VARENA("COMMIT", start, varena->page_size * amount, info);
#endif
    varena->page_count += amount;
    return 0;
}

static uintptr_t
align_forward(uintptr_t ptr, size_t alignment)
{
    assert((alignment & (alignment - 1)) == 0);
    uintptr_t modulo = ptr & (alignment - 1);
    if (modulo != 0) {
        ptr += alignment - modulo;
    }
    return ptr;
}

static void
varena_increase_capacity(VArena* varena, size_t size)
{
    size_t end_offset   = varena->used + size;
    size_t committed    = varena->page_size * varena->page_count;
    size_t bytes_needed = end_offset > committed ? end_offset - committed : 0;
    size_t pages_needed =
      (bytes_needed + varena->page_size - 1) / varena->page_size;

    varena->used += size;

    if (pages_needed == 0) {
        return;
    }

    int err = varena_commit_pages(varena, pages_needed);
    if (err != 0) {
        fprintf(stderr, "VArena: Error while committing pages.\n");
    }
}

void*
varena_push(VArena* varena, size_t size)
{
    size_t start_offset = align_forward(varena->used, varena->alignment);
    size_t end_offset   = start_offset + size;

    varena_increase_capacity(varena, end_offset - varena->used);

    void* result = (uint8_t*)varena->base + start_offset;
#ifdef CCORE_VERBOSE
    CSV_LOG_VARENA("PUSH", result, size, "");
#endif
    return result;
}

void
varena_push_copy(VArena* arena, const void* data, size_t size)
{
    memcpy(varena_push(arena, size), data, size);
}

void
arena_init_ex(Arena* arena, void* base, size_t size, size_t alignment)
{
    arena->base      = base;
    arena->size      = size;
    arena->alignment = alignment;
    arena->used      = 0;
}

void
arena_clear(Arena* arena)
{
    arena->used = 0;
#ifdef CCORE_VERBOSE
    /* printf("CCORE: ARENA, %p, CLEAR\n", arena->base); */
#endif
}

void
arena_init(Arena* arena, void* base, size_t size)
{
    arena_init_ex(arena, base, size, DEFAULT_ALIGNMENT);
}

static void*
arena_push_aligned(Arena* arena, size_t size, size_t alignment)
{
    arena->used = align_forward(arena->used, alignment);
    arena->used += size;
    if (arena->used > arena->size) {
        printf("Arena is full\n");
        return NULL;
    }

    return arena->base + arena->used - size;
}

void*
arena_allocate(Arena* a, size_t size)
{
    return arena_push_aligned(a, size, a->alignment);
}

void
arena_push_copy(Arena* arena, const void* data, size_t size)
{
    memcpy(arena_allocate(arena, size), data, size);
}

static void*
arena_alloc_(size_t bytes, void* context)
{
    return arena_allocate((Arena*)context, bytes);
}

static void*
arena_realloc_(void* start, size_t old_size, size_t new_size, void* context)
{
    Arena* arena = context;
    if (new_size < old_size) {
        if (start + old_size == arena->base + arena->used) {
            arena->used -= old_size - new_size;
        }
        return start;
    }

    /* If at the end of the arena, we can just push
    the required size and return the original pointer. */
    if (start + old_size == arena->base + arena->used) {
        arena->used += new_size - old_size;
        if (arena->used > arena->size) {
            printf("Arena is full\n");
            return NULL;
        }
        return start;
    } else {
        void* new_start = arena_allocate(arena, new_size);
#ifdef CCORE_VERBOSE
        printf("Copied %lu bytes from %p to %p.\n", old_size, start, new_start);
#endif
        memcpy(new_start, start, old_size);
        return new_start;
    }
}

static void
arena_free_(void* ptr, size_t bytes, void* context)
{
}

static void*
varena_alloc_(size_t bytes, void* context)
{
    return varena_push((VArena*)context, bytes);
}

static void
varena_free_(void* ptr, size_t bytes, void* context)
{
}

static void*
varena_realloc_(void* start, size_t old_size, size_t new_size, void* context)
{
    VArena* varena = context;
    if (new_size < old_size) {
        if (start + old_size == varena->base + varena->used) {
            varena->used -= old_size - new_size;
        }
        return start;
    }

    /* If at the end of the arena, we can just push
    the required size and return the original pointer. */
    if (start + old_size == varena->base + varena->used) {
        varena_increase_capacity(varena, new_size - old_size);
        return start;
    } else {
        void* new_start = varena_push(varena, new_size);
        memcpy(new_start, start, old_size);
        return new_start;
    }
}

static void*
pool_alloc_(size_t bytes, void* context)
{
    assert(bytes <= ((Pool*)context)->chunk_size &&
           "Size was larger than chunk size");
    printf("Allocating chunk from pool.\n");
    return pool_allocate((Pool*)context);
}

static void
pool_free_(void* ptr, size_t bytes, void* context)
{
    Pool* pool = context;
    pool_free(pool, ptr);
}

static void*
pool_realloc_(void* start, size_t old_size, size_t new_size, void* context)
{
    Pool* pool = context;
#ifdef CCORE_VERBOSE
    printf("Realloc requested for pool. Doing nothing\n");
#endif
    return start;
}

static void*
buddy_alloc_(size_t bytes, void* context)
{
    return buddy_allocator_alloc((BuddyAllocator*)context, bytes);
}

static void
buddy_free_(void* ptr, size_t bytes, void* context)
{
    buddy_allocator_free((BuddyAllocator*)context, ptr);
}

static void*
buddy_realloc_(void* start, size_t old_size, size_t new_size, void* context)
{
#ifdef CCORE_VERBOSE
    printf("Buddy allocator realloc called for %p (%lu bytes to %lu "
           "bytes)\n",
           start,
           old_size,
           new_size);
#endif
    BuddyAllocator* buddy_allocator = context;
    BuddyBlock* block =
      (BuddyBlock*)((uintptr_t)start - buddy_allocator->alignment);

    if (new_size > old_size && new_size < block->size) {
#ifdef CCORE_VERBOSE
        printf("Block size %lu was sufficient.\n", block->size);
#endif
        return start;
    }

    buddy_allocator_free(buddy_allocator, start);
    void* new_start = buddy_allocator_alloc(buddy_allocator, new_size);
    if (new_start == NULL)
        return NULL;
    size_t smaller_size = old_size > new_size ? new_size : old_size;
    memcpy(new_start, start, smaller_size);
    return new_start;
}

Allocator
arena_allocator(Arena* arena)
{
    return (Allocator){
        .alloc   = arena_alloc_,
        .realloc = arena_realloc_,
        .free    = arena_free_,
        .context = arena,
    };
}

Allocator
varena_allocator(VArena* varena)
{
    return (Allocator){
        .alloc   = varena_alloc_,
        .realloc = varena_realloc_,
        .free    = varena_free_,
        .context = varena,
    };
}

Allocator
buddy_allocator(BuddyAllocator* buddy)
{
    return (Allocator){
        .alloc   = buddy_alloc_,
        .realloc = buddy_realloc_,
        .free    = buddy_free_,
        .context = buddy,
    };
}

Allocator
pool_allocator(Pool* pool)
{
    return (Allocator){
        .alloc   = pool_alloc_,
        .realloc = pool_realloc_,
        .free    = pool_free_,
        .context = pool,
    };
}

void
pool_free_all(Pool* p)
{
    size_t chunk_count = p->capacity / p->chunk_size;
    size_t i;

    for (i = 0; i < chunk_count; i++) {
        void* ptr          = &p->base[i * p->chunk_size];
        PoolFreeNode* node = (PoolFreeNode*)ptr;
        node->next         = p->head;
        p->head            = node;
    }
}

void
pool_init(Pool* pool,
          void* base,
          size_t capacity,
          size_t chunk_size,
          size_t chunk_alignment)
{
    uintptr_t initial_start = (uintptr_t)base;
    uintptr_t start = align_forward(initial_start, (uintptr_t)chunk_alignment);
    capacity -= (size_t)(start - initial_start);

    chunk_size = align_forward(chunk_size, chunk_alignment);

    assert(chunk_size >= sizeof(PoolFreeNode) && "Chunk size is too small");
    assert(capacity >= chunk_size &&
           "Backing buffer length is smaller than the chunk size");

    pool->base       = (unsigned char*)base;
    pool->capacity   = capacity;
    pool->chunk_size = chunk_size;
    pool->head       = NULL;
#ifdef CCORE_VERBOSE
    CSV_LOG_POOL(pool, "INIT", pool->base, pool->capacity, "");
#endif

    pool_free_all(pool);
}

void*
pool_allocate(Pool* p)
{
    PoolFreeNode* node = p->head;

    if (node == NULL) {
        fprintf(stderr, "Pool allocator has no free memory\n");
        return NULL;
    }

    p->head = p->head->next;
#ifdef CCORE_VERBOSE
    CSV_LOG_POOL(p, "ALLOC", node, p->chunk_size, "");
#endif
    return memset(node, 0, p->chunk_size);
}

void
pool_free(Pool* p, void* ptr)
{
    PoolFreeNode* node;

    void* start = p->base;
    void* end   = &p->base[p->capacity];

    if (ptr == NULL) {
        return;
    }

    if (!(start <= ptr && ptr < end)) {
        assert(0 && "Memory is out of bounds of the buffer in this pool");
        return;
    }

    node       = (PoolFreeNode*)ptr;
    node->next = p->head;
    p->head    = node;
#ifdef CCORE_VERBOSE
    CSV_LOG_POOL(p, "FREE", ptr, p->chunk_size, "");
#endif
}

static BuddyBlock*
buddy_block_next(BuddyBlock* block)
{
    return (BuddyBlock*)((char*)block + block->size);
}

static BuddyBlock*
buddy_block_split(BuddyBlock* block, size_t size)
{
    if (block != NULL && size != 0) {
        while (size < block->size) {
            size_t new_size = block->size >> 1;
            block->size     = new_size;
            block           = buddy_block_next(block);
            block->size     = new_size;
            block->is_free  = true;
        }

        if (size <= block->size) {
            return block;
        }
    }

    return NULL;
}

static BuddyBlock*
buddy_block_find_best(BuddyBlock* head, BuddyBlock* tail, size_t size)
{
    BuddyBlock* best_block = NULL;
    BuddyBlock* block      = head;
    BuddyBlock* buddy      = buddy_block_next(block);

    if (buddy == tail && block->is_free) {
        return buddy_block_split(block, size);
    }

    while (block < tail && buddy < tail) {
        /* Merge empty buddies with same size, reduces fragmentation
         */
        if (block->is_free && buddy->is_free && block->size == buddy->size) {
            block->size <<= 1;
            if (size <= block->size &&
                (best_block == NULL || block->size <= best_block->size)) {
                best_block = block;
            }

            block = buddy_block_next(buddy);
            if (block < tail) {
                buddy = buddy_block_next(block);
            }
            continue;
        }

        /* The current block is suitable */
        if (block->is_free && size <= block->size &&
            (best_block == NULL || block->size <= best_block->size)) {
            best_block = block;
        }

        /* Pick the buddy if it has smaller size */
        if (buddy->is_free && size <= buddy->size &&
            (best_block == NULL || buddy->size < best_block->size)) {
            best_block = buddy;
        }

        if (block->size <= buddy->size) {
            block = buddy_block_next(buddy);
            if (block < tail) {
                buddy = buddy_block_next(block);
            }
        } else {
            block = buddy;
            buddy = buddy_block_next(buddy);
        }
    }

    if (best_block != NULL) {
        /* Handles the case where the best block is a perfect fit */
        return buddy_block_split(best_block, size);
    }

    return NULL;
}

static bool
is_power_of_two(size_t x)
{
    return (x & (x - 1)) == 0;
}

void
buddy_allocator_init(BuddyAllocator* buddy,
                     void* data,
                     size_t size,
                     size_t alignment)
{
    assert(data != NULL);
    assert(is_power_of_two(size) && "size is not a power-of-two");
    assert(is_power_of_two(alignment) && "alignment is not a power-of-two");

    assert(is_power_of_two(sizeof(BuddyBlock)));
    if (alignment < sizeof(BuddyBlock)) {
        alignment = sizeof(BuddyBlock);
    }
    assert((uintptr_t)data % alignment == 0 &&
           "data is not aligned to minimum alignment");

    buddy->head          = (BuddyBlock*)data;
    buddy->head->size    = size;
    buddy->head->is_free = true;

    buddy->tail = buddy_block_next(buddy->head);

    buddy->alignment = alignment;

#ifdef CCORE_VERBOSE
    CSV_LOG_BUDDY(buddy, "INIT", buddy->head, size, "");
#endif
}

static size_t
buddy_block_size_required(BuddyAllocator* b, size_t size)
{
    size_t actual_size = b->alignment;

    size += sizeof(BuddyBlock);
    size = align_forward(size, b->alignment);

    while (size > actual_size) {
        actual_size <<= 1;
    }

    return actual_size;
}

static void
buddy_block_coalescence(BuddyBlock* head, BuddyBlock* tail)
{
    for (;;) {
        BuddyBlock* block = head;
        BuddyBlock* buddy = buddy_block_next(block);

        bool no_coalescence = true;
        while (block < tail && buddy < tail) {
            if (block->is_free && buddy->is_free &&
                block->size == buddy->size) {
                block->size <<= 1;
                block = buddy_block_next(block);
                if (block < tail) {
                    buddy          = buddy_block_next(block);
                    no_coalescence = false;
                }
            } else if (block->size < buddy->size) {
                block = buddy;
                buddy = buddy_block_next(buddy);
            } else {
                block = buddy_block_next(buddy);
                if (block < tail) {
                    buddy = buddy_block_next(block);
                }
            }
        }

        if (no_coalescence) {
            return;
        }
    }
}

void*
buddy_allocator_alloc(BuddyAllocator* buddy, size_t size)
{
#ifdef CCORE_VERBOSE
    printf("Buddy allocator allocate called %lu bytes\n", size);
#endif
    if (size != 0) {
        size_t actual_size = buddy_block_size_required(buddy, size);

        BuddyBlock* found =
          buddy_block_find_best(buddy->head, buddy->tail, actual_size);
        if (found == NULL) {
            buddy_block_coalescence(buddy->head, buddy->tail);
            found =
              buddy_block_find_best(buddy->head, buddy->tail, actual_size);
        }

        if (found != NULL) {
            found->is_free = false;
#ifdef CCORE_VERBOSE
            CSV_LOG_BUDDY(buddy, "ALLOC", found, found->size, "");
#endif
            return (void*)((char*)found + buddy->alignment);
        }
    }

#ifdef CCORE_VERBOSE
    fprintf(stderr,
            "No block with sufficient size was found. Size "
            "requested: %lu bytes.\n",
            size);
#endif

    return NULL;
}

void
buddy_allocator_free(BuddyAllocator* buddy, void* data)
{
    if (data != NULL) {
        BuddyBlock* block;

        assert((uintptr_t)buddy->head <= (uintptr_t)data);
        assert((uintptr_t)data < (uintptr_t)buddy->tail);

        block          = (BuddyBlock*)((char*)data - buddy->alignment);
        block->is_free = true;
#ifdef CCORE_VERBOSE
        CSV_LOG_BUDDY(buddy, "FREE", block, block->size, "");
#endif
    }
}

void*
array_init(size_t item_size, size_t capacity, Allocator* allocator)
{
    size_t size         = item_size * capacity + sizeof(ArrayHeader);
    ArrayHeader* header = allocator->alloc(size, allocator->context);

    void* ptr = NULL;
    if (header) {
        header->capacity = capacity;
#ifdef CCORE_VERBOSE
        /* printf("Array initialized with capacity %zu\n", capacity);
         */
#endif
        header->length    = 0;
        header->item_size = item_size;
        header->allocator = allocator;
        ptr               = header + 1;
    }

    return ptr;
}

ArrayHeader*
array_header(const void* arr)
{
    return (ArrayHeader*)(arr)-1;
}

size_t
array_len(const void* a)
{
    return array_header(a)->length;
}

void
array_remove(void* arr, size_t idx)
{
    ArrayHeader* h = array_header(arr);
    if (idx == h->length - 1) {
        h->length -= 1;
    } else if (h->length > 1 && idx < h->length) {
        uintptr_t ptr  = (uintptr_t)arr + idx * h->item_size;
        uintptr_t last = (uintptr_t)arr + (h->length - 1) * h->item_size;
        h->length -= 1;
        memcpy((void*)ptr, (void*)last, h->item_size);
    }
}

void
array_assign(void* dest, const void* src)
{
    int added_count = array_len(src) - array_len(dest);
    if (added_count >= 0) {
        array_ensure_capacity(dest, added_count);
    }

    memcpy(dest, src, array_header(src)->item_size * array_len(src));
    array_header(dest)->length = array_len(src);
}

void*
array_copy(const void* original, Allocator* allocator)
{
    ArrayHeader original_header = *array_header(original);
    size_t copy_size = original_header.item_size * original_header.length;
    void* result     = array_init(sizeof(uint8_t), copy_size, allocator);
    memcpy(result, original, copy_size);
    ArrayHeader* result_header = array_header(result);
    *result_header             = *array_header(original);

    return result;
}

void*
array_ensure_capacity(void* arr, size_t added_count)
{
    ArrayHeader* old_header = array_header(arr);

    size_t desired_capacity = old_header->length + added_count;
    if (desired_capacity <= old_header->capacity) {
        return arr;
    }

    /* Realloc array */
    size_t new_capacity = 2 * old_header->capacity;
    if (new_capacity == 0)
        new_capacity = 1;
    while (new_capacity < desired_capacity) {
        new_capacity *= 2;
    }

    size_t old_size =
      sizeof(ArrayHeader) + old_header->capacity * old_header->item_size;
    size_t new_size =
      sizeof(ArrayHeader) + new_capacity * old_header->item_size;

#ifdef CCORE_VERBOSE
    /* printf(
       "Reallocing array from %zu bytes to %zu bytes.\n", old_size,
       new_size);
     */
#endif
    ArrayHeader* new_header = old_header->allocator->realloc(
      old_header, old_size, new_size, old_header->allocator->context);

    if (new_header == NULL) {
        return NULL;
    }

    new_header->capacity = new_capacity;
    return new_header + 1;
}

char*
cstr_from_dynstr(const char* src, Allocator* allocator)
{
    char* cstr = make(char, array_len(src) + 1, allocator);
    memcpy(cstr, src, array_len(src));
    cstr[array_len(src)] = '\0';
    return cstr;
}

char*
dynstr_from_cstr(const char* cstr, size_t capacity, Allocator* allocator)
{
    size_t len = strlen(cstr);

    char* arr = array(char, capacity, allocator);
    array_ensure_capacity(arr, len + 1);
    memcpy(arr, cstr, len);
    arr[len]                  = '\0';
    array_header(arr)->length = len + 1;

    return arr;
}

char*
dynstr_init(size_t capacity, Allocator* a)
{
    char* arr = array(char, capacity, a);
    array_append(arr, (u8)'\0');

    return arr;
}

void
dynstr_append(char* dest, const char* src)
{
    size_t src_len = strlen(src);
    array_ensure_capacity(dest, src_len);
    size_t dest_str_len = dynstr_len(dest);
    array_header(dest)->length += src_len;
    memcpy(&dest[dest_str_len], src, src_len);
    dest[dynstr_len(dest)] = '\0';
}

void
dynstr_set(char* dest, const char* src)
{
    size_t src_len = dynstr_len(src);
    int diff       = src_len - dynstr_len(dest);
    if (diff > 0) {
        array_ensure_capacity(dest, diff);
    }

    memcpy(dest, src, src_len);
    dest[src_len]              = '\0';
    array_header(dest)->length = src_len + 1;
}

void
dynstr_shrink(char* str, size_t amount)
{
    /* Clamp */
    amount = (amount > dynstr_len(str) ? dynstr_len(str) : amount);
    array_header(str)->length -= amount;
    str[array_len(str) - 1] = '\0';
}

void
dynstr_clear(char* str)
{
    array_header(str)->length = 1;
    str[0]                    = '\0';
}

ByteString
byte_string_from_cstr(const char* str)
{
    return (ByteString){ .ptr = str, .length = strlen(str) };
}

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

/* From: https://benhoyt.com/writings/hash-table-in-c/
   Return 64-bit FNV-1a hash for key (NUL-terminated). See
   description:
   https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function */
uint64_t
cstr_hash(const char* key)
{
    uint64_t hash = FNV_OFFSET;
    const char* p = key;
    for (p = key; *p; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

uint64_t
bytes_hash(const u8* key, size_t length)
{
    uint64_t hash = FNV_OFFSET;
    size_t i      = 0;
    for (i = 0; i < length; i++) {
        hash ^= (uint64_t)(unsigned char)(key[i]);
        hash *= FNV_PRIME;
    }
    return hash;
}

void
hashmap_clear(Hashmap* hashmap)
{
    size_t i = 0;
    for (i = 0; i < hashmap->capacity; i++) {
        hashmap->records[i].type  = HASHMAP_RECORD_EMPTY;
        hashmap->records[i].key   = NULL;
        hashmap->records[i].value = NULL;
    }
    hashmap->length = 0;
}

void
hashmap_init(Hashmap* hashmap,
             uint64_t (*hash_fn)(const void*),
             bool (*equals_fn)(const void*, const void*),
             size_t capacity,
             Allocator* allocator)
{
    hashmap->records =
      allocator->alloc(sizeof(HashmapRecord) * capacity, allocator->context);
    hashmap->capacity  = capacity;
    hashmap->length    = 0;
    hashmap->hash_fn   = hash_fn;
    hashmap->equals_fn = equals_fn;
    size_t i           = 0;
    for (i = 0; i < hashmap->capacity; i++) {
        hashmap->records[i].type  = HASHMAP_RECORD_EMPTY;
        hashmap->records[i].key   = NULL;
        hashmap->records[i].value = NULL;
    }
}

static uint64_t
byte_string_hash(const void* byte_string)
{
    ByteString b = *(ByteString*)byte_string;
    return bytes_hash((const u8*)b.ptr, b.length);
}

static bool
byte_string_equal(const void* first, const void* second)
{
    ByteString a = *(ByteString*)first;
    ByteString b = *(ByteString*)second;
    return a.length == b.length && memcmp(a.ptr, b.ptr, a.length) == 0;
}

void
hashmap_byte_string_init(Hashmap* hashmap,
                         size_t capacity,
                         Allocator* allocator)
{
    hashmap->records =
      allocator->alloc(sizeof(HashmapRecord) * capacity, allocator->context);
    hashmap->capacity  = capacity;
    hashmap->length    = 0;
    hashmap->hash_fn   = byte_string_hash;
    hashmap->equals_fn = byte_string_equal;
    size_t i           = 0;
    for (i = 0; i < hashmap->capacity; i++) {
        hashmap->records[i].type  = HASHMAP_RECORD_EMPTY;
        hashmap->records[i].key   = NULL;
        hashmap->records[i].value = NULL;
    }
}

int
hashmap_insert(Hashmap* hashmap, void* key, void* value)
{
    if (value == NULL)
        return false;

    u16 idx  = hashmap->hash_fn(key) % hashmap->capacity;
    size_t i = 0;
    for (i = 0; i < hashmap->capacity; i++) {
        HashmapRecord* record =
          &hashmap->records[(idx + i) % hashmap->capacity];

        if (record->type == HASHMAP_RECORD_EMPTY ||
            record->type == HASHMAP_RECORD_DELETED) {
            record->key   = key;
            record->value = value;
            record->type  = HASHMAP_RECORD_FILLED;
            hashmap->length++;
            return 0;
        } else if (record->type == HASHMAP_RECORD_FILLED &&
                   hashmap->equals_fn(record->key, key)) {
            return 1;
        }
    }

    return 1;
}

void*
hashmap_get(Hashmap* hashmap, void* key)
{
    u16 hash = hashmap->hash_fn(key) % hashmap->capacity;
    size_t i = 0;
    for (i = 0; i < hashmap->capacity; i++) {
        u16 idx               = (hash + i) % hashmap->capacity;
        HashmapRecord* record = &hashmap->records[idx];
        if (record->type == HASHMAP_RECORD_EMPTY) {
            return NULL;
        }
        if (record->type == HASHMAP_RECORD_DELETED) {
            continue;
        }

        if (hashmap->equals_fn(key, record->key)) {
            return record->value;
        }
    }

    return NULL;
}

void*
hashmap_byte_string_get(Hashmap* hashmap, ByteString key)
{
    u16 hash = byte_string_hash(&key) % hashmap->capacity;
    size_t i = 0;
    for (i = 0; i < hashmap->capacity; i++) {
        u16 idx               = (hash + i) % hashmap->capacity;
        HashmapRecord* record = &hashmap->records[idx];
        if (record->type == HASHMAP_RECORD_EMPTY) {
            return NULL;
        }
        if (record->type == HASHMAP_RECORD_DELETED) {
            continue;
        }

        if (byte_string_equal(&key, record->key)) {
            return record->value;
        }
    }

    return NULL;
}

void*
hashmap_delete(Hashmap* hashmap, void* key)
{
    u16 hash = hashmap->hash_fn(key) % hashmap->capacity;
    size_t i = 0;
    for (i = 0; i < hashmap->capacity; i++) {
        u16 idx               = (hash + i) % hashmap->capacity;
        HashmapRecord* record = &hashmap->records[idx];
        if (record->type == HASHMAP_RECORD_EMPTY) {
            return NULL;
        }
        if (record->type == HASHMAP_RECORD_DELETED) {
            continue;
        }

        if (hashmap->equals_fn(key, record->key)) {
            void* temp    = record->value;
            record->type  = HASHMAP_RECORD_DELETED;
            record->key   = NULL;
            record->value = NULL;
            return temp;
        }
    }

    return NULL;
}
