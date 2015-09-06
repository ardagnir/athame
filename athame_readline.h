/* athame_readline.h -- Full vim integration for your shell.*/

/* Copyright (C) 2015 James Kolb

   This file is part of Athame.

   Athame is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Athame is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Athame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "readline.h"
#include "history.h"
#include <wchar.h>
#include "rlprivate.h"

char* athame_buffer_store = 0;
static char* ap_get_line_buffer()
{
  if (rl_end == 0)
  {
    return "";
  }
  if(athame_buffer_store)
  {
    free(athame_buffer_store);
  }
  return athame_buffer_store = strndup(rl_line_buffer, rl_end);
}

static int ap_get_line_buffer_length()
{
  return rl_end;
}

static int helper_ap_char_length(char* buffer)
{
  int ret;
  int len = mbstowcs(NULL, buffer, 0);
  wchar_t* wbuf = malloc((1 + len) * sizeof(wchar_t));
  mbstowcs(wbuf, buffer, len + 1);
  ret = wcswidth(wbuf, len + 1);
  free(wbuf);
  return ret;
}

static int ap_get_line_char_length()
{
  return helper_ap_char_length(ap_get_line_buffer());
}

static void ap_set_line_buffer(char* newText)
{
  rl_replace_line(newText, 0);
}

static int ap_get_cursor()
{
  int ret;
  char* temp = strndup(rl_line_buffer, rl_point);
  ret = helper_ap_char_length(temp);
  free(temp);
  return ret;
}

static void ap_set_cursor(int c)
{
  rl_point = 0;
  if (c == 0)
  {
    return;
  }
  rl_forward_char(c, 'l');
}

static void ap_set_cursor_end()
{
  rl_point = rl_end;
}

static void ap_display()
{
  rl_redisplay();
}

static int ap_get_term_width()
{
  int height, width;
  _rl_sigwinch_resize_terminal(); //Incase the terminal changed size while readline wasn't looking.
  rl_get_screen_size(&height, &width);
  return width;
}

static int ap_get_term_height()
{
  int height, width;
  _rl_sigwinch_resize_terminal(); //Incase the terminal changed size while readline wasn't looking.
  rl_get_screen_size(&height, &width);
  return height;
}

static int ap_get_prompt_length()
{
  return rl_expand_prompt(rl_prompt);
}

HISTORY_STATE* hs;
int hs_counter;
static void ap_get_history_start()
{
  hs = history_get_history_state(); 
  hs_counter = 0;
}

static char* ap_get_history_next()
{
  if (hs->entries && hs->entries[hs_counter])
  {
    return hs->entries[hs_counter++]->line;
  }
  else
  {
    return NULL;
  }
}

static void ap_get_history_end()
{
  xfree(hs);
}

static int ap_needs_to_leave()
{
  return rl_done || rl_num_chars_to_read > 0 && rl_end >= rl_num_chars_to_read;
}

static char* ap_get_slice(char* text, int start, int end)
{
  int mbchars;
  int pos_s = 0;
  int pos = 0;
  for(mbchars = 0; mbchars < end; mbchars++)
  {
    if (mbchars == start)
    {
      pos_s = pos;
    }
    int l = mblen(text + pos, MB_CUR_MAX);
    if (l >=0 )
    {
      pos += l;
    }
    else
    {
      if (mbchars < start)
      {
        return strdup("");
      }
      break;
    }
  }
  return strndup(text + pos_s, pos - pos_s);
}

#define SIGINT 2
static char ap_handle_signals()
{
  if (_rl_caught_signal == SIGINT)
  {
    _rl_signal_handler(_rl_caught_signal);
    if (rl_signal_event_hook)
    {
      (*rl_signal_event_hook) ();
    }
  }
  return 0;
}
