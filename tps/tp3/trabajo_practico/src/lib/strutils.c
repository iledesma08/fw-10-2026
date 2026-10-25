/**
 * @file    strutils.c
 * @brief   Buffer de strings de tamaño fijo del TP3.
 *
 * Sin memoria dinámica; todas las escrituras se acotan a STR_MAX_LEN.
 */

#include "strutils.h"

void strbuf_init(strbuf_t *buf)
{
    buf->data[0] = '\0';
    buf->len = 0;
}

void strbuf_set(strbuf_t *buf, const char *src)
{
    size_t i = 0;

    while (src[i] != '\0' && i < STR_MAX_LEN - 1) {
        buf->data[i] = src[i];
        i++;
    }

    buf->data[i] = '\0';
    buf->len = i;
}

size_t strbuf_append(strbuf_t *buf, const char *src)
{
    size_t i = 0;

    while (src[i] != '\0' && buf->len < STR_MAX_LEN - 1) {
        buf->data[buf->len] = src[i];
        buf->len++;
        i++;
    }

    buf->data[buf->len] = '\0';
    return i;
}

size_t strbuf_append_int(strbuf_t *buf, int32_t value)
{
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

const char *strbuf_cstr(const strbuf_t *buf)
{
    return buf->data;
}
