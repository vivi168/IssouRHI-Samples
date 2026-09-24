#pragma once

#include <DirectXMath.h>

struct GLFWwindow;

class Camera
{
public:
  Camera();
  void Translate(float x, float y, float z);
  void Target(float x, float y, float z);
  void Follow(DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 offset);
  void Orient(float pitch, float yaw);
  void ProcessKeyboard(GLFWwindow* window, float dt);
  DirectX::XMMATRIX LookAt();
  DirectX::XMFLOAT3 WorldPos() const;

private:
  float m_Yaw, m_Pitch;
  float m_Speed, m_Sensitivity;

  static const DirectX::XMVECTOR worldUp;

  DirectX::XMFLOAT3 m_Translate;
};
