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


/**
 * Get the system time in milliseconds.
 *
 * The time value returned as 64-bit unsigned integer should represent the milliseconds
 * since the UNIX epoch (1970/01/01 00:00 UTC).
 *
 * \return the system time with millisecond resolution.
 */
PAL_API uint64_t
Hal_getTimeInMs(void);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif


#endif /* HAL_TIME_H */
