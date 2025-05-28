#include "mudclient.h"
#include <SDL/SDL.h>
#include <ctype.h>
#include <kos/dbglog.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>

// Framebuffer dimensions
#define FRAMEBUFFER_WIDTH 512
#define FRAMEBUFFER_HEIGHT 346

// Keyboard Buffer
#define DREAMCAST_KEYBOARD_BUFFER_SIZE 64

// D-Pad movement speed (pixels per frame)
#define DPAD_SPEED 5

// Static variables for tracking mouse position
static int s_cursor_x = FRAMEBUFFER_WIDTH / 2;
static int s_cursor_y = FRAMEBUFFER_HEIGHT / 2;

// Keyboard buffer
char dreamcast_keyboard_buffer[DREAMCAST_KEYBOARD_BUFFER_SIZE];
int keyboard_buffer_pos = 0;

// Function declarations
void append_to_keyboard_buffer(char c);
void process_backspace(void);
void poll_dreamcast_controller(mudclient *mud);

void reset_keyboard_buffer() {
    memset(dreamcast_keyboard_buffer, 0, DREAMCAST_KEYBOARD_BUFFER_SIZE);
    keyboard_buffer_pos = 0;
}

void append_to_keyboard_buffer(char c) {
    if (keyboard_buffer_pos < DREAMCAST_KEYBOARD_BUFFER_SIZE - 1) {
        dreamcast_keyboard_buffer[keyboard_buffer_pos++] = c;
        dreamcast_keyboard_buffer[keyboard_buffer_pos] = '\0';
    } else {
        dbglog(DBG_WARNING, "Keyboard buffer full, cannot append character: %c\n", c);
    }
}

void process_backspace() {
    if (keyboard_buffer_pos > 0) {
        dreamcast_keyboard_buffer[--keyboard_buffer_pos] = '\0';
    }
}

void poll_dreamcast_controller(mudclient *mud) {
    maple_device_t *cont;
    cont_state_t *state;
    
    cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    if(cont) {
        state = (cont_state_t *)maple_dev_status(cont);
        if(state) {
            // Handle D-pad for mouse movement
if(state->buttons & CONT_DPAD_UP) {
    s_cursor_y -= DPAD_SPEED;
}
            if(state->buttons & CONT_DPAD_DOWN) {
                s_cursor_y += DPAD_SPEED;

            }
            if(state->buttons & CONT_DPAD_LEFT) {
                s_cursor_x -= DPAD_SPEED;

            }
            if(state->buttons & CONT_DPAD_RIGHT) {
                s_cursor_x += DPAD_SPEED;

            }
            
            // Boundary check
            if(s_cursor_x < 0) s_cursor_x = 0;
            if(s_cursor_x >= FRAMEBUFFER_WIDTH) s_cursor_x = FRAMEBUFFER_WIDTH - 1;
            if(s_cursor_y < 0) s_cursor_y = 0;
            if(s_cursor_y >= FRAMEBUFFER_HEIGHT) s_cursor_y = FRAMEBUFFER_HEIGHT - 1;
            
            // Update mouse position
            mudclient_mouse_moved(mud, s_cursor_x, s_cursor_y);
            SDL_WarpMouse(s_cursor_x, s_cursor_y);

            // Handle button presses for mouse clicks
            static int a_pressed = 0;
            static int b_pressed = 0;
            
            // A button = left click
            if(state->buttons & CONT_A) {
                if(!a_pressed) {
                    mudclient_mouse_pressed(mud, s_cursor_x, s_cursor_y, 1);
                    a_pressed = 1;
                }
            } else {
                if(a_pressed) {
                    mudclient_mouse_released(mud, s_cursor_x, s_cursor_y, 1);
                    a_pressed = 0;
                }
            }
            
            // B button = right click
            if(state->buttons & CONT_B) {
                if(!b_pressed) {
                    mudclient_mouse_pressed(mud, s_cursor_x, s_cursor_y, 3);
                    b_pressed = 1;
                }
            } else {
                if(b_pressed) {
                    mudclient_mouse_released(mud, s_cursor_x, s_cursor_y, 3);
                    b_pressed = 0;
                }
            }
            
            // L/R triggers for scrolling
            if(state->ltrig > 10) {
                mud->mouse_scroll_delta = -1;
            }
            if(state->rtrig > 10) {
                mud->mouse_scroll_delta = 1;
            }
        }
    }
}

void mudclient_poll_events(mudclient *mud) {
    // Poll Dreamcast controller first
    poll_dreamcast_controller(mud);
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {

            case SDL_QUIT:
                exit(0);
                break;

            case SDL_KEYDOWN: {
                char char_code = -1;
                int code = -1;
                get_sdl_keycodes(&event.key.keysym, &char_code, &code);
                if (code != -1) {
                    dbglog(DBG_INFO, "Key pressed: %d (%c)\n", code, char_code);
                    if (char_code != -1 && isprint(char_code)) {
                        append_to_keyboard_buffer(char_code);
                    } else if (code == SDLK_BACKSPACE) {
                        process_backspace();
                    }
                    mudclient_key_pressed(mud, code, char_code);
                }

                // Toggle FPS display
                if (event.key.keysym.sym == SDLK_F2) {
                    mud->options->display_fps = !mud->options->display_fps;
                }
                break;
            }

            case SDL_KEYUP: {
                char char_code = -1;
                int code = -1;
                get_sdl_keycodes(&event.key.keysym, &char_code, &code);
                if (code != -1) {
                    dbglog(DBG_INFO, "Key released: %d\n", code);
                    mudclient_key_released(mud, code);
                }
                break;
            }

#ifndef SDL12
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    mudclient_on_resize(mud);
                }
                break;
#else
            case SDL_VIDEORESIZE:
                mudclient_sdl1_on_resize(mud, event.resize.w, event.resize.h);
                break;
#endif

            default:
                break;
        } // end switch
    } // end while (SDL_PollEvent)
}

