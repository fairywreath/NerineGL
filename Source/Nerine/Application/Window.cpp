#include "Window.h"

#include <Core/Logger.h>

namespace Nerine
{

Window::Window(u32 width, u32 height, const std::string& name)
    : m_Width(width), m_Height(height), m_Name(name)
{
    glfwSetErrorCallback(
        [](int error, const char* description) { LOG_ERROR("GLFW error: ", description); });

    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    m_Window = glfwCreateWindow(m_Width, m_Height, m_Name.c_str(), nullptr, nullptr);

    if (!m_Window)
    {
        LOG_ERROR("Failed to create glfw window!");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(0);

    glfwSetWindowUserPointer(m_Window, this);

    glfwSetCursorPosCallback(m_Window, OnCursorPosCallback);
    glfwSetMouseButtonCallback(m_Window, OnMouseButtonCallback);
    glfwSetKeyCallback(m_Window, OnKeyCallback);
    glfwSetFramebufferSizeCallback(m_Window, OnResizeCallback);
    glfwSetScrollCallback(m_Window, OnScrollCallback);
}

Window::~Window()
{
    if (m_Window)
    {
        Destroy();
    }
}

void Window::Destroy()
{
    glfwDestroyWindow(m_Window);
    glfwTerminate();

    m_Window = nullptr;
}

void Window::Close()
{
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

void Window::PollEvents()
{
    glfwPollEvents();
}

void Window::SwapBuffers()
{
    glfwSwapBuffers(m_Window);
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

void Window::Resize(u32 width, u32 height)
{
    glfwSetWindowSize(m_Window, width, height);
}

u32 Window::GetWidth() const
{
    return m_Width;
}

u32 Window::GetHeight() const
{
    return m_Height;
}

void* Window::GetNativeWindow() const
{
    return static_cast<void*>(m_Window);
}

void Window::OnCursorPos(double x, double y)
{
    for (const auto& callback : m_CursorPosCallbacks)
    {
        callback(x, y);
    }
}

void Window::OnMouseButton(int key, int action, int mods)
{
    for (const auto& callback : m_MouseButtonCallbacks)
    {
        callback(key, action, mods);
    }
}

void Window::OnKey(int key, int scancode, int action, int mods)
{
    for (const auto& callback : m_KeyCallbacks)
    {
        callback(key, scancode, action, mods);
    }
}

void Window::OnResize(int width, int height)
{
    for (const auto& callback : m_ResizeCallbacks)
    {
        callback(width, height);
    }
}

void Window::OnScroll(double xOffset, double yOffset)
{
    for (const auto& callback : m_ScrollCallbacks)
    {
        callback(xOffset, yOffset);
    }
}

void Window::OnCursorPosCallback(GLFWwindow* window, double x, double y)
{
    static_cast<Window*>(glfwGetWindowUserPointer(window))->OnCursorPos(x, y);
}

void Window::OnMouseButtonCallback(GLFWwindow* window, int key, int action, int mods)
{
    static_cast<Window*>(glfwGetWindowUserPointer(window))->OnMouseButton(key, action, mods);
}

void Window::OnKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    static_cast<Window*>(glfwGetWindowUserPointer(window))->OnKey(key, scancode, action, mods);
}

void Window::OnResizeCallback(GLFWwindow* window, int width, int height)
{
    static_cast<Window*>(glfwGetWindowUserPointer(window))->OnResize(width, height);
}

void Window::OnScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    static_cast<Window*>(glfwGetWindowUserPointer(window))->OnScroll(xOffset, yOffset);
}

} // namespace Nerine
