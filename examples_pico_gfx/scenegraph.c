#define _POSIX_C_SOURCE 199309L

#include <SDL2/SDL.h>

#define PICO_TIME_IMPLEMENTATION
#include "../pico_time.h"

#define PICO_MATH_IMPLEMENTATION
#include "../pico_math.h"

#define SOKOL_GLCORE33
#define SOKOL_GFX_IMPL
#define PICO_GFX_IMPLEMENTATION
#include "../pico_gfx.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHILDREN 5
#define FIXED_STEP (1.0 / 50.0)

pg_ctx_t* ctx = NULL;
pg_vs_t pg_vs;

typedef struct
{
    int w, h;
    pg_texture_t* tex;
    pg_vbuffer_t* buf;
} sprite_t;

typedef struct node_s
{
    struct node_s* parent;
    struct node_s* children[MAX_CHILDREN];
    int child_count;
    pm_t2 local;
    pm_t2 world;
    pm_t2 last;
    sprite_t* sprite;
} node_t;

sprite_t* sprite_new(int w, int h, float d, pg_texture_t* tex)
{
    sprite_t* sprite = malloc(sizeof(sprite_t));
    sprite->w = w;
    sprite->h = h;
    sprite->tex = tex;

    pg_vertex_t vertices[6] =
    {
        { { 0, 0, d }, { 1, 1, 1, 1 }, { 0, 1 } },
        { { 0, h, d }, { 1, 1, 1, 1 }, { 0, 0 } },
        { { w, 0, d }, { 1, 1, 1, 1 }, { 1, 1 } },

        { { 0, h, d }, { 1, 1, 1, 1 }, { 0, 0 } },
        { { w, h, d }, { 1, 1, 1, 1 }, { 1, 0 } },
        { { w, 0, d }, { 1, 1, 1, 1 }, { 1, 1 } }
    };

    sprite->buf = pg_create_vbuffer(vertices, 6);

    return sprite;
}

void sprite_free(sprite_t* sprite)
{
    pg_destroy_vbuffer(sprite->buf);
    pg_destroy_texture(sprite->tex);
    free(sprite);
}

node_t* node_new(sprite_t* sprite)
{
    node_t* node = malloc(sizeof(node_t));
    memset(node, 0, sizeof(node_t));
    node->sprite = sprite;
    node->parent = NULL;
    node->local = pm_t2_identity();
    node->world = pm_t2_identity();
    node->last = pm_t2_identity();
    return node;
}

void node_free(node_t* node)
{
    for (int i = 0; i < node->child_count; i++)
    {
        node_free(node->children[i]);
    }

    free(node);
}

void node_add_child(node_t* parent, node_t* node)
{
    assert(parent->child_count < MAX_CHILDREN);
    node->parent = parent;
    parent->children[parent->child_count] = node;
    parent->child_count++;
}

// Recursive updates world transforms from local ones. This function is at the
// core of a scene graph.
void node_update_transform(node_t* node)
{
    if (node->parent)
    {
        node_update_transform(node->parent);
        node->world = pm_t2_mult(&node->parent->world, &node->local);
    }
    else
    {
        node->world = node->local;
    }
}

pm_t2 node_get_world(node_t* node)
{
    node_update_transform(node);
    return node->world;
}

void node_update_last(node_t* node)
{
    node->last = node_get_world(node);

    for (int i = 0; i < node->child_count; i++)
    {
        node_update_last(node->children[i]);
    }
}

pg_texture_t* load_texture(const char* file, int* w, int* h)
{

    int c;
    unsigned char* bitmap = stbi_load(file, w, h, &c, 0);
    size_t size = (*w) * (*h) * c;

    assert(bitmap);

    pg_texture_t* tex = pg_create_texture(*w, *h, bitmap, size, 0, false, false);

    assert(tex);
    free(bitmap);

    return tex;
}

static struct
{
    SDL_Window* window;
    SDL_GLContext context;
    int screen_w;
    int screen_h;
} app;

