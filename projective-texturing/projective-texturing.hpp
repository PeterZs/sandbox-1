#include "index.hpp"
#include "gl-gizmo.hpp"

struct gl_material_projector
{
    float4x4 modelViewMatrix;

    GlShader shader;

    std::shared_ptr<GlTexture2D> cookieTexture;
    std::shared_ptr<GlTexture2D> gradientTexture;

    float4x4 get_view_projection_matrix(bool isOrthographic = false)
    {
        if (isOrthographic)
        {
            const float halfSize = 1.0 * 0.5f;
            return mul(make_orthographic_matrix(-halfSize, halfSize, -halfSize, halfSize, -halfSize, halfSize), modelViewMatrix);
        }
        return mul(make_projection_matrix(to_radians(45.f), 1.0f, 0.1f, 16.f), modelViewMatrix);
    }

    // Transforms a position into projective texture space.
    // This matrix combines the light view, projection and bias matrices.
    float4x4 get_projector_matrix(bool isOrthographic = false)
    {
        // Bias matrix is a constant.
        // It performs a linear transformation to go from the [�1, 1]
        // range to the [0, 1] range. Having the coordinates in the [0, 1]
        // range is necessary for the values to be used as texture coordinates.
        const float4x4 biasMatrix = {
            { 0.5f,  0.0f,  0.0f,  0.0f },
            { 0.0f,  0.5f,  0.0f,  0.0f },
            { 0.0f,  0.0f,  0.5f,  0.0f },
            { 0.5f,  0.5f,  0.5f,  1.0f }
        };

        return mul(biasMatrix, get_view_projection_matrix(false));
    }
};

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };
    std::unique_ptr<gui::imgui_wrapper> igm;
    GlGpuTimer gpuTimer;
    std::unique_ptr<GlGizmo> gizmo;

    gl_material_projector projector;

    float elapsedTime{ 0 };

    GlShader normalDebug;

    GlMesh terrainMesh;

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};