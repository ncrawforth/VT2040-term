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

#define TERM_KC_UP 0xE000
#define TERM_KC_DOWN 0xE001
#define TERM_KC_RIGHT 0xE002
#define TERM_KC_LEFT 0xE003
#define TERM_KC_PGUP 0xE004
#define TERM_KC_PGDN 0xE005
#define TERM_KC_HOME 0xE006
#define TERM_KC_END 0xE007

extern char* (*term_putc)(char);
void term_init();
char* term_keypress(int32_t keycode);
