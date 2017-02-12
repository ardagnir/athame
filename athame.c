/* athame.c -- Full vim integration for your shell.*/

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

#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef BSD
//Includes OSX
#include <util.h>
#else
#include <pty.h>
#endif

#include "athame.h"
#include "athame_intermediary.h"
#include "athame_util.h"

/* Forward declarations used in this file. */
void athame_init(int instream, FILE* outstream)
{
  athame_outstream = outstream ? outstream : stderr;
  expr_pid = 0;
  athame_dirty = 0;
  updated = 1;
  athame_mode[0] = 'n';
  athame_mode[1] = '\0';
  athame_command[0] = '\0';
  command_cursor = 0;
  bottom_display[0] = '\0';
  last_athame_mode[0] = '\0';
  bottom_color = 0;
  bottom_style = 0;
  bottom_cursor = 0;
  cs_confirmed = 0;
  athame_failure = 0;

  dev_null = 0;

  servername = 0;
  dir_name = 0;
  slice_file_name = 0;
  contents_file_name = 0;
  update_file_name = 0;
  messages_file_name = 0;
  vimbed_file_name = 0;

  if (!athame_is_set("ATHAME_ENABLED", 1))
  {
    athame_failure = strdup("Athame was disabled on init.");
    return;
  }
  if (!getenv("DISPLAY"))
  {
    athame_set_failure("No X display found.");
    return;
  }

  asprintf(&servername, "athame_%d", getpid());
  asprintf(&dir_name, "/tmp/vimbed/%s", servername);
  asprintf(&slice_file_name, "%s/slice.txt", dir_name);
  asprintf(&contents_file_name, "%s/contents.txt", dir_name);
  asprintf(&update_file_name, "%s/update.txt", dir_name);
  asprintf(&messages_file_name, "%s/messages.txt", dir_name);
  if (getenv("ATHAME_VIMBED_LOCATION"))
  {
    asprintf(&vimbed_file_name, "%s/vimbed.vim", getenv("ATHAME_VIMBED_LOCATION"));
  }
  else
  {
    asprintf(&vimbed_file_name, "%s/vimbed.vim", VIMBED_LOCATION);
  }

  dev_null = fopen("/dev/null", "w");
  mkdir("/tmp/vimbed", S_IRWXU);
  mkdir(dir_name, S_IRWXU);

  if (athame_setup_history())
  {
    return;
  }

  if (is_vim_alive()) {
    athame_remote_expr("Vimbed_Reset()", 1);
    int cursor = ap_get_cursor();
    snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Vimbed_UpdateText(%d, %d, %d, %d, 1, 'StartLine')", athame_row+1, cursor+1, athame_row+1, cursor+1);
    athame_remote_expr(athame_buffer, 1);
    athame_poll_vim(1);
  } else {
    vim_stage = VIM_NOT_STARTED;
  }

  athame_ensure_vim(1, instream);
}

void athame_cleanup()
{
  int persist = athame_is_set("ATHAME_VIM_PERSIST", 0) && vim_stage == VIM_RUNNING && is_vim_alive();

  if(expr_pid > 0) {
    kill(expr_pid, SIGTERM);
  }
  if(vim_pid && !persist)
  {
    // forkpty will keep vim open on OSX if we don't close the fd
    kill(vim_pid, SIGTERM);
    close(vim_term);
  }
  if(dev_null)
  {
    fclose(dev_null);
  }
  if(slice_file_name)
  {
    unlink(slice_file_name);
    free(slice_file_name);
  }
  if(contents_file_name)
  {
    unlink(contents_file_name);
    free(contents_file_name);
  }
  if(update_file_name)
  {
    unlink(update_file_name);
    free(update_file_name);
  }
  if(messages_file_name)
  {
    unlink(messages_file_name);
    free(messages_file_name);
  }
  if(dir_name)
  {
    rmdir(dir_name);
    free(dir_name);
  }
  if(vimbed_file_name)
  {
    free(vimbed_file_name);
  }
  if(servername)
  {
    free(servername);
  }
  if(athame_failure)
  {
    athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
    free((char*)athame_failure);
  }
  else if (athame_is_set("ATHAME_SHOW_MODE", 1))
  {
    athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
  }
  if(expr_pid > 0) {
    wait_then_kill(expr_pid);
  }
  if(vim_pid && !persist)
  {
    wait_then_kill(vim_pid);
    vim_pid = 0;
  }
}

