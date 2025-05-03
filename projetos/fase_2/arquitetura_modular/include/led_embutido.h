#ifndef LED_EMBUTIDO_INCLUDED
#define LED_EMBUTIDO_INCLUDED

#include <stdbool.h>

/**
 * @brief Inicializa o modulo CYW43 do Pico W e confere se foi um sucesso.
 * 
 * @return Se true, a inicialização foi um sucesso.
 * @return Se false, a inicialização falhou.
 */
inline bool picow_led_init(void);


/**
 * @brief API para ligar ou desligar LED ligado no CYW43 do Pico W.
 * 
 * @param status Se true acende LED, se false desliga LED.
 */
inline void picow_led_put(bool status);
#endif

