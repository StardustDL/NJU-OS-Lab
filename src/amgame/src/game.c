#include <game.h>

void initScreen();
void splash();
void gameStart();
bool gameMain();
bool gameEnd(bool win);
bool goRound(int direction, bool *win);

uint32_t blocks[4][4];
// 0xcdc1b4,0xeee4da,0xede0c8,0xf2b179,0xf59563,0xf67c5f,0xf65e3b,0xedcf72,0xedcc61,0xedc850,0xedc53f,0xedc22e
uint32_t colors[20] = {0xffffff, 0xff0000, 0xff7f00, 0xffff00, 0x00ff00, 0x00ffff, 0x0000ff, 0x8b00ff};
// L,T,R,D
const int dx[4] = {-1, 0, 1, 0}, dy[4] = {0, -1, 0, 1};

typedef enum
{
  Ended,
  Running,
} GameState;

typedef struct
{
  int w, h;
} Size;

typedef struct
{
  int x, y;
} Position;

Size screenSize, mainSize, blockSize;
Position topLeft;
GameState state;

int main()
{
  // Operating system is a C program
  _ioe_init();
  initScreen();
  bool result = false;
  do
  {
    gameStart();
    result = gameMain();
  } while (gameEnd(result));

  return 0;
}

void gameStart()
{
  puts("Welcome to 7Colors Game on bare-metal.");
  puts("Version 1.0. Copyright (C) 2019-2019 StardustDL.");
  puts("");
  puts("Rules:");
  puts("7Colors game is similar to the famous game 2048, but it's more easy.");
  puts("You will have blocks in 7 colors: red, orange, yellow, green, cyan, blue and purple.");
  puts("Just like 2, 4, 8, 16, 32 and so on.");
  puts("Your goal is to make a purple block by using left/right/up/down key.");
  puts("Each round the game will append a reg block randomly.");
  puts("If there is no empty block to append, you will lose.");
  puts("");

  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
    {
      blocks[i][j] = 0;
    }
  blocks[rand() % 4][rand() % 4] = 1;

  state = Running;
  puts("Game started.");
}

bool goRound(int direction, bool *win)
{
  for (int x = 0; x < 4; x++)
  {
    for (int y = 0; y < 4; y++)
    {
      if (blocks[x][y] == 0)
        continue;
      int lx = x, ly = y;
      int tx = x + dx[direction], ty = y + dy[direction];
      while (tx >= 0 && tx < 4 && ty >= 0 && ty < 4)
      {
        if (blocks[tx][ty] == 0)
          swap(&blocks[tx][ty], &blocks[lx][ly]);
        else if (blocks[tx][ty] == blocks[lx][ly])
        {
          blocks[tx][ty] += 1;
          blocks[lx][ly] = 0;
        }
        else
          break;
        lx = tx, ly = ty;
        tx += dx[direction];
        ty += dy[direction];
      }
    }
  }
  int empty = 0;
  *win = false;
  for (int x = 0; x < 4; x++)
    for (int y = 0; y < 4; y++)
    {
      empty += blocks[x][y] == 0;
      *win = *win || blocks[x][y] >= 7;
    }
  if (empty == 0)
    return false;
  int choice = rand() % empty;
  for (int x = 0; x < 4; x++)
    for (int y = 0; y < 4; y++)
    {
      if (blocks[x][y] == 0)
      {
        if (choice == 0)
        {
          blocks[x][y] = 1;
          return true;
        }
        else
          choice--;
      }
    }
  _halt(1); // Error with add 2
}

bool gameMain()
{
  if (state != Running)
  {
    puts("The game is not running.");
    return false;
  }
  splash();
  while (1)
  {
    uint32_t key = read_press_key();

#ifdef DEBUG
    show_key(key);
#endif

    bool flg = true, win = false;

    switch (key)
    {
    case _KEY_LEFT:
      flg = goRound(0, &win);
      break;
    case _KEY_UP:
      flg = goRound(1, &win);
      break;
    case _KEY_RIGHT:
      flg = goRound(2, &win);
      break;
    case _KEY_DOWN:
      flg = goRound(3, &win);
      break;
    default:
      break;
    }

    splash();
    if (win)
      return true;
    if (!flg)
      return false;
  }
}

bool gameEnd(bool win)
{
  if (win)
  {
    puts("You have got the final color-purple.");
    puts("You WIN!");
  }
  else
  {
    puts("You have no empty block to move.");
    puts("You LOSE!");
  }
  state = Ended;
  puts("Game ended.");
  puts("");
  puts("If you want to start another game, press key R. Any other key to exit the game.");
  if (_KEY_R == read_press_key())
  {
    return true;
  }
  else
  {
    return false;
  }
}

void initScreen()
{
  screenSize.w = screen_width();
  screenSize.h = screen_height();
  if (screenSize.w == screenSize.h)
  {
    topLeft.x = 0;
    topLeft.y = 0;
    mainSize.w = mainSize.h = screenSize.w;
  }
  else if (screenSize.w < screenSize.h)
  {
    topLeft.x = 0;
    topLeft.y = (screenSize.h - screenSize.w) / 2;
    mainSize.w = mainSize.h = screenSize.w;
  }
  else
  {
    topLeft.x = (screenSize.w - screenSize.h) / 2;
    topLeft.y = 0;
    mainSize.w = mainSize.h = screenSize.h;
  }
  blockSize.w = mainSize.w / 4;
  blockSize.h = mainSize.h / 4;

#ifdef DEBUG
  Info("Screen W:%d H:%d", screenSize.w, screenSize.h);
  Info("TopLeft X:%d Y:%d", topLeft.x, topLeft.y);
  Info("MainSize W:%d H:%d", mainSize.w, mainSize.h);
  Info("BlockSize W:%d H:%d", blockSize.w, blockSize.h);
#endif
}

void setSolidColor(Size size, uint32_t color, uint32_t *out)
{
  for (int i = 0; i < size.w * size.h; i++)
  {
    out[i] = color;
  }
}

void splash()
{
  uint32_t pixels[SIDE * SIDE];
  Size side = (Size){SIDE, SIDE};
  for (int x = topLeft.x / SIDE; x * SIDE < topLeft.x + mainSize.w; x++)
  {
    for (int y = topLeft.y / SIDE; y * SIDE < topLeft.y + mainSize.h; y++)
    {
      int val = blocks[(x * SIDE - topLeft.x) / blockSize.w][(y * SIDE - topLeft.y) / blockSize.h];
      setSolidColor(side, colors[val], pixels);
      draw_rect(pixels, x * SIDE, y * SIDE, SIDE, SIDE);
    }
  }
}
