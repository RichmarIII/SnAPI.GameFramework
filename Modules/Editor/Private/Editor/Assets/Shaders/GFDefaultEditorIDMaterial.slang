module GFDefaultEditorIDMaterial;

import GFEditorIDShadingModel;

public struct Material : IGFEditorIDHooks
{
    public static void VertexShader(inout GFEditorIDVSOut io)
    {
    }

    public static void FragmentShader(in GFEditorIDPSIn pin, inout GFEditorIDPSOut po)
    {
    }
};
