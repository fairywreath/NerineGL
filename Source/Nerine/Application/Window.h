#pragma once

#include <GLFW/glfw3.h>

#include <Core/Types.h>

#include <functional>
#include <string>
#include <vector>

namespace Nerine
{

class Window
{
public:
    explicit Window(u32 width, u32 height, const std::string& name);
    ~Window();

    NON_COPYABLE(Window);
    NON_MOVEABLE(Window);

    void Destroy();
    void Close();
    void PollEvents();

    void SwapBuffers();

    bool ShouldClose() const;
    void Resize(u32 width, u32 height);

    u32 GetWidth() const;
    u32 GetHeight() const;
    void* GetNativeWindow() const;

    using CursorPosCallback = std::function<void(double, double)>;
    using MouseButtonCallback = std::function<void(int, int, int)>;
    using KeyCallback = std::function<void(int, int, int, int)>;
    using ResizeCallback = std::function<void(int, int)>;
    using ScrollCallback = std::function<void(double, double)>;

    void AddCursorPosCallback(const CursorPosCallback& callback)
    {
        m_CursorPosCallbacks.push_back(callback);
    }
    void AddMouseButtonCallback(const MouseButtonCallback& callback)
    {
        m_MouseButtonCallbacks.push_back(callback);
    }
    void AddKeyCallback(const KeyCallback& callback)
    {
        m_KeyCallbacks.push_back(callback);
    }
    void AddResizeCallback(const ResizeCallback& callback)
    {
        m_ResizeCallbacks.push_back(callback);
    }
    void AddScrollCallback(const ScrollCallback& callback)
    {
        m_ScrollCallbacks.push_back(callback);
    }

private:
    void OnCursorPos(double x, double y);
    void OnMouseButton(int key, int action, int mods);
    void OnKey(int key, int scancode, int action, int mods);
    void OnResize(int width, int height);
    void OnScroll(double xOffset, double yOffset);

    static void OnCursorPosCallback(GLFWwindow* window, double x, double y);
    static void OnMouseButtonCallback(GLFWwindow* window, int key, int action, int mods);
    static void OnKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void OnResizeCallback(GLFWwindow* window, int width, int height);
    static void OnScrollCallback(GLFWwindow* window, double xOffset, double yOffset);

private:
    GLFWwindow* m_Window{nullptr};

    u32 m_Width{0};
    u32 m_Height{0};
    std::string m_Name{""};

    std::vector<CursorPosCallback> m_CursorPosCallbacks;
    std::vector<MouseButtonCallback> m_MouseButtonCallbacks;
    std::vector<KeyCallback> m_KeyCallbacks;
    std::vector<ResizeCallback> m_ResizeCallbacks;
    std::vector<ScrollCallback> m_ScrollCallbacks;
};

} // namespace Nerine