#include "app.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_MAX_DIMENSIONS 8192
#include "stb_image.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GL_FUNCTIONS(X)                                                        \
  X(PFNGLCREATESHADERPROC, CreateShader)                                       \
  X(PFNGLSHADERSOURCEPROC, ShaderSource)                                       \
  X(PFNGLCOMPILESHADERPROC, CompileShader)                                     \
  X(PFNGLGETSHADERIVPROC, GetShaderiv)                                         \
  X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog)                               \
  X(PFNGLDELETESHADERPROC, DeleteShader) X(                                    \
      PFNGLCREATEPROGRAMPROC,                                                  \
      CreateProgram) X(PFNGLATTACHSHADERPROC,                                  \
                       AttachShader) X(PFNGLLINKPROGRAMPROC,                   \
                                       LinkProgram) X(PFNGLGETPROGRAMIVPROC,   \
                                                      GetProgramiv)            \
      X(PFNGLGETPROGRAMINFOLOGPROC,                                            \
        GetProgramInfoLog) X(PFNGLDELETEPROGRAMPROC,                           \
                             DeleteProgram) X(PFNGLUSEPROGRAMPROC, UseProgram) \
          X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) X(                \
              PFNGLUNIFORM1FPROC, Uniform1f) X(PFNGLUNIFORM1IPROC, Uniform1i)  \
              X(PFNGLUNIFORM3FPROC, Uniform3f) X(                              \
                  PFNGLACTIVETEXTUREPROC,                                      \
                  ActiveTexture) X(PFNGLGENFRAMEBUFFERSPROC, GenFramebuffers)  \
                  X(PFNGLBINDFRAMEBUFFERPROC, BindFramebuffer) X(              \
                      PFNGLFRAMEBUFFERTEXTURE2DPROC, FramebufferTexture2D)     \
                      X(PFNGLGENRENDERBUFFERSPROC, GenRenderbuffers) X(        \
                          PFNGLBINDRENDERBUFFERPROC, BindRenderbuffer)         \
                          X(PFNGLRENDERBUFFERSTORAGEPROC, RenderbufferStorage) \
                              X(PFNGLFRAMEBUFFERRENDERBUFFERPROC,              \
                                FramebufferRenderbuffer)                       \
                                  X(PFNGLCHECKFRAMEBUFFERSTATUSPROC,           \
                                    CheckFramebufferStatus)                    \
                                      X(PFNGLDELETEFRAMEBUFFERSPROC,           \
                                        DeleteFramebuffers)                    \
                                          X(PFNGLDELETERENDERBUFFERSPROC,      \
                                            DeleteRenderbuffers)
#define DECL(type, name) static type p##name;
GL_FUNCTIONS(DECL)
typedef struct Cache {
  char path[512];
  unsigned hash;
  GLuint texture, program;
  struct Cache *next;
} Cache;
static const char *vertex_shader =
    "#version 120\nvarying vec3 P,N; varying vec2 UV; varying vec4 C; void "
    "main(){vec4 "
    "p=gl_ModelViewMatrix*gl_Vertex;P=p.xyz;N=normalize(gl_NormalMatrix*gl_"
    "Normal);UV=gl_MultiTexCoord0.xy;C=gl_Color;gl_Position=gl_"
    "ProjectionMatrix*p;}";
