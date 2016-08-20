static void athame_send_to_vim(char input);
static void athame_get_vim_info();
static void athame_set_failure(char* fail_str);
static char athame_process_input(int instream);
static char athame_process_char(char instream);
static int athame_setup_history();
static char* athame_get_lines_from_vim(int start_row, int end_row);
static int athame_sleep(int msec, int char_break, int instream);
static int athame_get_vim_info_inner(int read_pipe);
static void athame_update_vimline(int row, int col);
static int athame_remote_expr(char* expr, int bock);
static char athame_get_first_char(int instream);
static void athame_highlight(int start, int end);
static void athame_bottom_display(char* string, int style, int color, int cursor);
static void athame_bottom_mode();
static void athame_poll_vim(int block);
static void athame_draw_failure();
static int athame_has_clean_quit();
static int athame_wait_for_file(char* file_name, int sanity, int char_break, int instream);
static int athame_select(int file_desc1, int file_desc2, int timeout_sec, int timeout_ms, int no_signals);
static int athame_is_set(char* env, int def);

#define DEFAULT_BUFFER_SIZE 1024

static char athame_buffer[DEFAULT_BUFFER_SIZE];
static char last_vim_command[DEFAULT_BUFFER_SIZE];
static int last_cmd_pos;
static const char* athame_failure;
static int vim_pid;
static int expr_pid;
static int vim_term;
static int cs_confirmed;
static FILE* dev_null;
static int athame_row;
static int updated;
static char* slice_file_name;
static char* contents_file_name;
static char* update_file_name;
static char* meta_file_name;
static char* messages_file_name;
static char* vimbed_file_name;
static char* dir_name;
static char* servername;
#define VIM_NOT_STARTED 0
#define VIM_TRIED_START 1
#define VIM_CONFIRMED_START 2
#define VIM_RUNNING 3
static int vim_stage = VIM_NOT_STARTED;
static int sent_to_vim = 0;
static int needs_poll = 0;
static FILE* athame_outstream = 0;

static char athame_mode[3];
static char athame_displaying_mode[3];
static int end_col; //For visual mode
static int end_row; //For visual mode

static int athame_dirty;

static int start_vim(int char_break, int instream)
{
  if (char_break && athame_select(instream, -1, 0, 0, 0))
  {
    return 2;
  }
  else
  {
    char* etcrc = "/etc/athamerc";
    char homerc[256];
    char* athamerc;
    if (!getenv("ATHAME_TEST_RC"))
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
      char* testrc;
      if (ATHAME_VIM_BIN[0]) {
        if (testrc = getenv("ATHAME_TEST_RC")) {
          vim_error = execl(ATHAME_VIM_BIN, "vim", "--servername", servername, "-u", "NONE", "-S", vimbed_file_name, "-S", testrc, "-s", "/dev/null", "+call Vimbed_SetupVimbed('', 'slice')", athame_buffer, NULL);
        } else {
          vim_error = execl(ATHAME_VIM_BIN, "vim", "--servername", servername, "-S", vimbed_file_name, "-S", athamerc, "-s", "/dev/null", "+call Vimbed_SetupVimbed('', 'slice')", athame_buffer, NULL);
        }
      }
      else {
        if (testrc = getenv("ATHAME_TEST_RC")) {
          vim_error = execlp("vim", "vim", "--servername", servername, "-u", "NONE", "-S", vimbed_file_name, "-S", testrc, "-s", "/dev/null", "+call Vimbed_SetupVimbed('', 'slice')", athame_buffer, NULL);
        } else {
          vim_error = execlp("vim", "vim", "--servername", servername, "-S", vimbed_file_name, "-S", athamerc, "-s", "/dev/null", "+call Vimbed_SetupVimbed('', 'slice')", athame_buffer, NULL);
        }
      }
      if (vim_error != 0)
      {
        printf("Error: %d", errno);
        exit(EXIT_FAILURE);
      }
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
}

static int confirm_vim_start(int char_break, int instream)
{
  int selected = athame_select(vim_term, char_break ? instream : -1, char_break ? 5 : 1, 0, 1);
  if (selected == 0) {
    athame_set_failure("Vim timed out");
    return 1;
  }
  else if (selected == 2) {
    return 2;
  }

  int amount = read(vim_term, athame_buffer, 200);
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
  else if(strstr(athame_buffer, "--servername"))
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
  }
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

  // Grab the mode now because the program might block (like gdb).
  int i;
  for(i = 0; i < 6; i++) {
    if (athame_mode[0] != 'n' || athame_sleep(10, char_break, instream)) {
      break;
    }
    athame_get_vim_info(0, 0);
  }
  athame_bottom_mode();
  return 0;
}

