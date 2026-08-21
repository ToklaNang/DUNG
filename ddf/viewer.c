#define  DDF_IMPLEMENTATION
#include "player.h"
#include "ddf.h"

Vector2 offset  = {0.0f, 0.0f};
float   scale   = 1.0f;

void view_control() {

  if (IsMouseButtonDown(0)) {
    Vector2 mouse_delta = Vector2Scale(GetMouseDelta(), 1.0f);
    offset              = Vector2Add(offset, mouse_delta);
  }
  float scroll = GetMouseWheelMove();
  
  if (scroll > 0) {
    scale += 0.25f;
  } else if (scroll != 0) {
    scale -= (scale - 0.25f > 0) ? 0.25f : 0.0f;
  }
}

void render_2d(ddf_t *ddf) {
  Vector2 p_pos = {ddf->p.pos.x, ddf->p.pos.z};
  p_pos      = Vector2Scale(p_pos, scale);
  p_pos      = Vector2Add(p_pos,   offset);

  Vector2 dir = {_cosDeg(ddf->p.rot.y), _sinDeg(ddf->p.rot.y)};
  dir         = Vector2Scale(dir, scale * 10);

  DrawLineV(p_pos, Vector2Add(p_pos, dir), GREEN);
  DrawCircleV(p_pos, scale * 3, SKYBLUE);

  for (int n = 0; n < ddf->sectors.len; n++) {
    sector_t s = at(ddf->sectors, n);

    for (int j = 0; j < s.n_walls; j++) {
      wall_t w = at(ddf->walls, s.start_wall + j);

      Vector2 p1 = Vector2Scale(w.start, scale);
      Vector2 p2 = Vector2Scale(w.end, scale);

      p1 = Vector2Add(p1, offset);
      p2 = Vector2Add(p2, offset);

      Color color;
      if (w.portal >= 0) {
        color = YELLOW;
      } else {
        color = RED;
      }

      Vector2 m_pos  = GetMousePosition();
      float dist1 = Vector2Distance(m_pos, p1);
      float dist2 = Vector2Distance(m_pos, p2);

      if (dist1 < scale) {
        DrawText(TextFormat("Pos: (%.2f, %.2f)", w.start.x, w.start.y), 10, 10, 24, ORANGE);
      }
      if (dist2 < scale) {
        DrawText(TextFormat("Pos: (%.2f, %.2f)", w.end.x, w.end.y), 10, 10, 24, ORANGE);
      }

      DrawLineV(p1, p2, color);
      DrawCircle(p1.x, p1.y, scale, WHITE);
      DrawCircle(p2.x, p2.y, scale, WHITE);
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Expected an argument");
    return -1;
  }

  ddf_t ddf;
  bool  loaded = 0;

  SetTraceLogLevel(LOG_WARNING);
  InitWindow(800, 800, TextFormat("Viewing %s", argv[1]));
  SetTargetFPS(60);

  while (!WindowShouldClose()) {    
    BeginDrawing(); 
    ClearBackground(DARKGRAY);

    if (IsKeyPressed(KEY_F1)) {
      loaded = 0;
    }

    if (!loaded) {
      ddf    = load_ddf(argv[1]);      
      loaded = 1;
    } else {
      render_2d(&ddf);
      view_control();
    }

    EndDrawing();
  }

  clean_up:
  CloseWindow();
}