static const char *fragment_shader =
    "#version 120\nvarying vec3 P,N;varying vec2 UV;varying vec4 C;uniform "
    "vec3 sun;uniform float "
    "metallic,roughness,emission,has_tex,has_normal;uniform sampler2D "
    "tex,normal_tex;"
    "void main(){vec4 col=C;if(has_tex>0.5)col*=texture2D(tex,UV);vec3 "
    "base=pow(max(col.rgb,vec3(0.0)),vec3(2.2));vec3 "
    "n=normalize(N);if(!gl_FrontFacing)n=-n;if(has_normal>0.5){vec3 "
    "q1=dFdx(P),q2=dFdy(P);vec2 u1=dFdx(UV),u2=dFdy(UV);vec3 "
    "T=normalize(q1*u2.y-q2*u1.y),B=normalize(-q1*u2.x+q2*u1.x);n=normalize("
    "mat3(T,B,n)*(texture2D(normal_tex,UV).xyz*2.0-1.0));}vec3 "
    "v=normalize(-P),l=normalize(sun),h=normalize(v+l);float "
    "nv=max(dot(n,v),0.001),nl=max(dot(n,l),0.0),nh=max(dot(n,h),0.0),vh=max("
    "dot(v,h),0.0);float "
    "a=max(roughness*roughness,0.002),a2=a*a,d=nh*nh*(a2-1.0)+1.0;float "
    "D=a2/(3.141593*d*d),k=(roughness+1.0)*(roughness+1.0)/8.0;float "
    "G=nv/(nv*(1.0-k)+k)*nl/(nl*(1.0-k)+k);vec3 "
    "f0=mix(vec3(0.04),base,metallic),F=f0+(1.0-f0)*pow(1.0-vh,5.0);vec3 "
    "spec=D*G*F/(4.0*nv*max(nl,0.001));vec3 "
    "rgb=((1.0-F)*(1.0-metallic)*base/"
    "3.141593+spec)*nl*3.0+base*(0.12+max(n.y,0.0)*0.12)+base*emission;rgb=rgb/"
    "(rgb+vec3(1.0));gl_FragColor=vec4(pow(rgb,vec3(1.0/2.2)),col.a);}";
static const char *grid_vertex = "#version 120\nvarying vec3 world;void "
                                 "main(){world=gl_Vertex.xyz;gl_Position=gl_"
                                 "ModelViewProjectionMatrix*gl_Vertex;}";
static const char *grid_fragment =
    "#version 120\nvarying vec3 world;uniform float cell,axis,extent;uniform "
    "vec3 eye;void main(){vec2 p=world.xz/cell;vec2 "
    "w=max(fwidth(p),vec2(0.00001));vec2 g=abs(fract(p-0.5)-0.5)/w;float "
    "minor=1.0-min(min(g.x,g.y),1.0);vec2 majorP=p/10.0;vec2 "
    "gm=abs(fract(majorP-0.5)-0.5)/max(fwidth(majorP),vec2(0.00001));float "
    "major=1.0-min(min(gm.x,gm.y),1.0);vec3 "
    "color=mix(vec3(0.30,0.36,0.43),vec3(0.50,0.57,0.66),major);float "
    "alpha=max(minor*0.32,major*0.62);if(axis>0.5){float "
    "x=1.0-min(abs(world.z)/"
    "max(fwidth(world.z),0.001),1.0),z=1.0-min(abs(world.x)/"
    "max(fwidth(world.x),0.001),1.0);color=mix(color,vec3(0.9,0.24,0.28),x);"
    "color=mix(color,vec3(0.25,0.5,0.95),z);alpha=max(alpha,max(x,z)*0.9);}"
    "alpha*=1.0-smoothstep(extent*0.5,extent,length(world-eye));gl_FragColor="
    "vec4(color,alpha);}";
