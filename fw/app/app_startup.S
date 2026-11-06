/*
 * app_startup.S — TP4 (Bloque 4): arranque del FW cuando corre detrás del BL.
 * Reemplaza a ../startup.S (que no se toca).
 *
 * Header, copia de datos y limpieza antes de main (hito 4).
 */
#include "fw_image.h"

#define REG_FW_STATUS_ADDR  0x00040000      /* project_config.def: REG_FW_STATUS */
#define FW_STATUS_PASS      0xFF

/* ------------------------------------------------------------------------ */
/*  1) Header + vector table del FW (app.ld lo pone al inicio de APP_ROM)    */
/* ------------------------------------------------------------------------ */
    .section .fw_header, "a"
    .balign 4
    .globl  __fw_header
    .type   __fw_header, @object
__fw_header:
    .word FW_IMAGE_MAGIC       # marca de formato
    .word __fw_start           # base definida por linker
    .word __fw_size            # bytes de imagen cargable
    .word FW_IMAGE_CRC_UNSET   # pendiente: 0xFFFFFFFF
    .word _stack_top           # pila solicitada por FW
    .word Reset_Handler        # dirección de su startup
    .size   __fw_header, . - __fw_header

/* ------------------------------------------------------------------------ */
/*  2) Reset_Handler: "De 0 a main" (diapos 5 y 6)                           */
/* ------------------------------------------------------------------------ */
    .section .text.Reset_Handler, "ax"
    .balign 4                       /* contrato: reset_handler alineado a 4 */
    .globl  Reset_Handler
    .type   Reset_Handler, @function
Reset_Handler:
    la sp, _stack_top          # pila válida antes de entrar en C
    la t0, _sidata             # origen ROM de los inicializadores
    la t1, _sdata              # destino RAM
    la t2, _edata              # final excluido
copy_data:
    bgeu t1, t2, clear_setup   # también acepta sección vacía
    lw t3, 0(t0)              # lee cuatro bytes desde origen
    sw t3, 0(t1)              # escribe cuatro bytes en destino
    addi t0, t0, 4            # próxima palabra de ROM
    addi t1, t1, 4            # próxima palabra de RAM
    j copy_data
clear_setup:
    la t1, _sbss              # comienzo de datos a limpiar
    la t2, _ebss              # final excluido
clear_bss:
    bgeu t1, t2, run_main
    sw zero, 0(t1)            # registro zero siempre contiene cero
    addi t1, t1, 4
    j clear_bss
run_main:
    call main                 # guarda retorno en ra
    li t0, REG_FW_STATUS_ADDR
    lw t1, 0(t0)              # lee resultado dejado por main
    li t2, FW_STATUS_PASS
    beq t1, t2, finished
    ebreak                    # retorno sin éxito: trap
finished:
    j finished                # no cae dentro de otro código
    .size   Reset_Handler, . - Reset_Handler
