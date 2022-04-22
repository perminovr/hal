#ifndef HAL_THREAD_H
#define HAL_THREAD_H


#include "hal_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * \file hal_thread.h
 * \brief Abstraction layer for threading and synchronization
 */

/*! \addtogroup hal
   *
   *  @{
   */

/**
 * @defgroup HAL_THREAD Threading and synchronization API
 *
 * @{
 */


/** Qpaque reference of a Semaphore instance */
typedef void *Semaphore;

/** Qpaque reference of a Mutex instance */
typedef void *Mutex;

/** Opaque reference of a Thread instance */
typedef struct sThread *Thread;

/** Opaque reference of a SmartMutex instance */
typedef struct sSmartMutex *SmartMutex;

/** Qpaque reference of a Signal instance */
typedef struct sSignal *Signal;

/** Reference to a function that is called when starting the thread */
typedef void* (*ThreadExecutionFunction) (void*);

#ifdef __cplusplus
class HalThreadedObject {
public:
	HalThreadedObject() = default;
	virtual ~HalThreadedObject() = default;
	virtual void threadLoop(void) = 0;
	static void *nativeThreadLoop(void *This) {
		((HalThreadedObject*)This)->threadLoop();
		return NULL;
	}
};
#endif


/**
 * \brief Create a new Thread instance
 *
 * \param stackSize thread stack size (0 means auto)
 * \param function the entry point of the thread
 * \param parameter a parameter that is passed to the threads start function
 * \param autodestroy the thread is automatically destroyed if the ThreadExecutionFunction has finished.
 *
 * \return the newly created Thread instance
 */
HAL_API Thread
Thread_create(size_t stackSize, ThreadExecutionFunction function, void *parameter, bool autodestroy);

/**
 * \brief Start a Thread.
 *
 * This function invokes the start function of the thread. The thread terminates when
 * the start function returns.
 *
 * \param thread the Thread instance to start
 */
HAL_API void
Thread_start(Thread thread);

/**
 * \brief Destroy a Thread and free all related resources.
 *
 * \param thread the Thread instance to destroy
 */
HAL_API void
Thread_destroy(Thread thread);

/**
 * \brief Suspend execution of the Thread for the specified number of milliseconds
 */
HAL_API void
Thread_sleep(int millies);

/**
 * \brief Switch executing thread
 */
HAL_API void
Thread_yield(void);

/**
 * \brief Send cancel signal to the thread
 */
HAL_API void
Thread_cancel(Thread thread);

/**
 * \brief Check for cancel condition on the thread
 */
HAL_API void
Thread_testCancel(Thread thread);

/**
 * \brief Get signal that will be raised on \ref Thread_cancel call
 * \details the signal shoud be polled
 */
HAL_API Signal
Thread_getCancelSignal(Thread thread);

/**
 * \brief Send pause signal to the thread
 */
HAL_API void
Thread_pause(Thread thread);

/**
 * \brief Resume the thread
 */
HAL_API void
Thread_resume(Thread thread);

/**
 * \brief Check for pause condition on the thread
 */
HAL_API void
Thread_testPause(Thread thread);

/**
 * \brief Get signal that will be raised on \ref Thread_pause call
 */
HAL_API Signal
Thread_getPauseSignal(Thread thread);

/**
 * \brief Set thread name
 */
HAL_API void
Thread_setName(Thread thread, const char *name);

/**
 * \brief Return native system descriptor
 */
HAL_API unidesc
Thread_getNativeDescriptor(Thread thread);


HAL_API Semaphore
Semaphore_create(int initialValue);

/* Wait until semaphore value is greater than zero. Then decrease the semaphore value. */
HAL_API void
Semaphore_wait(Semaphore self);

HAL_API void
Semaphore_post(Semaphore self);

HAL_API void
Semaphore_destroy(Semaphore self);


HAL_API Mutex
Mutex_create(void);

HAL_API void
Mutex_lock(Mutex self);

HAL_API int
Mutex_trylock(Mutex self);

/* Wait until lock becomes available, or specified time passes. */
HAL_API int
Mutex_timedlock(Mutex self, int millies);

HAL_API void
Mutex_unlock(Mutex self);

HAL_API void
Mutex_destroy(Mutex self);


#define SmartMutex Mutex
#define SmartMutex_create Mutex_create
#define SmartMutex_lock Mutex_lock
#define SmartMutex_unlock Mutex_unlock


HAL_API Signal
Signal_create(void);

HAL_API void
Signal_raise(Signal self);

HAL_API void
Signal_end(Signal self);

HAL_API bool
Signal_event(Signal self);

HAL_API void
Signal_destroy(Signal self);

HAL_API unidesc
Signal_getDescriptor(Signal self);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif

#endif /* HAL_THREAD_H */