void app_startup()
{
    printf("Scene graph rendering demo\n");

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);

    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
    SDL_GL_CONTEXT_PROFILE_CORE);

    app.window = SDL_CreateWindow("Scene Graph Example",
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  1024, 768,
                                  SDL_WINDOW_OPENGL);

    SDL_GL_GetDrawableSize(app.window, &app.screen_w, &app.screen_h);

    SDL_GL_SetSwapInterval(1);
    app.context = SDL_GL_CreateContext(app.window);

    stbi_set_flip_vertically_on_load(true);
}

void app_shutdown()
{
    SDL_GL_DeleteContext(app.context);
    SDL_DestroyWindow(app.window);

    SDL_Quit();
}

typedef struct
{
    int w;
    int h;
    node_t* root_node;
    node_t* pivot_node;
    sprite_t* bg_sprite;
    sprite_t* star_sprite;
    sprite_t* ship_sprite;
    sprite_t* jet_sprite;
} scenegraph_t;

// Build the scene graph
scenegraph_t* sg_build(int scene_w, int scene_h)
{
    scenegraph_t* sg = malloc(sizeof(scenegraph_t));
    sg->w = scene_w;
    sg->h = scene_h;

    int w, h;

    //////////// Root Node ////////////

    // Does not have a sprite or a parent
    sg->root_node = node_new(NULL);

    //////////// BG Node ////////////

    // Load texture
    pg_texture_t* bg_tex = load_texture("./space.png", &w, &h);

    // New sprite
    sg->bg_sprite = sprite_new(scene_w, scene_h, 10.0f, bg_tex);

    // Create a new node that uses this sprite. In theory more than one node
    // could have the same sprite.
    node_t* bg_node = node_new(sg->bg_sprite);

    // And the node as a child of the root node
    node_add_child(sg->root_node, bg_node);

    // The rest of the nodes are similar

    //////////// Star Node ////////////

    pg_texture_t* star_tex = load_texture("./star.png", &w, &h);

    sg->star_sprite = sprite_new(w / 3, h / 3, 0.0f, star_tex);
    node_t* star_node = node_new(sg->star_sprite);

    pm_v2 screen_center = pm_v2_make(scene_w / 2, scene_h / 2);

    pm_v2 star_center = pm_v2_make(w / 6, h / 6);
    pm_t2_translate(&star_node->local, pm_v2_scale(star_center, -1.0f));
    pm_t2_translate(&star_node->local, screen_center);

    node_add_child(sg->root_node, star_node);

    //////////// Pivot Node ////////////

    node_t* pivot_node = node_new(NULL);
    sg->pivot_node = pivot_node;

    pm_t2_translate(&pivot_node->local, screen_center);
    node_add_child(sg->root_node, pivot_node);

    //////////// Ship Node ////////////

    pg_texture_t* ship_tex = load_texture("./ship.png", &w, &h);

    int ship_w = w;

    sg->ship_sprite = sprite_new(w, h, 5.0f, ship_tex);
    node_t* ship_node = node_new(sg->ship_sprite);

    pm_t2_translate(&ship_node->local, pm_v2_make(-w / 2, -h / 2));
    pm_t2_translate(&ship_node->local, pm_v2_make(200, 0));

    node_add_child(pivot_node, ship_node);

    //////////// Jet Node ////////////

    pg_texture_t* jet_tex = load_texture("./jet.png", &w, &h);

    sg->jet_sprite = sprite_new(w, h, 0.0f, jet_tex);
    node_t* jet_node = node_new(sg->jet_sprite);

    pm_t2_translate(&jet_node->local, pm_v2_make(0, 32));
    pm_t2_translate(&jet_node->local, pm_v2_make(ship_w / 2 - w / 2, 0));

    node_add_child(ship_node, jet_node);

    return sg;
}

void sg_free(scenegraph_t* sg)
{
    sprite_free(sg->bg_sprite);
    sprite_free(sg->star_sprite);
    sprite_free(sg->ship_sprite);
    sprite_free(sg->jet_sprite);
    node_free(sg->root_node);
    free(sg);
}

// Hires time
double time_now()
{
    return (double)SDL_GetPerformanceCounter() /
           (double)SDL_GetPerformanceFrequency();
}

int main(int argc, char* argv[])
{


    pg_vs = (pg_vs_t)
    {
        0
    };

    //pg_register_uniform_block()

    return 0;
}