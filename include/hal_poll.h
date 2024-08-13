#ifndef HAL_POLL_H
#define HAL_POLL_H


#include "hal_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * \file hal_poll.h
 * \brief Abstraction layer for posix poll
 */

/*! \addtogroup hal
   *
   *  @{
   */

/**
 * @defgroup HAL_POLL Poll API
 *
 * @{
 */


/* Maximum pollfd size */
#define HAL_POLL_MAX	64
#define HALPOLL_AUTO_REALLOC_STEP 16


/* Event types that can be polled for.  These bits may be set in `events'
   to indicate the interesting event types; they will appear in `revents'
   to indicate the status of the file descriptor.  */
#define HAL_POLLIN		0x001		/* There is data to read.  */
#define HAL_POLLPRI		0x002		/* There is urgent data to read. Irq for ex */
#define HAL_POLLOUT		0x004		/* Writing now will not block.  */

/* Event types always implicitly polled for.  These bits need not be set in
   `events', but they will appear in `revents' to indicate the status of
   the file descriptor.  */
#define HAL_POLLERR		0x008		/* Error condition.  */
#define HAL_POLLHUP		0x010		/* Hung up.  */
#define HAL_POLLNVAL	0x020		/* Invalid polling request.  */

#define HAL_POLL_FOREVER   -1
#define HAL_POLL_NOWAIT    0

/** Opaque reference of a Poll instance */
typedef struct sPollfd *Pollfd;

/** Callback for events on hal object */
#ifdef HALCPPDEFINED
typedef std::function<void(void *user, void *object, int revents)> PollfdReventsHandlerCpp;
#endif // HALCPPDEFINED
typedef void (*PollfdReventsHandler)(void *user, void *object, int revents);

struct sPollfd {
	unidesc fd;			/* File descriptor to poll.  */
	int events;			/* Types of events poller cares about.  */
	int revents;		/* Types of events that actually occurred.  */
	void *object;		/* Hal wrapped object eg socket, port, etc. Provided by user for himself */
	void *user;			/* Some user data. Provided by user for himself */
	PollfdReventsHandler handler;	/* Callback for events on hal object. Provided by user for himself */
	HALDEFCPP(PollfdReventsHandlerCpp cpphandler;)
};


/**
 * \brief  Poll the file descriptors.
 *
 * \param pfd the file descriptors
 * \param size size of pfd. Must be less then HAL_POLL_MAX
 * \param timeout in milliseconds. If TIMEOUT is nonzero and not -1, allow TIMEOUT milliseconds for
 *  an event to occur; if TIMEOUT is -1, block until an event occurs.
 *
 * \details
 * Example 1:
 * 		struct sPollfd pfd[1];							// define array of poll structures
 * 		ClientSocket pcs = TcpClientSocket_create();	// create socket for poll
 * 		// ... some usefull actions (connect, etc)
 * 		pfd[0].fd = ClientSocket_getDescriptor(pcs);	// assign file descriptor
 * 		pfd[0].events = HAL_POLLIN;						// set poll for input data
 * 		pfd[0].object = pcs;							// store hal object
 * 		int rc = Hal_poll(pfd, 1, 1000);				// start poll for one socket until 1000 ms
 * 		if (rc > 0) {									// some events were occurred
 * 			if (pfd[0].revents & HAL_POLLIN) {			// our socket has input data
 * 				ClientSocket pcs1 = (ClientSocket)pfd[0].object; // pcs1 could be used as pcs
 * 				// ClientSocket_read(pcs1, ... );
 * 			}
 * 		}
 *
 * Example 2:
 * 		void user_handler1(void *user, void *object, int revents) {
 * 			if (revents & HAL_POLLIN) {
 * 				ClientSocket pcs = (ClientSocket)object;
 * 				// ClientSocket_read(pcs, ... );
 * 			}
 * 		}
 * 		// ...
 * 		void main() {
 * 			struct sPollfd pfd[1];							// define array of poll structures
 * 			ClientSocket pcs = TcpClientSocket_create();	// create socket for poll
 * 			// ... some usefull actions (connect, etc)
 * 			pfd[0].fd = ClientSocket_getDescriptor(pcs);	// assign file descriptor
 * 			pfd[0].events = HAL_POLLIN;						// set poll for input data
 * 			pfd[0].object = pcs;							// store hal object
 * 			pfd[0].handler = user_handler1;					// stick handler
 * 			int rc = Hal_poll(pfd, 1, 1000);				// start poll for one socket until 1000 ms
 * 			if (rc > 0) {									// some events were occurred
 * 				// for (i : pfd_size) 						// thru all pfd array
 * 				// 		pfd[i].handler(0, pfd[i].object, pfd[i].revents);
 * 			}
 * 		}
 *
 * \return the number of file descriptors with events, zero if timed out,
 *  or -1 for errors.
*/
HAL_API int
Hal_poll(Pollfd pfd, unsigned long int size, int timeout);

/**
 * \brief  Poll single file descriptors. Acts like \ref Hal_poll
 *
 * \param revents - can be NULL pointer
 *
*/
HAL_API int
Hal_pollSingle(unidesc fd, int events, int *revents, int timeout);



/** Opaque reference of a HalPoll instance */
typedef struct sHalPoll *HalPoll;


/**
 * \brief Create HalPoll instance
 *
 * \param maxSize - maximum available size for descriptors in the poll queue
 *
 * \return a new HalPoll instance.
*/
HAL_API HalPoll
HalPoll_create(int maxSize);

