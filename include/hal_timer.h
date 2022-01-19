#ifndef HAL_TIMER_H
#define HAL_TIMER_H


#include "hal_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * \file hal_timer.h
 * \brief Abstraction layer for timers
 */

/*! \addtogroup hal
   *
   *  @{
   */

/**
 * @defgroup HAL_TIMER Timer API
 *
 * @{
 */


typedef struct {
	uint32_t sec;
	uint32_t nsec;
} AccurateTime_t;


typedef struct sTimer *Timer;


/**
 * \brief Create Timer instance
 *
 * \return a new Timer instance.
*/
PAL_API Timer
Timer_create(void);

/**
 * \brief Setup the timer timeout. Will raise once
 *
 * \param timeout
 *
 * \return true in case of success, false otherwise
*/
PAL_API bool
Timer_setTimeout(Timer self, AccurateTime_t *timeout);

/**
 * \brief Repeat last timeout setup \ref Timer_setTimeout
 *
 * \return true in case of success, false otherwise
*/
PAL_API bool
Timer_repeatTimeout(Timer self);

/**
 * \brief Setup timer period. Will be raising periodically
 *
 * \param period
 *
 * \return true in case of success, false otherwise
*/
PAL_API bool
Timer_setPeriod(Timer self, AccurateTime_t *period);

/**
 * \brief Stop the timer (timeout and period)
 *
 * \return true in case of success, false otherwise
*/
PAL_API bool
Timer_stop(Timer self);

/**
 * \brief End the timer event. Should be called on raise event
 *
 * \return true in case of success, false otherwise
*/
PAL_API void
Timer_endEvent(Timer self);

/**
 * \brief Destroy Timer instance
*/
PAL_API void
Timer_destroy(Timer self);

/**
 * \brief Get the system descriptor
 */
PAL_API unidesc
Timer_getDescriptor(Timer self);


/**
 * \brief Set single shot timeout without Timer instance
 *
 * \return the system descriptor
*/
PAL_API unidesc
Timer_setSingleShot(AccurateTime_t *time);

/**
 * \brief End single shot timeout
*/
PAL_API void
Timer_endSingleShot(unidesc desc);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif


#endif // HAL_TIMER_H
