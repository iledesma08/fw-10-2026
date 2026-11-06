/* Layout del firmware y límites usados por el startup. */
ENTRY(Reset_Handler)
INCLUDE memory_map.ld
SECTIONS
{
  .fw_header ORIGIN(APP_ROM) : {
    __fw_start = .;             /* dirección inicial de imagen */
    KEEP(*(.fw_header))         /* datos leídos por el BL */
  } > APP_ROM
  .text : {
    KEEP(*(.text.Reset_Handler)) /* primer código tras header */
    *(.text .text.*)
    . = ALIGN(4);
  } > APP_ROM
  .rodata : {
    *(.rodata .rodata.* .srodata .srodata.*)
    . = ALIGN(4);
  } > APP_ROM
  .data : {
    . = ALIGN(4);
    _sdata = .;
    *(.data .data.* .sdata .sdata.*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT> APP_ROM
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) : {
    . = ALIGN(4);
    _sbss = .;
    *(.bss .bss.* .sbss .sbss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM
  /DISCARD/ : { *(.eh_frame .eh_frame_hdr) }
}
__fw_end = LOADADDR(.data) + SIZEOF(.data);
__fw_size = __fw_end - __fw_start;
_stack_top = ORIGIN(RAM) + LENGTH(RAM);
ASSERT(ADDR(.fw_header) == ORIGIN(APP_ROM), "Header desplazado")
ASSERT(SIZEOF(.fw_header) == 24, "Header incorrecto")
ASSERT(__fw_end <= ORIGIN(APP_ROM) + LENGTH(APP_ROM), "Imagen grande")
ASSERT(_ebss <= _stack_top - 4096, "Sin reserva de stack")
