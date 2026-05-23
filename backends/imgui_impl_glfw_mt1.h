// dear imgui: Platform Backend for GLFW with separated event/render queue
// Currently this needs to be used along with a OpenGL3 Renderer
// (Info: GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// Requires: GLFW 3.4+

#ifndef IMGUI_IMPL_GLFW_MT1_H
#define IMGUI_IMPL_GLFW_MT1_H

#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

struct GLFWwindow;
struct GLFWmonitor;

IMGUI_IMPL_API bool     ImGui_ImplGlfw_MT_InitForOpenGL(GLFWwindow* window, bool install_callbacks);
IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_NewFrame();

IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_CursorPosCallback(GLFWwindow* window, double x, double y);
IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_WindowSizeCallback(GLFWwindow* window, int width, int height);
IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_FramebufferSizeCallback(GLFWwindow* window, int width, int height);
IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_WindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale);
IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

//IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_WindowFocusCallback(GLFWwindow* window, int focused);
//IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_CursorEnterCallback(GLFWwindow* window, int entered);
//IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
//IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_CharCallback(GLFWwindow* window, unsigned int c);
//IMGUI_IMPL_API void     ImGui_ImplGlfw_MT_MonitorCallback(GLFWmonitor* monitor, int event);

#endif // #ifndef IMGUI_DISABLE
#endif // #ifndef IMGUI_IMPL_GLFW_MT1_H