int athame_enabled()
{
  if (athame_is_set("ATHAME_ENABLED", 1))
  {
    if(athame_failure)
    {
      athame_draw_failure();
      return 0;
    }
    unsetenv("ATHAME_ERROR");
    return !ap_temp_novim();
  }
  else
  {
    return 0;
  }
}

char athame_loop(int instream)
{
  char returnVal = 0;
  sent_to_vim = 0;
  vim_sync = VIM_SYNC_YES;
  time_to_sync = get_time();
  last_athame_mode[0] = '\0';
  bottom_display[0] = '\0';

  // This is a performance step that allows us to bypass starting up vim if we aren't going to talk to it.
  char first_char = (vim_stage != VIM_RUNNING) ? athame_get_first_char(instream) : 0;
  if (first_char && strchr(ap_nl, first_char))
  {
    return first_char;
  }
  athame_ensure_vim(0, 0);

  if(!updated)
  {
    athame_update_vimline(athame_row, ap_get_cursor());
  }
  else
  {
    athame_redisplay();
  }


  while(!returnVal && !athame_failure)
  {
    struct timeval timeout;
    int selected = 0;
    if(first_char){
      returnVal = athame_process_char(first_char);
      first_char = 0;
    }
    else {
      while(selected == 0 && !athame_failure)
      {
        long timeout_msec = get_timeout_msec();
        selected = athame_select(instream, vim_term, 0, timeout_msec, 0);
        if (is_vim_alive()) // Is vim still running?
        {
          if (selected == 1)
          {
            returnVal = athame_process_input(instream);
          }
          else if (selected == 2)
          {
            read(vim_term, athame_buffer, DEFAULT_BUFFER_SIZE-1);
            if (!athame_get_vim_info()) {
              request_poll();
            }
          }
          else if (selected == 0)
          {
            if(vim_sync >= VIM_SYNC_CHAR_BEHIND) {
              vim_sync = VIM_SYNC_NEEDS_POLL;
            }
          }
          else if (selected == -1)
          {
            char sig_result;
            if (sig_result = ap_handle_signals())
            {
              return sig_result;
            } else {
              // Make sure we keep the mode drawn on a resize.
              athame_bottom_mode();
            }
          }
          if (time_to_poll >= 0 && time_to_poll < get_time()) {
            athame_poll_vim(0);
          }
        }
        else // Vim quit
        {
          if (!athame_has_clean_quit()) {
            athame_set_failure("Vim quit unexpectedly");
          }
          else if (sent_to_vim)
          {
            ap_set_line_buffer("");
            if(athame_dirty) {
              fprintf(athame_outstream, "\e[%dG\e[K", ap_get_prompt_length() + 1);
            }
            ap_display();
            return ap_delete;
          }
          else
          {
            // If we didn't send anything to vim, it shouldn't have quit.
            // We never want to kill the user's shell without giving them a chance
            // to type anything.
            athame_set_failure("Vim quit");
          }
        }
      }
    }
    if (ap_needs_to_leave()) //We need to leave now
    {
       return '\0';
    }
  }
  if(!athame_failure)
  {
    if(sent_to_vim)
    {
      // Should already be synced here, but this is a no-op in that case and
      // it makes the code less fragile to make sure.
      athame_force_vim_sync();
      if(strcmp(athame_mode, "i") == 0)
      {
        athame_send_to_vim('\x1d'); //<C-]> Finish abbrevs/kill mappings
        athame_force_vim_sync();
      }
    }
    if (athame_is_set("ATHAME_SHOW_MODE", 1))
    {
      athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
    }
    updated = 0;
  } else {
    // If we ate the first_char, we should return it.
    if(returnVal == 0) {
      returnVal = first_char;
    }
  }

  return returnVal;
}

void athame_after_bypass() {
  if (athame_failure && athame_is_set("ATHAME_SHOW_ERROR", 1))
  {
    athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
  }
}

// Redraw bottom stuff before handing off to a process that might block,
// like python.
void athame_char_handled() {
  if (athame_is_set("ATHAME_ENABLED", 1) && !ap_needs_to_leave()) {
    if(athame_failure)
    {
      athame_draw_failure();
    }
    else
    {
      athame_bottom_mode();
    }
  }
}
