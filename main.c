#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wswitch-default"
#include <SDL.h>
#include <SDL_image.h>
#pragma GCC diagnostic pop

// XXX: Defines

#ifndef DEBUG
#define DEBUG 0
#endif

#define RND (rand() / (float)RAND_MAX)

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884f /* pi */
#endif

#ifndef M_PI_2
#define M_PI_2 1.570796326794896619231321691639751442f /* pi/2 */
#endif

#ifndef M_3PI_2
#define M_3PI_2 4.712388980384689857693965074919254326f /* (3*pi)/2 */
#endif

#ifndef M_2PI
#define M_2PI 6.283185307179586476925286766559005768f /* 2*pi */
#endif

// XXX: This magical number is used to calculate tank sprite rotation
// It has been discovered intuitively and empiricaly.
#ifndef M_360_2PI
#define M_360_2PI 57.295779513082320876798154814105170336f /* 360/(2*pi) */
#endif

#define kDegreeToRadian (M_PI / 180.0f)

#define kScreenWidth 1024
#define kScreenHeight 512
#define kScreenPixels (kScreenWidth * kScreenHeight)

// XXX: Data structures

enum eGameState
{
  eGameState_wait,
  eGameState_logo,
  eGameState_title,
  eGameState_game,
};
typedef enum eGameState eGameState;

enum eKey
{
  eKey_up,
  eKey_down,
  eKey_left,
  eKey_right,
  eKey_shoot,
  eKey_cancel,
  eKey_pause,
  eKey_count,
};
typedef enum eKey eKey;

enum eKeyState
{
  eKeyState_off = 0b00,
  eKeyState_up = 0b01,
  eKeyState_pressed = 0b10,
  eKeyState_held = 0b11,
  eKeyState_active_bit = 0b10,
};
typedef enum eKeyState eKeyState;

typedef struct Raycaster Raycaster;
struct Raycaster
{
  SDL_FRect position;
  SDL_FRect offset;
  float angle;
  size_t depth_of_field;
};

typedef struct Player Player;
struct Player
{
  SDL_Color color;
  SDL_FPoint position;
  SDL_FPoint delta;
  float angle;
  SDL_Scancode* key_map;
  int64_t score;
  uint8_t* key_states;
};

typedef struct Level Level;
struct Level
{
  uint8_t* grid;
  size_t width;
  size_t height;
  size_t size;
};

typedef struct ScreenManager ScreenManager;
struct ScreenManager
{
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  SDL_Window* window;
  bool fullscreen;
  size_t height;
  size_t pixels;
  size_t width;
};

typedef struct GameManager GameManager;
struct GameManager
{
  const char* name;
  Level level;
  Player player[4];
  Raycaster raycaster;
  ScreenManager screen_manager;
  bool game_over;
  eGameState state;
  size_t players_count;
  time_t seed;
  uint64_t ticks;
};

// XXX: Global data structure

GameManager g_game_manager = {0};

// XXX: Functions declarations

void SDL_PanicCheck(const bool condition, const char* function);
void debug_printf(const char* fmt, const char* fn, const char* fmt2, ...);
void game_draw(GameManager* gm);
void background_draw(GameManager* gm);
void player_draw(GameManager* gm);
void player_direction_draw(GameManager* gm);
void level_draw(GameManager* gm);
void game_events(GameManager* gm);
void game_load(GameManager* gm);
void game_reset(GameManager* gm);
void game_update(GameManager* gm);
bool key_get(GameManager* gm, size_t player_id, eKey key);
void key_state_update(uint8_t* state, bool is_down);
void player_update(GameManager* gm, size_t player_id);
void sdl_load(GameManager* gm);
void sdl_unload(GameManager* gm);
void sdl_renderer_color_set(SDL_Renderer* renderer, Uint8 red, Uint8 green, Uint8 blue);
void sdl_renderer_color_reset(SDL_Renderer* renderer);
void raycaster_update(GameManager* gm);
void raycaster_ui_draw(GameManager* gm);

// XXX: Debug functions

