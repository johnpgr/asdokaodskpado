// Include system GL first, then our loader
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <gl/gl.h>
#include <windows.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include "renderer.h"
#include "util/loader.opengl.h"
#include <print>

using std::println;

#define MAX_QUADS 10000
#define MAX_VERTICES (MAX_QUADS * 4)
#define MAX_INDICES (MAX_QUADS * 6)
#define MAX_TEXTURES 256

// Base resolution (safe zone - always visible)
#define BASE_WIDTH 320
#define BASE_HEIGHT 180

// Max buffer size for overscan (supports up to 1920x1080 at 5x scale)
// This accommodates various aspect ratios without black bars
#define MAX_TARGET_WIDTH 384
#define MAX_TARGET_HEIGHT 216

struct Vertex {
    f32 pos[2];
    f32 uv[2];
    f32 color[4];
};

struct Renderer {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLuint shader_program;
    GLint u_resolution_loc;
    GLint u_texture_loc;

    GLuint blit_vao;
    GLuint blit_vbo;
    GLuint blit_shader_program;
    GLint blit_u_texture_loc;

    GLuint offscreen_fbo;
    GLuint offscreen_texture;

    GLuint textures[MAX_TEXTURES];
    u32 texture_count;

    u32 vertex_count;
    u32 current_texture;

    f32 clear_color[4];
    u32 width;         // Window width (pixels)
    u32 height;        // Window height (pixels)
    u32 target_width;  // Current render target width (game pixels)
    u32 target_height; // Current render target height (game pixels)
};

// Static allocations
static Vertex global_vertex_buffer[MAX_VERTICES];
static Vertex global_blit_quad[4];
static Renderer global_renderer = {};

// GLSL Shaders - embedded from external files using C++23 #embed
static const char vs_source[] = {
#embed "shaders/sprite.vert.glsl"
};

static const char fs_source[] = {
#embed "shaders/sprite.frag.glsl"
};

static const char blit_vs_source[] = {
#embed "shaders/blit.vert.glsl"
};

static const char blit_fs_source[] = {
#embed "shaders/blit.frag.glsl"
};

static GLuint compile_shader(GLenum type, const char* source, GLint length) {
    GLuint shader = gl_CreateShader(type);
    gl_ShaderSource(shader, 1, &source, &length);
    gl_CompileShader(shader);

    GLint success;
    gl_GetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        gl_GetShaderInfoLog(shader, 512, nullptr, log);
        println("Shader compilation failed: {}", log);
        return 0;
    }
    return shader;
}

static GLuint create_shader_program(
    const char* vs,
    GLint vs_len,
    const char* fs,
    GLint fs_len
) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vs, vs_len);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fs, fs_len);

    if (!vertex_shader || !fragment_shader) {
        return 0;
    }

    GLuint program = gl_CreateProgram();
    gl_AttachShader(program, vertex_shader);
    gl_AttachShader(program, fragment_shader);
    gl_LinkProgram(program);

    GLint success;
    gl_GetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        gl_GetProgramInfoLog(program, 512, nullptr, log);
        println("Shader linking failed: {}", log);
        return 0;
    }

    gl_DeleteShader(vertex_shader);
    gl_DeleteShader(fragment_shader);

    return program;
}

