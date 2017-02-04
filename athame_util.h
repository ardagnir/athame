#include <sys/time.h>

static void athame_send_to_vim(char input);
static int athame_get_vim_info();
static void athame_set_failure(char* fail_str);
static char athame_process_input(int instream);
static char athame_process_char(char instream);
static int athame_setup_history();
static int athame_sleep(int msec, int char_break, int instream);
static int athame_get_vim_info_inner();
static void athame_update_vimline(int row, int col);
static int athame_remote_expr(char* expr, int bock);
static char athame_get_first_char(int instream);
static void athame_highlight(int start, int end);
static void athame_redisplay();
static void athame_bottom_display(char* string, int style, int color, int cursor, int force);
static void athame_bottom_mode();
static void athame_poll_vim(int block);
static void athame_draw_failure();
static int athame_has_clean_quit();
static int athame_wait_for_file(char* file_name, int sanity, int char_break, int instream);
static int athame_select(int file_desc1, int file_desc2, int timeout_sec, int timeout_ms, int no_signals);
static int athame_is_set(char* env, int def);
static char* athame_tok(char** pointer, char delim);
static long get_time();
static void athame_force_vim_sync();

#define DEFAULT_BUFFER_SIZE 2048

static char athame_buffer[DEFAULT_BUFFER_SIZE];
static char bottom_display[DEFAULT_BUFFER_SIZE];
static char athame_command[DEFAULT_BUFFER_SIZE];
static int command_cursor;
static int bottom_color;
static int bottom_style;
static int bottom_cursor;
static const char* athame_failure;
static int vim_pid = 0;
static int expr_pid;
static int vim_term;
static int cs_confirmed;
static FILE* dev_null;
static int athame_row;
static int updated;
static char* slice_file_name;
static char* contents_file_name;
static char* update_file_name;
static char* messages_file_name;
static char* vimbed_file_name;
static char* dir_name;
static char* servername;
#define VIM_NOT_STARTED 0
#define VIM_TRIED_START 1
#define VIM_CONFIRMED_START 2
#define VIM_RUNNING 3
static int vim_stage = VIM_NOT_STARTED;

// Have we sent any keys to vim since readline started.
static int sent_to_vim = 0;

// How closely is vim synced with athame?
#define VIM_SYNC_YES 0
#define VIM_SYNC_NEEDS_INFO_READ 1
#define VIM_SYNC_WAITING_POLL_DONE 2
#define VIM_SYNC_NEEDS_POLL 3
#define VIM_SYNC_CHAR_BEHIND 4
#define VIM_SYNC_NO 5
static int vim_sync = VIM_SYNC_YES;
static long time_to_sync = 0;

// Measured in ms since epoch
static long time_to_poll = -1;
static long vim_started = -1;

// Number of stale polls since last keypress or change
static int stale_polls = 0;

static int change_since_key=0;
static FILE* athame_outstream = 0;

static char athame_mode[3];
static char last_athame_mode[3];
static int end_col; //For visual mode
static int end_row; //For visual mode

static int athame_dirty;

static int start_vim(int char_break, int instream)
{
  if (char_break && athame_select(instream, -1, 0, 0, 0) > 0)
  {
    return 2;
  }

  char* etcrc = "/etc/athamerc";
  char homerc[256];
  char* athamerc = getenv("ATHAME_TEST_RC");
  int testrc = athamerc != NULL;
  if (!testrc)
  {
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
      return 1;
    }
  }

  int pid = forkpty(&vim_term, NULL, NULL, NULL);
  if (pid == 0)
  {
    int cursor = ap_get_cursor();
    snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "+call Vimbed_UpdateText(%d, %d, %d, %d, 1)", athame_row+1, cursor+1, athame_row+1, cursor+1);
    int vim_error = 0;
    if (ATHAME_VIM_BIN[0]) {
      if (testrc) {
        vim_error = execl(ATHAME_VIM_BIN, "vim", "--servername", servername, "-u", "NONE", "-S", vimbed_file_name, "-S", athamerc, "+call Vimbed_SetupVimbed('', 'slice')", athame_buffer, NULL);
      } else {
        vim_error = execl(ATHAME_VIM_BIN, "vim", "--servername", servername, "-S", vimbed_file_name, "-S", athamerc, "+call Vimbed_SetupVimbed('', 'slice')", athame_buffer, NULL);
      }
    }
    else {
      if (testrc) {
        vim_error = execlp("vim", "vim", "--servername", servername, "-u", "NONE", "-S", vimbed_file_name, "-S", athamerc,  "+call Vimbed_SetupVimbed('', 'slice')", athame_buffer, NULL);
      } else {
        vim_error = execlp("vim", "vim", "--servername", servername, "-S", vimbed_file_name, "-S", athamerc, "+call Vimbed_SetupVimbed('', 'slice')", athame_buffer, NULL);
      }
    }
    if (vim_error != 0)
    {
      printf("Error: %d", errno);
      exit(EXIT_FAILURE);
    }
    return 1;
  }
  else if (pid == -1)
  {
    athame_set_failure("Failure starting Vim");
    return 1;
  }
  else
  {
    vim_pid = pid;
    ap_set_control_chars();
    return 0;
  }
}

