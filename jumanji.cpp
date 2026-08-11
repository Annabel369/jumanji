#include <iostream.h>
#include <stdio.h>
#include <dos.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alloc.h>

// Screen dimensions (VESA Mode 103h)
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// Board settings
#define TOTAL_TILES 60
#define TOTAL_PLAYERS 4

// Position structure
struct Position {
    int x, y;
};

// Event card structure
struct EventCard {
    char title[20];
    char line1[20];
    char line2[20];
    int move_offset;
    int extra_turn;
    int skip_turn;
};

// Global variables
Position path[TOTAL_TILES];
int p_pos[TOTAL_PLAYERS] = {0, 0, 0, 0};
int p_skip[TOTAL_PLAYERS] = {0, 0, 0, 0};
int p_dead[TOTAL_PLAYERS] = {0, 0, 0, 0};

// Turn order variables (resolved by the tournament)
int turn_order[TOTAL_PLAYERS] = {0, 1, 2, 3}; // Map slots 0-3 to player indices
int current_turn_idx = 0;                     // Index in turn_order (0-3)
int last_dead_player = -1;                    // Track the last player index who died

unsigned int current_bank = 999;
unsigned char far* rom_font = NULL;

// Event cards deck
EventCard deck[10] = {
    { "TERREMOTO!",    "ABALO SISMICO!", "RECUE 5 CASAS",  -5, 0, 0 },
    { "ERUPCAO!",      "LAVA ARDENTE!",  "RECUE 6 CASAS",  -6, 0, 0 },
    { "TORNADO!",      "VENTO ARRASA!",  "RECUE 3 CASAS",  -3, 0, 0 },
    { "INUNDACAO!",    "RIO TRANSBORDA!","RECUE 8 CASAS",  -8, 0, 0 },
    { "ATAQUE DE LEAO!","FUJA PARA TRAS!","RECUE 4 CASAS",  -4, 0, 0 },
    { "SOL BRILHA!",   "CAMINHO LIMPO!", "AVANCE 5 CASAS",  5, 0, 0 },
    { "MAPA ACHADO!",  "ATALHO SEGURO!", "AVANCE 8 CASAS",  8, 0, 0 },
    { "ABRIGO SEGURO!","NOITE CALMA!",   "AVANCE 4 CASAS",  4, 0, 0 },
    { "VENTO FORTE!",  "A SEU FAVOR!",   "AVANCE 6 CASAS",  6, 0, 0 },
    { "PORTAL RETORNO!","ULTIMO JOGADOR", "MORTO RESUSCITA!",0, 0, 0 }
};

// Path coordinates generator
void generate_path() {
    int idx = 0;
    int x, y;
    
    // Ring 1 (outer border) - y=540, x from 50 to 750 (step 70)
    for (x = 50; x <= 750; x += 70) {
        if (idx < TOTAL_TILES) { path[idx].x = x; path[idx].y = 540; idx++; }
    }
    // Ring 1 (right edge) - x=750, y from 470 down to 50 (step 70)
    for (y = 470; y >= 50; y -= 70) {
        if (idx < TOTAL_TILES) { path[idx].x = 750; path[idx].y = y; idx++; }
    }
    // Ring 1 (top edge) - y=50, x from 680 down to 50 (step 70)
    for (x = 680; x >= 50; x -= 70) {
        if (idx < TOTAL_TILES) { path[idx].x = x; path[idx].y = 50; idx++; }
    }
    // Ring 1 (left edge) - x=50, y from 120 to 470 (step 70)
    for (y = 120; y <= 470; y += 70) {
        if (idx < TOTAL_TILES) { path[idx].x = 50; path[idx].y = y; idx++; }
    }

    // Ring 2 - y=470, x from 120 to 680 (step 70)
    for (x = 120; x <= 680; x += 70) {
        if (idx < TOTAL_TILES) { path[idx].x = x; path[idx].y = 470; idx++; }
    }
    // Ring 2 (right edge) - x=680, y from 400 down to 120 (step 70)
    for (y = 400; y >= 120; y -= 70) {
        if (idx < TOTAL_TILES) { path[idx].x = 680; path[idx].y = y; idx++; }
    }
    // Ring 2 (top edge) - y=120, x from 610 down to 120 (step 70)
    for (x = 610; x >= 120; x -= 70) {
        if (idx < TOTAL_TILES) { path[idx].x = x; path[idx].y = 120; idx++; }
    }
    // Ring 2 (left edge) - x=120, y from 190 to 400 (step 70)
    for (y = 190; y <= 400; y += 70) {
        if (idx < TOTAL_TILES) { path[idx].x = 120; path[idx].y = y; idx++; }
    }

    // Ring 3 (inner spiral adjusting coordinates to approach the crystal ball)
    // Ring 3 bottom row
    for (x = 190; x <= 540; x += 70) {
        if (idx < TOTAL_TILES) { path[idx].x = x; path[idx].y = 400; idx++; }
    }
    
    // Fine-tune final tiles spiraling into the center crystal ball
    if (idx < TOTAL_TILES) { path[idx].x = 540; path[idx].y = 330; idx++; }
    if (idx < TOTAL_TILES) { path[idx].x = 540; path[idx].y = 260; idx++; }
    if (idx < TOTAL_TILES) { path[idx].x = 470; path[idx].y = 190; idx++; }
    if (idx < TOTAL_TILES) { path[idx].x = 400; path[idx].y = 190; idx++; }
    if (idx < TOTAL_TILES) { path[idx].x = 330; path[idx].y = 190; idx++; }
    if (idx < TOTAL_TILES) { path[idx].x = 260; path[idx].y = 220; idx++; }
    if (idx < TOTAL_TILES) { path[idx].x = 260; path[idx].y = 290; idx++; }
    
    // Fill remaining elements to be safe
    while (idx < TOTAL_TILES) {
        path[idx].x = 260;
        path[idx].y = 300;
        idx++;
    }
}

