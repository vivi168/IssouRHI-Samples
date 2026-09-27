#pragma once

#include <DirectXMath.h>

class Camera
{
public:
  Camera();
  void Translate(float x, float y, float z);
  void Target(float x, float y, float z);
  void Follow(DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 offset);
  void Orient(float pitch, float yaw);
  void Move(float right, float up, float forward);
  void Rotate(float pitch, float yaw);
  void RotateAndMove(float pitch, float yaw, float right, float up, float forward);
  DirectX::XMMATRIX LookAt();
  DirectX::XMFLOAT3 WorldPos() const;

private:
  float m_Yaw, m_Pitch;

  static const DirectX::XMVECTOR worldUp;

  DirectX::XMFLOAT3 m_Translate;
};
