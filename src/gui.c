#include "gui.h"
#include "ppu.h"
#include <stdio.h>
#include <string.h>

static SDL_GLContext create_best_gl_context(SDL_Window* w) {
    static const struct {
        uint8_t v_major, v_minor;
    } list[] = {
        {4,6}, {4,5}, {4,4}, {4,3}, {4,2}, {4,1}, {4,0}, {3,3}, {3,2}
    };

    int32_t num = (int32_t)(sizeof(list) / sizeof(list[0]));
    for (int32_t i = 0; i < num; i++) {
        uint8_t v_major = list[i].v_major;
        uint8_t v_minor = list[i].v_minor;

#ifdef __APPLE__
        if (v_major > 4 || (v_major == 4 && v_minor > 1)) continue;
#endif
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, v_major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, v_minor);
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

static void detect_glsl_version(char* out, size_t size) {
    int32_t v_major = 0, v_minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &v_major);
    glGetIntegerv(GL_MINOR_VERSION, &v_minor);

    if (!v_major && !v_minor) {
        const char* ver = (const char* )glGetString(GL_VERSION);
        if (!ver || sscanf(ver, "%d.%d", &v_major, &v_minor) != 2) {
            v_major = 2;
            v_minor = 1;
        }
    }

    printf("Using OpenGL Version %d.%d\n", v_major, v_minor);

    uint32_t glsl = 150;
    if (v_major == 2) {
        glsl = (v_minor == 0) ? 110 : 120;
    } else if (v_major == 3) {
        glsl = (v_minor == 0) ? 130 : (v_minor == 1 ? 140 : 150);
    } else if (v_major == 4) {
        if (v_minor == 0) glsl = 400;
        else if (v_minor == 1) glsl = 410;
        else if (v_minor == 2) glsl = 420;
        else if (v_minor == 3) glsl = 430;
        else if (v_minor == 4) glsl = 440;
        else if (v_minor == 5) glsl = 450;
        else glsl = 460;
    }

    snprintf(out, size, "#version %d", glsl);
}

