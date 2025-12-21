#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"

#include "renderer.h"

#define MAX_QUADS 10000
#define MAX_VERTICES (MAX_QUADS * 4)
#define MAX_INDICES (MAX_QUADS * 6)
#define MAX_TEXTURES 256

#define TARGET_WIDTH 320
#define TARGET_HEIGHT 180

struct Vertex {
    f32 pos[2];
    f32 uv[2];
    f32 color[4];
};

struct Renderer {
    sg_pipeline pipeline;
    sg_bindings bindings;
    sg_pass_action pass_action;

    u32 vertex_count;
    u32 current_texture;

    sg_image textures[MAX_TEXTURES];
    sg_view texture_views[MAX_TEXTURES];
    u32 texture_count;

    sg_sampler sampler;

    // Offscreen render target
    sg_image offscreen_target;
    sg_view offscreen_color_view;
    sg_view offscreen_texture_view;
    sg_pass_action offscreen_pass_action;

    // Blit pass resources
    sg_pipeline blit_pipeline;
    sg_bindings blit_bindings;
    sg_buffer blit_quad_buffer;
    sg_pass_action blit_pass_action;

    u32 width;
    u32 height;
};

// Static allocations - no runtime memory allocation needed
static Vertex global_vertex_buffer[MAX_VERTICES];
static Vertex global_blit_quad[4];
static Renderer global_renderer = {};

static const char* vertex_shader_source = R"(
#include <metal_stdlib>
using namespace metal;

struct vs_in {
    float2 pos [[attribute(0)]];
    float2 uv [[attribute(1)]];
    float4 color [[attribute(2)]];
};

struct vs_out {
    float4 pos [[position]];
    float2 uv;
    float4 color;
};

struct Uniforms {
    float2 resolution;
};

