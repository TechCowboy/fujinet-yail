#ifdef BUILD_PMD85
/** 
 * Input routines
 */

#include <conio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "input.h"
#include "cursor.h"
#include "colors.h"

extern unsigned short entry_timer;
extern bool long_entry_displayed;
extern unsigned char copy_host_slot;
extern bool copy_mode;

/**
 * Get input from keyboard/joystick
 * @return keycode (or synthesized keycode if joystick)
 */
unsigned char input()
{
  return cgetc();
}

// unsigned char input_ucase(void)
// {
//   unsigned char c = cgetc();
//   if ((c>='a') && (c<='z')) c&=~32;
//   return c;
// }

// static void input_clear_bottom(void)
// {
// }

void input_line(unsigned char x, unsigned char y, unsigned char o, char *c, unsigned char len, bool password)
{
  char i;
  char a;

  i = o; // index into array and y-coordinate
  // x += o;

  cputs("\x1Bq"); // reverse off
  textcolor(EDITLINE_COLOR);

  while(1)
  {
    cursor_pos(x + i, y); // update cursor position
    cursor(1);            // turn cursor on
    gotoxy(x + i, y);     // update text position

    a = input();
    switch (a)
    {
    case KEY_LEFT_ARROW:
    case KEY_DELETE:
      if (i>0)
      {
        c[--i] = '\0';
        gotoxy(x + i, y);
        cputc(' ');
      }
      break;
    case KEY_RIGHT_ARROW:
      break;
    case KEY_CLR:
      while (i>0)
      {
        c[--i] = '\0';
        gotoxy(x + i, y);
        cputc(' ');
      }
      break;
    case KEY_RETURN:
      c[i] = '\0';
      cursor(0); // turn off cursor
      return; // done
    default:
      if (i < len && a > 0x1F && a < 0x7F)
      {
        // hide cursor
        cursor(0);
        // draw character on cursor position
        if (password)
          cputc('*');
        else
          cputc(a);
        c[i++] = a;
      }
    break;
    }
  }
}

#endif /* BUILD_PMD85 */
