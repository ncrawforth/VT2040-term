bool g0_graphics = false;
bool g1_graphics = false;
bool use_g1 = false;

bool mode_appkeypad = false;
bool mode_insert = false;
bool mode_origin = false;
bool mode_crlf = false;

int attr_fgcolor = 7;
int attr_bgcolor = 0;
bool attr_bright = false;
bool attr_underline = false;
bool attr_reverse = false;

int saved_x = 0;
int saved_y = 0;
int saved_fgcolor = 7;
int saved_bgcolor = 0;
bool saved_bright = false;
bool saved_underline = false;
bool saved_reverse = false;

int params[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int paramc = 0;

char* putc_normal(char c);
char* putc_escape(char c);
char* putc_bracket(char c);
char* putc_setg0(char c);
char* putc_setg1(char c);
char* putc_ignore(char c);

char* putc_normal(char c) {
  switch (c) {
    case 27: // Escape
      term_putc = putc_escape;
      break;
    case 14: // Lock shift G1
      use_g1 = true;
      break;
    case 15: // Lock shift G0
      use_g1 = false;
      break;
    default:
      int bgcolor = attr_reverse ? attr_fgcolor : attr_bgcolor;
      int fgcolor = attr_reverse ? attr_bgcolor : attr_fgcolor;
      if (attr_bright) fgcolor |= 8;
      bool graphics = c > 0x5e && c < 0x7f && (use_g1 ? g1_graphics : g0_graphics;
      put(unicodetocodepoint(c, graphics), fgcolor, bgcolor, attr_underline);
  }
}

char* putc_escape(char c) {
  paramc = 0;
  switch (c) {
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
      cursor_down(1);
      break;
    case 'M': // Cursor up
      cursor_up(1);
      break;
    case 'E': // CR + NL
      cursor_down(1);
      set_cursor_x(0);
      break;
    case '7': // Save attributes and cursor position
    case 's':
      saved_x = get_cursor_x();
      saved_y = get_cursor_y();
      saved_fgcolor = attr_fgcolor;
      saved_bgcolor = attr_bgcolor;
      saved_bright = attr_bright;
      saved_underline = attr_underline;
      saved_reverse = attr_reverse;
      break;
    case '8': // Restore attributes and cursor position
    case 'u':
      set_cursor_x(saved_x);
      set_cursor_y(saved_y);
      attr_fgcolor = saved_fgcolor;
      attr_bgcolor = saved_bgcolor;
      attr_bright = saved_bright;
      attr_underline = saved_underline;
      attr_reverse = saved_reverse;
      break;
    case '=': // Keypad into applications mode
      keypad_app = true;
      break;
    case '>': // Keypad into numeric mode
      keypad_app = false;
      break;
    case 'Z': // Report terminal type
      return "\033[?1;0c";
      break;
    case 'c': // Reset to initial state
      set_scroll_region(0, INT_MAX);
      set_cursor_x(0);
      set_cursor_y(0);
      clear_screen();
      g0_graphics = false;
      g1_graphics = false;
      use_g1 = false;
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
      set_tab();
      break;
    default:
      term_putc = putc_normal;
  }
}

char* putc_bracket(char c) {
  term_putc = putc_normal;
  switch (c) {
    case 27: // Escape
      term_putc = putc_escape;
      break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      term_putc = putc_bracket;
      if (paramc == 0) {
        paramc = 1;
        params[0] = 0;
      }
      params[paramc - 1] = (params[paramc] * 10) + c - '0';
      break;
    case ';':
      term_putc = putc_bracket;
      paramc++;
      params[paramc - 1] = 0;
      break;
    case '?':
      term_putc = putc_bracket;
      break;
    case 'A': // Cursor up
      cursor_up(paramc < 1 ? 1 : params[0]);
      break;
    case 'B': // Cursor down
      cursor_down(paramc < 1 ? 1 : params[0]);
      break;
    case 'C': // Cursor right
      cursor_right(paramc < 1 ? 1 : params[0]);
      break;
    case 'D': // Cursor left
      cursor_left(paramc < 1 ? 1 : params[0]);
      break;
    case 'X': // Clear characters
      erase_chars(paramc < 1 ? 1 : params[0]);
      break;
    case 'K': // Clear line
      if (paramc == 0) {
        clear_end_of_line();
      } else if (params[0] == 0) {
        clear_end_of_line();
      } else if (params[0] == 1) {
        clear_start_of_line();
      } else if (params[0] == 2) {
        clear_line();
      }
      break;
    case 'J': // Clear screen
      if (paramc == 0) {
        clear_end_of_screen();
      } else if (params[0] == 0) {
        clear_end_of_screen();
      } else if (params[0] == 1) {
        clear_start_of_screen();
      } else if (params[0] == 2) {
        clear_screen();
      }
      break;
    case 'n': // Device status report
      if (paramc > 0 && params[0] == 5) {
        return "\033[0n";
      } else if (paramc > 0 && params[0] == 6) {
        char temp[32];
        sprintf(temp, "\033[%d;%dR", get_cursor_y() + 1, get_cursor_x() + 1);
        return temp
      }
      break;
    case 'c': // Identify
      return "\033[?1;2c";
      break;
    case 'x': // Request terminal parameters
      if (paramc > 0 && params[0] == 1) {
        return "\033[3;1;1;120;120;1;0x";
      } else {
        return "\033[2;1;1;120;120;1;0x";
      }
      break;
    case 's': // Save attributes and cursor position
      saved_x = get_cursor_x();
      saved_y = get_cursor_y();
      saved_fgcolor = attr_fgcolor;
      saved_bgcolor = attr_bgcolor;
      saved_bright = attr_bright;
      saved_underline = attr_underline;
      saved_reverse = attr_reverse;
      break;
    case 'u': // Restore attributes and cursor position
      set_cursor_x(saved_x);
      set_cursor_y(saved_y);
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
            break;
          case 20:
            mode_crlf = true;
            break;
          case 25:
            hide_cursor();
            break;
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
            break;
          case 20:
            mode_crlf = false;
            break;
          case 25:
            show_cursor();
            break;
      break;
    case 'H': // Set cursor position
    case 'f':
      set_cursor_y(paramc < 1 ? 0 : max(0, params[0] - 1));
      set_cursor_x(paramc < 2 ? 0 : max(0, params[1] - 1));
      break;
    case 'G': // Cursor to column x
    case '`':
      set_cursor_x(paramc < 1 ? 0 : max(0, params[0] - 1));
      break;
    case 'g': // Clear tab stops
      if (paramc < 1 || params[0] == 0) {
        clear_tab();
      } else {
        clear_all_tabs();
      }
      break;
    case 'm': // Set attribute
      for (int i = 0; i < paramc; i++) {
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
      break;
    case 'L': // Insert lines
      insert_lines(paramc < 1 ? 1 : params[0]);
      break;
    case 'M': // Delete lines
      delete_lines(paramc < 1 ? 1 : params[0]);
      break;
    case 'P': // Delete characters
      delete_chars(paramc < 1 ? 1 : params[0]);
      break;
    case '@': // Insert characters
      insert_chars(paramc < 1 ? 1 : params[0]);
      break;
    case 'r': // Set scroll region
      set_scroll_top(paramc < 1 ? 0 : max(0, params[0] - 1));
      set_scroll_bottom(paramc < 2 ? 0 : max(0, params[1] - 1));
      break;
  }
}

char* putc_setg0(char c) {
  term_putc = putc_normal;
  switch (c) {
    case 27: // Escape
      term_putc = putc_escape;
      break;
    case '0':
      g0_graphics = true;
      break;
    default:
      g0_graphics = false;
  }
}

char* putc_setg1(char c) {
  term_putc = putc_normal;
  switch (c) {
    case 27: // Escape
      term_putc = putc_escape;
      break;
    case '0':
      g1_graphics = true;
      break;
    default:
      g1_graphics = false;
  }
}

char* putc_ignore(char c) {
  if (c == 27) {
    term_putc = putc_escape;
  } else {
    term_putc = putc_normal;
  }
}

