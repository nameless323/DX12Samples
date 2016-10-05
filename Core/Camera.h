//
// Simple first person camera class. Just enough to review the world.
//

#pragma once

#include "D3DUtil.h"

class Camera
{
public:
    Camera();
    ~Camera();

    /**
     * \brief Get camera position as XMVECTOR. Actualy loads XMFLOAT3 to XMVECTOR internally.
     */
    DirectX::XMVECTOR GetPosition() const;
    /**
     * \brief Get camera position as XMFLOAT3.
     */
    DirectX::XMFLOAT3 GetPosition3f() const;
    /**
     * \brief Set position as x, y, z (just loads it values to XMFLOAT3 internally).
     */
    void SetPosition(float x, float y, float z);
    /**
     * \brief Set position as XMFLOAT (faster then x,y,z overload).
     */
    void SetPosition(const DirectX::XMFLOAT3& v);
    /**
     * \brief Get camera X axis. Actualy loads XMFLOAT3 to XMVECTOR internally.
     */
    DirectX::XMVECTOR GetRight() const;
    /**
     * \brief Get camera X axis.
     */
    DirectX::XMFLOAT3 GetRight3f() const;
    /**
     * \brief Get camera Y axis. Actualy loads XMFLOAT3 to XMVECTOR internally.
     */
    DirectX::XMVECTOR GetUp() const;
    /**
     * \brief Get camera Y axis.
     */
    DirectX::XMFLOAT3 GetUp3f() const;
    /**
     * \brief Get camera Z axis. Actualy loads XMFLOAT3 to XMVECTOR internally.
     */
    DirectX::XMVECTOR GetFwd() const;
    /**
     * \brief Get camera Z axis.
     */
    DirectX::XMFLOAT3 GetFwd3f() const;
    /**
     * \brief Get camera near clip plane.
     */
    float GetNear() const;
    /**
     * \brief Get camera far clip plane.
     */
    float GetFar() const;
    /**
     * \brief Get camera aspect ratio.
     */
    float GetAspect() const;
    /**
     * \brief Get Y fov.
     */
    float GetFovY() const;
    /**
     * \brief Calculates and returns X fov from Y fov.
     */
    float GetFovX() const;
    /**
     * \brief Returns near window width.
     */
    float GetNearWindowWidth() const;
    /**
     * \brief Returns near window height.
     */
    float GetNearWindowHeight() const;
    /**
     * \brief Returns far window width.
     */
    float GetFarWindowWidth() const;
    /**
     * \brief Returns far window height.
     */
    float GetFarWindowHeight() const;
    /**
     * \brief Creates and sets perspective matrix based on parameters.
     */
    void SetFrustum(float fovY, float aspect, float nearZ, float farZ);
    /**
     * \brief Creates lookAt transform.
     */
    void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
    /**
     * \brief Creates lookAt transform. Internally uses LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp)
     */
    void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& worldUp);
    /**
     * \brief Get camera view matirx. Actualy loads XMFLOAT4X4 to XMMATRIX internally.
     */
    DirectX::XMMATRIX GetView() const;
    /**
     * \brief Get camera projection matirx. Actualy loads XMFLOAT4X4 to XMMATRIX internally.
     */
    DirectX::XMMATRIX GetProj() const;
    /**
     * \brief Get camera view matirx.
     */
    DirectX::XMFLOAT4X4 GetView4x4f() const;
    /**
     * \brief Get camera projection matirx.
     */
    DirectX::XMFLOAT4X4 GetProj4x4f() const;
    /**
     * \brief Strafe camera.
     */
    void Strafe(float d);
    /**
     * \brief Move camera forvard/backward.
     */
    void Walk(float d);
    /**
     * \brief Rotates camera around camera's X axis.
     */
    void Pitch(float angle);
    /**
     * \brief Rotates camera around world Y axis.
     */
    void RotateY(float angle);

    /**
     * \brief Call this when transform was changed.
     */
    void UpdateViewMatrix();

private:
    DirectX::XMFLOAT3 _position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 _right = { 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 _up = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 _fwd = { 0.0f, 0.0f, 1.0f };

    float _near = 0.0f;
    float _far = 0.0f;
    float _aspect = 0.0f;
    float _fovY = 0.0f;
    float _nearWindowHeight = 0.0f;
    float _farWindowHeight = 0.0f;

    bool _isViewDirty = true;

    DirectX::XMFLOAT4X4 _view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _proj = MathHelper::Identity4x4();
};
