#define DDF_IMPLEMENTATION
#include "renderer.h"
#include "player.h"

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "DUNG!"

#define CNV_WIDTH  840
#define CNV_HEIGHT 480

#define GAME_TPS 60

int main()
{
  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);

  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);

  HideCursor();
  DisableCursor();

  ddf_t ddf = load_ddf("assets/maps/map.ddf");

  player_init(&ddf, 90.0f);
  renderer_init(&ddf);

  double accumulator = 0.0f;
  double previous    = GetTime();
  double delta_time  = 1.0f / GAME_TPS;

  RenderTexture canvas = LoadRenderTexture(CNV_WIDTH, CNV_HEIGHT);
  SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);

  while (!WindowShouldClose()) {
    double current  = GetTime();
    accumulator    += current - previous;
    previous        = current;

    while (accumulator >= delta_time) {
      update_world(&ddf, delta_time);
      player_update(&ddf, delta_time);
    
      accumulator -= delta_time;
    }
    camera_movement(&ddf.p);
    handle_interactions(&ddf);

    BeginTextureMode(canvas);
    ClearBackground(WHITE);

    BeginMode3D(ddf.p.camera);
    render_world(&ddf);
    EndMode3D();
    
    EndTextureMode();

    BeginDrawing();
    ClearBackground(DARKGRAY);

    Rectangle src = {0.0f, 0.0f, canvas.texture.width, -canvas.texture.height};
    Rectangle des = {0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT};
    DrawTexturePro(canvas.texture, src, des, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);

    DrawFPS(10, 10);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}