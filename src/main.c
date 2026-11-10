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

Color LogoLerp(Color a, Color b, float t)
{
  return (Color){(unsigned char)(a.r + (b.r - a.r) * t),
                 (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t),
                 255};
}

void DrawCornerOrnament(float x, float y, float sx, float sy)
{
  Color gold = {184, 134, 11, 255};
  Color bronze = {120, 80, 10, 255};

  DrawLineEx((Vector2){x, y}, (Vector2){x + sx * 120.0f, y}, 5.0f, gold);
  DrawLineEx((Vector2){x, y}, (Vector2){x, y + sy * 120.0f}, 5.0f, gold);

  DrawLineEx((Vector2){x + sx * 14.0f, y + sy * 14.0f}, (Vector2){x + sx * 90.0f, y + sy * 14.0f}, 3.0f, bronze);
  DrawLineEx((Vector2){x + sx * 14.0f, y + sy * 14.0f}, (Vector2){x + sx * 14.0f, y + sy * 90.0f}, 3.0f, bronze);

  float qx = sx > 0.0f ? x + 24.0f : x - 24.0f - 30.0f;
  float qy = sy > 0.0f ? y + 24.0f : y - 24.0f - 30.0f;
  DrawRectangleLinesEx((Rectangle){qx, qy, 30.0f, 30.0f}, 4.0f, gold);

  DrawCircleLines((int)(x + sx * 78.0f), (int)(y + sy * 34.0f), 9.0f, gold);
  DrawCircleLines((int)(x + sx * 34.0f), (int)(y + sy * 78.0f), 9.0f, gold);
  DrawLineEx((Vector2){x + sx * 56.0f, y + sy * 24.0f}, (Vector2){x + sx * 70.0f, y + sy * 46.0f}, 3.0f, gold);
  DrawLineEx((Vector2){x + sx * 24.0f, y + sy * 56.0f}, (Vector2){x + sx * 46.0f, y + sy * 70.0f}, 3.0f, gold);
}

void DrawFist(float cx, float cy)
{
  Color outline = {110, 30, 0, 255};

  DrawCircleGradient((int)cx, (int)cy, 150.0f, Fade(YELLOW, 0.95f), Fade(ORANGE, 0.0f));

  DrawRectangleRounded((Rectangle){cx - 66, cy - 10, 132, 78}, 0.3f, 8, outline);
  DrawRectangleRounded((Rectangle){cx - 60, cy - 4, 120, 66}, 0.3f, 8, ORANGE);

  for (int i = 0; i < 4; i++)
  {
    float kx = cx - 60.0f + i * 30.0f;
    DrawRectangleRounded((Rectangle){kx - 2, cy - 48, 32, 56}, 0.5f, 8, outline);
    DrawRectangleRounded((Rectangle){kx + 1, cy - 45, 26, 50}, 0.5f, 8, i % 2 ? GOLD : (Color){255, 190, 40, 255});
    DrawRectangleRounded((Rectangle){kx + 5, cy - 41, 18, 16}, 0.6f, 6, Fade(YELLOW, 0.8f));
  }

  DrawRectangleRounded((Rectangle){cx - 4, cy + 22, 70, 36}, 0.5f, 8, outline);
  DrawRectangleRounded((Rectangle){cx, cy + 25, 62, 30}, 0.5f, 8, (Color){255, 140, 20, 255});

  DrawRectangleRounded((Rectangle){cx - 52, cy + 8, 100, 14}, 0.6f, 6, Fade(RED, 0.35f));
}

