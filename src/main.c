#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "resource_dir.h"

#define SCREEN_W 1600
#define SCREEN_H 900
#define GROUND_Y 750
#define MOVE_SPEED 320.0f
#define JUMP_SPEED 850.0f
#define GRAVITY 2200.0f

typedef enum
{
  MENU,
  FIGHT,
  ROUNDEND,
  MATCHEND
} GameState;

typedef struct Move
{
  float startup;
  float active;
  float total;
  int damage;
  float reach;
  float yoff;
  float height;
  float stun;
  float knock;
} Move;

static const Move MOVES[3] = {
    {0},
    {0.08f, 0.12f, 0.32f, 6, 70.0f, 30.0f, 32.0f, 0.28f, 450.0f},
    {0.14f, 0.14f, 0.50f, 11, 100.0f, 90.0f, 34.0f, 0.40f, 600.0f}};

typedef struct Fighter
{
  float x;
  float y;
  float vy;
  float kx;
  float w;
  float h;
  int health;
  int facing;
  bool onGround;
  int attackType;
  float attackTimer;
  bool hitDone;
  float hitstun;
  bool blocking;
  int wins;
  bool isMoving;
} Fighter;

typedef struct Input
{
  int move;
  bool jump;
  bool block;
  bool punch;
  bool kick;
} Input;

void ResetFighter(Fighter *f, float x, int facing)
{
  f->w = 120.0f;
  f->h = 240.0f;
  f->x = x;
  f->y = GROUND_Y - f->h;
  f->vy = 0.0f;
  f->kx = 0.0f;
  f->health = 100;
  f->facing = facing;
  f->onGround = true;
  f->attackType = 0;
  f->attackTimer = 0.0f;
  f->hitDone = false;
  f->hitstun = 0.0f;
  f->blocking = false;
  f->isMoving = false;
}

Rectangle AttackBox(Fighter *f)
{
  const Move *m = &MOVES[f->attackType];
  float bx = f->facing == 1 ? f->x + f->w : f->x - m->reach;
  return (Rectangle){bx, f->y + m->yoff, m->reach, m->height};
}

bool AttackIsActive(Fighter *f)
{
  if (f->attackType == 0)
    return false;
  const Move *m = &MOVES[f->attackType];
  return f->attackTimer >= m->startup && f->attackTimer <= m->startup + m->active;
}

void ApplyHit(Fighter *f, Fighter *o, const Move *m)
{
  if (o->blocking)
  {
    o->health -= 1;
    o->kx = f->facing * 250.0f;
  }
  else
  {
    o->health -= m->damage;
    o->hitstun = m->stun;
    o->kx = f->facing * m->knock;
    o->attackType = 0;
  }
  if (o->health < 0)
    o->health = 0;
}

void UpdateFighter(Fighter *f, Fighter *o, Input in, float dt)
{
  float groundTop = GROUND_Y - f->h;
  f->onGround = f->y >= groundTop;

  if (f->onGround && f->attackType == 0 && f->hitstun <= 0.0f)
    f->facing = o->x > f->x ? 1 : -1;

  f->blocking = false;
  f->isMoving = (in.move != 0);

  if (f->hitstun > 0.0f)
  {
    f->hitstun -= dt;
  }
  else if (f->attackType != 0)
  {
    const Move *m = &MOVES[f->attackType];
    f->attackTimer += dt;

    if (!f->hitDone && AttackIsActive(f))
    {
      Rectangle box = AttackBox(f);
      Rectangle body = {o->x, o->y, o->w, o->h};
      if (CheckCollisionRecs(box, body))
      {
        ApplyHit(f, o, m);
        f->hitDone = true;
      }
    }

    if (f->attackTimer >= m->total)
      f->attackType = 0;
  }
  else
  {
    if (in.block && f->onGround)
    {
      f->blocking = true;
    }
    else
    {
      f->x += in.move * MOVE_SPEED * dt;

      if (in.jump && f->onGround)
      {
        f->vy = -JUMP_SPEED;
        f->onGround = false;
      }

      if (f->onGround && (in.punch || in.kick))
      {
        f->attackType = in.punch ? 1 : 2;
        f->attackTimer = 0.0f;
        f->hitDone = false;
      }
    }
  }

  f->x += f->kx * dt;
  f->kx -= f->kx * 8.0f * dt;

  f->vy += GRAVITY * dt;
  f->y += f->vy * dt;
  if (f->y >= groundTop)
  {
    f->y = groundTop;
    f->vy = 0.0f;
  }

  if (f->x < 0.0f)
    f->x = 0.0f;
  if (f->x > SCREEN_W - f->w)
    f->x = SCREEN_W - f->w;
}