// VESA SVGA Initialization
void init_vesa() {
    union REGS regs;
    regs.x.ax = 0x4F02; // Set VBE Mode
    regs.x.bx = 0x103;  // Mode 103h: 800x600, 256 colors
    int86(0x10, &regs, &regs);
    current_bank = 999;
}

// Restore standard text mode
void close_vesa() {
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x03; // Text mode
    int86(0x10, &regs, &regs);
}

// Get BIOS ROM 8x8 font pointer
void init_font() {
    struct REGPACK regs;
    regs.r_ax = 0x1130;
    regs.r_bx = 0x0300;
    intr(0x10, &regs);
    rom_font = (unsigned char far*)MK_FP(regs.r_es, regs.r_bp);
}

// VESA Set Bank page
void set_vesa_bank(unsigned int bank) {
    if (bank == current_bank) return;
    union REGS regs;
    regs.x.ax = 0x4F05;
    regs.x.bx = 0x0000;
    regs.x.dx = bank;
    int86(0x10, &regs, &regs);
    current_bank = bank;
}

// Put pixel on SVGA VRAM directly
void put_pixel(int x, int y, unsigned char color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        long addr = (long)y * 800L + x;
        unsigned int bank = (unsigned int)(addr >> 16);
        unsigned int offset = (unsigned int)(addr & 0xFFFF);
        set_vesa_bank(bank);
        char far* vram = (char far*)MK_FP(0xA000, offset);
        *vram = color;
    }
}

// Clear screen in SVGA
void clear_screen(unsigned char color) {
    for (unsigned int bank = 0; bank < 8; bank++) {
        set_vesa_bank(bank);
        char far* vram = (char far*)MK_FP(0xA000, 0);
        _fmemset(vram, color, 65536U);
    }
}

// Draw line using Bresenham's algorithm
void draw_line(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        put_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

// Draw rectangle
void draw_rect(int x1, int y1, int x2, int y2, unsigned char color, int fill) {
    if (fill) {
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                put_pixel(x, y, color);
            }
        }
    } else {
        draw_line(x1, y1, x2, y1, color);
        draw_line(x2, y1, x2, y2, color);
        draw_line(x2, y2, x1, y2, color);
        draw_line(x1, y2, x1, y1, color);
    }
}

// Draw horizontal line
void draw_hline(int x1, int x2, int y, unsigned char color) {
    for (int x = x1; x <= x2; x++) {
        put_pixel(x, y, color);
    }
}

// Draw circle
void draw_circle(int xc, int yc, int r, unsigned char color, int fill) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    while (y >= x) {
        if (fill) {
            draw_hline(xc - x, xc + x, yc - y, color);
            draw_hline(xc - x, xc + x, yc + y, color);
            draw_hline(xc - y, xc + y, yc - x, color);
            draw_hline(xc - y, xc + y, yc + x, color);
        } else {
            put_pixel(xc + x, yc + y, color);
            put_pixel(xc - x, yc + y, color);
            put_pixel(xc + x, yc - y, color);
            put_pixel(xc - x, yc - y, color);
            put_pixel(xc + y, yc + x, color);
            put_pixel(xc - y, yc + x, color);
            put_pixel(xc + y, yc - x, color);
            put_pixel(xc - y, yc - x, color);
        }
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

// Draw character
void draw_char(int x, int y, char c, unsigned char color) {
    if (!rom_font) return;
    unsigned char far* char_ptr = rom_font + (unsigned char)c * 8;
    for (int r = 0; r < 8; r++) {
        unsigned char data = char_ptr[r];
        for (int col = 0; col < 8; col++) {
            if (data & (0x80 >> col)) {
                put_pixel(x + col, y + r, color);
            }
        }
    }
}

// Draw string
void draw_string(int x, int y, const char* str, unsigned char color) {
    while (*str) {
        draw_char(x, y, *str, color);
        x += 8;
        str++;
    }
}

// Localized background drawing (wood planks)
void draw_background_rect(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; y++) {
        unsigned char base_color = 95;
        if ((y / 20) % 2 == 0) base_color = 96;
        for (int x = x1; x <= x2; x++) {
            unsigned char col = base_color;
            if ((x + y * 3) % 19 == 0) col = base_color - 1;
            if ((x - y * 2) % 29 == 0) col = base_color + 1;
            if (y % 20 == 0) col = 98; // Wood partition
            put_pixel(x, y, col);
        }
    }
}

// Draw entire wood planks background
void draw_wood_background() {
    draw_background_rect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
}

