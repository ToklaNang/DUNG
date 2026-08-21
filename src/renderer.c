#include "renderer.h"

static v_face     faces     = VECTOR_EMPTY;
static v_edge     edges     = VECTOR_EMPTY;
static v_point    points    = VECTOR_EMPTY;
static v_triangle triangles = VECTOR_EMPTY;

static Material             default_material;
static vector(texture_mesh) static_geometry = VECTOR_EMPTY;
static vector(Texture2D)    textures        = VECTOR_EMPTY;

float polygon2_area(Vector2 *pts, int count)
{
  float area = 0.0f;
  
  for (int i = 0; i < count; i++) {
    Vector2 a = pts[i];
    Vector2 b = pts[(i + 1) % count];
    
    area += Vector2CrossProduct(a, b);
  }

  return area * 0.5f;
}
Vector2 triangle_tex(Vector3 norm, Vector3 p, float tex_scale) 
{
  if (
    fabs(norm.x) >= fabs(norm.y) && 
    fabs(norm.x) >= fabs(norm.z)
  ) {
    return (Vector2){p.z * tex_scale, p.y * tex_scale};
  } else if (
    fabs(norm.y) >= fabs(norm.x) && 
    fabs(norm.y) >= fabs(norm.z)
  ) {
    return (Vector2){p.x * tex_scale, p.z * tex_scale};
  } else if (
    fabs(norm.z) >= fabs(norm.x) && 
    fabs(norm.z) >= fabs(norm.y)
  ) {
    return (Vector2){p.x * tex_scale, p.y * tex_scale};
  }
  return (Vector2){0.0f, 0.0f};
}
intersect_t point_on_edge(Vector3 p, Vector3 p1, Vector3 p2)
{
  intersect_t result;
  result.t         = 0.0f;
  result.intersect = false;
  result.position  = (Vector3){0.0f, 0.0f, 0.0f};

  if (
    Vector3Equals(p, p1) ||
    Vector3Equals(p, p2)
  )
    goto end;

  Vector3 ab = Vector3Subtract(p2, p1);
  Vector3 ap = Vector3Subtract(p,  p1);

  float t = Vector3DotProduct(ap, ab) /
            Vector3DotProduct(ab, ab);
  
  if (t < 0.0f || t > 1.0f) 
    goto end;

  Vector3 point = Vector3Add(p1, Vector3Scale(ab, t));
  float   dist  = Vector3Distance(p, point);

  if (dist > EPSILON)
    goto end;

  result.t         = t;
  result.position  = point;
  result.intersect = true;

  end:
  return result;
}
static v_intersect get_intersections(edge_t e, uint32_t edge_id)
{
  v_intersect result = VECTOR_EMPTY;

  for (int n = 0; n < edges.len; n++) {
    if (n == edge_id) continue;

    edge_t e1 = at(edges, n);

    intersect_t i1 =  point_on_edge(e1.start, e.start, e.end);
    intersect_t i2 =  point_on_edge(e1.end,   e.start, e.end);

    if (i1.intersect)
      push_back(result, i1);
    
    if (i2.intersect)
      push_back(result, i2);
  }

  return result;
}
static int cmp_func(const void *arg1, const void *arg2)
{
  intersect_t i1 = *(intersect_t*)(arg1);
  intersect_t i2 = *(intersect_t*)(arg2);

  // Sort from smallest to biggest
  return i2.t - i1.t;
}
static void push_edges(uint32_t texture_id, Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4)
{
  push_back(edges, ((edge_t){faces.len, p1, p2}));
  push_back(edges, ((edge_t){faces.len, p2, p3}));
  push_back(edges, ((edge_t){faces.len, p3, p4}));
  push_back(edges, ((edge_t){faces.len, p4, p1}));

  Vector3 center = {
    (p1.x + p2.x + p3.x + p4.x) / 4.0f,
    (p1.y + p2.y + p3.y + p4.y) / 4.0f,
    (p1.z + p2.z + p3.z + p4.z) / 4.0f,
  };

  push_back(faces, ((face_t){center, texture_id}));
}
void get_triangles(ddf_t *ddf, bool static_mode)
{
  // Since everything like edges, faces, triangle
  // is stored globally, Reset it before getting
  // the triangles again
  vector_clear(faces);
  vector_clear(edges);
  vector_clear(points);
  vector_clear(triangles);

  for (int n = 0; n < ddf->sectors.len; n++) {
    sector_t s = at(ddf->sectors, n);

    // if static_mode (get only the triangles of
    // static geometry) is true exclude any sector
    // that have tags aka dynamic geometry 
    if (
      ( static_mode && s.tag >= 0) ||
      (!static_mode && s.tag <  0)
    )
      continue;

    // Get all edges from walls    
    for (int i = 0; i < s.n_walls; i++) {
      wall_t w = at(ddf->walls, s.start_wall + i);

      Vector3 tl = {w.start.x, s.ceil_level,  w.start.y};
      Vector3 tr = {w.end.x,   s.ceil_level,  w.end.y  };
      Vector3 br = {w.end.x,   s.floor_level, w.end.y  };
      Vector3 bl = {w.start.x, s.floor_level, w.start.y};

      if (w.portal >= 0) {
        sector_t neighbor = at(ddf->sectors, w.portal);
        
        Vector3 ntl = {w.start.x, neighbor.ceil_level,  w.start.y};
        Vector3 ntr = {w.end.x,   neighbor.ceil_level,  w.end.y  };
        Vector3 nbr = {w.end.x,   neighbor.floor_level, w.end.y  };
        Vector3 nbl = {w.start.x, neighbor.floor_level, w.start.y};
 
        if (s.floor_level < neighbor.floor_level || s.tag >= 0)
          push_edges(s.bottom_tex, nbl, nbr, br, bl);
        if (s.ceil_level  > neighbor.ceil_level  || s.tag >= 0)
          push_edges(s.top_tex, tl, tr, ntr, ntl);

        continue;
      }

      push_edges(w.texture, tl, tr, br, bl);
    }

    // Floor & Ceiling
    Vector2 points[s.n_walls];
    for (int i = 0; i < s.n_walls; i++) {
      wall_t w = at(ddf->walls, s.start_wall + i);

      points[i] = w.start;
    }
    float area = polygon2_area(points, s.n_walls);

    // If point order were CW, reverse the order to CCW
    if (area < 0.0f)
      for (int i = 0; i < s.n_walls / 2; i++) {
        Vector2 tmp = points[i];

        points[i]                 = points[s.n_walls - 1 - i];
        points[s.n_walls - 1 - i] = tmp;
      }
    
    // Triangulate
    for (int i = 0; i < s.n_walls; i++) {
      Vector2 prev = points[(i - 1 + s.n_walls) % s.n_walls];
      Vector2 curr = points[i];

      Vector3 fp1 = {prev.x,     s.floor_level, prev.y    };
      Vector3 fp2 = {s.center.x, s.floor_level, s.center.y};
      Vector3 fp3 = {curr.x,     s.floor_level, curr.y    };

      Vector3 cp1 = fp1, 
              cp2 = fp2, 
              cp3 = fp3;

      cp1.y = s.ceil_level; 
      cp2.y = s.ceil_level; 
      cp3.y = s.ceil_level;

      push_back(triangles, ((triangle_t){s.floor_tex, fp1, fp2, fp3}));
      push_back(triangles, ((triangle_t){s.ceil_tex, cp1, cp3, cp2}));
    }
  }

  // Get points from those edges, for some fucking
  // reason there's this T-junction ah error which I
  // now need to check if there is an intersection and
  // solve it

  for (int n = 0; n < edges.len; n++) {
    edge_t e = at(edges, n);

    v_intersect intersections = get_intersections(e, n);

    push_back(points, ((point_t){e.face_id, e.start}));

    qsort(
      intersections.items, 
      intersections.len, 
      sizeof(intersect_t),
      cmp_func
    );
    for (int i = 0; i < intersections.len; i++)
      push_back(points, ((point_t){e.face_id, at(intersections, i).position}));

  }

  // Build all the points after modifying that shii to fix
  // the T-junction thing back to walls (as traingles)
  v_point grouped_points = VECTOR_EMPTY;
  for (int n = 0; n < faces.len; n++) {
    face_t f = at(faces, n);
    
    for (int i = 0; i < points.len; i++) {
      point_t p = at(points, i);

      if (p.face_id != n) 
        continue;
      
      push_back(grouped_points, p);
    }

    for (int i = 0; i < grouped_points.len; i++) {
      point_t prev = at(grouped_points, i - 1);
      point_t curr = at(grouped_points, i);

      push_back(triangles, ((triangle_t){f.texture_id, prev.position, f.center, curr.position}));
    }
    vector_clear(grouped_points);
  }

  // Calculate triangle normals
  for (int n = 0; n < triangles.len; n++) {
    triangle_t *t = ref(triangles, n);
  
    Vector3 e1   = Vector3Subtract(t->p2, t->p1);
    Vector3 e2   = Vector3Subtract(t->p3, t->p1);
    Vector3 norm = Vector3CrossProduct(e1, e2);

    t->norm = Vector3Normalize(norm);
  }
}
static Mesh create_mesh(triangle_t *triangles, size_t len)
{
  Mesh m = {0};
  m.triangleCount = len; 
  m.vertexCount   = m.triangleCount * 3;
  m.vertices      = (float*)MemAlloc(m.vertexCount * 3 * sizeof(float));
  m.texcoords     = (float*)MemAlloc(m.vertexCount * 2 * sizeof(float));

  for (int n = 0; n < len; n++) {
    triangle_t t = triangles[n];
    Vector2    tex_coord;

    int offsetVert  = n * 9;
    int offsetUvs   = n * 6; 

    m.vertices[offsetVert + 0] = t.p1.x;
    m.vertices[offsetVert + 1] = t.p1.y;
    m.vertices[offsetVert + 2] = t.p1.z;

    tex_coord                   = triangle_tex(t.norm, t.p1, 0.05f);
    m.texcoords[offsetUvs + 0] = tex_coord.x;
    m.texcoords[offsetUvs + 1] = tex_coord.y;

    m.vertices[offsetVert + 3] = t.p2.x;
    m.vertices[offsetVert + 4] = t.p2.y;
    m.vertices[offsetVert + 5] = t.p2.z;

    tex_coord                   = triangle_tex(t.norm, t.p2, 0.05f);
    m.texcoords[offsetUvs + 2] = tex_coord.x;
    m.texcoords[offsetUvs + 3] = tex_coord.y;
  
    m.vertices[offsetVert + 6] = t.p3.x;
    m.vertices[offsetVert + 7] = t.p3.y;
    m.vertices[offsetVert + 8] = t.p3.z;

    tex_coord                   = triangle_tex(t.norm, t.p3, 0.05f);
    m.texcoords[offsetUvs + 4] = tex_coord.x;
    m.texcoords[offsetUvs + 5] = tex_coord.y;
  }

  UploadMesh(&m, false);
  return m;
}
void draw_quad(Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4, uint32_t tex)
{
  rlBegin(RL_QUADS);
  rlSetTexture(tex);

  rlColor4ub(255, 255, 255, 255);

  rlTexCoord2f(0.0f, 1.0f);
  rlVertex3f(p4.x, p4.y, p4.z);

  rlTexCoord2f(1.0f, 1.0f);
  rlVertex3f(p3.x, p3.y, p3.z);
  
  rlTexCoord2f(1.0f, 0.0f);
  rlVertex3f(p2.x, p2.y, p2.z);
  
  rlTexCoord2f(0.0f, 0.0f);
  rlVertex3f(p1.x, p1.y, p1.z);

  rlEnd();
}
void draw_triangle(triangle_t t, float tex_scale)
{
  rlBegin(RL_TRIANGLES);
  rlSetTexture(at(textures, t.texture_id).id);
  
  rlColor4ub(255, 255, 255, 255);

  Vector2 uv1 = triangle_tex(t.norm, t.p1, tex_scale);
  Vector2 uv2 = triangle_tex(t.norm, t.p2, tex_scale);
  Vector2 uv3 = triangle_tex(t.norm, t.p3, tex_scale);

  rlTexCoord2f(uv1.x, uv1.y);
  rlVertex3f(t.p1.x, t.p1.y, t.p1.z);

  rlTexCoord2f(uv2.x, uv2.y);
  rlVertex3f(t.p2.x, t.p2.y, t.p2.z);

  rlTexCoord2f(uv3.x, uv3.y);
  rlVertex3f(t.p3.x, t.p3.y, t.p3.z);

  rlEnd();
}
void renderer_init(ddf_t *ddf)
{
  for (int n = 0; n < ddf->textures.len; n++) {
    char *fn = at(ddf->textures, n);
  
    if (fn == NULL)
      continue;

    Texture2D t = LoadTexture(fn);
    if (t.id == 0) {
      fprintf(stderr, "Failed to load texture [%s]\n", fn);
      continue;
    }

    push_back(textures, t);
  }

  get_triangles(ddf, true);

  v_triangle group = {0};
  for (int i = 0; i < textures.len; i++) {

    for (int j = 0; j < triangles.len; j++) {
      triangle_t t = at(triangles, j);

      if (t.texture_id != i) continue;
      push_back(group, t);
    }
    if (group.len == 0) continue;

    texture_mesh tm = {0};
    tm.texture_id   = i;
    tm.mesh         = create_mesh(group.items, group.len);
    
    push_back(static_geometry, tm);
    vector_clear(group);
  }

  default_material = LoadMaterialDefault();
}
void update_world(ddf_t *ddf, float delta_time)
{
  for (int i = 0; i < ddf->sectors.len; i++) {
    sector_t *s = ref(ddf->sectors, i);
  
    if (s->tag < 0)
      continue;

    // Find decals that associated with this sector 
    // (decals that have the same tag value as the sector)

    bool activate = true;

    for (int j = 0; j < ddf->decals.len; j++) {
      decal_t d = at(ddf->decals, j);

      if (d.tag != s->tag)
        continue;
      
      if (!d.state)
        activate = false;
    }

    if (!activate) {
      s->ceil_level = (s->ceil_level > s->floor_level)
      ? s->ceil_level - 125.0f * delta_time
      : s->floor_level;
    } else {
      s->ceil_level = (s->ceil_level < s->og_ceil)
      ? s->ceil_level + 125.0f * delta_time
      : s->og_ceil;
    }
  }
}
void render_world(ddf_t *ddf)
{
  // Static geometry
  for (int n = 0; n < static_geometry.len; n++) {
    texture_mesh tm = at(static_geometry, n);
    default_material.maps[MATERIAL_MAP_ALBEDO].texture =
      at(textures, tm.texture_id);

    DrawMesh(tm.mesh, default_material, MatrixIdentity());
  }

  // Dynamic geometry
  get_triangles(ddf, false);

  // Render wall & floor/ceil
  for (int n = 0; n < triangles.len; n++) {
    triangle_t t = at(triangles, n);

    draw_triangle(t, 0.05f);
  }

  // Render decals
  for (int n = 0; n < ddf->decals.len; n++) {
    decal_t d = at(ddf->decals, n);
    wall_t  w = at(ddf->walls, d.wall_no);

    Vector2 wp = Vector2Subtract(w.end, w.start);

    Vector2 p1 = Vector2Add(w.start, Vector2Scale(wp, d.u1));
    Vector2 p2 = Vector2Add(w.start, Vector2Scale(wp, d.u2));
  
    // Push it from the wall a bit to stop overlapping
    p1 = Vector2Subtract(p1, Vector2Scale(w.normals, 0.1f));
    p2 = Vector2Subtract(p2, Vector2Scale(w.normals, 0.1f));

    Vector3 tl = {p1.x, d.top,    p1.y};
    Vector3 tr = {p2.x, d.top,    p2.y};
    Vector3 br = {p2.x, d.bottom, p2.y};
    Vector3 bl = {p1.x, d.bottom, p1.y};

    uint32_t tex_id = (d.state) ? d.tex1 : d.tex2;
    draw_quad(tl, tr, br, bl, textures.items[tex_id].id);
  }
}