/* athame.h -- Full vim integration for your shell.*/

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

#ifndef _ATHAME_H_
#define _ATHAME_H_

#include <stdio.h>

#define ATHAME_NORMAL 0
#define ATHAME_BOLD 1

#define ATHAME_DEFAULT 0
#define ATHAME_RED 31

#ifdef __cplusplus
extern "C" {
#endif

// Initialize athame.
extern void athame_init(int instream, FILE* outstream);
static void athame_init_sig(int instream, FILE* outstream);

// Loop over input and do vimy stuff until a special key is pressed.
extern char athame_loop(int instream);
static char athame_loop_sig(int instream);

// Is athame currently enabled?
extern int athame_enabled();

// Cleanup all the memory allocated by athame and shut vim down.
extern void athame_cleanup(int);

// Run after bypassing athame.
extern void athame_after_bypass();

// Called after we are done with the char returned by athame.
// This is really a hack to display mode while an external process is waiting for char input that it will probably send to readline and thus vim.
extern void athame_char_handled();

#ifdef __cplusplus
}
#endif

#endif /* !_ATHAME_H_ */