// Localized tile redraw
void redraw_tile(int tile_index, int highlight_color) {
    Position pos = path[tile_index];
    int x1 = pos.x - 25;
    int y1 = pos.y - 18;
    int x2 = pos.x + 25;
    int y2 = pos.y + 18;

    // 1. Redraw background in the tile bounds
    draw_background_rect(x1 - 1, y1 - 1, x2 + 1, y2 + 1);

    // 2. Draw stone slab
    // Tiles 20 and 40 are visual Death Spaces
    if (tile_index == 20 || tile_index == 40) {
        draw_rect(x1, y1, x2, y2, 0, 1);  // Black background
        draw_rect(x1 + 1, y1 + 1, x2 - 1, y2 - 1, 4, 0); // Dark red inner outline
    } else if (tile_index == 10) {
        draw_rect(x1, y1, x2, y2, 4, 1);  // Dark Red background (Guerra)
        draw_rect(x1 + 1, y1 + 1, x2 - 1, y2 - 1, 15, 0); // White outline
    } else if (tile_index == 30) {
        draw_rect(x1, y1, x2, y2, 2, 1);  // Dark Green background (Pandemia)
        draw_rect(x1 + 1, y1 + 1, x2 - 1, y2 - 1, 15, 0); // White outline
    } else if (tile_index == 50) {
        draw_rect(x1, y1, x2, y2, 13, 1); // Purple background (Invasão Alien)
        draw_rect(x1 + 1, y1 + 1, x2 - 1, y2 - 1, 15, 0); // White outline
    } else {
        draw_rect(x1, y1, x2, y2, 7, 1);  // Light gray fill
        draw_rect(x1 + 1, y1 + 1, x2 - 1, y2 - 1, 8, 0); // Dark gray shadow
    }

    // Border (highlighted or normal)
    unsigned char border_col = (highlight_color != 0) ? highlight_color : 0;
    draw_rect(x1, y1, x2, y2, border_col, 0);

    // Label text
    char label[8];
    unsigned char text_color = 0; // Black text by default
    
    if (tile_index == 0) {
        strcpy(label, "INICIO");
        text_color = 10; // Light green
    } else if (tile_index == TOTAL_TILES - 1) {
        strcpy(label, "FIM");
        text_color = 12; // Light red
    } else if (tile_index == 20 || tile_index == 40) {
        strcpy(label, "MORTE");
        text_color = 40; // Bright red text for death spaces
    } else if (tile_index == 10) {
        strcpy(label, "GUERRA");
        text_color = 15; // White
    } else if (tile_index == 30) {
        strcpy(label, "PANDEM");
        text_color = 15; // White
    } else if (tile_index == 50) {
        strcpy(label, "ALIEN");
        text_color = 15; // White
    } else {
        itoa(tile_index, label, 10);
    }

    int txt_len = strlen(label);
    int txt_x = pos.x - (txt_len * 8) / 2;
    int txt_y = pos.y - 4;
    draw_string(txt_x, txt_y, label, text_color);

    // 3. Draw player tokens currently on this tile
    int on_tile[TOTAL_PLAYERS];
    int count = 0;
    for (int p = 0; p < TOTAL_PLAYERS; p++) {
        if (p_pos[p] == tile_index) {
            on_tile[count] = p;
            count++;
        }
    }

    // Grid coordinates offset within the tile for up to 4 tokens
    int ox[4] = { -12, 12, -12, 12 };
    int oy[4] = { -8, -8, 8, 8 };
    unsigned char token_colors[4] = { 40, 54, 47, 14 }; // Red, Blue, Green, Yellow

    if (count == 1) {
        // Draw centered token
        int px = pos.x;
        int py = pos.y;
        int p_idx = on_tile[0];
        
        // If dead, token color is black with a gray outline
        unsigned char col = p_dead[p_idx] ? 0 : token_colors[p_idx];
        draw_circle(px, py, 6, col, 1);
        draw_circle(px, py, 6, p_dead[p_idx] ? 8 : 15, 0); // Gray border if dead, else white
        if (!p_dead[p_idx]) put_pixel(px - 1, py - 1, 15);
    } else {
        // Draw split tokens in 2x2 grid
        for (int i = 0; i < count; i++) {
            int p_idx = on_tile[i];
            int px = pos.x + ox[i];
            int py = pos.y + oy[i];
            
            unsigned char col = p_dead[p_idx] ? 0 : token_colors[p_idx];
            draw_circle(px, py, 6, col, 1);
            draw_circle(px, py, 6, p_dead[p_idx] ? 8 : 15, 0);
            if (!p_dead[p_idx]) put_pixel(px - 1, py - 1, 15);
        }
    }
}

// Localized Crystal Ball Redraw
void redraw_crystal_ball(const char* l1, const char* l2, const char* l3, const char* l4, const char* l5, unsigned char text_color) {
    int xc = 400;
    int yc = 300;
    int r = 100;

    // Draw sphere shaders
    draw_circle(xc, yc, r - 3, 2, 1);   // Green outer
    draw_circle(xc, yc, r - 25, 23, 1); // Dark green inner
    draw_circle(xc, yc, r - 55, 0, 1);  // Black center core

    // Frames
    draw_circle(xc, yc, r - 1, 44, 0);
    draw_circle(xc, yc, r, 14, 0);
    draw_circle(xc, yc, r + 1, 8, 0);

    // Glass shine
    put_pixel(xc - 50, yc - 50, 15);
    put_pixel(xc - 49, yc - 49, 15);
    put_pixel(xc - 48, yc - 50, 15);
    put_pixel(xc - 50, yc - 48, 15);

    // Lines formatting
    const char* lines[5] = { l1, l2, l3, l4, l5 };
    int line_y[5] = { yc - 40, yc - 20, yc, yc + 20, yc + 40 };

    for (int i = 0; i < 5; i++) {
        if (lines[i] && strlen(lines[i]) > 0) {
            int lx = xc - (strlen(lines[i]) * 8) / 2;
            draw_string(lx, line_y[i], lines[i], text_color);
        }
    }
}

// Redraw only text inside the crystal ball using fast scanline circles to clear previous text
void redraw_crystal_ball_text(const char* l1, const char* l2, const char* l3, const char* l4, const char* l5, unsigned char text_color) {
    int xc = 400;
    int yc = 300;

    draw_circle(xc, yc, 70, 23, 1); // Dark green inner sphere
    draw_circle(xc, yc, 45, 0, 1);  // Black center core

    const char* lines[5] = { l1, l2, l3, l4, l5 };
    int line_y[5] = { yc - 40, yc - 20, yc, yc + 20, yc + 40 };

    for (int i = 0; i < 5; i++) {
        if (lines[i] && strlen(lines[i]) > 0) {
            int lx = xc - (strlen(lines[i]) * 8) / 2;
            draw_string(lx, line_y[i], lines[i], text_color);
        }
    }
}

// Sound effects wrapper
void play_sound(int type) {
    int f;
    switch (type) {
        case 0:
            sound(100 + rand() % 800);
            delay(15);
            nosound();
            break;
        case 1:
            sound(1200);
            delay(25);
            nosound();
            break;
        case 2:
            for (f = 300; f < 1300; f += 80) {
                sound(f);
                delay(12);
            }
            nosound();
            break;
        case 3:
            for (f = 1000; f > 150; f -= 60) {
                sound(f);
                delay(15);
            }
            nosound();
            break;
        case 4:
            sound(523); delay(150);
            sound(659); delay(150);
            sound(784); delay(150);
            sound(1046); delay(400);
            nosound();
            break;
    }
}