void
debug_printf(const char* fmt, const char* fn, const char* fmt2, ...)
{
#if DEBUG
  va_list ap;

// #pragma clang diagnostic push
#pragma GCC diagnostic push
// #pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  printf(fmt, fn);
// #pragma clang diagnostic pop
#pragma GCC diagnostic pop

  va_start(ap, fmt2);
  vprintf(fmt2, ap);
  va_end(ap);

  puts("\033[0m");
#else
  (void)fmt;
  (void)fn;
  (void)fmt2;
#endif
}

#define dPrintDebug(...) debug_printf("\033[0;34m[DEBUG] %s ", __func__, __VA_ARGS__);
#define dPrintInfo(...) debug_printf("\033[0;34m[INFO] %s ", __func__, __VA_ARGS__);
#define dPrintNotImplemented() dPrintPanic("not implemented");
#define dPrintStub() debug_printf("\033[0;31m[STUB] %s ", __func__, "%s", "stubbed");
#define dPrintWarn(...) debug_printf("\033[0;33m[WARN] %s ", __func__, __VA_ARGS__);

#define dPrintPanic(...)                                            \
  do                                                                \
    {                                                               \
      debug_printf("\033[0;31m[PANIC] %s ", __func__, __VA_ARGS__); \
      exit(EXIT_FAILURE);                                           \
    }                                                               \
  while (0)

inline void
SDL_PanicCheck(const bool condition, const char* function)
{
#if DEBUG
  if (condition)
    {
      debug_printf("\033[0;31m[PANIC] SDL_%s(): ", function, "%s", SDL_GetError());
      exit(EXIT_FAILURE);
    }
#else
  (void)condition;
  (void)function;
#endif
}

// XXX: Input functions

bool
key_get(GameManager* gm, size_t player_id, eKey key)
{
  Player* player = &gm->player[player_id];

  return player->key_states[key] == eKeyState_pressed || player->key_states[key] == eKeyState_held;
}

void
key_state_update(uint8_t* state, bool is_down)
{
  switch (*state) // look at prev state
    {
    case eKeyState_held:
    case eKeyState_pressed:
      *state = is_down ? eKeyState_held : eKeyState_up;
      break;
    case eKeyState_off:
    case eKeyState_up:
    default:
      *state = is_down ? eKeyState_pressed : eKeyState_off;
      break;
    }
}

// XXX: Player functions

void
player_update(GameManager* gm, size_t player_id)
{
  Player* player = &gm->player[player_id];

  if (key_get(gm, player_id, eKey_up))
    {
      player->position.x += player->delta.x;
      player->position.y += player->delta.y;
    }

  if (key_get(gm, player_id, eKey_down))
    {
      player->position.x -= player->delta.x;
      player->position.y -= player->delta.y;
    }

  if (key_get(gm, player_id, eKey_left))
    {
      player->angle -= 0.1f;
      if (player->angle < 0.0f)
      {
        player->angle += M_2PI;
      }
      player->delta.x = cosf(player->angle) * 5.0f;
      player->delta.y = sinf(player->angle) * 5.0f;
    }

  if (key_get(gm, player_id, eKey_right))
    {
      player->angle += 0.1f;
      if (player->angle > M_2PI)
      {
        player->angle -= M_2PI;
      }
      player->delta.x = cosf(player->angle) * 5.0f;
      player->delta.y = sinf(player->angle) * 5.0f;
    }
}

// XXX: Renderer function

void
sdl_renderer_color_set(SDL_Renderer* renderer, Uint8 red, Uint8 green, Uint8 blue)
{
  const int sdl_setrenderdrawcolor_result = SDL_SetRenderDrawColor(renderer, red, green, blue, 0xff);
  SDL_PanicCheck(sdl_setrenderdrawcolor_result, "SetRenderDrawColor");
}

void
sdl_renderer_color_reset(SDL_Renderer* renderer)
{
  sdl_renderer_color_set(renderer, 0x00, 0x00, 0x00);
}

// XXX: Draw functions
void
background_draw(GameManager* gm)
{
  ScreenManager* sm = &gm->screen_manager;

  sdl_renderer_color_set(sm->renderer, 0x7f, 0x7f, 0x7f);

  const int sdl_renderfillrect_result = SDL_RenderFillRect(sm->renderer, &(const SDL_Rect){.x = 0, .y = 0, .w = (int)sm->width, .h = (int)sm->height});
  SDL_PanicCheck(sdl_renderfillrect_result, "RenderFillRect");

  sdl_renderer_color_reset(sm->renderer);
}