vertex vs_out vs_main(vs_in in [[stage_in]], constant Uniforms& uniforms [[buffer(0)]]) {
    vs_out out;
    float2 normalized = (in.pos / uniforms.resolution) * 2.0 - 1.0;
    normalized.y = -normalized.y;
    out.pos = float4(normalized, 0.0, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
}

fragment float4 fs_main(vs_out in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {
    return tex.sample(smp, in.uv) * in.color;
}
)";

// Blit shader - vertices are already in NDC space (-1 to 1)
static const char* blit_shader_source = R"(
#include <metal_stdlib>
using namespace metal;

struct vs_in {
    float2 pos [[attribute(0)]];
    float2 uv [[attribute(1)]];
    float4 color [[attribute(2)]];
};

struct vs_out {
    float4 pos [[position]];
    float2 uv;
    float4 color;
};

vertex vs_out vs_blit(vs_in in [[stage_in]]) {
    vs_out out;
    out.pos = float4(in.pos, 0.0, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
}

fragment float4 fs_blit(vs_out in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {
    return tex.sample(smp, in.uv) * in.color;
}
)";

Renderer* renderer_init() {
    Renderer* r = &global_renderer;

    r->vertex_count = 0;
    r->current_texture = 0;
    r->texture_count = 0;

    sg_buffer vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
        .size = MAX_VERTICES * sizeof(Vertex),
        .usage = {.vertex_buffer = true, .stream_update = true},
    });

    // Build index buffer on stack - only needed during init, uploaded to GPU
    u16 indices[MAX_INDICES];
    for (u32 i = 0; i < MAX_QUADS; i++) {
        indices[i * 6 + 0] = i * 4 + 0; // Triangle 1: vertex 0
        indices[i * 6 + 1] = i * 4 + 1; //             vertex 1
        indices[i * 6 + 2] = i * 4 + 2; //             vertex 2
        indices[i * 6 + 3] = i * 4 + 0; // Triangle 2: vertex 0
        indices[i * 6 + 4] = i * 4 + 2; //             vertex 2
        indices[i * 6 + 5] = i * 4 + 3; //             vertex 3
    }

    sg_buffer index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {.index_buffer = true},
        .data = {.ptr = indices, .size = MAX_INDICES * sizeof(u16)},
    });

    // Create white 1x1 texture for solid color rectangles
    u32 white_pixel = 0xFFFFFFFF;
    r->textures[0] = sg_make_image(&(sg_image_desc){
        .width = 1,
        .height = 1,
        .data.mip_levels[0] = {.ptr = &white_pixel, .size = sizeof(u32)},
    });
    r->texture_views[0] = sg_make_view(&(sg_view_desc){
        .texture.image = r->textures[0],
    });
    r->texture_count = 1;

    // Create shader with modern API
    sg_shader shader = sg_make_shader(&(sg_shader_desc){
        .vertex_func =
            {
                .source = vertex_shader_source,
                .entry = "vs_main",
            },
        .fragment_func =
            {
                .source = vertex_shader_source,
                .entry = "fs_main",
            },
        .uniform_blocks[0] =
            {
                .stage = SG_SHADERSTAGE_VERTEX,
                .size = 8,
                .msl_buffer_n = 0,
            },
        .views[0] =
            {
                .texture =
                    {
                        .stage = SG_SHADERSTAGE_FRAGMENT,
                        .image_type = SG_IMAGETYPE_2D,
                        .sample_type = SG_IMAGESAMPLETYPE_FLOAT,
                        .msl_texture_n = 0,
                    },
            },
        .samplers[0] =
            {
                .stage = SG_SHADERSTAGE_FRAGMENT,
                .sampler_type = SG_SAMPLERTYPE_FILTERING,
                .msl_sampler_n = 0,
            },
        .texture_sampler_pairs[0] =
            {
                .stage = SG_SHADERSTAGE_FRAGMENT,
                .view_slot = 0,
                .sampler_slot = 0,
            },
    });

    r->pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shader,
        .layout =
            {
                .attrs =
                    {
                        [0] = {.format = SG_VERTEXFORMAT_FLOAT2},
                        [1] = {.format = SG_VERTEXFORMAT_FLOAT2},
                        [2] = {.format = SG_VERTEXFORMAT_FLOAT4},
                    },
            },
        .index_type = SG_INDEXTYPE_UINT16,
        .colors[0] =
            {
                .blend =
                    {
                        .enabled = true,
                        .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                        .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
                        .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    },
            },
    });

    r->bindings.vertex_buffers[0] = vertex_buffer;
    r->bindings.index_buffer = index_buffer;
    r->bindings.views[0] = r->texture_views[0];

    r->sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });
    r->bindings.samplers[0] = r->sampler;

    r->pass_action.colors[0] = (sg_color_attachment_action){
        .load_action = SG_LOADACTION_CLEAR,
        .clear_value = {0.0f, 0.0f, 0.0f, 1.0f},
    };

    // Create offscreen render target (320x180)
    r->offscreen_target = sg_make_image(&(sg_image_desc){
        .usage = {.color_attachment = true},
        .width = TARGET_WIDTH,
        .height = TARGET_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
    });

    // View for rendering TO the target
    r->offscreen_color_view = sg_make_view(&(sg_view_desc){
        .color_attachment.image = r->offscreen_target,
    });

    // View for sampling FROM the target (for blit pass)
    r->offscreen_texture_view = sg_make_view(&(sg_view_desc){
        .texture.image = r->offscreen_target,
    });

    // Pass action for offscreen rendering
    r->offscreen_pass_action.colors[0] = (sg_color_attachment_action){
        .load_action = SG_LOADACTION_CLEAR,
        .store_action = SG_STOREACTION_STORE,
        .clear_value = {0.0f, 0.0f, 0.0f, 1.0f},
    };

    // Blit pass action - clear to black for letterbox bars
    r->blit_pass_action.colors[0] = (sg_color_attachment_action){
        .load_action = SG_LOADACTION_CLEAR,
        .clear_value = {0.0f, 0.0f, 0.0f, 1.0f},
    };

    // Create blit quad vertex buffer (dynamic for aspect ratio adjustment)
    r->blit_quad_buffer = sg_make_buffer(&(sg_buffer_desc){
        .size = 4 * sizeof(Vertex),
        .usage = {.vertex_buffer = true, .stream_update = true},
    });

    // Create blit shader (no uniforms needed - vertices in NDC)
    sg_shader blit_shader = sg_make_shader(&(sg_shader_desc){
        .vertex_func = {
            .source = blit_shader_source,
            .entry = "vs_blit",
        },
        .fragment_func = {
            .source = blit_shader_source,
            .entry = "fs_blit",
        },
        .views[0] = {
            .texture = {
                .stage = SG_SHADERSTAGE_FRAGMENT,
                .image_type = SG_IMAGETYPE_2D,
                .sample_type = SG_IMAGESAMPLETYPE_FLOAT,
                .msl_texture_n = 0,
            },
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .sampler_type = SG_SAMPLERTYPE_FILTERING,
            .msl_sampler_n = 0,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .view_slot = 0,
            .sampler_slot = 0,
        },
    });

    // Create blit pipeline (no blending needed)
    r->blit_pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = blit_shader,
        .layout = {
            .attrs = {
                [0] = {.format = SG_VERTEXFORMAT_FLOAT2},
                [1] = {.format = SG_VERTEXFORMAT_FLOAT2},
                [2] = {.format = SG_VERTEXFORMAT_FLOAT4},
            },
        },
        .index_type = SG_INDEXTYPE_UINT16,
    });

    // Set up blit bindings
    r->blit_bindings.vertex_buffers[0] = r->blit_quad_buffer;
    r->blit_bindings.index_buffer = index_buffer;
    r->blit_bindings.views[0] = r->offscreen_texture_view;
    r->blit_bindings.samplers[0] = r->sampler;

    return r;
}

