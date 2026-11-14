#ifndef LED_H
#define LED_H

#include <stdint.h>

/**
 * @brief Inicializa el registro de LEDs. Limpia el registro poniendo todos los
 * LEDs en estado apagado.
 */
void led_init(void);

/**
 * @brief Enciende el LED correspondiente al índice idx aplicando una OR.
 * @param idx Índice del LED a encender (0-7).
 */
void led_on(uint8_t idx);

/**
 * @brief Apaga el LED correspondiente al índice idx aplicando una AND con el
 * complemento.
 * @param idx Índice del LED a apagar (0-7).
 */
void led_off(uint8_t idx);

/**
 * @brief Alterna el estado del LED correspondiente al índice idx aplicando una
 * XOR.
 * @param idx Índice del LED a alternar (0-7).
 */
void led_toggle(uint8_t idx);

/**
 * @brief Escribe un valor de 8 bits en el registro de LEDs.
 * @param value Valor a escribir en el registro de LEDs.
 */
void led_write(uint8_t value);

/**
 * @brief Establece la máscara de LEDs.
 * @param mask Máscara de LEDs a establecer.
 */
void led_set_mask(uint32_t mask);

/**
 * @brief Obtiene la máscara de LEDs.
 * @return Máscara de LEDs actual.
 */
uint32_t led_get_mask(void);

/**
 * @brief Obtiene el estado de un LED específico.
 * @param idx Índice del LED del que se quiere obtener el estado (0-7).
 * @return Estado del LED (1 si está encendido, 0 si está apagado).
 */
uint8_t led_get_status(uint8_t idx);

#endif /* LED_H */