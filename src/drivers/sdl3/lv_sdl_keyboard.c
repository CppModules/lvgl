/**
 * @file lv_sdl_keyboard.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../sdl/lv_sdl_keyboard.h"
#if LV_USE_SDL3

#include "../../core/lv_group.h"
#include "../../core/lv_obj_event.h"
#include "../../stdlib/lv_string.h"
#include "../../misc/lv_text_private.h"
#include "lv_sdl3_private.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    char buf[KEYBOARD_BUFFER_SIZE];
    bool dummy_read;
} lv_sdl_keyboard_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void sdl_keyboard_read(lv_indev_t * indev, lv_indev_data_t * data);
static uint32_t keycode_to_ctrl_key(SDL_Keycode sdl_key);
static void release_indev_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_indev_t * lv_sdl_keyboard_create(void)
{
    lv_sdl_keyboard_t * dsc = lv_malloc_zeroed(sizeof(lv_sdl_keyboard_t));
    LV_ASSERT_MALLOC(dsc);
    if(dsc == NULL) return NULL;

    lv_indev_t * indev = lv_indev_create();
    LV_ASSERT_MALLOC(indev);
    if(indev == NULL) {
        lv_free(dsc);
        return NULL;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, sdl_keyboard_read);
    lv_indev_set_driver_data(indev, dsc);
    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    lv_indev_add_event_cb(indev, release_indev_cb, LV_EVENT_DELETE, indev);

    return indev;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Get the byte length of a UTF-8 character from its leading byte.
 * @param c the leading byte
 * @return 1..4 on success, 1 on invalid leading byte (treat as single byte)
 */
static uint8_t utf8_byte_length(uint8_t c)
{
    if((c & 0x80) == 0)    return 1;  /* 0xxxxxxx — ASCII */
    if((c & 0xE0) == 0xC0) return 2;  /* 110xxxxx */
    if((c & 0xF0) == 0xE0) return 3;  /* 1110xxxx */
    if((c & 0xF8) == 0xF0) return 4;  /* 11110xxx */
    return 1;                          /* invalid — consume 1 byte */
}

/**
 * Count the number of UTF-8 characters in a null-terminated string.
 */
static size_t utf8_char_count(const char * s)
{
    size_t count = 0;
    while(*s) {
        s += utf8_byte_length((uint8_t)*s);
        count++;
    }
    return count;
}

/**
 * Pack a UTF-8 character into a uint32_t for lv_textarea_add_char().
 *
 * lv_textarea_add_char(obj, c) interprets c as:
 *   uint32_t u32_buf[2] = {c, 0};
 *   const char *letter_buf = (char *)&u32_buf;
 * On little-endian systems the raw UTF-8 bytes must sit at the low address,
 * which means they occupy the least-significant bytes of the uint32_t.
 * This is exactly what memcpy(&result, s, len) produces on little-endian.
 *
 * @param s pointer to UTF-8 bytes (from SDL_TEXTINPUT, guaranteed UTF-8)
 * @param out_len receives the byte length of the character (1..4)
 * @return UTF-8 bytes packed into uint32_t (native byte order)
 */
static uint32_t utf8_pack_to_key(const char * s, uint8_t * out_len)
{
    uint8_t len = utf8_byte_length((uint8_t)s[0]);
    if(out_len) *out_len = len;

    /* Pack raw UTF-8 bytes directly into uint32_t.
     * On little-endian: bytes[0] goes to LSB — matches what
     * lv_textarea_add_char expects when it casts uint32_t* to char*. */
    uint32_t key = 0;
    lv_memcpy(&key, s, len);
    return key;
}

static void sdl_keyboard_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    lv_sdl_keyboard_t * dev = lv_indev_get_driver_data(indev);

    const size_t len = lv_strlen(dev->buf);

    /*Send a release manually*/
    if(dev->dummy_read) {
        dev->dummy_read = false;
        data->state = LV_INDEV_STATE_RELEASED;
    }
    /*Send the pressed character — read a full UTF-8 character*/
    else if(len > 0) {
        dev->dummy_read = true;
        data->state = LV_INDEV_STATE_PRESSED;

        uint8_t char_len = 0;
        data->key = utf8_pack_to_key(dev->buf, &char_len);

        /* Shift the remaining buffer content (including the null terminator) */
        lv_memmove(dev->buf, dev->buf + char_len, len - char_len + 1);
    }
}

static void release_indev_cb(lv_event_t * e)
{
    lv_indev_t * indev = (lv_indev_t *) lv_event_get_user_data(e);
    lv_sdl_keyboard_t * dev = lv_indev_get_driver_data(indev);
    if(dev) {
        lv_indev_set_driver_data(indev, NULL);
        lv_indev_set_read_cb(indev, NULL);
        lv_free(dev);
        LV_LOG_INFO("done");
    }
}

