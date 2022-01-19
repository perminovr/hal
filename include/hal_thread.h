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
PAL_API Thread
Thread_create(size_t stackSize, ThreadExecutionFunction function, void *parameter, bool autodestroy);

/**
 * \brief Start a Thread.
 *
 * This function invokes the start function of the thread. The thread terminates when
 * the start function returns.
 *
 * \param thread the Thread instance to start
 */
PAL_API void
Thread_start(Thread thread);

/**
 * \brief Destroy a Thread and free all related resources.
 *
 * \param thread the Thread instance to destroy
 */
PAL_API void
Thread_destroy(Thread thread);

/**
 * \brief Suspend execution of the Thread for the specified number of milliseconds
 */
PAL_API void
Thread_sleep(int millies);

PAL_API void
Thread_yield(void);

/**
 * \brief Send cancel signal to the thread
 */
PAL_API void
Thread_cancel(Thread thread);

/**
 * \brief Check for cancel condition on the thread
 */
PAL_API void
Thread_testCancel(Thread thread);

/**
 * \brief Return native system descriptor
 */
PAL_API unidesc
Thread_getNativeDescriptor(Thread thread);

/**
 * \brief Set signal that will be raised on \ref Thread_cancel call
 */
PAL_API void
Thread_setCancelSignal(Thread thread, Signal signal);


PAL_API Semaphore
Semaphore_create(int initialValue);

/* Wait until semaphore value is greater than zero. Then decrease the semaphore value. */
PAL_API void
Semaphore_wait(Semaphore self);

PAL_API void
Semaphore_post(Semaphore self);

PAL_API void
Semaphore_destroy(Semaphore self);


PAL_API Mutex
Mutex_create(void);

PAL_API void
Mutex_lock(Mutex self);

PAL_API int
Mutex_trylock(Mutex self);

/* Wait until lock becomes available, or specified time passes. */
PAL_API int
Mutex_timedlock(Mutex self, int millies);

PAL_API void
Mutex_unlock(Mutex self);

PAL_API void
Mutex_destroy(Mutex self);


PAL_API SmartMutex
SmartMutex_create(void);

PAL_API void
SmartMutex_lock(SmartMutex self);

PAL_API void
SmartMutex_unlock(SmartMutex self);

PAL_API void
SmartMutex_destroy(SmartMutex self);


PAL_API Signal
Signal_create(void);

PAL_API void
Signal_raise(Signal self);

PAL_API void
Signal_end(Signal self);

PAL_API bool
Signal_event(Signal self);

PAL_API void
Signal_destroy(Signal self);

PAL_API unidesc
Signal_getNativeDescriptor(Signal self);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif

#endif /* HAL_THREAD_H */