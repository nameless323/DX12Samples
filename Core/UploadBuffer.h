//
// Simple class which helps to create constant buffers.
//

#pragma once

#include "D3DUtil.h"

template<typename T>
class UploadBuffer
{
public:
    /**
     * \brief Creates upload buffer.
     * \param device to create resource.
     * \param elementCount how much elements of type T will be in buffer.
     * \param isConstantBuffer if this buffer are constant buffer then size of each element must be multiple of 256 bytes.
     */
    UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : _isConstantBuffer(isConstantBuffer)
    {
        _elementByteSize = sizeof(T);
        if (isConstantBuffer)
            _elementByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(T));
        ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(_elementByteSize * elementCount),
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_uploadBuffer)));
        ThrowIfFailed(_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&_mappedData)))
    }

    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
    ~UploadBuffer()
    {
        if (_uploadBuffer != nullptr)
            _uploadBuffer->Unmap(0, nullptr);
        _mappedData = nullptr;
    }
    /**
     * \brief Get constant buffer resource.
     */
    ID3D12Resource* Resource() const
    {
        return _uploadBuffer.Get();
    }
    /**
     * \brief Overwrite data at constant buffer for element with index elementIndex to data.
     */
    void CopyData(int elementIndex, const T& data)
    {
        memcpy(&_mappedData[elementIndex*_elementByteSize], &data, sizeof(T));
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> _uploadBuffer;
    BYTE* _mappedData = nullptr;

    UINT _elementByteSize = 0;
    bool _isConstantBuffer = false;
};
