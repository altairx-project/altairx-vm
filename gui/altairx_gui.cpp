// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#include "altairx_gui.hpp"

#include <array>
#include <vector>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

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

namespace
{
struct ConsoleData
{
    bool showed{};
    std::array<char, 256> input{};
    std::vector<std::string> items{};
    std::vector<std::string> commands{};
    std::vector<std::string> history{};
    int history_pos{}; // -1: new line, 0..History.Size-1 browsing history.
    // ImGuiTextFilter filter;
    bool auto_scroll{};
    bool scroll_to_bottom{};
};
}

struct AltairXGUI::Data
{
#if __cplusplus >= 202002L
    const std::string working_directory{reinterpret_cast<const char*>(fs::current_path().u8string().data())};
#else
    const std::string working_directory{fs::current_path().u8string()};
#endif

    bool done{};
    bool selecting_file{};
    std::string selected_file{};
    ConsoleData console{};
};

namespace
{

std::string from_path(const fs::path& path)
{
#if 1 || __cplusplus >= 202002L
    const auto tmp = path.u8string();
    return std::string{reinterpret_cast<const char*>(tmp.data()), tmp.size()};
#else
    return path.u8string();
#endif
}

fs::path to_path(const std::string& path)
{
#if __cplusplus >= 202002L
    std::u8string output;
    output.resize(path.size());
    std::memcpy(output.data(), path.data(), path.size());
    return fs::path{std::move(output)};
#else
    return fs::u8path(path);
#endif
}

bool is_root_path(const std::string& str)
{
#ifdef _WIN32
    return str.size() >= 3 && str.substr(1, 3) == ":/";
#else
    return !str.empty() && str[0] == '/';
#endif
}

int select_file_callback(ImGuiInputTextCallbackData* data)
{
  std::string& str = *static_cast<std::string*>(data->UserData);
  if(data->EventFlag == ImGuiInputTextFlags_CallbackResize)
  {
      str.resize(data->BufTextLen);
      data->Buf = str.data();
  }
  else if(data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
  {
      const fs::path current_path = to_path(str);
      const std::string beginning = from_path(current_path.filename());
      for(auto& entry : fs::directory_iterator{current_path.parent_path()})
      {
          if(!entry.is_regular_file() && !entry.is_directory())
          {
              continue;
          }

          const auto name = from_path(entry.path().filename());
          if(std::empty(beginning))
          {
              if(str.back() != '/')
              {
                  data->InsertChars(data->CursorPos, "/");
              }
              data->InsertChars(data->CursorPos, from_path(name).data());
              return 0;
          }
          else if(name == beginning && entry.is_directory())
          {
              data->InsertChars(data->CursorPos, "/");
              return 0;
          }
          else if(name.size() > beginning.size() && name.substr(0, beginning.size()) == beginning)
          {
              data->InsertChars(data->CursorPos, from_path(name.substr(beginning.size())).data());
              return 0;
          }
      }
  }

  return 0;
}

}

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
        ImGui::ShowDemoWindow();
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
                m_data->selected_file.clear();
                m_data->selecting_file = true;
            }

            if(ImGui::MenuItem("Exit"))
            {
                m_data->done = true;
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("View"))
        {
          ImGui::MenuItem("Console", "CTRL+I", &m_data->console);
          ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    draw_console();

    if(ImGui::Begin("Main Window"))
    {
        if(m_data->selecting_file)
        {
            ImGui::OpenPopup("OpenFile");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if(ImGui::BeginPopupModal("OpenFile", &m_data->selecting_file, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Enter file path:");
            ImGui::Separator();
            if(ImGui::InputTextWithHint("##", m_data->working_directory.data(), m_data->selected_file.data(), m_data->selected_file.capacity() + 1,
                ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_EnterReturnsTrue,
                &select_file_callback, &m_data->selected_file))
            {
                std::cout << "Loaded file " << m_data->selected_file << std::endl;
            }

            ImGui::EndPopup();
        }

    }

    ImGui::End();
}

void AltairXGUI::draw_console()
{
    /*
  ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
  if(!ImGui::Begin("Console", &m_data->console))
  {
    ImGui::End();
  }

  ImGui::TextWrapped("Enter 'HELP' for help.");
  ImGui::Separator();

  // Reserve enough left-over height for 1 separator + 1 input text
  const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
  if(ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
  {
    if(ImGui::BeginPopupContextWindow())
    {
      if(ImGui::Selectable("Clear")) 
      {
        //ClearLog();
      }
      ImGui::EndPopup();
    }

    // Display every line as a separate entry so we can change their color or add custom widgets.
    // If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
    // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping
    // to only process visible items. The clipper will automatically measure the height of your first item and then
    // "seek" to display only items in the visible area.
    // To use the clipper we can replace your standard loop:
    //      for (int i = 0; i < Items.Size; i++)
    //   With:
    //      ImGuiListClipper clipper;
    //      clipper.Begin(Items.Size);
    //      while (clipper.Step())
    //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
    // - That your items are evenly spaced (same height)
    // - That you have cheap random access to your elements (you can access them given their index,
    //   without processing all the ones before)
    // You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
    // We would need random-access on the post-filtered list.
    // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
    // or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
    // and appending newly elements as they are inserted. This is left as a task to the user until we can manage
    // to improve this example code!
    // If your items are of variable height:
    // - Split them into same height items would be simpler and facilitate random-seeking into your list.
    // - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
    for(const char* item : Items)
    {
      if(!Filter.PassFilter(item))
        continue;
    
      // Normally you would store more information in your item than just a string.
      // (e.g. make Items[] an array of structure, store color/type etc.)
      ImVec4 color;
      bool has_color = false;
      if(strstr(item, "[error]"))
      {
          color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
          has_color = true;
      }
      else if(strncmp(item, "# ", 2) == 0)
      {
          color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f);
          has_color = true;
      }
      if(has_color)
      {
          ImGui::PushStyleColor(ImGuiCol_Text, color);
      }
      ImGui::TextUnformatted(item);
      if(has_color)
      {
          ImGui::PopStyleColor();
      }
    }
    //if(copy_to_clipboard)
    //{
    //    ImGui::LogFinish();
    //}

    // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
    // Using a scrollbar or mouse-wheel will take away from the bottom edge.
    if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
      ImGui::SetScrollHereY(1.0f);
    }

    ImGui::PopStyleVar();
  }
  ImGui::EndChild();
  ImGui::Separator();

  auto TextEditCallbackStub = [](ImGuiInputTextCallbackData* data)
  {

    //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
    switch(data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCompletion:
    {
      // Example of TEXT COMPLETION

      // Locate beginning of current word
      const char* word_end = data->Buf + data->CursorPos;
      const char* word_start = word_end;
      while(word_start > data->Buf)
      {
        const char c = word_start[-1];
        if(c == ' ' || c == '\t' || c == ',' || c == ';')
          break;
        word_start--;
      }

      // Build a list of candidates
      ImVector<const char*> candidates;
      //for(int i = 0; i < Commands.Size; i++)
      //  if(Strnicmp(Commands[i], word_start, (int)(word_end - word_start)) == 0)
      //    candidates.push_back(Commands[i]);

      if(candidates.Size == 0)
      {
        // No match
        //AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
      }
      else if(candidates.Size == 1)
      {
        // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
        data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
        data->InsertChars(data->CursorPos, candidates[0]);
        data->InsertChars(data->CursorPos, " ");
      }
      else
      {
        // Multiple matches. Complete as much as we can..
        // So inputting "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
        int match_len = (int)(word_end - word_start);
        for(;;)
        {
          int c = 0;
          bool all_candidates_matches = true;
          for(int i = 0; i < candidates.Size && all_candidates_matches; i++)
            if(i == 0)
              c = toupper(candidates[i][match_len]);
            else if(c == 0 || c != toupper(candidates[i][match_len]))
              all_candidates_matches = false;
          if(!all_candidates_matches)
            break;
          match_len++;
        }

        if(match_len > 0)
        {
          data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
          data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
        }

        // List matches
        //AddLog("Possible matches:\n");
        //for(int i = 0; i < candidates.Size; i++)
          //AddLog("- %s\n", candidates[i]);
      }

      break;
    }
    //case ImGuiInputTextFlags_CallbackHistory:
    //{
    //  // Example of HISTORY
    //  const int prev_history_pos = HistoryPos;
    //  if(data->EventKey == ImGuiKey_UpArrow)
    //  {
    //    if(HistoryPos == -1)
    //      HistoryPos = History.Size - 1;
    //    else if(HistoryPos > 0)
    //      HistoryPos--;
    //  }
    //  else if(data->EventKey == ImGuiKey_DownArrow)
    //  {
    //    if(HistoryPos != -1)
    //      if(++HistoryPos >= History.Size)
    //        HistoryPos = -1;
    //  }
    //
    //  // A better implementation would preserve the data on the current input line along with cursor position.
    //  if(prev_history_pos != HistoryPos)
    //  {
    //    const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
    //    data->DeleteChars(0, data->BufTextLen);
    //    data->InsertChars(0, history_str);
    //  }
    //}
    //}
    //return 0;
  };
  
  //// Command-line
  //bool reclaim_focus = false;
  //ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
  //if(ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, TextEditCallbackStub, (void*)this))
  //{
  //  char* s = InputBuf;
  //  //Strtrim(s);
  //  //if(s[0])
  //  //  ExecCommand(s);
  //  //strcpy(s, "");
  //  reclaim_focus = true;
  //}

  // Auto-focus on window apparition
  //ImGui::SetItemDefaultFocus();
  //if(reclaim_focus)
  //{
  //    ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
  //}

  ImGui::End();*/
}