// Localized Dice Redraw
void redraw_dice(int d1, int d2, int rolling) {
    int x1 = 680, y1 = 12;
    int x2 = 720, y2 = 12;
    int size = 24;

    draw_background_rect(x1 - 5, y1 - 5, x2 + size + 5, y1 + size + 5);

    draw_rect(x1, y1, x1 + size, y1 + size, 15, 1);
    draw_rect(x1, y1, x1 + size, y1 + size, 0, 0);

    draw_rect(x2, y2, x2 + size, y2 + size, 15, 1);
    draw_rect(x2, y2, x2 + size, y2 + size, 0, 0);

    #define SV_DOT(dx, dy, ox) draw_circle(ox + dx, y1 + dy, 2, 0, 1)

    if (!rolling && d1 > 0) {
        int ox = x1;
        if (d1 == 1) { SV_DOT(12, 12, ox); }
        else if (d1 == 2) { SV_DOT(6, 6, ox); SV_DOT(18, 18, ox); }
        else if (d1 == 3) { SV_DOT(6, 6, ox); SV_DOT(12, 12, ox); SV_DOT(18, 18, ox); }
        else if (d1 == 4) { SV_DOT(6, 6, ox); SV_DOT(18, 6, ox); SV_DOT(6, 18, ox); SV_DOT(18, 18, ox); }
        else if (d1 == 5) { SV_DOT(6, 6, ox); SV_DOT(18, 6, ox); SV_DOT(12, 12, ox); SV_DOT(6, 18, ox); SV_DOT(18, 18, ox); }
        else if (d1 == 6) { SV_DOT(6, 6, ox); SV_DOT(18, 6, ox); SV_DOT(6, 12, ox); SV_DOT(18, 12, ox); SV_DOT(6, 18, ox); SV_DOT(18, 18, ox); }
    }

    if (!rolling && d2 > 0) {
        int ox = x2;
        if (d2 == 1) { SV_DOT(12, 12, ox); }
        else if (d2 == 2) { SV_DOT(6, 6, ox); SV_DOT(18, 18, ox); }
        else if (d2 == 3) { SV_DOT(6, 6, ox); SV_DOT(12, 12, ox); SV_DOT(18, 18, ox); }
        else if (d2 == 4) { SV_DOT(6, 6, ox); SV_DOT(18, 6, ox); SV_DOT(6, 18, ox); SV_DOT(18, 18, ox); }
        else if (d2 == 5) { SV_DOT(6, 6, ox); SV_DOT(18, 6, ox); SV_DOT(12, 12, ox); SV_DOT(6, 18, ox); SV_DOT(18, 18, ox); }
        else if (d2 == 6) { SV_DOT(6, 6, ox); SV_DOT(18, 6, ox); SV_DOT(6, 12, ox); SV_DOT(18, 12, ox); SV_DOT(6, 18, ox); SV_DOT(18, 18, ox); }
    }
}

// Localized HUD Box Redraw
void draw_hud() {
    int x1 = 10, y1 = 8;
    int x2 = 220, y2 = 100;
    
    // Fill HUD background
    draw_rect(x1, y1, x2, y2, 0, 1);
    draw_rect(x1, y1, x2, y2, 15, 0);

    char val[8];
    
    // Draw status lines. Display MORTO if dead.
    // Red row
    if (p_dead[0]) {
        draw_string(15, 15, "P1 (VERM): MORTO", 8); // Dark gray text
    } else {
        draw_string(15, 15, "P1 (VERM):", 40);
        itoa(p_pos[0], val, 10);
        draw_string(110, 15, val, 15);
        if (turn_order[current_turn_idx] == 0) draw_string(155, 15, "<--", 15);
    }

    // Blue row
    if (p_dead[1]) {
        draw_string(15, 35, "P2 (AZUL): MORTO", 8);
    } else {
        draw_string(15, 35, "P2 (AZUL):", 54);
        itoa(p_pos[1], val, 10);
        draw_string(110, 35, val, 15);
        if (turn_order[current_turn_idx] == 1) draw_string(155, 35, "<--", 15);
    }

    // Green row
    if (p_dead[2]) {
        draw_string(15, 55, "P3 (VERD): MORTO", 8);
    } else {
        draw_string(15, 55, "P3 (VERD):", 47);
        itoa(p_pos[2], val, 10);
        draw_string(110, 55, val, 15);
        if (turn_order[current_turn_idx] == 2) draw_string(155, 55, "<--", 15);
    }

    // Yellow row
    if (p_dead[3]) {
        draw_string(15, 75, "P4 (AMAR): MORTO", 8);
    } else {
        draw_string(15, 75, "P4 (AMAR):", 14);
        itoa(p_pos[3], val, 10);
        draw_string(110, 75, val, 15);
        if (turn_order[current_turn_idx] == 3) draw_string(155, 75, "<--", 15);
    }
}

// Draw static board elements
void draw_static_board() {
    draw_wood_background();

    // Draw main panel borders
    draw_rect(240, 8, 550, 28, 0, 1);
    draw_rect(240, 8, 550, 28, 14, 0);
    draw_string(255, 13, "JUMANJI BOARD GAME (SVGA)", 10);

    // Draw footer instructions
    draw_rect(200, 572, 600, 592, 0, 1);
    draw_rect(200, 572, 600, 592, 15, 0);
    draw_string(215, 577, "ESPACO: Roda Dados  |  ESC: Sair do Jogo", 15);

    // Render all initial tiles
    for (int i = 0; i < TOTAL_TILES; i++) {
        redraw_tile(i, 0);
    }
}