Renderer* renderer_init(void) {
    Renderer* r = &global_renderer;

    r->vertex_count = 0;
    r->current_texture = 0;
    r->texture_count = 0;
    r->clear_color[0] = 0.0f;
    r->clear_color[1] = 0.0f;
    r->clear_color[2] = 0.0f;
    r->clear_color[3] = 1.0f;

    // Create main shader program
    r->shader_program = create_shader_program(
        vs_source,
        sizeof(vs_source),
        fs_source,
        sizeof(fs_source)
    );
    r->u_resolution_loc =
        gl_GetUniformLocation(r->shader_program, "u_resolution");
    r->u_texture_loc = gl_GetUniformLocation(r->shader_program, "u_texture");

    // Create blit shader program
    r->blit_shader_program = create_shader_program(
        blit_vs_source,
        sizeof(blit_vs_source),
        blit_fs_source,
        sizeof(blit_fs_source)
    );
    r->blit_u_texture_loc =
        gl_GetUniformLocation(r->blit_shader_program, "u_texture");

    // Create VAO and buffers for sprite rendering
    gl_GenVertexArrays(1, &r->vao);
    gl_BindVertexArray(r->vao);

    gl_GenBuffers(1, &r->vbo);
    gl_BindBuffer(GL_ARRAY_BUFFER, r->vbo);
    gl_BufferData(
        GL_ARRAY_BUFFER,
        MAX_VERTICES * sizeof(Vertex),
        nullptr,
        GL_DYNAMIC_DRAW
    );

    // Build index buffer
    u16 indices[MAX_INDICES];
    for (u32 i = 0; i < MAX_QUADS; i++) {
        indices[i * 6 + 0] = i * 4 + 0;
        indices[i * 6 + 1] = i * 4 + 1;
        indices[i * 6 + 2] = i * 4 + 2;
        indices[i * 6 + 3] = i * 4 + 0;
        indices[i * 6 + 4] = i * 4 + 2;
        indices[i * 6 + 5] = i * 4 + 3;
    }

    gl_GenBuffers(1, &r->ebo);
    gl_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ebo);
    gl_BufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(indices),
        indices,
        GL_STATIC_DRAW
    );

    // Vertex attributes: pos, uv, color
    gl_VertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, pos)
    );
    gl_EnableVertexAttribArray(0);
    gl_VertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, uv)
    );
    gl_EnableVertexAttribArray(1);
    gl_VertexAttribPointer(
        2,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, color)
    );
    gl_EnableVertexAttribArray(2);

    gl_BindVertexArray(0);

    // Create VAO and buffers for blit quad
    gl_GenVertexArrays(1, &r->blit_vao);
    gl_BindVertexArray(r->blit_vao);

    gl_GenBuffers(1, &r->blit_vbo);
    gl_BindBuffer(GL_ARRAY_BUFFER, r->blit_vbo);
    gl_BufferData(
        GL_ARRAY_BUFFER,
        4 * sizeof(Vertex),
        nullptr,
        GL_DYNAMIC_DRAW
    );

    // Reuse the same index buffer for blit (just first 6 indices)
    gl_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ebo);

    gl_VertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, pos)
    );
    gl_EnableVertexAttribArray(0);
    gl_VertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, uv)
    );
    gl_EnableVertexAttribArray(1);
    gl_VertexAttribPointer(
        2,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, color)
    );
    gl_EnableVertexAttribArray(2);

    gl_BindVertexArray(0);

    // Create white 1x1 texture for solid color rectangles
    u32 white_pixel = 0xFFFFFFFF;
    glGenTextures(1, &r->textures[0]);
    glBindTexture(GL_TEXTURE_2D, r->textures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        1,
        1,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        &white_pixel
    );
    r->texture_count = 1;

    // Create offscreen render target at max size for overscan
    glGenTextures(1, &r->offscreen_texture);
    glBindTexture(GL_TEXTURE_2D, r->offscreen_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        MAX_TARGET_WIDTH,
        MAX_TARGET_HEIGHT,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    gl_GenFramebuffers(1, &r->offscreen_fbo);
    gl_BindFramebuffer(GL_FRAMEBUFFER, r->offscreen_fbo);
    gl_FramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        r->offscreen_texture,
        0
    );

    GLenum status = gl_CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        println("Framebuffer incomplete: 0x{:x}", status);
    }

    gl_BindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return r;
}

void renderer_begin_frame(
    Renderer* renderer,
    u32 width,
    u32 height,
    u32 target_width,
    u32 target_height
) {
    renderer->width = width;
    renderer->height = height;
    renderer->target_width = target_width;
    renderer->target_height = target_height;
    renderer->vertex_count = 0;
    renderer->current_texture = 0;

    // Render to offscreen target (using only the portion we need)
    gl_BindFramebuffer(GL_FRAMEBUFFER, renderer->offscreen_fbo);
    glViewport(0, 0, target_width, target_height);
    glClearColor(
        renderer->clear_color[0],
        renderer->clear_color[1],
        renderer->clear_color[2],
        renderer->clear_color[3]
    );
    glClear(GL_COLOR_BUFFER_BIT);

    gl_UseProgram(renderer->shader_program);
    gl_Uniform2f(
        renderer->u_resolution_loc,
        (f32)target_width,
        (f32)target_height
    );
    gl_Uniform1i(renderer->u_texture_loc, 0);

    gl_ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->textures[0]);

    gl_BindVertexArray(renderer->vao);
}

static void renderer_flush(Renderer* r) {
    if (r->vertex_count == 0) {
        return;
    }

    gl_BindBuffer(GL_ARRAY_BUFFER, r->vbo);
    gl_BufferSubData(
        GL_ARRAY_BUFFER,
        0,
        r->vertex_count * sizeof(Vertex),
        global_vertex_buffer
    );

    u32 index_count = (r->vertex_count / 4) * 6;
    glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, 0);

    r->vertex_count = 0;
}

