#include "led.h"
#include "config.h"

void led_init(void) { REG_LED = 0; }
void led_on(uint8_t idx) {
  if (idx >= 0 && idx < 8) {
    REG_LED |= (1u << idx);
  }
}
void led_off(uint8_t idx) {
  if (idx >= 0 && idx < 8) {
    REG_LED &= ~(1u << idx);
  }
}
void led_toggle(uint8_t idx) {
  if (idx >= 0 && idx < 8) {
    REG_LED ^= (1u << idx);
  }
}
void led_write(uint8_t value) { REG_LED = value; }
void led_set_mask(uint32_t mask) { REG_LED = mask; }
uint32_t led_get_mask(void) { return REG_LED; }
uint8_t led_get_status(uint8_t idx) {
  if (idx >= 0 && idx < 8) {
    return (REG_LED >> idx) & 1;
  }
  return 0;
}