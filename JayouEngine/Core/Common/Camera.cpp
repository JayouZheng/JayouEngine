//
// Camera.cpp
//

#include "Camera.h"
#include <cassert>

using namespace Utility;

Camera::Camera()
{
	SetFrustum(0.25f*XM_PI, 1, 1, 1.0f, 1000.0f);
}

Camera::~Camera()
{
}

XMVECTOR Camera::GetPosition() const
{
	return XMLoadFloat3(&m_pos);
}

XMFLOAT3 Camera::GetPosition3f() const
{
	return m_pos;
}

void Camera::SetPosition(float x, float y, float z)
{
	m_pos = XMFLOAT3(x, y, z);
	m_viewDirty = true;
}

void Camera::SetPosition(const XMFLOAT3& v)
{
	m_pos = v;
	m_viewDirty = true;
}

void Camera::SetPosition(const XMVECTOR& v)
{
	XMFLOAT3 temp_v;
	XMStoreFloat3(&temp_v, v);
	m_pos = temp_v;
	m_viewDirty = true;
}

XMVECTOR Camera::GetRight() const
{
	return XMLoadFloat3(&m_right);
}

XMFLOAT3 Camera::GetRight3f() const
{
	return m_right;
}

XMVECTOR Camera::GetUp() const
{
	return XMLoadFloat3(&m_up);
}

XMFLOAT3 Camera::GetUp3f() const
{
	return m_up;
}

XMVECTOR Camera::GetLook() const
{
	return XMLoadFloat3(&m_look);
}

XMFLOAT3 Camera::GetLook3f() const
{
	return m_look;
}

float Camera::GetNearZ() const
{
	return m_nearZ;
}

float Camera::GetFarZ() const
{
	return m_farZ;
}

float Camera::GetAspect() const
{
	return m_aspectRatio;
}

float Camera::GetFovY() const
{
	return m_fovY;
}

float Camera::GetFovX() const
{
	float halfWidth = 0.5f*GetNearWindowWidth();
	return float(2.0f*atan(halfWidth / m_nearZ));
}

void Camera::SetFarZ(float farZ)
{
	m_farZ = farZ;
	UpdateProj();
}

float Camera::GetNearWindowWidth() const
{
	return m_aspectRatio * m_nearWindowHeight;
}

float Camera::GetNearWindowHeight() const
{
	return m_nearWindowHeight;
}

float Camera::GetFarWindowWidth() const
{
	return m_aspectRatio * m_farWindowHeight;
}

float Camera::GetFarWindowHeight() const
{
	return m_farWindowHeight;
}

ECameraViewType Camera::GetViewType() const
{
	return m_viewType;
}

void Camera::SetViewType(const ECameraViewType& type)
{
	m_viewType = type;
	m_viewDirty = true;
}

ECameraProjType Camera::GetProjType() const
{
	return m_projType;
}

void Camera::SetProjType(const ECameraProjType& type)
{
	m_projType = type;
	UpdateProj();
}

void Camera::SetFrustum(float fovY, int width, int height, float zn, float zf)
{
	// cache properties
	m_fovY = fovY;
	m_width = (float)width;
	m_height = (float)height;
	m_aspectRatio = static_cast<float>(width) / height;
	m_nearZ = zn;
	m_farZ = zf;

	m_nearWindowHeight = 2.0f * m_nearZ * tanf(0.5f*m_fovY);
	m_farWindowHeight = 2.0f * m_farZ * tanf(0.5f*m_fovY);

	UpdateProj();
}

void Camera::SetPerspectiveProj()
{
	if (m_projType != CP_PerspectiveProj)
		return;
	XMMATRIX P = XMMatrixPerspectiveFovLH(m_fovY, m_aspectRatio, m_nearZ, m_farZ);
	XMStoreFloat4x4(&m_proj, P);
}

void Camera::SetOrthographicProj()
{
	if (m_projType != CP_OrthographicProj)
		return;
	XMMATRIX P = XMMatrixOrthographicLH(m_width*m_radius*0.001f, m_height*m_radius*0.001f, m_nearZ, m_farZ);
	XMStoreFloat4x4(&m_proj, P);
}

void Camera::SetDirLightFrustum(float radius)
{
	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMFLOAT3 Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMVECTOR targetPos = XMLoadFloat3(&Center);
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, GetView()));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - radius;
	float b = sphereCenterLS.y - radius;
	float n = sphereCenterLS.z - radius;
	float r = sphereCenterLS.x + radius;
	float t = sphereCenterLS.y + radius;
	float f = sphereCenterLS.z + radius;

	m_nearZ = n;
	m_farZ = f;
	XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	XMStoreFloat4x4(&m_proj, P);
}