static int confirm_vim_start(int char_break, int instream)
{
  int selected = athame_select(vim_term, char_break ? instream : -1, char_break ? 5 : 1, 0, 1);
  if (selected == -1) {
    athame_set_failure("Select interupted");
    return 1;
  }
  if (selected == 0) {
    athame_set_failure("Vim timed out");
    return 1;
  }
  else if (selected == 2) {
    return 2;
  }

  int amount = read(vim_term, athame_buffer, 5);
  athame_buffer[amount] = '\0';
  if(strncmp(athame_buffer, "Error", 5) == 0)
  {
    if (ATHAME_VIM_BIN[0])
    {
      char* error;
      asprintf(&error, "Couldn't load vim path: %s", ATHAME_VIM_BIN);
      athame_set_failure(error);
      free(error);
    }
    else
    {
      athame_set_failure("Couldn't load vim.");
    }
    return 1;
  }
  else if(strncmp(athame_buffer, "VIM", 3) == 0)
    // Vim should dump termcap stuff before this if things are working, so only bother checking for errors if we start with VIM.
    // If Vim changes to not start errors with "VIM", we will still catch the error later with the vague "Vimbed failure" label
  {
    athame_sleep(20, 0, 0);
    amount = read(vim_term, athame_buffer, 200);
    athame_buffer[amount] = '\0';
    if(strstr(athame_buffer, "--servername"))
    {
      if (ATHAME_VIM_BIN[0])
      {
        char* error;
        asprintf(&error, "%s was not compiled with clientserver support.", ATHAME_VIM_BIN);

        athame_set_failure(error);
        free(error);
      }
      else
      {
        athame_set_failure("Tried running vim without clientserver support.");
      }
      return 1;
    } //--servername
  } //VIM
  return 0;
}

static int athame_wait_for_vim(int char_break, int instream)
{
  int error = athame_wait_for_file(slice_file_name, 50, char_break, instream);
  if (error == 1)
  {
    if (athame_wait_for_file(contents_file_name, 1, 0, 0) == 0) {
      athame_set_failure("Using incompatible vimbed version.");
    }
    else {
      athame_set_failure("Vimbed failure.");
    }
  }
  if (error > 0)
  {
     return error;
  }

  vim_started = get_time();

  if (char_break) {
    // Grab the mode now because the program might block (like gdb).
    int i;
    for(i = 0; i < 6; i++) {
      if (athame_mode[0] != 'n' || athame_sleep(10, char_break, instream)) {
        break;
      }
      athame_get_vim_info();
    }
  } else {
    athame_get_vim_info();
  }
  athame_redisplay();
  return 0;
}

// Make sure vim is running,
//  or set athame_failure,
//  or (if char_break) break on char press
static void athame_ensure_vim(int char_break, int instream)
{
  // These will fallthrough if not interrupted.
  if(vim_stage == VIM_NOT_STARTED) {
    if(!start_vim(char_break, instream)) {
      vim_stage = VIM_TRIED_START;
    }
  }
  if(vim_stage == VIM_TRIED_START) {
    if(!confirm_vim_start(char_break, instream)) {
      vim_stage = VIM_CONFIRMED_START;
    }
  }
  if(vim_stage == VIM_CONFIRMED_START) {
    if(!athame_wait_for_vim(char_break, instream)) {
      vim_stage = VIM_RUNNING;
    }
  }
}

