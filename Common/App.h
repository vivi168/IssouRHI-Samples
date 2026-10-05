#pragma once

#include <IssouRHI.h>

#include <cstdint>
#include <memory>
#include <string>

struct GLFWwindow;
struct ImGuiContext;

class App
{
public:
  App(std::string title, int width = 800, int height = 600);
  virtual ~App();

  int Run();

protected:
  virtual void OnInit() = 0;
  virtual void OnUpdate(float /*dt*/, uint32_t /*frameIndex*/) {}
  virtual void OnRender(IssouRHI::Texture* target) = 0;
  virtual void OnResize() = 0;

  IssouRHI::Device& Device() const { return *m_Device; }
  IssouRHI::Surface& Surface() const { return *m_Surface; }

  void InitImGui(const IssouRHI::SurfaceConfiguration& config);
  void RenderImGui(IssouRHI::CommandEncoder* encoder, IssouRHI::Texture* target);

  void* NativeWindow() const;
  GLFWwindow* Window() const { return m_Window; }

  uint32_t Width() const { return static_cast<uint32_t>(m_Width); }
  uint32_t Height() const { return static_cast<uint32_t>(m_Height); }
  float AspectRatio() const { return (m_AspectRatio); }

private:
  void Init();

  static void ErrorCallback(int error, const char* description);
  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void ResizeCallback(GLFWwindow* window, int width, int height);

  std::string m_Title;
  int m_Width;
  int m_Height;
  float m_AspectRatio;
  bool m_ResizePending = false;
  bool m_GlfwInitialized = false;
  GLFWwindow* m_Window = nullptr;
  std::unique_ptr<IssouRHI::Device> m_Device;
  std::shared_ptr<IssouRHI::Surface> m_Surface;
  ImGuiContext* m_ImGuiContext = nullptr;
};
