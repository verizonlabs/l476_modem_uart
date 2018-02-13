/**
 * \file rw_port.h
 * \copyright Copyright (C) 2017 Verizon. All rights reserved.
 * \brief Defines the interface to a read-write port.
 */

#ifndef _RW_PORT_H
#define _RW_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * \brief Port receive callback.
 * \details This callback is invoked when data is received on the port.
 * \param[out] sz Number of bytes received.
 * \param[out] data Pointer to the buffer holding the received data.
 */
typedef void (*rw_port_rx_cb)(size_t sz, const uint8_t data[]);

/**
 * \brief Port transmit callback.
 * \details After a call to the transmit function, this callback is invoked with
 * a result code.
 * \param[out] sz Number of bytes actually transmitted.
 * \param[out] err Flag that is set to true when there was an error transmitting.
 */
typedef void (*rw_port_tx_cb)(size_t sz, bool err);

/**
 * \brief Type defining the interface to the port peripheral.
 * \note All function pointers in this interface must be defined.
 */
typedef struct {
	/**
	 * \brief Initialize the port peripheral.
	 * \details Use this routine to setup the port along with transmit and
	 * receive callbacks. For example, if a port represents a UART peripheral,
	 * calling this routine will set the correct baud rate, parity settings
	 * etc.
	 *
	 * \param[in] tx_cb Pointer to callback that's invoked at the end of an
	 * attempt to transmit.
	 * \param[in] rx_cb Pointer to callback that's invoked on receiving data.
	 *
	 * \retval true On successful initialization
	 * \retval false Initialization failed.
	 * \note This routine must be called before all other routines in this
	 * interface.
	 */
	bool (*init)(rw_port_tx_cb tx_cb, rw_port_rx_cb rx_cb);

	/**
	 * \brief Transmit bytes through the port.
	 * \param[in] sz Number of bytes to transmit.
	 * \param[in] data Pointer to buffer holding the data to be sent.
	 * \pre \ref init must be called before using this routine.
	 */
	void (*tx)(size_t sz, const uint8_t data[]);

	/**
	 * \brief Request a read from the device on the remote end of the port.
	 * \details The result of the read will be notified through the \ref
	 * rw_port_rx_cb.
	 * \param[in] sz Number of bytes to read.
	 * \param[in] data Pointer to buffer where the data will be read into.
	 * \pre \ref init must be called before using this routine.
	 */
	void (*rx)(size_t sz);
} rw_port_t;

#endif