void DrawDragonFistLogo(float time)
{
  static float ex[28];
  static float ey[28];
  static float es[28];
  static float ep[28];
  static bool init = false;

  if (!init)
  {
    for (int i = 0; i < 28; i++)
    {
      ex[i] = (float)GetRandomValue(0, SCREEN_W);
      ey[i] = (float)GetRandomValue(0, SCREEN_H);
      es[i] = 20.0f + (float)GetRandomValue(0, 40);
      ep[i] = (float)GetRandomValue(0, 1000) / 1000.0f * 6.0f;
    }
    init = true;
  }

  DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, (Color){10, 16, 48, 255}, (Color){4, 4, 8, 255});

  DrawCornerOrnament(20.0f, 20.0f, 1.0f, 1.0f);
  DrawCornerOrnament(SCREEN_W - 20.0f, 20.0f, -1.0f, 1.0f);
  DrawCornerOrnament(20.0f, SCREEN_H - 20.0f, 1.0f, -1.0f);
  DrawCornerOrnament(SCREEN_W - 20.0f, SCREEN_H - 20.0f, -1.0f, -1.0f);

  float cx = SCREEN_W / 2.0f;
  float pulse = sinf(time * 3.0f) * 14.0f;

  DrawCircleGradient((int)cx, 195, 440.0f + pulse, Fade(RED, 0.35f), Fade(MAROON, 0.0f));
  DrawCircleGradient((int)cx, 190, 330.0f + pulse, Fade(ORANGE, 0.45f), Fade(RED, 0.0f));
  DrawCircleGradient((int)cx, 185, 220.0f + pulse * 0.6f, Fade(YELLOW, 0.50f), Fade(ORANGE, 0.0f));

  DrawFist(cx, 92.0f);

  const char *title = "DRAGON FIST";
  int size = 110;
  int w = MeasureText(title, size);
  int tx = (int)(cx - w / 2.0f);
  int ty = 150;

  for (int d = 14; d >= 8; d--)
    DrawText(title, tx + d, ty + d, size, BLACK);

  for (int d = 8; d >= 1; d--)
    DrawText(title, tx + d, ty + d, size, MAROON);

  for (int a = 0; a < 16; a++)
  {
    float ang = a * (2.0f * PI / 16.0f);
    DrawText(title, tx + (int)(cosf(ang) * 5.0f), ty + (int)(sinf(ang) * 5.0f), size, BLACK);
  }

  for (int a = 0; a < 16; a++)
  {
    float ang = a * (2.0f * PI / 16.0f);
    DrawText(title, tx + (int)(cosf(ang) * 3.0f), ty + (int)(sinf(ang) * 3.0f), size, RED);
  }

  const int bands = 10;
  float bandH = (float)size / bands;
  for (int i = 0; i < bands; i++)
  {
    float t = (float)i / (bands - 1);
    Color c = LogoLerp(YELLOW, ORANGE, t);
    BeginScissorMode(tx - 20, ty + (int)(i * bandH), w + 40, (int)bandH + 1);
    DrawText(title, tx, ty, size, c);
    EndScissorMode();
  }

  float bx = 420.0f;
  float by = 280.0f;
  float bw = 760.0f;
  float bh = 70.0f;
  Color gold = {184, 134, 11, 255};

  DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, (Color){48, 48, 54, 255});
  DrawLineEx((Vector2){bx, by + 3}, (Vector2){bx + bw, by + 3}, 5.0f, gold);
  DrawLineEx((Vector2){bx, by + bh - 3}, (Vector2){bx + bw, by + bh - 3}, 5.0f, gold);
  DrawLineEx((Vector2){bx, by + 10}, (Vector2){bx + bw, by + 10}, 2.0f, (Color){120, 80, 10, 255});
  DrawLineEx((Vector2){bx, by + bh - 10}, (Vector2){bx + bw, by + bh - 10}, 2.0f, (Color){120, 80, 10, 255});

  DrawRectangleGradientH((int)bx - 38, (int)by - 12, 38, (int)bh + 24, GRAY, BLACK);
  DrawRectangleGradientH((int)(bx + bw), (int)by - 12, 38, (int)bh + 24, BLACK, GRAY);
  DrawRectangle((int)bx - 42, (int)by - 18, 46, 8, gold);
  DrawRectangle((int)bx - 42, (int)(by + bh + 10), 46, 8, gold);
  DrawRectangle((int)(bx + bw) - 4, (int)by - 18, 46, 8, gold);
  DrawRectangle((int)(bx + bw) - 4, (int)(by + bh + 10), 46, 8, gold);

  const char *sub = "RISE OF WARRIOR";
  int ss = 44;
  int sx = (int)(cx - MeasureText(sub, ss) / 2.0f);
  int sy = (int)(by + (bh - ss) / 2.0f);
  DrawText(sub, sx + 2, sy + 2, ss, BLACK);
  DrawText(sub, sx, sy, ss, (Color){245, 235, 205, 255});

  for (int i = 0; i < 28; i++)
  {
    float py = fmodf(ey[i] - time * es[i], (float)SCREEN_H);
    if (py < 0.0f)
      py += SCREEN_H;
    float px = ex[i] + sinf(time * 1.5f + ep[i]) * 14.0f;
    float a = 0.6f + 0.4f * sinf(time * 4.0f + ep[i]);
    int sz = 2 + (i % 3) * 2;
    DrawRectangle((int)px, (int)py, sz, sz, Fade(i % 2 ? YELLOW : ORANGE, a));
  }
}