// Animate moving a player step by step
void animate_movement(int player_idx, int steps) {
    int start = p_pos[player_idx];
    int target = start + steps;

    if (target < 0) target = 0;
    if (target >= TOTAL_TILES) target = TOTAL_TILES - 1;

    int increment = (steps > 0) ? 1 : -1;
    unsigned char p_colors[4] = { 40, 54, 47, 14 };

    int current = start;
    while (current != target) {
        int old = current;
        current += increment;

        // Apply new pos
        p_pos[player_idx] = current;

        // Local redrawing
        redraw_tile(old, 0);
        redraw_tile(current, p_colors[player_idx]); // Blink new tile

        play_sound(1);
        delay(220);

        // Clear highlight
        redraw_tile(current, 0);
    }
}

// Check and apply events for special tiles (10: Guerra, 30: Pandemia, 50: Alien)
void check_tile_events(int active_player) {
    int current_pos = p_pos[active_player];
    char c_l1[20], c_l2[20], c_l3[20], c_l4[20], c_l5[20];
    unsigned char c_color;

    if (current_pos == 10) {
        // Guerra
        strcpy(c_l1, "GUERRA MUNDIAL!");
        strcpy(c_l2, "O MUNDO ESTA EM GUERRA");
        strcpy(c_l3, "TODOS OS JOGADORES");
        strcpy(c_l4, "RECUAM 3 CASAS!");
        strcpy(c_l5, "PRESSIONE ENTER");
        c_color = 12; // Red

        play_sound(3); // Disaster sweep
        redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, c_color);

        while (1) {
            char k = getch();
            if (k == 13 || k == ' ') break;
        }

        // Apply back movement to all alive players
        for (int p = 0; p < TOTAL_PLAYERS; p++) {
            if (!p_dead[p] && p_pos[p] > 0) {
                animate_movement(p, -3);
            }
        }
    } 
    else if (current_pos == 30) {
        // Pandemia
        strcpy(c_l1, "PANDEMIA GLOBAL!");
        strcpy(c_l2, "MUNDO EM QUARENTENA");
        strcpy(c_l3, "TODOS OS JOGADORES");
        strcpy(c_l4, "PERDEM O TURNO!");
        strcpy(c_l5, "PRESSIONE ENTER");
        c_color = 10; // Light green

        play_sound(3);
        redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, c_color);

        while (1) {
            char k = getch();
            if (k == 13 || k == ' ') break;
        }

        // Set skip turn for all alive players
        for (int p = 0; p < TOTAL_PLAYERS; p++) {
            if (!p_dead[p]) {
                p_skip[p] = 1;
            }
        }
    } 
    else if (current_pos == 50) {
        // Invasão Alien
        strcpy(c_l1, "INVASAO ALIEN!");
        strcpy(c_l2, "VOCE FOI ABDUZIDO!");
        strcpy(c_l3, "RECUARA 10 CASAS");
        strcpy(c_l4, "PARA RETORNO.");
        strcpy(c_l5, "PRESSIONE ENTER");
        c_color = 13; // Light magenta

        play_sound(3);
        redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, c_color);

        while (1) {
            char k = getch();
            if (k == 13 || k == ' ') break;
        }

        // Move active player back by 10
        animate_movement(active_player, -10);
    }
}

// Check if any player has landed on a Death space and flag them
void check_all_deaths() {
    char c_l1[20], c_l2[20], c_l3[20], c_l4[20], c_l5[20];
    int p;
    for (p = 0; p < TOTAL_PLAYERS; p++) {
        if (!p_dead[p] && (p_pos[p] == 20 || p_pos[p] == 40)) {
            p_dead[p] = 1;
            last_dead_player = p;
            redraw_tile(p_pos[p], 0);
            
            char dead_str[16];
            itoa(p + 1, dead_str, 10);
            strcpy(c_l1, "MORTE!");
            strcpy(c_l2, "JOGADOR ");
            strcat(c_l2, dead_str);
            strcpy(c_l3, "MORREU NA CASA ");
            char pos_str[8];
            itoa(p_pos[p], pos_str, 10);
            strcat(c_l3, pos_str);
            strcpy(c_l4, "FICARA PRETO E");
            strcpy(c_l5, "FORA DO JOGO!");
            
            redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, 12);
            play_sound(3); // Sad sweep
            delay(3000);
        }
    }
}

// Check if all players are dead to trigger defeat GameOver screen
int check_defeat_game_over() {
    int alive_count = 0;
    char c_l1[20], c_l2[20], c_l3[20], c_l4[20], c_l5[20];
    int p;
    for (p = 0; p < TOTAL_PLAYERS; p++) {
        if (!p_dead[p]) alive_count++;
    }
    if (alive_count == 0) {
        strcpy(c_l1, "FIM DE JOGO!");
        strcpy(c_l2, "TODOS OS JOGADORES");
        strcpy(c_l3, "MORRERAM NA SELVA");
        strcpy(c_l4, "JUMANJI VENCEU!");
        strcpy(c_l5, "APERTEM ESC");
        while (1) {
            redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, 12);
            delay(200);
            if (kbhit()) {
                char k = getch();
                if (k == 27 || k == 13 || k == ' ') break;
            }
        }
        return 1;
    }
    return 0;
}

// Handle game win: play sound, log winner to INFO.LOG, show victory screen
void handle_win_game(int active_player) {
    play_sound(4); // Fanfare
    char winner[16];
    itoa(active_player + 1, winner, 10);
    
    char c_l1[20], c_l2[20], c_l3[20], c_l4[20], c_l5[20];
    strcpy(c_l1, "JUMANJI!");
    strcpy(c_l2, "JOGADOR ");
    strcat(c_l2, winner);
    strcpy(c_l3, "VENCEU O JOGO!");
    strcpy(c_l4, "");
    strcpy(c_l5, "");
    
    unsigned char win_colors[4] = { 40, 54, 47, 14 };
    const char* col_names[4] = { "VERMELHO", "AZUL", "VERDE", "AMARELO" };

    // Log the winner to INFO.LOG (overwrite mode 'w')
    FILE* f = fopen("INFO.LOG", "w");
    if (f != NULL) {
        fprintf(f, "%s\n", col_names[active_player]);
        fclose(f);
    }

    while (1) {
        redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, win_colors[active_player]);
        delay(200);
        if (kbhit()) { getch(); break; }
    }
}

