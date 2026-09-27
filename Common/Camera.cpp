#include "Camera.h"

#include <cmath>
#include <limits>

using namespace DirectX;

static constexpr float epsilon = std::numeric_limits<float>::epsilon();
static constexpr float upper = XM_PIDIV2 - epsilon;
static constexpr float lower = -XM_PIDIV2 + epsilon;

const XMVECTOR Camera::worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);

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
  XMVECTOR position = XMLoadFloat3(&m_Translate);

  return XMMatrixLookToRH(position, front, worldUp);
}

XMFLOAT3 Camera::WorldPos() const { return m_Translate; }

void Camera::Translate(float x, float y, float z) { m_Translate = {x, y, z}; }

void Camera::Target(float x, float y, float z)
{
  XMVECTOR p = XMLoadFloat3(&m_Translate);
  XMVECTOR t = XMVectorSet(x, y, z, 0.0f);
  XMVECTOR d = XMVector3Normalize(t - p);
  XMFLOAT3 dir;
  XMStoreFloat3(&dir, d);

  m_Yaw = atan2f(dir.x, dir.z);
  m_Pitch = asinf(dir.y);
}

void Camera::Follow(XMFLOAT3 position, XMFLOAT3 offset)
{
  XMVECTOR newPosition = XMLoadFloat3(&position) + XMLoadFloat3(&offset);
  XMStoreFloat3(&m_Translate, newPosition);
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
