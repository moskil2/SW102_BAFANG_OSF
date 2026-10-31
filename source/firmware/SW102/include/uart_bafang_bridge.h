#ifndef _UART_BAFANG_BRIDGE_H_
#define _UART_BAFANG_BRIDGE_H_

#include <stdint.h>

/*
 * Bridge between this platform's low-level UART driver (uart.c, which
 * only owns the nRF UART peripheral - pins, baud rate, interrupts) and
 * the hardware-independent Bafang protocol/display-loop modules
 * (bafang_protocol.c / bafang_display.c), which know nothing about nRF
 * SDK types. uart.c calls the registered callback once per raw byte
 * received; the caller (main.c, once wired up) forwards it to
 * bafang_display_on_data_received().
 */
typedef void (*uart_data_callback_t)(uint8_t byte);

void uart_set_data_callback(uart_data_callback_t callback);

#endif
