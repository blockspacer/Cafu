/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2013 Carsten Fuchs Software.

Cafu is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

Cafu is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Cafu. If not, see <http://www.gnu.org/licenses/>.

For support and more information about Cafu, visit us at <http://www.cafu.de>.
=================================================================================
*/

/**************/
/*** Shader ***/
/**************/

// Required for #include <GL/gl.h> with MS VC++.
#if defined(_WIN32) && defined(_MSC_VER)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

// This is required for cg.h to get the function calling conventions (Win32 import/export/lib) right.
#ifdef _WIN32
#undef WIN32    // VC++ 8 seems to predefine something here that is not an integer, and thus doesn't work with the "#if WIN32" expression in cgGL.h.
#define WIN32 1
#endif

#include <GL/gl.h>
#include <Cg/cg.h>
#include <Cg/cgGL.h>

#include "../../Common/OpenGLState.hpp"
#include "../RendererImpl.hpp"
#include "../RenderMaterial.hpp"
#include "../Shader.hpp"
#include "../TextureMapImpl.hpp"
#include "../../Mesh.hpp"
#include "_CommonCgHelpers.hpp"
#include "../../Common/OpenGLEx.hpp"


using namespace MatSys;


class Shader_L_Diff_Norm_Spec : public ShaderT
{
    private:

    CGprogram     VertexShader;
    CGparameter   VS_EyePos;
    CGparameter   VS_LightPos;
    CGparameter   VS_LightRadius;

    CGprogram     FragmentShader;
    CGparameter   FS_LightColorDiff;
    CGparameter   FS_LightColorSpec;

    unsigned long InitCounter;


