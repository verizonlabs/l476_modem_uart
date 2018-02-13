/* Copyright (c) 2017 Verizon. All rights reserved. */
#ifndef _MODEM_PORT_H
#define _MODEM_PORT_H

#include "rw_port.h"

bool modem_port_init(rw_port_tx_cb tx_cb, rw_port_rx_cb rx_cb);
void modem_port_tx(size_t sz, const uint8_t data[]);
void modem_hw_init(void);
void modem_hw_reset(void);

#endif
