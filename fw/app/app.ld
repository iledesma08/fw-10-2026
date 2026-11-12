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
ASSERT(ADDR(.fw_header) == ORIGIN(APP_ROM), "Error: .fw_header no se encuentra al inicio de APP_ROM")
ASSERT(SIZEOF(.fw_header) == 24, "Error: el tamaño de .fw_header no es de 24 bytes (se esperan 6 campos de 32 bits)")
ASSERT(__fw_end <= ORIGIN(APP_ROM) + LENGTH(APP_ROM), "Error: la imagen del FW excede el espacio disponible en APP_ROM")
ASSERT(_ebss <= _stack_top - 4096, "Error: espacio insuficiente en RAM (se requieren al menos 4096 bytes libres para el stack)")