// Rock-Paper-Scissors Match resolution helper
int beats(int a, int b) {
    if (a == 0 && b == 2) return 1; // Pedra beats Tesoura
    if (a == 1 && b == 0) return 1; // Papel beats Pedra
    if (a == 2 && b == 1) return 1; // Tesoura beats Papel
    return 0;
}

// Draw visual RPS screen static base (wood background, borders, titles)
void draw_tournament_base() {
    int box_x1 = 80, box_y1 = 40;
    int box_x2 = 720, box_y2 = 560;

    // Redraw wood background in this box once
    draw_background_rect(box_x1 - 5, box_y1 - 5, box_x2 + 5, box_y2 + 5);

    // Draw main black panel
    draw_rect(box_x1, box_y1, box_x2, box_y2, 0, 1);
    draw_rect(box_x1, box_y1, box_x2, box_y2, 14, 0); // Gold frame

    // Text Header
    draw_string(280, 65, "JUMANJI - DECISAO DOS TURNOS", 14);
    draw_string(220, 85, "TORNEIO DE PEDRA, PAPEL E TESOURA DOS 4", 15);

    // Player blocks configuration
    int px[4] = { 120, 260, 400, 540 };
    int py = 160;
    int w = 110, h = 240;
    unsigned char colors[4] = { 40, 54, 47, 14 };
    const char* names[4] = { "VERMELHO (P1)", "AZUL (P2)", "VERDE (P3)", "AMARELO (P4)" };

    for (int p = 0; p < TOTAL_PLAYERS; p++) {
        int x1 = px[p];
        int y1 = py;
        int x2 = px[p] + w;
        int y2 = py + h;

        // Block outline
        draw_rect(x1, y1, x2, y2, 15, 0);

        // Player colored token representation
        draw_circle(x1 + 55, y1 + 30, 12, colors[p], 1);
        draw_circle(x1 + 55, y1 + 30, 12, 15, 0);

        // Player Name
        draw_string(x1 + 55 - (strlen(names[p]) * 8) / 2, y1 + 55, names[p], 15);
    }
}

// Update only the dynamic elements of the RPS screen in VRAM
void update_tournament_choices(int is_active[TOTAL_PLAYERS], int choices[TOTAL_PLAYERS], int finished[TOTAL_PLAYERS], const char* r_title, const char* r_status) {
    int px[4] = { 120, 260, 400, 540 };
    int py = 160;
    int w = 110, h = 240;
    int p, r;

    // 1. Redraw Round Title (clear with black first)
    draw_rect(100, 110, 700, 130, 0, 1);
    draw_string(400 - (strlen(r_title) * 8) / 2, 115, r_title, 10);

    // 2. Redraw Player blocks dynamic choices and status
    for (p = 0; p < TOTAL_PLAYERS; p++) {
        int x1 = px[p];
        int y1 = py;
        int x2 = px[p] + w;
        int y2 = py + h;

        // Clear and draw Player Status
        draw_rect(x1 + 5, y1 + 75, x2 - 5, y1 + 95, 0, 1);
        char status[12];
        unsigned char status_col = 15;
        if (finished[p]) {
            strcpy(status, "COLOCADO");
            status_col = 8; // Gray
        } else if (is_active[p]) {
            strcpy(status, "JOGANDO");
            status_col = 10; // Green
        } else {
            strcpy(status, "ESPERANDO");
            status_col = 8; // Gray
        }
        draw_string(x1 + 55 - (strlen(status) * 8) / 2, y1 + 80, status, status_col);

        // Clear and draw Choice display panel
        int cx1 = x1 + 15;
        int cy1 = y1 + 120;
        int cx2 = x2 - 15;
        int cy2 = y2 - 20;

        draw_rect(cx1 + 1, cy1 + 1, cx2 - 1, cy2 - 1, 15, 1); // White fill
        draw_rect(cx1, cy1, cx2, cy2, 0, 0);                  // Black border

        char choice_str[10];
        if (finished[p] || !is_active[p] || choices[p] == -1) {
            strcpy(choice_str, "-");
        } else {
            if (choices[p] == 0) strcpy(choice_str, "PEDRA");
            else if (choices[p] == 1) strcpy(choice_str, "PAPEL");
            else if (choices[p] == 2) strcpy(choice_str, "TESOURA");
        }
        draw_string(x1 + 55 - (strlen(choice_str) * 8) / 2, cy1 + 40, choice_str, 0);
    }

    // 3. Clear and draw Rank Summary Bar
    int rx = 100, ry = 430;
    draw_rect(rx + 1, ry + 1, 700 - 1, 480 - 1, 8, 1); // Gray fill bar
    draw_rect(rx, ry, 700, 480, 15, 0);

    char rank_bar[80] = "Ranks: ";
    const char* col_names[4] = { "Vermelho", "Azul", "Verde", "Amarelo" };

    for (r = 0; r < TOTAL_PLAYERS; r++) {
        char r_label[24];
        itoa(r + 1, r_label, 10);
        strcat(rank_bar, r_label);
        strcat(rank_bar, "o:[");
        if (turn_order[r] == -1) {
            strcat(rank_bar, "?");
        } else {
            strcat(rank_bar, col_names[turn_order[r]]);
        }
        strcat(rank_bar, "] ");
        if (r < TOTAL_PLAYERS - 1) strcat(rank_bar, "| ");
    }
    draw_string(rx + 20, ry + 18, rank_bar, 15);

    // 4. Clear and draw Prompt lines
    draw_rect(100, 500, 700, 525, 0, 1);
    draw_string(400 - (strlen(r_status) * 8) / 2, 510, r_status, 14); // Gold color
    draw_string(400 - (strlen("PRESSIONE ENTER PARA CONTINUAR") * 8) / 2, 535, "PRESSIONE ENTER PARA CONTINUAR", 15);
}