void mudclient_start_application(mudclient *mud, char *title) {
    dbglog(DBG_INFO, "Mudclient initialized for Dreamcast\n");
    int init = SDL_INIT_VIDEO | SDL_INIT_TIMER;
    if (SDL_Init(init) < 0) {
        mud_error("SDL_Init(): %s\n", SDL_GetError());
        exit(1);
    }

    SDL_EnableUNICODE(1);
    SDL_WM_SetCaption(title, NULL);

#ifdef RENDER_SW
    mud->screen = SDL_SetVideoMode(mud->game_width, mud->game_height, 16,
                                   SDL_SWSURFACE | SDL_FULLSCREEN);
#elif defined(RENDER_GL)
    mud->screen = SDL_SetVideoMode(mud->game_width, mud->game_height, 16,
                                   SDL_OPENGL | SDL_FULLSCREEN);
#endif
}

#ifdef SDL12
void get_sdl_keycodes(SDL_keysym *keysym, char *char_code, int *code) {
    *code = -1;
    *char_code = -1;

    // Check for printable characters directly
    if (keysym->unicode > 0 && keysym->unicode < 128 && isprint((unsigned char)keysym->unicode)) {
        *code = keysym->unicode;
        *char_code = keysym->unicode;
        return;
    }

    // Handle specific keys
    switch (keysym->sym) {
        case SDLK_TAB: 
            *code = K_TAB; 
            *char_code = '\t'; 
            break;
        case SDLK_BACKSPACE: 
            *code = K_BACKSPACE; 
            *char_code = '\b'; 
            break;
        case SDLK_RETURN: 
            *code = K_ENTER; 
            *char_code = '\r'; 
            break;
        case SDLK_ESCAPE: 
            *code = K_ESCAPE; 
            break;

        // Arrow keys
        case SDLK_LEFT: case SDLK_RIGHT: case SDLK_UP: case SDLK_DOWN:
            *code = keysym->sym;
            break;

        // Function keys
        case SDLK_F1: case SDLK_F2: case SDLK_F3: case SDLK_F4: case SDLK_F5:
        case SDLK_F6: case SDLK_F7: case SDLK_F8: case SDLK_F9:
        case SDLK_F10: case SDLK_F11: case SDLK_F12:
            *code = keysym->sym;
            break;

        // Alphabet (upper-case and lower-case)
        case SDLK_a: case SDLK_b: case SDLK_c: case SDLK_d: case SDLK_e:
        case SDLK_f: case SDLK_g: case SDLK_h: case SDLK_i: case SDLK_j:
        case SDLK_k: case SDLK_l: case SDLK_m: case SDLK_n: case SDLK_o:
        case SDLK_p: case SDLK_q: case SDLK_r: case SDLK_s: case SDLK_t:
        case SDLK_u: case SDLK_v: case SDLK_w: case SDLK_x: case SDLK_y:
        case SDLK_z:
            *char_code = (keysym->mod & KMOD_SHIFT) ? toupper(keysym->sym) : tolower(keysym->sym);
            *code = keysym->sym;
            break;

        // Numeric and shifted symbols
        case SDLK_1:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '!' : '1';
            *code = keysym->sym;
            break;
        case SDLK_2:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '@' : '2';
            *code = keysym->sym;
            break;
        case SDLK_3:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '#' : '3';
            *code = keysym->sym;
            break;
        case SDLK_4:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '$' : '4';
            *code = keysym->sym;
            break;
        case SDLK_5:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '%' : '5';
            *code = keysym->sym;
            break;
        case SDLK_6:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '^' : '6';
            *code = keysym->sym;
            break;
        case SDLK_7:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '&' : '7';
            *code = keysym->sym;
            break;
        case SDLK_8:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '*' : '8';
            *code = keysym->sym;
            break;
        case SDLK_9:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '(' : '9';
            *code = keysym->sym;
            break;
        case SDLK_0:
            *char_code = (keysym->mod & KMOD_SHIFT) ? ')' : '0';
            *code = keysym->sym;
            break;

        // Punctuation and symbols
        case SDLK_MINUS:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '_' : '-';
            *code = keysym->sym;
            break;
        case SDLK_EQUALS:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '+' : '=';
            *code = keysym->sym;
            break;
        case SDLK_BACKQUOTE:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '~' : '`';
            *code = keysym->sym;
            break;
        case SDLK_LEFTBRACKET:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '{' : '[';
            *code = keysym->sym;
            break;
        case SDLK_RIGHTBRACKET:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '}' : ']';
            *code = keysym->sym;
            break;
        case SDLK_BACKSLASH:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '|' : '\\';
            *code = keysym->sym;
            break;
        case SDLK_SEMICOLON:
            *char_code = (keysym->mod & KMOD_SHIFT) ? ':' : ';';
            *code = keysym->sym;
            break;
        case SDLK_QUOTE:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '"' : '\'';
            *code = keysym->sym;
            break;
        case SDLK_COMMA:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '<' : ',';
            *code = keysym->sym;
            break;
        case SDLK_PERIOD:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '>' : '.';
            *code = keysym->sym;
            break;
        case SDLK_SLASH:
            *char_code = (keysym->mod & KMOD_SHIFT) ? '?' : '/';
            *code = keysym->sym;
            break;

        default:
            if (keysym->unicode > 0) {
                *code = keysym->unicode;
                *char_code = keysym->unicode;
            }
            break;
    }
}
#endif