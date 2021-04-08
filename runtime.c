#include <stdlib.h>
#include <assert.h>
#include <cheri/cheric.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdbool.h>

void *__cheri_sealing_capability;

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

// A byte here is defined as containing CHAR_BITS bits
// conincidentally if CHAR_BTIS == 8, then four_bytes takes up
// exactly allocation_unit_size space
struct __cheri_alloc_description
{
    size_t repeat_offset;
    size_t repeat_size;
    size_t external_entries_start;
    uint32_t entries[]; 
};

// Unused currently
/*
// A gadget to prevent rellocation of allocation descriptions.
struct preamble_gadget
{
    struct preamble_gadget **backlink;
    struct allocation_description *desc;
};

struct type_offset_pair
{
    uint32_t type;
    size_t offset;
};
*/

const int ALLOC_DESC_OTYPE = 1;
const size_t ALLOC_PREAMBLE_SIZE = MAX(sizeof(struct __cheri_alloc_description*),alignof(max_align_t));

void *__cheri_tagged_malloc(size_t size, void *adesc)
{
	struct __cheri_alloc_description *alloc_desc = adesc;
    void *p = malloc(size + ALLOC_PREAMBLE_SIZE);

    // handle failed allocations
    if (p == NULL)
        return p;

    // Sanity check that the allocator returned a capability with tight bounds
    assert(cheri_getbase(p) == (unsigned long) p);

    struct __cheri_alloc_description **ad = (struct __cheri_alloc_description**) p;
    void *sc = __cheri_sealing_capability + ALLOC_DESC_OTYPE;
    *ad = cheri_seal(alloc_desc, sc);

    return p + ALLOC_PREAMBLE_SIZE;
}

void __cheri_tagged_free(void *p)
{
    assert(cheri_getbase(p) == (unsigned long) (p-ALLOC_PREAMBLE_SIZE));
    p -= ALLOC_PREAMBLE_SIZE;
    free(p);
}

static inline bool isExternal(uint32_t entry)
{
	return entry & (1<<31);
}

static inline bool isFinal(uint32_t entry)
{
	return entry & (1<<31);
}

static inline unsigned getMultiplicity(uint32_t entry)
{
	return (entry >> 28) & 7;
}

static inline unsigned getOffset(uint32_t entry)
{
	return (entry >> 28) & 7;
}

static inline unsigned getType(uint32_t entry)
{
	return entry & ((1<<24)-1);
}

// the compiler should never call this function if the cast_target_type is
// void, char, unsigned car, or signed_char
void __cheri_cast_check(void *castp, unsigned cast_target_type, unsigned cast_target_size)
{
    // allocation base
    struct __cheri_alloc_description *alloc_desc = *((struct __cheri_alloc_description **)cheri_setoffset(castp, 0));
    void *sc = __cheri_sealing_capability + ALLOC_DESC_OTYPE; 
    alloc_desc = cheri_unseal(alloc_desc, sc);

    size_t offset = cheri_getoffset(castp)-ALLOC_PREAMBLE_SIZE;
    if (offset >= alloc_desc->repeat_offset) {
        if (alloc_desc->repeat_size == 0)
            goto abort;
        offset = alloc_desc->repeat_offset + (offset - alloc_desc->repeat_offset) % alloc_desc->repeat_size;
    }
    size_t inter_offset = (offset%4)/cast_target_size;
    offset = offset/4;
    printf("%lu %lu\n", offset, inter_offset);

    uint32_t e = alloc_desc->entries[offset];
    if (!isExternal(e))
    {
        if (getType(e) != cast_target_type)
            goto abort;
        if (inter_offset + 1 > getMultiplicity(e))
            goto abort;
        return;
    }
    
    uint32_t *ext_entries = alloc_desc->entries + alloc_desc->external_entries_start;
    uint32_t *ext_entry = ext_entries + getType(e);
    while (true)
    {
        if (getType(*ext_entry) == cast_target_type && getOffset(*ext_entry) == inter_offset)
            return;
        if (isFinal(*ext_entry))
            goto abort;
        ext_entry++;
    }

    goto abort;
abort:
    // TODO: change to more suitable protection fault
    // abort might be handled?
    abort();
}

int __cheri_cast_init_runtime()
{
    size_t size = sizeof(__cheri_sealing_capability);
    if (sysctlbyname("security.cheri.sealcap", &__cheri_sealing_capability, &size, NULL, 0) < 0) {
        fputs("Error getting root sealing cap", stderr);
        abort();
    }
    return 0;
}