// 1v1 Match resolution helper
int play_1v1_match(int p1, int p2, const char* r_title, int finished[TOTAL_PLAYERS]) {
    int is_active[TOTAL_PLAYERS] = {0, 0, 0, 0};
    int choices[TOTAL_PLAYERS] = {-1, -1, -1, -1};
    int p, cycle;

    // Only p1 and p2 are active in this match
    is_active[p1] = 1;
    is_active[p2] = 1;

    char status_buf[60];
    const char* col_names[4] = { "VERMELHO", "AZUL", "VERDE", "AMARELO" };

    int winner = -1;
    while (winner == -1) {
        // Cycling animation
        for (cycle = 0; cycle < 8; cycle++) {
            choices[p1] = rand() % 3;
            choices[p2] = rand() % 3;
            update_tournament_choices(is_active, choices, finished, r_title, "DISPUTANDO JOGADAS...");
            play_sound(0);
            delay(90);
        }

        // Lock in choices
        choices[p1] = rand() % 3;
        choices[p2] = rand() % 3;

        if (choices[p1] == choices[p2]) {
            // Tie
            strcpy(status_buf, "EMPATE! JOGAM DE NOVO");
            update_tournament_choices(is_active, choices, finished, r_title, status_buf);
            play_sound(3); // Tie beep
            while (1) {
                char k = getch();
                if (k == 13 || k == ' ') break;
            }
        } else {
            // Determine winner
            if (beats(choices[p1], choices[p2])) {
                winner = p1;
            } else {
                winner = p2;
            }

            strcpy(status_buf, col_names[winner]);
            strcat(status_buf, " VENCEU O CONFRONTO!");
            
            update_tournament_choices(is_active, choices, finished, r_title, status_buf);
            play_sound(2); // Victory sweep
            while (1) {
                char k = getch();
                if (k == 13 || k == ' ') break;
            }
        }
    }
    return winner;
}

// Rock-Paper-Scissors Tournament Manager (1v1 Bracket)
void play_rps_tournament() {
    int finished[TOTAL_PLAYERS] = {0, 0, 0, 0};
    int r;

    // Reset turn order ranks
    for (r = 0; r < TOTAL_PLAYERS; r++) {
        turn_order[r] = -1;
    }

    draw_tournament_base();

    // Match 1: Semifinal 1 (P0 vs P1 - Red vs Blue)
    int w1 = play_1v1_match(0, 1, "SEMIFINAL 1: VERMELHO VS AZUL", finished);
    int l1 = (w1 == 0) ? 1 : 0;

    // Match 2: Semifinal 2 (P2 vs P3 - Green vs Yellow)
    int w2 = play_1v1_match(2, 3, "SEMIFINAL 2: VERDE VS AMARELO", finished);
    int l2 = (w2 == 2) ? 3 : 2;

    // Match 3: 3rd Place Match (L1 vs L2)
    int w3 = play_1v1_match(l1, l2, "DISPUTA DE 3o E 4o LUGAR", finished);
    int l3 = (w3 == l1) ? l2 : l1;

    // Save 3rd and 4th place ranks
    turn_order[2] = w3;
    finished[w3] = 1;
    
    turn_order[3] = l3;
    finished[l3] = 1;

    // Match 4: Grand Finals (W1 vs W2)
    int w4 = play_1v1_match(w1, w2, "GRANDE FINAL: DECIDINDO 1o E 2o", finished);
    int l4 = (w4 == w1) ? w2 : w1;

    // Save 1st and 2nd place ranks
    turn_order[0] = w4;
    finished[w4] = 1;

    turn_order[1] = l4;
    finished[l4] = 1;

    // Show final bracket rank screen
    int is_active[TOTAL_PLAYERS] = {0, 0, 0, 0};
    int choices[TOTAL_PLAYERS] = {-1, -1, -1, -1};
    update_tournament_choices(is_active, choices, finished, "TORNEIO CONCLUIDO!", "ORDEM DEFINIDA! JUMANJI AGUARDA...");
    play_sound(4); // Fanfare
    while (1) {
        char k = getch();
        if (k == 13 || k == ' ') break;
    }
}

