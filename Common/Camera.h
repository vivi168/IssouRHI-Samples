#pragma once

#include <DirectXMath.h>

struct WorldPosition {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;

  DirectX::XMFLOAT3 RelativeTo(const WorldPosition& origin) const;
};

class Camera
{
public:
  Camera();
  void Translate(double x, double y, double z);
  void Target(double x, double y, double z);
  void Follow(WorldPosition position, DirectX::XMFLOAT3 offset);
  void Orient(float pitch, float yaw);
  void Move(float right, float up, float forward);
  void Rotate(float pitch, float yaw);
  void RotateAndMove(float pitch, float yaw, float right, float up, float forward);
  DirectX::XMMATRIX LookAt();
  WorldPosition WorldPos() const;
  DirectX::XMFLOAT3 WorldToLocal(const WorldPosition& position) const;

private:
  float m_Yaw, m_Pitch;

  static const DirectX::XMVECTOR worldUp;

  WorldPosition m_Translate;
};
