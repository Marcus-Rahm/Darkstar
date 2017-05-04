#include "FrustumComputeShader.h"

FrustumComputeShader::FrustumComputeShader()
{
	m_computeShader = 0;
	m_StoVParamBuffer = 0;
	m_dispatchBuffer = 0;
}

FrustumComputeShader::~FrustumComputeShader()
{
}

bool FrustumComputeShader::Initialize(ID3D11Device * device, HWND hwnd)
{
	bool result;

	result = InitializeShader(device, hwnd, L"../GraphicEngine/Shaders/frustum_cs.hlsl");
	if (!result)
	{
		return false;
	}

	m_dispatchParameters.numThreads = XMUINT3(std::ceil(SCREEN_WIDTH / (float)BLOCK_SIZE), std::ceil(SCREEN_HEIGHT / (float)BLOCK_SIZE), 1);
	m_dispatchParameters.numThreadGroups = XMUINT3(std::ceil(m_dispatchParameters.numThreads.x / (float)BLOCK_SIZE), std::ceil(m_dispatchParameters.numThreads.y / (float)BLOCK_SIZE), 1);
	
	CreateStructuredBuffer(device);

	D3D11_BUFFER_DESC dispatchBufferDesc;
	D3D11_BUFFER_DESC StoVBufferDesc;

	// Setup the description of the dynamic matrix constant buffer that is in the vertex shader.
	dispatchBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	dispatchBufferDesc.ByteWidth = sizeof(DispatchParams);
	dispatchBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	dispatchBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	dispatchBufferDesc.MiscFlags = 0;
	dispatchBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = device->CreateBuffer(&dispatchBufferDesc, NULL, &m_dispatchBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Setup the description of the dynamic matrix constant buffer that is in the vertex shader.
	StoVBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	StoVBufferDesc.ByteWidth = sizeof(ScreenToViewParams);
	StoVBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	StoVBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	StoVBufferDesc.MiscFlags = 0;
	StoVBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = device->CreateBuffer(&StoVBufferDesc, NULL, &m_StoVParamBuffer);
	if (FAILED(result))
	{
		return false;
	}

	return true;
}

bool FrustumComputeShader::SetShaderParameters(ID3D11DeviceContext * deviceContext, XMMATRIX projectionMatrix)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	DispatchParams* dataPtr;
	ScreenToViewParams* dataPtr2;

	//Lock the light constant buffer so it can be written to
	result = deviceContext->Map(m_dispatchBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		return false;
	}

	//Get a pointer to the data in the constant buffer
	dataPtr = (DispatchParams*)mappedResource.pData;

	dataPtr->numThreadGroups = m_dispatchParameters.numThreadGroups;
	dataPtr->numThreads = m_dispatchParameters.numThreads;

	//Unlock the constant buffer
	deviceContext->Unmap(m_dispatchBuffer, 0);

	deviceContext->CSSetConstantBuffers(0, 1, &m_dispatchBuffer);

	//Lock the light constant buffer so it can be written to
	result = deviceContext->Map(m_StoVParamBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		return false;
	}

	dataPtr2 = (ScreenToViewParams*)mappedResource.pData;

	XMFLOAT4X4 f4x4Proj, f4x4InvProj;
	XMStoreFloat4x4(&f4x4Proj, projectionMatrix);
	XMStoreFloat4x4(&f4x4InvProj, XMMatrixIdentity());
	f4x4InvProj._11 = 1.0f / f4x4Proj._11;
	f4x4InvProj._22 = 1.0f / f4x4Proj._22;
	f4x4InvProj._33 = 0.0f;
	f4x4InvProj._34 = 1.0f / f4x4Proj._43;
	f4x4InvProj._43 = 1.0f;
	f4x4InvProj._44 = -f4x4Proj._33 / f4x4Proj._43;
	XMMATRIX mInvProj = XMLoadFloat4x4(&f4x4InvProj);


	XMMATRIX invProjMatrix = XMMatrixInverse(nullptr, projectionMatrix);
	invProjMatrix = XMMatrixTranspose(invProjMatrix);

	dataPtr2->InverseProjectionMatrix = XMMatrixTranspose(mInvProj);
	dataPtr2->ScreenDimensions = XMFLOAT2(SCREEN_WIDTH, SCREEN_HEIGHT);

	//Unlock the constant buffer
	deviceContext->Unmap(m_StoVParamBuffer, 0);

	deviceContext->CSSetConstantBuffers(1, 1, &m_StoVParamBuffer);

	return true;
}

