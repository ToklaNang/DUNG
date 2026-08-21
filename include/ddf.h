#ifndef DDF_H
#define DDF_H

#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <raylib.h>
#include <raymath.h>

#include "vector.h"

typedef enum   scan_mode  scan_mode;
typedef struct player_t   player_t;
typedef struct sector_t   sector_t;
typedef struct wall_t     wall_t;
typedef struct decal_t    decal_t;
typedef struct ddf_t      ddf_t;

typedef vector(sector_t)  v_sector;
typedef vector(wall_t)    v_wall;
typedef vector(decal_t)   v_decal;

typedef int(*parse_func)(const char *, uint64_t, ddf_t*);

enum scan_mode {
  SCAN_NONE,
  SCAN_WALL,
  SCAN_DECAL,
  SCAN_SECTOR,
  SCAN_PLAYER,
  SCAN_TEXTURE,
};

struct player_t {
  Vector2  rot;
  uint32_t sector;
  Camera3D camera;
  bool     on_ground;
  Vector3  pos, velocity,
           dir, tilt;
};
struct sector_t {
  int      tag;
  Vector2  center;
  uint32_t start_wall,  n_walls;

  uint32_t bottom_tex,  top_tex;
  uint32_t floor_tex,   ceil_tex;
  
  float    og_floor,    og_ceil;  
  float    floor_level, ceil_level;
};
struct wall_t {
  int     portal;
  int     texture;
  Vector2 normals;
  Vector2 start, end;
};
struct decal_t {
  int      tag;
  bool     state;
  uint32_t wall_no;
  float    u1,   u2;
  uint32_t tex1, tex2;
  float    top,  bottom;
};
struct ddf_t {
  player_t p;
  v_wall   walls;
  v_decal  decals;
  v_sector sectors;

  // Path to every textures, Because our
  // parser need to run without needing raylib
  // initialization so we can only load it later
  vector(char*) textures;
};

static int parse_texture(const char *buff, uint64_t line, ddf_t *out);
static int parse_decal(const char *buff, uint64_t line, ddf_t *out);
static int parse_player(const char *buff, uint64_t line, ddf_t *out);
static int parse_wall(const char *buffer, uint64_t line, ddf_t *out);
static int parse_sector(const char *buffer, uint64_t line, ddf_t *out);

ddf_t load_ddf(const char *fn);
void  unload_ddf(ddf_t *ddf);

#ifdef DDF_IMPLEMENTATION

parse_func parse_lookup[] = {
  [SCAN_WALL]    = parse_wall,
  [SCAN_DECAL]   = parse_decal,
  [SCAN_SECTOR]  = parse_sector,
  [SCAN_PLAYER]  = parse_player,
  [SCAN_TEXTURE] = parse_texture
};