// Note: renderer_shutdown is no longer needed with arena allocation
// The arena will be freed as a whole when the application shuts down

void renderer_begin_frame(Renderer* renderer, u32 width, u32 height) {
    renderer->width = width;
    renderer->height = height;
    renderer->vertex_count = 0;
    renderer->current_texture = 0;
    renderer->bindings.views[0] = renderer->texture_views[0];

    // Render to offscreen target (320x180)
    sg_begin_pass(&(sg_pass){
        .action = renderer->offscreen_pass_action,
        .attachments = {
            .colors[0] = renderer->offscreen_color_view,
        },
    });
    sg_apply_pipeline(renderer->pipeline);
    sg_apply_bindings(&renderer->bindings);
}

static void renderer_flush(Renderer* r) {
    if (r->vertex_count == 0) {
        return;
    }

    sg_update_buffer(
        r->bindings.vertex_buffers[0],
        &(sg_range){.ptr = global_vertex_buffer, .size = r->vertex_count * sizeof(Vertex)
        }
    );

    sg_apply_bindings(&r->bindings);

    // Use fixed target resolution for sprite rendering
    f32 resolution[2] = {(f32)TARGET_WIDTH, (f32)TARGET_HEIGHT};
    sg_apply_uniforms(0, &(sg_range){.ptr = resolution, .size = 8});

    u32 index_count = (r->vertex_count / 4) * 6;
    sg_draw(0, index_count, 1);

    r->vertex_count = 0;
}

