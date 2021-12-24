#include <ncurses.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH 100
#define HEIGHT 25
#define END_WIDTH 20
#define END_HEIGHT 8
#define N_BOMBS 200
#define HEADER_HEIGHT 4

typedef struct {
  int min, seconds;
} Timestamp;

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
static const char BG_CHAR='-';
static const char * TITLE = "Minesweeper";
static const float VERSION=1.0;

// State
static int player_x, player_y;
static int h_pad, v_pad, h_end_pad, v_end_pad;
static bool bomb[WIDTH][HEIGHT];
static bool show[WIDTH][HEIGHT];
static bool visited[WIDTH][HEIGHT];
static bool flag[WIDTH][HEIGHT];
static int neighbors[WIDTH][HEIGHT];
static time_t start_time;
static bool first_move, won, lost;
WINDOW * term;
WINDOW * header_win;
WINDOW * game_win;
WINDOW * exit_win;

Timestamp elapsed (void)
{
  Timestamp t = {0, 0};
  time_t now;
  time(&now);
  now -= start_time;
  t.seconds = now % 60;
  t.min = now / 60;
  return t;
}

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

// Returns number of flags close to the player
int n_flags_close ()
{
  int total=0;
  bool neg_x = player_x > 0;
  bool pos_x = player_x < WIDTH - 1;
  bool neg_y = player_y > 0;
  bool pos_y = player_y < HEIGHT - 1;

  if (neg_x && neg_y && flag[player_x-1][player_y-1]) ++total;
  if (neg_x          && flag[player_x-1][ player_y ]) ++total;
  if (neg_x && pos_y && flag[player_x-1][player_y+1]) ++total;
  if (         neg_y && flag[ player_x ][player_y-1]) ++total;
  if (         pos_y && flag[ player_x ][player_y+1]) ++total;
  if (pos_x && neg_y && flag[player_x+1][player_y-1]) ++total;
  if (pos_x          && flag[player_x+1][ player_y ]) ++total;
  if (pos_x && pos_y && flag[player_x+1][player_y+1]) ++total;

  return total;
}

int n_bombs_covered ()
{
  int x, y, total=N_BOMBS;
  for (x=0; x<WIDTH; ++x)
    for (y=0; y<HEIGHT; ++y)
      if (flag[x][y] && bomb[x][y])
        ++total;
  return total;
}

int n_bombs_remaining ()
{
  return N_BOMBS - n_flags();
}

void score_flood (bool marks[WIDTH][HEIGHT], int x, int y)
{
  if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return; // Out of bounds
  if (marks[x][y]) return;
  marks[x][y] = true;
  if (neighbors[x][y] == 0) {
    score_flood(marks, x-1, y-1);
    score_flood(marks, x-1,  y );
    score_flood(marks, x-1, y+1);
    score_flood(marks,  x,  y-1);
    score_flood(marks,  x,  y+1);
    score_flood(marks, x+1, y-1);
    score_flood(marks, x+1,  y );
    score_flood(marks, x+1, y+1);
  }
}

int score ()
{
  int x, y, total=0;
  bool marks[WIDTH][HEIGHT] = {0};
  memcpy(marks, show, sizeof(bool)*WIDTH*HEIGHT);
  for (x=0; x<WIDTH; ++x) {
    for (y=0; y<HEIGHT; ++y) {
      if (neighbors[x][y]) continue; // For every empty cell
      if (bomb[x][y]) continue; // For every empty cell
      if (marks[x][y]) continue;
      score_flood(marks, x, y);
      ++total;
    }
  }
  for (x=0; x<WIDTH; ++x)
    for (y=0; y<HEIGHT; ++y)
      if (!marks[x][y] && !bomb[x][y])
        ++total;
  return total;
}

// Game end functions
void win (void)
{
  char buf[END_WIDTH+1] = {0};
  Timestamp t = elapsed();

  mvwprintw(exit_win, 1, (END_WIDTH-7)/2, "You won");
  sprintf(buf, "Elapsed: %d:%02d", t.min, t.seconds);
  mvwprintw(exit_win, 3, (END_WIDTH-strlen(buf))/2, "%s", buf);
  sprintf(buf, "Bombs: %d/%d", N_BOMBS, N_BOMBS);
  mvwprintw(exit_win, 4, (END_WIDTH-strlen(buf))/2, "%s", buf);
  mvwprintw(exit_win, 6, (END_WIDTH-15)/2, "Press q to exit");
  wmove(exit_win, 6, ((END_WIDTH-15)/2)+6);

  wrefresh(exit_win);
  while (1) {
    if (wgetch(exit_win) == 'q')
      exit(EXIT_SUCCESS);
  }
}

