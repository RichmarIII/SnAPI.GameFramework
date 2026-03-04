module GFDefaultEditorOverlayMaterial;

import GFEditorOverlayShadingModel;

public struct Material : IGFEditorOverlayHooks
{
    public static void VertexShader(inout GFEditorOverlayVSOut io)
    {
    }

    public static void FragmentShader(in GFEditorOverlayPSIn pin, inout GFEditorOverlayPSOut po)
    {
    }
};
