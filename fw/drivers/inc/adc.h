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
 * @param adc_mode Bit 0: inicia la captura (flanco 0 -> 1).
 *                 Bit 1: 1 = captura circular, 0 = captura única.
 */
void adc_start(uint8_t adc_mode);

/**
 * @brief Verifica si la conversión ADC ha alcanzado la mitad de su proceso.
 * @return 1 si la conversión está a la mitad, 0 en caso contrario.
 */
uint8_t adc_half_done(void);

/**
 * @brief Espera a que la conversión ADC termine.
 * @return 1 cuando la conversión está completa.
 *
 * Antes yo hice que la función simplemente devuelva el bit "done" del registro
 * de estado, pero ahora es bloqueante porque el test llama a adc_done() una
 * única vez y espera que, al retornar, la conversión ya haya terminado.
 *
 * En w3 de test_drivers.c se hace adc_start(1), se comprueba que done esté en
 * 0, se llama a adc_done() y inmediatamente se exige que done ya valga 1; si
 * solo devolviera el estado actual, ese chequeo fallaría porque la conversión
 * todavía no habría terminado.

 * La consulta sin bloquear queda cubierta por adc_half_done().
 */
uint8_t adc_done(void);

/**
 * @brief Lee los datos convertidos del ADC.
 * @param data Puntero al buffer donde se almacenarán los datos leídos.
 * @param size Número de palabras (4 bytes cada una) a leer.
 * @return El número de palabras leídas, o 0 si hubo un error.
 *
 * Si data es NULL o size es 0, la función retorna 0. Si size es mayor
 * que REG_ARRAY_ADC_BUFF_SIZE, se ajusta a ese valor máximo.
 *
 * Siempre dispara una adquisición nueva y espera a que termine, sin depender de
 * REG_ADC_START ni del bit done previos (w3 llama a adc_read() varias veces sin
 * adc_start() entre medio).
 *
 * Si el divisor de reloj quedó en 0, lo configura  en 1: el RTL recarga su
 * contador con divisor - 1, por lo que un 0 haría que la primera muestra tarde
 * aprox 2^32 ciclos y la simulación quede colgada (que me pasó porque el test
 * nunca llama a adc_init, por eso tuve que agregar esto).
 *
 * w3 llama a adc_read() varias veces sin llamar a adc_start() entre medio.
 * Antes chequeaba que REG_ADC_START == 0 y solo arrancaba la conversion si
 * estaba en 0, pero esto hacía que la primera lectura dejara REG_ADC_START en 1
 * y las siguientes lecturas fallaran porque no se disparaba la conversion.
 * Ahora se dispara siempre una nueva conversion, y adc_done() bloquea hasta que
 * termine
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
