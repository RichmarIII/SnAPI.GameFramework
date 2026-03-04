module GFEditorCompositeMaterial;

import IShadingModel;
import PostProcessShadingModel;

// Input texture bindings - names must match outputs from previous passes
[[vk::binding(0, 0)]] Sampler2D TM_Out;              // ToneMap output
[[vk::binding(1, 0)]] Sampler2D UI_Out;              // UI output
[[vk::binding(2, 0)]] Sampler2D EditorOverlay_Color; // Editor overlay output

// Editor composite material - alpha blends UI and editor overlay on top of tone-mapped image.
public struct Material : IPostProcessHooks
{
    public struct PSOut : IShadingOutput
    {
        public float4 Composite_Out : SV_Target0;
    };

    public static float4 Process(float2 uv)
    {
        const float4 toneMapped = TM_Out.Sample(uv);
        const float4 ui = UI_Out.Sample(uv);
        const float4 overlay = EditorOverlay_Color.Sample(uv);

        const float3 sceneWithUi = toneMapped.rgb * (1.0f - ui.a) + ui.rgb * ui.a;
        const float overlayAlpha = saturate(overlay.a);
        const float3 tinted = sceneWithUi + ((overlay.rgb - sceneWithUi) * (overlayAlpha * 0.45f));
        const float3 screened = 1.0f - ((1.0f - sceneWithUi) * (1.0f - (overlay.rgb * overlayAlpha)));
        const float3 finalColor = saturate(lerp(tinted, screened, 0.35f));

        return float4(finalColor, 1.0f);
    }

    public static PSOut MakeOutput(float4 color)
    {
        PSOut o;
        o.Composite_Out = color;
        return o;
    }
};
