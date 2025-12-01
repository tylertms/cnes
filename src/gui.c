#include "gui.h"
#include "dcimgui.h"
#include "ppu.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <float.h>

#include "nes_vert_spv.h"
#include "nes_frag_spv.h"
#include "nes_vert_msl.h"
#include "nes_frag_msl.h"

#define FRAME_HISTORY 120

static float g_frame_times[FRAME_HISTORY];
static int g_frame_times_index = 0;
static bool g_frame_times_filled = false;
static uint64_t g_last_perf_counter = 0;

static char* game_name(char* path) {
    char* last_forslash = strrchr(path, '/');
    char* last_backslash = strrchr(path, '\\');
    char* last_slash = last_forslash > last_backslash ? last_forslash : last_backslash;
    return last_slash == NULL ? path : last_slash + 1;
}

static float get_window_scale(SDL_Window *window) {
    int w, h, pw, ph;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    if (w > 0 && h > 0) {
        float sx = (float)pw / (float)w;
        float sy = (float)ph / (float)h;
        float s = sx > 0.f ? sx : (sy > 0.f ? sy : 1.f);
        return s > 0.f ? s : 1.f;
    }
    return 1.f;
}

static void record_frame_time(uint64_t frame_end) {
    uint64_t freq = SDL_GetPerformanceFrequency();
    if (g_last_perf_counter != 0 && freq != 0) {
        double dt_ms = (double)(frame_end - g_last_perf_counter) * 1000.0 / (double)freq;
        g_frame_times[g_frame_times_index] = (float)dt_ms;
        g_frame_times_index = (g_frame_times_index + 1) % FRAME_HISTORY;
        if (g_frame_times_index == 0)
            g_frame_times_filled = true;
    }
    g_last_perf_counter = frame_end;
}

static bool configure_present_mode(_gui *gui) {
    const SDL_GPUSwapchainComposition comp = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    const SDL_GPUPresentMode modes[] = {
        SDL_GPU_PRESENTMODE_MAILBOX,
        #ifdef __APPLE__
        SDL_GPU_PRESENTMODE_VSYNC,
        SDL_GPU_PRESENTMODE_IMMEDIATE,
        #else
        SDL_GPU_PRESENTMODE_IMMEDIATE,
        SDL_GPU_PRESENTMODE_VSYNC,
        #endif

    };
    const size_t num_modes = sizeof(modes) / sizeof(modes[0]);
    for (size_t i = 0; i < num_modes; ++i) {
        SDL_GPUPresentMode mode = modes[i];
        if (!SDL_WindowSupportsGPUPresentMode(gui->gpu_device, gui->window, mode))
            continue;
        if (SDL_SetGPUSwapchainParameters(gui->gpu_device, gui->window, comp, mode)) {
            SDL_Log("Using present mode %d (0=VSYNC, 1=IMMEDIATE, 2=MAILBOX)", (int)mode);
            return true;
        }
    }
    SDL_Log("Warning: Failed to set preferred present modes, keeping default swapchain parameters!");
    return false;
}