void SeparateFighters(Fighter *a, Fighter *b)
{
  Rectangle ra = {a->x, a->y, a->w, a->h};
  Rectangle rb = {b->x, b->y, b->w, b->h};

  if (CheckCollisionRecs(ra, rb))
  {
    float overlap = a->x < b->x ? (a->x + a->w - b->x) : (b->x + b->w - a->x);
    if (overlap > 0.0f)
    {
      float push = overlap / 2.0f;
      if (a->x < b->x)
      {
        a->x -= push;
        b->x += push;
      }
      else
      {
        a->x += push;
        b->x -= push;
      }
    }
  }

  if (a->x < 0.0f)
    a->x = 0.0f;
  if (b->x < 0.0f)
    b->x = 0.0f;
  if (a->x > SCREEN_W - a->w)
    a->x = SCREEN_W - a->w;
  if (b->x > SCREEN_W - b->w)
    b->x = SCREEN_W - b->w;
}

Input AIInput(Fighter *me, Fighter *opp, float dt)
{
  static float think = 0.0f;
  static int move = 0;
  static bool block = false;

  Input in = {0};
  think -= dt;

  float dist = fabsf(opp->x - me->x);
  int toward = opp->x > me->x ? 1 : -1;

  if (think <= 0.0f)
  {
    think = 0.15f + GetRandomValue(0, 250) / 1000.0f;
    move = 0;
    block = false;

    int r = GetRandomValue(0, 99);
    if (dist > 120.0f)
    {
      if (r < 85)
        move = toward;
      else if (r < 92)
      {
        in.jump = true;
        move = toward;
      }
    }
    else
    {
      if (r < 35)
        in.punch = true;
      else if (r < 55)
        in.kick = true;
      else if (r < 75)
        block = true;
      else
        move = -toward;
    }
  }

  in.move = move;
  in.block = block;
  return in;
}

Color HealthColor(int health)
{
  if (health > 50)
    return GREEN;
  if (health > 25)
    return ORANGE;
  return RED;
}

void DrawFighterSprite(Fighter *f, Texture2D spriteSheet, float gameTime)
{
  DrawEllipse((int)(f->x + f->w / 2), GROUND_Y + 4, 40, 8, Fade(BLACK, 0.4f));

  if (spriteSheet.id == 0)
  {
    DrawRectangle((int)f->x, (int)f->y, (int)f->w, (int)f->h, RED);
    return;
  }

  int row = 0;
  int col = 0;

  if (f->hitstun > 0.0f)
  {
    row = 6;
    col = (int)(f->hitstun * 12.0f) % 2;
  }
  else if (f->attackType == 1)
  {
    row = 4;
    float progress = f->attackTimer / MOVES[1].total;
    col = (int)(progress * 4.0f);
    if (col > 3)
      col = 3;
  }
  else if (f->attackType == 2)
  {
    row = 5;
    float progress = f->attackTimer / MOVES[2].total;
    col = (int)(progress * 4.0f);
    if (col > 3)
      col = 3;
  }
  else if (f->blocking)
  {
    row = 3;
    col = 0;
  }
  else if (!f->onGround)
  {
    row = 2;
    col = f->vy < -200 ? 0 : (f->vy < 200 ? 1 : 2);
  }
  else if (f->isMoving)
  {
    row = 1;
    col = (int)(gameTime * 10.0f) % 4;
  }
  else
  {
    row = 0;
    col = (int)(gameTime * 7.0f) % 7;
  }

  float frameWidth = (float)spriteSheet.width / 7.0f;
  float frameHeight = (float)spriteSheet.height / 7.0f;

  Rectangle srcRec = {
      col * frameWidth,
      row * frameHeight,
      f->facing == 1 ? frameWidth : -frameWidth,
      frameHeight};

  Rectangle destRec = {
      f->x - 30.0f,
      f->y - 20.0f,
      f->w + 60.0f,
      f->h + 20.0f};

  Color tint = (f->hitstun > 0.0f && ((int)(f->hitstun * 30.0f) % 2 == 0)) ? RED : WHITE;

  DrawTexturePro(spriteSheet, srcRec, destRec, (Vector2){0, 0}, 0.0f, tint);

  if (f->blocking)
    DrawRectangleLinesEx((Rectangle){f->x - 6, f->y - 6, f->w + 12, f->h + 12}, 3, SKYBLUE);
}

