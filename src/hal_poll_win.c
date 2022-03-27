
#if defined(_WIN32) || defined(_WIN64)

#include "hal_poll.h"
#include <winsock2.h>


static inline int poll( struct pollfd *pfd, unsigned long int nfds, int timeout) 
{
	return WSAPoll ( (LPWSAPOLLFD)pfd, (ULONG)nfds, (INT)timeout ); 
}

static inline int events_win_to_hal0(SHORT pollev)
{
	switch (pollev) {
		case POLLRDNORM: case POLLRDBAND: case POLLIN: return HAL_POLLIN;
		case POLLOUT: return HAL_POLLOUT;
		case POLLPRI: return HAL_POLLPRI;
		case POLLERR: return HAL_POLLERR;
		case POLLHUP: return HAL_POLLHUP;
		case POLLNVAL: return HAL_POLLNVAL;
		default: return 0;
	}
}
static int events_win_to_hal(SHORT pollev)
{
	const SHORT pollarr[] = { 
		POLLIN, POLLOUT, POLLPRI, 
		POLLERR, POLLHUP, POLLNVAL 
	};
	int ret = 0;
	for (int i = 0; i < 6; ++i) {
		ret |= events_win_to_hal0( pollev & pollarr[i] );
	}
	return ret;
}

static inline SHORT events_hal_to_win0(int pollev)
{
	switch (pollev) {
		case HAL_POLLIN: return POLLIN;
		case HAL_POLLOUT: return POLLOUT;
		case HAL_POLLPRI: return POLLPRI;
		case HAL_POLLERR: return POLLERR;
		case HAL_POLLHUP: return POLLHUP;
		case HAL_POLLNVAL: return POLLNVAL;
		default: return 0;
	}
}
static SHORT events_hal_to_win(int pollev)
{
	const int pollarr[] = { 
		HAL_POLLIN, HAL_POLLOUT, HAL_POLLPRI, 
		HAL_POLLERR, HAL_POLLHUP, HAL_POLLNVAL 
	};
	SHORT ret = 0;
	for (int i = 0; i < 6; ++i) {
		ret |= events_hal_to_win0( pollev & pollarr[i] );
	}
	return ret;
}


int Hal_poll(Pollfd pfd, unsigned long int size, int timeout)
{
	unsigned long int i;
	if (pfd == NULL) return -1;

	int ret = 0;
	struct pollfd lpfd[HAL_POLL_MAX];

	if (size > HAL_POLL_MAX) return -1;

	for (i = 0; i < size; ++i) {
		lpfd[i].fd = pfd[i].fd.u64;
		lpfd[i].events = events_hal_to_win(pfd[i].events);
		lpfd[i].revents = 0;
	}

	ret = poll(lpfd, size, timeout);

	for (i = 0; i < size; ++i) {
		pfd[i].revents = events_win_to_hal(lpfd[i].revents);
	}

	return ret;
}


int Hal_pollSingle(unidesc fd, int events, int *revents, int timeout)
{
	if (Hal_unidescIsInvalid(fd)) return -1;

	int ret = 0;
	struct pollfd lpfd;
	lpfd.fd = fd.u64;
	lpfd.events = events_hal_to_win(events);
	lpfd.revents = 0;

	ret = poll(&lpfd, 1, timeout);
	if (revents) *revents = events_win_to_hal(lpfd.revents);

	return ret;
}


typedef struct {
	void *object;
	void *user;
	PollfdReventsHandler handler;
	HALDEFCPP(PollfdReventsHandlerCpp cpphandler;)
} EventObject;

struct sHalPoll {
	EventObject *objects;
	struct pollfd *pfd;
	int maxSize;
	int size;
	void *user;
	bool updated;
};


HalPoll HalPoll_create(int maxSize)
{
	HalPoll self = (HalPoll)calloc(1, sizeof(struct sHalPoll));
	if (self) {
		self->objects = (EventObject *)calloc(maxSize, sizeof(EventObject));
		if (!self->objects) goto exit_self;
		self->pfd = (struct pollfd *)calloc(maxSize, sizeof(struct pollfd));
		if (!self->pfd) goto exit_objects;
		self->maxSize = maxSize;
	}
	return self;

exit_objects:
	free(self->objects);
exit_self:
	free(self);
	return NULL;
}


static inline int getFdIndex(HalPoll self, unidesc fd)
{
	int ret = 0;
	for (; ret < self->size; ++ret) {
		if ((SOCKET)fd.u64 == self->pfd[ret].fd) {
			break;
		}
	}
	return ret;
}


static inline void setSysPollfd(HalPoll self, int idx, uint64_t fd, int events, int revents, bool iswin)
{
	self->pfd[idx].fd = (SOCKET)fd;
	self->pfd[idx].events = (iswin)? events : events_hal_to_win(events);
	self->pfd[idx].revents = (iswin)? revents : events_hal_to_win(revents);
}


bool HalPoll_update(HalPoll self, unidesc fd, int events, void *object, void *user, PollfdReventsHandler handler)
{
	if (self == NULL) return false;
	int i = getFdIndex(self, fd);
	if (i == self->size) { // add new
		if (self->size >= self->maxSize) return false;
		setSysPollfd(self, i, fd.u64, events, 0, false);
		self->objects[i].object = object;
		self->objects[i].user = user;
		self->objects[i].handler = handler;
		self->size++;
	} else { // update old
		setSysPollfd(self, i, fd.u64, events, 0, false);
		self->objects[i].object = object;
		self->objects[i].user = user;
		self->objects[i].handler = handler;
	}
	self->updated = true;
	return true;
}


