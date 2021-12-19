#include <ncurses.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH 72
#define HEIGHT 32
#define N_BOMBS 100
#define HEADER_HEIGHT 3

// Configs
static const char FLAG_CHAR='F';
static const char HIDDEN_CHAR='.';
static const char EMPTY_CHAR=' ';
static const char BOMB_CHAR='X';
static const char WRONG_FLAG_CHAR='n';
static const char RIGHT_FLAG_CHAR='y';
static const char HORIZ_BORDER_CHAR='=';
static const char VERT_BORDER_CHAR='|';
static const char LL_CORNER_CHAR='\\';
static const char LR_CORNER_CHAR='/';
static const char UL_CORNER_CHAR='\\';
static const char UR_CORNER_CHAR='/';
static const char * TITLE = "Minesweeper";
static const float VERSION=0.01;

// State
static int player_x, player_y;
static int h_pad, v_pad;
static bool bomb[WIDTH][HEIGHT];
static bool show[WIDTH][HEIGHT];
static bool visited[WIDTH][HEIGHT];
static bool flag[WIDTH][HEIGHT];
static int neighbors[WIDTH][HEIGHT];
static WINDOW * term;
static WINDOW * header_win;
static WINDOW * game_win;

// Debug functions
void print_debug_board (void)
{
  int x, y;
  for (y=0; y<HEIGHT; ++y) {
    for (x=0; x<WIDTH; ++x) {
      if (bomb[x][y]) {
        printf("x");
      } else if (neighbors[x][y]) {
        printf("%d", neighbors[x][y]);
      } else {
        printf(" ");
      }
    }
    printf("\n");
  }
}

// Dynamic values
int n_flags ()
{
  int x, y, n=0;
  for (x=0; x<WIDTH; ++x)
    for (y=0; y<HEIGHT; ++y)
      if (flag[x][y])
        ++n;
  return n;
}

int n_bombs_remaining ()
{
  return N_BOMBS - n_flags();
}

int score ()
{
  return 10;
}

// Increment all the points in the neighborhood of the point
void inc_neighborhood (int x, int y)
{
  bool neg_x = x > 0;
  bool pos_x = x < WIDTH - 1;
  bool neg_y = y > 0;
  bool pos_y = y < WIDTH - 1;

  if (neg_x && neg_y) ++neighbors[x-1][y-1];
  if (neg_x         ) ++neighbors[x-1][ y ];
  if (neg_x && pos_y) ++neighbors[x-1][y+1];
  if (         neg_y) ++neighbors[ x ][y-1];
  if (         pos_y) ++neighbors[ x ][y+1];
  if (pos_x && neg_y) ++neighbors[x+1][y-1];
  if (pos_x         ) ++neighbors[x+1][ y ];
  if (pos_x && pos_y) ++neighbors[x+1][y+1];
}

// Move a bomb
// This can only happen on the first move
void move_bomb (int x, int y)
{
  int new_x, new_y;
  bomb[x][y] = false;

  bool neg_x = x > 0;
  bool pos_x = x < WIDTH - 1;
  bool neg_y = y > 0;
  bool pos_y = y < WIDTH - 1;

  if (neg_x && neg_y) --neighbors[x-1][y-1];
  if (neg_x         ) --neighbors[x-1][ y ];
  if (neg_x && pos_y) --neighbors[x-1][y+1];
  if (         neg_y) --neighbors[ x ][y-1];
  if (         pos_y) --neighbors[ x ][y+1];
  if (pos_x && neg_y) --neighbors[x+1][y-1];
  if (pos_x         ) --neighbors[x+1][ y ];
  if (pos_x && pos_y) --neighbors[x+1][y+1];

  while (1) {
    new_x = rand() % WIDTH;
    new_y = rand() % HEIGHT;
      if (!bomb[new_x][new_y] && (new_x != x || new_y != y)) {
        bomb[x][y] = true;
        inc_neighborhood(x, y);
        return;
      }
  }
}

// Erase and clear all global state
void regenerate_map (void)
{
  int x, y, i;
  time_t t;
  srand((unsigned) time(&t));
  // Zero out state
  player_x = WIDTH / 2;
  player_y = HEIGHT / 2;
  for (x=0; x<WIDTH; ++x) {
    for (y=0; y<HEIGHT; ++y) {
      bomb[x][y]      = false;
      show[x][y]      = false;
      visited[x][y]   = false;
      flag[x][y]      = false;
      neighbors[x][y] = 0;
    }
  }
  // Place bombs
  for (i=0; i<N_BOMBS; ++i) {
    while (1) {
      x = rand() % WIDTH;
      y = rand() % HEIGHT;
      if (!bomb[x][y]) {
        bomb[x][y] = true;
        inc_neighborhood(x, y);
        break;
      }
    }
  }
}

// Recalculate padding values
void gen_pads ()
{
  getmaxyx(term, h_pad, v_pad);
  h_pad -= WIDTH+2;
  v_pad -= HEADER_HEIGHT + HEIGHT;
  h_pad /= 2;
  v_pad /= 2;
}

// End the curses session
void destroy_curses (void)
{
  delwin(game_win);
  delwin(header_win);
  delwin(term);
  endwin();
  refresh();
}

// Initalize curses
void init_curses (void)
{
  if ((term = initscr()) == NULL ) {
    fprintf(stderr, "Error initialising ncurses.\n");
    exit(EXIT_FAILURE);
  }
  printw("Initalizing");
  refresh();
  gen_pads();
  header_win = newwin(HEADER_HEIGHT, WIDTH, v_pad, h_pad);
  game_win = newwin(HEIGHT, WIDTH, v_pad+HEADER_HEIGHT, h_pad);
  box(header_win, 1, 0);
  box(game_win, 2, 0);
  wrefresh(header_win);
  wrefresh(game_win);
  wrefresh(term);
  noecho();
  // Create windows
  atexit(destroy_curses);
}

// Draw the header
void draw_header ()
{
  const int center = (WIDTH+3)/2;
  char buf[16]={0};
  int space = WIDTH;
  // Title
  mvwprintw(header_win, 0, center-(strlen(TITLE)/2), TITLE);
  // x y coords
  sprintf(buf, "x: %3d/%3d", player_x, WIDTH);
  mvwprintw(header_win, 1, 0, "%s", buf);
  sprintf(buf, "y: %3d/%3d", player_y, HEIGHT);
  space -= strlen(buf);
  mvwprintw(header_win, 2, 0, "%s", buf);
  // Version
  sprintf(buf, "v%.3f", VERSION);
  mvwprintw(header_win, 0, WIDTH+2-strlen(buf), "%s", buf);
  // Bombs
  sprintf(buf, "Bombs:%4d", n_bombs_remaining());
  mvwprintw(header_win, 1, WIDTH+2-strlen(buf), "%s", buf);
  sprintf(buf, "Flags:%4d", n_flags());
  space -= strlen(buf);
  mvwprintw(header_win, 2, WIDTH+2-strlen(buf), "%s", buf);
  // Score
  sprintf(buf, "Score: %d", score());
  if (space >= strlen(buf)) {
    mvwprintw(header_win, 2, center-(strlen(buf)/2), "%s", buf);
  }
}

void init_screen ()
{
  draw_header();
}

int main ()
{
  regenerate_map();
  init_curses();
  //init_screen();
  draw_header();
  getch();
  //wgetch(game_win);
  wgetch(header_win);
  exit(EXIT_SUCCESS);
}
