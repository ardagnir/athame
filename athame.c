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
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "athame.h"
#include "athame_intermediary.h"

#define DEFAULT_BUFFER_SIZE 1024
#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

#define NORMAL 0
#define BOLD 1

#define DEFAULT 0
#define RED 31

static char athame_buffer[DEFAULT_BUFFER_SIZE];
static char last_vim_command[DEFAULT_BUFFER_SIZE];
static int last_cmd_pos;
static const char* athame_failure;
static int vim_pid;
static int expr_pid;
static int vim_to_readline[2];
static int readline_to_vim[2];
static int from_vim;
static int to_vim;
static int cs_confirmed;
static FILE* dev_null;
static int athame_row;
static int updated;
static char* contents_file_name;
static char* update_file_name;
static char* meta_file_name;
static char* messages_file_name;
static char* vimbed_file_name;
static char* dir_name;
static char* servername;
static int sent_to_vim = 0;
static int needs_poll = 0;
static FILE* athame_outstream = 0;

 //Keep track of if last key was a tab. We need to fake keys between tabpresses or readline completion gets confused.
static int last_tab;
static int tab_fix; //We just sent a fake space to help completion work. Now delete it.

static char athame_mode[3];
static char athame_displaying_mode[3];
static int end_col; //For visual mode
static int end_row; //For visual mode


static int athame_dirty;

static char first_char;

/* Forward declarations used in this file. */
static void athame_send_to_vim(char input);
static void athame_get_vim_info();
static void athame_set_failure(char* fail_str);
static char athame_process_input(int instream);
static char athame_process_char(char instream);
static void athame_extraVimRead(int timer);
static int athame_setup_history();
static char* athame_get_lines_from_vim(int start_row, int end_row);
static void athame_sleep(int msec);
static int athame_get_vim_info_inner(int read_pipe);
static void athame_update_vimline(int row, int col);
static int athame_remote_expr(char* expr, int bock);
static void athame_bottom_display(char* string, int style, int color, int cursor);
static int athame_wait_for_file(char* filename, int sanity);
static int athame_wait_for_vim();
static char athame_get_first_char();
static int athame_highlight(int start, int end);
static void athame_bottom_mode();
static void athame_poll_vim(int block);
static void athame_draw_failure();
int athame_is_set(char* env, int def);