void lv_sdl_keyboard_handler(SDL_Event * event)
{
    uint32_t win_id = UINT32_MAX;
    switch(event->type) {
        case SDL_KEYDOWN:
            win_id = event->key.windowID;
            break;
        case SDL_TEXTINPUT:
            win_id = event->text.windowID;
            break;
        default:
            return;
    }

    lv_display_t * disp = lv_sdl_get_disp_from_win_id(win_id);


    /*Find a suitable indev*/
    lv_indev_t * indev = lv_indev_get_next(NULL);
    while(indev) {
        if(lv_indev_get_read_cb(indev) == sdl_keyboard_read) {
            /*If disp is NULL for any reason use the first indev with the correct type*/
            if(disp == NULL || lv_indev_get_display(indev) == disp) break;
        }
        indev = lv_indev_get_next(indev);
    }
    if(indev == NULL) return;
    lv_sdl_keyboard_t * dsc = lv_indev_get_driver_data(indev);

    /* We only care about SDL_KEYDOWN and SDL_TEXTINPUT events */
    switch(event->type) {
        case SDL_KEYDOWN: {                     /*Button press*/
                /* Handle Ctrl+A/C/V/X shortcuts — these bypass the char buffer
                   and send a custom key code directly to the focused object. */
                /*SDL3 removed the keysym indirection: keysym.sym -> key,
                  keysym.mod -> mod.*/
                if(event->key.mod & KMOD_CTRL) {
                    uint32_t custom_key = 0;
                    switch(event->key.key) {
                        case SDLK_a: custom_key = LV_KEY_CTRL_A; break;
                        case SDLK_c: custom_key = LV_KEY_CTRL_C; break;
                        case SDLK_v: custom_key = LV_KEY_CTRL_V; break;
                        case SDLK_x: custom_key = LV_KEY_CTRL_X; break;
                        default: break;
                    }
                    if(custom_key) {
                        lv_group_t * g = lv_group_get_default();
                        if(g) {
                            lv_obj_t * focused = lv_group_get_focused(g);
                            if(focused) {
                                lv_obj_send_event(focused, LV_EVENT_KEY, &custom_key);
                            }
                        }
                        return;
                    }
                }

                const uint32_t ctrl_key = keycode_to_ctrl_key(event->key.key);
                if(ctrl_key == '\0')
                    return;
                const size_t len = lv_strlen(dsc->buf);
                if(len < KEYBOARD_BUFFER_SIZE - 1) {
                    dsc->buf[len] = ctrl_key;
                    dsc->buf[len + 1] = '\0';
                }
                break;
            }
        case SDL_TEXTINPUT: {                   /*Text input*/
                const size_t len = lv_strlen(dsc->buf) + lv_strlen(event->text.text);
                if(len < KEYBOARD_BUFFER_SIZE - 1)
                    lv_strcat(dsc->buf, event->text.text);
            }
            break;
        default:
            break;

    }

    /* Count UTF-8 characters (not bytes) so that each multi-byte character
     * is delivered as a single key press + release pair. */
    size_t char_cnt = utf8_char_count(dsc->buf);
    while(char_cnt) {
        lv_indev_read(indev);

        /*Call again to handle dummy read in `sdl_keyboard_read`*/
        lv_indev_read(indev);
        char_cnt--;
    }
}

/**
 * Convert a SDL key code to it's LV_KEY_* counterpart or return '\0' if it's not a control character.
 * @param sdl_key the key code
 * @return LV_KEY_* control character or '\0'
 */
static uint32_t keycode_to_ctrl_key(SDL_Keycode sdl_key)
{
    /*Remap some key to LV_KEY_... to manage groups*/
    switch(sdl_key) {
        case SDLK_RIGHT:
        case SDLK_KP_PLUS:
            return LV_KEY_RIGHT;

        case SDLK_LEFT:
        case SDLK_KP_MINUS:
            return LV_KEY_LEFT;

        case SDLK_UP:
            return LV_KEY_UP;

        case SDLK_DOWN:
            return LV_KEY_DOWN;

        case SDLK_ESCAPE:
            return LV_KEY_ESC;

        case SDLK_BACKSPACE:
            return LV_KEY_BACKSPACE;

        case SDLK_DELETE:
            return LV_KEY_DEL;

        case SDLK_KP_ENTER:
        case '\r':
            return LV_KEY_ENTER;

        case SDLK_TAB:
        case SDLK_PAGEDOWN:
            return LV_KEY_NEXT;

        case SDLK_PAGEUP:
            return LV_KEY_PREV;

        case SDLK_HOME:
            return LV_KEY_HOME;

        case SDLK_END:
            return LV_KEY_END;

        default:
            return '\0';
    }
}

#endif /*LV_USE_SDL3*/
