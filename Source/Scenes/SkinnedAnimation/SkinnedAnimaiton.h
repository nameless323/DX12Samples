//
// Scene with skinned animation.
//

#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"
#include "../SSAO/SSAO.h"
#include "../../Common/RenderItem.h"
#include "SkinnedAnimFrameResource.h"
#include "../../../Core/Camera.h"
#include "../Shadowmapping/ShadowMap.h"
#include "../../../Core/M3dLoader.h"

namespace DX12Samples
{
class SkinnedAnimation : public Application
{
public:
    SkinnedAnimation(HINSTANCE hInstance);
    SkinnedAnimation(const SkinnedAnimation& rhs) = delete;
    SkinnedAnimation& operator=(const SkinnedAnimation& rhs) = delete;
    /**
     * \brief Scene initiallization including texture loading, loading all pipline etc.
     */
    bool Init() override;
    ~SkinnedAnimation() override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    int Run() override;

protected:
    /**
     * \brief Create render target and depth stencil heaps with additional descriptors for ssao.
     */
    void CreateRtvAndDsvDescriptorHeaps() override;
    /**
     * \brief Calls when window are resized to rebuild size dependent resources.
     */
    void OnResize() override;
    /**
     * \brief Update game logic.
     */
    void Update(const GameTimer& timer) override;
    /**
     * \brief Draw scene.
     */
    void Draw(const GameTimer& timer) override;
    /**
     * \brief Calls when mouse button down.
     */
    void OnMouseDown(WPARAM btnState, int x, int y) override;
    /**
     * \brief Calls when mouse button up.
     */
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    /**
     * \brief Calls when mouse moves.
     */
    void OnMouseMove(WPARAM btnState, int x, int y) override;
    /**
     * \brief Handle keyboard input.
     */
    void OnKeyboardInput(const GameTimer& timer);
    /**
     * \brief Animate materials e.g water.
     */
    void AnimateMaterials(const GameTimer& timer);
    /**
     * \brief Update objects constant buffers for current frame.
     */
    void UpdateObjectCBs(const GameTimer& timer);
    /**
     * \brief Update constan buffers for skinned meshes.
     */
    void UpdateSkinnedCBs(const GameTimer& timer);
    /**
     * \brief Update materials constant buffers for current frame.
     */
    void UpdateMaterialBuffer(const GameTimer& timer);
    /**
     * \brief Update transform of shadow casting light.
     */
    void UpdateShadowTransform(const GameTimer& timer);
    /**
     * \brief Update pass buffer.
     */
    void UpdateMainPassCB(const GameTimer& timer);
    /**
     * \brief Update shadow pass constant buffer.
     */
    void UpdateShadowPassCB(const GameTimer& timer);
    /**
     * \brief Update SSAO constant buffer.
     */
    void UpdateSSAOCB(const GameTimer& timer);
    /**
     * \brief Load scene texutres from dds.
     */
    void LoadTextures();
    /**
     * \brief Build scene main root signature.
     */
    void BuildRootSignature();
    /**
     * \brief Build root signature for SSAO.
     */
    void BuildSSAORootSignature();
    /**
     * \brief Build nesessary deccriptor heaps for scene.
     */
    void BuildDescriptorHeaps();
    /**
     * \brief Load shaders from hlsl files and construct input layouts.
     */
    void BuildShaderAndInputLayout();
    /**
     * \brief Build level geometry.
     */
    void BuildShapeGeometry();
    /**
     * \brief Load sinned model from file.
     */
    void LoadSkinnedModel();
    /**
     * \brief Build skull geometry.
     */
    void BuildSkullGeometry();
    /**
     * \brief Build pipline state objects.
     */
    void BuildPSOs();
    /**
     * \brief Build nessesary amount for frame resources.
     */
    void BuildFrameResources();
    /**
     * \brief Build materials for scene.
     */
    void BuildMaterials();
    /**
     * \brief Build scene objects.
     */
    void BuildRenderItems();
    /**
     * \brief Draw scene objects.
     */
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems);
    /**
     * \brief Draw scene to shadow map for shadow casting.
     */
    void DrawSceneToShadowMap();
    /**
     * \brief Draw normals and depth for SSAO pass.
     */
    void DrawNormalsAndDepth();
    /**
     * \brief Get CPU SRV for index.
     */
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int index) const;
    /**
     * \brief Get GPU SRV for index.
     */
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int index) const;
    /**
     * \brief Get CPU DSV for index.
     */
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int index) const;
    /**
     * \brief Get CPU RTV for index.
     */
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int index) const;
    /**
     * \brief Get static samplers array for use in Root Signature creation.
     */
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

private:
    std::vector<std::unique_ptr<SkinnedAnimFrameResource>> _frameResources;
    SkinnedAnimFrameResource* _currFrameResource = nullptr;
    int _currentFrameResourceIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> _ssaoRootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> _materials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _PSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;
    std::vector<D3D12_INPUT_ELEMENT_DESC> _skinnedInputLayout;

    std::vector<std::unique_ptr<RenderItem>> _allRenderItems;
    std::vector<RenderItem*> _renderItemLayer[(int)RenderItem::RenderLayer::Count];

    UINT _skyTexHeapIndex = 0;
    UINT _shadowMapHeapIndex = 0;
    UINT _ssaoHeapIndex = 0;
    UINT _ssaoAmbientMapIndex = 0;

    UINT _nullCubeSrvIndex = 0;
    UINT _nullTexSrvIndex1 = 0;
    UINT _nullTexSrvIndex2 = 0;

    CD3DX12_GPU_DESCRIPTOR_HANDLE _nullSrv;

    SkinnedAnimFrameResource::PassConstants _mainPassCB;
    SkinnedAnimFrameResource::PassConstants _shadowPassCB;

    UINT _skinnedSrvHeapStart = 0;
    std::string _skinnedModelFilename = "Models\\soldier.m3d";
    std::unique_ptr<SkinnedModelInstance> _skinnedModelInst;
    SkinnedData _skinnedInfo;
    std::vector<M3dLoader::Subset> _skinnedSubsets;
    std::vector<M3dLoader::M3dMaterial> _skinnedMats;
    std::vector<std::string> _skinnedTextureNames;

    Camera _camera;
    std::unique_ptr<ShadowMap> _shadowMap;
    std::unique_ptr<SSAO> _ssao;
    DirectX::BoundingSphere _sceneBounds;

    float _lightNearZ = 0.0f;
    float _lightFarZ = 0.0f;
    DirectX::XMFLOAT3 _lightPosW;
    DirectX::XMFLOAT4X4 _lightView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _lightProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _shadowTransform = MathHelper::Identity4x4();

    float _lightRotationAngle = 0.0f;
    DirectX::XMFLOAT3 _baseLightDirections[3] =
    {
        DirectX::XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
        DirectX::XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
        DirectX::XMFLOAT3(0.0f, -0.707f, -0.707f)
    };
    DirectX::XMFLOAT3 _rotatedLightDirections[3];

    POINT _lastMousePos;
};
}