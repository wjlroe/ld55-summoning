/* date = May 5th 2024 8:22 am */

#ifndef UTILS_H
#define UTILS_H

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))
#define MIN(x,y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  i64;
typedef int32_t  i32;
typedef int16_t  i16;
typedef int8_t   i8;

#define KB(x) ((size_t) (x) << 10)
#define MB(x) ((size_t) (x) << 20)

#ifdef DEBUG
static FILE* debug_file;
#define DEBUG_MSG(...) fprintf(debug_file, __VA_ARGS__)
#else
// TODO: what about release mode - where do we get logs if that goes wrong?
#define DEBUG_MSG(...) do { printf(__VA_ARGS__); } while (0)
#endif

#endif //UTILS_H
