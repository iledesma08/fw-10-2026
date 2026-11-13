/*
 * bl_start.S — TP4 (Bloque 4): vector table + arranque mínimo del BOOTLOADER
 *
 * En Cortex-M la vector table es una tabla de DIRECCIONES (SP inicial,
 * Reset_Handler, NMI, ...) que el core lee solo al salir de reset.
 * En PicoRV32 (RISC-V) no: al salir de reset el core EJECUTA la instrucción
 * que está en PROGADDR_RESET (0x0000_0000, default en platform/rtl/top.v) y, si tuviera
 * IRQs habilitadas, saltaría a PROGADDR_IRQ (0x0000_0010). Por eso la vector
 * table del BL es una tabla de SALTOS ubicada en esos offsets.
 * (La del FW, en cambio, sí es de direcciones: ver fw_image.h.)
 */

#define REG_FW_STATUS_ADDR  0x00040000      /* project_config.def: REG_FW_STATUS */
#define FW_STATUS_FAIL      0xBAD           /* convención de los tests de platform/fw */

/* ------------------------------------------------------------------------ */
/*  Vector table del BL (bl.ld la ubica en 0x0000_0000)                     */
/* ------------------------------------------------------------------------ */
    .section .vectors, "ax"
    .option push
    .option norvc                   /* saltos de 4 bytes aunque se compile con -march=..c: */
                                    /* si no, el vector de IRQ no caería en 0x10           */
    .globl  _bl_vectors
    .type   _bl_vectors, @function
_bl_vectors:
    j       bl_reset_handler        /* 0x00  PROGADDR_RESET                          */
    j       bl_unexpected           /* 0x04  no es vector en PicoRV32                */
    j       bl_unexpected           /* 0x08  no es vector en PicoRV32                */
    j       bl_unexpected           /* 0x0C  no es vector en PicoRV32                */
    j       bl_unexpected           /* 0x10  PROGADDR_IRQ (con ENABLE_IRQ=0 no se usa) */
    .size   _bl_vectors, . - _bl_vectors
    .option pop

/* ------------------------------------------------------------------------ */
/*  Reset: init mínimo del BL                                               */
/* ------------------------------------------------------------------------ */
    .section .text.bl_reset_handler, "ax"
    .globl  bl_reset_handler
    .type   bl_reset_handler, @function
bl_reset_handler:
    /* 1) Stack del BL. PicoRV32 no inicializa sp por HW (STACKADDR queda en
     *    0xFFFF_FFFF en top.v): sin esto, la primera llamada a C rompe todo. */
    la      sp, _bl_stack_top

    /* 2) .data/.bss del BL: no hay (bl.ld lo garantiza con ASSERT), así que
     *    no hay nada que copiar ni poner en cero. Ese es el "init mínimo". */

    /* 3) Validar el FW y saltar. bl_main() no vuelve. */
    call    bl_main
    j       bl_unexpected           /* si volviera, es un bug: FAIL */
    .size   bl_reset_handler, . - bl_reset_handler

/* ------------------------------------------------------------------------ */
/*  void bl_jump(uint32_t sp, uint32_t entry)  __attribute__((noreturn))    */
/*                                                                          */
/*  Se llama desde C: por la calling convention de RISC-V (ABI ILP32) el    */
/*  primer argumento llega en a0 y el segundo en a1. Es un SALTO, no una    */
/*  llamada: jr (sin link) y ra = 0, así el FW no tiene "a dónde volver".   */
/* ------------------------------------------------------------------------ */
    .section .text.bl_jump, "ax"
    .globl  bl_jump
    .type   bl_jump, @function
bl_jump:
    mv      sp, a0                  /* SP inicial que pide el FW (su vector table) */
    li      ra, 0
    jr      a1                      /* -> Reset_Handler del FW                     */
    .size   bl_jump, . - bl_jump

/* ------------------------------------------------------------------------ */
/*  bl_unexpected: vector inesperado o bl_main() volvió -> FAIL + trap      */
/*  void bl_halt(void): corta la simulación                                 */
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
    ebreak                          /* ENABLE_IRQ=0: ebreak => trap del core =>   */
                                    /* testbench.sv termina la simulación         */
1:  j       1b
    .size   bl_halt, . - bl_halt
    .size   bl_unexpected, . - bl_unexpected
