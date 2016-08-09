#include "Camera.h"

using namespace DirectX;

Camera::Camera()
{
    SetFrustum(0.25f * MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera()
{}

XMVECTOR Camera::GetPosition() const
{
    return XMLoadFloat3(&_position);
}

XMFLOAT3 Camera::GetPosition3f() const
{
    return _position;
}

void Camera::SetPosition(float x, float y, float z)
{
    _position = XMFLOAT3(x, y, z);
    _isViewDirty = true;
}

void Camera::SetPosition(const XMFLOAT3& v)
{
    _position = v;
    _isViewDirty = true;
}

XMVECTOR Camera::GetRight() const
{
    return XMLoadFloat3(&_right);
}

XMFLOAT3 Camera::GetRight3f() const
{
    return _right;
}

XMVECTOR Camera::GetUp() const
{
    return XMLoadFloat3(&_up);
}

XMFLOAT3 Camera::GetUp3f() const
{
    return _up;
}

XMVECTOR Camera::GetFwd() const
{
    return XMLoadFloat3(&_fwd);
}

XMFLOAT3 Camera::GetFwd3f() const
{
    return _fwd;
}

float Camera::GetNear() const
{
    return _near;
}

float Camera::GetFar() const
{
    return _far;
}

float Camera::GetAspect() const
{
    return _aspect;
}

float Camera::GetFovY() const
{
    return _fovY;
}

float Camera::GetFovX() const
{
    float halfWidth = 0.5f * GetNearWindowWidth();
    return 2.0f * atan(halfWidth / _near);
}

float Camera::GetNearWindowWidth() const
{
    return _aspect * _nearWindowHeight;
}

float Camera::GetNearWindowHeight() const
{
    return _nearWindowHeight;
}

float Camera::GetFarWindowWidth() const
{
    return _aspect * _farWindowHeight;
}

float Camera::GetFarWindowHeight() const
{
    return _farWindowHeight;
}

void Camera::SetFrustum(float fovY, float aspect, float nearZ, float farZ)
{
    _fovY = fovY;
    _aspect = aspect;
    _near = nearZ;
    _far = farZ;

    _nearWindowHeight = 2.0f * _near * tanf(0.5f * _fovY);
    _farWindowHeight = 2.0f * _far * tanf(0.5f * _fovY);

    XMMATRIX p = XMMatrixPerspectiveFovLH(_fovY, _aspect, _near, _far);
    XMStoreFloat4x4(&_proj, p);
}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
    XMVECTOR fwd = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, fwd));
    XMVECTOR up = XMVector3Cross(fwd, right);

    XMStoreFloat3(&_position, pos);
    XMStoreFloat3(&_fwd, fwd);
    XMStoreFloat3(&_right, right);
    XMStoreFloat3(&_up, up);

    _isViewDirty = true;
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& worldUp)
{
    XMVECTOR p = XMLoadFloat3(&pos);
    XMVECTOR t = XMLoadFloat3(&target);
    XMVECTOR u = XMLoadFloat3(&worldUp);
    LookAt(p, t, u);

    _isViewDirty = true;
}

XMMATRIX Camera::GetView() const
{
    assert(!_isViewDirty);
    return XMLoadFloat4x4(&_view);
}

XMMATRIX Camera::GetProj() const
{
    return XMLoadFloat4x4(&_proj);
}

XMFLOAT4X4 Camera::GetView4x4f() const
{
    assert(!_isViewDirty);
    return _view;
}

XMFLOAT4X4 Camera::GetProj4x4f() const
{
    return _proj;
}

void Camera::Strafe(float d)
{
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR r = XMLoadFloat3(&_right);
    XMVECTOR p = XMLoadFloat3(&_position);
    XMStoreFloat3(&_position, XMVectorMultiplyAdd(s, r, p));
    _isViewDirty = true;
}

void Camera::Walk(float d)
{
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR f = XMLoadFloat3(&_fwd);
    XMVECTOR p = XMLoadFloat3(&_position);
    XMStoreFloat3(&_position, XMVectorMultiplyAdd(s, f, p));
    _isViewDirty = true;
}

/// <summary> Rotates around local right. </summary>
void Camera::Pitch(float angle)
{
    XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&_right), angle);

    XMStoreFloat3(&_up, XMVector3TransformNormal(XMLoadFloat3(&_up), R));
    XMStoreFloat3(&_fwd, XMVector3TransformNormal(XMLoadFloat3(&_fwd), R));

    _isViewDirty = true;
}

/// <summary> Rotates around world up. </summary>
void Camera::RotateY(float angle)
{
    XMMATRIX R = XMMatrixRotationY(angle);
    XMStoreFloat3(&_up, XMVector3TransformNormal(XMLoadFloat3(&_up), R));
    XMStoreFloat3(&_fwd, XMVector3TransformNormal(XMLoadFloat3(&_fwd), R));
    XMStoreFloat3(&_right, XMVector3TransformNormal(XMLoadFloat3(&_right), R));

    _isViewDirty = true;
}

void Camera::UpdateViewMatrix()
{
    if (_isViewDirty)
    {
        XMVECTOR right = XMLoadFloat3(&_right);
        XMVECTOR up = XMLoadFloat3(&_up);
        XMVECTOR fwd = XMLoadFloat3(&_fwd);
        XMVECTOR pos = XMLoadFloat3(&_position);

        fwd = XMVector3Normalize(fwd);
        up = XMVector3Normalize(XMVector3Cross(fwd, right));
        right = XMVector3Cross(up, fwd);

        float x = -XMVectorGetX(XMVector3Dot(pos, right));
        float y = -XMVectorGetX(XMVector3Dot(pos, up));
        float z = -XMVectorGetX(XMVector3Dot(pos, fwd));

        XMStoreFloat3(&_right, right);
        XMStoreFloat3(&_up, up);
        XMStoreFloat3(&_fwd, fwd);

        _view(0, 0) = _right.x;
        _view(1, 0) = _right.y;
        _view(2, 0) = _right.z;
        _view(3, 0) = x;

        _view(0, 1) = _up.x;
        _view(1, 1) = _up.y;
        _view(2, 1) = _up.z;
        _view(3, 1) = y;

        _view(0, 2) = _fwd.x;
        _view(1, 2) = _fwd.y;
        _view(2, 2) = _fwd.z;
        _view(3, 2) = z;

        _view(0, 3) = 0.0f;
        _view(1, 3) = 0.0f;
        _view(2, 3) = 0.0f;
        _view(3, 3) = 1.0f;

        _isViewDirty = false;
    }
}
