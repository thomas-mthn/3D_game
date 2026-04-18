#ifndef LANGEXT_H
#define LANGEXT_H

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stddef.h>

#define structure(NAME) typedef struct NAME NAME;struct NAME

#define countof(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

#define thread_local _declspec(thread)

#ifdef _MSC_VER
#define forceinline __forceinline
#elif defined(__GNUC__)
#define forceinline __attribute__((always_inline))
#else
#define forceinline
#endif

#ifdef _MSC_VER
#define noinline _declspec(noinline)
#elif defined(__GNUC__)
#define noinline __attribute__((noinline))
#else
#define noinline
#endif

#define alignas(VALUE) _declspec(align(VALUE))

typedef int8_t   int8;
typedef uint8_t  uint8;
typedef int16_t  int16;
typedef uint16_t uint16;
typedef int32_t  int32;
typedef uint32_t uint32;
typedef int64_t  int64;
typedef uint64_t uint64;

typedef void (*funcptr_t)(void);

#ifdef __wasm__
#define stdcall 
#elif defined(__linux__)
#define stdcall __attribute__((stdcall))
#else
#define stdcall _stdcall
#endif

#if __STDC_VERSION__ < 202311
#define nullptr NULL
#endif

#endif
