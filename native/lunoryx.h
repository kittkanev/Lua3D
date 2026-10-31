#ifndef LUNORYX_H
#define LUNORYX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LX_MAX_OBJECTS 4096
#define LX_MAX_VERTICES 200000
#define LX_MAX_SCRIPTS 64
#define LX_CODE_SIZE 65536
#define LX_BONES 64
#define LX_PI 3.14159265358979323846f

typedef struct {
  float x, y, z;
} V3;
typedef struct {
  float x, y, z, w;
} Q4;
typedef struct {
  float m[16];
} M4;
typedef struct {
  V3 p, n;
  float uv[2], color[4];
  uint16_t joints[4];
  float weights[4];
} Vertex;
typedef struct {
  Vertex *v;
  uint32_t *i;
  size_t nv, ni, cv, ci;
} Mesh;
typedef struct {
  char name[64];
  int parent;
  V3 position, scale;
  Q4 rotation;
  M4 inverse_bind;
} Bone;
typedef struct {
  int bone, path;
  size_t count;
  float *time;
  Q4 *value;
  int step;
} Track;
typedef struct {
  Bone bones[LX_BONES];
  int count;
  Track *tracks;
  size_t track_count;
  float duration;
  bool procedural;
} Rig;
typedef struct {
  float color[4], metallic, roughness, emission;
  char texture[512], normal[512], shader[2048];
} Material;
typedef struct {
  char name[64], text[256];
  double number;
  int type;
} Property;
typedef enum {
  LX_CUBE,
  LX_SPHERE,
  LX_CYLINDER,
  LX_PLANE,
  LX_WEDGE,
  LX_TERRAIN,
  LX_MESH,
  LX_GROUP,
  LX_UNION,
  LX_ATTACHMENT,
  LX_EFFECT,
  LX_CHARACTER,
  LX_SOUND,
  LX_PYRAMID
} Kind;
typedef struct {
  uint32_t id, parent;
  char name[128];
  Kind kind;
  Mesh mesh;
  Material material;
  V3 position, rotation, scale, pivot, velocity, angular_velocity;
  float mass, bounce, friction;
  bool visible, selected, locked, anchored, operand, negative, grounded;
  float speed, jump;
  Rig *rig;
  Property properties[32];
  int property_count;
  int effect;
  float effect_rate;
  char sound[512];
  float volume, pitch, min_distance, max_distance;
  bool loop, spatial, autoplay;
} Object;
typedef struct {
  uint32_t a, b;
  int type;
  V3 offset, axis;
  float length, stiffness;
} Constraint;
typedef struct {
  char name[64], code[LX_CODE_SIZE];
  bool open, enabled;
  int breakpoints[64], break_count;
} Script;
typedef struct {
  uint32_t id, parent;
  int type, layout;
  char name[64], text[256], image[512], on_click[64];
  float x, y, w, h, anchor_x, anchor_y, scale_x, scale_y, min_w, min_h, max_w,
      max_h, aspect, padding, gap;
  float color[4], text_color[4], rounding;
  bool visible;
  int font_size;
} Gui;
typedef struct {
  Object *objects;
  size_t count, capacity;
  Constraint *constraints;
  size_t constraint_count;
  Script *scripts;
  size_t script_count;
  Gui *gui;
  size_t gui_count;
  uint32_t next_id;
  float sky_top[3], sky_bottom[3], gravity, sun[3];
  char sky[6][512];
  int sky_mode;
  char title[128];
} Scene;
typedef struct {
  char **states;
  size_t count, cursor, bytes;
} History;
typedef struct {
  V3 origin, direction;
} Ray;
V3 v3(float x, float y, float z);
V3 add(V3 a, V3 b);
V3 sub(V3 a, V3 b);
V3 mul(V3 a, float s);
float dot(V3 a, V3 b);
V3 cross(V3 a, V3 b);
float length3(V3 a);
V3 unit(V3 a);
M4 identity(void);
M4 mmul(M4 a, M4 b);
M4 trs(V3 p, Q4 q, V3 s);
M4 inverse(M4 a);
V3 point(M4 a, V3 p);
V3 direction(M4 a, V3 p);
Q4 quat(V3 degrees);
Q4 qslerp(Q4 a, Q4 b, float t);
float wrap_angle(float x);
void mesh_free(Mesh *m);
bool mesh_copy(Mesh *dst, const Mesh *src);
bool mesh_vertex(Mesh *m, Vertex v);
bool mesh_triangle(Mesh *m, uint32_t a, uint32_t b, uint32_t c);
void mesh_normals(Mesh *m);
bool mesh_primitive(Mesh *m, Kind kind);
bool mesh_subdivide(Mesh *m);
bool mesh_sculpt(Mesh *m, V3 center, float radius, float strength, int mode);
bool mesh_csg(Mesh *result, const Mesh *a, const Mesh *b, int operation);
double mesh_volume(const Mesh *m);
void rig_free(Rig *r);
Rig *rig_copy(const Rig *r);
void rig_pose(const Rig *r, float time, bool moving, M4 out[LX_BONES]);
Vertex skin_vertex(Vertex v, const M4 pose[LX_BONES], int count);
void scene_init(Scene *s);
void scene_free(Scene *s);
bool scene_copy(Scene *d, const Scene *s);
Object *scene_find(Scene *s, uint32_t id);
const Object *scene_get(const Scene *s, uint32_t id);
Object *scene_add(Scene *s, Kind kind, const char *name);
bool scene_delete(Scene *s, uint32_t id);
bool scene_duplicate(Scene *s, uint32_t id);
M4 object_world(const Scene *s, const Object *o);
bool object_visible(const Scene *s, const Object *o);
bool scene_reparent(Scene *s, uint32_t id, uint32_t parent);
uint32_t scene_group(Scene *s);
bool scene_separate(Scene *s, uint32_t id);
bool scene_boolean(Scene *s, int operation);
bool scene_join(Scene *s);
void scene_align(Scene *s, int axis);
bool scene_character(Scene *s, float height, float width);
bool scene_constraint(Scene *s, int type);
void scene_step(Scene *s, float dt, float time, V3 input, bool jump);
bool scene_raycast(const Scene *s, Ray ray, uint32_t *id, V3 *hit,
                   float *distance);
bool scene_validate(const Scene *s, char error[256]);
char *scene_json(const Scene *s);
bool scene_parse(Scene *s, const char *text, char error[256]);
bool scene_save(const Scene *s, const char *path, char error[256]);
bool scene_load(Scene *s, const char *path, char error[256]);
bool scene_import(Scene *s, const char *path, char error[256]);
bool scene_export_glb(const Scene *s, const char *path, char error[256]);
void history_free(History *h);
bool history_capture(History *h, const Scene *s);
bool history_undo(History *h, Scene *s);
bool history_redo(History *h, Scene *s);
Script *scene_script(Scene *s, const char *name);
Gui *scene_gui(Scene *s, int type);
#endif