    void Initialize()
    {
        VertexShader=UploadCgProgram(RendererImplT::GetInstance().GetCgContext(), CG_PROFILE_ARBVP1,
            // Assumptions:
            //   - The w-component of InPos is 1.0.
            //   - InNormal  has unit length.
            //   - InTangent has unit length.
            " void main(in float4   InPos            : POSITION,                                        \n"
            "           in float3   InNormal         : NORMAL,                                          \n"
            "           in float3   InTangent        : TANGENT,                                         \n"
            "           in float3   InBiNormal       : BINORMAL,                                        \n"
            "           in float2   InTexCoord       : TEXCOORD0,                                       \n"
            "          out float4   OutPos           : POSITION,                                        \n"
            "          out float2   OutTexCoord      : TEXCOORD0,                                       \n"
            "          out float3   OutLightVector   : TEXCOORD1,                                       \n"
            "          out float3   OutLightVectorA  : TEXCOORD2,                                       \n" // "Normalized" (wrt. LightRadius) light vector for attenuation calculations.
            "          out float3   OutHalfwayVector : TEXCOORD3,                                       \n"
            "      uniform float3   EyePos,                                                             \n"
            "      uniform float3   LightPos,                                                           \n"
            "      uniform float    LightRadius)                                                        \n"
            " {                                                                                         \n"
            "     const float3x3 RotMat=float3x3(InTangent,                                             \n"
            "                                    InBiNormal,                                            \n"
            "                                    InNormal);                                             \n"
            "                                                                                           \n"
            "     OutPos     =mul(glstate.matrix.mvp, InPos);                                           \n"
            "     OutTexCoord=InTexCoord;                                                               \n"
            "                                                                                           \n"
            "     const float3 LightVector=LightPos-InPos.xyz;                                          \n"
            "     const float  LightVecLen=length(LightVector);                                         \n"
            "                                                                                           \n"
            "     // Light vector, rotated from model space into tangent space.                         \n"
            "     OutLightVector=mul(RotMat, LightVector);                                              \n"
            "                                                                                           \n"
            "     // Light vector for attenuation computations.                                         \n"
            "     // Kept in model space, but normalized wrt. 'LightRadius'.                            \n"
            "     // OutLightAtten=saturate(1.0-length(LightVector)/LightRadius);                       \n" // linear, 1-d/r
            "     // OutLightAtten=saturate(2000.0/length(LightVector)-0.01);                           \n" // 1/d
            "     OutLightVectorA=LightVector/LightRadius;                                              \n" // Depending on the fragment shader, this will become 1-d/r or 1-(d/r)^2
            "                                                                                           \n"
            "     // Halfway vector, rotated from model space into tangent space.                       \n"
            "     // Normally, we would write something like                                            \n"
            "     //    const float3 EyeDir  =normalize(EyePos-InPos.xyz);                              \n"
            "     //    const float3 LightDir=LightVector/LightVecLen;                                  \n"
            "     //    OutHalfwayVector=mul(RotMat, EyeDir+LightDir);                                  \n"
            "     // but that makes the HalfwayVector, when interpolated across the polygon,            \n"
            "     // become so short that the normalization cube map look-up returns undefined results! \n"
            "     // Therefore, multiply the entire vector with LightVecLen.                            \n"
            "     OutHalfwayVector=mul(RotMat, normalize(EyePos-InPos.xyz)*LightVecLen + LightVector);  \n"
            " }                                                                                         \n");

        VS_EyePos     =cgGetNamedParameter(VertexShader, "EyePos"     );
        VS_LightPos   =cgGetNamedParameter(VertexShader, "LightPos"   );
        VS_LightRadius=cgGetNamedParameter(VertexShader, "LightRadius");


        FragmentShader=UploadCgProgram(RendererImplT::GetInstance().GetCgContext(), CG_PROFILE_ARBFP1,
            " void main(in half2       InTexCoord                  : TEXCOORD0,                                 \n"
            "           in half3       InLightVector               : TEXCOORD1,                                 \n"
            "           in half3       InLightVectorA              : TEXCOORD2,                                 \n"
            "           in half3       InHalfwayVector             : TEXCOORD3,                                 \n"
            "          out half4       OutColor                    : COLOR,                                     \n"
            "      uniform half4       LightColor_diff,                                                         \n"
            "      uniform half4       LightColor_spec,                                                         \n"
            "      uniform sampler2D   DiffuseMapSampler           : TEXUNIT0,                                  \n"
            "      uniform sampler2D   SpecularMapSampler          : TEXUNIT1,                                  \n"
            "      uniform sampler2D   NormalMapSampler            : TEXUNIT2,                                  \n"
            "      uniform samplerCUBE NormalizeCubeForLightVector : TEXUNIT3)                                  \n"
            " {                                                                                                 \n"
            "     const half3 LightDir  =2.0*(texCUBE(NormalizeCubeForLightVector, InLightVector  ).xyz-0.5);   \n"
            "     const half3 HalfwayDir=2.0*(texCUBE(NormalizeCubeForLightVector, InHalfwayVector).xyz-0.5);   \n"
            "     const half3 Normal    =2.0*(tex2D(NormalMapSampler, InTexCoord).xyz-0.5);                     \n"
            "                                                                                                   \n"
            "     const half4 DiffuseC  =tex2D(DiffuseMapSampler,  InTexCoord);                                 \n"
            "     const half4 SpecularC =tex2D(SpecularMapSampler, InTexCoord);                                 \n"
            "                                                                                                   \n"
            "     // Compute the attenuation as 1-(d/r)^2.                                                      \n"
            "     // Note that it would actually be nice to remove the ^2 by some 1D texture lookup that        \n"
            "     // returns the square root, but the strict binding of texture units to texture samplers       \n"
            "     // in this profile makes that impossible.                                                     \n"
            "     // IMPORTANT: Note that 'InLightVector' and 'InLightVectorA' are ENTIRELY DIFFERENT!          \n"
            "     // Only 'InLightVectorA' is good for attenuation computations (model space), while            \n"
            "     // 'InLightVector' (local tangent space) takes SmoothGroups into account!!!                   \n"
            "     // So they must never be collapsed, even if the profile admitted that!                        \n"
            "     const half Atten=saturate(1.0-dot(InLightVectorA, InLightVectorA));                           \n" // 1-(d/r)^2
         // "     const half Atten=1.0-smoothstep(0, LightRadius, LightDist);                                   \n" // slow!
         // "     const half Atten=saturate(1.0-length(InLightVectorA));                                        \n" // linear, 1-d/r
         // "     const half Atten=saturate(2000.0/LightDist-0.01);                                             \n" // 1/d
            "                                                                                                   \n"
            "     const half  diff    =LightDir.z>0.0 ? saturate(dot(Normal, LightDir)) : 0.0;                  \n" // See "The Cg Tutorial", p. 230.
         // "     const half  diff    =           saturate(dot(Normal, LightDir));                              \n"
            "     const half  spec    =diff>0.0 ? saturate(dot(Normal, HalfwayDir)) : 0.0;                      \n"
         // "     const half4 lightval=lit(dot(Normal, LightDir), dot(Normal, HalfwayDir), 32);                 \n"
            "                                                                                                   \n"
            "     OutColor=Atten*(diff*DiffuseC*LightColor_diff + pow(spec, 32.0)*SpecularC*LightColor_spec);   \n"
         // "     OutColor=Atten*(lightval.y*DiffuseC*LightColor_diff + lightval.z*SpecularC*LightColor_spec);  \n" // Contrary to Cg documentation, the lit() approach is slower than the "manual" approach.
            " }                                                                                                 \n");

        FS_LightColorDiff=cgGetNamedParameter(FragmentShader, "LightColor_diff");
        FS_LightColorSpec=cgGetNamedParameter(FragmentShader, "LightColor_spec");
    }