void DrawHealthBar(Fighter *f, bool left, const char *name)
{
  int barW = 450;
  int barX = left ? 40 : SCREEN_W - 40 - barW;
  int fill = barW * f->health / 100;

  DrawRectangle(barX, 30, barW, 28, DARKGRAY);
  if (left)
    DrawRectangle(barX, 30, fill, 28, HealthColor(f->health));
  else
    DrawRectangle(barX + barW - fill, 30, fill, 28, HealthColor(f->health));
  DrawRectangleLines(barX, 30, barW, 28, WHITE);

  int nameW = MeasureText(name, 24);
  DrawText(name, left ? barX : barX + barW - nameW, 62, 24, WHITE);

  for (int i = 0; i < 2; i++)
  {
    int cx = left ? barX + 12 + i * 26 : barX + barW - 12 - i * 26;
    DrawCircleLines(cx, 105, 9, WHITE);
    if (i < f->wins)
      DrawCircle(cx, 105, 9, GOLD);
  }
}

void DrawCentered(const char *text, int y, int size, Color color)
{
  DrawText(text, SCREEN_W / 2 - MeasureText(text, size) / 2, y, size, color);
}

void DrawFieryText(Font font, const char *text, float size, float cx, float y, float time)
{
  float spacing = size / 8.0f;
  Vector2 dim = MeasureTextEx(font, text, size, spacing);
  Vector2 pos = {cx - dim.x / 2.0f, y};

  float outline = size * 0.06f;
  if (outline < 2.0f)
    outline = 2.0f;
  int depth = (int)(size * 0.10f);
  if (depth < 2)
    depth = 2;

  DrawTextEx(font, text,
             (Vector2){pos.x + depth + outline + 6.0f, pos.y + depth + outline + 6.0f},
             size, spacing, Fade(BLACK, 0.6f));

  for (int d = 0; d <= depth; d += 2)
  {
    for (int a = 0; a < 16; a++)
    {
      float ang = a * (2.0f * PI / 16.0f);
      Vector2 p = {pos.x + d + cosf(ang) * outline, pos.y + d + sinf(ang) * outline};
      DrawTextEx(font, text, p, size, spacing, BLACK);
    }
  }

  for (int d = depth; d >= 1; d--)
  {
    float t = (float)d / (float)depth;
    Color c = {(unsigned char)(160.0f * (1.0f - t) + 45.0f * t),
               (unsigned char)(25.0f * (1.0f - t)),
               0,
               255};
    DrawTextEx(font, text, (Vector2){pos.x + d, pos.y + d}, size, spacing, c);
  }

  static const Color bands[6] = {
      {255, 255, 170, 255},
      {255, 230, 0, 255},
      {255, 170, 0, 255},
      {255, 110, 0, 255},
      {230, 40, 0, 255},
      {170, 10, 10, 255}};

  float bandH = dim.y / 6.0f;
  for (int i = 0; i < 6; i++)
  {
    BeginScissorMode((int)pos.x - 40, (int)(pos.y + i * bandH), (int)dim.x + 80, (int)bandH + 1);
    float wobble = sinf(time * 6.0f + i * 1.3f) * 1.5f;
    DrawTextEx(font, text, (Vector2){pos.x + wobble, pos.y}, size, spacing, bands[i]);
    EndScissorMode();
  }
}

