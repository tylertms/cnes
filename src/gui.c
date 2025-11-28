#include "gui.h"
#include "ppu.h"
#include <stdio.h>
#include <string.h>

static SDL_GLContext create_best_gl_context(SDL_Window *w) {
    static const struct { int maj, min; } list[] = {
        {4,6},{4,5},{4,4},{4,3},{4,2},{4,1},{4,0},{3,3},{3,2}
    };
    int n = (int)(sizeof list / sizeof list[0]);
    for (int i = 0; i < n; i++) {
        int maj = list[i].maj, min = list[i].min;
#ifdef __APPLE__
        if (maj > 4 || (maj == 4 && min > 1)) continue;
#endif
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, maj);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, min);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
#endif
        SDL_GLContext ctx = SDL_GL_CreateContext(w);
        if (ctx) return ctx;
    }
    return NULL;
}

static void detect_glsl_version(char *out, size_t sz) {
    int maj = 0, min = 0;

    glGetIntegerv(GL_MAJOR_VERSION, &maj);
    glGetIntegerv(GL_MINOR_VERSION, &min);
    if (!maj && !min) {
        const char *ver = (const char *)glGetString(GL_VERSION);
        if (!ver || sscanf(ver, "%d.%d", &maj, &min) != 2) {
            maj = 2;
            min = 1;
        }
    }

    SDL_Log("Using OpenGL Version %d.%d", maj, min);

    int glsl = 150;
    if (maj == 2) {
        glsl = (min == 0) ? 110 : 120;
    } else if (maj == 3) {
        glsl = (min == 0) ? 130 : (min == 1 ? 140 : 150);
    } else if (maj == 4) {
        if      (min == 0) glsl = 400;
        else if (min == 1) glsl = 410;
        else if (min == 2) glsl = 420;
        else if (min == 3) glsl = 430;
        else if (min == 4) glsl = 440;
        else if (min == 5) glsl = 450;
        else               glsl = 460;
    }

    snprintf(out, sz, "#version %d", glsl);
}

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            char buf[512];
            glGetShaderInfoLog(s, sizeof buf, NULL, buf);
            SDL_Log("shader compile error: %s", buf);
        }
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "pos");
    glBindAttribLocation(p, 1, "uv");
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            char buf[512];
            glGetProgramInfoLog(p, sizeof buf, NULL, buf);
            SDL_Log("program link error: %s", buf);
        }
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static int init_quad_and_shader(_gui *g) {
    char vs[512], fs[512];
    snprintf(vs, sizeof vs, "%s\n%s", g->glsl_version, gui_vs_src);
    snprintf(fs, sizeof fs, "%s\n%s", g->glsl_version, gui_fs_src);

    GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
    if (!v) return 1;
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);
    if (!f) {
        glDeleteShader(v);
        return 1;
    }

    g->program = link_program(v, f);
    glDeleteShader(v);
    glDeleteShader(f);
    if (!g->program) return 1;

    float verts[] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f
    };

    glGenVertexArrays(1, &g->vao);
    glGenBuffers(1, &g->vbo);

    glBindVertexArray(g->vao);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return 0;
}