void
player_draw(GameManager* gm)
{
  ScreenManager* sm = &gm->screen_manager;
  Player* player = &gm->player[0];

  sdl_renderer_color_set(sm->renderer, 0xff, 0xff, 0x00);

  const int sdl_renderfillrect_result = SDL_RenderFillRect(sm->renderer, &(const SDL_Rect){.x = (int)(player->position.x - 4.0f), .y = (int)(player->position.y - 4.0f), .w = 8, .h = 8});
  SDL_PanicCheck(sdl_renderfillrect_result, "RenderFillRect");

  sdl_renderer_color_reset(sm->renderer);
}

void
player_direction_draw(GameManager* gm)
{
  ScreenManager* sm = &gm->screen_manager;
  Player* player = &gm->player[0];

  sdl_renderer_color_set(sm->renderer, 0xff, 0xff, 0x00);

  const int sdl_renderdrawline_result = SDL_RenderDrawLine(sm->renderer, (int)player->position.x, (int)player->position.y, (int)(player->position.x + player->delta.x * 5.0f), (int)(player->position.y + player->delta.y * 5.0f));
  SDL_PanicCheck(sdl_renderdrawline_result, "RenderDrawLine");

  sdl_renderer_color_reset(sm->renderer);
}

void
raycaster_ui_draw(GameManager* gm)
{
  ScreenManager* sm = &gm->screen_manager;
  Player* p = &gm->player[0];
  Raycaster* r = &gm->raycaster;

  sdl_renderer_color_set(sm->renderer, 0x00, 0xff, 0x00);

  const int sdl_renderdrawline_result = SDL_RenderDrawLine(sm->renderer, (int)p->position.x, (int)p->position.y, (int)r->position.x, (int)r->position.y);
  SDL_PanicCheck(sdl_renderdrawline_result, "RenderDrawLine");

  sdl_renderer_color_reset(sm->renderer);
}

void
level_draw(GameManager* gm)
{
  ScreenManager* sm = &gm->screen_manager;

  for (size_t i = 0; i < gm->level.width; ++i)
    {
      for (size_t j = 0; j < gm->level.height; ++j)
        {
          if (gm->level.grid[j * gm->level.width + i])
            {
              sdl_renderer_color_set(sm->renderer, 0xff, 0xff, 0xff);
            }
          else
            {
              sdl_renderer_color_set(sm->renderer, 0x00, 0x00, 0x00);
            }

          const int sdl_renderfillrect_result = SDL_RenderFillRect(sm->renderer, &(const SDL_Rect){.x = (int)(i * 64), .y = (int)(j * 64), .w = 63, .h = 63});
          SDL_PanicCheck(sdl_renderfillrect_result, "RenderFillRect");
        }
    }

  sdl_renderer_color_reset(sm->renderer);
}

