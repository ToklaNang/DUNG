#include "player.h"

void queue_clear(sect_queue *q) 
{
  q->head = q->tail = 0;
}
bool queue_empty(sect_queue *q) 
{
  return q->head == q->tail;
}
void queue_push(sect_queue *q, int sector) 
{
  if (q->tail - q->head < MAX_QUEUE) {
    q->items[q->tail % MAX_QUEUE] = sector;
    q->tail++;
  }
}
int queue_pop(sect_queue *q) 
{
  int n = q->items[q->head % MAX_QUEUE];
  q->head++;
  return n;
}
static Vector3 euler_angle(Vector2 v) 
{
  Vector3 dir = {
      _cosDeg(v.y) * _cosDeg(v.x),
    - _sinDeg(v.x),
      _sinDeg(v.y) * _cosDeg(v.x)
  };
  return Vector3Normalize(dir);
}
static float point_side(Vector2 p, Vector2 p1, Vector2 p2) 
{
  Vector2 v1 = Vector2Subtract(p2, p1);
  Vector2 v2 = Vector2Subtract(p, p1);

  return Vector2CrossProduct(v1, v2);
}
static bool point_in_sector(ddf_t *ddf, int sector_no) 
{
  sector_t s = at(ddf->sectors, sector_no);

  for (int n = 0; n < s.n_walls; n++) {
    wall_t w = at(ddf->walls, s.start_wall + n);
    Vector2    p = {ddf->p.pos.x, ddf->p.pos.z};

    if (point_side(p, w.start, w.end) < 0) {
      return false;
    }
  }
  return true;
}
void camera_movement(player_t *p) 
{
  Vector2 add = Vector2Scale(GetMouseDelta(), MOUSE_SENSIVITY);

  p->rot.y += add.x;
  p->rot.x += (
    p->rot.x + add.y >= -75.0f &&
    p->rot.x + add.y <=  75.0f
  ) ? add.y : 0.0f;

  p->dir = euler_angle(p->rot);

  Vector3 worldUp = (Vector3) { 0.0f, 1.0f, 0.0f };
  Vector3 forward  = euler_angle(Vector2Add(p->rot, (Vector2){p->tilt.x, p->tilt.y}));
  Vector3 side     = Vector3CrossProduct(forward, worldUp);
  Vector3 up       = Vector3CrossProduct(side, forward);

  p->tilt.x *= 0.5f;
  p->tilt.z *= 0.5f;

  float forward_speed = Vector3DotProduct(p->velocity, forward);
  float strafe_speed  = Vector3DotProduct(p->velocity, side);
  p->tilt.x          += forward_speed * TILT_FACTOR;
  p->tilt.z          += strafe_speed * TILT_FACTOR;

  Vector3 axis = Vector3Normalize(Vector3Subtract(p->camera.target, p->camera.position));
  up           = Vector3RotateByAxisAngle(up, axis, _deg2Rad(p->tilt.z));

  p->camera.up       = up;
  p->camera.position = Vector3Add(p->pos, (Vector3){0.0f, VIEW_HEIGHT, 0.0f});
  p->camera.target   = Vector3Add(p->camera.position, forward);
}
void player_movement(ddf_t *ddf, float delta_time) 
{
  Vector3 final_direction = {0.0f, 0.0f, 0.0f};

  Vector3 up   = {0.0f, 1.0f, 0.0f};
  Vector3 side = Vector3CrossProduct(ddf->p.dir, up);

  if (IsKeyDown(KEY_SPACE) && ddf->p.on_ground) {
    ddf->p.velocity.y = 150.0f;
  }
  if (IsKeyDown(KEY_W)) {
    final_direction.x += ddf->p.dir.x;
    final_direction.z += ddf->p.dir.z;    
  }
  if (IsKeyDown(KEY_S)) {
    final_direction.x -= ddf->p.dir.x;
    final_direction.z -= ddf->p.dir.z;
  }
  if (IsKeyDown(KEY_A)) {
    final_direction.x -= side.x;
    final_direction.z -= side.z;
  }
  if (IsKeyDown(KEY_D)) {
    final_direction.x += side.x;
    final_direction.z += side.z;
  }
  final_direction = Vector3Normalize(final_direction);
  final_direction = Vector3Scale(final_direction, ACCELERATION);
  
  ddf->p.velocity.x *= (ddf->p.on_ground) ? FRICTION : AIR_DRAG;
  ddf->p.velocity.z *= (ddf->p.on_ground) ? FRICTION : AIR_DRAG;
  ddf->p.velocity.y -= GRAVITY;

  ddf->p.velocity = Vector3Add(ddf->p.velocity, final_direction);
  move_pos(ddf, &ddf->p.pos, &ddf->p.velocity, &ddf->p.on_ground, ddf->p.sector, delta_time);
}
int interact_decals(ddf_t *ddf)
{
  for (int n = 0; n < ddf->decals.len; n++) {
    decal_t d = at(ddf->decals, n);
    wall_t  w = at(ddf->walls, d.wall_no);
    
    float max_dist = 30.0f;

    Vector3 ray_origin = ddf->p.camera.position;
    Vector3 ray_dir    = ddf->p.dir;

    Vector2 ray_origin_2d = {ray_origin.x, ray_origin.z};
    Vector2 ray_dir_2d    = {ray_dir.x,    ray_dir.z};

    Vector2 wall_vec = Vector2Subtract(w.end, w.start);
    Vector2 to_wall  = Vector2Subtract(w.start, ray_origin_2d);

    // Check if the player is looking the same direction
    // as the wall (parallel).
    float denom = Vector2CrossProduct(ray_dir_2d, wall_vec);
    if (fabs(denom) < EPSILON) 
      continue;
    
    // Check the distance between the player and the decal
    // exceed the limit or not and if the player is not
    // facing away from the decals
    float t = Vector2CrossProduct(to_wall, wall_vec) / denom;
    if (t < 0.0f || t > max_dist) 
      continue;

    // Checking if the player is looking in range of the decals
    // or not (tbh I'm just guessing lol)
    float u_seg = Vector2CrossProduct(to_wall, ray_dir_2d) / denom;
    if (u_seg < 0.0f || u_seg > 1.0f) 
      continue;

    // Get hit position
    Vector3 hit = Vector3Add(ray_origin, Vector3Scale(ray_dir, t));
    if (hit.y < d.bottom || hit.y > d.top) 
      continue;

    // Check if the hit position is in the range of the
    // decals or not (is it colliding the decals or no)
    Vector2 hit_vec = Vector2Subtract((Vector2){hit.x, hit.z}, w.start);
    
    float u = Vector2DotProduct(hit_vec, wall_vec) /
              Vector2DotProduct(wall_vec, wall_vec);

    if (u < d.u1 || u > d.u2) 
      continue;
    
    return n;
  }
  
  return -1; // No decals interacted
}
void handle_interactions(ddf_t *ddf)
{
  // Decals interaction

  int decal_interacted = -1;
  if (
    IsKeyPressed(KEY_E) && 
    (decal_interacted = interact_decals(ddf)) != -1
  ) {
    // Update decal state
    decal_t *d = ref(ddf->decals, decal_interacted);
    d->state = !d->state;
  }
}
int move_pos(ddf_t *ddf, Vector3 *p, Vector3 *velocity, bool *on_ground, int sector, float delta_time) 
{
  Vector3    step = Vector3Scale((*velocity), delta_time);
  sector_t   s    = at(ddf->sectors, sector);

  int   portal     = -1;
  float hitbox_rad  = 7.0f;

  // check collision y
  Vector3 normal = { 0.0f, 1.0f, 0.0f };
  if (p->y + step.y < s.floor_level) {
    p->y = s.floor_level;

    if (velocity->y < 0.0f) {
      velocity->y = 0.0f;
    }
    step.y = 0.0f;

    *on_ground = true;
  } else {
    *on_ground = false;
  }

  // collision xz
  for (int n = 0; n < s.n_walls; n++) {
    wall_t   w   = at(ddf->walls, s.start_wall + n);
    Vector2  pos = {p->x, p->z};
  
    Vector2 wall_dist = Vector2Subtract(w.end, w.start);
    Vector2 to_wall   = Vector2Subtract(pos,  w.start);
    
    float t = Vector2DotProduct(to_wall,   wall_dist) /
              Vector2DotProduct(wall_dist, wall_dist);
    
    t = Clamp(t, 0.0f, 1.0f);

    Vector2 point = Vector2Add(w.start, Vector2Scale(wall_dist, t));
    float   dist  = Vector2Distance(pos, point);

    if (dist > hitbox_rad) continue;
    
    if (
      w.portal >= 0 && 
      (
        p->y + STEP_HEIGHT >= at(ddf->sectors, w.portal).floor_level &&
        p->y + VIEW_HEIGHT <= at(ddf->sectors, w.portal).ceil_level
      )
    ) {
      float d = Vector2DotProduct((Vector2){step.x, step.z}, w.normals);
      if (d < 0) portal = w.portal;

      sector_t neighbor = at(ddf->sectors, w.portal);
      
      float dist = neighbor.floor_level - p->y;
      if (dist > 0 && dist < STEP_HEIGHT) {
        velocity->y = fmaxf(velocity->y, dist * 5.0f);
      }

    } else {
      float   penetration = hitbox_rad - dist;
      Vector3 norm        = {w.normals.x, 0.0f, w.normals.y};

      step = Vector3Subtract(step, Vector3Scale(norm, penetration));
    }
  }

  *p = Vector3Add(*p, step);
  return portal;
}
void player_init(ddf_t *ddf, float fovy)
{
  Camera3D cam;
  cam.position   = ddf->p.pos;
  cam.target     = euler_angle(ddf->p.rot);
  cam.up         = (Vector3){0.0f, 1.0f, 0.0f};
  cam.projection = CAMERA_PERSPECTIVE;
  cam.fovy       = fovy;

  ddf->p.camera    = cam;
  ddf->p.dir       = (Vector3){0.0f, 0.0f, 0.0f};
  ddf->p.velocity  = (Vector3){0.0f, 0.0f, 0.0f};
  ddf->p.tilt      = (Vector3){0.0f, 0.0f, 0.0f};
  ddf->p.on_ground = false;
}
void player_update(ddf_t *ddf, float delta_time) 
{ 
  sect_queue queue = {0};
  queue_clear(&queue);

  bool seen[MAX_SECTOR];
  memset(seen, 0, MAX_SECTOR);

  int found_sect = -1;
  
  if (
    ddf->p.sector <  0 ||
    ddf->p.sector >= ddf->sectors.len
  ) {
    return;
  }
  queue_push(&queue, ddf->p.sector);

  while (!queue_empty(&queue)) {
    int sect_no = queue_pop(&queue);
    if (sect_no < 0) 
      continue;
    
    if (seen[sect_no]) 
      continue;

    seen[sect_no] = true;

    if (point_in_sector(ddf, sect_no)) {
      found_sect = sect_no;
      break;
    }
    
    sector_t s = at(ddf->sectors, sect_no);
    
    for (int n = 0; n < s.n_walls; n++) {
      wall_t w = at(ddf->walls, s.start_wall + n);
      
      if (w.portal < 0) 
        continue;

      queue_push(&queue, w.portal);
    }
  }
  
  if (found_sect >= 0)
    ddf->p.sector = found_sect;

  player_movement(ddf, delta_time);
}