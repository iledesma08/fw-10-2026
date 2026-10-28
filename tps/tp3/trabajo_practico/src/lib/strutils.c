/**
 * @file    strutils.c
 * @brief   Buffer de strings de tamaño fijo del TP3.
 *
 * Sin memoria dinámica; todas las escrituras se acotan a STR_MAX_LEN.
 */

#include "strutils.h"
#include <ctype.h>

void strbuf_init(strbuf_t *buf) {
  buf->data[0] = '\0';
  buf->len = 0;
}

void strbuf_set(strbuf_t *buf, const char *src) {
  size_t i = 0;

  while (src[i] != '\0' && i < STR_MAX_LEN - 1) {
    buf->data[i] = src[i];
    i++;
  }

  buf->data[i] = '\0';
  buf->len = i;
}

size_t strbuf_append(strbuf_t *buf, const char *src) {
  size_t i = 0;

  while (src[i] != '\0' && buf->len < STR_MAX_LEN - 1) {
    buf->data[buf->len] = src[i];
    buf->len++;
    i++;
  }

  buf->data[buf->len] = '\0';
  return i;
}

size_t strbuf_append_int(strbuf_t *buf, int32_t value) {
  char digits[12];
  char text[12];
  size_t n = 0;
  size_t out = 0;
  uint32_t magnitude;
  int negative = 0;

  if (value < 0) {
    negative = 1;
    magnitude = (uint32_t)(-(int64_t)value);
  } else {
    magnitude = (uint32_t)value;
  }

  /* Dígitos en orden inverso sobre un buffer temporal. */
  do {
    digits[n++] = (char)('0' + magnitude % 10);
    magnitude /= 10;
  } while (magnitude != 0);

  /* Reordenar en el buffer definitivo, ya terminado en '\0'. */
  if (negative == 1)
    text[out++] = '-';

  while (n > 0)
    text[out++] = digits[--n];

  text[out] = '\0';

  return strbuf_append(buf, text);
}

const char *strbuf_cstr(const strbuf_t *buf) { return buf->data; }

size_t str_trim(char *dst, const char *src, size_t max_len) {
  size_t start = 0;
  size_t end;
  size_t len;

  if (max_len == 0)
    return 0;

  while (src[start] != '\0' && isspace((unsigned char)src[start]))
    start++;

  end = start;
  while (src[end] != '\0')
    end++;

  while (end > start && isspace((unsigned char)src[end - 1]))
    end--;

  len = end - start;
  if (len > max_len - 1)
    len = max_len - 1;

  for (size_t i = 0; i < len; i++)
    dst[i] = src[start + i];
  dst[len] = '\0';

  return len;
}

uint8_t str_crc8(const uint8_t *data, size_t len) {
  uint8_t crc = STR_CRC8_INIT;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if ((crc & 0x80U) != 0U)
        crc = (uint8_t)((crc << 1) ^ 0x07U);
      else
        crc = (uint8_t)(crc << 1);
    }
  }

  return crc;
}