void DrawComputerIcon(float x, float y)
{
  Color beige = {206, 196, 165, 255};
  Color beigeDark = {160, 150, 120, 255};
  Color beigeLight = {228, 220, 192, 255};
  Color keyCol = {238, 232, 210, 255};
  Color phosphor = {150, 255, 170, 255};

  DrawEllipse((int)x + 58, (int)y + 102, 56, 4, Fade(BLACK, 0.35f));

  DrawRectangleRounded((Rectangle){x + 11, y + 7, 84, 64}, 0.12f, 6, beigeDark);
  DrawRectangleRounded((Rectangle){x + 8, y + 4, 84, 64}, 0.12f, 6, beige);
  DrawRectangleRounded((Rectangle){x + 10, y + 5, 80, 14}, 0.4f, 6, Fade(beigeLight, 0.6f));

  DrawRectangleRounded((Rectangle){x + 14, y + 10, 72, 46}, 0.1f, 6, (Color){45, 42, 36, 255});
  DrawRectangleRounded((Rectangle){x + 17, y + 13, 66, 40}, 0.18f, 8, (Color){8, 28, 20, 255});
  DrawRectangleGradientV((int)x + 19, (int)y + 16, 62, 34, (Color){40, 120, 80, 255}, (Color){10, 40, 28, 255});

  DrawText("C:\\>DIR", (int)x + 23, (int)y + 19, 8, phosphor);
  DrawRectangle((int)x + 23, (int)y + 30, 34, 2, Fade(phosphor, 0.8f));
  DrawRectangle((int)x + 23, (int)y + 35, 46, 2, Fade(phosphor, 0.8f));
  DrawRectangle((int)x + 23, (int)y + 40, 28, 2, Fade(phosphor, 0.8f));
  DrawText("C:\\>_", (int)x + 23, (int)y + 44, 8, phosphor);

  for (int sy = 16; sy < 50; sy += 3)
    DrawRectangle((int)x + 19, (int)y + sy, 62, 1, Fade(BLACK, 0.3f));

  DrawTriangle((Vector2){x + 19, y + 16}, (Vector2){x + 19, y + 36}, (Vector2){x + 45, y + 16}, Fade(WHITE, 0.12f));

  DrawRectangle((int)x + 14, (int)y + 59, 26, 4, beigeDark);
  DrawRectangle((int)x + 16, (int)y + 60, 22, 2, GRAY);
  DrawCircle((int)x + 62, (int)y + 61, 3, beigeDark);
  DrawCircle((int)x + 70, (int)y + 61, 3, beigeDark);
  DrawCircle((int)x + 80, (int)y + 61, 3, Fade(LIME, 0.35f));
  DrawCircle((int)x + 80, (int)y + 61, 2, LIME);

  DrawRectangle((int)x + 32, (int)y + 68, 36, 6, beigeDark);
  DrawRectangleRounded((Rectangle){x + 20, y + 73, 60, 6}, 0.5f, 6, beige);
  DrawRectangle((int)x + 22, (int)y + 78, 56, 1, beigeDark);

  DrawRectangleRounded((Rectangle){x + 100, y + 16, 20, 62}, 0.1f, 4, beigeDark);
  DrawRectangleRounded((Rectangle){x + 97, y + 13, 20, 62}, 0.1f, 4, beige);
  for (int i = 0; i < 2; i++)
  {
    DrawRectangle((int)x + 100, (int)y + 18 + i * 14, 14, 9, beigeDark);
    DrawRectangle((int)x + 101, (int)y + 22 + i * 14, 12, 2, BLACK);
    DrawRectangle((int)x + 109, (int)y + 25 + i * 14, 3, 2, GRAY);
  }
  DrawCircle((int)x + 107, (int)y + 50, 4, beigeDark);
  DrawCircle((int)x + 107, (int)y + 50, 2, GRAY);
  DrawCircle((int)x + 101, (int)y + 58, 2, RED);
  DrawCircle((int)x + 101, (int)y + 63, 2, LIME);
  for (int i = 0; i < 4; i++)
    DrawRectangle((int)x + 105, (int)y + 59 + i * 3, 9, 1, beigeDark);

  DrawRectangleRounded((Rectangle){x + 5, y + 85, 88, 16}, 0.25f, 6, beigeDark);
  DrawRectangleRounded((Rectangle){x + 4, y + 83, 88, 15}, 0.25f, 6, beige);
  for (int i = 0; i < 12; i++)
    DrawRectangle((int)x + 8 + i * 7, (int)y + 86, 5, 3, keyCol);
  for (int i = 0; i < 11; i++)
    DrawRectangle((int)x + 11 + i * 7, (int)y + 91, 5, 3, keyCol);
  DrawRectangle((int)x + 22, (int)y + 95, 40, 2, keyCol);
}

