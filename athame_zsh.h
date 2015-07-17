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

static char* ap_get_line_buffer()
{
  if (zlemetaline != 0)
  {
    return dupstring(zlemetaline);
  }
  return zlelineasstring(zleline, zlell, 0, NULL, NULL, 1);
}

static int ap_get_line_buffer_length()
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
  fixsuffix();
  menucmp = 0;
}

static void ap_display()
{
  zrefresh();
}

static void ap_force_display()
{
  printf("%s", lpromptbuf);
  fflush(stdout);
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
  return strlen(lpromptbuf);
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
  return ret;
}

static void ap_get_history_end()
{
}