static int athame_wait_for_file(char* file_name, int sanity, int char_break, int instream)
{
  // Check for existance of a file to see if we have advanced that far
  FILE* theFile = 0;
  theFile = fopen(file_name, "r");
  while (!theFile)
  {
    if(sanity-- < 0)
    {
      return 1;
    }
    if (athame_sleep(20, char_break, instream)) {
      return 2;
    }
    theFile = fopen(file_name, "r");
  }
  fclose(theFile);
  return 0;
}

static char athame_get_first_char(int instream)
{
  char return_val = '\0';
  if (athame_select(instream, -1, 0, 0, 0) > 0) {
    read(instream, &return_val, 1);
  }
  return return_val;
}

static int athame_sleep(int msec, int char_break, int instream)
{
    if(char_break)
    {
      return athame_select(instream, -1, 0, msec, 1) > 0;
    }
    else
    {
      struct timespec timeout, timeout2;
      timeout.tv_sec = 0;
      timeout.tv_nsec = msec * 1000000;
      while (nanosleep(&timeout, &timeout2)<0)
      {
        timeout = timeout2;
      }
      return 0;
    }
}

/* Write history file and store number of lines in athame_row */
static int athame_setup_history()
{
  FILE* updateFile = fopen(update_file_name, "w+");

  if(!updateFile) {
    athame_set_failure("Couldn't create temporary file in /tmp/vimbed.");
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
  fwrite(ap_get_line_buffer(), 1, ap_get_line_buffer_length(), updateFile);
  fwrite("\n", 1, 1, updateFile);
  athame_row = total_lines;
  ap_get_history_end();

  fclose(updateFile);
  return 0;
}

// Sends a message to vim
//
// All the use_pipe stuff is for checking that clientserver actually works. By
// now Vim has told us that it does, but we actually make sure since some
// versions (like MacVim) lie about it.
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
  if (expr_pid > 0)
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

    if (ATHAME_VIM_BIN[0])
    {
      execl(ATHAME_VIM_BIN, "vim", "--servername", servername, "--remote-expr", expr, NULL);
    }
    else
    {
      execlp("vim", "vim", "--servername", servername, "--remote-expr", expr, NULL);
    }
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
    athame_set_failure("Clientserver error.");
    return -1;
  }
  else
  {
    if(block)
    {
      waitpid(expr_pid, NULL, 0);
      expr_pid = 0;
    }
    if(use_pipe)
    {
      close(stdout_to_readline[1]);
      close(stderr_to_readline[1]);
      int selected = athame_select(stdout_to_readline[0], stderr_to_readline[0], -1, -1, 0);
      if(selected > 0)
      {
        char error[80];
        if(selected == 2 || read(stdout_to_readline[0], error, 1) < 1) {
          error[read(stderr_to_readline[0], error, sizeof(error)/sizeof(char))] = '\0';
          snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Clientserver error for %s:%s", expr, error);
          athame_set_failure(athame_buffer);
          return -1;
        }
        cs_confirmed = 1;
      }
      close(stdout_to_readline[0]);
      close(stderr_to_readline[0]);
    }
  }
  return 0;
}


static void athame_update_vimline(int row, int col)
{
  FILE* updateFile = fopen(update_file_name, "w+");

  fwrite(ap_get_line_buffer(), 1, ap_get_line_buffer_length(), updateFile);
  fwrite("\n", 1, 1, updateFile);

  fclose(updateFile);

  snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Vimbed_UpdateText(%d, %d, %d, %d, 0)", row+1, col+1, row+1, col+1);

  athame_remote_expr(athame_buffer, 1);
  updated = 1;
}

static long get_timeout_msec() {
  if(vim_sync >= VIM_SYNC_CHAR_BEHIND) {
    time_to_sync = 100 + get_time();
    return 100;
  }
  if (time_to_poll == -1) {
    return -1;
  }
  return MAX(30, time_to_poll - get_time() + 5);
}


static void request_poll() {
  if (stale_polls > 2) {
    return;
  }
  long request_time = get_time() + (change_since_key || stale_polls > 0 ? 500 : 100);
  time_to_poll = time_to_poll < 0 ? request_time : MIN(time_to_poll, request_time);
}

static void athame_poll_vim(int block)
{
  time_to_poll = -1;

  //Poll Vim. If we fail, postpone the poll by requesting a new poll
  if (athame_remote_expr("Vimbed_Poll()", block) == 0) {
    if (vim_sync == VIM_SYNC_WAITING_POLL_DONE) {
      vim_sync = VIM_SYNC_NEEDS_INFO_READ;
    } else if (vim_sync == VIM_SYNC_NEEDS_POLL) {
      if (block) {
        vim_sync = VIM_SYNC_NEEDS_INFO_READ;
      } else {
        vim_sync = VIM_SYNC_WAITING_POLL_DONE;
      }
    }
    stale_polls++;
  } else {
    request_poll();
  }
}