void FrustumComputeShader::Shutdown()
{
	ShutdownShader();
	m_structuredBuffer.Shutdown();
}

void FrustumComputeShader::Bind(ID3D11DeviceContext * deviceContext, UINT startSlot)
{
	deviceContext->CSSetShaderResources(startSlot, 1, m_structuredBuffer.GetResourceView());
}

void FrustumComputeShader::Unbind(ID3D11DeviceContext * deviceContext, UINT startSlot)
{
	ID3D11ShaderResourceView* resource = { nullptr };
	deviceContext->CSSetShaderResources(startSlot, 1, &resource);
}

void FrustumComputeShader::CreateStructuredBuffer(ID3D11Device * device)
{
	m_structuredBuffer.Initialize(m_dispatchParameters.numThreads.x * m_dispatchParameters.numThreads.y * m_dispatchParameters.numThreads.z, sizeof(FrustumComputeShader::Frustum), false, true, NULL, device);
	m_structuredBuffer.InitializeAccessView(device);
	m_structuredBuffer.InitializeResourceView(device);
}

void FrustumComputeShader::DestroyStructuredBuffer()
{
	m_structuredBuffer.Shutdown();
}

bool FrustumComputeShader::InitializeShader(ID3D11Device * device, HWND hwnd, WCHAR * filename)
{
	HRESULT result;
	ID3D10Blob* errorMessage;
	ID3D10Blob* computeShader;

	errorMessage = 0;
	computeShader = 0;

	result = D3DCompileFromFile(filename, NULL, NULL, "FrustumComputeShader", "cs_5_0", D3DCOMPILE_PREFER_FLOW_CONTROL | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
		&computeShader, &errorMessage);

	if (FAILED(result))
	{
		// If the shader failed to compile it should have writen something to the error message.
		if (errorMessage)
		{
			OutputShaderErrorMessage(errorMessage, hwnd, filename);
		}
		// If there was  nothing in the error message then it simply could not find the shader file itself.
		else
		{
			MessageBox(hwnd, filename, L"Missing Shader File", MB_OK);
		}

		return false;
	}

	result = device->CreateComputeShader(computeShader->GetBufferPointer(), computeShader->GetBufferSize(), NULL, &m_computeShader);
	if (FAILED(result))
	{
		return false;
	}

	computeShader->Release();
	computeShader = 0;

	return true;
}

void FrustumComputeShader::ShutdownShader()
{
	if (m_computeShader)
	{
		m_computeShader->Release();
		m_computeShader = 0;
	}

	if (m_dispatchBuffer)
	{
		m_dispatchBuffer->Release();
		m_dispatchBuffer = 0;
	}

	if (m_StoVParamBuffer)
	{
		m_StoVParamBuffer->Release();
		m_StoVParamBuffer = 0;
	}
}

void FrustumComputeShader::OutputShaderErrorMessage(ID3D10Blob * errorMessage, HWND hwnd, WCHAR * shaderFilename)
{
	char* compileErrors;
	unsigned long bufferSize, i;
	ofstream fout;


	// Get a pointer to the error message text buffer.
	compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Get the length of the message.
	bufferSize = errorMessage->GetBufferSize();

	// Open a file to write the error message to.
	fout.open("shader-error.txt");

	// Write out the error message.
	for (i = 0; i<bufferSize; i++)
	{
		fout << compileErrors[i];
	}

	// Close the file.
	fout.close();

	// Release the error message.
	errorMessage->Release();
	errorMessage = 0;

	// Pop a message up on the screen to notify the user to check the text file for compile errors.
	MessageBox(hwnd, L"Error compiling shader.  Check shader-error.txt for message.", shaderFilename, MB_OK);
}

void FrustumComputeShader::Dispatch(ID3D11DeviceContext* deviceContext, ID3D11Device * device, int x, int y, int z)
{

	deviceContext->CSSetShader(m_computeShader, NULL, 0);

	deviceContext->CSSetUnorderedAccessViews(0, 1, m_structuredBuffer.GetUnorderedAccessView(), 0);

	deviceContext->Dispatch(m_dispatchParameters.numThreadGroups.x, m_dispatchParameters.numThreadGroups.y, m_dispatchParameters.numThreadGroups.z);
}
