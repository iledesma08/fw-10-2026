/**
 * @file    strutils.h
 * @brief   Buffer de strings de tamano fijo y utilidades
 *
 * Todas las funciones trabajan sobre buffers del llamador, sin memoria
 * dinamica y con un limite explicito de tamano
 */

#ifndef STRUTILS_H
#define STRUTILS_H

#include <stddef.h>
#include <stdint.h>

/** @brief Longitud maxima de un buffer de string, incluyendo el NUL */
#define STR_MAX_LEN 128

/** @brief Valor inicial del CRC-8 */
#define STR_CRC8_INIT 0xFF

/**
 * @brief Buffer de string de capacidad fija
 *
 * Guarda su propia longitud para que las operaciones de append no tengan
 * que recorrer el string completo
 */
typedef struct {
  char data[STR_MAX_LEN]; /**< Almacenamiento terminado en NUL */
  size_t len;             /**< Longitud actual, sin contar el NUL */
} strbuf_t;

/**
 * @brief Inicializa un buffer como string vacio
 * @param buf Buffer a inicializar
 */
void strbuf_init(strbuf_t *buf);

/**
 * @brief Reemplaza el contenido del buffer por un string
 * @param buf Buffer destino
 * @param src String fuente terminado en NUL -> se trunca si no entra
 */
void strbuf_set(strbuf_t *buf, const char *src);

/**
 * @brief Agrega un string al final del buffer
 * @param buf Buffer destino
 * @param src String a agregar -> se trunca si no entra
 * @return Cantidad de caracteres efectivamente agregados
 */
size_t strbuf_append(strbuf_t *buf, const char *src);

/**
 * @brief Agrega la representacion decimal de un entero
 * @param buf Buffer destino
 * @param value Entero a convertir y agregar
 * @return Cantidad de caracteres efectivamente agregados
 */
size_t strbuf_append_int(strbuf_t *buf, int32_t value);

/**
 * @brief Devuelve un puntero de solo lectura al string del buffer
 * @param buf Buffer a consultar
 * @return Puntero a buf->data, terminado en NUL
 */
const char *strbuf_cstr(const strbuf_t *buf);

/**
 * @brief Recorta espacios en blanco al inicio y al final
 * @param dst     Buffer destino
 * @param src     String fuente
 * @param max_len Tamano de @p dst incluyendo espacio para el NUL
 * @return Longitud del string recortado
 *
 * Se consideran los caracteres de isspace: espacio, tab, LF, VT, FF y CR
 * Copia como maximo max_len - 1 caracteres y siempre agrega el NUL
 * Si max_len es 0, no escribe en @p dst y devuelve 0
 */
size_t str_trim(char *dst, const char *src, size_t max_len);

/**
 * @brief Calcula el CRC-8 con polinomio 0x07
 * @param data Puntero a los datos de entrada
 * @param len  Cantidad de bytes
 * @return CRC-8 de 8 bits, comenzando en STR_CRC8_INIT
 */
uint8_t str_crc8(const uint8_t *data, size_t len);

#endif /* STRUTILS_H */
