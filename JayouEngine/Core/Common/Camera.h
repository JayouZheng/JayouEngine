//
// Camera.h
//

#pragma once

#include "../Math/Math.h"
#include "TypeDef.h"

#include "Interface/IObject.h"

using namespace Core;
using namespace DirectX;

namespace Utility
{
	class Camera : public IObject
	{
	public:

		Camera();
		~Camera();

		// Get/Set world camera position.
		XMVECTOR GetPosition() const;
		XMFLOAT3 GetPosition3f() const;
		void SetPosition(float x, float y, float z);
		void SetPosition(const XMFLOAT3& v);

		// Get camera basis vectors.
		XMVECTOR GetRight() const;
		XMFLOAT3 GetRight3f() const;
		XMVECTOR GetUp() const;
		XMFLOAT3 GetUp3f() const;
		XMVECTOR GetLook() const;
		XMFLOAT3 GetLook3f() const;

		// Get/Set frustum properties.
		float GetNearZ() const;
		float GetFarZ() const;
		float GetAspect() const;
		float GetFovY() const;
		float GetFovX() const;
		void  SetFarZ(float farZ);

		// Get near and far plane dimensions in view space coordinates.
		float GetNearWindowWidth() const;
		float GetNearWindowHeight() const;
		float GetFarWindowWidth() const;
		float GetFarWindowHeight() const;

		// Get/Set View Type.
		ECameraViewType GetViewType() const;
		void SetViewType(const ECameraViewType& type);

		// Get/Set Proj Type.
		ECameraProjType GetProjType() const;
		void SetProjType(const ECameraProjType& type);

		// Set frustum.
		void SetFrustum(float fovY, int width, int height, float zn, float zf);
		void SetPerspectiveProj();
		void SetOrthographicProj();

		void UpdateProj();

		// Define camera space via LookAt parameters.
		void LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp);
		void LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up);

		// Get View/Proj matrices.
		XMMATRIX GetView() const;
		XMMATRIX GetProj() const;
		XMMATRIX GetViewProj() const;

		XMFLOAT4X4 GetView4x4f() const;
		XMFLOAT4X4 GetProj4x4f() const;

		// Strafe/Walk the camera a distance d.
		void Strafe(float d);
		void Walk(float d);

		// Rotate the camera.
		void Pitch(float angle);
		void RotateY(float angle);
		void Roll(float angle);
		
		void FocusTheta(float angle);
		void FocusPhi(float angle);
		void FocusRadius(float length);

		void Move2D(float up, float right);

		void ProcessMoving(float dx, float dy);

		// After modifying camera position/orientation, call to rebuild the view matrix.
		void UpdateViewMatrix();

		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		const XMMATRIX NDCToTextureSpace = XMMATRIX(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

	private:

		ECameraViewType m_viewType = CV_FirstPersonView;
		ECameraProjType m_projType = CP_PerspectiveProj;

		// Camera coordinate system with coordinates relative to world space.
		XMFLOAT3 m_pos = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 m_right = { 1.0f, 0.0f, 0.0f };	// x.
		XMFLOAT3 m_up = { 0.0f, 1.0f, 0.0f };		// y.
		XMFLOAT3 m_look = { 0.0f, 0.0f, 1.0f };		// z.

		// Cache frustum properties.
		float m_nearZ = 0.0f;
		float m_farZ = 0.0f;
		float m_width = 0.0f;
		float m_height = 0.0f;
		float m_aspectRatio = 0.0f;
		float m_fovY = 0.0f;
		float m_nearWindowHeight = 0.0f;
		float m_farWindowHeight = 0.0f;

		bool m_viewDirty = true;

		// Cache View/Proj matrices.
		XMFLOAT4X4 m_view = Math::Identity4x4();
		XMFLOAT4X4 m_proj = Math::Identity4x4();

		// Focus Point View Data.
		float m_theta = 1.5f*XM_PI;
		float m_phi = 1.5f*XM_PIDIV4;
		float m_radius = 25.0f;

		// Others.
		float m_offsetUp = 0.0f;
		float m_offsetRight = 0.0f;

		float m_move2DSpeed = 20.0f;
	};
}