static uint32_t compile_shader(GLenum type, const char* src) {
    uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    int32_t result = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

    if (!result) {
        int32_t len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

        if (len > 0) {
            char buf[512];
            glGetShaderInfoLog(shader, sizeof(buf), NULL, buf);
            printf("ERROR: Failed to compile shader: %s", buf);
        }

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static uint32_t link_program(uint32_t vertex, uint32_t frag) {
    uint32_t program = glCreateProgram();

    glAttachShader(program, vertex);
    glAttachShader(program, frag);
    glBindAttribLocation(program, 0, "pos");
    glBindAttribLocation(program, 1, "uv");
    glLinkProgram(program);

    int32_t result = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &result);

    if (!result) {
        int32_t len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

        if (len > 0) {
            char buf[512];
            glGetProgramInfoLog(program, sizeof(buf), NULL, buf);
            printf("ERROR: Failed to link program: %s", buf);
        }

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static uint32_t program_init(_gui* gui) {
    char vertex_buf[1024], frag_buf[1024];
    snprintf(vertex_buf, sizeof(vertex_buf), "%s\n%s", gui->glsl_version, vertex_source);
    snprintf(frag_buf, sizeof(frag_buf), "%s\n%s", gui->glsl_version, frag_source);

    uint32_t vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_buf);
    if (!vertex_shader) {
        return 1;
    }

    uint32_t frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag_buf);
    if (!frag_shader) {
        glDeleteShader(vertex_shader);
        return 1;
    }

    gui->program = link_program(vertex_shader, frag_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(frag_shader);
    if (!gui->program) {
        return 1;
    }

    float verts[] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f
    };

    glGenVertexArrays(1, &gui->vao);
    glGenBuffers(1, &gui->vbo);

    glBindVertexArray(gui->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gui->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return 0;
}

uint8_t gui_init(_gui* gui, char* file) {
    memset(gui, 0, sizeof(_gui));

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    char title[128];
    snprintf(title, sizeof(title), "cnes - %s", file);

    gui->window = SDL_CreateWindow(
        title, 256 * 3, 240 * 3,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!gui->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    gui->gl_ctx = create_best_gl_context(gui->window);
    if (!gui->gl_ctx) {
        SDL_Log("Failed to create any OpenGL context: %s", SDL_GetError());
        return 1;
    }

    SDL_GL_MakeCurrent(gui->window, gui->gl_ctx);
    if (SDL_GL_SetSwapInterval(-1)) {
        printf("INFO: Using Adaptive Sync\n");
    } else if (SDL_GL_SetSwapInterval(1)) {
        printf("INFO: Using VSync\n");
    } else if (SDL_GL_SetSwapInterval(0)) {
        printf("INFO: Using No VSync\n");
    }

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        SDL_Log("Failed to initialize GLAD");
        return 1;
    }

    detect_glsl_version(gui->glsl_version, sizeof(gui->glsl_version));

    if (program_init(gui)) {
        SDL_Log("Failed to init program");
        return 1;
    }

    glGenTextures(1, &gui->nes_texture);
    glBindTexture(GL_TEXTURE_2D, gui->nes_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, NES_W, NES_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    gui->pixels = SDL_calloc(NES_PIXELS, sizeof(uint32_t));
    if (!gui->pixels) {
        SDL_Log("Failed to allocate pixel buffer");
        return 1;
    }

    CIMGUI_CHECKVERSION();
    gui->im_ctx = ImGui_CreateContext(NULL);
    ImGui_SetCurrentContext(gui->im_ctx);

    ImGuiIO* io = ImGui_GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    cImGui_ImplSDL3_InitForOpenGL(gui->window, gui->gl_ctx);
    cImGui_ImplOpenGL3_InitEx(gui->glsl_version);

    return 0;
}

void set_pixel(_gui* gui, uint16_t x, uint16_t y, uint32_t color) {
    if (!gui->pixels) return;
    if (x < NES_W && y < NES_H) gui->pixels[y * NES_W + x] = color;
}

static void draw_nes_texture(_gui* gui) {
    glBindTexture(GL_TEXTURE_2D, gui->nes_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, NES_W, NES_H, GL_RGBA, GL_UNSIGNED_BYTE, gui->pixels);

    int32_t width, height;
    SDL_GetWindowSize(gui->window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gui->program);
    int32_t location = glGetUniformLocation(gui->program, "u_tex");
    glUniform1i(location, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gui->nes_texture);

    glBindVertexArray(gui->vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

uint64_t gui_draw(_gui* gui) {
    if (!gui || !gui->window || !gui->gl_ctx) return 0;

    SDL_GL_MakeCurrent(gui->window, gui->gl_ctx);

    cImGui_ImplSDL3_NewFrame();
    cImGui_ImplOpenGL3_NewFrame();
    ImGui_NewFrame();

    if (ImGui_BeginMainMenuBar()) {
        if (ImGui_BeginMenu("File")) {
            if (ImGui_MenuItem("Settings")) {
                gui->show_settings = true;
            }

            if (ImGui_MenuItemWithIconEx("Quit", "X", "Ctrl-Q", false, true)) {
                gui->quit = true;
            }

            ImGui_EndMenu();
        }
        if (ImGui_BeginMenu("View")) {
            ImGui_EndMenu();
        }
        ImGui_EndMainMenuBar();
    }

    if (gui->show_settings) {
        if (ImGui_Begin("Settings", &gui->show_settings, 0)) {
            ImGui_Text("Settings");
        }
        ImGui_End();
    }

    ImGui_Render();

    draw_nes_texture(gui);
    cImGui_ImplOpenGL3_RenderDrawData(ImGui_GetDrawData());

    ImGuiIO* io = ImGui_GetIO();
    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui_UpdatePlatformWindows();
        ImGui_RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(gui->window, gui->gl_ctx);
    }

    uint64_t frame_end = SDL_GetPerformanceCounter();

    SDL_GL_SwapWindow(gui->window);
    return frame_end;
}

void gui_deinit(_gui* gui) {
    if (!gui) return;

    if (gui->im_ctx) {
        ImGui_SetCurrentContext(gui->im_ctx);
        cImGui_ImplOpenGL3_Shutdown();
        cImGui_ImplSDL3_Shutdown();
        ImGui_DestroyContext(gui->im_ctx);
        gui->im_ctx = NULL;
    }

    if (gui->pixels) {
        SDL_free(gui->pixels);
        gui->pixels = NULL;
    }

    if (gui->nes_texture) {
        glDeleteTextures(1, &gui->nes_texture);
        gui->nes_texture = 0;
    }

    if (gui->vao) {
        glDeleteVertexArrays(1, &gui->vao);
        gui->vao = 0;
    }

    if (gui->vbo) {
        glDeleteBuffers(1, &gui->vbo);
        gui->vbo = 0;
    }

    if (gui->program) {
        glDeleteProgram(gui->program);
        gui->program = 0;
    }

    if (gui->gl_ctx) {
        SDL_GL_DestroyContext(gui->gl_ctx);
        gui->gl_ctx = NULL;
    }

    if (gui->window) {
        SDL_DestroyWindow(gui->window);
        gui->window = NULL;
    }

    SDL_Quit();
}