    public:

    Shader_L_Diff_Norm_Spec()
    {
        VertexShader     =NULL;
        VS_EyePos        =NULL;
        VS_LightPos      =NULL;
        VS_LightRadius   =NULL;

        FragmentShader   =NULL;
        FS_LightColorDiff=NULL;
        FS_LightColorSpec=NULL;

        InitCounter=0;
    }

    const std::string& GetName() const
    {
        static const std::string Name="L_Diff_Norm_Spec";

        return Name;
    }

    char CanHandleAmbient(const MaterialT& /*Material*/) const
    {
        return 0;
    }

    char CanHandleLighting(const MaterialT& Material) const
    {
        if (Material.NoDraw    ) return 0;
        if (Material.NoDynLight) return 0;

        if (Material.DiffMapComp.IsEmpty()) return 0;
        if (Material.NormMapComp.IsEmpty()) return 0;
        if (Material.SpecMapComp.IsEmpty()) return 0;

        return 255;
    }

    bool CanHandleStencilShadowVolumes() const
    {
        return false;
    }

    void Activate()
    {
        if (InitCounter<RendererImplT::GetInstance().GetInitCounter())
        {
            Initialize();
            InitCounter=RendererImplT::GetInstance().GetInitCounter();
        }

        // These are very expensive calls!
        cgGLBindProgram(VertexShader);
        cgGLBindProgram(FragmentShader);
    }

    void Deactivate()
    {
    }

    bool NeedsNormals() const
    {
        return true;
    }

    bool NeedsTangentSpace() const
    {
        return true;
    }

    bool NeedsXYAttrib() const
    {
        return false;
    }

