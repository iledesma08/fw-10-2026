/*
 * bl.ld -- TP4 (Bloque 4): linker script del BOOTLOADER
 *
 * El BL vive en BL_ROM y exige datos mutables vacíos.
 */
ENTRY(_bl_vectors)

INCLUDE memory_map.ld

SECTIONS
{
  .text : {

    KEEP(*(.vectors))
    *(.text .text.*)
    *(.rodata .rodata.* .srodata .srodata.*)
    . = ALIGN(4);
  } > BL_ROM


  .data : {
    KEEP(*(.data .data.* .sdata .sdata.*))
  } > RAM
  .bss (NOLOAD) : {
    KEEP(*(.bss .bss.* .sbss .sbss.*))
    KEEP(*(COMMON))
  } > RAM
  ASSERT(SIZEOF(.data) == 0, "Error: el BL no admite variables globales inicializadas (.data no vacio)")
  ASSERT(SIZEOF(.bss) == 0, "Error: el BL no admite variables globales sin inicializar (.bss no vacio)")
  /DISCARD/ : { *(.eh_frame .eh_frame_hdr) }
}


/* (YA ESTA HECHO) Donde espera el BL encontrar al FW: bl_main.c los usa como extern */
__fw_slot_start = ORIGIN(APP_ROM);
__fw_slot_size  = LENGTH(APP_ROM);


_bl_stack_top = ORIGIN(RAM) + LENGTH(RAM);
ASSERT(_bl_vectors == ORIGIN(BL_ROM), "Error: Tabla BL desplazada")