int last_bdisplay_top = 0;
int last_bdisplay_bottom = 0;
int text_lines_for_bdisplay = 0;

void athame_bottom_display(char* string, int style, int color, int cursor, int force)
{
    int term_height, term_width;
    ap_get_term_size(&term_height, &term_width);

    int new_text_lines = (ap_get_line_char_length() + ap_get_prompt_length())/term_width;

    if (!force && term_height == last_bdisplay_bottom) {
      int changed = 0;
      changed |= (strcmp(bottom_display, string));
      if (string[0] != '\0') {
        changed |= (style != bottom_style);
        changed |= (color != bottom_color);
        changed |= (cursor != bottom_cursor);
        changed |= (new_text_lines != text_lines_for_bdisplay);
      }
      if (!changed) {
        return;
      }
    }
    text_lines_for_bdisplay = new_text_lines;

    strncpy(bottom_display, string, DEFAULT_BUFFER_SIZE-1);
    bottom_display[DEFAULT_BUFFER_SIZE-1]='\0';
    bottom_style = style;
    bottom_color = color;
    bottom_cursor = cursor;

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

    //\e7\n\e8\e[B\e[A         Add a line underneath if at bottom
    //\e7                      Save cursor position
    //%s(erase)                Delete old athame_bottom_display
    //\e[%d;1H                 Go to position for new athame_bottom_display
    //%s(colorstyle)%s(string) Write bottom display using given color/style
    //%s                       Draw cursor for command mode
    //\e[0m                    Reset style
    //\e8                      Return to saved position
    fprintf(athame_outstream, "\e7\n\e8\e[B\e[A\e7%s\e[%d;1H%s%s%s\e[0m\e8", erase, term_height-extra_lines, colorstyle, string, cursor_code);

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
    athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);

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
  athame_bottom_mode();
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
  int term_width;
  ap_get_term_size(NULL, &term_width);
  //How much more than one line does the text take up (if it wraps)
  int extra_lines = ((int)strlen(text) + prompt_len - 1)/term_width;
  //How far down is the start of the highlight (if text wraps)
  int extra_lines_s = (start + prompt_len) /term_width;
  //How far down is the end of the highlight (if text wraps)
  int extra_lines_e = (end + prompt_len - 1) /term_width;

  char* with_space = athame_copy_w_space(text);
  char* highlighted = ap_get_substr(with_space, start, end);
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

