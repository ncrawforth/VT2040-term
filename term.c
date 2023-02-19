#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "term.h"
#include "unicodetocodepage.h"

bool g0_graphics; 
bool g1_graphics;
bool mode_shiftout;
bool mode_appkeypad;
bool mode_insert;
bool mode_origin;
bool mode_crlf;

int scroll_top;
int scroll_bottom;

int tabs[TERM_WIDTH];

int attr_fgcolor;
int attr_bgcolor;
bool attr_bright;
bool attr_underline;
bool attr_reverse;

int saved_x;
int saved_y;
int saved_fgcolor;
int saved_bgcolor;
bool saved_bright;
bool saved_underline;
bool saved_reverse;

int params[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int paramc = 0;
int32_t unicode;
char temp[32];

char term_chars[TERM_WIDTH * TERM_HEIGHT];
char term_attrs[TERM_WIDTH * TERM_HEIGHT];
volatile int term_cursor_x;
volatile int term_cursor_y;
volatile bool term_cursor_visible;

int min(int a, int b) {return (a > b) ? b : a;}
int max(int a, int b) {return (a > b) ? a : b;}

char* putc_normal(char c);
char* putc_escape(char c);
char* putc_bracket(char c);
char* putc_setg0(char c);
char* putc_setg1(char c);
char* putc_ignore(char c);
char* putc_unicode1(char c);
char* putc_unicode2(char c);
char* putc_unicode3(char c);

char* (*term_putc)(char) = putc_normal;

void clear_screen() {
  int len = TERM_WIDTH * TERM_HEIGHT;
  memset(term_chars, unicodetocodepage(32, false), len);
  memset(term_attrs, 0, len);
}

void clear_chars(int n) {
  if (term_cursor_x >= TERM_WIDTH) return;
  int dst = term_cursor_y * TERM_WIDTH + term_cursor_x;
  int len = min(n, TERM_WIDTH - term_cursor_x);
  memset(term_chars + dst, unicodetocodepage(32, false), len);
  memset(term_attrs + dst, 0, len);
}

void clear_end_of_line() {
  if (term_cursor_x >= TERM_WIDTH) return;
  int dst = term_cursor_y * TERM_WIDTH + term_cursor_x;
  int len = TERM_WIDTH - term_cursor_x;
  memset(term_chars + dst, unicodetocodepage(32, false), len);
  memset(term_attrs + dst, 0, len);
}

void clear_start_of_line() {
  int dst = term_cursor_y * TERM_WIDTH;
  int len = min(TERM_WIDTH, term_cursor_x + 1);
  memset(term_chars + dst, unicodetocodepage(32, false), len);
  memset(term_attrs + dst, 0, len);
}

void clear_line() {
  int dst = term_cursor_y * TERM_WIDTH;
  memset(term_chars + dst, unicodetocodepage(32, false), TERM_WIDTH);
  memset(term_attrs + dst, 0, TERM_WIDTH);
}

void clear_end_of_screen() {
  int dst = term_cursor_y * TERM_WIDTH + min(TERM_WIDTH, term_cursor_x);
  int len = (TERM_WIDTH * TERM_HEIGHT) - dst;
  memset(term_chars + dst, unicodetocodepage(32, false), len);
  memset(term_attrs + dst, 0, len);
}

void clear_start_of_screen() {
  int len = term_cursor_y * TERM_WIDTH + min(TERM_WIDTH, term_cursor_x + 1);
  memset(term_chars, unicodetocodepage(32, false), len);
  memset(term_attrs, 0, len);
}

void delete_chars(int n) {
  if (term_cursor_x >= TERM_WIDTH) return;
  n = min(n, TERM_WIDTH - term_cursor_x);
  int dst = term_cursor_y * TERM_WIDTH + term_cursor_x;
  int src = dst + n;
  int len = TERM_WIDTH - term_cursor_x - n;
  memmove(term_chars + dst, term_chars + src, len);
  memmove(term_attrs + dst, term_attrs + src, len);
  memset(term_chars + dst + len, unicodetocodepage(32, false), n);
  memset(term_attrs + dst + len, 0, n);
}

void insert_chars(int n) {
  if (term_cursor_x >= TERM_WIDTH) return;
  n = min(n, TERM_WIDTH - term_cursor_x);
  int src = term_cursor_y * TERM_WIDTH + term_cursor_x;
  int dst = src + n;
  int len = TERM_WIDTH - term_cursor_x - n;
  memmove(term_chars + dst, term_chars + src, len);
  memmove(term_attrs + dst, term_attrs + src, len);
  memset(term_chars + src, unicodetocodepage(32, false), n);
  memset(term_attrs + src, 0, n);
}

void scroll_up(int top, int n) {
  // FIXME: optimise this
  n = min(n, term_cursor_y + 1 - top);
  int dst = top * TERM_WIDTH;
  int src = dst + TERM_WIDTH;
  int len = (term_cursor_y - top) * TERM_WIDTH;
  for (int i = 0; i < n; i++) {
    memmove(term_chars + dst, term_chars + src, len);
    memmove(term_attrs + dst, term_attrs + src, len);
    clear_line();
  }
}

void scroll_down(int bottom, int n) {
  // FIXME: optimise this
  n = min(n, bottom + 1 - term_cursor_y);
  int src = term_cursor_y * TERM_WIDTH;
  int dst = src + TERM_WIDTH;
  int len = (bottom - term_cursor_y) * TERM_WIDTH;
  for (int i = 0; i < n; i++) {
    memmove(term_chars + dst, term_chars + src, len);
    memmove(term_attrs + dst, term_attrs + src, len);
    clear_line();
  }
}

void cursor_down() {
  if (term_cursor_y == scroll_bottom) {
    scroll_up(scroll_top, 1);
  } else if (term_cursor_y == TERM_HEIGHT - 1) {
    scroll_up(0, 1);
  } else {
    term_cursor_y++;
  }
}

void cursor_up() {
  if (term_cursor_y == scroll_top) {
    scroll_down(scroll_bottom, 1);
  } else if (term_cursor_y == 0) {
    scroll_down(TERM_HEIGHT - 1, 1);
  } else {
    term_cursor_y--;
  }
}

void print_char(char c) {
  if (mode_insert) insert_chars(1);
  int dst = term_cursor_y * TERM_WIDTH + term_cursor_x;
  int bgcolor = attr_reverse ? attr_fgcolor : attr_bgcolor;
  int fgcolor = attr_reverse ? attr_bgcolor : attr_fgcolor;
  if (attr_bright) fgcolor |= 8;
  term_chars[dst] = c;
  term_attrs[dst] = (attr_underline << 7) | (bgcolor << 4) | fgcolor;
  term_cursor_x++;
}

char* putc_normal(char c) {
  switch (c) {
    case 8: // Backspace
      if (term_cursor_x > 0) term_cursor_x--;
      break;
    case 9: // Tab
      while (term_cursor_x < TERM_WIDTH - 1) {
        term_cursor_x++;
        if (tabs[term_cursor_x]) break;
      }
      break;
    case 10: case 11: case 12: // Linefeed
      cursor_down();
      if (mode_crlf) term_cursor_x = 0;
      break;
    case 13: // Carriage return
      term_cursor_x = 0;
      break;
    case 14: // Lock shift G1
      mode_shiftout = true;
      break;
    case 15: // Lock shift G0
      mode_shiftout = false;
      break;
    case 24: case 26: // Cancel
      break;
    case 27: // Escape
      term_putc = putc_escape;
      break;
    default:
      if (c < 32) break;
      if (c >= 192 && c <= 223) {
        unicode = c & 31;
        term_putc = putc_unicode1;
        break;
      }
      if (c >= 224 && c <= 239) {
        unicode = c & 15;
        term_putc = putc_unicode2;
        break;
      }
      if (c >= 240 && c <= 247) {
        unicode = c & 7;
        term_putc = putc_unicode3;
        break;
      }
      if (term_cursor_x >= TERM_WIDTH) {
        cursor_down();
        term_cursor_x = 0;
      }
      print_char(unicodetocodepage(c, mode_shiftout ? g1_graphics : g0_graphics));
  }
  return "";
}

char* putc_escape(char c) {
  term_putc = putc_normal;
  paramc = 0;
  params[0] = 0;
  params[1] = 0;
  switch (c) {
    case 24: case 26: // Cancel
      break;
    case 27: // Escape
      term_putc = putc_escape;
      break;
    case '[': // Start of bracket escape sequence
      term_putc = putc_bracket;
      break;
    case '(': // Set G0 character set
      term_putc = putc_setg0;
      break;
    case ')': // Set G1 character set
      term_putc = putc_setg1;
      break;
    case '#': // Double height/width (not implemented)
      term_putc = putc_ignore;
      break;
    case 'D': // Cursor down
      cursor_down();
      break;
    case 'M': // Cursor up
      cursor_up();
      break;
    case 'E': // CR + NL
      cursor_down();
      term_cursor_x = 0;
      break;
    case '7': // Save attributes and cursor position
    case 's':
      saved_x = term_cursor_x;
      saved_y = term_cursor_y;
      saved_fgcolor = attr_fgcolor;
      saved_bgcolor = attr_bgcolor;
      saved_bright = attr_bright;
      saved_underline = attr_underline;
      saved_reverse = attr_reverse;
      break;
    case '8': // Restore attributes and cursor position
    case 'u':
      term_cursor_x = saved_x;
      term_cursor_y = saved_y;
      attr_fgcolor = saved_fgcolor;
      attr_bgcolor = saved_bgcolor;
      attr_bright = saved_bright;
      attr_underline = saved_underline;
      attr_reverse = saved_reverse;
      break;
    case '=': // Keypad into applications mode
      mode_appkeypad = true;
      break;
    case '>': // Keypad into numeric mode
      mode_appkeypad = false;
      break;
    case 'Z': // Report terminal type
      return "\033[?1;2c";
    case 'c': // Reset to initial state
      scroll_top = 0;
      scroll_bottom = TERM_HEIGHT - 1;
      memset(tabs, 0, sizeof(bool) * TERM_WIDTH);
      term_cursor_x = 0;
      term_cursor_y = 0;
      term_cursor_visible = true;
      clear_screen();
      g0_graphics = false;
      g1_graphics = false;
      mode_shiftout = false;
      mode_appkeypad = false;
      mode_insert = false;
      mode_origin = false;
      mode_crlf = false;
      attr_fgcolor = 7;
      attr_bgcolor = 0;
      attr_bright = false;
      attr_underline = false;
      attr_reverse = false;
      saved_x = 0;
      saved_y = 0;
      saved_fgcolor = 7;
      saved_bgcolor = 0;
      saved_bright = false;
      saved_underline = false;
      saved_reverse = false;
      break;
    case 'H': // Set tab at current position
      if (term_cursor_x < TERM_WIDTH) tabs[term_cursor_x] = true;
      break;
  }
  return "";
}

char* putc_bracket(char c) {
  term_putc = putc_normal;
  switch (c) {
    case 24: case 26: // Cancel
      break;
    case 27: // Escape
      term_putc = putc_escape;
      break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      term_putc = putc_bracket;
      if (paramc == 0) paramc = 1;
      params[paramc - 1] = (params[paramc - 1] * 10) + c - '0';
      break;
    case ';':
      if (paramc == 16) break; // Cancel sequence if it has more than 16 params
      term_putc = putc_bracket;
      if (paramc == 0) paramc = 1;
      paramc++;
      params[paramc - 1] = 0;
      break;
    case '?':
      term_putc = putc_bracket;
      break;
    case 'A': // Cursor up
      term_cursor_y = max(scroll_top, term_cursor_y - max(1, params[0]));
      break;
    case 'B': // Cursor down
      term_cursor_y = min(scroll_bottom, term_cursor_y + max(1, params[0]));
      break;
    case 'C': // Cursor right
      term_cursor_x = min(TERM_WIDTH - 1, term_cursor_x + max(1, params[0]));
      break;
    case 'D': // Cursor left
      term_cursor_x = max(0, term_cursor_x - max(1, params[0]));
      break;
    case 'X': // Clear characters
      clear_chars(max(1, params[0]));
      break;
    case 'K': // Clear line
      switch (params[0]) {
        case 0:
          clear_end_of_line();
          break;
        case 1:
          clear_start_of_line();
          break;
        case 2:
          clear_line();
          break;
      }
      break;
    case 'J': // Clear screen
      switch (params[0]) {
        case 0:
          clear_end_of_screen();
          break;
        case 1:
          clear_start_of_screen();
          break;
        case 2:
          clear_screen();
          break;
      }
      break;
    case 'n': // Device status report
      switch (params[0]) {
        case 5:
          return "\033[0n";
        case 6:
          sprintf(temp, "\033[%d;%dR", term_cursor_y + 1, term_cursor_x + 1);
          return temp;
      }
      break;
    case 'c': // Identify
      if (paramc == 0) {
        return "\033[?1;2c";
      }
      break;
    case 'x': // Request terminal parameters
      if (params[0] == 1) {
        return "\033[3;1;1;120;120;1;0x";
      }
      return "\033[2;1;1;120;120;1;0x";
    case 's': // Save attributes and cursor position
      saved_x = term_cursor_x;
      saved_y = term_cursor_y;
      saved_fgcolor = attr_fgcolor;
      saved_bgcolor = attr_bgcolor;
      saved_bright = attr_bright;
      saved_underline = attr_underline;
      saved_reverse = attr_reverse;
      break;
    case 'u': // Restore attributes and cursor position
      term_cursor_x = saved_x;
      term_cursor_y = saved_y;
      attr_fgcolor = saved_fgcolor;
      attr_bgcolor = saved_bgcolor;
      attr_bright = saved_bright;
      attr_underline = saved_underline;
      attr_reverse = saved_reverse;
      break;
    case 'h': // Set setting
      for (int i = 0; i < paramc; i++) {
        switch (params[i]) {
          case 1:
            mode_appkeypad = true;
            break;
          case 4:
            mode_insert = true;
            break;
          case 6:
            mode_origin = true;
            term_cursor_x = 0;
            term_cursor_y = scroll_top;
            break;
          case 20:
            mode_crlf = true;
            break;
          case 25:
            term_cursor_visible = true;
            break;
        }
      }
      break;
    case 'l': // Reset setting
      for (int i = 0; i < paramc; i++) {
        switch (params[i]) {
          case 1:
            mode_appkeypad = false;
            break;
          case 4:
            mode_insert = false;
            break;
          case 6:
            mode_origin = false;
            term_cursor_x = 0;
            term_cursor_y = 0;
            break;
          case 20:
            mode_crlf = false;
            break;
          case 25:
            term_cursor_visible = false;
            break;
        }
      }
      break;
    case 'H': // Set cursor position
    case 'f':
      if (mode_origin) {
        term_cursor_y = min(TERM_HEIGHT - 1, scroll_top + max(0, params[0] - 1));
      } else {
        term_cursor_y = min(TERM_HEIGHT - 1, max(0, params[0] - 1));
      }
      term_cursor_x = max(0, params[1] - 1);
      break;
    case 'G': // Cursor to column x
    case '`':
      term_cursor_x = max(0, params[0] - 1);
      break;
    case 'g': // Clear tab stops
      if (params[0] == 0) {
        if (term_cursor_x < TERM_WIDTH) tabs[term_cursor_x] = false;
      } else {
        memset(tabs, 0, sizeof(bool) * TERM_WIDTH);
      }
      break;
    case 'm': // Set attribute
      for (int i = 0; i < max(1, paramc); i++) {
        switch (params[i]) {
          case 0:
            attr_bgcolor = 0;
            attr_fgcolor = 7;
            attr_bright = false;
            attr_underline = false;
            attr_reverse = false;
            break;
          case 1:
            attr_bright = true;
            break;
          case 4:
            attr_underline = true;
            break;
          case 7:
            attr_reverse = true;
            break;
          case 22:
            attr_bright = false;
            break;
          case 24:
            attr_underline = false;
            break;
          case 27:
            attr_reverse = false;
            break;
          case 39:
            attr_fgcolor = 7;
            break;
          case 49:
            attr_bgcolor = 0;
            break;
          default:
            if (params[i] >= 30 && params[i] <= 37) {
              attr_fgcolor = params[i] - 30;
            } else if (params[i] >= 40 && params[i] <= 47) {
              attr_bgcolor = params[i] - 40;
            }
        }
      }
      break;
    case 'L': // Insert lines
      if (term_cursor_y < scroll_bottom) {
        scroll_down(scroll_bottom, max(1, params[0]));
      } else {
        scroll_down(TERM_HEIGHT - 1, max(1, params[0]));
      }
      break;
    case 'M': // Delete lines
      if (term_cursor_y < scroll_bottom) {
        scroll_up(scroll_bottom, max(1, params[0]));
      } else {
        scroll_up(TERM_HEIGHT - 1, max(1, params[0]));
      }
      break;
    case 'P': // Delete characters
      delete_chars(max(1, params[0]));
      break;
    case '@': // Insert characters
      insert_chars(max(1, params[0]));
      break;
    case 'r': // Set scroll region
      scroll_top = min(TERM_HEIGHT - 2, max(0, params[0] - 1));
      scroll_bottom = min(TERM_HEIGHT - 1, max(scroll_top + 1, params[1] - 1));
      break;
  }
  return "";
}

char* putc_setg0(char c) {
  term_putc = putc_normal;
  switch (c) {
    case 24: case 26: // Cancel
      break;
    case 27: // Escape
      term_putc = putc_escape;
      break;
    case '0':
      g0_graphics = true;
      break;
    default:
      g0_graphics = false;
  }
  return "";
}

char* putc_setg1(char c) {
  term_putc = putc_normal;
  switch (c) {
    case 24: case 26: // Cancel
      break;
    case 27: // Escape
      term_putc = putc_escape;
      break;
    case '0':
      g1_graphics = true;
      break;
    default:
      g1_graphics = false;
  }
  return "";
}

char* putc_ignore(char c) {
  if (c == 27) {
    term_putc = putc_escape;
  } else {
    term_putc = putc_normal;
  }
  return "";
}

char* putc_unicode1(char c) {
  unicode = (unicode << 6) | (c & 63);
  print_char(unicodetocodepage(unicode, false));
  term_putc = putc_normal;
  return "";
}

char* putc_unicode2(char c) {
  unicode = (unicode << 6) | (c & 63);
  term_putc = putc_unicode1;
  return "";
}

char* putc_unicode3(char c) {
  unicode = (unicode << 6) | (c & 63);
  term_putc = putc_unicode2;
  return "";
}

void term_init() {
  putc_escape('c');
}

char* term_keypress(int32_t keycode) {
  switch (keycode) {
    case TERM_KC_UP:
      return mode_appkeypad ? "\x2bOA" : "\x1b[A";
    case TERM_KC_DOWN:
      return mode_appkeypad ? "\x2bOB" : "\x1b[B";
    case TERM_KC_RIGHT:
      return mode_appkeypad ? "\x2bOC" : "\x1b[C";
    case TERM_KC_LEFT:
      return mode_appkeypad ? "\x2bOD" : "\x1b[D";
    case TERM_KC_HOME:
      return mode_appkeypad ? "\x2bOH" : "\x1b[H";
    case TERM_KC_END:
      return mode_appkeypad ? "\x2bOF" : "\x1b[F";
    case TERM_KC_PGUP:
      return "\x1b[5~";
    case TERM_KC_PGDN:
      return "\x1b[6~";
    default:
      if (keycode <= 0x7f) {
        temp[0] = keycode;
        temp[1] = 0;
      } else if (keycode <= 0x7ff) {
        temp[0] = 0xc0 | (keycode >> 6);
        temp[1] = 0x80 | (keycode & 0x3f);
        temp[2] = 0;
      } else if (keycode <= 0xffff) {
        temp[0] = 0xe0 | (keycode >> 12);
        temp[1] = 0x80 | ((keycode >> 6) & 0x3f);
        temp[2] = 0x80 | (keycode & 0x3f);
        temp[3] = 0;
      } else {
        temp[0] = 0xf0 | (keycode >> 18);
        temp[1] = 0x80 | ((keycode >> 12) & 0x3f);
        temp[2] = 0x80 | ((keycode >> 6) & 0x3f);
        temp[3] = 0x80 | (keycode & 0x3f);
        temp[4] = 0;
      }
      return temp;
  }
}
