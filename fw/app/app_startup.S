/*
 * app_startup.S — TP4 (Bloque 4): arranque del FW cuando corre detrás del BL.
 * Reemplaza a platform/fw/startup.S, que solo hacía "la sp, _stack_top; call main".
 *
 *  1) Header + vector table del FW (sección .fw_header, primera en APP_ROM):
 *     el BL la lee para validar la imagen y saber a dónde saltar.
 *  2) Reset_Handler: el "De 0 a main" completo (diapos 5 y 6 del Bloque 4).
 */
#include "fw_image.h"

#define REG_FW_STATUS_ADDR  0x00040000      /* project_config.def: REG_FW_STATUS */
#define FW_STATUS_PASS      0xFF

/* ------------------------------------------------------------------------ */
/*  1) Header + vector table (app.ld la pone en 0x0000_1000)                 */
/* ------------------------------------------------------------------------ */
    .section .fw_header, "a"
    .balign 4
    .globl  __fw_header
    .type   __fw_header, @object
__fw_header:
    .word   FW_IMAGE_MAGIC          /* 0x00 magic "TP4F"                               */
    .word   __fw_start              /* 0x04 load_addr  (linker: dónde quedó linkeado)  */
    .word   __fw_size               /* 0x08 image_size (linker: app.ld)                */
    .word   FW_IMAGE_CRC_UNSET      /* 0x0C image_crc  (lo escribe tools/mkimage.py)   */
    /* --- vector table del FW: direcciones, como en Cortex-M --- */
    .word   _stack_top              /* 0x10 initial_sp                                 */
    .word   Reset_Handler           /* 0x14 reset_handler                              */
    .size   __fw_header, . - __fw_header

/* ------------------------------------------------------------------------ */
/*  2) Reset_Handler: "De 0 a main"                                          */
/* ------------------------------------------------------------------------ */
    .section .text.Reset_Handler, "ax"
    .balign 4                       /* contrato: reset_handler alineado a 4, aun con -march=..c */
    .globl  Reset_Handler
    .type   Reset_Handler, @function
Reset_Handler:
    /* 1) Stack. El BL ya cargó sp desde la vector table, pero lo recargamos:
     *    así el FW también arranca bien si alguien salta directo a él. */
    la      sp, _stack_top

    /* 2) Copiar .data: de su LMA (dentro de la imagen del FW) a su VMA (RAM) */
    la      t0, _sidata
    la      t1, _sdata
    la      t2, _edata
1:  bgeu    t1, t2, 2f
    lw      t3, 0(t0)
    sw      t3, 0(t1)
    addi    t0, t0, 4
    addi    t1, t1, 4
    j       1b

    /* 3) Poner .bss en cero */
2:  la      t0, _sbss
    la      t1, _ebss
3:  bgeu    t0, t1, 4f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       3b

    /* 4) No hay C++ (no hay constructores en .init_array): directo a main() */
4:  call    main

    /* 5) main() volvió. Si no dejó PASS (0xFF) en REG_FW_STATUS, ebreak:
     *    con ENABLE_IRQ=0 PicoRV32 hace trap y el testbench corta la simulación
     *    (con el startup.S original, un FAIL = 0xBAD la dejaba corriendo). */
    li      t0, REG_FW_STATUS_ADDR
    lw      t1, 0(t0)
    li      t2, FW_STATUS_PASS
    beq     t1, t2, 5f
    ebreak
5:  j       5b
    .size   Reset_Handler, . - Reset_Handler
