#ifndef ADC_H
#define ADC_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Inicializa el ADC con un divisor de reloj específico.
 * @param clk_div El divisor de reloj para el ADC.
 */
void adc_init(uint32_t clk_div);

/**
 * @brief Inicia la conversión ADC en el modo especificado.
 * @param adc_mode El modo de operación del ADC (0: single, 1: continuous).
 */
void adc_start(uint8_t adc_mode);

/**
 * @brief Verifica si la conversión ADC ha alcanzado la mitad de su proceso.
 * @return 1 si la conversión está a la mitad, 0 en caso contrario.
 */
uint8_t adc_half_done(void);

/**
 * @brief Espera a que la conversión ADC haya finalizado.
 * @return 1 cuando la conversión está completa.
 *
 * Bloquea hasta que el bit de "done" del registro de estado se active.
 */
uint8_t adc_done(void);

/**
 * @brief Lee los datos convertidos del ADC.
 * @param data Puntero al buffer donde se almacenarán los datos leídos.
 * @param size Número de palabras (4 bytes cada una) a leer.
 * @return El número de palabras leídas, o 0 si hubo un error.
 *
 * Si `data` es NULL o `size` es 0, la función retorna 0. Si `size` es mayor
 * que `REG_ARRAY_ADC_BUFF_SIZE`, se ajusta a ese valor máximo.
 *
 * La función dispara una adquisición nueva, espera a que termine y recién
 * entonces copia el buffer. Si el divisor de reloj quedó en 0, lo configura
 * en 1 para evitar que la conversión quede colgada.
 */
int adc_read(uint32_t *data, uint32_t size);

/**
 * @brief Configura el divisor de reloj del ADC.
 * @param clk_div El divisor de reloj a establecer.
 *
 * Se agrega aunque la consigna no la pide porque en test_drivers.c se utiliza
 * la función set_adc_clk_divider (o cambio el test o agrego la función).
 */
void set_adc_clk_divider(uint32_t clk_div);

#endif /* ADC_H */