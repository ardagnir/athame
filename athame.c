/* athame.c -- Full vim integration for readline.*/

/* Copyright (C) 2015 James Kolb

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

#define READLINE_LIBRARY

#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "history.h"
#include "athame.h"
#include "readline.h"

#define DEFAULT_BUFFER_SIZE 1024
#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

#define NORMAL 0
#define BOLD 1

#define DEFAULT 0
#define RED 31

static char athame_buffer[DEFAULT_BUFFER_SIZE];
static char last_vim_command[DEFAULT_BUFFER_SIZE];
static const char* athame_fail_str;
static int vim_pid;
static int expr_pid = 0;
static int vim_to_readline[2];
static int readline_to_vim[2];
static int from_vim;
static int to_vim;
static FILE* dev_null;
static int athame_row;
static int updated;
static char contents_file_name[64];
static char update_file_name[64];
static char meta_file_name[64];
static char messages_file_name[64];
static char vimbed_file_name[256];
static char dir_name[64];
static char servername[32];
static int sent_to_vim = 0;
static int needs_poll = 0;

 //Keep track of if last key was a tab. We need to fake keys between tabpresses or readline completion gets confused.
static int last_tab = 0;
static int tab_fix = 0; //We just sent a fake space to help completion work. Now delete it.

static char athame_mode[3] = {'n', '\0', '\0'};
static char athame_displaying_mode[3] = {'n', '\0', '\0'};
static int end_col; //For visual mode
static int end_row; //For visual mode

static int athame_failed;

static int athame_dirty = 0;

static char first_char;

/* Forward declarations used in this file. */
static void athame_send_to_vim(char input);
static void athame_get_vim_info();
static char athame_process_input(int instream);
static char athame_process_char(char instream);
static void athame_extraVimRead(int timer);
static int athame_update_vim(int col);
static char* athame_get_lines_from_vim(int start_row, int end_row);
static void athame_sleep(int msec);
static int athame_get_vim_info_inner(int read_pipe);
static void athame_update_vimline(int row, int col);
static int athame_remote_expr(char* expr, int bock);
static void athame_bottom_display(char* string, int style, int color);
static int athame_wait_for_file();
static char athame_get_first_char();
static int athame_highlight(int start, int end);
static void athame_bottom_mode();
static int athame_term_height();
static int athame_term_width();

void athame_init()
{
  first_char = athame_get_first_char();
  if(first_char == '\r')
  {
    return;
  }
  //Note that this rand() is not seeded.by athame.
  //It only establishes uniqueness within a single process using readline.
  //The pid establishes uniqueness between processes and makes debugging easier.
  snprintf(servername, 31, "athame_%d_%d", getpid(), rand() % (1000000000));
  snprintf(dir_name, 63, "/tmp/vimbed/%s", servername);
  snprintf(contents_file_name, 63, "%s/contents.txt", dir_name);
  snprintf(update_file_name, 63, "%s/update.txt", dir_name);
  snprintf(meta_file_name, 63, "%s/meta.txt", dir_name);
  snprintf(messages_file_name, 63, "%s/messages.txt", dir_name);
  snprintf(vimbed_file_name, 255, "%s/athame/vimbed.vim", RL_LIBRARY_LOCATION);
  if(vimbed_file_name[254] != '\0')
  {
    athame_failed = 1;
    athame_fail_str = "Vimbed path too long.";
  }

  last_vim_command[0] = '\0';

  athame_failed = 0;

  mkdir("/tmp/vimbed", S_IRWXU);
  mkdir(dir_name, S_IRWXU);

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
    if (execlp("vim", "vim", "--servername", servername, "-S", vimbed_file_name, "-S", "~/.athamerc", "-s", "/dev/null", "+call Vimbed_SetupVimbed('', '')", NULL)!=0)
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

    athame_failed = athame_update_vim(0);
  }
  athame_bottom_display("--INSERT--", BOLD, DEFAULT);
}

