module GFEditorIDShadingModel;

import IShadingModel;
import Attributes;

#ifndef WITH_INSTANCING
#define WITH_INSTANCING 0
#endif

struct GFEditorID_PerObject
{
    float4x4 model;
    float4x4 prevModel;
    uint instanceOffset;
    uint objectID;
    uint flags;
    uint _pad;
};
[[vk::push_constant]]
ConstantBuffer<GFEditorID_PerObject> PerObject;

struct GFEditorID_PerFrame
{
    float4x4 view;
    float4x4 proj;
    float4x4 prevViewProj;
};
// Keep camera UBO in set 0 for editor-only materials.
// These materials do not use a set-0 material texture block like main GBuffer materials.
[[vk::binding(0, 0)]]
cbuffer GFEditorID_CameraData
{
    GFEditorID_PerFrame PerFrame;
};

public struct GFEditorIDVSAttribs : IShadingInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    uint4 boneIndices : TEXCOORD1;
    float4 boneWeights : TEXCOORD2;
#if WITH_INSTANCING
    float4x4 instanceModel : TEXCOORD3;
#endif
};

public struct GFEditorIDVSIn : IShadingInput
{
    public float4 clipPos;
};

public struct GFEditorIDVSOut : IShadingOutput
{
    public float4 clipPos : SV_Position;
};

public struct GFEditorIDPSIn : IShadingInput
{
};

public struct GFEditorIDPSOut : IShadingOutput
{
    [ETextureFormat("R32_UInt")]
    public uint EditorID_Value : SV_Target0;
};

public interface IGFEditorIDHooks
{
    static void VertexShader(inout GFEditorIDVSOut io);
    static void FragmentShader(in GFEditorIDPSIn pin, inout GFEditorIDPSOut po);
}

public struct GFEditorIDShadingModel<M : IGFEditorIDHooks> : IShadingModel
{
    public typealias VSIn = GFEditorIDVSIn;
    public typealias VSOut = GFEditorIDVSOut;
    public typealias PSIn = GFEditorIDPSIn;
    public typealias PSOut = GFEditorIDPSOut;

    static VSOut ShadeVS(VSIn vin)
    {
        GFEditorIDVSOut o;
        o.clipPos = vin.clipPos;
        M::VertexShader(o);
        return o;
    }

    static PSOut ShadePS(PSIn pin)
    {
        GFEditorIDPSOut out;
        out.EditorID_Value = PerObject.objectID;
        M::FragmentShader(pin, out);
        return out;
    }
};

[shader("vertex")]
public GFEditorIDVSOut VSMain<M : IGFEditorIDHooks>(GFEditorIDVSAttribs vin)
{
    float4x4 model = PerObject.model;
#if WITH_INSTANCING
    model = vin.instanceModel;
#endif

    float4 worldPos = mul(model, float4(vin.position, 1.0));

    GFEditorIDVSIn inVS;
    inVS.clipPos = mul(PerFrame.proj, mul(PerFrame.view, worldPos));
    if ((PerObject.flags & 0x1u) != 0u)
    {
        // Keep gizmos selectable through scene geometry while preserving
        // relative depth ordering between gizmo pieces.
        const float depth = saturate(inVS.clipPos.z / max(inVS.clipPos.w, 1.0e-6f));
        const float biasedDepth = 0.995f + (depth * 0.005f);
        inVS.clipPos.z = biasedDepth * inVS.clipPos.w;
    }

    return GFEditorIDShadingModel<M>::ShadeVS(inVS);
}

[shader("fragment")]
public GFEditorIDPSOut PSMain<M : IGFEditorIDHooks>(GFEditorIDVSOut pin)
{
    GFEditorIDPSIn inPS;
    return GFEditorIDShadingModel<M>::ShadePS(inPS);
}