void renderer_end_frame(Renderer* renderer) {
    renderer_flush(renderer);
    sg_end_pass();

    // Calculate aspect-ratio-preserving quad size
    f32 target_aspect = (f32)TARGET_WIDTH / (f32)TARGET_HEIGHT;
    f32 window_aspect = (f32)renderer->width / (f32)renderer->height;

    f32 quad_w, quad_h;
    if (window_aspect > target_aspect) {
        // Window wider than target - pillarbox (bars on sides)
        quad_h = 1.0f;
        quad_w = target_aspect / window_aspect;
    } else {
        // Window taller than target - letterbox (bars top/bottom)
        quad_w = 1.0f;
        quad_h = window_aspect / target_aspect;
    }

    // Build blit quad vertices in NDC (-1 to 1)
    // Note: UV y is flipped because render target has inverted y
    global_blit_quad[0] = {{-quad_w, -quad_h}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
    global_blit_quad[1] = {{ quad_w, -quad_h}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
    global_blit_quad[2] = {{ quad_w,  quad_h}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
    global_blit_quad[3] = {{-quad_w,  quad_h}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};

    sg_update_buffer(
        renderer->blit_quad_buffer,
        &(sg_range){.ptr = global_blit_quad, .size = sizeof(global_blit_quad)}
    );

    // Blit to swapchain
    sg_begin_pass(&(sg_pass){
        .action = renderer->blit_pass_action,
        .swapchain = sglue_swapchain(),
    });
    sg_apply_pipeline(renderer->blit_pipeline);
    sg_apply_bindings(&renderer->blit_bindings);
    sg_draw(0, 6, 1);
    sg_end_pass();

    sg_commit();
}

void renderer_draw_rect(
    Renderer* renderer,
    f32 x,
    f32 y,
    f32 w,
    f32 h,
    Color color
) {
    if (renderer->vertex_count + 4 > MAX_VERTICES) {
        renderer_flush(renderer);
    }

    if (renderer->current_texture != 0) {
        renderer_flush(renderer);
        renderer->current_texture = 0;
        renderer->bindings.views[0] = renderer->texture_views[0];
    }

    u32 idx = renderer->vertex_count;
    f32 r = color_r(color);
    f32 g = color_g(color);
    f32 b = color_b(color);
    f32 a = color_a(color);

    global_vertex_buffer[idx + 0] = {{x, y}, {0.0f, 0.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 1] = {{x + w, y}, {1.0f, 0.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 2] = {{x + w, y + h}, {1.0f, 1.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 3] = {{x, y + h}, {0.0f, 1.0f}, {r, g, b, a}};

    renderer->vertex_count += 4;
}

void renderer_draw_sprite(
    Renderer* renderer,
    f32 x,
    f32 y,
    f32 w,
    f32 h,
    u32 texture_id,
    Color tint
) {
    if (renderer->vertex_count + 4 > MAX_VERTICES) {
        renderer_flush(renderer);
    }

    if (renderer->current_texture != texture_id) {
        renderer_flush(renderer);
        renderer->current_texture = texture_id;
        renderer->bindings.views[0] = renderer->texture_views[texture_id];
    }

    u32 idx = renderer->vertex_count;
    f32 r = color_r(tint);
    f32 g = color_g(tint);
    f32 b = color_b(tint);
    f32 a = color_a(tint);

    global_vertex_buffer[idx + 0] = {{x, y}, {0.0f, 0.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 1] = {{x + w, y}, {1.0f, 0.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 2] = {{x + w, y + h}, {1.0f, 1.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 3] = {{x, y + h}, {0.0f, 1.0f}, {r, g, b, a}};

    renderer->vertex_count += 4;
}

u32 renderer_load_texture(
    Renderer* renderer,
    void* pixels,
    i32 width,
    i32 height,
    i32 channels
) {
    if (renderer->texture_count >= MAX_TEXTURES) {
        return 0;
    }

    sg_pixel_format format =
        (channels == 4) ? SG_PIXELFORMAT_RGBA8 : SG_PIXELFORMAT_R8;

    u32 texture_id = renderer->texture_count;

    renderer->textures[texture_id] = sg_make_image(&(sg_image_desc){
        .width = width,
        .height = height,
        .pixel_format = format,
        .data.mip_levels[0] =
            {
                .ptr = pixels,
                .size = (usize)(width * height * channels),
            },
    });

    renderer->texture_views[texture_id] = sg_make_view(&(sg_view_desc){
        .texture.image = renderer->textures[texture_id],
    });

    renderer->texture_count++;
    return texture_id;
}

void renderer_set_clear_color(Renderer* renderer, Color color) {
    renderer->offscreen_pass_action.colors[0].clear_value = (sg_color
    ){color_r(color), color_g(color), color_b(color), color_a(color)};
}
