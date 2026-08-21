#ifndef PLAYER_H
#define PLAYER_H

#include "ddf.h"

#define MOUSE_SENSIVITY 0.09f
#define ACCELERATION    11.0f
#define FRICTION        0.9f
#define AIR_DRAG        0.91f
#define STEP_HEIGHT     12.0f
#define VIEW_HEIGHT     25.0f
#define GRAVITY         9.40f
#define TILT_FACTOR     0.01f
#define MAX_QUEUE       128
#define MAX_SECTOR      64

#define _deg2Rad(deg) ((deg) * (PI / 180.0f))
#define _cosDeg(deg)  (cos(_deg2Rad(deg)))
#define _sinDeg(deg)  (sin(_deg2Rad(deg)))

typedef struct sect_queue sect_queue; 

struct sect_queue { 
  int items[MAX_QUEUE]; 
  int head, tail;
};

static void queue_clear(sect_queue *q);
static bool queue_empty(sect_queue *q);
static void queue_push(sect_queue *q, int sector);
static int  queue_pop(sect_queue *q);

static Vector3 euler_angle(Vector2 v);
static float   point_side(Vector2 p, Vector2 p1, Vector2 p2);
static bool    point_in_sector(ddf_t *ddf, int sector_no);

void camera_movement(player_t *p);
void player_movement(ddf_t *ddf, float delta_time);
void handle_interactions(ddf_t *ddf);
int  interact_decals(ddf_t *ddf);

int  move_pos(ddf_t *ddf, Vector3 *p, Vector3 *velocity, bool *on_ground, int sector, float delta_time);
void player_init(ddf_t *ddf, float fovy);
void player_update(ddf_t *ddf, float delta_time) ;

#endif