
#ifndef HAL_SYSHELPER_H
#define HAL_SYSHELPER_H

#include "hal_base.h"
#include "hal_socket_stream.h"
#include "hal_socket_dgram.h"

typedef int (*hsfh_spec_rw_cb_t)(void *user, uint8_t *buffer, int bufSize);

void HalShSys_init(void);
void HalShSys_deinit(void);

// STREAM

typedef struct sShDescStream *ShDescStream;

ShDescStream ShDescStream_create(unidesc h, uint32_t rwBufSize, int pauseCharMs);
void ShDescStream_destroy(ShDescStream self);
unidesc ShDescStream_getDescriptor(ShDescStream self);
int ShDescStream_write(ShDescStream self, const uint8_t *buf, int size);
int ShDescStream_read(ShDescStream self, uint8_t *buf, int size);
void ShDescStream_setSpecCallbacks(ShDescStream self, void *user, hsfh_spec_rw_cb_t rcb, hsfh_spec_rw_cb_t wcb);

// DGRAM

typedef struct sShDescDgram *ShDescDgram;

ShDescDgram ShDescDgram_create(unidesc h, uint32_t rwBufSize);
void ShDescDgram_destroy(ShDescDgram self);
unidesc ShDescDgram_getDescriptor(ShDescDgram self);
int ShDescDgram_write(ShDescDgram self, const uint8_t *buf, int size);
int ShDescDgram_read(ShDescDgram self, uint8_t *buf, int size);
void ShDescDgram_setSpecCallbacks(ShDescDgram self, void *user, hsfh_spec_rw_cb_t rcb, hsfh_spec_rw_cb_t wcb);

#endif // HAL_SYSHELPER_H