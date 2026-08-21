#ifndef RENDERER_H
#define RENDERER_H

#include <rlgl.h>

#include "ddf.h"

typedef struct face_t       face_t;
typedef struct edge_t       edge_t;
typedef struct point_t      point_t;
typedef struct intersect_t  intersect_t;
typedef struct triangle_t   triangle_t;
typedef struct texture_mesh texture_mesh;

typedef vector(face_t)       v_face;
typedef vector(edge_t)       v_edge;
typedef vector(point_t)      v_point;
typedef vector(intersect_t)  v_intersect;
typedef vector(triangle_t)   v_triangle;

struct face_t {
  Vector3  center;
  uint32_t texture_id;
};
struct edge_t {
  uint32_t face_id;
  Vector3  start, end;
};
struct point_t {
  uint32_t face_id;
  Vector3  position;
};
struct intersect_t {
  float   t;
  bool    intersect;
  Vector3 position;
};
struct triangle_t {
  uint32_t texture_id;
  Vector3  p1, p2, p3, norm;
};
struct texture_mesh {
  uint32_t texture_id;
  Mesh     mesh;
};

float       polygon2_area(Vector2 *pts, int count);
Vector2     triangle_tex(Vector3 norm, Vector3 p, float tex_scale);
intersect_t point_on_edge(Vector3 p, Vector3 p1, Vector3 p2);

static v_intersect get_intersections(edge_t e, uint32_t edge_id);
static int         cmp_func(const void *arg1, const void *arg2);
static void        push_edges(uint32_t texture_id, Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4);
void get_triangles(ddf_t *ddf, bool static_mode);

static Mesh create_mesh(triangle_t *triangles, size_t len);

void draw_quad(Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4, uint32_t tex);
void draw_triangle(triangle_t t, float tex_scale);

void renderer_init(ddf_t *ddf);
void update_world(ddf_t *ddf, float delta_time);
void render_world(ddf_t *ddf);

#endif