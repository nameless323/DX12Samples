#pragma once

#include "D3DUtil.h"

class Camera
{
public:
    Camera();
    ~Camera();

    DirectX::XMVECTOR GetPosition() const;
    DirectX::XMFLOAT3 GetPosition3f() const;
    void SetPosition(float x, float y, float z);
    void SetPosition(const DirectX::XMFLOAT3& v);

    DirectX::XMVECTOR GetRight() const;
    DirectX::XMFLOAT3 GetRight3f() const;
    DirectX::XMVECTOR GetUp() const;
    DirectX::XMFLOAT3 GetUp3f() const;
    DirectX::XMVECTOR GetFwd() const;
    DirectX::XMFLOAT3 GetFwd3f() const;

    float GetNear() const;
    float GetFar() const;
    float GetAspect() const;
    float GetFovY() const;
    float GetFovX() const;

    float GetNearWindowWidth() const;
    float GetNearWindowHeight() const;
    float GetFarWindowWidth() const;
    float GetFarWindowHeight() const;

    void SetFrustum(float fovY, float aspect, float nearZ, float farZ);

    void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
    void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& worldUp);

    DirectX::XMMATRIX GetView() const;
    DirectX::XMMATRIX GetProj() const;

    DirectX::XMFLOAT4X4 GetView4x4f() const;
    DirectX::XMFLOAT4X4 GetProj4x4f() const;

    void Strafe(float d);
    void Walk(float d);

    void Pitch(float angle);
    void RotateY(float angle);

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