    void RenderMesh(const MeshT& Mesh)
    {
        const RendererImplT& Renderer   =RendererImplT::GetInstance();
        RenderMaterialT*     RM         =Renderer.GetCurrentRenderMaterial();
        const MaterialT&     Material   =*(RM->Material);
        OpenGLStateT*        OpenGLState=OpenGLStateT::GetInstance();

        const float*         EyePos            =Renderer.GetCurrentEyePosition();
        const float*         LightPos          =Renderer.GetCurrentLightSourcePosition();
        float                LightRadius       =Renderer.GetCurrentLightSourceRadius();
        const float*         LightDiffuseColor =Renderer.GetCurrentLightSourceDiffuseColor();
        const float*         LightSpecularColor=Renderer.GetCurrentLightSourceSpecularColor();

        if (InitCounter<Renderer.GetInitCounter())
        {
            Initialize();
            InitCounter=Renderer.GetInitCounter();
        }


        // Render the lit diffuse map.
        if (!Material.TwoSided)
        {
            OpenGLState->Enable(GL_CULL_FACE);
            OpenGLState->FrontFace(OpenGLStateT::WindingToOpenGL[Mesh.Winding]);
            OpenGLState->CullFace(GL_BACK);
        }
        else OpenGLState->Disable(GL_CULL_FACE);

        if (Material.DepthOffset!=0.0f)
        {
            OpenGLState->Enable(OpenGLStateT::PolygonModeToOpenGL_Offset[Material.PolygonMode]);
            OpenGLState->PolygonOffset(Material.DepthOffset, Material.DepthOffset);
        }
        else OpenGLState->Disable(OpenGLStateT::PolygonModeToOpenGL_Offset[Material.PolygonMode]);

        OpenGLState->PolygonMode(OpenGLStateT::PolygonModeToOpenGL[Material.PolygonMode]);
        OpenGLState->Disable(GL_ALPHA_TEST);
        OpenGLState->Enable(GL_BLEND);
        OpenGLState->BlendFunc(GL_ONE, GL_ONE);
        OpenGLState->DepthFunc(GL_EQUAL);   // Using GL_LEQUAL here yields problems with alpha-tested materials...  :(
        OpenGLState->ColorMask(Material.LightMask[0], Material.LightMask[1], Material.LightMask[2], Material.LightMask[3]);
        OpenGLState->DepthMask(Material.LightMask[4]);
     // OpenGLState->EnableOrDisable(GL_STENCIL_TEST);      // This is INTENTIONALLY not decided here! We take whatever was set by previous shaders.
        if (cf::GL_EXT_stencil_two_side_AVAIL)
        {
            OpenGLState->Disable(GL_STENCIL_TEST_TWO_SIDE_EXT);
            OpenGLState->ActiveStencilFace(GL_FRONT);
        }
        OpenGLState->StencilFunc(GL_EQUAL, 0, ~0);
     // OpenGLState->StencilOp(GL_KEEP, GL_KEEP, GL_INCR);  // Uh, this works only if only ONE rendering pass follows. We often have TWO passes (diff+spec), though...
        OpenGLState->StencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        OpenGLState->ActiveTextureUnit(GL_TEXTURE0_ARB);
        OpenGLState->Enable(GL_TEXTURE_2D);
        OpenGLState->BindTexture(GL_TEXTURE_2D, RM->DiffTexMap->GetOpenGLObject());

        OpenGLState->ActiveTextureUnit(GL_TEXTURE1_ARB);
        OpenGLState->Enable(GL_TEXTURE_2D);
        OpenGLState->BindTexture(GL_TEXTURE_2D, RM->SpecTexMap->GetOpenGLObject());

        OpenGLState->ActiveTextureUnit(GL_TEXTURE2_ARB);
        OpenGLState->Enable(GL_TEXTURE_2D);
        OpenGLState->BindTexture(GL_TEXTURE_2D, RM->NormTexMap->GetOpenGLObject());

        OpenGLState->ActiveTextureUnit(GL_TEXTURE3_ARB);
        OpenGLState->Enable(GL_TEXTURE_CUBE_MAP_ARB);
        OpenGLState->BindTexture(GL_TEXTURE_CUBE_MAP_ARB, Renderer.GetNormalizationCubeMap()->GetOpenGLObject());


        // The cgGLSetParameter*() functions are very slow, so cache their parameters in order to call them as little as possible.
        static float EyePosCache[3]={ 0.0, 0.0, 0.0 };
        if (EyePos[0]!=EyePosCache[0] || EyePos[1]!=EyePosCache[1] || EyePos[2]!=EyePosCache[2])
        {
            cgGLSetParameter3fv(VS_EyePos, EyePos);
            EyePosCache[0]=EyePos[0];
            EyePosCache[1]=EyePos[1];
            EyePosCache[2]=EyePos[2];
        }

        static float LightPosCache[3]={ 0.0, 0.0, 0.0 };
        if (LightPos[0]!=LightPosCache[0] || LightPos[1]!=LightPosCache[1] || LightPos[2]!=LightPosCache[2])
        {
            cgGLSetParameter3fv(VS_LightPos, LightPos);
            LightPosCache[0]=LightPos[0];
            LightPosCache[1]=LightPos[1];
            LightPosCache[2]=LightPos[2];
        }

        static float LightRadiusCache=0.0;
        if (LightRadius!=LightRadiusCache)
        {
            cgGLSetParameter1f (VS_LightRadius, LightRadius);
            LightRadiusCache=LightRadius;
        }

        static float LightDiffColorCache[4]={ -1.0, -1.0, -1.0, 0.0 };
        if (LightDiffuseColor[0]!=LightDiffColorCache[0] || LightDiffuseColor[1]!=LightDiffColorCache[1] || LightDiffuseColor[2]!=LightDiffColorCache[2])
        {
            LightDiffColorCache[0]=LightDiffuseColor[0];
            LightDiffColorCache[1]=LightDiffuseColor[1];
            LightDiffColorCache[2]=LightDiffuseColor[2];
            cgGLSetParameter4fv(FS_LightColorDiff, LightDiffColorCache);
        }

        static float LightSpecColorCache[4]={ -1.0, -1.0, -1.0, 0.0 };
        if (LightSpecularColor[0]!=LightSpecColorCache[0] || LightSpecularColor[1]!=LightSpecColorCache[1] || LightSpecularColor[2]!=LightSpecColorCache[2])
        {
            LightSpecColorCache[0]=LightSpecularColor[0];
            LightSpecColorCache[1]=LightSpecularColor[1];
            LightSpecColorCache[2]=LightSpecularColor[2];
            cgGLSetParameter4fv(FS_LightColorSpec, LightSpecColorCache);
        }


        glBegin(OpenGLStateT::MeshToOpenGLType[Mesh.Type]);
            for (unsigned long VertexNr=0; VertexNr<Mesh.Vertices.Size(); VertexNr++)
            {
                glNormal3fv(Mesh.Vertices[VertexNr].Normal);                                // Normal
                cf::glMultiTexCoord3fvARB(GL_TEXTURE6_ARB, Mesh.Vertices[VertexNr].Tangent);    // Tangent
                cf::glMultiTexCoord3fvARB(GL_TEXTURE7_ARB, Mesh.Vertices[VertexNr].BiNormal);   // BiNormal

                cf::glMultiTexCoord2fvARB(GL_TEXTURE0_ARB, Mesh.Vertices[VertexNr].TextureCoord);
                glVertex3dv(Mesh.Vertices[VertexNr].Origin);
            }
        glEnd();
    }
};


// The constructor of the base class automatically registers this shader with the ShaderRepository.
static Shader_L_Diff_Norm_Spec MyShader;
