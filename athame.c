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

static char athame_buffer[DEFAULT_BUFFER_SIZE];
static char last_vim_command[DEFAULT_BUFFER_SIZE];
int vim_pid;
int expr_pid = 0;
int vim_to_readline[2];
int readline_to_vim[2];
int from_vim;
int to_vim;
FILE* dev_null;
int athame_row;
int updated;
char contents_file_name[64];
char meta_file_name[64];
char messages_file_name[64];
char temp_file_name[64];
char dir_name[64];
char servername[32];
int key_pressed = 0;
int needs_poll = 0;

 //Keep track of if last key was a tab. We need to fake keys between tabpresses or readline completion gets confused.
int last_tab = 0;
int tab_fix = 0; //We just sent a fake space to help completion work. Now delete it.

char athame_mode[3] = {'n', '\0', '\0'};
char athame_displaying_mode[3] = {'n', '\0', '\0'};
int end_col; //For visual mode
int end_row; //For visual mode

int athame_failed;

int athame_dirty = 0;

char first_char;

void athame_init()
{
  first_char = get_first_char();
  if(first_char == '\r')
  {
    return;
  }
  //Note that this rand() is not seeded.by athame.
  //It only establishes uniqueness within a single process using readline.
  //The pid establishes uniqueness between processes and makes debugging easier.
  snprintf(servername, 32, "athame_%d_%d", getpid(), rand() % (1000000000));
  snprintf(dir_name, 64, "/tmp/vimbed/%s", servername);
  snprintf(contents_file_name, 64, "%s/contents.txt", dir_name);
  snprintf(meta_file_name, 64, "%s/meta.txt", dir_name);
  snprintf(messages_file_name, 64, "%s/messages.txt", dir_name);
  snprintf(temp_file_name, 64, "%s/temp.txt", dir_name);

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
    if (execl("/usr/bin/vim", "/usr/bin/vim", "--servername", servername, "-S", "~/.athamerc", "-s", "/dev/null", "+call Vimbed_SetupVimbed('', '')", NULL)!=0)
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
  athame_bottom_display("--INSERT--", 1);
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
    unlink(meta_file_name);
    unlink(messages_file_name);
    unlink(temp_file_name);
    rmdir(dir_name);
  }
}

char get_first_char()
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

void athame_sleep(int msec)
{
    struct timespec timeout, timeout2;
    timeout.tv_sec = 0;
    timeout.tv_nsec = msec * 1000000;
    while (nanosleep(&timeout, &timeout2)<0)
    {
      timeout = timeout2;
    }
}

int athame_update_vim(int col)
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
    printf("Athame Failure: Pipe");
    return failure;
  }

  read(from_vim, athame_buffer, 5);
  athame_buffer[5] = 0;
  if(strcmp(athame_buffer, "Error") == 0)
  {
    printf("Athame Failure: Found Error string");
    return failure;
  }

  FILE* contentsFile = fopen(contents_file_name, "w+");

  if(!contentsFile){
    printf("Athame Failure: No contents file");
    return failure;
  }

  HISTORY_STATE* hs = history_get_history_state();
  int counter;
  for(counter = 0; counter < hs->length; counter++)
  {
    fwrite(hs->entries[counter]->line, 1, strlen(hs->entries[counter]->line), contentsFile);
    fwrite("\n", 1, 1, contentsFile);
  }
  fwrite("\n", 1, 1, contentsFile);
  fclose(contentsFile);
  athame_row = hs->length;
  snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Vimbed_UpdateText(%d, %d, %d, %d, 0)", hs->length+1, col+1, hs->length+1, col+1);
  xfree(hs);

  if(wait_for_vimbed())
  {
    return failure;
  }

  athame_remote_expr(athame_buffer, 1);
  updated = 1;
  return 0;
}

int wait_for_vimbed()
{
  //Check for existance of metafile to see if vimbed has loaded.
  FILE* metaFile = 0;
  int sanity = 100;
  while (!metaFile)
  {
    if(sanity-- < 0)
    {
      return 1;
    }
    athame_sleep(15);
    metaFile = fopen(meta_file_name, "r+");
  }
  fclose(metaFile);
  return 0;
}

int athame_remote_expr(char* expr, int block)
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
    //This is faster, but we it's managing to mess up the terminal, even with stdout/stderr redirrected.
    //snprintf(remote_expr_buffer, DEFAULT_BUFFER_SIZE-1, "+call remote_expr('%s', '%s')", servername, expr);
    //execl ("/usr/bin/vim", "/usr/bin/vim", remote_expr_buffer, "+q!", "-u", "NONE", "-v", "-s", "/dev/null", NULL);
    execl ("/usr/bin/vim", "/usr/bin/vim", "--servername", servername, "--remote-expr", expr, NULL);
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

void athame_update_vimline(int row, int col)
{
  FILE* contentsFile = fopen(contents_file_name, "r");
  FILE* tempFile = fopen(temp_file_name, "w+");

  int reading_row = 0;
  while (fgets(athame_buffer, DEFAULT_BUFFER_SIZE, contentsFile))
  {
    if(row != reading_row)
    {
      fwrite(athame_buffer, 1, strlen(athame_buffer), tempFile);
    }
    else
    {
      fwrite(rl_line_buffer, 1, rl_end, tempFile);
      fwrite("\n", 1, 1, tempFile);
    }
    reading_row++;
  }
  fclose(contentsFile);
  fclose(tempFile);
  rename(temp_file_name, contents_file_name);
  snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Vimbed_UpdateText(%d, %d, %d, %d, 0)", row+1, col+1, row+1, col+1);

  athame_remote_expr(athame_buffer, 1);
  updated = 1;
}

