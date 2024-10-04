//#include <stdint.h>
#include "cvi_spinlock.h"
#include <stdint.h>
#include <stdbool.h>
#include <types.h>
#include "malloc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "mmio.h"
#include "delay.h"

//#include <stdint.h>
// #include "linux/types.h"
#include "uart.h"
#include "top_reg.h"

static struct dw_regs *uart = 0;

void hal_srv_uart_init(device_uart dev_uart, int baudrate, int uart_clock)
{
	int divisor = uart_clock / (16 * baudrate);
    divisor = 5;  //TODO: too lazy to actually round up to the closest properly lol

	switch (dev_uart) {
		case UART0:
			uart = (struct dw_regs *)UART0_BASE;
			break;
		case UART1:
			uart = (struct dw_regs *)UART1_BASE;
			break;
		case UART2:
			uart = (struct dw_regs *)UART2_BASE;
			break;
		case UART3:
			uart = (struct dw_regs *)UART3_BASE;
			break;
		default:
			break;
	}
	uart->lcr = uart->lcr | UART_LCR_DLAB | UART_LCR_8N1;
	uart->dll = divisor & 0xff;
	uart->dlm = (divisor >> 8) & 0xff;
	uart->lcr = uart->lcr & (~UART_LCR_DLAB);

	uart->ier = 0;
	uart->mcr = UART_MCRVAL;
	uart->fcr = UART_FCR_DEFVAL;

	uart->lcr = 3;
}

void hal_srv_uart_putc(uint8_t ch)
{
	while (!(uart->lsr & UART_LSR_THRE))
		;
	uart->rbr = ch;
}

int hal_srv_uart_getc(void)
{
    int timeout = 1000;  // sorta spinlock
    int i = 0;

    while (!(uart->lsr & UART_LSR_DR))
    {
        if (i++ >= timeout)
        {
            return -1; // Timeout occurred
        }
    }

    return (int)uart->rbr;
}

int hal_srv_uart_getc_fast(void)
{
    int timeout = 0;  // sorta spinlock
    int i = 0;

    while (!(uart->lsr & UART_LSR_DR))
    {
        if (i++ >= timeout)
        {
            return -1; // Timeout occurred
        }
    }

    return (int)uart->rbr;
}

int hal_srv_uart_tstc(void)
{
	return (!!(uart->lsr & UART_LSR_DR));
}

//-------------------------------------
//-------------------------------------
//-------------------------------------
//-------------------------------------

void uart_srv_init(void)
{
	int baudrate = 1000000;
	int uart_clock = 25 * 1000 * 1000;

	/* set uart to pinmux_uart1 */
	//pinmux_config(PINMUX_UART0);

	hal_srv_uart_init(UART0, baudrate, uart_clock);
}

uint8_t uart_srv_putc(uint8_t ch)
{
	// if (ch == '\n') {
	// 	hal_uart_putc('\r');
	// }
	hal_srv_uart_putc(ch);
	return ch;
}

void uart_srv_puts(char *str)
{
	if (!str)
		return;

	while (*str) {
		uart_srv_putc(*str++);
	}
}

int uart_srv_put_array(const uint8_t *array, size_t size)
{
    if (!array || size == 0)
        return 0;

    size_t count = 0;

    for (size_t i = 0; i < size; i++) {
        hal_srv_uart_putc(array[i]);
        count++;
    }

    return count;
}

int uart_srv_getc(void)
{
	return (int)hal_srv_uart_getc();
}

int uart_srv_tstc(void)
{
	return hal_srv_uart_tstc();
}
