#ifndef HAL_LED_INCLUDED
#define HAL_LED_INCLUDED

#include <stdbool.h>

/**
 * @brief Inicializa o LED embebido na placa.
 * 
 * @return Se true, inicialização não teve problema.
 * @return Se false, inicialização falhou.
 */
inline bool hal_led_init(void);

/**
 * @brief Alterna o estado do LED embebido na placa.
 * 
 */
inline void hal_led_toggle(void);

#endif