/*
 * app.ld -- TP4 (Bloque 4): linker script del FIRMWARE (el del TP anterior)
 * cuando corre detras del bootloader.
 *
 * Diferencias con platform/fw/linker.ld (el original, FW solo en 0x0):
 *   - el FW va en APP_ROM (0x0000_1000), el slot donde lo busca el BL;
 *   - lo primero de la imagen es el header + vector table que lee el BL;
 *   - define _sidata/_sdata/_edata/_sbss/_ebss para que el arranque copie
 *     .data y ponga .bss en cero ("De 0 a main");
 *   - incluye .sdata/.sbss/.srodata (las secciones "small data" que GCC usa
 *     en RISC-V para globales chicas), que *(.data*) y *(.bss*) no capturan;
 *   - calcula el tamano de la imagen (__fw_size) para el header.
 */
ENTRY(Reset_Handler)

INCLUDE memory_map.ld

/* Stack minimo que tiene que quedar libre entre el fin de .bss y el tope de RAM */
__min_stack_size = 0x800;

SECTIONS
{
  .text : {
    __fw_start = .;
    KEEP(*(.fw_header))                        /* header + vector table: SIEMPRE primero */
    *(.text .text.*)
    *(.rodata .rodata.* .srodata .srodata.*)
    . = ALIGN(4);
  } > APP_ROM

  .data : {
    . = ALIGN(4);
    _sdata = .;                                /* VMA: donde viven las variables (RAM) */
    *(.data .data.* .sdata .sdata.*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > APP_ROM
  _sidata = LOADADDR(.data);                   /* LMA: donde esta su valor inicial (imagen) */

  .bss (NOLOAD) : {
    . = ALIGN(4);
    _sbss = .;
    *(.bss .bss.* .sbss .sbss.* COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /DISCARD/ : { *(.eh_frame .eh_frame_hdr) }
}

/* Fin de la imagen cargable = fin de la copia de .data dentro de APP_ROM */
__fw_end   = _sidata + (_edata - _sdata);
__fw_size  = __fw_end - __fw_start;
_stack_top = ORIGIN(RAM) + LENGTH(RAM);

ASSERT(__fw_start == ORIGIN(APP_ROM),
       "TP4/FW: el header del FW tiene que quedar al principio de APP_ROM")
ASSERT(_ebss + __min_stack_size <= _stack_top,
       "TP4/FW: .data + .bss no dejan lugar para el stack")