int gui_init(_gui *g, char *file) {
    memset(g, 0, sizeof *g);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    char title[128];
    snprintf(title, sizeof title, "cnes - %s", file);
    g->window = SDL_CreateWindow(title, 256 * 3, 240 * 3,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!g->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    g->gl_ctx = create_best_gl_context(g->window);
    if (!g->gl_ctx) {
        SDL_Log("Failed to create any OpenGL context");
        return 1;
    }

    SDL_GL_MakeCurrent(g->window, g->gl_ctx);
    if (SDL_GL_SetSwapInterval(-1) != 0) SDL_GL_SetSwapInterval(1);

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        SDL_Log("Failed to initialize glad");
        return 1;
    }

    detect_glsl_version(g->glsl_version, sizeof g->glsl_version);

    if (init_quad_and_shader(g) != 0) {
        SDL_Log("Failed to init GL quad/shader");
        return 1;
    }

    glGenTextures(1, &g->nes_texture);
    glBindTexture(GL_TEXTURE_2D, g->nes_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, NES_W, NES_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    g->pixels = SDL_calloc(NES_PIXELS, sizeof(uint32_t));
    if (!g->pixels) {
        SDL_Log("Failed to allocate pixel buffer");
        return 1;
    }

    CIMGUI_CHECKVERSION();
    g->im_ctx = ImGui_CreateContext(NULL);
    ImGui_SetCurrentContext(g->im_ctx);

    ImGuiIO *io = ImGui_GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    cImGui_ImplSDL3_InitForOpenGL(g->window, g->gl_ctx);
    cImGui_ImplOpenGL3_InitEx(g->glsl_version);

    return 0;
}

void set_pixel(_gui *g, uint16_t x, uint16_t y, uint32_t c) {
    if (!g->pixels) return;
    if (x < NES_W && y < NES_H) g->pixels[y * NES_W + x] = c;
}

static void draw_nes_texture(_gui *g) {
    glBindTexture(GL_TEXTURE_2D, g->nes_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, NES_W, NES_H, GL_RGBA, GL_UNSIGNED_BYTE, g->pixels);

    int w, h;
    SDL_GetWindowSize(g->window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g->program);
    GLint loc = glGetUniformLocation(g->program, "u_tex");
    glUniform1i(loc, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g->nes_texture);

    glBindVertexArray(g->vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

uint64_t gui_draw(_gui *g) {
    if (!g || !g->window || !g->gl_ctx) return 0;

    SDL_GL_MakeCurrent(g->window, g->gl_ctx);

    cImGui_ImplSDL3_NewFrame();
    cImGui_ImplOpenGL3_NewFrame();
    ImGui_NewFrame();

    if (ImGui_BeginMainMenuBar()) {
        if (ImGui_BeginMenu("File")) {
            if (ImGui_MenuItem("Settings")) g->show_settings = true;
            if (ImGui_MenuItem("Quit")) g->request_quit = true;
            ImGui_EndMenu();
        }
        if (ImGui_BeginMenu("View")) {
            ImGui_EndMenu();
        }
        ImGui_EndMainMenuBar();
    }

    if (g->show_settings) {
        if (ImGui_Begin("Settings", &g->show_settings, 0)) {
            ImGui_Text("Settings");
        }
        ImGui_End();
    }

    ImGui_Render();

    draw_nes_texture(g);
    cImGui_ImplOpenGL3_RenderDrawData(ImGui_GetDrawData());

    ImGuiIO *io = ImGui_GetIO();
    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui_UpdatePlatformWindows();
        ImGui_RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(g->window, g->gl_ctx);
    }

    uint64_t frame_end = SDL_GetPerformanceCounter();

    SDL_GL_SwapWindow(g->window);
    return frame_end;
}

void gui_deinit(_gui *g) {
    if (!g) return;

    if (g->im_ctx) {
        ImGui_SetCurrentContext(g->im_ctx);
        cImGui_ImplOpenGL3_Shutdown();
        cImGui_ImplSDL3_Shutdown();
        ImGui_DestroyContext(g->im_ctx);
        g->im_ctx = NULL;
    }

    if (g->pixels) {
        SDL_free(g->pixels);
        g->pixels = NULL;
    }

    if (g->nes_texture) {
        glDeleteTextures(1, &g->nes_texture);
        g->nes_texture = 0;
    }

    if (g->vbo) {
        glDeleteBuffers(1, &g->vbo);
        g->vbo = 0;
    }

    if (g->vao) {
        glDeleteVertexArrays(1, &g->vao);
        g->vao = 0;
    }

    if (g->program) {
        glDeleteProgram(g->program);
        g->program = 0;
    }

    if (g->gl_ctx) {
        SDL_GL_DestroyContext(g->gl_ctx);
        g->gl_ctx = NULL;
    }

    if (g->window) {
        SDL_DestroyWindow(g->window);
        g->window = NULL;
    }

    SDL_Quit();
}