XMMATRIX Camera::GetShadowTransform()
{
	return GetViewProj() * Camera::NDCToTextureSpace;
}

void Camera::UpdateProj()
{
	switch (m_projType)
	{
	case CP_PerspectiveProj:
		SetPerspectiveProj();
		break;
	case CP_OrthographicProj:
		SetOrthographicProj();
		break;
	default:
		break;
	}
}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
	XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
	XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&m_pos, pos);
	XMStoreFloat3(&m_look, L);
	XMStoreFloat3(&m_right, R);
	XMStoreFloat3(&m_up, U);

	m_viewDirty = true;
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
	XMVECTOR P = XMLoadFloat3(&pos);
	XMVECTOR T = XMLoadFloat3(&target);
	XMVECTOR U = XMLoadFloat3(&up);

	LookAt(P, T, U);

	m_viewDirty = true;
}

void Camera::DirLightLookAt(const XMVECTOR& pos, const XMVECTOR& target, const XMVECTOR& up)
{
	LookAt(pos, target, up);
	XMStoreFloat4x4(&m_view, XMMatrixLookAtLH(pos, target, up));
}

XMMATRIX Camera::GetView() const
{
	// Only return if viewDirty false (After UpdateViewMatrix).
	// assert(!m_viewDirty); 
	return XMLoadFloat4x4(&m_view);
}

XMMATRIX Camera::GetProj() const
{
	return XMLoadFloat4x4(&m_proj);
}

XMMATRIX Camera::GetViewProj() const
{
	return GetView() * GetProj();
}

XMFLOAT4X4 Camera::GetView4x4f() const
{
	// Only return if viewDirty false (After UpdateViewMatrix).
	// assert(!m_viewDirty);
	return m_view;
}

XMFLOAT4X4 Camera::GetProj4x4f() const
{
	return m_proj;
}

void Camera::Strafe(float d)
{
	if (m_viewType != CV_FirstPersonView)
		return;
	// m_pos += d * m_right;
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&m_right);
	XMVECTOR p = XMLoadFloat3(&m_pos);
	XMStoreFloat3(&m_pos, XMVectorMultiplyAdd(s, r, p));

	m_viewDirty = true;
}

void Camera::Walk(float d)
{
	if (m_viewType != CV_FirstPersonView)
		return;
	// m_pos += d * m_look;
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&m_look);
	XMVECTOR p = XMLoadFloat3(&m_pos);
	XMStoreFloat3(&m_pos, XMVectorMultiplyAdd(s, l, p));

	m_viewDirty = true;
}

void Camera::Pitch(float angle)
{
	if (m_viewType != CV_FirstPersonView)
		return;
	// Rotate up and look vector about the right vector.

	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&m_right), angle);

	XMStoreFloat3(&m_up,   XMVector3TransformNormal(XMLoadFloat3(&m_up), R));
	XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), R));

	m_viewDirty = true;
}

void Camera::RotateY(float angle)
{
	if (m_viewType != CV_FirstPersonView)
		return;
	// Rotate the basis vectors about the world y-axis.

	XMMATRIX R = XMMatrixRotationY(angle);

	XMStoreFloat3(&m_right,   XMVector3TransformNormal(XMLoadFloat3(&m_right), R));
	XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), R));
	XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), R));

	m_viewDirty = true;
}

void Camera::Roll(float angle)
{
	if (m_viewType != CV_FirstPersonView)
		return;
	// Rotate up and look vector about the right vector.

	XMMATRIX L = XMMatrixRotationAxis(XMLoadFloat3(&m_look), angle);

	XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), L));
	XMStoreFloat3(&m_right, XMVector3TransformNormal(XMLoadFloat3(&m_right), L));

	m_viewDirty = true;
}

void Camera::FocusTheta(float angle)
{
	if (m_viewType == CV_FirstPersonView)
		return;
	m_theta += angle;
	m_viewDirty = true;
}

void Camera::FocusPhi(float angle)
{
	if (m_viewType == CV_FirstPersonView)
		return;
	m_phi += angle;
	m_phi = Math::Clamp(m_phi, 0.1f, XM_PI - 0.1f);
	m_viewDirty = true;
}

void Camera::FocusRadius(float length)
{
	if (m_viewType == CV_FirstPersonView)
		return;
	m_radius += length;
	m_radius = Math::Clamp(m_radius, m_nearZ, m_farZ);
	m_viewDirty = true;
}

