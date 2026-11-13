/*
 * bl.ld -- TP4 (Bloque 4): linker script del BOOTLOADER
 *
<<<<<<< HEAD
 * - El BL vive en BL_ROM, desde 0x0000_0000 (PROGADDR_RESET de picorv32).
 * - Lo primero es su vector table (seccion .vectors de bl_start.S).
 * - No tiene .data ni .bss: su init de runtime es solo cargar el SP.
 * - Exporta a C donde tiene que buscar al FW (__fw_slot_start/__fw_slot_size).
=======
 * COMPLETAR los TODO (hito 2). El BL vive en BL_ROM (memory_map.ld).
>>>>>>> 35c8079eb177b400fbb9e33272a9a174b5e427d7
 */
ENTRY(_bl_vectors)

INCLUDE memory_map.ld

SECTIONS
{
  .text : {
<<<<<<< HEAD
    KEEP(*(.vectors))                          /* vector table del BL: en 0x0 */
=======
    /* TODO: la vector table (seccion .vectors de bl_start.S) tiene que quedar
     *       PRIMERA, en 0x0000_0000. Ojo con --gc-sections: nadie la referencia. */
>>>>>>> 35c8079eb177b400fbb9e33272a9a174b5e427d7
    *(.text .text.*)
    *(.rodata .rodata.* .srodata .srodata.*)
    . = ALIGN(4);
  } > BL_ROM

<<<<<<< HEAD
  /* El BL no usa variables globales. Si aparecen, el link falla (ASSERT abajo)
   * en lugar de generar un BL que lee basura de la RAM. */
  .data : {
    __bl_data_start = .;
    *(.data .data.* .sdata .sdata.*)
    __bl_data_end = .;
  } > RAM AT > BL_ROM

  .bss (NOLOAD) : {
    __bl_bss_start = .;
    *(.bss .bss.* .sbss .sbss.* COMMON)
    __bl_bss_end = .;
  } > RAM
=======
  /* TODO: el BL no usa variables globales. Hagan que el link FALLE si alguien
   *       agrega una (.data/.sdata o .bss/.sbss/COMMON con tamano distinto de 0).
   *       Pista: secciones con simbolos de inicio/fin + ASSERT. */
>>>>>>> 35c8079eb177b400fbb9e33272a9a174b5e427d7

  /DISCARD/ : { *(.eh_frame .eh_frame_hdr) }
}

<<<<<<< HEAD
/* Stack del BL: tope de la RAM (despues el FW carga el suyo desde su vector table) */
_bl_stack_top = ORIGIN(RAM) + LENGTH(RAM);

/* Donde espera el BL encontrar al FW (los usa bl_main.c como extern) */
__fw_slot_start = ORIGIN(APP_ROM);
__fw_slot_size  = LENGTH(APP_ROM);

ASSERT(_bl_vectors == ORIGIN(BL_ROM),
       "TP4/BL: la vector table del BL tiene que quedar en PROGADDR_RESET (0x0)")
ASSERT(__bl_data_end == __bl_data_start,
       "TP4/BL: el BL no debe usar variables globales inicializadas (.data)")
ASSERT(__bl_bss_end == __bl_bss_start,
       "TP4/BL: el BL no debe usar variables globales sin inicializar (.bss)")
=======
/* TODO: _bl_stack_top (lo usa bl_start.S). ¿Donde conviene que quede el stack del BL? */

/* (YA ESTA HECHO) Donde espera el BL encontrar al FW: bl_main.c los usa como extern */
__fw_slot_start = ORIGIN(APP_ROM);
__fw_slot_size  = LENGTH(APP_ROM);

/* TODO: ASSERT que verifique que la vector table quedo en ORIGIN(BL_ROM). */
>>>>>>> 35c8079eb177b400fbb9e33272a9a174b5e427d7
