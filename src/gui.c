#include "gui.h"
#include "ppu.h"
#include <stdio.h>
#include <string.h>

static SDL_GLContext create_best_gl_context(SDL_Window* window) {
    struct {
        int major;
        int minor;
    } candidates[] = {
        {4, 6},
        {4, 5},
        {4, 4},
        {4, 3},
        {4, 2},
        {4, 1},
        {4, 0},
        {3, 3},
        {3, 2}
    };

    int count = (int)(sizeof(candidates) / sizeof(candidates[0]));
    for (int i = 0; i < count; i++) {
        int major = candidates[i].major;
        int minor = candidates[i].minor;

#ifdef __APPLE__
        if (major > 4 || (major == 4 && minor > 1)) {
            continue;
        }
#endif

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
#endif

        SDL_GLContext ctx = SDL_GL_CreateContext(window);
        if (ctx) {
            printf("OpenGL Version %d.%d\n", major, minor);
            return ctx;
        }
    }

    return NULL;
}

static void detect_glsl_version(char* glsl_version, size_t size) {
    int major = 0;
    int minor = 0;

    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    if (major == 0 && minor == 0) {
        const char* ver = (const char*)glGetString(GL_VERSION);
        if (ver) {
            if (sscanf(ver, "%d.%d", &major, &minor) != 2) {
                major = 2;
                minor = 1;
            }
        } else {
            major = 2;
            minor = 1;
        }
    }

    SDL_Log("Using OpenGL Version %d.%d", major, minor);

    int glsl = 150;
    if (major == 2 && minor == 0) glsl = 110;
    else if (major == 2 && minor == 1) glsl = 120;
    else if (major == 3 && minor == 0) glsl = 130;
    else if (major == 3 && minor == 1) glsl = 140;
    else if (major == 3 && minor >= 2) glsl = 150;
    else if (major == 4) {
        if      (minor == 0) glsl = 400;
        else if (minor == 1) glsl = 410;
        else if (minor == 2) glsl = 420;
        else if (minor == 3) glsl = 430;
        else if (minor == 4) glsl = 440;
        else if (minor == 5) glsl = 450;
        else                 glsl = 460;
    }

    snprintf(glsl_version, size, "#version %d", glsl);
}

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            char buf[512];
            glGetShaderInfoLog(shader, sizeof(buf), NULL, buf);
            SDL_Log("shader compile error: %s", buf);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "aPos");
    glBindAttribLocation(program, 1, "aUV");
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            char buf[512];
            glGetProgramInfoLog(program, sizeof(buf), NULL, buf);
            SDL_Log("program link error: %s", buf);
        }
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static int init_quad_and_shader(_gui* gui) {
    char vs_src[512];
    char fs_src[512];

    snprintf(
        vs_src,
        sizeof(vs_src),
        "%s\n"
        "in vec2 aPos;\n"
        "in vec2 aUV;\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    vUV = aUV;\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n",
        gui->glsl_version
    );

    snprintf(
        fs_src,
        sizeof(fs_src),
        "%s\n"
        "uniform sampler2D uTex;\n"
        "in vec2 vUV;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    FragColor = texture(uTex, vec2(vUV.x, 1.0 - vUV.y));\n"
        "}\n",
        gui->glsl_version
    );

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    if (!vs) return 1;
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!fs) {
        glDeleteShader(vs);
        return 1;
    }

    gui->program = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!gui->program) return 1;

    float verts[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };

    glGenVertexArrays(1, &gui->vao);
    glGenBuffers(1, &gui->vbo);

    glBindVertexArray(gui->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gui->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*  sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*  sizeof(float), (void*)(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return 0;
}

int gui_init(_gui* gui, char* file) {
    memset(gui, 0, sizeof(_gui));

    int width = 256*  3;
    int height = 240*  3;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    char title[128];
    snprintf(title, sizeof(title), "cnes - %s", file);
    gui->window = SDL_CreateWindow(
        title,
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!gui->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    gui->gl_ctx = create_best_gl_context(gui->window);
    if (!gui->gl_ctx) {
        SDL_Log("Failed to create any OpenGL context");
        return 1;
    }

    SDL_GL_MakeCurrent(gui->window, gui->gl_ctx);
    if (SDL_GL_SetSwapInterval(-1) != 0) {
        SDL_GL_SetSwapInterval(1);
    }

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        SDL_Log("Failed to initialize glad");
        return 1;
    }

    detect_glsl_version(gui->glsl_version, sizeof(gui->glsl_version));

    if (init_quad_and_shader(gui) != 0) {
        SDL_Log("Failed to init GL quad/shader");
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
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    cImGui_ImplSDL3_InitForOpenGL(gui->window, gui->gl_ctx);
    cImGui_ImplOpenGL3_InitEx(gui->glsl_version);

    return 0;
}

void set_pixel(_gui* gui, uint16_t x, uint16_t y, uint32_t color) {
    if (!gui->pixels) return;
    if (x < NES_W && y < NES_H) {
        gui->pixels[y*  NES_W + x] = color;
    }
}

static void draw_nes_texture(_gui* gui) {
    glBindTexture(GL_TEXTURE_2D, gui->nes_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, NES_W, NES_H, GL_RGBA, GL_UNSIGNED_BYTE, gui->pixels);

    int win_w;
    int win_h;
    SDL_GetWindowSize(gui->window, &win_w, &win_h);
    glViewport(0, 0, win_w, win_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gui->program);
    GLint loc = glGetUniformLocation(gui->program, "uTex");
    glUniform1i(loc, 0);
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
            if (ImGui_MenuItem("Quit")) {
                gui->request_quit = true;
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

    if (gui->vbo) {
        glDeleteBuffers(1, &gui->vbo);
        gui->vbo = 0;
    }

    if (gui->vao) {
        glDeleteVertexArrays(1, &gui->vao);
        gui->vao = 0;
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