// Make sure vim is running,
//  or set athame_failure,
//  or (if char_break) break on char press
static int athame_ensure_vim(int char_break, int instream)
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
    }
    if(use_pipe)
    {
      close(stdout_to_readline[1]);
      close(stderr_to_readline[1]);
      int selected = athame_select(stdout_to_readline[0], stderr_to_readline[0], -1, -1, 0);
      if(selected)
      {
        char error[80];
        if(selected == 2 || read(stdout_to_readline[0], &error, 1) < 1) {
          char* prefix = "Clientserver error: ";
          int prelen = strlen(prefix);
          strcpy(error, prefix);
          error[read(stderr_to_readline[0], &error+prelen, 80-prelen) + prelen] = '\0';
          athame_set_failure(error);
          return -1;
        }
        cs_confirmed = 1;
      }
      close(stdout_to_readline[0]);
      close(stderr_to_readline[0]);
    }
    return 0;
  }
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

static void athame_poll_vim(int block)
{
  //Poll Vim. If we fail, postpone the poll by setting needs_poll to true
  needs_poll = (athame_remote_expr("Vimbed_Poll()", block) != 0);
}

int last_bdisplay_top = 0;
int last_bdisplay_bottom = 0;

void athame_bottom_display(char* string, int style, int color, int cursor)
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

static void athame_highlight(int start, int end)
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
  if (athame_dirty)
  {
    fprintf(athame_outstream, "\e[%dA", athame_dirty);
  }
  fflush(athame_outstream);
}

static int athame_has_clean_quit()
{
  FILE* metaFile = fopen(meta_file_name, "r");
  if (!metaFile)
  {
    return 0;
  }
  int bytes_read = fread(athame_buffer, 1, 4, metaFile);
  fclose(metaFile);
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

static void athame_bottom_mode()
{
  if(athame_failure)
  {
    return;
  }
  static int text_lines = 0;
  int new_text_lines = (ap_get_line_char_length() + ap_get_prompt_length())/ap_get_term_width();
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
        athame_bottom_display(last_vim_command, ATHAME_NORMAL, ATHAME_DEFAULT, last_cmd_pos);
      }
    }
    if (athame_is_set("ATHAME_SHOW_MODE", 1))
    {
      if (athame_mode[0] == 'n')
      {
        athame_bottom_display("", ATHAME_BOLD, ATHAME_DEFAULT, 0);
      }
      else if(athame_mode[0] != 'c' && mode_string[0])
      {
        sprintf(athame_buffer, "-- %s --", mode_string);
        athame_bottom_display(athame_buffer, ATHAME_BOLD, ATHAME_DEFAULT, 0);
      }
    }
  }
  text_lines = new_text_lines;
}

static void athame_draw_failure()
{
  setenv("ATHAME_ERROR", athame_failure, 1);
  if (athame_is_set("ATHAME_SHOW_ERROR", 1))
  {
    snprintf(athame_buffer, DEFAULT_BUFFER_SIZE-1, "Athame Failure: %s", athame_failure);
    athame_bottom_display(athame_buffer, ATHAME_BOLD, ATHAME_RED, 0);
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

static int athame_handle_special_char(char char_read) {
  //Unless in vim commandline send special chars to readline instead of vim
  if(athame_failure || (strchr(ap_special, char_read) && strcmp(athame_mode, "c") != 0 ))
  {
    return 1;
  }
  ap_set_nospecial();
  return 0;
}

static char athame_process_char(char char_read){
  if (athame_handle_special_char(char_read))
  {
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
    return 0;
  }
}

static void athame_send_to_vim(char input)
{
  write(vim_term, &input, 1);
}

static void athame_get_vim_info(int read, int allow_poll)
{
  if (athame_get_vim_info_inner(read))
  {
    athame_redisplay();
  }
  else if (allow_poll)
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
    read(vim_term, athame_buffer, DEFAULT_BUFFER_SIZE-1);
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
          athame_bottom_display(command, ATHAME_NORMAL, ATHAME_DEFAULT, cmd_pos);
        }
        //Don't record a change because the highlight for incsearch might not have changed yet.
      }
    }
    else
    {
      if(athame_mode[0] == 'c' && athame_is_set("ATHAME_SHOW_COMMAND", 1))
      {
        athame_bottom_display("", ATHAME_NORMAL, ATHAME_DEFAULT, 0);
      }
      if(strcmp(mode, "quit") == 0) {
        // Don't do work if we're quitting.
        return 0;
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
  FILE* sliceFile = fopen(slice_file_name, "r");
  int bytes_read = fread(athame_buffer, 1, DEFAULT_BUFFER_SIZE-1, sliceFile);
  if (bytes_read > 0) {
    // -1 to remove trailing newline
    athame_buffer[bytes_read - 1] = '\0';
  }
  else {
    athame_buffer[0] = '\0';
  }
  fclose(sliceFile);
  return athame_buffer;
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
    return 0;
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
