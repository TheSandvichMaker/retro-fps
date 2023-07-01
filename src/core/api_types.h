#ifndef API_TYPES_H
#define API_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdalign.h>

#ifdef SINGLE_TRANSLATION_UNIT_BUILD
#define DREAM_API static
#else
#define DREAM_API extern
#endif

#define DREAM_INLINE static inline

// still not sure where this should go, but it should be present in headers as well.
// if the convention is that headers include api_types.h, then I guess this is the place
// to be.

#define DeferLoop(begin, end) for (int PASTE(_i_, __LINE__) = (begin, 0); !PASTE(_i_, __LINE__); PASTE(_i_, __LINE__) += (end, 1))

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

#define DEFAULT_STRING_ALIGN 16

typedef struct string_t
{
    size_t count;
    const char *data;
} string_t;

#define STRING_STORAGE(size) struct { size_t count; char data[size]; }
#define STRING_FROM_STORAGE(storage) (string_t) { (storage).count, (storage).data }
#define STRING_INTO_STORAGE(storage, string) (copy_memory((storage).data, (string).data, MIN(ARRAY_COUNT((storage).data), (string).count)), (storage).count = (string).count)
#define STRING_STORAGE_SIZE(storage) ARRAY_COUNT((storage).data)

#define strinit(text) { sizeof(text)-1, (const char *)("" text) }
#define strlit(text) ((string_t) { sizeof(text)-1, (const char *)("" text) })
#define strexpand(string) (int)(string).count, (string).data
#define strnull (string_t){ 0 }

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

typedef union v2i_t
{
    struct { int x, y; };
    int e[2];
} v2i_t;

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

typedef union v4_t
{
    struct { float x, y, z, w; };
    struct { v3_t xyz; float w0; };
    float e[4];
} v4_t;

typedef v4_t quat_t;

typedef union m4x4_t
{
    float e[4][4];
    v4_t col[4];
} m4x4_t;

typedef struct mat_t
{
    int m;
    int n;
    float *e;
} mat_t;

#define M(mat, row, col) (mat)->e[(mat)->n*(col) + (row)]

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

#define NULL_RESOURCE_HANDLE ((resource_handle_t) { 0, 0 })
#define RESOURCE_HANDLE_VALID(x) ((x).index != 0)
#define RESOURCE_HANDLES_EQUAL(a, b) ((a).value == (b).value)

typedef struct node_t
{
    struct node_t *parent;
    struct node_t *first_child, *last_child;
    struct node_t *next, *prev;
} node_t;

#define NODE_STRUCTURE(type)               \
    struct type *parent;                   \
    struct type *first_child, *last_child; \
    struct type *next, *prev;

typedef enum axis2_t
{
    AXIS2_X,
    AXIS2_Y,
    AXIS2_COUNT,
} axis2_t;

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
