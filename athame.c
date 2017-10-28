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
// Includes OSX
#include <util.h>
#else
#include <pty.h>
#endif

#include "athame.h"
#include "athame_intermediary.h"
#include "athame_util.h"

/* Forward declarations used in this file. */
void athame_init(int instream, FILE* outstream) {
  if (!ap_is_catching_signals()) {
    steal_signal_handler();
  }
  athame_init_sig(instream, outstream);
  if (!ap_is_catching_signals()) {
    return_signal_handler();
  }
}

static void athame_init_sig(int instream, FILE* outstream) {
  athame_outstream = outstream ? outstream : stderr;
  cleaned = 0;
  expr_pid = 0;
  athame_dirty = 0;
  updated = 1;
  time_to_poll = -1;
  vim_started = -1;
  stale_polls = 0;
  change_since_key = 0;
  keys_since_change = 0;
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
  msg_sent = 0;

  dev_null = 0;
  fifo = 0;

  servername = 0;
  dir_name = 0;
  slice_file_name = 0;
  contents_file_name = 0;
  update_file_name = 0;
  messages_file_name = 0;
  vimbed_file_name = 0;
  fifo_name = 0;
  msg_count_file_name = 0;

  if (!athame_is_set("ATHAME_ENABLED", 1)) {
    athame_failure = strdup("Athame was disabled on init");
    return;
  }
  if (!athame_is_set("ATHAME_USE_JOBS", ATHAME_USE_JOBS_DEFAULT) && !getenv("DISPLAY")) {
    athame_set_failure("Vim with +job required to use Athame without X");
    return;
  }

  asprintf(&servername, "athame_%d", getpid());

  char* tmpdir = getenv(temp_dir_loc());
  if (tmpdir == NULL) {
    char* failure;
    asprintf(&failure, "%s environment variable not set", temp_dir_loc());
    athame_set_failure(failure);
    free(failure);
    return;
  }
  char* parent_name;
  asprintf(&parent_name, "%s/athame_vimbed", tmpdir);
  asprintf(&dir_name, "%s/%s", parent_name, servername);
  mkdir(parent_name, S_IRWXU);
  mkdir(dir_name, S_IRWXU);
  free(parent_name);

  asprintf(&slice_file_name, "%s/slice.txt", dir_name);
  asprintf(&contents_file_name, "%s/contents.txt", dir_name);
  asprintf(&update_file_name, "%s/update.txt", dir_name);
  asprintf(&messages_file_name, "%s/messages.txt", dir_name);
  asprintf(&msg_count_file_name, "%s/messageCount.txt", dir_name);
  if (getenv("ATHAME_VIMBED_LOCATION")) {
    asprintf(&vimbed_file_name, "%s/vimbed.vim",
             getenv("ATHAME_VIMBED_LOCATION"));
  } else {
    asprintf(&vimbed_file_name, "%s/vimbed.vim", VIMBED_LOCATION);
  }

  asprintf(&fifo_name, "%s/exprPipe", dir_name);
  mkfifo(fifo_name, S_IRWXU);

  dev_null = fopen("/dev/null", "w");

  if (athame_setup_history()) {
    return;
  }

  if (is_vim_alive()) {
    athame_remote_expr("Vimbed_Reset()", 1);
    int cursor = ap_get_cursor();
    snprintf(athame_buffer, DEFAULT_BUFFER_SIZE - 1,
             "Vimbed_UpdateText(%d, %d, %d, %d, 1, 'StartLine')",
             athame_row + 1, cursor + 1, athame_row + 1, cursor + 1);
    athame_remote_expr(athame_buffer, 1);
    athame_poll_vim(1);
  } else {
    vim_stage = VIM_NOT_STARTED;
  }

  athame_ensure_vim(1, instream);
}

// Cleanup everything athame related.
// If locked is set, make sure not to let control leave athame during cleanup.
// (This is useful if the shell wants to shutdown as soon as it gets any control)
void athame_cleanup(int locked) {
  if (cleaned) {
    return;
  }
  cleaned = 1;
  int persist = athame_is_set("ATHAME_VIM_PERSIST", 0) &&
                vim_stage == VIM_RUNNING && is_vim_alive();

  if (expr_pid > 0) {
    kill(expr_pid, SIGTERM);
  }
  if (vim_pid && !persist) {
    // forkpty will keep vim open on OSX if we don't close the fd
    kill(vim_pid, SIGTERM);
    athame_sleep(20, 0, 0);
    close(vim_term);
  }
  if (dev_null) {
    fclose(dev_null);
  }
  if (vimbed_file_name) {
    free(vimbed_file_name);
  }
  if (servername) {
    free(servername);
  }
  if (athame_failure) {
    if(!locked) {
      athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
    }
    free((char*)athame_failure);
  } else if (athame_is_set("ATHAME_SHOW_MODE", 1)) {
    if (!locked) {
      athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
    }
  }
  if (expr_pid > 0) {
    wait_then_kill(expr_pid);
  }
  if (vim_pid && !persist) {
    wait_then_kill(vim_pid);
    vim_pid = 0;
  }
  if (fifo) {
    close(fifo);
  }
  if (fifo_name) {
    unlink(fifo_name);
    free(fifo_name);
  }
  if (slice_file_name) {
    unlink(slice_file_name);
    free(slice_file_name);
  }
  if (contents_file_name) {
    unlink(contents_file_name);
    free(contents_file_name);
  }
  if (update_file_name) {
    unlink(update_file_name);
    free(update_file_name);
  }
  if (messages_file_name) {
    unlink(messages_file_name);
    free(messages_file_name);
  }
  if (msg_count_file_name) {
    unlink(msg_count_file_name);
    free(msg_count_file_name);
  }
  if (dir_name) {
    rmdir(dir_name);
    free(dir_name);
  }
}

