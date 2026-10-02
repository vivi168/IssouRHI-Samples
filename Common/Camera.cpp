#include "Camera.h"

#include <cmath>
#include <limits>

using namespace DirectX;

static constexpr float epsilon = std::numeric_limits<float>::epsilon();
static constexpr float upper = XM_PIDIV2 - epsilon;
static constexpr float lower = -XM_PIDIV2 + epsilon;

const XMVECTOR Camera::worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);

XMFLOAT3 WorldPosition::RelativeTo(const WorldPosition& origin) const
{
  return {
      static_cast<float>(x - origin.x),
      static_cast<float>(y - origin.y),
      static_cast<float>(z - origin.z),
  };
}

Camera::Camera()
{
  m_Yaw = XM_PI;
  m_Pitch = 0;

  m_Translate = {0, 0, 10.f};
}

XMMATRIX Camera::LookAt()
{
  const float r = cosf(m_Pitch);
  XMVECTOR front = XMVector3Normalize(XMVectorSet(sinf(m_Yaw) * r, sinf(m_Pitch), cosf(m_Yaw) * r, 0.f));
  return XMMatrixLookToRH(XMVectorZero(), front, worldUp);
}

WorldPosition Camera::WorldPos() const { return m_Translate; }

XMFLOAT3 Camera::WorldToLocal(const WorldPosition& position) const { return position.RelativeTo(m_Translate); }

void Camera::Translate(double x, double y, double z) { m_Translate = {x, y, z}; }

void Camera::Target(double x, double y, double z)
{
  const auto relative = WorldToLocal({x, y, z});
  XMVECTOR d = XMVector3Normalize(XMLoadFloat3(&relative));
  XMFLOAT3 dir;
  XMStoreFloat3(&dir, d);

  m_Yaw = atan2f(dir.x, dir.z);
  m_Pitch = asinf(dir.y);
}

void Camera::Follow(WorldPosition position, XMFLOAT3 offset)
{
  m_Translate = {position.x + offset.x, position.y + offset.y, position.z + offset.z};
}

void Camera::Orient(float pitch, float yaw)
{
  m_Pitch = pitch;
  m_Yaw = yaw;
}

void Camera::Move(float right, float up, float forward)
{
  m_Translate.x += sinf(m_Yaw) * forward - cosf(m_Yaw) * right;
  m_Translate.y += up;
  m_Translate.z += cosf(m_Yaw) * forward + sinf(m_Yaw) * right;
}

void Camera::Rotate(float pitch, float yaw)
{
  m_Pitch += pitch;
  m_Yaw += yaw;

  if (m_Pitch > upper) {
    m_Pitch = upper;
  } else if (m_Pitch < lower) {
    m_Pitch = lower;
  }
}

void Camera::RotateAndMove(float pitch, float yaw, float right, float up, float forward)
{
  Rotate(pitch, yaw);
  Move(right, up, forward);
}
