#include "ForwardPlusGPURenderer.h"

ForwardPlusGPU::ForwardPlusGPU()
{
	m_Shader = 0;
	m_depthPrePass = 0;
	m_frustumCS = 0;
}

ForwardPlusGPU::~ForwardPlusGPU()
{
}

bool ForwardPlusGPU::Initialize(ID3D11Device * device, ID3D11DeviceContext* deviceContext, HWND hwnd)
{
	bool result;

	// Create the light shader object.
	m_Shader = new LightShader;
	if (!m_Shader)
	{
		return false;
	}

	// Initialize the texture shader object.
	result = m_Shader->Initialize(device, hwnd);
	if (!result)
	{
		MessageBox(hwnd, L"Could not initialize the light shader object.", L"Error", MB_OK);
		return false;
	}

	m_depthPrePass = new DepthPass;
	if (!m_depthPrePass)
	{
		return false;
	}

	result = m_depthPrePass->Initialize(device, deviceContext, hwnd);
	if (!result)
	{
		MessageBox(hwnd, L"Could not initialize the depth pre-pass object.", L"Error", MB_OK);
		return false;
	}

	m_frustumCS = new FrustumComputeShader;
	if (!m_frustumCS)
	{
		return false;
	}

	result = m_frustumCS->Initialize(device, hwnd);
	if (!result)
	{
		MessageBox(hwnd, L"Could not initialize the frustum compute shader object.", L"Error", MB_OK);
		return false;
	}

	m_lightList.Random(15, 15, 4);

	return true;
}

void ForwardPlusGPU::Shutdown()
{
	// Release the color shader object.
	if (m_Shader)
	{
		m_Shader->Shutdown();
		delete m_Shader;
		m_Shader = 0;
	}

	if (m_depthPrePass)
	{
		m_depthPrePass->Shutdown();
		delete m_depthPrePass;
		m_depthPrePass = 0;
	}

	if (m_frustumCS)
	{
		m_frustumCS->Shutdown();
		delete m_frustumCS;
		m_frustumCS = 0;
	}
}

bool ForwardPlusGPU::Render(D3D * directX, Camera * camera, Model* model)
{
	XMMATRIX viewMatrix, projectionMatrix;
	bool result;

	camera->GetViewMatrix(viewMatrix);
	directX->GetProjectionMatrix(projectionMatrix);

	//Put the model vertex and index buffer on the graphics pipeline to prepare them for drawing
	model->Render(directX->GetDeviceContext());

	m_depthPrePass->Render(directX->GetDeviceContext(), model->GetIndexCount(), model->GetWorldMatrix(), viewMatrix, projectionMatrix);

	directX->Reset();

	//Render the model using the light shader
	result = m_Shader->Render(directX->GetDeviceContext(), model->GetIndexCount(), model->GetWorldMatrix(), viewMatrix, projectionMatrix,
									model->GetTexture(), camera->GetPosition(), &m_lightList);
	if (!result)
	{
		return false;
	}

	m_frustumCS->SetShaderParameters(directX->GetDeviceContext(), projectionMatrix);
	m_frustumCS->Dispatch(directX->GetDeviceContext(), 16, 16, 1);

	return true;
}
