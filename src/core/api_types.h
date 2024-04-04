#ifndef API_TYPES_H
#define API_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdalign.h>
#include <stdlib.h>

// TODO: Cursed? DON'T DO IT??
#define USING(type, name) union { type; type name; }

#ifdef SINGLE_TRANSLATION_UNIT_BUILD
#define DREAM_API    extern  // deprecated
#define DREAM_GLOBAL extern  // visible outside modules
#define DREAM_LOCAL  static  // not visible outside modules
#else
#define DREAM_API    extern  // deprecated
#define DREAM_GLOBAL extern 
#define DREAM_LOCAL  extern 
#endif

#define SUPPRESS_UNUSED_WARNING(x) (void)(x)

#define DREAM_DLLEXPORT extern __declspec(dllexport)
#define DREAM_INLINE static inline

// still not sure where this should go, but it should be present in headers as well.
// if the convention is that headers include api_types.h, then I guess this is the place
// to be.

#define DEFER_LOOP(begin, end) for (int PASTE(_i_, __LINE__) = (begin, 0); !PASTE(_i_, __LINE__); PASTE(_i_, __LINE__) += (end, 1))

#define IGNORED(x) (void)(x)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#define PASTE_(a, b) a##b
#define PASTE(a, b) PASTE_(a, b)
#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))
#define KB(x) ((size_t)(x) << 10)
#define MB(x) ((size_t)(x) << 20)
#define GB(x) ((size_t)(x) << 30)
#define TB(x) ((size_t)(x) << 40)
#define SWAP(t, a, b) do { t PASTE(temp__, __LINE__) = a; a = b; b = PASTE(temp__, __LINE__); } while(0)
#define DEG_TO_RAD (PI32 / 180.0f)
#define RAD_TO_DEG (180.0f / PI32)
#define PAD(n) char PASTE(pad__, __LINE__)[n]

#define MASK_BITS(n) ((1 << ((n) + 1)) - 1)
#define TOGGLE_BIT(x, n, b) ((b) ? ((x)|(n)) : ((x)&~(n)))

#define BIT_CAST(from, to, x) (union { from f; to t; }){.f=x}.t
#define F32_AS_U32(f) BIT_CAST(float, uint32_t, f)
#define U32_AS_F32(u) BIT_CAST(uint32_t, float, u)

#define U64_FROM_PTR(x) (uint64_t)(uintptr_t)(x)
#define PTR_FROM_U64(x) (void *)(uintptr_t)(x)

#define DEFAULT_STRING_ALIGN 16

typedef struct string_t
{
    size_t count;
    char *data;
} string_t;

#define strinit(text) { sizeof(text)-1, (char *)("" text) }
#define Sc(text) { sizeof(text)-1, (char *)("" text) }
#define strlit(text) ((string_t) { sizeof(text)-1, (char *)("" text) })
#define S(text) strlit(text) // new laziness thing, will it stick? answer: yes. I like it
#define strexpand(string) (int)(string).count, (string).data
#define Sx(text) strexpand(text)
#define strnull (string_t){ 0 }

typedef struct string_node_t
{
    string_t string;

    struct string_node_t *next;
    struct string_node_t *prev;
} string_node_t;

typedef struct string_list_t
{
    string_node_t *first, *last;
} string_list_t;

typedef struct dynamic_string_t
{
	size_t capacity;
	union
	{
		struct
		{
			size_t count;
			char  *data;
		};
		string_t string;
	};
} dynamic_string_t;

typedef struct string_storage_overlay_t
{
	size_t count;
	char data[1];
} string_storage_overlay_t;

#define string_storage_t(size) struct { size_t count; char data[size]; }
#define string_from_storage(storage) (string_t) { (storage).count, (storage).data }
#define string_into_storage(storage, string) (copy_memory((storage).data, (string).data, MIN(ARRAY_COUNT((storage).data), (string).count)), (storage).count = (string).count)
#define string_storage_size(storage) ARRAY_COUNT((storage).data)

#define LOC_CSTRING STRINGIFY(__FILE__) ":" STRINGIFY(__LINE__)
#define LOC_STRING S(LOC_CSTRING)

#define stack_t(type, count)                                               \
	struct { size_t at; type values[count]; }

