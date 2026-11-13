/*
 * bl_main.c — TP4 (Bloque 4): lógica del BOOTLOADER
 *
 * El BL "hace lo mínimo": avisa que arrancó, verifica que en el slot del FW
 * haya una imagen sana y le pasa el control. No usa variables globales (todo
 * vive en el stack o es const en ROM): por eso su init de runtime es solo
 * cargar el SP (ver bl_start.S y el ASSERT de bl.ld).
 *
 * Para avisar usa lo mismo que el TP2: print_str() de platform/fw/print.c y
 * REG_FW_REPORT. BL y FW linkean cada uno su propia copia de print.c: son dos
 * binarios independientes que no comparten código.
 */
#include <stdint.h>
#include "includes.h"   /* config.h (registros) + print_str() de print.c */
#include "fw_image.h"   /* header + vector table del FW                 */
#include "strutils.h"   /* str_crc8() del TP3 (bloque3/trabajo_practico)  */

/* ---- Símbolos que define bootloader/bl.ld a partir de memory_map.ld ----
 * Para el linker son direcciones; en C se usan con '&' (o como array),
 * igual que _sdata/_ebss en un startup. */
extern const fw_image_header_t __fw_slot_start;  /* ORIGIN(APP_ROM): acá espera al FW      */
extern const uint8_t           __fw_slot_size[]; /* LENGTH(APP_ROM): el valor ES la dirección */

/* ---- bl_start.S ---- */
void bl_jump(uint32_t sp, uint32_t entry) __attribute__((noreturn));
void bl_halt(void) __attribute__((noreturn));
void bl_main(void) __attribute__((noreturn));

/* ---- Marcas de avance en REG_FW_REPORT (como test_fw_status.c del TP2):
 *      el testbench no las imprime, pero se ven en las ondas (make wave) ---- */
#define BL_REPORT_START   0xB0070000u   /* el BL arrancó              */
#define BL_REPORT_IMG_OK  0xB0070100u   /* imagen del FW validada     */
#define BL_REPORT_JUMP    0xB0070200u   /* saltando al FW             */
#define BL_REPORT_ERROR   0xB007E000u   /* error; bits bajos = código */

/* ---- REG_FW_STATUS: 0xFF termina la simulación (TP2); 0xBAD = falla, como en los tests de platform/fw ---- */
#define FW_STATUS_RUNNING  0x000u
#define FW_STATUS_FAIL     0xBADu

enum bl_error {
    BL_ERR_MAGIC = 1,   /* no hay FW en el slot (o no tiene el header del TP4)  */
    BL_ERR_LOAD_ADDR,   /* el FW fue linkeado para otra dirección               */
    BL_ERR_SIZE,        /* image_size fuera de rango                            */
    BL_ERR_CRC,         /* imagen corrupta (o no pasó por tools/mkimage.py)     */
    BL_ERR_ENTRY,       /* reset_handler fuera de la imagen o desalineado       */
    BL_ERR_SP,          /* initial_sp fuera de la RAM o no alineado a 16        */
};

/* Arma "<msg>0xXXXXXXXX" en el stack y lo manda en UNA transmisión de UART0
 * (testbench.sv imprime cada transmisión como una línea "UART: ..."). */
static void bl_log_hex(const char *msg, uint32_t val)
{
    char line[64];
    unsigned int n = 0;

    while (*msg != '\0' && n < sizeof(line) - 11u)
        line[n++] = *msg++;
    line[n++] = '0';
    line[n++] = 'x';
    for (int shift = 28; shift >= 0; shift -= 4)
        line[n++] = "0123456789ABCDEF"[(val >> shift) & 0xFu];
    line[n] = '\0';
    print_str(line);
}

/* Error: queda visible en FW_REPORT y UART, y se corta la simulación.
 * Ojo: el testbench solo termina con FW_STATUS = 0xFF o con trap; un 0xBAD
 * solo la dejaría corriendo para siempre, por eso bl_halt() hace ebreak. */
static void __attribute__((noreturn)) bl_fail(enum bl_error code, const char *why)
{
    print_str(why);
    REG_FW_REPORT = BL_REPORT_ERROR | (uint32_t)code;
    REG_FW_STATUS = FW_STATUS_FAIL;
    print_str("[BL] no se salta al FW: ebreak -> trap -> fin de la simulacion");
    bl_halt();
}

void bl_main(void)
{
    const fw_image_header_t *hdr = &__fw_slot_start;
    const uint32_t slot_base = (uint32_t)&__fw_slot_start;
    const uint32_t slot_size = (uint32_t)__fw_slot_size;

    REG_FW_STATUS = FW_STATUS_RUNNING;
    REG_FW_REPORT = BL_REPORT_START;
    print_str("[BL] TP4 bootloader: arranque desde reset (0x00000000)");
    bl_log_hex("[BL] buscando FW en ", slot_base);

    /* 1) ¿Hay una imagen en el slot? */
    bl_log_hex("[BL]   magic      = ", hdr->magic);
    if (hdr->magic != FW_IMAGE_MAGIC)
        bl_fail(BL_ERR_MAGIC, "[BL] ERROR 1: no hay un FW valido en el slot (magic)");

    /* 2) ¿Fue linkeado para correr acá? ("FW en la dirección donde el BL lo espera") */
    if (hdr->load_addr != slot_base)
        bl_fail(BL_ERR_LOAD_ADDR, "[BL] ERROR 2: el FW fue linkeado para otra direccion");

    /* 3) ¿El tamaño tiene sentido? (además acota la lectura del CRC al slot) */
    bl_log_hex("[BL]   image_size = ", hdr->image_size);
    if (hdr->image_size < FW_HDR_SIZE || hdr->image_size > slot_size)
        bl_fail(BL_ERR_SIZE, "[BL] ERROR 3: image_size fuera de rango");

    /* 4) ¿Está sana? CRC-8 del TP3 desde la vector table hasta el final de la
     *    imagen: la misma validación que la trama del hito 10 del bloque 3. */
    const uint32_t crc = str_crc8((const uint8_t *)slot_base + FW_IMAGE_CRC_START,
                                  hdr->image_size - FW_IMAGE_CRC_START);
    bl_log_hex("[BL]   crc header = ", hdr->image_crc);
    bl_log_hex("[BL]   crc calc.  = ", crc);
    if (crc != hdr->image_crc)
        bl_fail(BL_ERR_CRC, "[BL] ERROR 4: el CRC no coincide (imagen corrupta o sin mkimage)");

    /* 5) Vector table del FW: SP inicial y Reset_Handler razonables */
    const uint32_t sp    = hdr->initial_sp;
    const uint32_t entry = hdr->reset_handler;
    bl_log_hex("[BL]   initial_sp = ", sp);
    bl_log_hex("[BL]   reset      = ", entry);
    if (entry < slot_base + FW_HDR_SIZE || entry >= slot_base + hdr->image_size || (entry & 3u))
        bl_fail(BL_ERR_ENTRY, "[BL] ERROR 5: reset_handler fuera de la imagen o desalineado");
    if (sp <= ADDR_START_DATA || sp > ADDR_START_DATA + ADDR_SIZE_DATA || (sp & 15u))
        bl_fail(BL_ERR_SP, "[BL] ERROR 6: initial_sp fuera de la RAM o no alineado a 16 (ABI)");

    REG_FW_REPORT = BL_REPORT_IMG_OK;
    print_str("[BL] FW OK -> salto al Reset_Handler del FW");
    REG_FW_REPORT = BL_REPORT_JUMP;
    bl_jump(sp, entry);             /* ABI ILP32: a0 = sp, a1 = entry. No vuelve. */
}