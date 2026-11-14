#include "includes.h"
#include "uart.h"

int main(void)
{
    REG_FW_STATUS = 0x0;
    REG_FW_REPORT = 0x0;

    uart_init();

    /* 1. Prueba de enviar un solo byte */
    uart_write_byte('K');
    if (REG_UART0_SIZE_TO_TRANSMIT != 1 || 
        REG_ARRAY_UART0_BUFF_OUT[0] != 'K' || 
        REG_UART0_TRANSMISION_DONE != 1) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* 2. Prueba de enviar una cadena */
    const char *test_msg = "UART0_OK";
    uart_write_str(test_msg);

    if (REG_UART0_SIZE_TO_TRANSMIT != 8) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    for (int i = 0; i < 8; i++) {
        if (REG_ARRAY_UART0_BUFF_OUT[i] != (unsigned int)(unsigned char)test_msg[i]) {
            REG_FW_STATUS = 0xBAD;
            return -1;
        }
    }
    if (REG_UART0_TRANSMISION_DONE != 1) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* 3. Prueba de string vacio */
    uart_write_str("");
    if (REG_UART0_TRANSMISION_DONE != 1) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* 4. Enviar mensaje de confirmacion por UART0 */
    uart_write_str("\n[TEST UART0]: Todos los checks pasaron con exito!\n");

    /* Indicar exito en el simulador */
    REG_FW_REPORT = 0xCAFE;
    REG_FW_STATUS = 0xFF; /* PASS */

    return 0;
}