int athame_enabled() {
  if (athame_is_set("ATHAME_ENABLED", 1)) {
    if (athame_failure) {
      athame_draw_failure();
      return 0;
    }
    unsetenv("ATHAME_ERROR");
    return !ap_temp_novim();
  } else {
    return 0;
  }
}

char athame_loop(int instream) {
  if (!ap_is_catching_signals()) {
    steal_signal_handler();
  }
  char r = athame_loop_sig(instream);
  if (!ap_is_catching_signals()) {
    return_signal_handler();
  }
  return r;
}

static char athame_loop_sig(int instream) {
  char returnVal = 0;
  sent_to_vim = 0;
  time_to_sync = get_time();
  last_athame_mode[0] = '\0';
  bottom_display[0] = '\0';

  // This is a performance step that allows us to bypass starting up vim if we
  // aren't going to talk to it.
  char first_char =
      (vim_stage != VIM_RUNNING) ? athame_get_first_char(instream) : 0;
  if (first_char && strchr(ap_nl, first_char)) {
    return first_char;
  }
  athame_ensure_vim(0, 0);

  // If we have a character already, assume Vim hasn't had time to sync.
  vim_sync = first_char ? VIM_SYNC_NO : VIM_SYNC_YES;

  if (!updated) {
    athame_update_vimline(athame_row, ap_get_cursor());
  } else {
    athame_redisplay();
  }

  while (!returnVal && !athame_failure) {
    int selected = 0;
    if (first_char) {
      returnVal = athame_process_char(first_char);
      first_char = 0;
    } else {
      while (selected == 0 && !athame_failure) {
        long timeout_msec = get_timeout_msec();
        selected = athame_select(instream, vim_term, 0, timeout_msec, 0);
        if (is_vim_alive())  // Is vim still running?
        {
          if (selected == 1) {
            returnVal = athame_process_input(instream);
          } else if (selected == 2) {
            read(vim_term, athame_buffer, DEFAULT_BUFFER_SIZE - 1);
            if (!athame_get_vim_info()) {
              request_poll();
            }
          } else if (selected == 0) {
            if (vim_sync >= VIM_SYNC_CHAR_BEHIND) {
              vim_sync = VIM_SYNC_NEEDS_POLL;
            }
          }
          int sr = process_signals();
          if (sr != -1) {
            return (char)sr;
          }
          if (selected == -1) {
            // Make sure we keep the mode drawn on a resize.
            // This must happen after process_signals so we don't change the
            // value of the caught signal
            athame_bottom_mode();
          }

          if (time_to_poll >= 0 && time_to_poll < get_time()) {
            athame_poll_vim(0);
          }

          // If vim isn't doing anything after pressing 20 keys, failover.
          // This allows the user to get back to a normal shell if everything
          // is working, but vim always starts up in a weird state
          // (Infinte, loop/ex mode, etc)
          // TODO: use an env variable instead of 20
          if (keys_since_change > 20) {
            athame_force_vim_sync();
            if (keys_since_change > 0) {
              athame_set_failure("20 keys pressed without change. To disable Athame use: AHTAME_ENABLED=0");
            }
          }
        } else  // Vim quit
        {
          if (!athame_has_clean_quit() && !athame_failure) {
            athame_set_failure("Vim quit unexpectedly");
          } else if (sent_to_vim) {
            ap_set_line_buffer("");
            if (athame_dirty) {
              fprintf(athame_outstream, "\e[%dG\e[K",
                      ap_get_prompt_length() + 1);
            }
            ap_display();
            return ap_delete;
          } else {
            // If we didn't send anything to vim, it shouldn't have
            // quit.
            // We never want to kill the user's shell without giving
            // them a to type anything.
            athame_set_failure("Vim quit");
          }
        }
      }
    }
    if (ap_needs_to_leave())  // We need to leave now
    {
      if (athame_is_set("ATHAME_SHOW_MODE", 1)) {
        athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
      }
      return '\0';
    }
  }
  if (!athame_failure) {
    if (sent_to_vim) {
      // Should already be synced here, but this is a no-op in that case
      // and it makes the code less fragile to make sure.
      athame_force_vim_sync();
      if (strcmp(athame_mode, "i") == 0) {
        athame_send_to_vim('\x1d');  //<C-]> Finish abbrevs/kill mappings
        athame_force_vim_sync();
      }
    }
    if (athame_is_set("ATHAME_SHOW_MODE", 1)) {
      athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
    }
    updated = 0;
  } else {
    // If we ate the first_char, we should return it.
    if (returnVal == 0) {
      returnVal = first_char;
    }
  }

  return returnVal;
}

void athame_after_bypass() {
  if (athame_failure && athame_is_set("ATHAME_SHOW_ERROR", 1)) {
    athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
  }
}

// Redraw bottom stuff before handing off to a process that might block,
// like python.
void athame_char_handled() {
  if (athame_is_set("ATHAME_ENABLED", 1) && !ap_needs_to_leave()) {
    if (athame_failure) {
      athame_draw_failure();
    } else {
      athame_bottom_mode();
    }
  }
}
