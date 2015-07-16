#include "zle.mdh"
//Note: Most of this code is from zle_params.c

static char* ap_get_line_buffer()
{
  if (zlemetaline != 0)
  {
    return dupstring(zlemetaline);
  }
  return zlelineasstring(zleline, zlell, 0, NULL, NULL, 1);
}

static int ap_get_line_buffer_length()
{
  return zlell;
}

static void ap_set_line_buffer(char* newText)
{
  setline(newText, 0);
}

static int ap_get_cursor()
{
  if (zlemetaline != NULL) {
    /* A lot of work for one number, but still... */
    ZLE_STRING_T tmpline;
    int tmpcs, tmpll, tmpsz;
    char *tmpmetaline = ztrdup(zlemetaline);
    tmpline = stringaszleline(tmpmetaline, zlemetacs,
            &tmpll, &tmpsz, &tmpcs);
    free(tmpmetaline);
    free(tmpline);
    return tmpcs;
  }
  return zlecs;
}

static void ap_set_cursor(int x)
{
  if(x < 0)
  {
    zlecs = 0;
  }
  else if(x > zlell)
  {
    zlecs = zlell;
  }
  else
  {
    zlecs = x;
  }
  fixsuffix();
  menucmp = 0;
}

static void ap_display()
{
  zrefresh();
}

static void ap_force_display()
{
  printf("%s", lpromptbuf);
  fflush(stdout);
  zrefresh();
}

static int ap_get_term_width()
{
  return zterm_columns;
}

static int ap_get_term_height()
{
  return zterm_lines;
}

static int ap_get_prompt_length()
{
  return strlen(lpromptbuf);
}

int history_num;
static void ap_get_history_start()
{
  history_num = 0;
}

static char* ap_get_history_next()
{
  if(history_num++ == 0)
  {
    return "Sorry, athame doesn't yet support zsh's history.";
  }
  return NULL;
}

static void ap_get_history_end()
{
}