void athame_poll_vim()
{
  //Poll Vim. If we fail, postpone the poll by setting needs_poll to true
  needs_poll = (athame_remote_expr("Vimbed_Poll()", 0) != 0);
}

void athame_bottom_display(char* string, int style)
{
    int temp = rl_point;
    rl_point = 0;
    rl_redisplay();
    printf("\n\e[A\e[s\e[999E\e[K\e[%dm%s\e[0m\e[u", style, string);
    fflush(stdout);
    rl_point = temp;
    rl_forced_update_display();
}

int athame_clear_dirty()
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

void athame_redisplay()
{
  if (strcmp(athame_mode, "v") == 0 || strcmp(athame_mode, "V") == 0 || strcmp(athame_mode, "s") == 0 || strcmp(athame_mode, "c") == 0)
  {
    athame_clear_dirty();
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

void athame_draw_line_with_highlight(char* text, int start, int end)
{
  char highlighted[DEFAULT_BUFFER_SIZE];
  strncpy(highlighted, text+start, end-start);
  if(!highlighted[end - start - 1])
  {
    highlighted[end - start - 1] = ' ';
  }
  highlighted[end - start] = '\0';
  printf("\e[1G\e[%dC%s", strlen(rl_prompt), text);
  printf("\e[1G\e[%dC\e[7m%s\e[0m", strlen(rl_prompt) + start, highlighted);
  fflush(stdout);
}

int athame_highlight(int start, int end)
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
    athame_draw_line_with_highlight(new_string, this_start, this_end);
    printf("\n");
    athame_dirty++;
    new_string = next_string;
  }
  printf("\e[%dA", athame_dirty);
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
    if (strcmp(athame_mode, athame_displaying_mode) != 0) {
      strcpy(athame_displaying_mode, athame_mode);
      if (strcmp(athame_mode, "i") == 0)
      {
        athame_bottom_display("--INSERT--", 1);
      }
      else if (strcmp(athame_mode, "v") == 0)
      {
        athame_bottom_display("--VISUAL--", 1);
      }
      else if (strcmp(athame_mode, "V") == 0)
      {
        athame_bottom_display("--VISUAL LINE--", 1);
      }
      else if (strcmp(athame_mode, "s") == 0)
      {
        athame_bottom_display("--SELECT--", 1);
      }
      else if (strcmp(athame_mode, "R") == 0)
      {
        athame_bottom_display("--REPLACE--", 1);
      }
      else if (strcmp(athame_mode, "c") !=0)
      {
        athame_bottom_display("", 1);
      }
    }

    athame_redisplay();

    struct timeval timeout;
    int results = 0;
    if(first_char){
      athame_process_char(first_char);
      first_char = 0;
    }
    else {
      while(results == 0)
      {
        fd_set files;
        FD_ZERO(&files);
        FD_SET(instream, &files);
        FD_SET(from_vim, &files);
        timeout.tv_sec = 0;
        timeout.tv_usec = 500 * 1000;

        results = select(MAX(from_vim, instream)+1, &files, NULL, NULL, (strcmp(athame_mode, "c") == 0 || needs_poll)? &timeout : NULL);
        if (results > 0)
        {
          if(FD_ISSET(instream, &files)){
            returnVal = athame_process_input(instream);
          }
          else if(FD_ISSET(from_vim, &files)){
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
    }
  }
  if(key_pressed)
  {
    athame_extraVimRead(100);
  }
  updated = 0;
  athame_bottom_display("", 1);
  athame_displaying_mode[0] = 'n';
  athame_displaying_mode[1] = '\0';
  return returnVal;
}

void athame_extraVimRead(int timer)
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

char athame_process_input(int instream)
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

char athame_process_char(char char_read){
  if(athame_failed || ((char_read == '\r' || char_read == '\t') && strcmp(athame_mode, "c") != 0 ))
  {
    last_tab = (char_read == '\t');
    return char_read;
  }
  else
  {
    key_pressed = 1;

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

void athame_send_to_vim(char input)
{
  write(to_vim, &input, 1);
}

void athame_get_vim_info()
{
  if (!athame_get_vim_info_inner(1))
  {
    athame_poll_vim();
  }
}

int athame_get_vim_info_inner(int read_pipe)
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

  FILE* metaFile = fopen(meta_file_name, "r+"); //Change to r
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
        athame_bottom_display(command, 0);
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
        int col = atoi(colStr);
        int row = atoi(rowStr);

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
          new_end_col = atoi(colStr);
          rowStr = strtok(NULL, ",");
          if(!rowStr)
          {
            return 0;
          }
          new_end_row = atoi(rowStr);
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
        if(athame_mode[0] == 'c') {
          line_text = athame_get_lines_from_vim(row, row);
        }
        else {
          int subtract_line = (end_row > row && end_col == 0 ? 1 : 0);
          line_text = athame_get_lines_from_vim(row, end_row - subtract_line);
        }

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

char* athame_get_lines_from_vim(int start_row, int end_row)
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