#define stack_capacity(stack)                                              \
	(ARRAY_COUNT(stack.values))                                            \

#define stack_empty(stack)                                                 \
	((stack).at == 0)

#define stack_nonempty(stack)                                              \
    !stack_empty(stack)

#define stack_full(stack)                                                  \
	((stack).at == ARRAY_COUNT((stack).values))

#define stack_nonfull(stack)                                               \
	!stack_full(stack)

#define stack_top(stack)                                                   \
	(ASSERT(!stack_empty(stack)), (stack).values[(stack).at - 1])

#define stack_add(stack) \
	(ASSERT((stack).at < ARRAY_COUNT((stack).values)),                     \
	 &(stack).values[(stack).at++])

#define stack_push(stack, value)                                           \
	(ASSERT((stack).at < ARRAY_COUNT((stack).values)),                     \
	 (stack).values[(stack).at++] = (value))

#define stack_push_back(stack, value)                                      \
	stack_push(stack, value)

#define stack_pop(stack)                                                   \
	(ASSERT(!stack_empty(stack)), (stack).values[--(stack).at])

#define stack_pop_back(stack)                                              \
	stack_pop(stack)

#define stack_count(stack)                                                 \
	((stack).at)

#define stack_unordered_remove(stack, index)                               \
	(ASSERT(!stack_empty(stack)),                                          \
	 ASSERT(index < (stack).at),                                           \
	 (stack).values[index] = (stack).values[--(stack).at])

#define stack_insert(stack, index, value)                                  \
	do {                                                                   \
		ASSERT(!stack_full(stack));                                        \
		for (size_t __i = ((stack).at); __i > index; __i--)                \
		{                                                                  \
			(stack).values[__i] = (stack).values[__i - 1];                 \
		}                                                                  \
		(stack).values[index] = value;                                     \
		(stack).at += 1;                                                   \
	} while (false) 

#define stack_remove(stack, index)                                         \
	do {                                                                   \
		ASSERT(!stack_empty(stack));                                       \
		ASSERT(index < (stack).at);                                        \
		for (size_t __i = index; __i < (stack).at - 1; __i++)              \
		{                                                                  \
			(stack).values[__i] = (stack).values[__i + 1];                 \
		}                                                                  \
		(stack).at -= 1;                                                   \
	} while (false) 

#define stack_erase(stack, value)                                          \
	do {                                                                   \
		for (size_t __k = 0; __k < stack_count(stack); __k++)              \
		{                                                                  \
			if ((stack).values[__k] == value)                              \
			{                                                              \
				stack_remove(stack, __k);                                  \
				break;                                                     \
			}                                                              \
		}                                                                  \
	} while (false)

#define stack_push_front(stack, value)                                     \
	stack_insert(stack, 0, value)

#define stack_pop_front(stack)                                             \
	stack_remove(stack, 0)

#define stack_reset(stack) \
	((stack).at = 0)

typedef struct string16_t
{
    size_t count;
    const wchar_t *data;
} string16_t;

#define strlit16(text) (string16_t) { sizeof(text) / sizeof(wchar_t) - 1, (const wchar_t *)(L"" text) }
#define strnull16 (string16_t){ 0 }

typedef struct arena_t
{
    bool owns_memory; // 1

    char *committed;  // 16
    char *end;        // 24
    char *at;         // 32
    char *buffer;     // 48
} arena_t;

typedef struct arena_marker_t
{
    arena_t *arena;
    char *at;
} arena_marker_t;

//
// mathy types
//

typedef union v2i_t
{
    struct { int32_t x, y; };
    int32_t e[2];
} v2i_t;

typedef union v2i16_t
{
	struct
	{
		int16_t x, y;
	};
	int16_t e[2];
} v2i16_t;

typedef union v3i_t
{
    struct { int x, y, z; };
    struct { v2i_t xy; float z0; };
    int e[3];
} v3i_t;

typedef union v2_t
{
    struct { float x, y; };
    float e[2];
} v2_t;

typedef union v3_t
{
    struct { float x, y, z; };
    struct { v2_t xy; float z0; };
    float e[3];
} v3_t;

#define V3_ZERO_STATIC { 0, 0, 0 }
#define V3_ZERO (v3_t) V3_ZERO_STATIC

