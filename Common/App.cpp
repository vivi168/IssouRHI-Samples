#include "App.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

App::App(std::string title, int width, int height) : m_Title(std::move(title)), m_Width(width), m_Height(height) {}

App::~App()
{
  m_Surface.reset();
  m_Device.reset();

  if (m_Window) {
    glfwDestroyWindow(m_Window);
  }

  if (m_GlfwInitialized) {
    glfwTerminate();
  }
}

void App::Init()
{
  glfwSetErrorCallback(ErrorCallback);

  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW");
  }

  m_GlfwInitialized = true;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);

  if (!m_Window) {
    throw std::runtime_error("Failed to create GLFW window");
  }

  glfwSetWindowUserPointer(m_Window, this);
  glfwSetKeyCallback(m_Window, KeyCallback);
  glfwSetFramebufferSizeCallback(m_Window, ResizeCallback);
  glfwGetFramebufferSize(m_Window, &m_Width, &m_Height);
  m_AspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);

  m_Device = IssouRHI::Device::CreateDevice(IssouRHI::Backend::D3D12, {});
  m_Surface = m_Device->CreateSurface(NativeWindow());

  OnInit();
}

int App::Run()
{
  Init();

  double previousTime = glfwGetTime();

  while (!glfwWindowShouldClose(m_Window)) {
    glfwPollEvents();

    if (glfwWindowShouldClose(m_Window)) {
      break;
    }

    const double now = glfwGetTime();
    const float dt = static_cast<float>(std::min(now - previousTime, 0.1));
    previousTime = now;

    if (m_Width <= 0 || m_Height <= 0) {
      glfwWaitEvents();
      continue;
    }

    if (m_ResizePending) {
      m_ResizePending = false;
      OnResize();
    }

    OnUpdate(dt);
    OnRender();
  }

  if (m_Device) {
    m_Device->GetQueue()->WaitForAll();
  }

  return 0;
}

void* App::NativeWindow() const
{
  return glfwGetWin32Window(m_Window);
}

void App::ErrorCallback(int, const char* description)
{
  fprintf(stderr, "GLFW: %s\n", description);
}

void App::KeyCallback(GLFWwindow* window, int key, int, int action, int)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

void App::ResizeCallback(GLFWwindow* window, int width, int height)
{
  auto& app = *static_cast<App*>(glfwGetWindowUserPointer(window));
  app.m_Width = width;
  app.m_Height = height;
  app.m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);

  app.m_ResizePending = true;
}
