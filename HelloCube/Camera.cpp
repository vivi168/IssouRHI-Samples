#include "Camera.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

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
  m_Speed = 3.0f;
  m_Sensitivity = 2.f;

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

void Camera::ProcessKeyboard(GLFWwindow* window, float dt)
{
  if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
    m_Pitch += m_Sensitivity * dt;
  }
  if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
    m_Pitch -= m_Sensitivity * dt;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
    m_Yaw += m_Sensitivity * dt;
  }
  if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
    m_Yaw -= m_Sensitivity * dt;
  }

  if (m_Pitch > upper)
    m_Pitch = upper;
  else if (m_Pitch < lower)
    m_Pitch = lower;

  float forwardX = sinf(m_Yaw);
  float forwardZ = cosf(m_Yaw);
  float rightX = -cosf(m_Yaw);
  float rightZ = sinf(m_Yaw);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    m_Translate.x += forwardX * m_Speed * dt;
    m_Translate.z += forwardZ * m_Speed * dt;
  }

  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    m_Translate.x -= forwardX * m_Speed * dt;
    m_Translate.z -= forwardZ * m_Speed * dt;
  }

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    m_Translate.x -= rightX * m_Speed * dt;
    m_Translate.z -= rightZ * m_Speed * dt;
  }

  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    m_Translate.x += rightX * m_Speed * dt;
    m_Translate.z += rightZ * m_Speed * dt;
  }

  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
    m_Translate.y -= m_Speed * dt;
  }

  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
    m_Translate.y += m_Speed * dt;
  }
}