void athame_cleanup()
{
  if(first_char != '\r')
  {
    close(to_vim);
    close(from_vim);
    fclose(dev_null);
    kill(vim_pid);
    unlink(contents_file_name);
    unlink(update_file_name);
    unlink(meta_file_name);
    unlink(messages_file_name);
    rmdir(dir_name);
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
  if(results){
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

static int athame_update_vim(int col)
{
  int failure = 1;
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
    athame_fail_str = "Vim timed out.";
    return failure;
  }

  read(from_vim, athame_buffer, 5);
  athame_buffer[5] = 0;
  if(strcmp(athame_buffer, "Error") == 0)
  {
    athame_fail_str = "Couldn't load vim";
    return failure;
  }

  FILE* updateFile = fopen(update_file_name, "w+");

  if(!updateFile){
    athame_fail_str = "Couldn't create temporary file in /tmp/vimbed";
    return failure;
  }

  //Send vim a return incase there was a warning (for example no ~/.athamerc)
  athame_send_to_vim('\r');

  //Vim's clientserver takes a while to start up. TODO: Figure out exactly when we can move forward.
  athame_sleep(100);

  HISTORY_STATE* hs = history_get_history_state();
  int extra_lines; //lines with 1 line in history that take include newlines
  int counter;
  for(counter = 0; counter < hs->length; counter++)
  {
    fwrite(hs->entries[counter]->line, 1, strlen(hs->entries[counter]->line), updateFile);
    char* line_search = hs->entries[counter]->line;
    while(line_search = strstr(line_search, "\n"))
    {
      line_search++;
      extra_lines++;
    }
    fwrite("\n", 1, 1, updateFile);
  }
  fwrite("\n", 1, 1, updateFile);
  fclose(updateFile);
  athame_row = hs->length + extra_lines;
  snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Vimbed_UpdateText(%d, %d, %d, %d, 0)", athame_row+1, col+1, athame_row+1, col+1);
  xfree(hs);

  //Wait for the metafile so we know vimbed has loaded
  if(athame_wait_for_file(meta_file_name))
  {
    athame_fail_str = "clientserver/vimbed error";
    return failure;
  }

  athame_remote_expr(athame_buffer, 1);

  //Wait for the contents_file so we know UpdateText has been handled
  if(athame_wait_for_file(contents_file_name))
  {
    athame_fail_str = "vimbed error";
    return failure;
  }

  updated = 1;
  return 0;
}

static int athame_wait_for_file(char* file_name)
{
  //Check for existance of a file to see if we have advanced that far
  FILE* theFile = 0;
  int sanity = 100;
  while (!theFile)
  {
    if(sanity-- < 0)
    {
      return 1;
    }
    athame_sleep(15);
    theFile = fopen(file_name, "r");
  }
  fclose(theFile);
  return 0;
}

static int athame_remote_expr(char* expr, int block)
{
  char remote_expr_buffer[DEFAULT_BUFFER_SIZE];

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
    dup2(fileno(dev_null), STDOUT_FILENO);
    dup2(fileno(dev_null), STDERR_FILENO);
    execlp("vim", "vim", "--servername", servername, "--remote-expr", expr, NULL);
    printf("Expr Error:%d", errno);
    exit (EXIT_FAILURE);
  }
  else if (expr_pid == -1)
  {
    //TODO: error handling
    perror("ERROR! Couldn't run vim remote_expr!");
  }
  if(block)
  {
    waitpid(expr_pid, NULL, 0);
  }
  return 0;
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
      fwrite(rl_line_buffer, 1, rl_end, updateFile);
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

static void athame_poll_vim()
{
  //Poll Vim. If we fail, postpone the poll by setting needs_poll to true
  needs_poll = (athame_remote_expr("Vimbed_Poll()", 0) != 0);
}

int last_bdisplay_top = 0;
int last_bdisplay_bottom = 0;

static void athame_bottom_display(char* string, int style, int color)
{
    int term_height = athame_term_height();
    if(!last_bdisplay_bottom)
    {
      last_bdisplay_top = term_height;
      last_bdisplay_bottom = term_height;
    }

    int temp = rl_point;
    if(!athame_dirty) {
      rl_point = 0;
      rl_redisplay();
    }

    int extra_lines = (strlen(string) - 1) / athame_term_width();
    int i;
    for(i = 0; i < extra_lines; i++)
    {
      printf("\n");
    }

    //Take into account both mutline commands and resizing.
    int erase_point = MIN(last_bdisplay_top, last_bdisplay_top - last_bdisplay_bottom + term_height);

    //\n\e[A:          Add a line underneath if at bottom
    //\e[s:            Save cursor position
    //\e[%d;1H\e[K     Delete old athame_bottom_display
    //\e[%d;1H         Go to position for new athame_bottom_display
    //\e[%d;%dm%s\e[0m Write bottom display using given color/style
    //\e[u             Return to saved position
    if (color)
    {
      printf("\n\e[A\e[s\e[%d;1H\e[J\e[%d;1H\e[%d;%dm%s\e[0m\e[u", erase_point, term_height-extra_lines, style, color, string);
    }
    else
    {
      printf("\n\e[A\e[s\e[%d;1H\e[J\e[%d;1H\e[%dm%s\e[0m\e[u", erase_point, term_height-extra_lines, style, string);
    }

    for(i = 0; i < extra_lines; i++)
    {
      printf("\e[A");
    }

    last_bdisplay_bottom = term_height;
    last_bdisplay_top = term_height - extra_lines;

    fflush(stdout);
    if(!athame_dirty) {
      rl_point = temp;
      rl_forced_update_display();
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
    printf("\e[2K\n");
    athame_dirty--;
  }
  while(count)
  {
    printf("\e[A");
    count--;
  }
  fflush(stdout);
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
      int temp = rl_point;
      rl_point = 0;
      rl_redisplay();
      rl_point = temp;
    }
    athame_highlight(rl_point, end_col);

  }
  else
  {
    if(athame_clear_dirty()){
      rl_forced_update_display();
    }
    else
    {
      rl_redisplay();
    }
  }
}

