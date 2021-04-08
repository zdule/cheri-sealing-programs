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

struct y {
    int x;
    int y[];
};

struct x {
    int a;
    struct y b[2];
};

/* Old impl
// This code is implementation specific. It only works if width of int 
// is larger than 16 bits.
union four_bytes_entry 
{
    struct table_entry {
        unsigned external:1, multiplicity:2, type:20;
    } table_entry;
    struct external_entry{
        unsigned final:1, offset:2, type:20;
    } external_entry;
};
static_assert(sizeof(struct table_entry) == sizeof(struct external_entry), "The extenral and table entry must be of same size");
static_assert(sizeof(struct table_entry) == sizeof(union four_bytes_entry), "The two entry types must be of same size as their union");
*/

// Unused, go for __cheri_alloc_description

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
void *my_malloc_aux(size_t size, struct __cheri_alloc_description *alloc_desc)
{
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

void my_free(void *p)
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
void my_check(void *castp, unsigned cast_target_type, unsigned cast_target_size)
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

struct TestStructA
{
    char a,b;
    float c;
    int d;
};

struct TestStruct
{
    int x;
    struct TestStructA y;
    double z;
    char c1[3];
    char c2[];
};

/*
// types: 1 char, 2 int, 3 float, 4 double, 5 TestStruct, 6 TestStructA
enum Types {
    t_char, t_int, t_float, t_double, t_TestStruct, t_TestStructA 
};

struct allocation_description test_struct_desc = {
    .repeating_type_offset = 27,
    .repeating_type_size = 1,
    .external_entries_start = 7,
    .entries = {
        {.table_entry = {.external = 1, .type = 0}},
        {.table_entry = {.external = 1, .type = 2}},
        {.table_entry = {.external = 0, .multiplicity = 0, .type = t_float}},
        {.table_entry = {.external = 0, .multiplicity = 0, .type = t_int}},
        {.table_entry = {.external = 0, .multiplicity = 0, .type = t_double}},
        {.table_entry = {.external = 0, .multiplicity = 3, .type = t_char}},
        {.table_entry = {.external = 0, .multiplicity = 3, .type = t_char}},

        {.external_entry = {.final = 0, .offset = 0, .type = t_TestStruct}},
        {.external_entry = {.final = 1, .offset = 0, .type = t_int}},
        {.external_entry = {.final = 0, .offset = 0, .type = t_TestStructA}},
        {.external_entry = {.final = 0, .offset = 0, .type = t_char}},
        {.external_entry = {.final = 0, .offset = 1, .type = t_char}},
    },
};
*/

void tstaf(struct TestStructA *p)
{
    p->a = 1;
}
   
int main()
{
    tstaf(&(struct TestStructA) {});

    printf("TestStructA %lu %lu %lu %lu\n", 
            offsetof(struct TestStructA, a),
            offsetof(struct TestStructA, b),
            offsetof(struct TestStructA, c),
            offsetof(struct TestStructA, d));
    printf("TestStruct %lu %lu %lu %lu %lu\n", 
            offsetof(struct TestStruct, x),
            offsetof(struct TestStruct, y),
            offsetof(struct TestStruct, z),
            offsetof(struct TestStruct, c1),
            offsetof(struct TestStruct, c2));


    size_t size = sizeof(__cheri_sealing_capability);
    if (sysctlbyname("security.cheri.sealcap", &__cheri_sealing_capability, &size, NULL, 0) < 0) {
        puts("Error getting root sealing cap");
        return 1;
    }
/*
    struct TestStruct *sp = my_malloc_aux(sizeof(struct TestStruct)+20*sizeof(char), &test_struct_desc);
    my_check(sp, t_TestStruct);
    my_check(sp, t_int);
    my_check(&(sp->z), t_double);
    my_check(&(sp->c1[2]), t_char);
    my_check(&(sp->c2[0]), t_char);
    my_check(&(sp->c2[10]), t_char);
    my_check(&(sp->c2[19]), t_char);
    my_check(&(sp->y), t_TestStructA);
    my_check(&(sp->y), t_char);
    my_check(&(sp->y.b), t_char);
    my_check(&(sp->y.c), t_float);
    my_check(&(sp->y.d), t_int);
    my_free(sp); */
    return 0;
}
