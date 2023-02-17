#define TERM_WIDTH 80
#define TERM_HEIGHT 24

extern char term_chars[TERM_WIDTH * TERM_HEIGHT];
extern char term_attrs[TERM_WIDTH * TERM_HEIGHT];
extern volatile int term_cursor_x;
extern volatile int term_cursor_y;
extern volatile bool term_cursor_visible;

#define term_attr_fgcolor(a) ((a) & 0x0f)
#define term_attr_bgcolor(a) (((a) >> 4) & 0x07)
#define term_attr_underline(a) ((a) >> 7)

#define TERM_KC_UP 256
#define TERM_KC_DOWN 257
#define TERM_KC_RIGHT 258
#define TERM_KC_LEFT 259
#define TERM_KC_PGUP 260
#define TERM_KC_PGDN 261
#define TERM_KC_HOME 262
#define TERM_KC_END 263
#define TERM_KC_POUND_SIGN 163
#define TERM_KC_DEGREE_SIGN 176
#define TERM_KC_LATIN_SMALL_LETTER_AE 230

extern char* (*term_putc)(char);
void term_init();
char* term_keypress(int keycode);