static int athame_term_height()
{
  int height, width;
  rl_get_screen_size(&height, &width);
  return height;
}

static int athame_term_width()
{
  int height, width;
  rl_get_screen_size(&height, &width);
  return width;
}

static int athame_draw_line_with_highlight(char* text, int start, int end)
{
  int prompt_len = rl_expand_prompt (rl_prompt);
  //How much more than one line does the text take up (if it wraps)
  int extra_lines = (strlen(text) + prompt_len - 1)/athame_term_width();
  //How far down is the start of the highlight (if text wraps)
  int extra_lines_s = (start + prompt_len) /athame_term_width();
  //How far down is the end of the highlight (if text wraps)
  int extra_lines_e = (end + prompt_len - 1) /athame_term_width();
  char highlighted[DEFAULT_BUFFER_SIZE];
  strncpy(highlighted, text+start, end-start);
  if(!highlighted[end - start - 1])
  {
    highlighted[end - start - 1] = ' ';
  }
  highlighted[end - start] = '\0';
  printf("\e[1G\e[%dC%s", prompt_len, text);
  if (extra_lines - extra_lines_s) {
    printf("\e[%dA", extra_lines - extra_lines_s);
  }

  if((prompt_len + start) % athame_term_width())
    printf("\e[1G\e[%dC\e[7m%s\e[0m", (prompt_len + start) % athame_term_width(), highlighted);
  else
    printf("\e[1G\e[7m%s\e[0m", highlighted);

  if (extra_lines - extra_lines_e){
    printf("\e[%dB", extra_lines - extra_lines_e);
  }
  printf("\n");
  return extra_lines + 1;
}

static int athame_highlight(int start, int end)
{
  char highlight_buffer[DEFAULT_BUFFER_SIZE];
  strncpy(highlight_buffer, rl_line_buffer, MIN(rl_end, DEFAULT_BUFFER_SIZE));
  highlight_buffer[MIN(rl_end, DEFAULT_BUFFER_SIZE)] = '\0';
  printf("\e[1G");
  fflush(stdout);
  int temp = rl_end;
  rl_line_buffer[MIN(DEFAULT_BUFFER_SIZE - 1, rl_end + 1)] = '\0';
  rl_end = 0;
  rl_forced_update_display();
  rl_end = temp;
  char * new_string = strtok(highlight_buffer, "\n");
  while (new_string){
    char * next_string;
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
  if(athame_dirty)
  {
    printf("\e[%dA", athame_dirty);
  }
  fflush(stdout);
}


char athame_loop(int instream)
{
  char returnVal = 0;
  if(first_char == '\r')
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

  if(!updated && !athame_failed)
  {
    athame_update_vimline(athame_row, rl_point);
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
        if(athame_failed)
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
              else if(!athame_failed && FD_ISSET(from_vim, &files)){
                athame_get_vim_info();
              }
            }
            else
            {
              if(needs_poll)
              {
                athame_poll_vim();
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
              rl_line_buffer[0] = '\0';
              rl_end = 0;
              return '\x04'; //<C-D>
            }
            else
            {
              // If we didn't send anything to vim, it shouldn't have quit.
              // We never want to kill the user's shell without giving them a chance
              // to type anything.
              athame_fail_str = "Vim quit";
              athame_failed = 1;
            }
          }
        }
      }
    }
  }
  if(!athame_failed)
  {
    if(sent_to_vim)
    {
      if(strcmp(athame_mode, "i") == 0)
      {
        athame_send_to_vim('\x1d'); //<C-]> Finish abbrevs/kill mappings
      }
      athame_extraVimRead(100);
    }
    updated = 0;
    //Hide bottom display if we leave athame for realsies, but not for the space/delete hack
    if(returnVal != ' ' && returnVal != '\b'){
      athame_bottom_display("", BOLD, DEFAULT);
    }
    athame_displaying_mode[0] = 'n';
    athame_displaying_mode[1] = '\0';
  }
  return returnVal;
}