void
raycaster_update(GameManager* gm)
{
  Player* p = &gm->player[0];
  Raycaster* r = &gm->raycaster;
  int32_t mx = 0;
  int32_t my = 0;
  int32_t mp = 0;

  r->depth_of_field = 0; // depth of field?

  r->position.x = 0.0f;
  r->position.y = 0.0f;
  r->angle = p->angle;
  r->offset.x = 0.0f;
  r->offset.y = 0.0f;

  for (size_t i = 0; i < 1; ++i)
  {
    r->depth_of_field = 0;
    float a_tan = -1.0f / tanf(r->angle);
    if (r->angle > M_PI)
    {
      r->position.y = (float)(((int) p->position.y >> 6) << 6) - 0.0001f;
      r->position.x = (p->position.y - r->position.y) * a_tan + p->position.x;
      r->offset.y = -64.0f;
      r->offset.x = -r->offset.y * a_tan;
    }
    if (r->angle < M_PI)
    {
      r->position.y = (float)(((int) p->position.y >> 6) << 6) + 64.0f;
      r->position.x = (p->position.y - r->position.y) * a_tan + p->position.x;
      r->offset.y = 64.0f;
      r->offset.x = -r->offset.y * a_tan;
    }
    // r->angle == 0 || r->angle == M_PI
    if (r->angle <= SDL_FLT_EPSILON || r->angle - M_PI <= SDL_FLT_EPSILON)
    {
      r->position.x = p->position.x;
      r->position.y = p->position.y;
      r->depth_of_field = 8;
    }
    while (r->depth_of_field < 8)
    {
      mx = (int32_t)(r->position.x)>>6;
      my = (int32_t)(r->position.y)>>6;
      mp = my * (int32_t)gm->level.width + mx;
      
      //TODO: Check that 0 < mp < level.size
      if (mp < 0 || mp > (int32_t)gm->level.size)
      {
        break;
      }

      if ((mp < (int32_t)gm->level.size) && (gm->level.grid[mp] == 1))
      {
        r->depth_of_field=8;
      }
      else
      {
        r->position.x += r->offset.x;
        r->position.y += r->offset.y;
        r->depth_of_field += 1;
      }
      }
    }
  }

  // XXX: Game functions

  void
  game_draw(GameManager* gm)
  {
    ScreenManager* sm = &gm->screen_manager;

    // XXX: Rendering the game to a texture
    const int sdl_setrendertarget_result = SDL_SetRenderTarget(sm->renderer, sm->texture);
    SDL_PanicCheck(sdl_setrendertarget_result, "SetRenderTarget");
    const int sdl_renderclear_result = SDL_RenderClear(sm->renderer);
    SDL_PanicCheck(sdl_renderclear_result, "RenderClear");

    background_draw(gm);
    level_draw(gm);
    raycaster_ui_draw(gm);
    player_draw(gm);
    player_direction_draw(gm);

    // SDL_RenderCopyExF()

    // XXX: Rendering the final texture to screen
    const int sdl_setrendertarget_final_result = SDL_SetRenderTarget(sm->renderer, NULL);
    SDL_PanicCheck(sdl_setrendertarget_final_result, "SetRenderTarget");
    const int sdl_renderclear_final_result = SDL_RenderClear(sm->renderer);
    SDL_PanicCheck(sdl_renderclear_final_result, "RenderClear");
    const int sdl_rendercopy_result = SDL_RenderCopy(sm->renderer, sm->texture, NULL, NULL);
    SDL_PanicCheck(sdl_rendercopy_result, "RenderCopy");

    SDL_RenderPresent(sm->renderer);
  }

  void
  game_events(GameManager* gm)
  {
    SDL_PumpEvents();
    SDL_Event event = {0};

    while (SDL_PollEvent(&event))
      {
        switch (event.type)
          {
          case SDL_QUIT:
            gm->game_over = true;
            break;

          case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
              {
                gm->game_over = true;
                break;
              }
            else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
              {
                if (gm->screen_manager.fullscreen)
                  {
                    const int sdl_showcursor_result = SDL_ShowCursor(1);
                    SDL_PanicCheck(sdl_showcursor_result < 0, "ShowCursor");
                }
            }
          else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
            {
              if (gm->screen_manager.fullscreen)
                {
                  const int sdl_showcursor_result = SDL_ShowCursor(0);
                  SDL_PanicCheck(sdl_showcursor_result < 0, "ShowCursor");
                }
            }
          break;

        default:
          break;
        }
    }

  int numkeys = 0;
  const uint8_t* keystate = SDL_GetKeyboardState(&numkeys);
  SDL_PanicCheck(!keystate, "GetKeyboardState");

  for (size_t p = 0; p < gm->players_count; ++p)
    {
      Player* player = &gm->player[p];

      for (size_t i = 0; i < eKey_count; ++i)
        {
          const int scancode = player->key_map[i];

          bool is_down = false;

          if (scancode && scancode < numkeys)
            {
              is_down |= 0 != keystate[scancode];
            }

          key_state_update(&player->key_states[i], is_down);
        }
    }
}