void Camera::Move2D(float up, float right)
{
	if (m_viewType == CV_FirstPersonView || m_viewType == CV_FocusPointView)
		return;
	m_offsetUp += up * m_move2DSpeed;
	m_offsetRight -= right * m_move2DSpeed;
	m_viewDirty = true;
}

void Camera::ProcessMoving(float dx, float dy)
{
	FocusTheta(dx);
	FocusPhi(dy);

	RotateY(dx);
	Pitch(dy);

	Move2D(dy, dx);
}

void Camera::UpdateViewMatrix()
{
	if(m_viewDirty)
	{
		XMVECTOR R;
		XMVECTOR U;
		XMVECTOR L;
		XMVECTOR P;

		switch (m_viewType)
		{
		case CV_FirstPersonView:
		{
			U = XMLoadFloat3(&m_up);
			L = XMLoadFloat3(&m_look);
			P = XMLoadFloat3(&m_pos);
		}
			break;
		case CV_FocusPointView:
		{			
			// Convert Spherical to Cartesian coordinates.
			float x = m_radius * sinf(m_phi)*cosf(m_theta);
			float z = m_radius * sinf(m_phi)*sinf(m_theta);
			float y = m_radius * cosf(m_phi);

			// Build the view matrix.
			P = XMVectorSet(x, y, z, 1.0f);
			L = XMVectorZero() - P; // Look = Target - Pos.
			if (sinf(m_phi) < 0)
			{
				U = XMVectorSet(0.0f, -1.0f, 0.0f, 1.0f);
			}
			else
			{
				U = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
			}

			// XMMATRIX view = XMMatrixLookAtLH(pos, look, up);
			// XMStoreFloat4x4(&m_view, view);
		}
			break;
		case CV_TopView:
		{
			P = XMVectorSet(m_offsetRight, m_radius, m_offsetUp, 1.0f);
			U = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
			R = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
			L = XMVectorSet(0.0f, -1.0f, 0.0f, 1.0f);
		}
			break;
		case CV_BottomView:
		{
			P = XMVectorSet(-m_offsetRight, -m_radius, m_offsetUp, 1.0f);
			U = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
			R = XMVectorSet(-1.0f, 0.0f, 0.0f, 1.0f);
			L = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
		}
		break;
		case CV_LeftView:
		{
			P = XMVectorSet(-m_radius, m_offsetUp, -m_offsetRight, 1.0f);
			U = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
			R = XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f);
			L = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
		}
		break;
		case CV_RightView:
		{
			P = XMVectorSet(m_radius, m_offsetUp, m_offsetRight, 1.0f);
			U = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
			R = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
			L = XMVectorSet(-1.0f, 0.0f, 0.0f, 1.0f);
		}
		break;
		case CV_FrontView:
		{
			P = XMVectorSet(m_offsetRight, m_offsetUp, -m_radius, 1.0f);
			U = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
			R = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
			L = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
		}
		break;
		case CV_BackView:
		{
			P = XMVectorSet(-m_offsetRight, m_offsetUp, m_radius, 1.0f);
			U = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
			R = XMVectorSet(-1.0f, 0.0f, 0.0f, 1.0f);
			L = XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f);
		}
		break;
		default:
			break;
		}
		XMStoreFloat3(&m_pos, P);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		R = XMVector3Normalize(XMVector3Cross(U, L));

		// U, L already ortho-normal, so no need to normalize cross product.
		U = XMVector3Cross(L, R);

		// Fill in the view matrix entries.
		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		if (m_viewType == CV_FirstPersonView)
		{
			XMStoreFloat3(&m_right, R);
			XMStoreFloat3(&m_up, U);
			XMStoreFloat3(&m_look, L);
		}

		m_view(0, 0) = XMVectorGetX(R);
		m_view(1, 0) = XMVectorGetY(R);
		m_view(2, 0) = XMVectorGetZ(R);
		m_view(3, 0) = x;

		m_view(0, 1) = XMVectorGetX(U);
		m_view(1, 1) = XMVectorGetY(U);
		m_view(2, 1) = XMVectorGetZ(U);
		m_view(3, 1) = y;

		m_view(0, 2) = XMVectorGetX(L);
		m_view(1, 2) = XMVectorGetY(L);
		m_view(2, 2) = XMVectorGetZ(L);
		m_view(3, 2) = z;

		m_view(0, 3) = 0.0f;
		m_view(1, 3) = 0.0f;
		m_view(2, 3) = 0.0f;
		m_view(3, 3) = 1.0f;

		m_viewDirty = false;
	}
}
