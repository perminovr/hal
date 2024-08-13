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

/** Qpaque reference of a RwLock instance */
typedef void *RwLock;

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
HalThread_create(size_t stackSize, ThreadExecutionFunction function, void *parameter, bool autodestroy);

/**
 * \brief Start a Thread.
 *
 * This function invokes the start function of the thread. The thread terminates when
 * the start function returns.
 *
 * \param thread the Thread instance to start
 */
HAL_API void
HalThread_start(Thread thread);

/**
 * \brief Join the thread
 */
HAL_API void
HalThread_join(Thread thread);

/**
 * \brief Destroy a Thread and free all related resources.
 *
 * \param thread the Thread instance to destroy
 */
HAL_API void
HalThread_destroy(Thread thread);

/**
 * \brief Suspend execution of the Thread for the specified number of milliseconds
 */
HAL_API void
HalThread_sleep(int millies);

/**
 * \brief Switch executing thread
 */
HAL_API void
HalThread_yield(void);

/**
 * \brief Send cancel signal to the thread
 */
HAL_API void
HalThread_cancel(Thread thread);

/**
 * \brief Check for cancel condition on the thread
 */
HAL_API void
HalThread_testCancel(Thread thread);

/**
 * \brief Get signal that will be raised on \ref HalThread_cancel call
 * \details the signal shoud be polled
 */
HAL_API Signal
HalThread_getCancelSignal(Thread thread);

/**
 * \brief Send pause signal to the thread
 */
HAL_API void
HalThread_pause(Thread thread);

/**
 * \brief Resume the thread
 */
HAL_API void
HalThread_resume(Thread thread);

/**
 * \brief Check for pause condition on the thread
 */
HAL_API void
HalThread_testPause(Thread thread);

/**
 * \brief Get signal that will be raised on \ref HalThread_pause call
 */
HAL_API Signal
HalThread_getPauseSignal(Thread thread);

/**
 * \brief Set thread name
 */
HAL_API void
HalThread_setName(Thread thread, const char *name);

/**
 * \brief Return native system descriptor
 */
HAL_API unidesc
HalThread_getNativeDescriptor(Thread thread);

/**
 * \brief Return current thread native system descriptor
 */
HAL_API unidesc
HalThread_getCurrentThreadNativeDescriptor(void);


HAL_API Semaphore
HalSemaphore_create(int initialValue);

/* Wait until semaphore value is greater than zero. Then decrease the semaphore value. */
HAL_API void
HalSemaphore_wait(Semaphore self);

HAL_API void
HalSemaphore_post(Semaphore self);

HAL_API void
HalSemaphore_destroy(Semaphore self);


HAL_API Mutex
HalMutex_create(void);

HAL_API void
HalMutex_lock(Mutex self);

HAL_API int
HalMutex_trylock(Mutex self);

/* Wait until lock becomes available, or specified time passes. */
HAL_API int
HalMutex_timedlock(Mutex self, int millies);

HAL_API void
HalMutex_unlock(Mutex self);

HAL_API void
HalMutex_destroy(Mutex self);


HAL_API RwLock
HalRwLock_create(void);

HAL_API void
HalRwLock_wlock(RwLock self);

HAL_API void
HalRwLock_rlock(RwLock self);

HAL_API void
HalRwLock_unlock(RwLock self);

HAL_API void
HalRwLock_destroy(RwLock self);


#define SmartMutex Mutex
#define SmartHalMutex_create HalMutex_create
#define SmartHalMutex_lock HalMutex_lock
#define SmartHalMutex_unlock HalMutex_unlock


HAL_API Signal
HalSignal_create(void);

HAL_API void
HalSignal_raise(Signal self);

HAL_API void
HalSignal_end(Signal self);

HAL_API bool
HalSignal_event(Signal self);

HAL_API void 
HalSignal_wait(Signal self);

HAL_API void
HalSignal_destroy(Signal self);

HAL_API unidesc
HalSignal_getDescriptor(Signal self);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif

#endif /* HAL_THREAD_H */