bool HalPoll_update_1(HalPoll self, unidesc fd, int events)
{
	if (self == NULL) return false;
	int i = getFdIndex(self, fd);
	if (i == self->size) { // add new
		if (self->size >= self->maxSize) return false;
		setSysPollfd(self, i, fd.u64, events, 0, false);
		self->size++;
	} else { // update old
		setSysPollfd(self, i, fd.u64, events, 0, false);
	}
	self->updated = true;
	return true;
}


bool HalPoll_update_2(HalPoll self, unidesc fd, void *object)
{
	if (self == NULL) return false;
	int i = getFdIndex(self, fd);
	if (i == self->size) { // add new
		if (self->size >= self->maxSize) return false;
		setSysPollfd(self, i, fd.u64, 0, 0, false);
		self->objects[i].object = object;
		self->size++;
	} else { // update old
		self->objects[i].object = object;
	}
	self->updated = true;
	return true;
}


bool HalPoll_update_3(HalPoll self, unidesc fd, void *user, PollfdReventsHandler handler)
{
	if (self == NULL) return false;
	int i = getFdIndex(self, fd);
	if (i == self->size) { // add new
		if (self->size >= self->maxSize) return false;
		setSysPollfd(self, i, fd.u64, 0, 0, false);
		self->objects[i].user = user;
		self->objects[i].handler = handler;
		self->size++;
	} else { // update old
		self->objects[i].user = user;
		self->objects[i].handler = handler;
	}
	self->updated = true;
	return true;
}


bool HalPoll_update_4(HalPoll self, unidesc old_fd, unidesc new_fd)
{
	if (self == NULL) return false;
	if (Hal_unidescIsInvalid(new_fd)) return false;
	if (Hal_unidescIsEqual(&old_fd, &new_fd)) return true;
	int i = getFdIndex(self, old_fd);
	if (i == self->size) return false;
	setSysPollfd(self, i, new_fd.u64, self->pfd[i].events, 0, true);
	self->updated = true;
	return true;
}


#ifdef HALCPPDEFINED
bool HalPoll_update_cpp(HalPoll self, unidesc fd, int events, void *object, void *user, PollfdReventsHandlerCpp handler)
{
	if (self == NULL) return false;
	int i = getFdIndex(self, fd);
	if (i == self->size) { // add new
		if (self->size >= self->maxSize) return false;
		setSysPollfd(self, i, fd.u64, events, 0, false);
		self->objects[i].object = object;
		self->objects[i].user = user;
		self->objects[i].cpphandler = handler;
		self->size++;
	} else { // update old
		setSysPollfd(self, i, fd.u64, events, 0, false);
		self->objects[i].object = object;
		self->objects[i].user = user;
		self->objects[i].cpphandler = handler;
	}
	self->updated = true;
	return true;
}

bool HalPoll_update_cpp_3(HalPoll self, unidesc fd, void *user, PollfdReventsHandlerCpp handler)
{
	if (self == NULL) return false;
	int i = getFdIndex(self, fd);
	if (i == self->size) { // add new
		if (self->size >= self->maxSize) return false;
		setSysPollfd(self, i, fd.u64, 0, 0, false);
		self->objects[i].user = user;
		self->objects[i].cpphandler = handler;
		self->size++;
	} else { // update old
		self->objects[i].user = user;
		self->objects[i].cpphandler = handler;
	}
	self->updated = true;
	return true;
}
#endif // HALCPPDEFINED


bool HalPoll_remove(HalPoll self, unidesc fd)
{
	if (self == NULL) return false;
	if (Hal_unidescIsInvalid(fd)) return false;
	int i = getFdIndex(self, fd);
	if (i == self->size) {
		return false;
	} else {
		for (; i < self->size-1; ++i) {
			setSysPollfd(self, i, self->pfd[i+1].fd, self->pfd[i+1].events, self->pfd[i+1].revents, true);
			self->objects[i].object = self->objects[i+1].object;
			self->objects[i].user = self->objects[i+1].user;
			self->objects[i].handler = self->objects[i+1].handler;
			HALDEFCPP(self->objects[i].cpphandler = self->objects[i+1].cpphandler;)
		}
		i = self->size-1;
		setSysPollfd(self, i, Hal_getInvalidUnidesc().u64, 0, 0, false);
		self->objects[i].object = NULL;
		self->objects[i].user = NULL;
		self->objects[i].handler = NULL;
		HALDEFCPP(self->objects[i].cpphandler = NULL;)
		self->size--;
	}
	self->updated = true;
	return true;
}


int HalPoll_wait(HalPoll self, int timeout)
{
	if (self == NULL) return -1;

	self->updated = false;
	int handled = 0;
	int res = poll(self->pfd, (unsigned long int)self->size, timeout);

	if (res <= 0) { return res; }

	for (int i = 0; i < self->size; ++i) {
		if (self->pfd[i].revents == 0) continue;
		PollfdReventsHandler handler = self->objects[i].handler;
		if (handler) {
			handler(self->objects[i].user, self->objects[i].object, events_win_to_hal(self->pfd[i].revents));
		}
		#ifdef HALCPPDEFINED
		PollfdReventsHandlerCpp cpphandler = self->objects[i].cpphandler;
		if (cpphandler) {
			cpphandler(self->objects[i].user, self->objects[i].object, events_win_to_hal(self->pfd[i].revents));
		}
		#endif
		handled++;
		if (self->updated) { // HalPoll_update or HalPoll_remove call from handler corrupt our cycle
			return handled;
		}
	}

	return res;
}


void HalPoll_destroy(HalPoll self)
{
	free(self->pfd);
	free(self->objects);
	free(self);
}


#endif // _WIN32 || _WIN64