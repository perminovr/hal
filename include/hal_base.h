#ifndef HAL_BASE_H
#define HAL_BASE_H


#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


#if defined(__cplusplus) && !defined(USER_FORCE_CC)
#include <functional>
#define HALDEFCPP(...) __VA_ARGS__
#define HALCPPDEFINED
#else
#define HALDEFCPP(...)
#undef HALCPPDEFINED
#endif


#ifdef __linux__
# define HAL_NOT_EMPTY 1
#endif
#ifdef _WIN32
# define HAL_NOT_EMPTY 1
#endif
#ifdef __clang__
# define HAL_NOT_EMPTY 1
#endif
#ifndef HAL_NOT_EMPTY
# warning "HAL is empty! Implement your HAL"
#endif


#ifdef __GNUC__
# define ATTRIBUTE_PACKED __attribute__ ((__packed__))
#else
# define ATTRIBUTE_PACKED
#endif

#ifndef DEPRECATED
# if defined(__GNUC__) || defined(__clang__)
#  define DEPRECATED __attribute__((deprecated))
# else
#  define DEPRECATED
# endif
#endif

#if defined _WIN32 || defined __CYGWIN__
# ifdef EXPORT_FUNCTIONS_FOR_DLL
#  define PAL_API __declspec(dllexport)
# else
#  define PAL_API
#endif

#define PAL_INTERNAL
#else
# if __GNUC__ >= 4
#  define PAL_API extern __attribute__ ((visibility ("default")))
#  define PAL_INTERNAL  __attribute__ ((visibility ("hidden")))
# else
#  define PAL_API
#  define PAL_INTERNAL
# endif
#endif

/**
 * \brief Unified system descriptor
 */
typedef union {
	int i32;
	uint32_t u32;
	uint64_t u64;
	int64_t i64;
	//
	void *vptr;
	int *pi32;
	int *pu32;
	uint64_t *pu64;
	int64_t *pi64;
} unidesc;

/**
 * \brief Get invalid descriptor
 */
PAL_API unidesc
Hal_getInvalidUnidesc(void);

/**
 * \brief Check descriptor for invalid value
 */
PAL_API bool
Hal_unidescIsInvalid(unidesc p);

/**
 * \brief Compare two descriptors
 *
 * \return true if equal, false otherwise
 */
PAL_API bool
Hal_unidescIsEqual(const unidesc *p1, const unidesc *p2);

/**
 * \brief Convert string ipv4 representation to binary
 *
 * \return binary ip data in network byte order
 */
PAL_API uint32_t
Hal_ipv4StrToBin(const char *ip);

#endif /* HAL_BASE_H */
