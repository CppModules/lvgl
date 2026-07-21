/**
 * @file lv_sdl_window.h
 *
 */

#ifndef LV_SDL_WINDOW_H
#define LV_SDL_WINDOW_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../display/lv_display.h"
#include "../../indev/lv_indev.h"

typedef struct SDL_Window SDL_Window;

#if LV_USE_SDL || LV_USE_SDL3

/*********************
 *      DEFINES
 *********************/

/* Possible values of LV_SDL_MOUSEWHEEL_MODE */
#define LV_SDL_MOUSEWHEEL_MODE_ENCODER  0  /* The mousewheel emulates an encoder input device*/
#define LV_SDL_MOUSEWHEEL_MODE_CROWN    1  /* The mousewheel emulates a smart watch crown*/

/**********************
 *      TYPEDEFS
 **********************/
//

// event type is SDL_Event *; set SDL_USEREVENT to skip default handling
typedef void(*lv_sdl_window_event_callback)(void* event);
typedef bool (*lv_sdl_window_present_callback)(lv_display_t * disp,
                                                struct SDL_Window * window,
                                                const void * pixels,
                                                int32_t width, int32_t height,
                                                lv_color_format_t format);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_display_t * lv_sdl_window_create(int32_t hor_res, int32_t ver_res);
lv_display_t * lv_sdl_window_create_from(int32_t hor_res, int32_t ver_res,void* window);

SDL_Window * lv_sdl_window_get_window(lv_display_t * disp);

void lv_sdl_window_set_resizeable(lv_display_t * disp, bool value);

void lv_sdl_window_set_zoom(lv_display_t * disp, float zoom);

float lv_sdl_window_get_zoom(lv_display_t * disp);

void lv_sdl_window_set_input_scale(lv_display_t * disp, float scale);

float lv_sdl_window_get_input_scale(lv_display_t * disp);

void lv_sdl_window_set_title(lv_display_t * disp, const char * title);

void * lv_sdl_window_get_renderer(lv_display_t * disp);

void lv_sdl_quit(void);

void lv_sdl_window_set_event_callback(lv_sdl_window_event_callback cb);

void lv_sdl_window_set_present_callback(lv_sdl_window_present_callback cb);

/**********************
 *      MACROS
 **********************/


#endif /* LV_DRV_SDL */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_SDL_WINDOW_H */