void DrawComputerIcon(float x, float y)
{
  DrawRectangle((int)x + 20, (int)y + 20, 76, 50, DARKGRAY);
  DrawRectangle((int)x + 25, (int)y + 25, 66, 40, SKYBLUE);
  DrawRectangle((int)x + 30, (int)y + 31, 30, 4, WHITE);
  DrawRectangle((int)x + 30, (int)y + 40, 44, 4, WHITE);
  DrawRectangle((int)x + 52, (int)y + 70, 12, 10, GRAY);
  DrawRectangle((int)x + 38, (int)y + 80, 40, 6, GRAY);
}

void DrawFightersIcon(float x, float y)
{
  Color c = WHITE;

  DrawCircle((int)x + 30, (int)y + 32, 8, c);
  DrawLineEx((Vector2){x + 30, y + 40}, (Vector2){x + 30, y + 66}, 3.0f, c);
  DrawLineEx((Vector2){x + 30, y + 48}, (Vector2){x + 56, y + 46}, 3.0f, c);
  DrawLineEx((Vector2){x + 30, y + 48}, (Vector2){x + 18, y + 58}, 3.0f, c);
  DrawLineEx((Vector2){x + 30, y + 66}, (Vector2){x + 20, y + 88}, 3.0f, c);
  DrawLineEx((Vector2){x + 30, y + 66}, (Vector2){x + 42, y + 88}, 3.0f, c);

  DrawCircle((int)x + 92, (int)y + 32, 8, c);
  DrawLineEx((Vector2){x + 92, y + 40}, (Vector2){x + 92, y + 66}, 3.0f, c);
  DrawLineEx((Vector2){x + 92, y + 48}, (Vector2){x + 66, y + 52}, 3.0f, c);
  DrawLineEx((Vector2){x + 92, y + 48}, (Vector2){x + 104, y + 58}, 3.0f, c);
  DrawLineEx((Vector2){x + 92, y + 66}, (Vector2){x + 82, y + 88}, 3.0f, c);
  DrawLineEx((Vector2){x + 92, y + 66}, (Vector2){x + 104, y + 88}, 3.0f, c);

  DrawCircle((int)x + 61, (int)y + 49, 4, YELLOW);
}

void DrawMenuButton(Rectangle r, const char *label, bool hovered, bool computerIcon)
{
  Color bg = hovered ? (Color){90, 40, 20, 255} : (Color){45, 20, 15, 255};
  Color border = hovered ? YELLOW : ORANGE;

  DrawRectangleRec(r, bg);
  DrawRectangleLinesEx(r, 3.0f, border);

  if (computerIcon)
    DrawComputerIcon(r.x + 5, r.y + 2);
  else
    DrawFightersIcon(r.x + 5, r.y + 2);

  DrawText(label, (int)r.x + 135, (int)r.y + 44, 22, hovered ? YELLOW : RAYWHITE);
}

Texture2D LoadBackgroundAsset(void)
{
  Texture2D tex = LoadTexture("background.png");
  if (tex.id == 0)
  {
    tex = LoadTexture("background.jpg");
  }
  return tex;
}

