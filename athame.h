/* athame.h -- Full vim integration for readline.*/

/* Copyright (C) 2014 James Kolb

   This file is part of the Athame patch for readline (Athame).

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

void athame_send_to_vim(char input);
void athame_get_vim_info();
char athame_loop(int instream);
char athame_process_input(int instream);
void athame_extraVimRead(int timer);
void athame_update_vim(int col);
char* athame_get_line_from_vim(int row);
void athame_sleep(int msec);
