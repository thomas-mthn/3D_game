#ifndef LANGEXT_H
#define LANGEXT_H

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stddef.h>

#define structure(NAME) typedef struct NAME NAME;struct NAME
#define enumeration(NAME) typedef enum NAME NAME;enum NAME

#define countof(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

#ifdef __POCC__
#define loop for(;1;)
#else
#define loop for(;;)
#endif

#define thread_local _declspec(thread)

#define forceinline __forceinline
#define noinline _declspec(noinline)

#define alignas(VALUE) _declspec(align(VALUE))

#define repeat(AMOUNT) for(size_t I = AMOUNT;I--;)

typedef int8_t   int8;
typedef uint8_t  uint8;
typedef int16_t  int16;
typedef uint16_t uint16;
typedef int32_t  int32;
typedef uint32_t uint32;
typedef int64_t  int64;
typedef uint64_t uint64;

#ifdef __wasm__
#define stdcall 
#else
#define stdcall _stdcall
#endif

#endif
