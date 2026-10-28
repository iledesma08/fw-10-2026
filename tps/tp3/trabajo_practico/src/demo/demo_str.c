/**
 * @file    demo_str.c
 * @brief   Demo del subsistema strutils (hitos 4 y 5)
 *
 * Sin printf: se usa el string builder y print.h
 */

#include "demo.h"
#include "print.h"
#include "strutils.h"

void demo_str(void) {
  strbuf_t sb;
  char trimmed[STR_MAX_LEN];
  static const uint8_t payload[] = {0x01U, 0x02U, 0x03U, 0x04U};
  uint8_t crc;

  /* Hito 4: value=42 */
  strbuf_init(&sb);
  strbuf_set(&sb, "value=");
  strbuf_append_int(&sb, 42);
  print_str(strbuf_cstr(&sb));
  print_str("\n");

  /* Hito 5: trim */
  str_trim(trimmed, "  hello  ", sizeof(trimmed));
  print_str("trimmed: '");
  print_str(trimmed);
  print_str("'\n");

  /* Hito 5: CRC-8 con init 0xFF */
  crc = str_crc8(payload, sizeof(payload));
  print_str("crc8: 0x");
  print_hex((unsigned int)crc, 2);
  print_str("\n");
}
