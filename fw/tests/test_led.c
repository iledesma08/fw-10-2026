#include "includes.h"
#include "led.h"

int main(void)
{
    REG_FW_STATUS = 0x0;
    REG_FW_REPORT = 0x0;

    /* 1. Inicializacion */
    led_init();
    if (REG_LED != 0 || led_get_mask() != 0) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* 2. Encendido individual (led_on) y lectura de estado (led_get_status) */
    led_on(0); /* bit 0 -> 0b00000001 (0x01) */
    if (REG_LED != 0x01 || led_get_status(0) != 1 || led_get_status(1) != 0) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    led_on(7); /* bit 7 -> 0b10000001 (0x81) */
    if (REG_LED != 0x81 || led_get_status(7) != 1 || led_get_status(0) != 1) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* 3. Apagado individual (led_off) */
    led_off(0); /* apagar bit 0 -> 0b10000000 (0x80) */
    if (REG_LED != 0x80 || led_get_status(0) != 0 || led_get_status(7) != 1) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* 4. Conmutacion de estado (led_toggle) */
    led_toggle(7); /* bit 7 conmuta a 0 -> 0b00000000 (0x00) */
    if (REG_LED != 0x00 || led_get_status(7) != 0) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    led_toggle(3); /* bit 3 conmuta a 1 -> 0b00001000 (0x08) */
    if (REG_LED != 0x08 || led_get_status(3) != 1) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* 5. Escritura de valor (led_write) */
    led_write(0xAA); /* 0b10101010 */
    if (REG_LED != 0xAA || led_get_mask() != 0xAA) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* 6. Configuracion y lectura de mascara (led_set_mask / led_get_mask) */
    led_set_mask(0x55); /* 0b01010101 */
    if (REG_LED != 0x55 || led_get_mask() != 0x55) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* 7. Validacion de indices invalidos (idx >= 8): no deben alterar el registro */
    led_set_mask(0x12); /* estado conocido: 0x12 */

    led_on(8);
    led_off(9);
    led_toggle(15);
    led_on(255);

    if (REG_LED != 0x12 || led_get_mask() != 0x12) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    if (led_get_status(8) != 0 || led_get_status(255) != 0) {
        REG_FW_STATUS = 0xBAD;
        return -1;
    }

    /* Limpiar LEDs al finalizar */
    led_init();

    /* Indicar exito en el simulador */
    REG_FW_REPORT = 0xCAFE;
    REG_FW_STATUS = 0xFF; /* PASS */

    return 0;
}
