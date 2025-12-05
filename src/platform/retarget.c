#include <stdio.h>
#include <stdint.h>

// UART0 Base Address for MPS3 Corstone-300
// The address 0x49303000 is for the UART in the expansion region (Shield 0)
// However, the default UART0 for console output on MPS3 is often at 0x41103000 (APB UART)
// Let's try the APB UART address if the previous one didn't work.
// Actually, for Corstone-300 (SSE-300), the UART0 is at 0x49303000.
// But let's double check the FVP configuration.
// The FVP parameter mps3_board.uart0.out_file="-" suggests it's the board UART.

// Let's try to define both and write to both just in case.
#define UART0_BASE 0x49303000
#define UART1_BASE 0x49304000
#define UART2_BASE 0x49305000
#define UART_APB_BASE 0x41103000

typedef struct {
    volatile uint32_t DATA;   // 0x00
    volatile uint32_t STATE;  // 0x04
    volatile uint32_t CTRL;   // 0x08
    volatile uint32_t INTSTATUS; // 0x0C
    volatile uint32_t BAUDDIV;   // 0x10
} UART_TypeDef;

#define UART0 ((UART_TypeDef *)UART0_BASE)
#define UART1 ((UART_TypeDef *)UART1_BASE)
#define UART2 ((UART_TypeDef *)UART2_BASE)
#define UART_APB ((UART_TypeDef *)UART_APB_BASE)

#define UART_STATE_TXFULL (1UL << 0)

void uart_putc(UART_TypeDef* uart, char c) {
    if (c == '\n') {
        uart_putc(uart, '\r');
    }
    
    while (uart->STATE & UART_STATE_TXFULL);
    uart->DATA = c;
}

int _write(int fd, char *ptr, int len) {
    if (fd != 1 && fd != 2) {
        return -1;
    }
    
    for (int i = 0; i < len; i++) {
        // Try writing to UART0, UART1, and UART2 to see which one works
        uart_putc(UART0, ptr[i]);
        uart_putc(UART1, ptr[i]);
        uart_putc(UART2, ptr[i]);
        uart_putc(UART_APB, ptr[i]);
    }
    
    return len;
}

int _read(int fd, char *ptr, int len) {
    return 0;
}

int _close(int fd) { return -1; }
int _fstat(int fd, void *st) { return -1; }
int _isatty(int fd) { return 1; }
int _lseek(int fd, int ptr, int dir) { return 0; }