static int parse_texture(const char *buff, uint64_t line, ddf_t *out)
{
  char *fn = (char*)malloc(128);
  if (fn == NULL)
    return 1;

  int ret = sscanf(buff, "%127s", fn);
  if (ret < 1) {
    fprintf(stderr, "Failed to parse texture (line %d)\n", line);
    return 2;
  }

  push_back(out->textures, fn);
  return 0;
}
static int parse_decal(const char *buff, uint64_t line, ddf_t *out)
{
  decal_t d = {0};

  int ret = sscanf(
    buff, "%u %f %f %f %f %u %u %d %d",
    &d.wall_no, &d.u1,
    &d.u2,      &d.top,
    &d.bottom,  &d.tex1,
    &d.tex2,    &d.tag,
    &d.state
  );
  if (ret < 9) {
    fprintf(stderr, "Faild to parse decal (line %d)\n", line);
    return 1;
  }

  push_back(out->decals, d);
  return 0;
}
static int parse_player(const char *buff, uint64_t line, ddf_t *out)
{
  player_t p = {0};

  int ret = sscanf(
    buff, "%f %f %f %f %f %u",
    &p.pos.x, &p.pos.y, &p.pos.z,
    &p.rot.x, &p.rot.y, &p.sector
  );
  if (ret < 6) {
    fprintf(stderr, "Failed to parse player (line %d)\n", line);
    return 1;
  }

  out->p = p;
  return 0;
}
static int parse_wall(const char *buffer, uint64_t line, ddf_t *out)
{
  wall_t w = {0};

  int ret = sscanf(
    buffer, "%f %f %f %f %u %u",
    &w.start.x, &w.start.y,
    &w.end.x,   &w.end.y,
    &w.texture, &w.portal
  );
  if (ret < 6) {
    fprintf(stderr, "Failed to parse wall (line %d)\n", line);
    return 1;
  }

  Vector2 dist = Vector2Subtract(w.end, w.start);
  w.normals   = (Vector2){dist.y, -dist.x};
  w.normals   = Vector2Normalize(w.normals);

  push_back(out->walls, w);
  return 0;
}
static int parse_sector(const char *buffer, uint64_t line, ddf_t *out)
{
  sector_t s = {0};

  int ret = sscanf(
    buffer, "%f %f %u %u %u %u %u %u %d",
    &s.floor_level, &s.ceil_level,
    &s.floor_tex,   &s.ceil_tex,
    &s.bottom_tex,  &s.top_tex,
    &s.start_wall,  &s.n_walls,
    &s.tag
  );
  if (ret < 9) {
    fprintf(stderr, "Failed to parse sector (line %d)\n", line);
    return 1;
  }
  
  s.og_floor = s.floor_level;
  s.og_ceil  = s.ceil_level;

  push_back(out->sectors, s);
  return 0;
}
ddf_t load_ddf(const char *fn)
{
  ddf_t result = {0};

  // Check whether the file extension is right
  // and if there was nothing wrong about the
  // file directory. The function will always
  // produce an output if there was something
  // wrong it would be empty or partially empty 

  const char *extension = strrchr(fn, '.');
  if (
    extension == NULL ||
    strcmp(extension + 1, "ddf") != 0
  ) {
    fprintf(stderr, "Expected .ddf file instead got %s\n", extension);
    goto end_func;
  }

  FILE *f_ptr = fopen(fn, "rb");
  if (f_ptr == NULL) {
    fprintf(stderr, "Failed to open file\n");
    goto end_func;
  }

  // Parse file
  char     buffer[1024];
  uint64_t line = 0;
  
  scan_mode curr_mode = SCAN_NONE;

  while (fgets(buffer, sizeof(buffer), f_ptr) != NULL) {
    line++;

    char *buffer_ptr = buffer;
    while (isspace(*buffer_ptr))
      buffer_ptr++;

    if (*buffer_ptr == '[') {
      char *token = strtok(buffer_ptr, "[]");

      if (strcmp(token, "PLAYER") == 0)
        curr_mode = SCAN_PLAYER;
      else if (strcmp(token, "SECTOR") == 0)
        curr_mode = SCAN_SECTOR;
      else if (strcmp(token, "WALL") == 0)
        curr_mode = SCAN_WALL;
      else if (strcmp(token, "DECAL") == 0)
        curr_mode = SCAN_DECAL;
      else if (strcmp(token, "TEXTURE") == 0)
        curr_mode = SCAN_TEXTURE;
      else
        curr_mode = SCAN_NONE;

      continue;
    } else if (*buffer_ptr == '\0')
      continue;

    if (curr_mode == SCAN_NONE)
      continue;

    parse_lookup[curr_mode](buffer, line, &result);
  }

  // Process that data and fix up some editor problem.
  // Correct wall normals, make the walls always face the
  // center of the sector it's part of.

  for (int i = 0; i < result.sectors.len; i++) {
    sector_t *s = ref(result.sectors, i);

    Vector2 sum = {0.0f, 0.0f};
    for (int j = 0; j < s->n_walls; j++) {
      wall_t w = at(result.walls, s->start_wall + j);

      sum = Vector2Add(sum, w.start);
    }
    s->center = Vector2Divide(sum, (Vector2){s->n_walls, s->n_walls});

    for (int j = 0; j < s->n_walls; j++) {
      wall_t *w = ref(result.walls, s->start_wall + j);
    
      Vector2 wall_center = {
        (w->start.x + w->end.x) * 0.5f,
        (w->start.y + w->end.y) * 0.5f
      };
      Vector2 to_wall = Vector2Subtract(wall_center, s->center);

      float t = Vector2DotProduct(to_wall, w->normals);

      if (t >= 0.0f) 
        continue;
    
      Vector2 tmp = w->start;
      w->start    = w->end;
      w->end      = tmp;

      Vector2 dist = Vector2Subtract(w->end, w->start);
      w->normals   = (Vector2){dist.y, -dist.x};
      w->normals   = Vector2Normalize(w->normals);
    }
  }

  end_func:
  return result;
}
void unload_ddf(ddf_t *ddf)
{
  vector_destroy(ddf->sectors);
  vector_destroy(ddf->decals);
  vector_destroy(ddf->walls);

  for (int n = 0; n < ddf->textures.len; n++)
    free(at(ddf->textures, n));
  
  vector_destroy(ddf->textures);
}

#endif

#endif