int main() {
    srand(time(NULL));

    init_font();
    generate_path();
    init_vesa();

    // 1. Play Rock-Paper-Scissors tournament to resolve player order
    play_rps_tournament();

    // 2. Initialize VRAM and draw main board
    draw_static_board();
    draw_hud();
    redraw_dice(0, 0, 0);

    // Welcome State
    char c_l1[20] = "JUMANJI";
    char c_l2[20] = "ROLE DADOS COM";
    char c_l3[20] = "ESPACO PARA JOGAR";
    char c_l4[20] = "EVITE AS CASAS";
    char c_l5[20] = "20 E 40 (MORTE)";
    unsigned char c_color = 14;

    // Draw full crystal ball once at start
    redraw_crystal_ball(c_l1, c_l2, c_l3, c_l4, c_l5, c_color);

    while (1) {
        redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, c_color);

        if (kbhit()) {
            char key = getch();
            if (key == 27) { // ESC exit
                break;
            }
            if (key == ' ') { // Roll turn
                int active_player = turn_order[current_turn_idx];

                // Check if current player is dead. If so, skip immediately.
                if (p_dead[active_player]) {
                    current_turn_idx = (current_turn_idx + 1) % TOTAL_PLAYERS;
                    draw_hud();
                    continue;
                }

                // Check if current player needs to skip turn
                if (p_skip[active_player] > 0) {
                    p_skip[active_player] = 0;
                    
                    char turn_str[16];
                    itoa(active_player + 1, turn_str, 10);
                    strcpy(c_l1, "JOGADOR ");
                    strcat(c_l1, turn_str);
                    strcpy(c_l2, "PERDEU O TURNO");
                    strcpy(c_l3, "");
                    strcpy(c_l4, "");
                    strcpy(c_l5, "");
                    c_color = 12;

                    play_sound(3);
                    redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, c_color);
                    delay(1500);

                    // Pass turn
                    current_turn_idx = (current_turn_idx + 1) % TOTAL_PLAYERS;
                    draw_hud();
                    continue;
                }

                // Animate rolling
                int d1 = 0, d2 = 0;
                for (int r = 0; r < 8; r++) {
                    d1 = 1 + rand() % 6;
                    d2 = 1 + rand() % 6;
                    redraw_dice(d1, d2, 1);
                    play_sound(0);
                    delay(80);
                }

                // Lock roll result
                d1 = 1 + rand() % 6;
                d2 = 1 + rand() % 6;
                redraw_dice(d1, d2, 0);

                int steps = d1 + d2;

                // Move current player
                animate_movement(active_player, steps);

                // Run check_tile_events and check_all_deaths/defeat
                check_tile_events(active_player);
                check_all_deaths();
                if (check_defeat_game_over()) {
                    break;
                }

                if (p_dead[active_player]) {
                    // Pass turn to next player
                    current_turn_idx = (current_turn_idx + 1) % TOTAL_PLAYERS;
                    draw_hud();
                    continue;
                }

                // Win check
                if (p_pos[active_player] >= TOTAL_TILES - 1) {
                    handle_win_game(active_player);
                    break;
                }

                // Draw card
                int idx = rand() % 10;
                EventCard card = deck[idx];

                if (idx == 9) {
                    // Portal Retorno resurrection event card
                    if (last_dead_player != -1 && p_dead[last_dead_player]) {
                        p_dead[last_dead_player] = 0; // Revive last dead player!
                        
                        const char* col_names[4] = { "VERMELHO", "AZUL", "VERDE", "AMARELO" };
                        strcpy(c_l1, "PORTAL RETORNO!");
                        strcpy(c_l2, "JOGADOR ");
                        strcat(c_l2, col_names[last_dead_player]);
                        strcpy(c_l3, "RESSUSCITOU!");
                        strcpy(c_l4, "MAIS UMA CHANCE!");
                        strcpy(c_l5, "PRESSIONE ENTER");
                        c_color = 10; // Green
                        
                        play_sound(2); // Mystical success sound
                        redraw_tile(p_pos[last_dead_player], 0); // Restore original token color on board
                    } else {
                        strcpy(c_l1, "PORTAL RETORNO!");
                        strcpy(c_l2, "NENHUM JOGADOR");
                        strcpy(c_l3, "MORTO PARA");
                        strcpy(c_l4, "RESSUSCITAR!");
                        strcpy(c_l5, "PRESSIONE ENTER");
                        c_color = 14; // Gold
                        play_sound(3); // Soft warning beep
                    }

                    redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, c_color);

                    // Wait for ENTER
                    while (1) {
                        char k = getch();
                        if (k == 13 || k == ' ') break;
                        if (k == 27) {
                            close_vesa();
                            return 0;
                        }
                    }

                    // Move to next player's turn normally
                    current_turn_idx = (current_turn_idx + 1) % TOTAL_PLAYERS;
                    draw_hud();
                    continue;
                }

                if (card.move_offset >= 0 && card.skip_turn == 0) {
                    play_sound(2); // Success sweep
                    c_color = 10;  // Green
                } else {
                    play_sound(3); // Disaster sweep
                    c_color = 12;  // Red
                }

                strcpy(c_l1, card.title);
                strcpy(c_l2, card.line1);
                strcpy(c_l3, card.line2);
                strcpy(c_l4, "PRESSIONE ENTER");
                strcpy(c_l5, "");
                
                redraw_crystal_ball_text(c_l1, c_l2, c_l3, c_l4, c_l5, c_color);

                // Wait for ENTER
                while (1) {
                    char k = getch();
                    if (k == 13 || k == ' ') break;
                    if (k == 27) {
                        close_vesa();
                        return 0;
                    }
                }

                // Apply movement card effect
                if (card.move_offset != 0) {
                    animate_movement(active_player, card.move_offset);

                    // Run tile events check and death checks for card movement
                    check_tile_events(active_player);
                    check_all_deaths();
                    if (check_defeat_game_over()) {
                        break;
                    }

                    if (p_dead[active_player]) {
                        current_turn_idx = (current_turn_idx + 1) % TOTAL_PLAYERS;
                        draw_hud();
                        continue;
                    }
                }

                // Double Win Check
                if (p_pos[active_player] >= TOTAL_TILES - 1) {
                    handle_win_game(active_player);
                    break;
                }

                // Apply skip turn card effect
                if (card.skip_turn > 0) {
                    p_skip[active_player] = 1;
                }

                // Determine next turn
                if (card.extra_turn > 0) {
                    strcpy(c_l1, "CARTA MISTICA!");
                    strcpy(c_l2, "JOGUE DE NOVO");
                    strcpy(c_l3, "");
                    strcpy(c_l4, "");
                    strcpy(c_l5, "");
                    c_color = 14;
                } else {
                    current_turn_idx = (current_turn_idx + 1) % TOTAL_PLAYERS;
                    strcpy(c_l1, "PROXIMO JOGADOR");
                    strcpy(c_l2, "ROLE OS DADOS");
                    strcpy(c_l3, "");
                    strcpy(c_l4, "");
                    strcpy(c_l5, "");
                    c_color = 15;
                }
                
                // Redraw HUD
                draw_hud();
            }
        }
    }

    close_vesa();
    cout << "Obrigado por jogar Jumanji SVGA!\n";
    return 0;
}