void
game_load(GameManager* gm)
{
  ScreenManager* sm = &gm->screen_manager;

  srand((unsigned int)gm->seed);

  gm->name = "Coqloup prototype";

  sm->width = kScreenWidth;
  sm->height = kScreenHeight;
  sm->pixels = kScreenPixels;

  gm->seed = time(NULL);

  gm->players_count = 1;

  for (size_t i = 0; i < gm->players_count; ++i)
    {
      gm->player[i].key_states = calloc(eKey_count, sizeof(*gm->player->key_states));
      gm->player[i].key_map = calloc(eKey_count, sizeof(*gm->player->key_map));
    }

  // XXX: Only for testing
  for (size_t i = 0; i < gm->players_count; ++i)
    {
      gm->player[i].key_map[eKey_up] = SDL_SCANCODE_W;
      gm->player[i].key_map[eKey_left] = SDL_SCANCODE_A;
      gm->player[i].key_map[eKey_down] = SDL_SCANCODE_S;
      gm->player[i].key_map[eKey_right] = SDL_SCANCODE_D;
      gm->player[i].key_map[eKey_shoot] = SDL_SCANCODE_SPACE;
      gm->player[i].key_map[eKey_cancel] = SDL_SCANCODE_ESCAPE;
      gm->player[i].key_map[eKey_pause] = SDL_SCANCODE_P;
    }
}

void
game_reset(GameManager* gm)
{
  gm->player[0].position.x = (float)gm->screen_manager.width / 2.0f;
  gm->player[0].position.y = (float)gm->screen_manager.height / 2.0f;
  gm->player[0].delta.x = 0.0f;
  gm->player[0].delta.y = 0.0f;
  gm->player[0].angle = 0.0f;
  gm->player[0].score = 0;

  gm->level.width = 8;
  gm->level.height = 8;
  gm->level.size = gm->level.width * gm->level.height;
  gm->level.grid = malloc(sizeof(*gm->level.grid)*gm->level.size);

  uint8_t level[64] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 1, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
};

  for (size_t i = 0; i < gm->level.width; ++i)
    {
      for (size_t j = 0; j < gm->level.height; ++j)
        {
          size_t cell = j * gm->level.width + i;
          gm->level.grid[cell] = level[cell];
        }
    }
}

void
game_update(GameManager* gm)
{
  gm->ticks = SDL_GetTicks64();

  game_events(gm);

  for (size_t i = 0; i < gm->players_count; ++i)
    {
      player_update(gm, i);
      raycaster_update(gm);
    }
}

// XXX: SDL functions

void
sdl_load(GameManager* gm)
{
  ScreenManager* sm = &gm->screen_manager;

  dPrintInfo("SDL2 initialization");
  const int sdl_init_result = SDL_Init(SDL_INIT_VIDEO);
  SDL_PanicCheck(sdl_init_result, "Init");

  sm->window = SDL_CreateWindow(gm->name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)sm->width, (int)sm->height, SDL_WINDOW_RESIZABLE);
  SDL_PanicCheck(!sm->window, "CreateWindow");

  sm->renderer = SDL_CreateRenderer(sm->window, -1, SDL_RENDERER_PRESENTVSYNC);
  SDL_PanicCheck(!sm->renderer, "CreateRenderer");

  sm->texture = SDL_CreateTexture(sm->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, (int)sm->width, (int)sm->height);
  SDL_PanicCheck(!sm->texture, "CreateTexture");

  const int sdl_rsls_result = SDL_RenderSetLogicalSize(sm->renderer, (int)sm->width, (int)sm->height);
  SDL_PanicCheck(sdl_rsls_result, "RenderSetLogicalSize");
}

void
sdl_unload(GameManager* gm)
{
  ScreenManager* sm = &gm->screen_manager;

  SDL_DestroyTexture(sm->texture);
  SDL_DestroyRenderer(sm->renderer);
  SDL_DestroyWindow(sm->window);
}

// XXX: Main function

int
main(const int argc, const char* argv[])
{
  (void)argc;
  (void)argv;

  game_load(&g_game_manager);
  sdl_load(&g_game_manager);

  game_reset(&g_game_manager);

  // XXX: Main loop
  while (!g_game_manager.game_over)
    {
      game_update(&g_game_manager);
      game_draw(&g_game_manager);
    }

  sdl_unload(&g_game_manager);
  SDL_Quit();

  return EXIT_SUCCESS;
}
