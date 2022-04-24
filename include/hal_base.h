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
#if defined (_WIN32) || defined(_WIN64)
# define HAL_NOT_EMPTY 1
#endif
#ifndef HAL_NOT_EMPTY
# warning "HAL is empty! Implement your HAL"
#endif


#if defined(__GNUC__)
# define ATTRIBUTE_PACKED __attribute__ ((__packed__))
#else
# define ATTRIBUTE_PACKED
#endif

#ifndef DEPRECATED
# if defined(__GNUC__)
#  define DEPRECATED __attribute__((deprecated))
# else
#  define DEPRECATED
# endif
#endif


#if defined (_WIN32) || defined(_WIN64)
# ifdef EXPORT_FUNCTIONS_FOR_DLL
#  define HAL_API __declspec(dllexport)
# else
#  define HAL_API extern // __declspec(dllimport)
# endif
# define HAL_INTERNAL
#endif
#ifdef __linux__
# if __GNUC__ >= 4
#  define HAL_API extern __attribute__ ((visibility ("default")))
#  define HAL_INTERNAL  __attribute__ ((visibility ("hidden")))
# endif
#endif
#ifndef HAL_API
# define HAL_API extern
#endif
#ifndef HAL_INTERNAL
# define HAL_INTERNAL
#endif


#if defined (_WIN32) || defined(_WIN64)
# ifndef HAL_LOCAL_SOCK_ADDR
#  define HAL_LOCAL_SOCK_ADDR "127.0.0.1"
# endif
# ifndef HAL_LOCAL_SOCK_PORT_MIN
#  define HAL_LOCAL_SOCK_PORT_MIN 40001
# endif
# ifndef HAL_LOCAL_SOCK_PORT_MAX
#  define HAL_LOCAL_SOCK_PORT_MAX 43000
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
HAL_API unidesc
Hal_getInvalidUnidesc(void);

/**
 * \brief Check descriptor for invalid value
 */
HAL_API bool
Hal_unidescIsInvalid(unidesc p);

/**
 * \brief Compare two descriptors
 *
 * \return true if equal, false otherwise
 */
HAL_API bool
Hal_unidescIsEqual(const unidesc *p1, const unidesc *p2);

#endif /* HAL_BASE_H */