void DrawSpriteIcon(Texture2D tex, Rectangle slot, bool flip)
{
  float fw = (float)tex.width / 7.0f;
  float fh = (float)tex.height / 7.0f;
  float s = fminf(slot.width / fw, slot.height / fh);
  float dw = fw * s;
  float dh = fh * s;

  Rectangle src = {0.0f, 0.0f, flip ? -fw : fw, fh};
  Rectangle dst = {slot.x + (slot.width - dw) / 2.0f, slot.y + slot.height - dh, dw, dh};
  DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}

void DrawFightersIcon(float x, float y, Texture2D leftTex, Texture2D rightTex)
{
  if (leftTex.id != 0 && rightTex.id != 0)
  {
    DrawSpriteIcon(leftTex, (Rectangle){x, y, 62.0f, 100.0f}, false);
    DrawSpriteIcon(rightTex, (Rectangle){x + 56.0f, y, 62.0f, 100.0f}, true);
    DrawCircle((int)x + 59, (int)y + 45, 4, YELLOW);
    return;
  }

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

void DrawMenuButton(Rectangle r, const char *label, bool hovered, bool computerIcon, Texture2D leftTex, Texture2D rightTex)
{
  Color bg = hovered ? (Color){90, 40, 20, 255} : (Color){45, 20, 15, 255};
  Color border = hovered ? YELLOW : ORANGE;

  DrawRectangleRec(r, bg);
  DrawRectangleLinesEx(r, 3.0f, border);

  if (computerIcon)
    DrawComputerIcon(r.x + 5, r.y + 2);
  else
    DrawFightersIcon(r.x + 5, r.y + 2, leftTex, rightTex);

  DrawText(label, (int)r.x + 135, (int)r.y + 44, 22, hovered ? YELLOW : RAYWHITE);
}

void DrawControlsPanel(Rectangle r)
{
  static const char *p1Keys[5] = {"A / D", "W", "S", "F", "G"};
  static const char *p2Keys[5] = {"Left / Right", "Up", "Down", ".", "/"};
  static const char *actions[5] = {"Move", "Jump", "Block", "Punch", "Kick"};

  int tx = (int)r.x + 20;
  int ax = (int)r.x + 190;
  int y = (int)r.y + 16;

  DrawRectangleRec(r, Fade(BLACK, 0.55f));
  DrawRectangleLinesEx(r, 3.0f, ORANGE);

  DrawText("CONTROLS", tx, y, 28, YELLOW);
  y += 40;
  DrawRectangle(tx, y, (int)r.width - 40, 2, ORANGE);
  y += 14;

  DrawText("PLAYER 1", tx, y, 22, SKYBLUE);
  y += 32;
  for (int i = 0; i < 5; i++)
  {
    DrawText(p1Keys[i], tx, y, 20, RAYWHITE);
    DrawText(actions[i], ax, y, 20, LIGHTGRAY);
    y += 26;
  }

  y += 14;
  DrawText("PLAYER 2", tx, y, 22, SKYBLUE);
  y += 32;
  for (int i = 0; i < 5; i++)
  {
    DrawText(p2Keys[i], tx, y, 20, RAYWHITE);
    DrawText(actions[i], ax, y, 20, LIGHTGRAY);
    y += 26;
  }
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

  const float btnW = 400.0f;
  const float btnH = 110.0f;
  const float btnGap = 40.0f;
  const float btnStartX = (SCREEN_W - (btnW * 2.0f + btnGap)) / 2.0f;
  const float btnY = (SCREEN_H - btnH) / 2.0f;

  Rectangle btnCPU = {btnStartX, btnY, btnW, btnH};
  Rectangle btnPVP = {btnStartX + btnW + btnGap, btnY, btnW, btnH};

  const float panelW = 320.0f;
  const float panelH = 420.0f;
  Rectangle controlsPanel = {30.0f, (SCREEN_H - panelH) / 2.0f, panelW, panelH};

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

      DrawDragonFistLogo(gameTime);

      Vector2 mouse = GetMousePosition();
      DrawMenuButton(btnCPU, "VS COMPUTER", CheckCollisionPointRec(mouse, btnCPU), true, texP1, texP2);
      DrawMenuButton(btnPVP, "PLAYER VS PLAYER", CheckCollisionPointRec(mouse, btnPVP), false, texP1, texP2);

      DrawControlsPanel(controlsPanel);

      DrawCentered("First to win 2 rounds wins the match", SCREEN_H - 90, 22, LIGHTGRAY);
      DrawCentered("Press 'R' anytime to reload background", SCREEN_H - 55, 20, ORANGE);
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