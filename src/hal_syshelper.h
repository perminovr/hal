
#ifndef HAL_SYSHELPER_H
#define HAL_SYSHELPER_H

#include "hal_base.h"
#include "hal_socket_stream.h"
#include "hal_socket_dgram.h"

typedef int (*hsfh_spec_rw_cb_t)(void *user, uint8_t *buffer, int bufSize);

HAL_INTERNAL void HalShSys_init(void);
HAL_INTERNAL void HalShSys_deinit(void);

// STREAM

typedef struct sShDescStream *ShDescStream;

HAL_INTERNAL ShDescStream ShDescStream_create(unidesc h, uint32_t rwBufSize, int pauseCharMs);
HAL_INTERNAL void ShDescStream_destroy(ShDescStream self);
HAL_INTERNAL unidesc ShDescStream_getDescriptor(ShDescStream self);
HAL_INTERNAL int ShDescStream_write(ShDescStream self, const uint8_t *buf, int size);
HAL_INTERNAL int ShDescStream_read(ShDescStream self, uint8_t *buf, int size);
HAL_INTERNAL int ShDescStream_readAvailable(ShDescStream self);
HAL_INTERNAL void ShDescStream_setSpecCallbacks(ShDescStream self, void *user, hsfh_spec_rw_cb_t rcb, hsfh_spec_rw_cb_t wcb);

// DGRAM

typedef struct sShDescDgram *ShDescDgram;

HAL_INTERNAL ShDescDgram ShDescDgram_create(unidesc h, uint32_t rwBufSize);
HAL_INTERNAL void ShDescDgram_destroy(ShDescDgram self);
HAL_INTERNAL unidesc ShDescDgram_getDescriptor(ShDescDgram self);
HAL_INTERNAL int ShDescDgram_write(ShDescDgram self, const uint8_t *buf, int size);
HAL_INTERNAL int ShDescDgram_read(ShDescDgram self, uint8_t *buf, int size);
HAL_INTERNAL int ShDescDgram_readAvailable(ShDescDgram self);
HAL_INTERNAL int ShDescDgram_peek(ShDescDgram self, uint8_t *buf, int size);
HAL_INTERNAL void ShDescDgram_setSpecCallbacks(ShDescDgram self, void *user, hsfh_spec_rw_cb_t rcb, hsfh_spec_rw_cb_t wcb);

#endif // HAL_SYSHELPER_H