/**
 * @file lv_sdl_keyboard.h
 *
 */

#ifndef LV_SDL_KEYBOARD_H
#define LV_SDL_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_sdl_window.h"
#if LV_USE_SDL || LV_USE_SDL3

/*********************
 *      DEFINES
 *********************/
#ifndef KEYBOARD_BUFFER_SIZE
#define KEYBOARD_BUFFER_SIZE 32
#endif

/** Custom key codes for Ctrl shortcuts (sent via LV_EVENT_KEY).
 *  Values chosen in a range that won't collide with LV_KEY_* (0x02..0x7F)
 *  or valid UTF-8 packed keys. */
#define LV_KEY_CTRL_A   0xF001u   /**< Select All */
#define LV_KEY_CTRL_C   0xF002u   /**< Copy */
#define LV_KEY_CTRL_V   0xF003u   /**< Paste */
#define LV_KEY_CTRL_X   0xF004u   /**< Cut */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_indev_t * lv_sdl_keyboard_create(void);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_SDL*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_SDL_KEYBOARD_H */