void renderer_end_frame(Renderer* renderer) {
    renderer_flush(renderer);

    // Calculate UV coordinates for the portion of the FBO we actually used
    f32 u_max = (f32)renderer->target_width / (f32)MAX_TARGET_WIDTH;
    f32 v_max = (f32)renderer->target_height / (f32)MAX_TARGET_HEIGHT;

    // Build blit quad vertices in NDC (full screen, no black bars with
    // overscan)
    global_blit_quad[0] =
        {{-1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
    global_blit_quad[1] =
        {{1.0f, -1.0f}, {u_max, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
    global_blit_quad[2] =
        {{1.0f, 1.0f}, {u_max, v_max}, {1.0f, 1.0f, 1.0f, 1.0f}};
    global_blit_quad[3] =
        {{-1.0f, 1.0f}, {0.0f, v_max}, {1.0f, 1.0f, 1.0f, 1.0f}};

    // Blit to default framebuffer
    gl_BindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, renderer->width, renderer->height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    gl_UseProgram(renderer->blit_shader_program);
    gl_Uniform1i(renderer->blit_u_texture_loc, 0);

    gl_ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->offscreen_texture);

    gl_BindVertexArray(renderer->blit_vao);
    gl_BindBuffer(GL_ARRAY_BUFFER, renderer->blit_vbo);
    gl_BufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(global_blit_quad),
        global_blit_quad
    );

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    gl_BindVertexArray(0);
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
        glBindTexture(GL_TEXTURE_2D, renderer->textures[0]);
    }

    u32 idx = renderer->vertex_count;
    f32 r = color_r(color);
    f32 g = color_g(color);
    f32 b = color_b(color);
    f32 a = color_a(color);

    global_vertex_buffer[idx + 0] = {{x, y}, {0.0f, 0.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 1] = {{x + w, y}, {1.0f, 0.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 2] =
        {{x + w, y + h}, {1.0f, 1.0f}, {r, g, b, a}};
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
        glBindTexture(GL_TEXTURE_2D, renderer->textures[texture_id]);
    }

    u32 idx = renderer->vertex_count;
    f32 r = color_r(tint);
    f32 g = color_g(tint);
    f32 b = color_b(tint);
    f32 a = color_a(tint);

    global_vertex_buffer[idx + 0] = {{x, y}, {0.0f, 0.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 1] = {{x + w, y}, {1.0f, 0.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 2] =
        {{x + w, y + h}, {1.0f, 1.0f}, {r, g, b, a}};
    global_vertex_buffer[idx + 3] = {{x, y + h}, {0.0f, 1.0f}, {r, g, b, a}};

    renderer->vertex_count += 4;
}

void renderer_draw_atlas_sprite(
    Renderer* renderer,
    f32 x,
    f32 y,
    f32 w,
    f32 h,
    f32 u0,
    f32 v0,
    f32 u1,
    f32 v1,
    u32 texture_id,
    Color tint
) {
    if (renderer->vertex_count + 4 > MAX_VERTICES) {
        renderer_flush(renderer);
    }

    if (renderer->current_texture != texture_id) {
        renderer_flush(renderer);
        renderer->current_texture = texture_id;
        glBindTexture(GL_TEXTURE_2D, renderer->textures[texture_id]);
    }

    u32 idx = renderer->vertex_count;
    f32 r = color_r(tint);
    f32 g = color_g(tint);
    f32 b = color_b(tint);
    f32 a = color_a(tint);

    global_vertex_buffer[idx + 0] = {{x, y}, {u0, v0}, {r, g, b, a}};
    global_vertex_buffer[idx + 1] = {{x + w, y}, {u1, v0}, {r, g, b, a}};
    global_vertex_buffer[idx + 2] = {{x + w, y + h}, {u1, v1}, {r, g, b, a}};
    global_vertex_buffer[idx + 3] = {{x, y + h}, {u0, v1}, {r, g, b, a}};

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

    GLenum internal_format = (channels == 4) ? GL_RGBA8 : GL_R8;
    GLenum format = (channels == 4) ? GL_RGBA : GL_RED;

    u32 texture_id = renderer->texture_count;

    glGenTextures(1, &renderer->textures[texture_id]);
    glBindTexture(GL_TEXTURE_2D, renderer->textures[texture_id]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internal_format,
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        pixels
    );

    renderer->texture_count++;
    return texture_id;
}

void renderer_set_clear_color(Renderer* renderer, Color color) {
    renderer->clear_color[0] = color_r(color);
    renderer->clear_color[1] = color_g(color);
    renderer->clear_color[2] = color_b(color);
    renderer->clear_color[3] = color_a(color);
}
