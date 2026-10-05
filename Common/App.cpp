#include "App.h"

#include <InteropD3D12.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_glfw.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

App::App(std::string title, int width, int height) : m_Title(std::move(title)), m_Width(width), m_Height(height) {}

App::~App()
{
  if (m_ImGuiContext) {
    m_Device->GetQueue()->WaitForAll();
    if (ImGui::GetIO().BackendRendererUserData) {
      ImGui_ImplDX12_Shutdown();
    }
    if (ImGui::GetIO().BackendPlatformUserData) {
      ImGui_ImplGlfw_Shutdown();
    }
    ImGui::DestroyContext(m_ImGuiContext);
  }

  m_Surface.reset();
  m_Device.reset();

  glfwDestroyWindow(m_Window);

  glfwTerminate();
}

void App::Init()
{
  glfwSetErrorCallback(ErrorCallback);

  m_GlfwInitialized = glfwInit();
  assert(m_GlfwInitialized);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
  assert(m_Window);

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

    auto target = m_Surface->GetCurrentTexture();

    if (m_ImGuiContext) {
      ImGui_ImplDX12_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
    }

    OnUpdate(dt, m_Surface->CurrentFrameIndex());
    OnRender(target);
  }

  if (m_Device) {
    m_Device->GetQueue()->WaitForAll();
  }

  return 0;
}

void App::InitImGui(const IssouRHI::SurfaceConfiguration& config)
{
  IMGUI_CHECKVERSION();

  {
    m_ImGuiContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    auto init = ImGui_ImplGlfw_InitForOther(m_Window, true);
    assert(init);
  }

  {
    ImGui_ImplDX12_InitInfo info{};
    info.Device = IssouRHI::D3D12::GetNativeDevice(m_Device.get());
    info.CommandQueue = IssouRHI::D3D12::GetNativeQueue(m_Device->GetQueue());
    info.NumFramesInFlight = static_cast<int>(config.bufferCount);
    info.RTVFormat = IssouRHI::D3D12::GetSwapChainFormat(m_Surface.get());
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.UserData = m_Device.get();
    info.SrvDescriptorHeap = IssouRHI::D3D12::CbvSrvUavDescriptorHeap(m_Device.get());
    info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* init, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
      IssouRHI::D3D12::AllocCbvSrvUavDescriptor(static_cast<IssouRHI::Device*>(init->UserData), cpu, gpu);
    };
    info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* init, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
      IssouRHI::D3D12::FreeCbvSrvUavDescriptor(static_cast<IssouRHI::Device*>(init->UserData), cpu, gpu);
    };

    auto init = ImGui_ImplDX12_Init(&info);
    assert(init);
  }
}

void App::RenderImGui(IssouRHI::CommandEncoder* encoder, IssouRHI::Texture* target)
{
  ImGui::Render();

  auto view = target->CreateView();
  auto rtv = IssouRHI::D3D12::RtvDescriptorHandle(view.get());

  auto commandList = IssouRHI::D3D12::GetNativeCommandList(encoder);
  commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
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
