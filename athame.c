/* athame.c -- Full vim integration for readline.*/

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

#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "history.h"
#include "athame.h"
#include "readline.h"

#define TIME_AMOUNT 4
#define DEFAULT_BUFFER_SIZE 1024
#define MAX(A, B) (((A) > (B)) ? (A) : (B))

static char athame_buffer[DEFAULT_BUFFER_SIZE];
int vim_pid;
int vim_to_readline[2];
int readline_to_vim[2];
int from_vim;
int to_vim;
FILE* dev_null;
int key_sent;
int athame_row;

char athame_mode[3] = {'n', '\0', '\0'};

int error;

void athame_init()
{
  dev_null = fopen("/dev/null", "w");
  key_sent = 0;
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
    if (execl("/usr/bin/vim", "/usr/bin/vim", "--servername", "athame", "-s", "/dev/null", "+call Vimbed_SetupVimbed('', '')", NULL)!=0)
    {
      printf("Error:%d", errno);
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

    athame_sleep(15*TIME_AMOUNT);

    athame_update_vim(0);
  }
}

void athame_cleanup()
{
  close(to_vim);
  close(from_vim);
  fclose(dev_null);
  kill(vim_pid);
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

void athame_update_vim(int col)
{
  FILE* contentsFile = fopen("/tmp/vimbed/athame/contents.txt", "w+");
  HISTORY_STATE* hs = history_get_history_state();
  int counter;
  for(counter; counter < hs->length; counter++)
  {
    fwrite(hs->entries[counter]->line, 1, strlen(hs->entries[counter]->line), contentsFile);
    fwrite("\n", 1, 1, contentsFile);
  }
  fwrite("\n", 1, 1, contentsFile);
  fclose(contentsFile);
  athame_row = hs->length;
  snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Vimbed_UpdateText(%d, %d, %d, %d, 0)", hs->length+1, col+1, hs->length+1, col+1);
  xfree(hs);

  int pid = fork();
  if (pid == 0)
  {
    dup2(fileno(dev_null), STDOUT_FILENO);
  //  dup2(fileno(dev_null), STDERR_FILENO);
    execl ("/usr/bin/vim", "/usr/bin/vim", "--servername", "athame", "--remote-expr", athame_buffer, NULL);
    printf("Expr Error:%d", errno);
    exit (EXIT_FAILURE);
  }
  else if (pid == -1)
  {
    //TODO: error handling
    perror("ERROR! Couldn't run vim remote-expr!");
  }
  athame_sleep(15*TIME_AMOUNT);
}

void athame_poll_vim()
{
  int pid = fork();
  if (pid == 0)
  {
    dup2(fileno(dev_null), STDOUT_FILENO);
  //  dup2(fileno(dev_null), STDERR_FILENO);
    execl ("/usr/bin/vim", "/usr/bin/vim", "--servername", "athame", "--remote-expr", "Vimbed_Poll()", NULL);
    printf("Expr Error:%d", errno);
    exit (EXIT_FAILURE);
  }
  else if (pid == -1)
  {
    //TODO: error handling
    perror("ERROR! Couldn't run vim remote-expr!");
  }
}

char athame_loop(int instream)
{
  char returnVal = 0;
  while(!returnVal)
  {
    fd_set files;
    FD_ZERO(&files);
    FD_SET(instream, &files);
    FD_SET(from_vim, &files);
    if (key_sent)
    {
      rl_redisplay();
    }
    int results = select(MAX(from_vim, instream)+1, &files, NULL, NULL, NULL);
    if (results>0)
    {
      if(FD_ISSET(from_vim, &files)){
        athame_get_vim_info();
      }
      if(FD_ISSET(instream, &files)){
        returnVal = athame_process_input(instream);
      }
    }
  }
  athame_extraVimRead(100);
  return returnVal;
}

//Timer is the number of milliseconds that we will wait if there is NO input
void athame_extraVimRead(int timer)
{
    fd_set files;
    FD_ZERO(&files);
    FD_SET(from_vim, &files);
    int results;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = timer * 500;
    do
    {
      athame_sleep(timer/2);
      results = select(from_vim + 1, &files, NULL, NULL, &timeout);
      if (results > 0)
        athame_get_vim_info();
      if (timeout.tv_usec > 5000)
      {
        timeout.tv_usec -= 5000; //Sanity check. We can't go on for ever.
      }
      else
      {
        break;
      }
    } while (results > 0);
    rl_redisplay();
}

char athame_process_input(int instream)
{
  char result;
  int chars_read = read(instream, &result, 1);
  if (chars_read == 1)
  {
    key_sent = 1;
    if((result == '\r' || result == '\t') && strcmp(athame_mode, "c") != 0 )
    {
      return result;
    }
    else
    {
      //Backspace
      if (result == '\177'){
        result = '\b';
      }
      athame_send_to_vim(result);
      return 0;
    }
  }
  else
  {
    return EOF;
  }
}

void athame_send_to_vim(char input)
{
  write(to_vim, &input, 1);
}

void athame_get_vim_info(){
  if (!athame_get_vim_info_inner(3,0))
  {
    athame_poll_vim();
    athame_sleep(15*TIME_AMOUNT);
    athame_get_vim_info_inner(3,1);
  }
}

int athame_get_vim_info_inner(int attempts, int changed)
{
  ssize_t bytes_read;
  read(from_vim, athame_buffer, DEFAULT_BUFFER_SIZE-1);

  FILE* metaFile = fopen("/tmp/vimbed/athame/meta.txt", "r+"); //Change to r
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
      if (command)
      {
        strcpy(rl_line_buffer, command);
        rl_end = strlen(command);
        rl_point = rl_end;
      }
      return 0;
    }
    char* location = strtok(NULL, "\n");
    if(location)
    {
      if(strtok(location, ","))
      {
        //TODO: better function
        int col = atoi(strtok(NULL, ","));
        int row = atoi(strtok(NULL, ","));

        if(athame_row != row)
        {
          athame_row = row;
          changed = 1;
        }

        char* line_text = athame_get_line_from_vim(row);

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

  if (attempts <= 1)
  {
    return 0;
  }
  else
  {
    return athame_get_vim_info_inner(attempts-1, changed);
  }
}

char* athame_get_line_from_vim(int row)
{
  FILE* contentsFile = fopen("/tmp/vimbed/athame/contents.txt", "r");
  int bytes_read = fread(athame_buffer, 1, DEFAULT_BUFFER_SIZE-1, contentsFile);
  athame_buffer[bytes_read] = 0;

  int current_line = 0;
  int failure = 0;
  char* line_text = athame_buffer;
  while (current_line < row )
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

  char* next_line = strchr(athame_buffer, '\n');

  //Some of the line might have gotten cut off by segmentation
  if(!next_line)
  {
     bytes_read = fread(athame_buffer+len, 1, DEFAULT_BUFFER_SIZE-1-len, contentsFile);
     athame_buffer[bytes_read+len] = 0;
     next_line = strchr(athame_buffer, '\n');
  }

  if(next_line)
  {
    *next_line = 0;
  }
  fclose(contentsFile);
  return athame_buffer;
}
