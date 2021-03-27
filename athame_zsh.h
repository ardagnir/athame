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

   This file incorporates work under the following copyright and permission
   notice:
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

// Note: Most of this code is from zle_params.c

static char* unmetafy_athame(char* text) {
  int length;
  char* unmeta = unmetafy(text, &length);
  for (int i = 0; i < length; i++) {
    if (unmeta[i] == NULL) {
      // Display nulls as spaces. This isn't perfect, but this is mostly
      // just to avoid memory errors.
      unmeta[i] = ' ';
    }
  }
  return unmeta;
}

static char* ap_get_line_buffer() {
  if (zlemetaline != 0) {
    return unmetafy_athame(dupstring(zlemetaline));
  }
  return unmetafy_athame(zlelineasstring(zleline, zlell, 0, NULL, NULL, 1));
}

static int ap_get_line_buffer_length() { return strlen(ap_get_line_buffer()); }

static int ap_get_line_char_length() { return zlell; }

static void ap_set_line_buffer(char* newText) { setline(newText, ZSL_COPY); }

static int ap_get_cursor() {
  if (zlemetaline != NULL) {
    /* A lot of work for one number, but still... */
    ZLE_STRING_T tmpline;
    int tmpcs, tmpll, tmpsz;
    char* tmpmetaline = ztrdup(zlemetaline);
    tmpline = stringaszleline(tmpmetaline, zlemetacs, &tmpll, &tmpsz, &tmpcs);
    free(tmpmetaline);
    free(tmpline);
    return tmpcs;
  }
  return zlecs;
}

static void ap_set_cursor(int x) {
  if (x < 0) {
    zlecs = 0;
  } else if (x > zlell) {
    zlecs = zlell;
  } else {
    zlecs = x;
  }
}

static void ap_set_cursor_end() { zlecs = zlell; }

/* Temporarily disable vim (used by zsh for completion).*/
static int ap_temp_novim() {
  return menucmp != 0 || suffixlen > 0 || showinglist != 0;
}

static void ap_redraw_prompt() { resetprompt(NULL); }

static void ap_display() { 
  zrefresh();
}

static void ap_get_term_size(int* height, int* width) {
  adjustwinsize(1);
  if (width) {
    *width = zterm_columns;
  }
  if (height) {
    *height = zterm_lines;
  }
}

static int ap_get_prompt_length() {
  int length;
  countprompt(lpromptbuf, &length, NULL, -1);
  return length;
}

Histent he;
static void ap_get_history_start() { he = hist_ring->down; }

#define GETZLETEXT(ent) ((ent)->zle_text ? (ent)->zle_text : (ent)->node.nam)
static char* ap_get_history_next() {
  if (he == hist_ring) {
    return 0;
  }
  char* ret = GETZLETEXT(he);
  he = down_histent(he);
  return unmetafy_athame(dupstring(ret));
}

static void ap_get_history_end() {}

static int ap_needs_to_leave() { return 0; }

static char* ap_get_substr(char* text, int start, int end) {
  int mbchars;
  int pos_s = 0;
  int pos = 0;
  for (mbchars = 0; mbchars < end; mbchars++) {
    if (mbchars == start) {
      pos_s = pos;
    }
    int l = mblen(text + pos, MB_CUR_MAX);
    if (l >= 0) {
      pos += l;
    } else {
      if (mbchars < start) {
        return strdup("");
      }
      break;
    }
  }
  return strndup(text + pos_s, pos - pos_s);
}

static char ap_handle_signals() {
  int q = queue_signal_level();

  // Calling dont_queue_signals() here would make zsh process all queued signals.
  // This is a modified version of dont_queue_signals() that calls athame_cleanup
  // before processing any SIGHUP. The SIGHUP would cause zsh to close and we need
  // to make sure we cleanup athame first.
  queueing_enabled = 0;
  while (queue_front != queue_rear) {      /* while signals in queue */
    sigset_t oset;
    queue_front = (queue_front + 1) % MAX_QUEUE_SIZE;
    oset = signal_setmask(signal_mask_queue[queue_front]);
    if (signal_queue[queue_front] == SIGHUP) {
      athame_cleanup(1);
    }
    zhandler(signal_queue[queue_front]);  /* handle queued signal   */
    signal_setmask(oset);
  }

  restore_queue_signals(q);

  if (errflag & ERRFLAG_INT) {
    return EOF;
  }
  return 0;
}

static char ap_nl[129];
static char ap_special[129];
static char ap_delete;
static char *ap_multi_special[129];

static int zsh_is_special_thingy(Thingy t, char key){
  // TODO: don't parse this variable for each char
  // This is inefficient, but it's only run at startup.
  char* specials = getenv("ATHAME_ZSH_SPECIAL");
  if (!specials){
    specials = "expand-or-complete,delete-char-or-list,clear-screen";
  }
  specials = strdup(specials);
  char* special;
  char* original_specials = specials;
  while (special = athame_tok(&specials, ',')){
   if (strcmp(t->nam, special) == 0){
      free(original_specials);
      return 1;
    }
  }
  free(original_specials);
  return 0;
}

static void ap_set_control_chars() {
  int specialLen = 0;
  int nlLen = 0;
  char keystring[3];
  keystring[1]='\0';
  ap_delete = '\x04';
  ap_multi_special[0] = 0;
  for (int key = 1; key<128; key++) {
    keystring[0]=key;
    metafy(keystring, 1, META_NOALLOC);
    char* s;
    Thingy t = keybind(curkeymap, keystring, &s);
    if (t == t_acceptline) {
      ap_special[specialLen++] = key;
      ap_nl[nlLen++] = key;
    } else if (t && zsh_is_special_thingy(t, key)) {
      ap_special[specialLen++] = key;
    }

    // We need delete so we can send it to zsh. This is separate from whether or not it's a special key.
    if (t == t_deletecharorlist) {
      ap_delete = key;
    }
  }
  ap_special[specialLen] = '\0';
  ap_nl[nlLen] = '\0';
}

static void ap_set_nospecial() {
  // We don't care about this in zsh.
}

static int ap_is_catching_signals() {
  return 1;
}