static bool create_texture(_gui *gui) {
    const SDL_GPUTextureCreateInfo tinfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = NES_W,
        .height = NES_H,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    };
    gui->nes_texture = SDL_CreateGPUTexture(gui->gpu_device, &tinfo);
    if (!gui->nes_texture) {
        SDL_Log("SDL_CreateGPUTexture failed: %s", SDL_GetError());
        return false;
    }
    const SDL_GPUTransferBufferCreateInfo transfer_buffer = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = NES_PIXELS * sizeof(uint32_t),
    };
    gui->nes_transfer = SDL_CreateGPUTransferBuffer(gui->gpu_device, &transfer_buffer);
    if (!gui->nes_transfer) {
        SDL_Log("SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }
    const SDL_GPUSamplerCreateInfo sampler_create_info = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    gui->nes_sampler = SDL_CreateGPUSampler(gui->gpu_device, &sampler_create_info);
    if (!gui->nes_sampler) {
        SDL_Log("ERROR: SDL_CreateGPUSampler failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

static bool create_pipeline(_gui *gui) {
    SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(gui->gpu_device);
    SDL_GPUShaderFormat fmt = 0;
    if (supported & SDL_GPU_SHADERFORMAT_SPIRV) {
        fmt = SDL_GPU_SHADERFORMAT_SPIRV;
    } else if (supported & SDL_GPU_SHADERFORMAT_MSL) {
        fmt = SDL_GPU_SHADERFORMAT_MSL;
    } else {
        SDL_Log("ERROR: No supported shader format (SPIR-V/MSL) available");
        return false;
    }

    const uint8_t *vs_code = NULL;
    const uint8_t *fs_code = NULL;
    size_t vs_size = 0;
    size_t fs_size = 0;

    if (fmt == SDL_GPU_SHADERFORMAT_SPIRV) {
        vs_code = (const uint8_t *)nes_vert_spv;
        vs_size = (size_t)nes_vert_spv_len;
        fs_code = (const uint8_t *)nes_frag_spv;
        fs_size = (size_t)nes_frag_spv_len;
    } else {
        vs_code = (const uint8_t *)nes_vert_msl;
        vs_size = (size_t)nes_vert_msl_len;
        fs_code = (const uint8_t *)nes_frag_msl;
        fs_size = (size_t)nes_frag_msl_len;
    }

    const char *entry = (fmt == SDL_GPU_SHADERFORMAT_MSL) ? "main0" : "main";

    const SDL_GPUShaderCreateInfo vs_info = {
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .format = fmt,
        .code = vs_code,
        .code_size = vs_size,
        .entrypoint = entry,
    };
    gui->nes_vs = SDL_CreateGPUShader(gui->gpu_device, &vs_info);
    if (!gui->nes_vs) {
        SDL_Log("ERROR: SDL_CreateGPUShader (vertex) failed: %s", SDL_GetError());
        return false;
    }

    const SDL_GPUShaderCreateInfo fs_info = {
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .format = fmt,
        .code = fs_code,
        .code_size = fs_size,
        .entrypoint = entry,
        .num_samplers = 1,
    };
    gui->nes_fs = SDL_CreateGPUShader(gui->gpu_device, &fs_info);
    if (!gui->nes_fs) {
        SDL_Log("ERROR: SDL_CreateGPUShader (fragment) failed: %s", SDL_GetError());
        return false;
    }

    const SDL_GPUVertexInputState vin = (SDL_GPUVertexInputState){0};
    const SDL_GPURasterizerState rs = {
        .fill_mode = SDL_GPU_FILLMODE_FILL,
        .cull_mode = SDL_GPU_CULLMODE_NONE,
        .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
    };
    const SDL_GPUMultisampleState ms = {
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
    const SDL_GPUDepthStencilState ds = {0};
    const SDL_GPUColorTargetDescription ctd = {
        .format = SDL_GetGPUSwapchainTextureFormat(gui->gpu_device, gui->window),
    };
    const SDL_GPUGraphicsPipelineTargetInfo ti = {
        .num_color_targets = 1,
        .color_target_descriptions = &ctd,
    };
    const SDL_GPUGraphicsPipelineCreateInfo pi = {
        .vertex_shader = gui->nes_vs,
        .fragment_shader = gui->nes_fs,
        .vertex_input_state = vin,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
        .rasterizer_state = rs,
        .multisample_state = ms,
        .depth_stencil_state = ds,
        .target_info = ti,
        .props = 0,
    };
    gui->nes_pipeline = SDL_CreateGPUGraphicsPipeline(gui->gpu_device, &pi);
    if (!gui->nes_pipeline) {
        SDL_Log("ERROR: SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

static void upload_texture(_gui *gui, SDL_GPUCommandBuffer *cmdbuf) {
    if (!gui || !gui->pixels || !gui->nes_transfer || !gui->nes_texture)
        return;

    uint32_t *dst = (uint32_t *)SDL_MapGPUTransferBuffer(gui->gpu_device, gui->nes_transfer, true);
    if (!dst)
        return;

    memcpy(dst, gui->pixels, NES_PIXELS * sizeof(uint32_t));
    SDL_UnmapGPUTransferBuffer(gui->gpu_device, gui->nes_transfer);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmdbuf);
    if (!copy)
        return;

    const SDL_GPUTextureTransferInfo xfer = {
        .transfer_buffer = gui->nes_transfer,
        .offset = 0,
        .pixels_per_row = NES_W,
        .rows_per_layer = NES_H,
    };
    const SDL_GPUTextureRegion region = {
        .texture = gui->nes_texture,
        .mip_level = 0,
        .layer = 0,
        .x = 0,
        .y = 0,
        .z = 0,
        .w = NES_W,
        .h = NES_H,
        .d = 1,
    };
    SDL_UploadToGPUTexture(copy, &xfer, &region, true);
    SDL_EndGPUCopyPass(copy);
}

static void draw_main_menu(_gui *gui, _nes *nes) {
    if (ImGui_BeginMainMenuBar()) {
        gui->menu_height = ImGui_GetWindowSize().y;

        if (ImGui_BeginMenu("FILE")) {
            if (ImGui_MenuItem("RESET"))
                nes_reset(nes);
            if (ImGui_MenuItem("SETTINGS"))
                gui->show_settings = true;
            if (ImGui_MenuItem("QUIT"))
                nes->cpu.halt = 1;
            ImGui_EndMenu();
        }
        if (ImGui_BeginMenu("VIEW")) {
            ImGui_EndMenu();
        }
        ImGui_EndMainMenuBar();
    } else {
        gui->menu_height = 0.0f;
    }
}

static void draw_settings_window(_gui *gui) {
    if (!gui->show_settings)
        return;

    ImGui_SetNextWindowSize((ImVec2){480.0f, 320.0f}, ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse;

    if (ImGui_Begin("Settings", &gui->show_settings, flags)) {
        int count = g_frame_times_filled ? FRAME_HISTORY : g_frame_times_index;
        if (count > 0) {
            ImVec2 size = (ImVec2){160.0f, 40.0f};
            ImGui_PlotLinesEx(
                "Frame time (ms)",
                g_frame_times,
                count,
                g_frame_times_filled ? g_frame_times_index : 0,
                NULL,
                NES_FRAME_TIME * 1000.0f - 2.0f,
                NES_FRAME_TIME * 1000.0f + 2.0f,
                size,
                sizeof(float)
            );
        }
    }
    ImGui_End();
}

uint8_t gui_init(_gui *gui, char *file) {
    memset(gui, 0, sizeof(_gui));

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        SDL_Log("ERROR: SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    gui->window = SDL_CreateWindow(
        game_name(file),
        NES_W * 3,
        NES_H * 3,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!gui->window) {
        SDL_Log("ERROR: SDL_CreateWindow failed: %s", SDL_GetError());
        gui_deinit(gui);
        return 1;
    }

    gui->gpu_device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL,
        false, NULL
    );
    if (!gui->gpu_device) {
        SDL_Log("ERROR: SDL_CreateGPUDevice failed: %s", SDL_GetError());
        gui_deinit(gui);
        return 1;
    }

    if (!SDL_ClaimWindowForGPUDevice(gui->gpu_device, gui->window)) {
        SDL_Log("ERROR: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        gui_deinit(gui);
        return 1;
    }

    configure_present_mode(gui);

    if (!create_texture(gui)) {
        gui_deinit(gui);
        return 1;
    }
    if (!create_pipeline(gui)) {
        gui_deinit(gui);
        return 1;
    }

    gui->pixels = (uint32_t *)SDL_calloc(NES_PIXELS, sizeof(uint32_t));
    if (!gui->pixels) {
        SDL_Log("ERROR: Failed to allocate pixel buffer");
        gui_deinit(gui);
        return 1;
    }

    CIMGUI_CHECKVERSION();
    gui->im_ctx = ImGui_CreateContext(NULL);
    ImGui_SetCurrentContext(gui->im_ctx);

    ImGuiIO *io = ImGui_GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io->ConfigViewportsNoDecoration = false;
    io->ConfigViewportsNoAutoMerge = true;

    ImGuiStyle *style = ImGui_GetStyle();
    style->FramePadding = (ImVec2){0.f, 8.f};
    style->ItemSpacing = (ImVec2){8.f, 16.f};
    style->Colors[ImGuiCol_MenuBarBg] = (ImVec4){0.1f, 0.1f, 0.1f, 1.0f};

    float scale = get_window_scale(gui->window);
    if (scale <= 0.0f)
        scale = 1.0f;

    ImFontConfig cfg;
    memset(&cfg, 0, sizeof(ImFontConfig));
    cfg.FontDataOwnedByAtlas = true;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.GlyphMaxAdvanceX = FLT_MAX;
    cfg.RasterizerMultiply = 1.0f;
    cfg.RasterizerDensity = scale;
    cfg.EllipsisChar = 0;

    float base_font_size = 10.0f;
    float actual_size_pixels = base_font_size * scale;

    gui->nes_font = ImFontAtlas_AddFontFromFileTTF(
        io->Fonts,
        "external/fonts/PressStart2P-Regular.ttf",
        actual_size_pixels,
        &cfg,
        NULL
    );
    if (!gui->nes_font) {
        SDL_Log("ERROR: failed to load NES font!");
    } else {
        io->FontDefault = gui->nes_font;
        io->FontGlobalScale = 1.0f / scale;
    }

    cImGui_ImplSDL3_InitForSDLGPU(gui->window);

    ImGui_ImplSDLGPU3_InitInfo init_info;
    SDL_zero(init_info);
    init_info.Device = gui->gpu_device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gui->gpu_device, gui->window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    cImGui_ImplSDLGPU3_Init(&init_info);

    return 0;
}

void set_pixel(_gui *gui, uint16_t x, uint16_t y, uint32_t color) {
    if (!gui || !gui->pixels) return;
    if (x >= NES_W || y >= NES_H) return;
    gui->pixels[y * NES_W + x] = color;
}

uint64_t gui_draw(_gui *gui, _nes *nes) {
    if (!gui || !gui->window || !gui->gpu_device) return 0;

    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(gui->gpu_device);
    if (!cmdbuf) return 0;

    upload_texture(gui, cmdbuf);

    SDL_GPUTexture *swapchain_tex = NULL;
    uint32_t sw = 0, sh = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, gui->window, &swapchain_tex, &sw, &sh) || !swapchain_tex) {
        SDL_CancelGPUCommandBuffer(cmdbuf);
        SDL_Delay(16);
        return SDL_GetPerformanceCounter();
    }

    cImGui_ImplSDLGPU3_NewFrame();
    cImGui_ImplSDL3_NewFrame();
    ImGui_NewFrame();

    draw_main_menu(gui, nes);
    draw_settings_window(gui);

    ImGui_Render();
    ImDrawData *draw_data = ImGui_GetDrawData();
    cImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdbuf);

    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = swapchain_tex;
    color_target.mip_level = 0;
    color_target.layer_or_depth_plane = 0;
    color_target.clear_color.r = 0.0f;
    color_target.clear_color.g = 0.0f;
    color_target.clear_color.b = 0.0f;
    color_target.clear_color.a = 1.0f;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.cycle = false;
    color_target.cycle_resolve_texture = false;

    SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(cmdbuf, &color_target, 1, NULL);
    if (render_pass) {
        ImGuiIO *io = ImGui_GetIO();
        float scale_y = io->DisplayFramebufferScale.y > 0.0f ? io->DisplayFramebufferScale.y : 1.0f;
        float menu_height_px = gui->menu_height * scale_y;
        if (menu_height_px < 0.0f) menu_height_px = 0.0f;
        if (menu_height_px >= (float)sh) menu_height_px = 0.0f;

        SDL_GPUViewport game_vp;
        game_vp.x = 0.0f;
        game_vp.y = menu_height_px;
        game_vp.w = (float)sw;
        game_vp.h = (float)sh - menu_height_px;
        game_vp.min_depth = 0.0f;
        game_vp.max_depth = 1.0f;
        SDL_SetGPUViewport(render_pass, &game_vp);

        SDL_Rect game_scissor;
        game_scissor.x = (int)game_vp.x;
        game_scissor.y = (int)game_vp.y;
        game_scissor.w = (int)game_vp.w;
        game_scissor.h = (int)game_vp.h;
        SDL_SetGPUScissor(render_pass, &game_scissor);

        SDL_BindGPUGraphicsPipeline(render_pass, gui->nes_pipeline);

        SDL_GPUTextureSamplerBinding tex_binding;
        SDL_zero(tex_binding);
        tex_binding.texture = gui->nes_texture;
        tex_binding.sampler = gui->nes_sampler;

        SDL_BindGPUFragmentSamplers(render_pass, 0, &tex_binding, 1);
        SDL_DrawGPUPrimitives(render_pass, 4, 1, 0, 0);

        SDL_GPUViewport ui_vp;
        ui_vp.x = 0.0f;
        ui_vp.y = 0.0f;
        ui_vp.w = (float)sw;
        ui_vp.h = (float)sh;
        ui_vp.min_depth = 0.0f;
        ui_vp.max_depth = 1.0f;
        SDL_SetGPUViewport(render_pass, &ui_vp);

        SDL_Rect ui_scissor = {0, 0, (int)sw, (int)sh};
        SDL_SetGPUScissor(render_pass, &ui_scissor);

        cImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdbuf, render_pass);
        SDL_EndGPURenderPass(render_pass);
    }

    ImGuiIO *io = ImGui_GetIO();
    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui_UpdatePlatformWindows();
        ImGui_RenderPlatformWindowsDefault();
    }

    uint64_t frame_end = SDL_GetPerformanceCounter();
    SDL_SubmitGPUCommandBuffer(cmdbuf);

    record_frame_time(frame_end);
    return frame_end;
}

void gui_deinit(_gui *gui) {
    if (!gui) return;

    if (gui->im_ctx) {
        ImGui_SetCurrentContext(gui->im_ctx);
        cImGui_ImplSDLGPU3_Shutdown();
        cImGui_ImplSDL3_Shutdown();
        ImGui_DestroyContext(gui->im_ctx);
        gui->im_ctx = NULL;
    }

    if (gui->pixels) {
        SDL_free(gui->pixels);
        gui->pixels = NULL;
    }

    if (gui->nes_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(gui->gpu_device, gui->nes_pipeline);
        gui->nes_pipeline = NULL;
    }

    if (gui->nes_vs) {
        SDL_ReleaseGPUShader(gui->gpu_device, gui->nes_vs);
        gui->nes_vs = NULL;
    }

    if (gui->nes_fs) {
        SDL_ReleaseGPUShader(gui->gpu_device, gui->nes_fs);
        gui->nes_fs = NULL;
    }

    if (gui->nes_sampler) {
        SDL_ReleaseGPUSampler(gui->gpu_device, gui->nes_sampler);
        gui->nes_sampler = NULL;
    }

    if (gui->nes_transfer) {
        SDL_ReleaseGPUTransferBuffer(gui->gpu_device, gui->nes_transfer);
        gui->nes_transfer = NULL;
    }

    if (gui->nes_texture) {
        SDL_ReleaseGPUTexture(gui->gpu_device, gui->nes_texture);
        gui->nes_texture = NULL;
    }

    if (gui->gpu_device) {
        SDL_ReleaseWindowFromGPUDevice(gui->gpu_device, gui->window);
        SDL_DestroyGPUDevice(gui->gpu_device);
        gui->gpu_device = NULL;
    }

    if (gui->window) {
        SDL_DestroyWindow(gui->window);
        gui->window = NULL;
    }

    SDL_Quit();
}