/**
 * \brief Update or add descriptor to poll queue
 *
 * \param self - a HalPoll instance
 * \param fd - unified system descriptor
 * \param events - poll events (HAL_POLLIN, etc)
 * \param object - hal wrapped object eg socket, port, etc. Provided by user for himself
 * \param user - some user data. Provided by user for himself
 * \param handler - callback for events on hal object. Provided by user for himself
 *
 * \return true in case of success, false then maximum size of the poll queue was reached
*/
HAL_API bool
HalPoll_update(HalPoll self, unidesc fd, int events, void *object, void *user, PollfdReventsHandler handler);

/**
 * \brief Update or add descriptor to poll queue
 *
 * \param self - a HalPoll instance
 * \param fd - unified system descriptor
 * \param events - poll events (HAL_POLLIN, etc)
 *
 * \return true in case of success, false then maximum size of the poll queue was reached
*/
HAL_API bool
HalPoll_update_1(HalPoll self, unidesc fd, int events);

/**
 * \brief Update or add descriptor to poll queue
 *
 * \param self - a HalPoll instance
 * \param fd - unified system descriptor
 * \param object - hal wrapped object eg socket, port, etc. Provided by user for himself
 *
 * \return true in case of success, false then maximum size of the poll queue was reached
*/
HAL_API bool
HalPoll_update_2(HalPoll self, unidesc fd, void *object);

/**
 * \brief Update or add descriptor to poll queue
 *
 * \param self - a HalPoll instance
 * \param fd - unified system descriptor
 * \param user - some user data. Provided by user for himself
 * \param handler - callback for events on hal object. Provided by user for himself
 *
 * \return true in case of success, false then maximum size of the poll queue was reached
*/
HAL_API bool
HalPoll_update_3(HalPoll self, unidesc fd, void *user, PollfdReventsHandler handler);

/**
 * \brief Update descriptor from poll queue
 *
 * \param self - a HalPoll instance
 * \param old_fd - old unified system descriptor
 * \param new_fd - new unified system descriptor
 *
 * \return true in case of success, false otherwise
*/
HAL_API bool
HalPoll_update_4(HalPoll self, unidesc old_fd, unidesc new_fd);

#ifdef HALCPPDEFINED
/**
 * \brief Update or add descriptor to poll queue
 *
 * \param self - a HalPoll instance
 * \param fd - unified system descriptor
 * \param events - poll events (HAL_POLLIN, etc)
 * \param object - hal wrapped object eg socket, port, etc. Provided by user for himself
 * \param user - some user data. Provided by user for himself
 * \param handler - callback for events on hal object. Provided by user for himself
 *
 * \return true in case of success, false then maximum size of the poll queue was reached
*/
HAL_API bool
HalPoll_update_cpp(HalPoll self, unidesc fd, int events, void *object, void *user, PollfdReventsHandlerCpp handler);

/**
 * \brief Update or add descriptor to poll queue
 *
 * \param self - a HalPoll instance
 * \param fd - unified system descriptor
 * \param user - some user data. Provided by user for himself
 * \param handler - callback for events on hal object. Provided by user for himself
 *
 * \return true in case of success, false then maximum size of the poll queue was reached
*/
HAL_API bool
HalPoll_update_cpp_3(HalPoll self, unidesc fd, void *user, PollfdReventsHandlerCpp handler);
#endif // HALCPPDEFINED

/**
 * \brief Remove descriptor from poll queue
 *
 * \param self - a HalPoll instance
 * \param fd - Unified system descriptor
 *
 * \return true in case of success, false then descriptor is invalid or the poll queue does not contain this descriptor
*/
HAL_API bool
HalPoll_remove(HalPoll self, unidesc fd);

/**
 * \brief Move descriptor inside the poll queue based on priority
 *
 * \param self - a HalPoll instance
 * \param fd - Unified system descriptor
 * \param priority - 0..n-1 - descriptor position, -(n)..-1 - inverted position: -1 = n-1, -2 = n-2, ..,  -n = 0
 *
 * \return true in case of moving done
*/
HAL_API bool
HalPoll_updatePriority(HalPoll self, unidesc fd, int priority);

/**
 * \brief Remove all descriptors from poll queue
*/
HAL_API void
HalPoll_clear(HalPoll self);

/**
 * \brief Wait for events on descriptors
 *
 * \param self - a HalPoll instance
 * \param timeout - Unified system descriptor
 *
 * \return -1 on error ; 0 on timeout ; >0 on events
*/
HAL_API int
HalPoll_wait(HalPoll self, int timeout);

/**
 * \brief Break HalPoll_wait callbacks calling cycle
 *
 * \param self - a HalPoll instance
*/
HAL_API void
HalPoll_breakEventCycle(HalPoll self);

/**
 * \brief Resize HalPoll object
 *
 * \param maxSize - maximum available size for descriptors in the poll queue
 * \details Increasing only
 *
 * \return true in case of success, false otherwise
*/
HAL_API bool
HalPoll_resize(HalPoll self, int maxSize);

HAL_API int
HalPoll_size(HalPoll self);

HAL_API int
HalPoll_maxSize(HalPoll self);

/**
 * \brief Enable or disable auto realloc on update 
 * if there is no memory for the new object
 *
 * \param self - a HalPoll instance
*/
HAL_API void
HalPoll_setAutoRealloc(HalPoll self, bool enable);

/**
 * \brief Destroy HallPoll instance
 *
 * \param self - a HalPoll instance
*/
HAL_API void
HalPoll_destroy(HalPoll self);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif


#endif /* HAL_POLL_H */