void athame_init(FILE* outstream)
{
  athame_outstream = outstream ? outstream : stderr;
  last_tab = 0;
  tab_fix = 0;
  expr_pid = 0;
  athame_dirty = 0;
  updated = 1;
  athame_mode[0] = 'n';
  athame_mode[1] = '\0';
  athame_displaying_mode[0] = 'n';
  athame_displaying_mode[1] = '\0';
  last_vim_command[0] = '\0';
  last_cmd_pos = 0;
  cs_confirmed = 0;
  athame_failure = 0;


  dev_null = 0;
  from_vim = 0;
  to_vim = 0;
  vim_pid = 0;

  servername = 0;
  dir_name = 0;
  contents_file_name = 0;
  update_file_name = 0;
  meta_file_name = 0;
  messages_file_name = 0;
  vimbed_file_name = 0;

  if (!athame_is_set("ATHAME_ENABLED", 1))
  {
    athame_failure = "Athame was disabled on init.";
    return;
  }
  first_char = athame_get_first_char();
  if (first_char && strchr("\n\r", first_char) != 0)
  {
    return;
  }

  if (!getenv("DISPLAY"))
  {
    athame_set_failure("No X display found.");
    return;
  }
  //Note that this rand() is not seeded.by athame.
  //It only establishes uniqueness within a single process using readline.
  //The pid establishes uniqueness between processes and makes debugging easier.
  asprintf(&servername, "athame_%d_%d", getpid(), rand() % (1000000000));
  asprintf(&dir_name, "/tmp/vimbed/%s", servername);
  asprintf(&contents_file_name, "%s/contents.txt", dir_name);
  asprintf(&update_file_name, "%s/update.txt", dir_name);
  asprintf(&meta_file_name, "%s/meta.txt", dir_name);
  asprintf(&messages_file_name, "%s/messages.txt", dir_name);
  asprintf(&vimbed_file_name, "%s/vimbed.vim", VIMBED_LOCATION);

  char* etcrc = "/etc/athamerc";
  char homerc[256];
  char* athamerc;
  snprintf(homerc, 255, "%s/.athamerc", getenv("HOME"));
  if (!access(homerc, R_OK))
  {
    athamerc = homerc;
  }
  else if (!access(etcrc, R_OK))
  {
    athamerc = etcrc;
  }
  else
  {
    athame_set_failure("No athamerc found.");
    return;
  }

  mkdir("/tmp/vimbed", S_IRWXU);
  mkdir(dir_name, S_IRWXU);

  if (athame_setup_history())
  {
    return;
  }

  dev_null = fopen("/dev/null", "w");
  pipe(vim_to_readline);
  pipe(readline_to_vim);
  int pid = fork();
  if (pid == 0)
  {
    dup2(readline_to_vim[0], STDIN_FILENO);
    dup2(vim_to_readline[1], STDOUT_FILENO);
    dup2(vim_to_readline[1], STDERR_FILENO);
    close(vim_to_readline[0]);
    close(readline_to_vim[1]);
    snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "+call Vimbed_UpdateText(%d, %d, %d, %d, 1)", athame_row+1, 1, athame_row+1, 1);
    if (execlp("vim", "vim", "--servername", servername, "-S", vimbed_file_name, "-S", athamerc, "-s", "/dev/null", "+call Vimbed_SetupVimbed('', '')", athame_buffer, NULL)!=0)
    {
      printf("Error: %d", errno);
      close(vim_to_readline[1]);
      close(readline_to_vim[0]);
      exit(EXIT_FAILURE);
    }
  }
  else if (pid == -1)
  {
    //TODO: error handling
    printf("ERROR! Couldn't run vim!");
  }
  else
  {
    close(vim_to_readline[1]);
    close(readline_to_vim[0]);
    vim_pid = pid;
    from_vim = vim_to_readline[0];
    to_vim = readline_to_vim[1];

    if(athame_wait_for_vim())
    {
      //We already set failure in the function
      return;
    }
    if (athame_wait_for_file(contents_file_name, 20))
    {
      athame_set_failure("Vimbed failure");
      return;
    }
    athame_sleep(50); // Give some time to send correct mode for gdb-like uses that need to know now
    athame_get_vim_info_inner(1);
    athame_bottom_mode();
  }
}

