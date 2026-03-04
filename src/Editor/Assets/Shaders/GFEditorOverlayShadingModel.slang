module GFEditorOverlayShadingModel;

import IShadingModel;
import Attributes;

#ifndef WITH_INSTANCING
#define WITH_INSTANCING 0
#endif

struct GFEditorOverlay_PerObject
{
    float4x4 model;
    float4x4 prevModel;
    uint instanceOffset;
    uint axisTag;
    uint _pad[2];
};
[[vk::push_constant]]
ConstantBuffer<GFEditorOverlay_PerObject> PerObject;

struct GFEditorOverlay_PerFrame
{
    float4x4 view;
    float4x4 proj;
    float4x4 prevViewProj;
};
// Keep camera UBO in set 0 for editor-only materials.
// These materials do not use a set-0 material texture block like main GBuffer materials.
[[vk::binding(0, 0)]]
cbuffer GFEditorOverlay_CameraData
{
    GFEditorOverlay_PerFrame PerFrame;
};
[[vk::binding(1, 0)]]
Sampler2D GBuffer_Depth;

public struct GFEditorOverlayVSAttribs : IShadingInput
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

public struct GFEditorOverlayVSIn : IShadingInput
{
    public float4 clipPos;
    public float3 normalWS;
};

public struct GFEditorOverlayVSOut : IShadingOutput
{
    public float4 clipPos : SV_Position;
    public float3 normalWS : TEXCOORD0;
};

public struct GFEditorOverlayPSIn : IShadingInput
{
    public float3 normalWS;
    public float4 fragPos;
};

public struct GFEditorOverlayPSOut : IShadingOutput
{
    public float4 Overlay_Out : SV_Target0;
};

public interface IGFEditorOverlayHooks
{
    static void VertexShader(inout GFEditorOverlayVSOut io);
    static void FragmentShader(in GFEditorOverlayPSIn pin, inout GFEditorOverlayPSOut po);
}

public struct GFEditorOverlayShadingModel<M : IGFEditorOverlayHooks> : IShadingModel
{
    public typealias VSIn = GFEditorOverlayVSIn;
    public typealias VSOut = GFEditorOverlayVSOut;
    public typealias PSIn = GFEditorOverlayPSIn;
    public typealias PSOut = GFEditorOverlayPSOut;

    static VSOut ShadeVS(VSIn vin)
    {
        GFEditorOverlayVSOut o;
        o.clipPos = vin.clipPos;
        o.normalWS = normalize(vin.normalWS);
        M::VertexShader(o);
        return o;
    }

    static PSOut ShadePS(PSIn pin)
    {
        GFEditorOverlayPSOut out;
        float3 baseColor = float3(1.0, 0.67, 0.0);
        float alpha = 0.35;

        // Axis-tag palette:
        // 0 = regular selection overlay, 1 = X, 2 = Y, 3 = Z.
        if (PerObject.axisTag == 1u)
        {
            baseColor = float3(0.95, 0.32, 0.29);
            alpha = 0.78;
        }
        else if (PerObject.axisTag == 2u)
        {
            baseColor = float3(0.32, 0.90, 0.40);
            alpha = 0.78;
        }
        else if (PerObject.axisTag == 3u)
        {
            baseColor = float3(0.30, 0.56, 0.98);
            alpha = 0.78;
        }

        const float3 n = normalize(pin.normalWS);
        const float normalSheen = 0.72 + 0.28 * saturate(abs(n.y) * 0.45 + abs(n.z) * 0.55);
        const float edgeHighlight = 0.04 + 0.10 * saturate(1.0 - abs(n.z));
        const float3 shadedColor = (baseColor * normalSheen) + edgeHighlight;

        const bool IsGizmo = (PerObject.axisTag != 0u);
        if (!IsGizmo)
        {
            uint Width = 0u;
            uint Height = 0u;
            GBuffer_Depth.GetDimensions(Width, Height);
            if (Width > 0u && Height > 0u)
            {
                const float2 uv = saturate(pin.fragPos.xy / float2((float)Width, (float)Height));
                const float sceneDepth = GBuffer_Depth.Sample(uv).r;
                const float fragmentDepth = saturate(pin.fragPos.z);
                if (sceneDepth > 0.0f)
                {
                    const float depthEpsilon = 1.0e-5f;
                    if ((fragmentDepth + depthEpsilon) < sceneDepth)
                    {
                        discard;
                    }
                }
            }
        }

        out.Overlay_Out = float4(saturate(shadedColor), alpha);
        M::FragmentShader(pin, out);
        return out;
    }
};

[shader("vertex")]
public GFEditorOverlayVSOut VSMain<M : IGFEditorOverlayHooks>(GFEditorOverlayVSAttribs vin)
{
    float4x4 model = PerObject.model;
#if WITH_INSTANCING
    model = vin.instanceModel;
#endif

    float4 worldPos = mul(model, float4(vin.position, 1.0));
    const float3 normalWS = normalize(mul((float3x3)model, vin.normal));

    GFEditorOverlayVSIn inVS;
    inVS.clipPos = mul(PerFrame.proj, mul(PerFrame.view, worldPos));
    if (PerObject.axisTag != 0u)
    {
        // Render gizmos on top of scene geometry while preserving depth
        // ordering within gizmo primitives.
        const float depth = saturate(inVS.clipPos.z / max(inVS.clipPos.w, 1.0e-6f));
        const float biasedDepth = 0.995f + (depth * 0.005f);
        inVS.clipPos.z = biasedDepth * inVS.clipPos.w;
    }
    inVS.normalWS = normalWS;

    return GFEditorOverlayShadingModel<M>::ShadeVS(inVS);
}

[shader("fragment")]
public GFEditorOverlayPSOut PSMain<M : IGFEditorOverlayHooks>(GFEditorOverlayVSOut pin)
{
    GFEditorOverlayPSIn inPS;
    inPS.normalWS = pin.normalWS;
    inPS.fragPos = pin.clipPos;
    return GFEditorOverlayShadingModel<M>::ShadePS(inPS);
}
