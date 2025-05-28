#include "worldlist.h"
#include <stdio.h>

#ifdef EMSCRIPTEN
#define USE_WEBSOCKS 1
#else
#define USE_WEBSOCKS 0
#endif

static void worldlist_select_default(mudclient *mud) {
    strcpy(mud->server, "game.openrsc.com");
    mud->port = USE_WEBSOCKS ? 43496 : 43596;
    strcpy(mud->rsa_exponent, "00010001");
    strcpy(mud->rsa_modulus, "87cef754966ecb19806238d9fecf0f421e816976f74f365c86a584e51049794d41fefbdc5fed3a3ed3b7495ba24262bb7d1dd5d2ff9e306b5bbf5522a2e85b25");
    mud->options->last_world = 0;
    printf("INFO: Using default server: OpenRSC Preservation\n");
}

void worldlist_new(mudclient *mud) {
    // Just set the default server instead of creating UI
    if (mud->server[0] == '\0') {
        worldlist_select_default(mud);
    }
}

void worldlist_handle_mouse(mudclient *mud) {
 }