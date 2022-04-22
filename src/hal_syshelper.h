
#ifndef HAL_SYSHELPER_H
#define HAL_SYSHELPER_H

#include "hal_base.h"
#include "hal_socket_stream.h"

typedef struct sHalShFdesc *HalShFdesc;
struct sHalShFdesc {
	unidesc h; // system object
	struct { // helper socks
		ServerSocket s;
		ClientSocket i; // internal
		ClientSocket e; // external
	};
};

void HalShSys_init(void);
void HalShSys_deinit(void);

HalShFdesc HalShFdesc_create(unidesc h);
void HalShFdesc_destroy(HalShFdesc self);
unidesc HalShFdesc_getDescriptor(HalShFdesc self);

typedef struct sHalShFdescHelper *HalShFdescHelper;

HalShFdescHelper HalShFdescHelper_create(unidesc h, uint32_t wrBufSize, int pauseCharMs);
void HalShFdescHelper_destroy(HalShFdescHelper self);
HalShFdesc HalShFdescHelper_getDescriptor(HalShFdescHelper self);
int HalShFdescHelper_write(HalShFdescHelper self, const uint8_t *buf, int size);
int HalShFdescHelper_read(HalShFdescHelper self, uint8_t *buf, int size);

#endif // HAL_SYSHELPER_H