void lose (void)
{
  char buf[END_WIDTH+1] = {0};
  Timestamp t = elapsed();

  mvwprintw(exit_win, 1, (END_WIDTH-8)/2, "You lose");
  sprintf(buf, "Elapsed: %d:%02d", t.min, t.seconds);
  mvwprintw(exit_win, 3, (END_WIDTH-strlen(buf))/2, "%s", buf);
  sprintf(buf, "Bombs: %d/%d", n_bombs_covered(), N_BOMBS);
  mvwprintw(exit_win, 4, (END_WIDTH-strlen(buf))/2, "%s", buf);
  mvwprintw(exit_win, 6, (END_WIDTH-15)/2, "Press q to exit");
  wmove(exit_win, 6, ((END_WIDTH-15)/2)+6);

  wrefresh(exit_win);
  while (1) {
    if (wgetch(exit_win) == 'q')
      exit(EXIT_SUCCESS);
  }
}


// Increment all the points in the neighborhood of the point
void inc_neighborhood (int x, int y)
{
  bool neg_x = x > 0;
  bool pos_x = x < WIDTH - 1;
  bool neg_y = y > 0;
  bool pos_y = y < HEIGHT - 1;

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

// Clear and reset all global state
void clear_state (void)
{
  int x, y;
  for (x=0; x<WIDTH; ++x)
    for (y=0; y<HEIGHT; ++y)
      flag[x][y] = false;
  won = false;
  lost = false;
  player_x = WIDTH / 2;
  player_y = HEIGHT / 2;
}

// Erase and clear all global state
void regenerate_map (void)
{
  int x, y, i;
  time_t t;
  //srand((unsigned) time(&t));
  // Zero out state
  first_move = true;
  for (x=0; x<WIDTH; ++x) {
    for (y=0; y<HEIGHT; ++y) {
      bomb[x][y]      = false;
      show[x][y]      = false;
      visited[x][y]   = false;
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
  getmaxyx(term, v_pad, h_pad);

  v_end_pad = v_pad;
  h_end_pad = h_pad;
  v_end_pad -= END_HEIGHT;
  h_end_pad -= END_WIDTH;
  v_end_pad /= 2;
  h_end_pad /= 2;

  h_pad -= WIDTH+2;
  v_pad -= HEADER_HEIGHT + HEIGHT;
  h_pad /= 2;
  v_pad /= 2;
}

// End the curses session
void destroy_curses (void)
{
  delwin(exit_win);
  delwin(game_win);
  delwin(header_win);
  delwin(term);
  endwin();
  refresh();
}

void fill_win (WINDOW * win, char fill, bool has_border)
{
  int width, height, i, n=0;
  getmaxyx(win, height, width);

  char * line = calloc(width+1, sizeof(char));

  if (has_border) {
    --width;
    --height;
    n=1;
  }

  for (i=0; i<width-n; ++i) line[i] = fill;
  for (i=n; i<height; ++i) mvwprintw(win, i, n, "%s", line);

  free(line);
  wrefresh(win);
}

// Initalize curses
void init_curses (void)
{
  if ((term = initscr()) == NULL ) {
    fprintf(stderr, "Error initialising ncurses.\n");
    exit(EXIT_FAILURE);
  }
  cbreak();
  //fill_win(term, BG_CHAR, false);
  refresh();
  noecho();
  gen_pads();
  header_win = newwin(HEADER_HEIGHT, WIDTH+2, v_pad, h_pad);
  game_win = newwin(HEIGHT+2, WIDTH+2, v_pad+HEADER_HEIGHT, h_pad);
  exit_win = newwin(END_HEIGHT, END_WIDTH, v_end_pad, h_end_pad);
  box(header_win, 0, 0);
  box(game_win, 0, 0);
  box(exit_win, 0, 0);
  wrefresh(header_win);
  wrefresh(game_win);
  // Create windows
  atexit(destroy_curses);
}

// Draw the header
void draw_header (void)
{
  const int center = (WIDTH+3)/2;
  char buf[16]={0};
  int space = WIDTH, s;
  // Title
  mvwprintw(header_win, 0, center-(strlen(TITLE)/2), "%s", TITLE);
  // x y coords
  sprintf(buf, "x: %3d/%3d", player_x+1, WIDTH);
  mvwprintw(header_win, 1, 2, "%s", buf);
  sprintf(buf, "y: %3d/%3d", player_y+1, HEIGHT);
  space -= strlen(buf);
  mvwprintw(header_win, 2, 2, "%s", buf);
  // Version
  sprintf(buf, "v%.3f", VERSION);
  mvwprintw(header_win, 0, WIDTH+2-strlen(buf), "%s", buf);
  // Bombs
  sprintf(buf, "Bombs:%4d", n_bombs_remaining());
  mvwprintw(header_win, 1, WIDTH-strlen(buf), "%s", buf);
  sprintf(buf, "Flags:%4d", n_flags());
  space -= strlen(buf);
  mvwprintw(header_win, 2, WIDTH-strlen(buf), "%s", buf);
  // Score
  s = score();
  if (s == 0) won = true;;
  sprintf(buf, "Score:%3d", s);
  if (space >= strlen(buf)) {
    mvwprintw(header_win, 2, center-(strlen(buf)/2), "%s", buf);
  }
  wrefresh(header_win);
}

void init_screen (void)
{
  init_curses();
  draw_header();
  fill_win(game_win, HIDDEN_CHAR, true);
}

// Game functions
void move_left (void)
{
  --player_x;
  if (player_x < 0) player_x = 0;
}
void move_right (void)
{
  ++player_x;
  if (player_x >= WIDTH-1) player_x = WIDTH-1;
}
void move_up (void)
{
  --player_y;
  if (player_y < 0) player_y = 0;
}
void move_down (void)
{
  ++player_y;
  if (player_y >= HEIGHT-1) player_y = HEIGHT-1;
}

void toggle_flag (void)
{
  if (show[player_x][player_y]) return; // Already guessed this cell
  flag[player_x][player_y] = !flag[player_x][player_y];
  if (flag[player_x][player_y]) {
    mvwaddch(game_win, player_y+1, player_x+1, FLAG_CHAR);
  } else {
    mvwaddch(game_win, player_y+1, player_x+1, HIDDEN_CHAR);
  }
}

// Uncover this cell. If it's zero, uncover it's neighbors
void uncover_cell (int x, int y)
{
  if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return; // Out of bounds
  if (visited[x][y]) return; // Already uncovered
  visited[x][y] = true;
  flag[x][y] = false;
  show[x][y] = true;
  if (neighbors[x][y] == 0) {
    uncover_cell(x-1, y-1);
    uncover_cell(x-1,  y );
    uncover_cell(x-1, y+1);
    uncover_cell( x , y-1);
    uncover_cell( x , y+1);
    uncover_cell(x+1, y-1);
    uncover_cell(x+1,  y );
    uncover_cell(x+1, y+1);
    mvwprintw(game_win, y+1, x+1, "%c", EMPTY_CHAR);
  } else {
    mvwprintw(game_win, y+1, x+1, "%d", neighbors[x][y]);
  }
}

void show_ending_bombs (void)
{
  int x, y;
  for (x=0; x<WIDTH; ++x) {
    for (y=0; y<HEIGHT; ++y) {
      if (flag[x][y] && bomb[x][y]) { // Correct flag
        mvwprintw(game_win, y+1, x+1, "%c", RIGHT_FLAG_CHAR);
      } else if (flag[x][y]) { // Incorrect flag
        mvwprintw(game_win, y+1, x+1, "%c", WRONG_FLAG_CHAR);
      } else if (bomb[x][y]) { // Missed bomb
        mvwprintw(game_win, y+1, x+1, "%c", BOMB_CHAR);
      }
    }
  }
  wrefresh(game_win);
}

void show_cell (void)
{
  if (flag[player_x][player_y]) return; // Can't guess on a flag
  if (show[player_x][player_y]) return;
  // if (show[player_x][player_y]) {
  //   if (neighbors[player_x][player_y] == n_flags_close()) {
  //     uncover_cell(player_x-1, player_y-1);
  //     uncover_cell(player_x-1,  player_y );
  //     uncover_cell(player_x-1, player_y+1);
  //     uncover_cell(player_x+1, player_y-1);
  //     uncover_cell(player_x+1, player_y-1 );
  //     uncover_cell(player_x+1, player_y-1);
  //     uncover_cell(player_x+1,  player_y );
  //     uncover_cell(player_x+1, player_y+1);
  //   }
  //   return;
  // }
  if (bomb[player_x][player_y]) {
    if (first_move) {
      regenerate_map();
      show_cell();
    } else {
      show_ending_bombs();
      lose();
    }
  }
  first_move = false;
  show[player_x][player_y] = true;
  if (neighbors[player_x][player_y] == 0) {
    uncover_cell(player_x, player_y);
  } else {
    mvwprintw(game_win, player_y+1, player_x+1, "%d", neighbors[player_x][player_y]);
  }
}

void update_game (void)
{
  draw_header();
  wmove(game_win, player_y+1, player_x+1);
  if (won) win();
  if (lost) lose();
}

// Run the main game loop
void game_loop (void)
{
  bool hit_exit = false;
  int input;
  time(&start_time);
  update_game();
  while (1) {
    input = wgetch(game_win);
    if (input == 'q') {
      if (hit_exit) exit(EXIT_SUCCESS);
      hit_exit = true;
    } else {
      hit_exit = false;
    }
    switch (input) {
    case KEY_LEFT:
    case 'h':
      move_left();
      break;
    case KEY_DOWN:
    case 'j':
      move_down();
      break;
    case KEY_UP:
    case 'k':
      move_up();
      break;
    case KEY_RIGHT:
    case 'l':
      move_right();
      break;
    case ' ':
      show_cell();
      break;
    case 'f':
      toggle_flag();
      break;
    }
    wrefresh(game_win);
    update_game();
  }
}

int main ()
{
  clear_state();
  regenerate_map();
  init_screen();
  game_loop();
}