static GLuint program(Renderer *r, const char *vs, const char *fs) {
  GLuint vert = pCreateShader(GL_VERTEX_SHADER),
         frag = pCreateShader(GL_FRAGMENT_SHADER);
  pShaderSource(vert, 1, &vs, NULL);
  pShaderSource(frag, 1, &fs, NULL);
  pCompileShader(vert);
  pCompileShader(frag);
  GLint ok = 0;
  pGetShaderiv(vert, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    pGetShaderInfoLog(vert, sizeof(r->error), NULL, r->error);
    goto fail;
  }
  pGetShaderiv(frag, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    pGetShaderInfoLog(frag, sizeof(r->error), NULL, r->error);
    goto fail;
  }
  GLuint id = pCreateProgram();
  pAttachShader(id, vert);
  pAttachShader(id, frag);
  pLinkProgram(id);
  pGetProgramiv(id, GL_LINK_STATUS, &ok);
  pDeleteShader(vert);
  pDeleteShader(frag);
  if (!ok) {
    pGetProgramInfoLog(id, sizeof(r->error), NULL, r->error);
    pDeleteProgram(id);
    return 0;
  }
  return id;
fail:
  pDeleteShader(vert);
  pDeleteShader(frag);
  return 0;
}
bool renderer_init(Renderer *r) {
  memset(r, 0, sizeof(*r));
#define LOAD(type, name)                                                       \
  {                                                                            \
    void *address = SDL_GL_GetProcAddress("gl" #name);                         \
    if (!address)                                                              \
      address = SDL_GL_GetProcAddress("gl" #name "EXT");                       \
    memcpy(&p##name, &address, sizeof(address));                               \
    if (!p##name) {                                                            \
      snprintf(r->error, sizeof(r->error), "OpenGL function missing: gl%s",    \
               #name);                                                         \
      return false;                                                            \
    }                                                                          \
  }
  GL_FUNCTIONS(LOAD)
  r->program = program(r, vertex_shader, fragment_shader);
  r->grid_program = program(r, grid_vertex, grid_fragment);
  if (!r->program || !r->grid_program)
    return false;
  pGenFramebuffers(1, &r->fbo);
  glGenTextures(1, &r->color);
  pGenRenderbuffers(1, &r->depth);
  return true;
}
void renderer_clear_cache(Renderer *r) {
  Cache *p = r->cache;
  while (p) {
    Cache *n = p->next;
    if (p->texture)
      glDeleteTextures(1, &p->texture);
    if (p->program)
      pDeleteProgram(p->program);
    free(p);
    p = n;
  }
  r->cache = NULL;
}
void renderer_free(Renderer *r) {
  renderer_clear_cache(r);
  if (r->program)
    pDeleteProgram(r->program);
  if (r->grid_program)
    pDeleteProgram(r->grid_program);
  if (r->fbo)
    pDeleteFramebuffers(1, &r->fbo);
  if (r->depth)
    pDeleteRenderbuffers(1, &r->depth);
  if (r->color)
    glDeleteTextures(1, &r->color);
}
GLuint renderer_texture(Renderer *r, const char *path) {
  if (!path[0])
    return 0;
  for (Cache *p = r->cache; p; p = p->next)
    if (!strcmp(p->path, path))
      return p->texture;
  Cache *c = calloc(1, sizeof(*c));
  if (!c)
    return 0;
  snprintf(c->path, sizeof(c->path), "%s", path);
  c->next = r->cache;
  r->cache = c;
  int w, h, n;
  unsigned char *pixels = stbi_load(path, &w, &h, &n, 4);
  if (!pixels) {
    snprintf(r->error, sizeof(r->error), "Cannot load image: %.500s", path);
    return 0;
  }
  glGenTextures(1, &c->texture);
  glBindTexture(GL_TEXTURE_2D, c->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);
  stbi_image_free(pixels);
  return c->texture;
}
static GLuint custom_shader(Renderer *r, const char *source) {
  unsigned hash = 2166136261u;
  for (const unsigned char *p = (const unsigned char *)source; *p; p++)
    hash = (hash ^ *p) * 16777619u;
  for (Cache *c = r->cache; c; c = c->next)
    if (!c->path[0] && c->hash == hash)
      return c->program ? c->program : r->program;
  Cache *c = calloc(1, sizeof(*c));
  if (!c)
    return r->program;
  c->hash = hash;
  c->program = program(r, vertex_shader, source);
  c->next = r->cache;
  r->cache = c;
  return c->program ? c->program : r->program;
}
static void uniform1(GLuint p, const char *name, float v) {
  pUniform1f(pGetUniformLocation(p, name), v);
}
static M4 camera_view(const App *a) {
  V3 f = unit(sub(a->target, a->camera)), r = unit(cross(f, v3(0, 1, 0))),
     u = cross(r, f);
  M4 m = {{r.x, u.x, -f.x, 0, r.y, u.y, -f.y, 0, r.z, u.z, -f.z, 0,
           -dot(r, a->camera), -dot(u, a->camera), dot(f, a->camera), 1}};
  return m;
}
static void vertex(Vertex v) {
  glNormal3f(v.n.x, v.n.y, v.n.z);
  glTexCoord2f(v.uv[0], v.uv[1]);
  glColor4fv(v.color);
  glVertex3f(v.p.x, v.p.y, v.p.z);
}
static void sky(App *a) {
  Renderer *r = &a->renderer;
  glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1, 1, 1, -1, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  pUseProgram(0);
  glBegin(GL_QUADS);
  glColor3fv(a->scene.sky_bottom);
  glVertex2f(-1, -1);
  glVertex2f(1, -1);
  glColor3fv(a->scene.sky_top);
  glVertex2f(1, 1);
  glVertex2f(-1, 1);
  glEnd();
  if (!a->scene.sky_mode)
    return;
  glEnable(GL_TEXTURE_2D);
  glColor3f(1, 1, 1);
  V3 forward = unit(sub(a->target, a->camera)),
     right = unit(cross(forward, v3(0, 1, 0))), up = cross(right, forward);
  float tangent = tanf(a->settings.fov * LX_PI / 360),
        aspect = (float)r->w / r->h;
  if (a->scene.sky_mode == 1) {
    GLuint tex = renderer_texture(r, a->scene.sky[0]);
    glBindTexture(GL_TEXTURE_2D, tex);
    int nx = 48, ny = 24;
    for (int y = 0; y < ny; y++) {
      glBegin(GL_TRIANGLE_STRIP);
      for (int x = 0; x <= nx; x++)
        for (int k = 0; k < 2; k++) {
          float sx = (float)x / nx * 2 - 1, sy = (float)(y + k) / ny * 2 - 1;
          V3 d = unit(add(forward, add(mul(right, sx * tangent * aspect),
                                       mul(up, sy * tangent))));
          float u = atan2f(d.z, d.x) / (2 * LX_PI) + .5f,
                v = .5f - asinf(d.y) / LX_PI;
          glTexCoord2f(u, v);
          glVertex2f(sx, sy);
        }
      glEnd();
    }
  } else {
    static const V3 corners[6][4] = {
        {{1, -1, -1}, {1, -1, 1}, {1, 1, 1}, {1, 1, -1}},
        {{-1, -1, 1}, {-1, -1, -1}, {-1, 1, -1}, {-1, 1, 1}},
        {{-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1}},
        {{-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}},
        {{1, -1, 1}, {-1, -1, 1}, {-1, 1, 1}, {1, 1, 1}},
        {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}}};
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float n = .01f, h = n * tangent;
    glFrustum(-h * aspect, h * aspect, h, -h, n, 10);
    M4 view = camera_view(a);
    view.m[12] = view.m[13] = view.m[14] = 0;
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.m);
    for (int face = 0; face < 6; face++) {
      glBindTexture(GL_TEXTURE_2D, renderer_texture(r, a->scene.sky[face]));
      glBegin(GL_QUADS);
      for (int k = 0; k < 4; k++) {
        glTexCoord2f(k == 1 || k == 2 ? 1 : 0, k >= 2 ? 0 : 1);
        V3 p = corners[face][k];
        glVertex3f(p.x, p.y, p.z);
      }
      glEnd();
    }
  }
  glDisable(GL_TEXTURE_2D);
}
static void line(V3 a, V3 b) {
  glVertex3f(a.x, a.y, a.z);
  glVertex3f(b.x, b.y, b.z);
}
void renderer_draw(App *a) {
  Renderer *r = &a->renderer;
  int w = (int)a->viewport.w, h = (int)a->viewport.h;
  if (w < 1 || h < 1)
    return;
  if (w != r->w || h != r->h) {
    r->w = w;
    r->h = h;
    glBindTexture(GL_TEXTURE_2D, r->color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    pBindRenderbuffer(GL_RENDERBUFFER, r->depth);
    pRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    pBindFramebuffer(GL_FRAMEBUFFER, r->fbo);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                          r->color, 0);
    pFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                             GL_RENDERBUFFER, r->depth);
    if (pCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      snprintf(r->error, sizeof(r->error),
               "Viewport framebuffer is incomplete");
  } else
    pBindFramebuffer(GL_FRAMEBUFFER, r->fbo);
  glViewport(0, 0, w, h);
  glClearColor(.04f, .06f, .09f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  sky(a);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float near_plane = a->settings.near_clip, far_plane = a->settings.far_clip,
        t = tanf(a->settings.fov * LX_PI / 360) * near_plane,
        aspect = (float)w / h;
  glFrustum(-t * aspect, t * aspect, t, -t, near_plane, far_plane);
  M4 view = camera_view(a);
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(view.m);
  glFrontFace(GL_CW);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (size_t i = 0; i < a->scene.count; i++) {
    Object *o = &a->scene.objects[i];
    if (!object_visible(&a->scene, o))
      continue;
    M4 world = object_world(&a->scene, o);
    if (!o->mesh.ni) {
      V3 p = point(world, v3(0, 0, 0));
      glColor3f(o->selected ? 1 : .4f, .7f, .9f);
      glPointSize(10);
      glBegin(GL_POINTS);
      glVertex3f(p.x, p.y, p.z);
      glEnd();
      continue;
    }
    GLuint program_id = o->material.shader[0]
                            ? custom_shader(r, o->material.shader)
                            : r->program;
    pUseProgram(program_id);
    V3 sun = direction(
        view, unit(v3(a->scene.sun[0], a->scene.sun[1], a->scene.sun[2])));
    pUniform3f(pGetUniformLocation(program_id, "sun"), sun.x, sun.y, sun.z);
    uniform1(program_id, "metallic", o->material.metallic);
    uniform1(program_id, "roughness", o->material.roughness);
    uniform1(program_id, "emission", o->material.emission);
    GLuint tex = renderer_texture(r, o->material.texture),
           normal = renderer_texture(r, o->material.normal);
    uniform1(program_id, "has_tex", tex ? 1 : 0);
    uniform1(program_id, "has_normal", normal ? 1 : 0);
    pActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    pUniform1i(pGetUniformLocation(program_id, "tex"), 0);
    pActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal);
    pUniform1i(pGetUniformLocation(program_id, "normal_tex"), 1);
    pActiveTexture(GL_TEXTURE0);
    glPushMatrix();
    glMultMatrixf(world.m);
    M4 pose[LX_BONES];
    if (o->rig)
      rig_pose(o->rig, a->time, length3(o->velocity) > .1f, pose);
    glBegin(GL_TRIANGLES);
    for (size_t j = 0; j < o->mesh.ni; j++) {
      Vertex v = o->mesh.v[o->mesh.i[j]];
      if (o->rig)
        v = skin_vertex(v, pose, o->rig->count);
      for (int k = 0; k < 4; k++)
        v.color[k] *= o->material.color[k];
      if (o->negative) {
        v.color[0] = 1;
        v.color[1] = .2f;
        v.color[2] = .3f;
        v.color[3] = .45f;
      }
      vertex(v);
    }
    glEnd();
    pUseProgram(0);
    if (o->selected && !a->playing) {
      glDisable(GL_TEXTURE_2D);
      glColor3f(.45f, .7f, 1);
      glLineWidth(1.5f);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glEnable(GL_POLYGON_OFFSET_LINE);
      glPolygonOffset(-1, -1);
      glBegin(GL_TRIANGLES);
      for (size_t j = 0; j < o->mesh.ni; j++) {
        V3 p = o->mesh.v[o->mesh.i[j]].p;
        glVertex3f(p.x, p.y, p.z);
      }
      glEnd();
      glDisable(GL_POLYGON_OFFSET_LINE);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    glPopMatrix();
  }
  if (a->settings.grid) {
    float cell = a->settings.grid_size;
    if (a->settings.adaptive_grid) {
      float scale = powf(10, floorf(log10f(fmaxf(a->distance * .02f, .01f))));
      cell = fmaxf(cell, scale);
    }
    float extent =
        fminf(a->settings.far_clip * .8f, fmaxf(1000, a->distance * 300));
    pUseProgram(r->grid_program);
    uniform1(r->grid_program, "cell", cell);
    uniform1(r->grid_program, "axis", a->settings.axes ? 1 : 0);
    uniform1(r->grid_program, "extent", extent);
    pUniform3f(pGetUniformLocation(r->grid_program, "eye"), a->camera.x,
               a->camera.y, a->camera.z);
    glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);
    glVertex3f(a->target.x - extent, 0, a->target.z - extent);
    glVertex3f(a->target.x + extent, 0, a->target.z - extent);
    glVertex3f(a->target.x + extent, 0, a->target.z + extent);
    glVertex3f(a->target.x - extent, 0, a->target.z + extent);
    glEnd();
    glDepthMask(GL_TRUE);
    pUseProgram(0);
  }
  /* Constraint and attachment visualisation. */ glDisable(GL_DEPTH_TEST);
  glLineWidth(1.5f);
  glBegin(GL_LINES);
  glColor3f(.9f, .65f, .2f);
  for (size_t i = 0; i < a->scene.constraint_count; i++) {
    const Constraint *c = &a->scene.constraints[i];
    const Object *oa = scene_get(&a->scene, c->a),
                 *ob = scene_get(&a->scene, c->b);
    if (oa && ob)
      line(point(object_world(&a->scene, oa), v3(0, 0, 0)),
           point(object_world(&a->scene, ob), v3(0, 0, 0)));
  }
  glEnd();
  Object *selected = scene_find(&a->scene, a->selected_id);
  if (selected && !a->playing) {
    M4 world = object_world(&a->scene, selected);
    V3 center = point(world, selected->pivot);
    float size = fmaxf(.1f, length3(sub(center, a->camera)) * .12f);
    if (a->tool > 0 && a->tool < 6) {
      glLineWidth(4);
      for (int k = 0; k < 3; k++) {
        V3 axis = k == 0 ? v3(1, 0, 0) : k == 1 ? v3(0, 1, 0) : v3(0, 0, 1);
        if (a->settings.local_space)
          axis = unit(direction(world, axis));
        glColor3f(k == 0 ? 1 : .25f, k == 1 ? 1 : .25f, k == 2 ? 1 : .25f);
        glBegin(GL_LINES);
        if (a->tool == 3) {
          V3 u = unit(cross(axis, k == 1 ? v3(1, 0, 0) : v3(0, 1, 0))),
             v = cross(axis, u);
          for (int j = 0; j < 64; j++) {
            float t0 = j * 2 * LX_PI / 64, t1 = (j + 1) * 2 * LX_PI / 64;
            line(
                add(center, mul(add(mul(u, cosf(t0)), mul(v, sinf(t0))), size)),
                add(center,
                    mul(add(mul(u, cosf(t1)), mul(v, sinf(t1))), size)));
          }
        } else
          line(center, add(center, mul(axis, size)));
        glEnd();
      }
    }
    if (selected->rig) {
      M4 pose[LX_BONES];
      rig_pose(selected->rig, a->time, false, pose);
      glLineWidth(3);
      glColor3f(1, .8f, .35f);
      glBegin(GL_LINES);
      for (int k = 0; k < selected->rig->count; k++) {
        Bone *b = &selected->rig->bones[k];
        if (b->parent >= 0) {
          V3 p = point(mmul(pose[k], inverse(b->inverse_bind)), v3(0, 0, 0)),
             q = point(
                 mmul(pose[b->parent],
                      inverse(selected->rig->bones[b->parent].inverse_bind)),
                 v3(0, 0, 0));
          line(point(world, p), point(world, q));
        }
      }
      glEnd();
    }
  }
  /* Lightweight deterministic particle emitters, animated in runtime. */
  for (size_t i = 0; i < a->scene.count; i++) {
    const Object *o = &a->scene.objects[i];
    if (o->kind != LX_EFFECT || !object_visible(&a->scene, o))
      continue;
    V3 p = point(object_world(&a->scene, o), v3(0, 0, 0));
    glPointSize(o->effect == 1 ? 6 : 3);
    glBegin(GL_POINTS);
    for (int k = 0; k < 48; k++) {
      float t = fmodf(a->time * .6f + k / 48.0f, 1), angle = k * 2.39996f;
      glColor4f(o->material.color[0], o->material.color[1],
                o->material.color[2], 1 - t);
      glVertex3f(p.x + cosf(angle) * t * .6f, p.y + t * 2,
                 p.z + sinf(angle) * t * .6f);
    }
    glEnd();
  }
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  pBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, a->width, a->height);
}