void athame_cleanup()
{
  if(to_vim)
  {
    close(to_vim);
  }
  if(from_vim)
  {
    close(from_vim);
  }
  if(dev_null)
  {
    fclose(dev_null);
  }
  if(vim_pid)
  {
    kill(vim_pid, 9);
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
  if(meta_file_name)
  {
    unlink(meta_file_name);
    free(meta_file_name);
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
}

static char athame_get_first_char()
{
  char return_val = '\0';
  fd_set files;
  FD_ZERO(&files);
  FD_SET(fileno(stdin), &files);
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 1 * 1000;

  int results = select(fileno(stdin)+1, &files, NULL, NULL, &timeout);
  if(results > 0){
    read(fileno(stdin), &return_val, 1);
  }
  return return_val;
}

static void athame_sleep(int msec)
{
    struct timespec timeout, timeout2;
    timeout.tv_sec = 0;
    timeout.tv_nsec = msec * 1000000;
    while (nanosleep(&timeout, &timeout2)<0)
    {
      timeout = timeout2;
    }
}

/* Write history file and store number of lines in athame_row */
static int athame_setup_history()
{
  FILE* updateFile = fopen(update_file_name, "w+");

  if(!updateFile){
    athame_set_failure("Couldn't create temporary file in /tmp/vimbed");
    return 1;
  }

  ap_get_history_start();
  int total_lines = 0;
  char* history_line;
  while (history_line = ap_get_history_next())
  {
    fwrite(history_line, 1, strlen(history_line), updateFile);
    // Count newlines in history
    while(history_line = strstr(history_line, "\n"))
    {
      history_line++;
      total_lines++;
    }
    fwrite("\n", 1, 1, updateFile);
    total_lines++;
  }
  fwrite("\n", 1, 1, updateFile);
  athame_row = total_lines;
  ap_get_history_end();

  fclose(updateFile);
  return 0;
}


static int athame_wait_for_vim()
{
  struct timespec timeout;
  timeout.tv_sec = 1;
  timeout.tv_nsec = 0;
  fd_set files;
  FD_ZERO(&files);
  FD_SET(from_vim, &files);
  sigset_t block_signals;
  sigfillset(&block_signals);
  //Wait for vim to start up
  int results = pselect(from_vim + 1, &files, NULL, NULL, &timeout, &block_signals);

  if (results <= 0)
  {
    athame_set_failure("Vim timed out.");
    return 1;
  }

  read(from_vim, athame_buffer, 5);
  athame_buffer[5] = 0;
  if(strcmp(athame_buffer, "Error") == 0)
  {
    athame_set_failure("Couldn't load vim");
    return 1;
  }
  return 0;
}

static int athame_wait_for_file(char* file_name, int sanity)
{
  //Check for existance of a file to see if we have advanced that far
  FILE* theFile = 0;
  theFile = fopen(file_name, "r");
  while (!theFile)
  {
    if(sanity-- < 0)
    {
      return 1;
    }
    athame_sleep(50);
    theFile = fopen(file_name, "r");
  }
  fclose(theFile);
  return 0;
}

static int athame_remote_expr(char* expr, int block)
{
  int stdout_to_readline[2];
  int stderr_to_readline[2];
  int use_pipe = 0;
  if(block && !cs_confirmed)
  {
    use_pipe = 1;
    pipe(stdout_to_readline);
    pipe(stderr_to_readline);
  }
  //wait for last remote_expr to finish
  if (expr_pid != 0)
  {
    if (waitpid(expr_pid, NULL, block?0:WNOHANG) == 0)
    {
      return -1;
    }
  }

  expr_pid = fork();
  if (expr_pid == 0)
  {
    if(use_pipe)
    {
      dup2(stdout_to_readline[1], STDOUT_FILENO);
      dup2(stderr_to_readline[1], STDERR_FILENO);
      close(stdout_to_readline[0]);
      close(stderr_to_readline[0]);
    }
    else
    {
      dup2(fileno(dev_null), STDOUT_FILENO);
      dup2(fileno(dev_null), STDERR_FILENO);
    }

    execlp("vim", "vim", "--servername", servername, "--remote-expr", expr, NULL);
    printf("Expr Error:%d", errno);
    exit (EXIT_FAILURE);
  }
  else if (expr_pid == -1)
  {
    //TODO: error handling
    if(use_pipe)
    {
      close(stdout_to_readline[0]);
      close(stderr_to_readline[0]);
      close(stdout_to_readline[1]);
      close(stderr_to_readline[1]);
    }
    athame_set_failure("Clientserver error");
    return -1;
  }
  else
  {
    if(block)
    {
      waitpid(expr_pid, NULL, 0);
    }
    if(use_pipe)
    {
      close(stdout_to_readline[1]);
      close(stderr_to_readline[1]);
      fd_set streams;
      FD_ZERO(&streams);
      FD_SET(stdout_to_readline[0], &streams);
      FD_SET(stdout_to_readline[0], &streams);
      if (select(MAX(stdout_to_readline[0], stderr_to_readline[1])+1, &streams, NULL, NULL, NULL) > 0)
      {
        if(FD_ISSET(stderr_to_readline[0], &streams))
        {
          athame_set_failure("Clientserver error");
          return -1;
        }
        if(FD_ISSET(stdout_to_readline[0], &streams))
        {
          cs_confirmed = 1;
        }
      }
      close(stdout_to_readline[0]);
      close(stderr_to_readline[0]);
    }
    return 0;
  }
}

static void athame_update_vimline(int row, int col)
{
  FILE* contentsFile = fopen(contents_file_name, "r");
  FILE* updateFile = fopen(update_file_name, "w+");

  int reading_row = 0;
  while (fgets(athame_buffer, DEFAULT_BUFFER_SIZE, contentsFile))
  {
    if(row != reading_row)
    {
      fwrite(athame_buffer, 1, strlen(athame_buffer), updateFile);
    }
    else
    {
      fwrite(ap_get_line_buffer(), 1, ap_get_line_buffer_length(), updateFile);
      fwrite("\n", 1, 1, updateFile);
    }
    reading_row++;
  }
  fclose(contentsFile);
  fclose(updateFile);
  snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Vimbed_UpdateText(%d, %d, %d, %d, 0)", row+1, col+1, row+1, col+1);

  athame_remote_expr(athame_buffer, 1);
  updated = 1;
}

static void athame_poll_vim(int block)
{
  //Poll Vim. If we fail, postpone the poll by setting needs_poll to true
  needs_poll = (athame_remote_expr("Vimbed_Poll()", block) != 0);
}

int last_bdisplay_top = 0;
int last_bdisplay_bottom = 0;

static void athame_bottom_display(char* string, int style, int color, int cursor)
{
    int term_height = ap_get_term_height();
    int term_width = ap_get_term_width();
    if(!last_bdisplay_bottom)
    {
      last_bdisplay_top = term_height;
      last_bdisplay_bottom = term_height;
    }

    int temp = ap_get_cursor();
    if(!athame_dirty) {
      ap_set_cursor_end();
      ap_display();
    }

    int extra_lines = ((int)strlen(string)) / term_width;
    int i;
    for(i = 0; i < extra_lines; i++)
    {
      fprintf(athame_outstream, "\e[B");
    }

    char colorstyle[64];
    if(color)
    {
      sprintf(colorstyle, "\e[%d;%dm", style, color);
    }
    else
    {
      sprintf(colorstyle, "\e[%dm", style);
    }

    char erase[64];
    if (term_height != last_bdisplay_bottom)
    {
      //We've been resized and have no idea where the last bottom display is. Clear everything after the current text.
      sprintf(erase, "\e[J");
    }
    else if (!athame_dirty && ap_get_line_char_length() + ap_get_prompt_length() >= term_width) {
      //Delete text in the way on my row and bottom display but leave everything else alone
      sprintf(erase, "\e[K\e[%d;1H\e[J", last_bdisplay_top);
    }
    else {
      sprintf(erase, "\e[%d;1H\e[J", last_bdisplay_top);
    }

    char cursor_code[64];
    if (cursor)
    {
      char cursor_char = string[MIN(cursor, strlen(string))];
      if (!cursor_char){
        cursor_char = ' ';
      }
      sprintf(cursor_code, "\e[%d;%dH\e[7m%c", term_height-extra_lines + cursor/term_width, cursor%term_width + 1, cursor_char);
    }
    else
    {
      cursor_code[0] = '\0';
    }

    //\e[s\n\e[u\e[B\e[A            Add a line underneath if at bottom
    //\e[s                          Save cursor position
    //%s(erase)                     Delete old athame_bottom_display
    //\e[%d;1H                      Go to position for new athame_bottom_display
    //%s(colorstyle)%s(string) Write bottom display using given color/style
    //%s                            Draw cursor for command mode
    //\e[0m                         Reset style
    //\e[u                          Return to saved position
    fprintf(athame_outstream, "\e[s\n\e[u\e[B\e[A\e[s%s\e[%d;1H%s%s%s\e[0m\e[u", erase, term_height-extra_lines, colorstyle, string, cursor_code);

    for(i = 0; i < extra_lines; i++)
    {
      fprintf(athame_outstream, "\e[A");
    }

    last_bdisplay_bottom = term_height;
    last_bdisplay_top = term_height - extra_lines;

    fflush(athame_outstream);
    if(!athame_dirty) {
      ap_set_cursor(temp);
      ap_display();
    }
}

static int athame_clear_dirty()
{
  if (!athame_dirty){
    return 0;
  }

  int count = athame_dirty;
  while(athame_dirty)
  {
    if (athame_dirty == count)
    {
      //Avoid deleting prompt
      fprintf(athame_outstream, "\e[%dG\e[K\n", ap_get_prompt_length() + 1);
    }
    else
    {
      fprintf(athame_outstream, "\e[2K\n");
    }
    athame_dirty--;
  }
  while(count)
  {
    fprintf(athame_outstream, "\e[A");
    count--;
  }
  fflush(athame_outstream);
  return 1;
}

static void athame_redisplay()
{
  if (strcmp(athame_mode, "v") == 0 || strcmp(athame_mode, "V") == 0 || strcmp(athame_mode, "s") == 0 || strcmp(athame_mode, "c") == 0)
  {
    // Athame_highlight assumes the cursor is on the current terminal row. If we came
    // from normal readline display, that might not be the case
    if(!athame_clear_dirty())
    {
      int temp = ap_get_cursor();
      ap_set_cursor(0);
      ap_display();
      ap_set_cursor(temp);
    }
    athame_highlight(ap_get_cursor(), end_col);

  }
  else
  {
    if(athame_clear_dirty()){
      fprintf(athame_outstream, "\e[%dG", ap_get_prompt_length() + 1);
      fflush(athame_outstream);
      ap_display();
    }
    else
    {
      ap_display();
    }
  }
}

static char* athame_copy_w_space(char* text)
{
  int len = strlen(text);
  text[len] = ' ';
  char* ret = strndup(text, len + 1);
  text[len] = '\0';
  return ret;
}

static int athame_draw_line_with_highlight(char* text, int start, int end)
{
  int prompt_len = ap_get_prompt_length();
  int term_width = ap_get_term_width();
  //How much more than one line does the text take up (if it wraps)
  int extra_lines = ((int)strlen(text) + prompt_len - 1)/term_width;
  //How far down is the start of the highlight (if text wraps)
  int extra_lines_s = (start + prompt_len) /term_width;
  //How far down is the end of the highlight (if text wraps)
  int extra_lines_e = (end + prompt_len - 1) /term_width;

  char* with_space = athame_copy_w_space(text);
  char* highlighted = ap_get_slice(with_space, start, end);
  free(with_space);

  fprintf(athame_outstream, "\e[%dG%s", prompt_len + 1, text);
  if (extra_lines - extra_lines_s) {
    fprintf(athame_outstream, "\e[%dA", extra_lines - extra_lines_s);
  }

  fprintf(athame_outstream, "\e[%dG\e[7m%s\e[0m", (prompt_len + start) % term_width + 1, highlighted);

  free(highlighted);

  if (extra_lines - extra_lines_e){
    fprintf(athame_outstream, "\e[%dB", extra_lines - extra_lines_e);
  }
  fprintf(athame_outstream, "\n");
  return extra_lines + 1;
}

static int athame_highlight(int start, int end)
{
  char* highlight_buffer = strdup(ap_get_line_buffer());
  int cursor = ap_get_cursor();
  ap_set_line_buffer("");
  ap_display();
  ap_set_line_buffer(highlight_buffer);
  ap_set_cursor(cursor);
  char* new_string = strtok(highlight_buffer, "\n");
  while (new_string){
    char* next_string;
    int this_start;
    int this_end;
    next_string = strtok(NULL, "\n");

    if (new_string == highlight_buffer){
      this_start = start;
    }
    else {
      this_start = 0;
    }
    if (next_string || end == 0 || (end <= start && new_string == highlight_buffer)){
      this_end = strlen(new_string) + 1;
    }
    else {
      this_end = end;
    }
    athame_dirty += athame_draw_line_with_highlight(new_string, this_start, this_end);
    new_string = next_string;
  }
  free(highlight_buffer);
  if(athame_dirty)
  {
    fprintf(athame_outstream, "\e[%dA", athame_dirty);
  }
  fflush(athame_outstream);
}

int athame_is_set(char* env, int def)
{
  char* env_val = getenv(env);
  if (!env_val)
  {
    setenv(env, def?"1":"0", 0);
    return def;
  }
  else {
    return env_val[0] == '1';
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
    return 1;
  }
  else
  {
    return 0;
  }
}

char athame_loop(int instream)
{
  char returnVal = 0;
  if (first_char && strchr("\n\r", first_char) != 0)
  {
    return first_char;
  }

  if(tab_fix)
  {
    //Delete the space we just sent to get completion working.
    tab_fix = 0;
    updated = 1; //We don't want to override the actual vim change.
    return '\b';
  }

  sent_to_vim = 0;

  if(!updated && !athame_failure)
  {
    athame_update_vimline(athame_row, ap_get_cursor());
  }
  else
  {
    athame_redisplay();
  }


  while(!returnVal)
  {
    athame_bottom_mode();
    athame_redisplay();

    struct timeval timeout;
    int results = 0;
    if(first_char){
      returnVal = athame_process_char(first_char);
      first_char = 0;
    }
    else {
      while(results == 0)
      {
        if(athame_failure)
        {
          fd_set files;
          FD_ZERO(&files);
          FD_SET(instream, &files);
          results = select(instream+1, &files, NULL, NULL, NULL);
          if (results > 0)
          {
            returnVal = athame_process_input(instream);
          }
        }
        else
        {
          fd_set files;
          FD_ZERO(&files);
          FD_SET(instream, &files);
          FD_SET(from_vim, &files);
          timeout.tv_sec = 0;
          timeout.tv_usec = 500 * 1000;

          results = select(MAX(from_vim, instream)+1, &files, NULL, NULL, (strcmp(athame_mode, "c") == 0 || needs_poll)? &timeout : NULL);
          if (waitpid(vim_pid, NULL, WNOHANG) == 0) //Is vim still running?
          {
            if (results > 0)
            {
              if(FD_ISSET(instream, &files)){
                returnVal = athame_process_input(instream);
              }
              else if(!athame_failure && FD_ISSET(from_vim, &files)){
                athame_get_vim_info();
              }
            }
            else
            {
              char sig_result;
              if (sig_result = ap_handle_signals())
              {
                return sig_result;
              }
              if(needs_poll)
              {
                athame_poll_vim(0);
              }
              else
              {
                athame_get_vim_info_inner(0);
              }
            }
          }
          else
          {
            //Vim quit
            if(sent_to_vim)
            {
              ap_set_line_buffer("");
              return '\x04'; //<C-D>
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
    }
    if (ap_needs_to_leave()) //We need to leave now
    {
       return '\0';
    }
  }
  if(!athame_failure)
  {
    if(returnVal != ' ' && returnVal != '\b'){
      if(sent_to_vim)
      {
        if(strcmp(athame_mode, "i") == 0)
        {
          athame_send_to_vim('\x1d'); //<C-]> Finish abbrevs/kill mappings
        }
        athame_extraVimRead(100);
      }
      if (athame_is_set("ATHAME_SHOW_MODE", 1))
      {
        athame_bottom_display("", BOLD, DEFAULT, 0);
      }
    }
    updated = 0;
    athame_displaying_mode[0] = 'n';
    athame_displaying_mode[1] = '\0';
  }
  else if (strchr("\n\r\t", returnVal))
  {
    //Hide failure messae where it might mess with something
    athame_bottom_display("", BOLD, DEFAULT, 0);
  }
  return returnVal;
}

static char* athame_get_mode_text(char* mode)
{
    switch(mode[0])
    {
      case 'i': return "INSERT"; break;
      case 'n': return "NORMAL"; break;
      case 'v': return "VISUAL"; break;
      case 'V': return "VISUAL LINE"; break;
      case 's': return "SELECT"; break;
      case 'S': return "SELECT LINE"; break;
      case 'R': return "REPLACE"; break;
      case 'c': return "COMMAND"; break;
      default: return "";
    }
}

static void athame_bottom_mode()
{
  if(athame_failure)
  {
    return;
  }
  static int text_lines = 0;
  int new_text_lines = (ap_get_line_char_length() + ap_get_prompt_length() - 1)/ap_get_term_width();
  int force_redraw = new_text_lines != text_lines || athame_dirty;
  if (strcmp(athame_mode, athame_displaying_mode) != 0 || force_redraw) {
    char* mode_string = athame_get_mode_text(athame_mode);
    setenv("ATHAME_VIM_MODE", mode_string, 1);
    if (strcmp(athame_mode, athame_displaying_mode) != 0)
    {
      strcpy(athame_displaying_mode, athame_mode);
      ap_redraw_prompt();
      // Redraw can hide the vim command, so we have to redraw it.
      if (athame_mode[0] == 'c' && athame_is_set("ATHAME_SHOW_COMMAND", 1))
      {
        athame_bottom_display(last_vim_command, NORMAL, DEFAULT, last_cmd_pos);
      }
    }
    if (athame_is_set("ATHAME_SHOW_MODE", 1))
    {
      if (athame_mode[0] == 'n')
      {
        athame_bottom_display("", BOLD, DEFAULT, 0);
      }
      else if(athame_mode[0] != 'c' && mode_string[0])
      {
        sprintf(athame_buffer, "-- %s --", mode_string);
        athame_bottom_display(athame_buffer, BOLD, DEFAULT, 0);
      }
    }
  }
  text_lines = new_text_lines;
}

static void athame_extraVimRead(int timer)
{
  fd_set files;
  FD_ZERO(&files);
  FD_SET(from_vim, &files);
  int results;
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = timer * 250;
  athame_sleep(timer * 3 / 4);
  results = select(from_vim + 1, &files, NULL, NULL, &timeout);
  if (results > 0)
    athame_get_vim_info();
  athame_mode[0] = 'n';
  athame_mode[1] = '\0';
  athame_redisplay();
}

static void athame_draw_failure()
{
  setenv("ATHAME_ERROR", athame_failure, 1);
  if (athame_is_set("ATHAME_SHOW_ERROR", 1))
  {
    snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Athame Failure: %s", athame_failure);
    athame_bottom_display(athame_buffer, BOLD, RED, 0);
  }
}

static void athame_set_failure(char* fail_str)
{
  athame_failure = fail_str;
  athame_draw_failure();
}

static char athame_process_input(int instream)
{
  char result;
  int chars_read = read(instream, &result, 1);
  if (chars_read == 1)
  {
    return athame_process_char(result);
  }
  else
  {
    return EOF;
  }
}

static char athame_process_char(char char_read){
  //Unless in vim commandline send return/tab/<C-D>/<C-L> to readline instead of vim
  if(athame_failure || (strchr("\n\r\t\x04\x0c", char_read) && strcmp(athame_mode, "c") != 0 ))
  {
    if(athame_failure)
    {
      athame_draw_failure();
      athame_sleep(5);
    }

    last_tab = (char_read == '\t');
    if (char_read == '\t') {
      return '\t';
    }
    return char_read;
  }
  else
  {
    sent_to_vim = 1;

    //Backspace
    if (char_read == '\177'){
      char_read = '\b';
    }
    athame_send_to_vim(char_read);
    if(last_tab)
    {
      last_tab = 0;
      tab_fix = 1;
      return ' ';
    }
    return 0;
  }
}

static void athame_send_to_vim(char input)
{
  write(to_vim, &input, 1);
}

static void athame_get_vim_info()
{
  if (!athame_get_vim_info_inner(1))
  {
    athame_poll_vim(0);
  }
}

static int athame_get_col_row(char* string, int* col, int* row)
{
    if(string && strtok(string, ","))
    {
      char* colStr = strtok(NULL, ",");
      if(!colStr){
        return 0;
      }
      *col = strtol(colStr, NULL, 10);
      if(row)
      {
        char* rowStr = strtok(NULL, ",");
        if(!rowStr){
          return 0;
        }
        *row = strtol(rowStr, NULL, 10);
      }
      return 1;
    }
    return 0;
}

static int athame_get_vim_info_inner(int read_pipe)
{
  int changed = 0;
  ssize_t bytes_read;
  if(read_pipe)
  {
    read(from_vim, athame_buffer, DEFAULT_BUFFER_SIZE-1);
  }

  if(athame_failure)
  {
    return 1;
  }

  FILE* metaFile = fopen(meta_file_name, "r");
  if (!metaFile)
  {
    return 0;
  }
  bytes_read = fread(athame_buffer, 1, DEFAULT_BUFFER_SIZE-1, metaFile);
  fclose(metaFile);

  athame_buffer[bytes_read] = 0;
  char* mode = strtok(athame_buffer, "\n");
  if(mode)
  {
    if (mode[0] == 'c')
    {
      strncpy(athame_mode, "c", 3);
      char* command = strtok(NULL, "\n");
      int cmd_pos = last_cmd_pos;
      if (mode[1] == ',')
      {
        cmd_pos = strtol(mode+2, NULL, 10);
      }
      if (command && (strcmp(command, last_vim_command) != 0 || cmd_pos != last_cmd_pos))
      {
        last_cmd_pos = cmd_pos;
        strncpy(last_vim_command, command, DEFAULT_BUFFER_SIZE-1);
        last_vim_command[DEFAULT_BUFFER_SIZE-1] = '\0';
        setenv("ATHAME_VIM_COMMAND", command, 1);
        if (athame_is_set("ATHAME_SHOW_COMMAND", 1))
        {
          athame_bottom_display(command, NORMAL, DEFAULT, cmd_pos);
        }
        //Don't record a change because the highlight for incsearch might not have changed yet.
      }
    }
    else
    {
      if(athame_mode[0] == 'c' && athame_is_set("ATHAME_SHOW_COMMAND", 1))
      {
        athame_bottom_display("", NORMAL, DEFAULT, 0);
      }
      strncpy(athame_mode, mode, 3);
      last_vim_command[0] = '\0';
    }
    char* location = strtok(NULL, "\n");
    if(location)
    {
      char* location2 = strtok(NULL, "\n");
      int col;
      int row;
      if(athame_get_col_row(location, &col, &row))
      {
        if(col < 0 || row < 0){
          col = 0;
          location2 = NULL;
        }

        int new_end_col;
        int new_end_row;
        if(athame_get_col_row(location2, &new_end_col, &new_end_row))
        {
          if(new_end_col != end_col || new_end_row != end_row)
          {
            end_col = new_end_col;
            end_row = new_end_row;
            changed = 1;
          }
        }
        else
        {
          end_col = col + 1;
          end_row = row;
        }

        if(athame_row != row)
        {
          athame_row = row;
          changed = 1;
        }

        char* line_text;
        int subtract_line = (end_row > row && end_col == 0 ? 1 : 0);
        line_text = athame_get_lines_from_vim(row, end_row - subtract_line);

        if (line_text)
        {
          if(strcmp(ap_get_line_buffer(), line_text) != 0)
          {
            changed = 1;
            ap_set_line_buffer(line_text);
          }
          if(ap_get_cursor() != col)
          {
            ap_set_cursor(col);
            changed = 1;
          }
          //Success
          return changed;
        }
        else
        {
          ap_set_line_buffer("");
        }
      }
    }
  }

  return changed;
}

static char* athame_get_lines_from_vim(int start_row, int end_row)
{
  FILE* contentsFile = fopen(contents_file_name, "r");
  int bytes_read = fread(athame_buffer, 1, DEFAULT_BUFFER_SIZE-1, contentsFile);
  athame_buffer[bytes_read] = 0;

  int current_line = 0;
  char* line_text = athame_buffer;
  while (current_line < start_row )
  {
    line_text = strchr(line_text, '\n');
    while(!line_text)
    {
      int bytes_read = fread(athame_buffer, 1, DEFAULT_BUFFER_SIZE-1, contentsFile);
      if (!bytes_read)
      {
        return 0;
      }
      athame_buffer[bytes_read] = 0;
      line_text = strchr(athame_buffer, '\n');
    }
    line_text++;
    current_line++;
  }
  int len = strlen(line_text);
  memmove(athame_buffer, line_text, len + 1);

  len += fread(athame_buffer + len, 1, DEFAULT_BUFFER_SIZE - 1 - len, contentsFile);
  athame_buffer[len] = 0;
  char* next_line = athame_buffer;
  for(current_line = start_row; current_line <= end_row; current_line++)
  {
    next_line = strchr(next_line, '\n');
    if(!next_line){
      break;
    }
    next_line++;
  }

  if(next_line)
  {
    *(next_line - 1) = '\0';
  }
  fclose(contentsFile);

  return athame_buffer;
}