static void athame_bottom_mode()
{
  if (strcmp(athame_mode, athame_displaying_mode) != 0 && !athame_failed) {
    strcpy(athame_displaying_mode, athame_mode);
    if (strcmp(athame_mode, "i") == 0)
    {
      athame_bottom_display("--INSERT--", BOLD, DEFAULT);
    }
    else if (strcmp(athame_mode, "v") == 0)
    {
      athame_bottom_display("--VISUAL--", BOLD, DEFAULT);
    }
    else if (strcmp(athame_mode, "V") == 0)
    {
      athame_bottom_display("--VISUAL LINE--", BOLD, DEFAULT);
    }
    else if (strcmp(athame_mode, "s") == 0)
    {
      athame_bottom_display("--SELECT--", BOLD, DEFAULT);
    }
    else if (strcmp(athame_mode, "R") == 0)
    {
      athame_bottom_display("--REPLACE--", BOLD, DEFAULT);
    }
    else if (strcmp(athame_mode, "c") !=0)
    {
      athame_bottom_display("", BOLD, DEFAULT);
    }
  }
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
  if(athame_failed || (strchr("\r\t\x04\x0c", char_read) && strcmp(athame_mode, "c") != 0 ))
  {
    if(athame_failed)
    {
      snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Athame Failure: %s", athame_fail_str);
      athame_bottom_display(athame_buffer, BOLD, RED);
      athame_sleep(5);
    }

    last_tab = (char_read == '\t');
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
    athame_poll_vim();
  }
}

static int athame_get_vim_info_inner(int read_pipe)
{
  int changed = 0;
  ssize_t bytes_read;
  if(read_pipe)
  {
    read(from_vim, athame_buffer, DEFAULT_BUFFER_SIZE-1);
  }

  if(athame_failed)
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
    strncpy(athame_mode, mode, 3);
    if (strcmp(athame_mode, "c") == 0)
    {
      char* command = strtok(NULL, "\n");
      if (command && strcmp(command, last_vim_command) != 0)
      {
        strncpy(last_vim_command, command, DEFAULT_BUFFER_SIZE-1);
        last_vim_command[DEFAULT_BUFFER_SIZE-1] = '\0';
        athame_bottom_display(command, NORMAL, DEFAULT);
        //Don't record a change because the highlight for incsearch might not have changed yet.
      }
    }
    else
    {
      last_vim_command[0] = '\0';
    }
    char* location = strtok(NULL, "\n");
    if(location)
    {
      char* location2 = strtok(NULL, "\n");
      if(strtok(location, ","))
      {
        char* colStr = strtok(NULL, ",");
        if(!colStr){
          return 0;
        }
        char* rowStr = strtok(NULL, ",");
        if(!rowStr){
          return 0;
        }
        int col = strtol(colStr, NULL, 10);
        int row = strtol(rowStr, NULL, 10);

        if(col < 0 || row < 0){
          col = 0;
          location2 = NULL;
        }

        if(location2 && strtok(location2, ","))
        {
          int new_end_col;
          int new_end_row;
          colStr = strtok(NULL, ",");
          if(!colStr)
          {
            return 0;
          }
          new_end_col = strtol(colStr, NULL, 10);
          rowStr = strtok(NULL, ",");
          if(!rowStr)
          {
            return 0;
          }
          new_end_row = strtol(rowStr, NULL, 10);
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

        //TODO: This command check is a temporary hack
        char* line_text;
        int subtract_line = (end_row > row && end_col == 0 ? 1 : 0);
        line_text = athame_get_lines_from_vim(row, end_row - subtract_line);

        if (line_text)
        {
          if(strcmp(rl_line_buffer, line_text) != 0)
          {
            changed = 1;
            strcpy(rl_line_buffer, line_text);
          }
          rl_end = strlen(line_text);
          if(rl_point != col)
          {
            rl_point = col;
            changed = 1;
          }
          //Success
          return changed;
        }
        else
        {
          rl_end = 0;
          rl_point = 0;
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
  int failure = 0;
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
