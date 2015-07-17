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

static void ap_set_line_buffer(char* newText)
{
  rl_replace_line(newText, 0);
}

static int ap_get_cursor()
{
  return rl_point;
}

static void ap_set_cursor(int c)
{
  rl_point = c;
}

static void ap_display()
{
  rl_redisplay();
}

static void ap_force_display()
{
  rl_forced_update_display();
}

static int ap_get_term_width()
{
  int height, width;
  rl_resize_terminal(); //Incase the terminal changed size while readline wasn't looking.
  rl_get_screen_size(&height, &width);
  return width;
}

static int ap_get_term_height()
{
  int height, width;
  rl_resize_terminal(); //Incase the terminal changed size while readline wasn't looking.
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