typedef union v4_t
{
    struct { float x, y, z, w; };
    struct { v3_t xyz; float w0; };
    float e[4];
} v4_t;

typedef v4_t quat_t;
#define QUAT_IDENTITY_STATIC { 0, 0, 0, 1 }
#define QUAT_IDENTITY (quat_t) QUAT_IDENTITY_STATIC

typedef union m2x2_t
{
    float e[2][2];
    v2_t col[2]; // col as in column, not color...
} m2x2_t;

#define M2X2_IDENTITY_STATIC \
    { 1, 0, \
      0, 1, }
#define M2X2_IDENTITY (m2x2_t) M2X2_IDENTITY_STATIC

typedef union m3x3_t
{
    float e[3][3];
    v3_t col[3]; // col as in column, not color...
} m3x3_t;

#define M3X3_IDENTITY_STATIC \
    { 1, 0, 0, \
      0, 1, 0, \
      0, 0, 1, }
#define M3X3_IDENTITY m3x3_t M3X3_IDENTITY_STATIC

typedef union m4x4_t
{
    float e[4][4];
    v4_t col[4];
} m4x4_t;

#define M4X4_IDENTITY_STATIC { \
    1, 0, 0, 0,  \
    0, 1, 0, 0,  \
    0, 0, 1, 0,  \
    0, 0, 0, 1,  \
}
#define M4X4_IDENTITY (m4x4_t) M4X4_IDENTITY_STATIC

typedef struct mat_t
{
    int m;
    int n;
    float *e;
} mat_t;

#define M(mat, row, col) (mat)->e[(mat)->n*(col) + (row)]

typedef struct range2_t
{
	float min, max;
} range2_t;

typedef union rect2_t
{
    struct
    {
        float x0, y0;
        float x1, y1;
    };
    struct
    {
        v2_t min;
        v2_t max;
    };
} rect2_t;

typedef union rect2i_t
{
    struct
    {
        int x0, y0;
        int x1, y1;
    };
    struct
    {
        v2i_t min;
        v2i_t max;
    };
} rect2i_t;

typedef struct rect2i16_t
{
	union
	{
		struct
		{
			v2i16_t min, max;
		};

		struct
		{
			int16_t x0, y0, x1, y1;
		};
	};
} rect2i16_t;

typedef union rect3_t
{
    struct
    {
        float x0, y0, z0;
        float x1, y1, z1;
    };
    struct
    {
        v3_t min;
        v3_t max;
    };
} rect3_t;

typedef struct edge_t
{
	v3_t a, b;
} edge_t;

typedef struct triangle_t
{
	v3_t a, b, c;
} triangle_t;

typedef struct plane_t
{
    v3_t n;
    float d;
} plane_t;

typedef union resource_handle_t 
{ 
    struct
    {
        uint32_t index, generation;
    };
    uint64_t value;
} resource_handle_t;

#define DEFINE_HANDLE_TYPE(type)        \
    typedef union type                  \
    {                                   \
        resource_handle_t base;         \
        struct                          \
        {                               \
            uint32_t index, generation; \
        };                              \
        uint64_t value;                 \
    } type;

#define CAST_HANDLE(type, handle) (type){ .value = (handle).value }

DEFINE_HANDLE_TYPE(texture_handle_t);
DEFINE_HANDLE_TYPE(mesh_handle_t);

#define NULL_HANDLE(type_t) ((type_t){0})
#define NULL_RESOURCE_HANDLE ((resource_handle_t){0})
#define NULL_TEXTURE_HANDLE ((texture_handle_t){0})
#define NULL_MESH_HANDLE ((mesh_handle_t){0})
#define NULLIFY_HANDLE(handle) ((handle).value = 0)
#define RESOURCE_HANDLE_VALID(x) ((x).index != 0)
#define RESOURCE_HANDLES_EQUAL(a, b) ((a).value == (b).value)

typedef struct random_series_t
{
    uint32_t state;
} random_series_t;

typedef struct mutex_t
{
	void *opaque; // legal to be zero-initialized
} mutex_t;

typedef struct cond_t
{
	void *opaque; // legal to be zero-initialized
} cond_t;

typedef struct hires_time_t
{
    uint64_t value;
} hires_time_t;

#endif /* API_TYPES_H */
