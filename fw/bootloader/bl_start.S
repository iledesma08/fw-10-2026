/*
 * bl_start.S — TP4 (Bloque 4): vector table + arranque mínimo del BOOTLOADER
 *
 * Vectores, stack mínimo y transferencia sin retorno (hitos 2 y 5).
 *
 * Datos de la plataforma (platform/rtl/top.v, parámetros de picorv32):
 *   - PROGADDR_RESET = 0x0000_0000: al salir de reset el core EJECUTA lo que hay ahí.
 *   - PROGADDR_IRQ   = 0x0000_0010 (ENABLE_IRQ = 0: no hay IRQs).
 *   - STACKADDR      = 0xFFFF_FFFF: el core NO inicializa sp.
 */

#define REG_FW_STATUS_ADDR  0x00040000      /* project_config.def: REG_FW_STATUS */
#define FW_STATUS_FAIL      0xBAD           /* convención de los tests de platform/fw */

/* ------------------------------------------------------------------------ */
/*  Vector table del BL (bl.ld la tiene que ubicar en 0x0000_0000)          */
/* ------------------------------------------------------------------------ */
    .section .vectors, "ax"
    .option push
    .option norvc                   /* instrucciones de 4 bytes: los offsets importan */
    .globl  _bl_vectors
    .type   _bl_vectors, @function
_bl_vectors:
    j bl_reset_handler  # offset 0x00: arranque normal
    j bl_unexpected     # offset 0x04: hueco defensivo
    j bl_unexpected     # offset 0x08: hueco defensivo
    j bl_unexpected     # offset 0x0C: hueco defensivo
    j bl_unexpected     # offset 0x10: entrada IRQ (PROGADDR_IRQ segun top.v)
    .size   _bl_vectors, . - _bl_vectors
    .option pop

/* ------------------------------------------------------------------------ */
/*  Reset: init mínimo del BL                                               */
/* ------------------------------------------------------------------------ */
    .section .text.bl_reset_handler, "ax"
    .globl  bl_reset_handler
    .type   bl_reset_handler, @function
bl_reset_handler:
    la sp, _bl_stack_top  # dirección del extremo superior de RAM
    call bl_main         # invoca C y guarda retorno en ra
    j       bl_unexpected           /* si bl_main volviera (o mientras no esté hecho): FAIL */
    .size   bl_reset_handler, . - bl_reset_handler

/* ------------------------------------------------------------------------ */
/*  void bl_jump(uint32_t sp, uint32_t entry)  __attribute__((noreturn))    */
/* ------------------------------------------------------------------------ */
    .section .text.bl_jump, "ax"
    .globl  bl_jump
    .type   bl_jump, @function
bl_jump:
    mv sp, a0          # adoptar el stack solicitado por el FW
    li ra, 0           # quitar una continuación residual hacia el BL
    jr a1              # saltar al Reset_Handler, sin guardar retorno
    .size   bl_jump, . - bl_jump

/* ------------------------------------------------------------------------ */
/*  bl_unexpected: vector inesperado -> FAIL + trap   (YA ESTÁ HECHO)       */
/*  void bl_halt(void): corta la simulación           (YA ESTÁ HECHO)       */
/* ------------------------------------------------------------------------ */
    .section .text.bl_halt, "ax"
    .globl  bl_unexpected
    .type   bl_unexpected, @function
bl_unexpected:
    li      t0, REG_FW_STATUS_ADDR
    li      t1, FW_STATUS_FAIL
    sw      t1, 0(t0)
    /* sigue en bl_halt */
    .globl  bl_halt
    .type   bl_halt, @function
bl_halt:
    ebreak                          /* Pregunta: ¿por qué hace falta el ebreak si ya */
                                    /* dejamos 0xBAD en REG_FW_STATUS?               */
1:  j       1b
    .size   bl_halt, . - bl_halt
    .size   bl_unexpected, . - bl_unexpected
