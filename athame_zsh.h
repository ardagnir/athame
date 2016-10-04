/* athame_zsh.h -- Full vim integration for your shell.*/

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

   This file incorporates work under the following copyright and permission notice:
      The Z Shell is copyright (c) 1992-2009 Paul Falstad, Richard Coleman,
      Zoltán Hidvégi, Andrew Main, Peter Stephenson, Sven Wischnowsky, and
      others.  All rights reserved.  Individual authors, whether or not
      specifically named, retain copyright in all changes; in what follows, they
      are referred to as `the Zsh Development Group'.  This is for convenience
      only and this body has no legal status.  The Z shell is distributed under
      the following licence; any provisions made in individual files take
      precedence.

      Permission is hereby granted, without written agreement and without
      licence or royalty fees, to use, copy, modify, and distribute this
      software and to distribute modified versions of this software for any
      purpose, provided that the above copyright notice and the following
      two paragraphs appear in all copies of this software.

      In no event shall the Zsh Development Group be liable to any party for
      direct, indirect, special, incidental, or consequential damages arising out
      of the use of this software and its documentation, even if the Zsh
      Development Group have been advised of the possibility of such damage.

      The Zsh Development Group specifically disclaim any warranties, including,
      but not limited to, the implied warranties of merchantability and fitness
      for a particular purpose.  The software provided hereunder is on an "as is"
      basis, and the Zsh Development Group have no obligation to provide
      maintenance, support, updates, enhancements, or modifications.

*/
#include "zle.mdh"

//Note: Most of this code is from zle_params.c

static char* unmetafy_athame(char* text)
{
  int length;
  char* unmeta = unmetafy(text, &length);
  for (int i = 0; i < length; i++)
  {
    if (unmeta[i] == NULL)
    {
      // Display nulls as spaces. This isn't perfect, but this is mostly
      // just to avoid memory errors.
      unmeta[i] = ' ';
    }
  }
  return unmeta;
}

static char* ap_get_line_buffer()
{
  if (zlemetaline != 0)
  {
    return unmetafy_athame(dupstring(zlemetaline));
  }
  return unmetafy_athame(zlelineasstring(zleline, zlell, 0, NULL, NULL, 1));
}

static int ap_get_line_buffer_length()
{
  return strlen(ap_get_line_buffer());
}

static int ap_get_line_char_length()
{
  return zlell;
}

static void ap_set_line_buffer(char* newText)
{
  setline(newText, ZSL_COPY);
}

static int ap_get_cursor()
{
  if (zlemetaline != NULL) {
    /* A lot of work for one number, but still... */
    ZLE_STRING_T tmpline;
    int tmpcs, tmpll, tmpsz;
    char *tmpmetaline = ztrdup(zlemetaline);
    tmpline = stringaszleline(tmpmetaline, zlemetacs,
            &tmpll, &tmpsz, &tmpcs);
    free(tmpmetaline);
    free(tmpline);
    return tmpcs;
  }
  return zlecs;
}

static void ap_set_cursor(int x)
{
  if(x < 0)
  {
    zlecs = 0;
  }
  else if(x > zlell)
  {
    zlecs = zlell;
  }
  else
  {
    zlecs = x;
  }
}

static void ap_set_cursor_end()
{
  zlecs = zlell;
}

/* Temporarily disable vim (used by zsh for completion).*/
static int ap_temp_novim()
{
  return menucmp != 0 || suffixlen > 0 || showinglist != 0;
}

static void ap_redraw_prompt()
{
  resetprompt(NULL);
}

static void ap_display()
{
  zrefresh();
}

static int ap_get_term_width()
{
  adjustwinsize(1);
  return zterm_columns;
}

static int ap_get_term_height()
{
  adjustwinsize(1);
  return zterm_lines;
}

static int ap_get_prompt_length()
{
  int length;
  countprompt(lpromptbuf, &length, NULL, -1);
  return length;
}


Histent he;
static void ap_get_history_start()
{
  he = hist_ring->down;
}

#define GETZLETEXT(ent) ((ent)->zle_text ? (ent)->zle_text : (ent)->node.nam)
static char* ap_get_history_next()
{
  if (he == hist_ring)
  {
    return 0;
  }
  char* ret = GETZLETEXT(he);
  he = down_histent(he);
  return unmetafy_athame(dupstring(ret));
}

static void ap_get_history_end()
{
}

static int ap_needs_to_leave()
{
  return 0;
}

static char* ap_get_substr(char* text, int start, int end)
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

static char ap_handle_signals()
{
  int q = queue_signal_level();
  // This forces all queued signals to be handled by zsh now.
  dont_queue_signals();
  restore_queue_signals(q);
  if (errflag & ERRFLAG_INT)
    return EOF;
  return 0;
}

static char* ap_nl = "\r\n";
static char* ap_special = "\t\x04\r\n\x0c";
static char ap_delete = '\x04';

static void ap_set_control_chars()
{
	//TODO: Lookup zsh control chars instead of assuming defaults.
}

static void ap_set_nospecial() {
	// We don't care about this in zsh.
}