static void athame_highlight(int start, int end)
{
  char* highlight_buffer = strdup(ap_get_line_buffer());
  char* hi_loc = highlight_buffer;
  int cursor = ap_get_cursor();
  ap_set_line_buffer("");
  ap_display();
  ap_set_line_buffer(highlight_buffer);
  ap_set_cursor(cursor);
  char* new_string = athame_tok(&hi_loc, '\n');
  while (new_string){
    char* next_string;
    int this_start;
    int this_end;
    next_string = athame_tok(&hi_loc, '\n');

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
  if (athame_dirty)
  {
    fprintf(athame_outstream, "\e[%dA", athame_dirty);
  }
  fflush(athame_outstream);
}

static int athame_has_clean_quit()
{
  FILE* sliceFile = fopen(slice_file_name, "r");
  if (!sliceFile)
  {
    return 0;
  }
  int bytes_read = fread(athame_buffer, 1, 4, sliceFile);
  fclose(sliceFile);
  return strncmp(athame_buffer, "quit", bytes_read) == 0;
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

static int athame_set_mode(char* mode)
{
  if (strcmp(athame_mode, mode) != 0)
  {
    char* mode_string = athame_get_mode_text(mode);
    strcpy(athame_mode, mode);
    setenv("ATHAME_VIM_MODE", mode_string, 1);
    ap_redraw_prompt();
    return 1;
  }
  return 0;
}

static void athame_bottom_mode()
{
  if(athame_failure)
  {
    return;
  }
  char* mode_string = athame_get_mode_text(athame_mode);
  {
    if(athame_mode[0] == 'c' && athame_is_set("ATHAME_SHOW_COMMAND", 1))
    {
      athame_bottom_display(athame_command, ATHAME_NORMAL, ATHAME_DEFAULT, command_cursor, 0);
    }
    else if (athame_is_set("ATHAME_SHOW_MODE", 1))
    {
      if (athame_mode[0] == 'n')
      {
        athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
      }
      else if(mode_string[0])
      {
        sprintf(athame_buffer, "-- %s --", mode_string);
        athame_bottom_display(athame_buffer, ATHAME_BOLD, ATHAME_DEFAULT, 0, 0);
      }
    }
  }
}

static void athame_draw_failure()
{
  setenv("ATHAME_ERROR", athame_failure, 1);
  if (athame_is_set("ATHAME_SHOW_ERROR", 1))
  {
    snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Athame Failure: %s", athame_failure);
    athame_bottom_display(athame_buffer, ATHAME_BOLD, ATHAME_RED, 0, 0);
  }
}

static void athame_set_failure(char* fail_str)
{
  athame_failure = strdup(fail_str);
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

// Should we send the char to readline?
static int athame_is_special_char(char char_read) {
  if (athame_failure) {
    return 1;
  }
  if (strchr(ap_special, char_read)) {
    // Make sure our mode is up to date.
    athame_force_vim_sync();
    if (strcmp(athame_mode, "c") != 0)
    {
      return 1;
    }
  }
  ap_set_nospecial();
  return 0;
}

static char athame_process_char(char char_read){
  if (athame_is_special_char(char_read))
  {
    return char_read;
  }
  else
  {
    sent_to_vim = 1;
    stale_polls = 0;
    change_since_key = 0;

    //Backspace
    if (char_read == '\177'){
      char_read = '\b';
    }
    athame_send_to_vim(char_read);
    return 0;
  }
}

static void athame_send_to_vim(char input)
{
  if (vim_sync < VIM_SYNC_CHAR_BEHIND) {
    vim_sync = VIM_SYNC_CHAR_BEHIND;
  } else {
    vim_sync = VIM_SYNC_NO;
  }
  write(vim_term, &input, 1);
}

static int athame_get_vim_info()
{
  if (vim_sync == VIM_SYNC_NEEDS_INFO_READ) {
    vim_sync = VIM_SYNC_YES;
  }
  if (athame_get_vim_info_inner())
  {
    time_to_poll = -1;
    stale_polls = 0;
    change_since_key = 1;
    athame_redisplay();
    return 1;
  }
  return 0;
}

static int athame_get_col_row(char* string, int* col, int* row)
{
    if(string && athame_tok(&string, ','))
    {
      char* colStr = athame_tok(&string, ',');
      if(!colStr){
        return 0;
      }
      *col = strtol(colStr, NULL, 10);
      if(row)
      {
        char* rowStr = athame_tok(&string, ',');
        if(!rowStr){
          return 0;
        }
        *row = strtol(rowStr, NULL, 10);
      }
      return 1;
    }
    return 0;
}

static int athame_get_vim_info_inner()
{
  int changed = 0;
  ssize_t bytes_read;
  if(athame_failure)
  {
    return 1;
  }

  FILE* sliceFile = fopen(slice_file_name, "r");
  if (!sliceFile)
  {
    return 0;
  }
  bytes_read = fread(athame_buffer, 1, DEFAULT_BUFFER_SIZE-1, sliceFile);
  fclose(sliceFile);

  athame_buffer[bytes_read] = 0;
  char* buffer_loc = athame_buffer;
  char* mode = athame_tok(&buffer_loc, '\n');
  if(mode && *mode)
  {
    if (mode[0] == 'c')
    {
      changed |= athame_set_mode("c");
      char* command = athame_tok(&buffer_loc, '\n');
      int cmd_pos = 0;
      if (mode[1] == ',')
      {
        cmd_pos = strtol(mode+2, NULL, 10);
      }
      if (command)
      {
        if(strcmp(athame_command, command) !=0 || command_cursor != cmd_pos) {
          setenv("ATHAME_VIM_COMMAND", command, 1);
          strcpy(athame_command, command);
          command_cursor = cmd_pos;
          changed =1;
        }
      }
    }
    else
    {
      if(athame_mode[0] == 'c' && athame_is_set("ATHAME_SHOW_COMMAND", 1))
      {
        athame_bottom_display("", ATHAME_NORMAL, ATHAME_DEFAULT, 0, 0);
      }
      if(strcmp(mode, "quit") == 0) {
        // Don't do work if we're quitting.
        return 0;
      }
      changed |= athame_set_mode(mode);
    }
    char* location = athame_tok(&buffer_loc, '\n');
    if(location)
    {
      char* location2 = athame_tok(&buffer_loc, '\n');
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

        if (buffer_loc)
        {
          if(strcmp(ap_get_line_buffer(), buffer_loc) != 0)
          {
            changed = 1;
            ap_set_line_buffer(buffer_loc);
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

static int athame_select(int file_desc1, int file_desc2, int timeout_sec, int timeout_ms, int no_signals) {
    fd_set files;
    FD_ZERO(&files);
    if (file_desc1 > -1) {
      FD_SET(file_desc1, &files);
    }
    if (file_desc2 > -1) {
      FD_SET(file_desc2, &files);
    }
    sigset_t block_signals;
    int use_timeout = timeout_sec >= 0 && timeout_ms >= 0;
    int results;
    if (no_signals) {
      struct timespec timeout;
      timeout.tv_sec = timeout_sec;
      timeout.tv_nsec = timeout_ms * 1000000;
      sigfillset(&block_signals);
      results = pselect(MAX(file_desc1, file_desc2) + 1, &files, NULL, NULL, use_timeout ? &timeout : NULL, &block_signals);
    } else {
      struct timeval timeout;
      timeout.tv_sec = timeout_sec;
      timeout.tv_usec = timeout_ms * 1000;
      results = select(MAX(file_desc1, file_desc2) + 1, &files, NULL, NULL, use_timeout ? &timeout : NULL);
    }
    if (results > 0) {
      if (file_desc1 >= 0 && FD_ISSET(file_desc1, &files)) {
         return 1;
      }
      if (file_desc2 >= 0 && FD_ISSET(file_desc2, &files)) {
         return 2;
      }
    }
    return results;
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

// Unlike normal strtok, this gives the empty strings between consecutive tokens.
static char* athame_tok(char** pointer, char delim) {
  if (*pointer == 0) {
    return 0;
  }
  char* original = *pointer;
  char* loc = strchr(original, delim);
  if (loc == 0) {
    *pointer = 0;
  } else {
    *loc = '\0';
    *pointer = loc + 1;
  }
  return original;
}

static long get_time() {
  // We need to check __MACH__ too because OSX claims it has this functionality
  // but doesn't.
  #if _POSIX_MONOTONIC_CLOCK && !__MACH__
    struct timespec t;
    #if CLOCK_MONOTONIC_COARSE
      clock_gettime(CLOCK_MONOTONIC_COARSE, &t);
    #else
      clock_gettime(CLOCK_MONOTONIC, &t);
    #endif
    return t.tv_sec*1000L + t.tv_nsec/1000/1000;
  #else
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000L + t.tv_usec/1000;
  #endif
}

static void wait_then_kill(int pid) {
    int i;
    for (i = 0; i < 10; i++) {
      if(waitpid(pid, NULL, WNOHANG) != 0) {
        break;
      }
      athame_sleep(5, 0, 0);
    }
    if (waitpid(pid, NULL, WNOHANG) == 0) {
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
    }
}

static void athame_force_vim_sync() {
      if (vim_sync == VIM_SYNC_YES) {
        return;
      }
      if (vim_sync == VIM_SYNC_CHAR_BEHIND) {
        athame_sleep(20,0,0);
        vim_sync = VIM_SYNC_NEEDS_POLL;
      }
      else if (vim_sync == VIM_SYNC_NO) {
        if (athame_select(vim_term, -1, 0, MAX(1, time_to_sync - get_time()), 1) != 0) {
          int sanity = 100;
          while (athame_select(vim_term, -1, 0, 100, 1) != 0 && sanity-- > 0) {
            athame_sleep(5, 0, 0);
            read(vim_term, athame_buffer, DEFAULT_BUFFER_SIZE-1);
          }
        }
        vim_sync = VIM_SYNC_NEEDS_POLL;
      }
      if (vim_sync == VIM_SYNC_WAITING_POLL_DONE) {
        waitpid(expr_pid, NULL, 0);
        vim_sync = VIM_SYNC_NEEDS_INFO_READ;
      }
      if (vim_sync >= VIM_SYNC_NEEDS_POLL) {
        athame_poll_vim(1);
      }
      athame_get_vim_info();
}

static int is_vim_alive() {
  return vim_pid && waitpid(vim_pid, NULL, WNOHANG) == 0;
}
