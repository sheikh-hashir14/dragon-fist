#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "resource_dir.h"

#define SCREEN_W 1200
#define SCREEN_H 600
#define GROUND_Y 500
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
  f->w = 60.0f;
  f->h = 140.0f;
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
  InitWindow(SCREEN_W, SCREEN_H, "Street Brawler");
  SetTargetFPS(60);
  SetRandomSeed((unsigned int)time(NULL));
 
  // Force working directory to the directory where executable/assets reside using Raylib's ChangeDirectory
  SearchAndSetResourceDir("resources");

  // Load PNG Assets
  Texture2D texP1 = LoadTexture("viper.png");
  Texture2D texP2 = LoadTexture("kai.png");
  Texture2D texBg = LoadBackgroundAsset();

  Fighter p1 = {0};
  Fighter p2 = {0};
  ResetFighter(&p1, 250.0f, 1);
  ResetFighter(&p2, 890.0f, -1);

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

    // Press 'R' to reload background at runtime
    if (IsKeyPressed(KEY_R))
    {
      UnloadTexture(texBg);
      texBg = LoadBackgroundAsset();
    }

    if (state == MENU)
    {
      if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_TWO))
      {
        vsCPU = IsKeyPressed(KEY_ONE);
        ResetFighter(&p1, 250.0f, 1);
        ResetFighter(&p2, 890.0f, -1);
        p1.wins = 0;
        p2.wins = 0;
        round = 1;
        roundTime = 60.0f;
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

    // Draw Stage Background (or gradient fallback if texture isn't loaded)
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
      DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.45f));
      DrawCentered("STREET BRAWLER", 110, 80, WHITE);
      DrawCentered("Press 1 : Player vs CPU", 240, 36, YELLOW);
      DrawCentered("Press 2 : Player vs Player", 290, 36, YELLOW);

      DrawCentered("P1:  A / D move   W jump   S block   F punch   G kick", 390, 26, RAYWHITE);
      DrawCentered("P2:  Left / Right move   Up jump   Down block   . punch   / kick", 430, 26, RAYWHITE);
      DrawCentered("First to win 2 rounds wins the match", 490, 24, LIGHTGRAY);
      DrawCentered("Press 'R' anytime to reload background", 530, 20, ORANGE);
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