int main()
{
  InitWindow(SCREEN_W, SCREEN_H, "Dragon Fist: Rise of the Warriors");
  SetTargetFPS(60);
  SetRandomSeed((unsigned int)time(NULL));

  SearchAndSetResourceDir("resources");

  Texture2D texP1 = LoadTexture("viper.png");
  Texture2D texP2 = LoadTexture("kai.png");
  Texture2D texBg = LoadBackgroundAsset();

  Font titleFont = GetFontDefault();
  SetTextureFilter(titleFont.texture, TEXTURE_FILTER_POINT);

  Fighter p1 = {0};
  Fighter p2 = {0};
  ResetFighter(&p1, 250.0f, 1);
  ResetFighter(&p2, 890.0f, -1);

  Rectangle btnCPU = {190.0f, 228.0f, 400.0f, 110.0f};
  Rectangle btnPVP = {610.0f, 228.0f, 400.0f, 110.0f};

  GameState state = MENU;
  bool vsCPU = true;
  int round = 1;
  float roundTime = 60.0f;
  float endTimer = 0.0f;
  float gameTime = 0.0f;
  int roundWinner = 0;
  const char *endMsg = "";

  while (!WindowShouldClose())
  {
    float dt = GetFrameTime();
    gameTime += dt;

    if (IsKeyPressed(KEY_R))
    {
      UnloadTexture(texBg);
      texBg = LoadBackgroundAsset();
    }

    if (state == MENU)
    {
      Vector2 mouse = GetMousePosition();
      bool overCPU = CheckCollisionPointRec(mouse, btnCPU);
      bool overPVP = CheckCollisionPointRec(mouse, btnPVP);

      SetMouseCursor((overCPU || overPVP) ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);

      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && (overCPU || overPVP))
      {
        vsCPU = overCPU;
        ResetFighter(&p1, 250.0f, 1);
        ResetFighter(&p2, 890.0f, -1);
        p1.wins = 0;
        p2.wins = 0;
        round = 1;
        roundTime = 60.0f;
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        state = FIGHT;
      }
    }
    else if (state == FIGHT)
    {
      Input i1 = {
          .move = (int)IsKeyDown(KEY_D) - (int)IsKeyDown(KEY_A),
          .jump = IsKeyPressed(KEY_W),
          .block = IsKeyDown(KEY_S),
          .punch = IsKeyPressed(KEY_F),
          .kick = IsKeyPressed(KEY_G)};

      Input i2;
      if (vsCPU)
      {
        i2 = AIInput(&p2, &p1, dt);
      }
      else
      {
        i2 = (Input){
            .move = (int)IsKeyDown(KEY_RIGHT) - (int)IsKeyDown(KEY_LEFT),
            .jump = IsKeyPressed(KEY_UP),
            .block = IsKeyDown(KEY_DOWN),
            .punch = IsKeyPressed(KEY_PERIOD),
            .kick = IsKeyPressed(KEY_SLASH)};
      }

      UpdateFighter(&p1, &p2, i1, dt);
      UpdateFighter(&p2, &p1, i2, dt);
      SeparateFighters(&p1, &p2);

      roundTime -= dt;

      if (p1.health <= 0 || p2.health <= 0 || roundTime <= 0.0f)
      {
        if (p1.health > p2.health)
        {
          p1.wins++;
          roundWinner = 1;
        }
        else if (p2.health > p1.health)
        {
          p2.wins++;
          roundWinner = 2;
        }
        else
        {
          roundWinner = 0;
        }

        endMsg = (p1.health <= 0 || p2.health <= 0) ? "K.O." : "TIME";
        endTimer = 2.5f;
        state = ROUNDEND;
      }
    }
    else if (state == ROUNDEND)
    {
      Input none = {0};
      UpdateFighter(&p1, &p2, none, dt);
      UpdateFighter(&p2, &p1, none, dt);
      SeparateFighters(&p1, &p2);

      endTimer -= dt;
      if (endTimer <= 0.0f)
      {
        if (p1.wins >= 2 || p2.wins >= 2)
        {
          state = MATCHEND;
        }
        else
        {
          round++;
          ResetFighter(&p1, 250.0f, 1);
          ResetFighter(&p2, 890.0f, -1);
          roundTime = 60.0f;
          state = FIGHT;
        }
      }
    }
    else if (state == MATCHEND)
    {
      if (IsKeyPressed(KEY_ENTER))
        state = MENU;
    }

    const char *name2 = vsCPU ? "CPU" : "P2";

    BeginDrawing();

    if (texBg.id != 0)
    {
      DrawTexturePro(texBg, (Rectangle){0, 0, (float)texBg.width, (float)texBg.height},
                     (Rectangle){0, 0, SCREEN_W, SCREEN_H}, (Vector2){0, 0}, 0.0f, WHITE);
    }
    else
    {
      DrawRectangleGradientV(0, 0, SCREEN_W, GROUND_Y, (Color){30, 30, 70, 255}, (Color){220, 100, 60, 255});
      DrawRectangle(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, (Color){50, 40, 35, 255});
    }

    if (state == MENU)
    {
      DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.65f));

      for (int sy = 0; sy < SCREEN_H; sy += 4)
        DrawRectangle(0, sy, SCREEN_W, 1, Fade(BLACK, 0.25f));

      float bob = sinf(gameTime * 2.0f) * 3.0f;
      DrawFieryText(titleFont, "DRAGON FIST", 90.0f, SCREEN_W / 2.0f, 25.0f + bob, gameTime);
      DrawFieryText(titleFont, "RISE OF THE WARRIORS", 38.0f, SCREEN_W / 2.0f, 135.0f + bob, gameTime + 1.0f);

      int barW = 640;
      int barX = SCREEN_W / 2 - barW / 2;
      DrawRectangle(barX, 195, barW, 3, YELLOW);
      DrawRectangle(barX, 200, barW, 3, ORANGE);
      DrawRectangle(barX, 205, barW, 3, RED);

      Vector2 mouse = GetMousePosition();
      DrawMenuButton(btnCPU, "VS COMPUTER", CheckCollisionPointRec(mouse, btnCPU), true);
      DrawMenuButton(btnPVP, "PLAYER VS PLAYER", CheckCollisionPointRec(mouse, btnPVP), false);

      DrawCentered("P1:  A / D move   W jump   S block   F punch   G kick", 375, 24, RAYWHITE);
      DrawCentered("P2:  Left / Right move   Up jump   Down block   . punch   / kick", 410, 24, RAYWHITE);
      DrawCentered("First to win 2 rounds wins the match", 470, 22, LIGHTGRAY);
      DrawCentered("Press 'R' anytime to reload background", 520, 20, ORANGE);
    }
    else
    {
      DrawFighterSprite(&p1, texP1, gameTime);
      DrawFighterSprite(&p2, texP2, gameTime);

      DrawHealthBar(&p1, true, "P1");
      DrawHealthBar(&p2, false, name2);

      int shownTime = (int)ceilf(roundTime < 0.0f ? 0.0f : roundTime);
      DrawCentered(TextFormat("%d", shownTime), 25, 50, WHITE);
      DrawCentered(TextFormat("ROUND %d", round), 80, 24, LIGHTGRAY);

      if (state == FIGHT && roundTime > 58.5f)
        DrawCentered("FIGHT!", SCREEN_H / 2 - 60, 90, YELLOW);

      if (state == ROUNDEND)
      {
        DrawCentered(endMsg, SCREEN_H / 2 - 100, 100, YELLOW);
        if (roundWinner == 0)
          DrawCentered("DRAW", SCREEN_H / 2 + 10, 40, WHITE);
        else
          DrawCentered(TextFormat("%s WINS THE ROUND", roundWinner == 1 ? "P1" : name2), SCREEN_H / 2 + 10, 40, WHITE);
      }

      if (state == MATCHEND)
      {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.65f));
        DrawCentered(TextFormat("%s WINS THE MATCH!", p1.wins > p2.wins ? "P1" : name2), SCREEN_H / 2 - 70, 70, GOLD);
        DrawCentered("Press ENTER to go back to the menu", SCREEN_H / 2 + 30, 30, WHITE);
      }
    }

    EndDrawing();
  }

  UnloadTexture(texP1);
  UnloadTexture(texP2);
  UnloadTexture(texBg);

  CloseWindow();
  return 0;
}