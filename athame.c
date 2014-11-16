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
#include <string.h>
#include <errno.h>
#include "athame.h"
#include "readline.h"

#define DEFAULT_BUFFER_SIZE 1024
#define MAX(A, B) (((A) > (B)) ? (A) : (B))

static char athame_buffer[DEFAULT_BUFFER_SIZE];
int vim_pid;
int vim_to_readline[2];
int readline_to_vim[2];
int from_vim;
int to_vim;

char athame_mode[3] = {'n', '\0', '\0'};

int error;

void athame_init()
{
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
    if (execl("/usr/bin/vim", "/usr/bin/vim", "--servername", "athame", "-s", "/dev/null", "+call Vimbed_SetupVimbed('', '')", NULL)!=0){
      printf("Error:%d", errno);
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
  }
}

void athame_cleanup()
{
  close(to_vim);
  close(from_vim);
  kill(vim_pid);
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
    rl_redisplay();
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

void athame_extraVimRead(int timer)
{
    fd_set files;
    FD_ZERO(&files);
    FD_SET(from_vim, &files);
    int results;
    int sanity = 100;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = timer * 1000;
    do
    {
      results = select(from_vim + 1, &files, NULL, NULL, &timeout);
      if (results > 0)
        athame_get_vim_info();
      if (timeout.tv_usec>0){
        timeout.tv_usec -= 10;
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
    if(result == '\r' && strcmp(athame_mode, "c") != 0 || result == '\t' || result == 'z')
    {
      return result;
    }
    else
    {
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

void athame_get_vim_info()
{
  ssize_t bytes_read;
  read(from_vim, &athame_buffer, DEFAULT_BUFFER_SIZE-1);

  FILE* contentsFile = fopen("/tmp/vimbed/athame/contents.txt", "r");
  bytes_read = fread(&athame_buffer, 1, DEFAULT_BUFFER_SIZE-1, contentsFile);
  fclose(contentsFile);
  athame_buffer[bytes_read] = 0;
  strcpy(rl_line_buffer, athame_buffer);
  //printf("first buffer: %s",athame_buffer);
  rl_end = bytes_read;

  FILE* metaFile = fopen("/tmp/vimbed/athame/meta.txt", "r+"); //Change to r
  bytes_read = fread(&athame_buffer, 1, DEFAULT_BUFFER_SIZE-1, metaFile);
  fclose(metaFile);

  athame_buffer[bytes_read] = 0;
  //printf("buffer: %s",athame_buffer);
  char* mode = strtok(athame_buffer, "\n");
  //printf("Mode: %s",mode);
  if(mode)
  {
    strncpy(athame_mode, mode, 2);
    char* location = strtok(NULL, "\n");
    //printf("Location: %s",mode);
    if(location)
    {
      if(strtok(location, ","))
      {
        char* point = strtok(NULL, ",");
        //TODO: better function
        rl_point = atoi(point);
        //printf("Point: %d",atoi(point));
        return;
      }
    }
  }
  //printf("failure");
}
