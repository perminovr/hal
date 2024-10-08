#ifndef HAL_SERIAL_PORT_H
#define HAL_SERIAL_PORT_H


#include "hal_base.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * \file hal_serial.h
 * \brief Abstraction layer for serial ports.
 */

/*! \addtogroup hal Platform (Hardware/OS) abstraction layer
   *
   *  @{
   */

/**
 * @defgroup HAL_SERIAL Access to serial interfaces
 *
 * @{
 */


typedef struct sSerialPort* SerialPort;

typedef enum {
	SERIAL_PORT_ERROR_NONE = 0,
	SERIAL_PORT_ERROR_INVALID_ARGUMENT = 1,
	SERIAL_PORT_ERROR_INVALID_BAUDRATE = 2,
	SERIAL_PORT_ERROR_OPEN_FAILED = 3,
	SERIAL_PORT_ERROR_UNKNOWN = 99
} SerialPortError;


/**
 * \brief Create a new SerialPort instance
 *
 * \param interfaceName identifier or name of the serial interface (e.g. "/dev/ttyS1" or "COM4")
 *
 * \return the new SerialPort instance
 */
HAL_API SerialPort
SerialPort_create(const char *interfaceName);

/**
 * \brief Reinit SerialPort
 *
 * \param baudRate the baud rate in baud (e.g. 9600)
 * \param dataBits the number of data bits (usually 8)
 * \param parity defines what kind of parity to use ('E' - even parity, 'O' - odd parity, 'N' - no parity)
 * \param stopBits the number of stop buts (usually 1)
 *
 * \return true in case of success, false otherwise
 */
HAL_API bool
SerialPort_reinit(SerialPort self, int baudRate, uint8_t dataBits, char parity, uint8_t stopBits);

/**
 * \brief Destroy the SerialPort instance and release all resources
 */
HAL_API void
SerialPort_destroy(SerialPort self);

/**
 * \brief Open the serial interface
 *
 * \return true in case of success, false otherwise (use \ref SerialPort_getLastError for a detailed error code)
 */
HAL_API bool
SerialPort_open(SerialPort self);

/**
 * \brief Close (release) the serial interface
 */
HAL_API void
SerialPort_close(SerialPort self);

/**
 * \brief Get the baudrate used by the serial interface
 *
 * \return the baud rate in baud
 */
HAL_API int
SerialPort_getBaudRate(SerialPort self);

/**
 * \brief Set the timeout used for message reception
 *
 * \param timeout the timeout value in ms.
 */
HAL_API void
SerialPort_setTimeout(SerialPort self, int timeout);

/**
 * \brief Discard all data in the input buffer of the serial interface
 */
HAL_API void
SerialPort_discardInBuffer(SerialPort self);

/**
 * \brief Read a byte from the interface
 *
 * \return number of read bytes of -1 in case of an error
 */
HAL_API int
SerialPort_readByte(SerialPort self);

/**
 * \brief Read the number of bytes from the serial interface to the buffer
 *
 * \param buffer the buffer containing the data after read
 * \param numberOfBytes number of bytes available in the buffer
 *
 * \return number of bytes readden, or -1 in case of an error
 */
HAL_API int
SerialPort_read(SerialPort self, uint8_t *buffer, int numberOfBytes);

/**
 * \brief Write the number of bytes from the buffer to the serial interface
 *
 * \param buffer the buffer containing the data to write
 * \param numberOfBytes number of bytes to write
 *
 * \return number of bytes written, or -1 in case of an error
 */
HAL_API int
SerialPort_write(SerialPort self, uint8_t *buffer, int numberOfBytes);

/**
 * \brief Same as @ref SerialPort_write, but wait for pending output to be written
 */
HAL_API int
SerialPort_writeAndWait(SerialPort self, uint8_t *buffer, int numberOfBytes);

/**
 * \brief Get the error code of the last operation
 */
HAL_API SerialPortError
SerialPort_getLastError(SerialPort self);

/**
 * \brief Get the system descriptor
 */
HAL_API unidesc
SerialPort_getDescriptor(SerialPort self);


/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif


#endif /* HAL_SERIAL_PORT_H */
