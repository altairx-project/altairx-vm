// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#include "altairx_gui.hpp"

#include <array>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <SDL3/SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <panic.hpp>

namespace
{

class AxSDLApp
{
public:
    AxSDLApp()
    {
        if(!SDL_Init(SDL_INIT_VIDEO))
        {
            ax_panic("Error: SDL_Init(): ", SDL_GetError());
        }
    }

    ~AxSDLApp()
    {
        SDL_Quit();
    }

    AxSDLApp(const AxSDLApp&) = delete;
    AxSDLApp& operator=(const AxSDLApp&) = delete;
    AxSDLApp(AxSDLApp&&) noexcept = delete;
    AxSDLApp& operator=(AxSDLApp&&) noexcept = delete;
};

class AxSDLWindow
{
public:
    AxSDLWindow()
    {
        // Create window with SDL_Renderer graphics context
        const auto window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
        if(!SDL_CreateWindowAndRenderer("AltairX K1 Virtual Machine", 1280, 720, window_flags, &m_window, &m_renderer))
        {
            ax_panic("Error: SDL_CreateWindow(): ", SDL_GetError());
        }

        SDL_SetRenderVSync(m_renderer, 1); // Always use VSync
        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(m_window);
    }

    ~AxSDLWindow()
    {
        SDL_DestroyRenderer(m_renderer);
        SDL_DestroyWindow(m_window);
    }

    AxSDLWindow(const AxSDLWindow&) = delete;
    AxSDLWindow& operator=(const AxSDLWindow&) = delete;
    AxSDLWindow(AxSDLWindow&&) noexcept = delete;
    AxSDLWindow& operator=(AxSDLWindow&&) noexcept = delete;

    SDL_Window* window() const noexcept
    {
        return m_window;
    }

    SDL_Renderer* renderer() const noexcept
    {
        return m_renderer;
    }

private:
    SDL_Window* m_window{};
    SDL_Renderer* m_renderer{};
};

class AxImGUWindow
{
public:
    AxImGUWindow(AxSDLWindow& window)
        :m_window{window}
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        m_context = ImGui::CreateContext();

        ImGui::SetCurrentContext(m_context);
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsLight();
        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForSDLRenderer(window.window(), window.renderer());
        ImGui_ImplSDLRenderer3_Init(window.renderer());
    }

    ~AxImGUWindow()
    {
        ImGui::SetCurrentContext(m_context);
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    AxImGUWindow(const AxImGUWindow&) = delete;
    AxImGUWindow& operator=(const AxImGUWindow&) = delete;
    AxImGUWindow(AxImGUWindow&&) noexcept = delete;
    AxImGUWindow& operator=(AxImGUWindow&&) noexcept = delete;

    void make_current()
    {
        ImGui::SetCurrentContext(m_context);
    }

    void begin_frame()
    {
        ImGui::SetCurrentContext(m_context);
        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void present()
    {
        // Rendering
        ImGui::Render();

        //SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(m_window.renderer(), m_clear_color.x, m_clear_color.y, m_clear_color.z, m_clear_color.w);
        SDL_RenderClear(m_window.renderer());
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_window.renderer());
        SDL_RenderPresent(m_window.renderer());
    }

private:
    AxSDLWindow& m_window;
    ImGuiContext* m_context{};
    ImVec4 m_clear_color{0.45f, 0.55f, 0.60f, 1.00f};
};

}

struct AltairXGUI::Impl
{
    AxSDLApp app{};
    AxSDLWindow window{};
    AxImGUWindow context{window};
};

struct AltairXGUI::Data
{
    bool done{};
    bool selecting_file{};
};

AltairXGUI::AltairXGUI()
    :m_impl{std::make_unique<Impl>()}
    ,m_data{std::make_unique<Data>()}
{

}

AltairXGUI::~AltairXGUI() = default;

int AltairXGUI::run()
{
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    // Main loop
    while(!m_data->done)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if(event.type == SDL_EVENT_QUIT)
            {
                m_data->done = true;
            }

            if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_impl->window.window()))
            {
                m_data->done = true;
            }
        }

        // So nothing when window is minimized
        if(SDL_GetWindowFlags(m_impl->window.window()) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        m_impl->context.begin_frame();
        //ImGui::ShowDemoWindow();
        draw_ui();
        m_impl->context.present();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    return 0;
}

void AltairXGUI::draw_ui()
{
    if(ImGui::BeginMainMenuBar())
    {
        if(ImGui::BeginMenu("File"))
        {
            if(ImGui::MenuItem("Load ELF file"))
            {
                m_data->selecting_file = true;
            }

            if(ImGui::MenuItem("Exit"))
            {
                m_data->done = true;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if(ImGui::Begin("Main Window"))
    {

    }
    
    ImGui::End();
}
