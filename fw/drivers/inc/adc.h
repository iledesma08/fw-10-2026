#ifndef ADC_H
#define ADC_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Inicializa el ADC con un divisor de reloj especifico
 * @param clk_div El divisor de reloj para el ADC
 */
void adc_init(uint32_t clk_div);

/**
 * @brief Inicia la conversion ADC en el modo especificado
 * @param adc_mode Bit 0: inicia la captura (flanco 0 -> 1).
 *                 Bit 1: 1 = captura circular, 0 = captura unica
 */
void adc_start(uint8_t adc_mode);

/**
 * @brief Espera a que la conversion ADC alcance la mitad de su proceso
 * @return 1 cuando el flag half_done (bit 0 de REG_ADC_STATUS) esta activo
 *
 * Funcion bloqueante: realiza polling sobre el bit 0 de REG_ADC_STATUS con
 * mascara hasta que la captura alcanza la mitad de las muestras
 */
uint8_t adc_half_done(void);

/**
 * @brief Espera a que la conversion ADC termine
 * @return 1 cuando la conversion esta completa (bit 1 de REG_ADC_STATUS)
 *
 * Funcion bloqueante: realiza polling sobre el bit 1 de REG_ADC_STATUS con
 * mascara hasta que finaliza la captura completa
 */
uint8_t adc_done(void);

/**
 * @brief Lee los datos convertidos del ADC
 * @param data Puntero al buffer donde se almacenaran los datos leidos
 * @param size Numero de palabras (4 bytes cada una) a leer
 * @return El numero de palabras leidas, o 0 si hubo un error
 *
 * Si data es NULL o size es 0, la funcion retorna 0. Si size es mayor
 * que REG_ARRAY_ADC_BUFF_SIZE, se ajusta a ese valor maximo.
 *
 * Utiliza el divisor de reloj ya configurado previamente en REG_ADC_CLK_DIVIDER
 * (por ejemplo via adc_init), dispara una captura unica con adc_start(1),
 * espera a que termine con adc_done() y copia las muestras al buffer de destino
 */
int adc_read(uint32_t *data, uint32_t size);

#endif /* ADC_H */
