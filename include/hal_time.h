#ifndef HAL_TIME_H
#define HAL_TIME_H


#include "hal_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * \file hal_time.h
 * \brief Abstraction layer for system time access
 */

/*! \addtogroup hal
   *
   *  @{
   */

/**
 * @defgroup HAL_TIME Time related functions
 *
 * @{
 */

struct timespechal { uint32_t tv_sec; uint32_t tv_nsec; };

/**
 * Get the system real time in timespec representation
 *
 * \return 0 - ok, !0 - failed
 */
HAL_API int
Hal_getRealTimeSpec(struct timespechal *ts);

/**
 * Get the system time in milliseconds.
 *
 * The time value returned as 64-bit unsigned integer should represent the milliseconds
 * since the UNIX epoch (1970/01/01 00:00 UTC).
 *
 * \return the system time with millisecond resolution.
 */
HAL_API uint64_t
Hal_getTimeInMs(void);

/**
 * Get the system monotonic time in milliseconds.
 *
 * \return the system time with millisecond resolution.
 */
HAL_API uint64_t
Hal_getMonotonicTimeInMs(void);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif


#endif /* HAL_TIME_H */
