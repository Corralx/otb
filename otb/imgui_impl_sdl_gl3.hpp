// ImGui SDL2 binding with OpenGL3
// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

struct SDL_Window;
typedef union SDL_Event SDL_Event;

bool        ImGui_ImplSdlGL3_Init(SDL_Window *window);
void        ImGui_ImplSdlGL3_Shutdown();
void        ImGui_ImplSdlGL3_NewFrame();
bool        ImGui_ImplSdlGL3_ProcessEvent(SDL_Event* event);

// Use if you want to reset your rendering device without losing ImGui state.
void        ImGui_ImplSdlGL3_InvalidateDeviceObjects();
bool        ImGui_ImplSdlGL3_CreateDeviceObjects();
