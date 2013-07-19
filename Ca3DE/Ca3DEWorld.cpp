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

#include "Ca3DEWorld.hpp"
#include "EngineEntity.hpp"
#include "ClipSys/ClipWorld.hpp"
#include "ClipSys/CollisionModel_static.hpp"
#include "ClipSys/TraceResult.hpp"
#include "ClipSys/TraceSolid.hpp"
#include "GameSys/World.hpp"
#include "MaterialSystem/Material.hpp"
#include "Models/ModelManager.hpp"
#include "../Common/CompGameEntity.hpp"
#include "../Common/WorldMan.hpp"
#include "SceneGraph/BspTreeNode.hpp"
#include "String.hpp"


#if defined(_WIN32) && defined(_MSC_VER)
// Turn off warning 4355: "'this' : wird in Initialisierungslisten fuer Basisklasse verwendet".
#pragma warning(disable:4355)
#endif


static WorldManT WorldMan;


Ca3DEWorldT::Ca3DEWorldT(cf::GameSys::GameInfoI* GameInfo, cf::GameSys::GameI* Game, const char* FileName, ModelManagerT& ModelMan, cf::GuiSys::GuiResourcesT& GuiRes, bool InitForGraphics, WorldT::ProgressFunctionT ProgressFunction) /*throw (WorldT::LoadErrorT)*/
    : m_Game(Game),
      m_World(WorldMan.LoadWorld(FileName, ModelMan, GuiRes, InitForGraphics, ProgressFunction)),
      m_ScriptWorld(NULL),
      m_ClipWorld(new cf::ClipSys::ClipWorldT(m_World->m_StaticEntityData[0]->m_CollModel)),
      m_PhysicsWorld(m_World->m_StaticEntityData[0]->m_CollModel),
      m_ScriptState(GameInfo, m_Game),
      m_EngineEntities(),
      m_ModelMan(ModelMan),
      m_GuiRes(GuiRes)
{
    try
    {
        std::string ScriptName = cf::String::StripExt(FileName) + ".cent";
        ScriptName = cf::String::Replace(ScriptName, "/Worlds/", "/Maps/");
        ScriptName = cf::String::Replace(ScriptName, "\\Worlds\\", "\\Maps\\");

        m_ScriptWorld = new cf::GameSys::WorldT(
            ScriptName,
            ModelMan,
            GuiRes,
            0 /*cf::GameSys::WorldT::InitFlag_InMapEditor*/);
    }
    catch (const cf::GameSys::WorldT::InitErrorT& IE)
    {
        throw WorldT::LoadErrorT(IE.what());
    }

    ArrayT< IntrusivePtrT<cf::GameSys::EntityT> > AllEnts;
    m_ScriptWorld->GetRootEntity()->GetAll(AllEnts);

    for (unsigned int EntNr = 0; EntNr < AllEnts.Size(); EntNr++)
        AllEnts[EntNr]->SetApp(
            new CompGameEntityT(EntNr < m_World->m_StaticEntityData.Size() ? m_World->m_StaticEntityData[EntNr] : NULL));
}


Ca3DEWorldT::~Ca3DEWorldT()
{
    // Note that the engine entities must be destructed *before* the ClipWorld,
    // so that they properly remove their clip model from the clip world on destruction.
    for (unsigned long EntityNr=0; EntityNr<m_EngineEntities.Size(); EntityNr++)
        delete m_EngineEntities[EntityNr];

    m_EngineEntities.Clear();

    // All entities should have been deleted by now, and their dtors should have removed their Lua associated instances.
    // m_ScriptState.PrintGlobalVars();
    // assert(!m_ScriptState.HasEntityInstances());
    // lua_gc(m_ScriptState.GetScriptState().GetLuaState(), LUA_GCCOLLECT, 0);

    // delete m_PhysicsWorld;
    // m_PhysicsWorld = NULL;

    delete m_ClipWorld;
    m_ClipWorld = NULL;

    delete m_ScriptWorld;
    m_ScriptWorld = NULL;

    if (m_World)
    {
        WorldMan.FreeWorld(m_World);
        m_World = NULL;
    }
}


cf::GameSys::GameI* Ca3DEWorldT::GetGame()
{
    return m_Game;
}


cf::ClipSys::ClipWorldT& Ca3DEWorldT::GetClipWorld()
{
    return *m_ClipWorld;
}


PhysicsWorldT& Ca3DEWorldT::GetPhysicsWorld()
{
    return m_PhysicsWorld;
}


cf::UniScriptStateT& Ca3DEWorldT::GetScriptState()
{
    return m_ScriptState.GetScriptState();
}


Vector3fT Ca3DEWorldT::GetAmbientLightColorFromBB(const BoundingBox3T<double>& Dimensions, const VectorT& Origin) const
{
    const Vector3dT BBCenter = scale(Dimensions.Min+Dimensions.Max, 0.5) + Origin;
    const cf::SceneGraph::BspTreeNodeT* BspTree = m_World->m_StaticEntityData[0]->m_BspTree;

#if 0
    // Performance profiling revealed that this method is frequently called
    // and that the call to CollModel->TraceRay() is pretty expensive!
    const Vector3dT Ray     =Vector3dT(0.0, 0.0, -999999.0);

    cf::ClipSys::TraceResultT Result(1.0);
    CollModel->TraceRay(BBCenter, Ray, MaterialT::Clip_Radiance, Result);

    Vector3dT       Ground  =BBCenter+Ray*Result.Fraction+Vector3dT(0, 0, 0.2);
#else
    // We therefore revert to this call, which is similarly limited (considers faces only) but is *much* faster!
    // Note that the proper future solution is to reimplement this method to use a precomputed lighting grid!!
    const double    Trace   =BspTree->ClipLine(BBCenter, Vector3dT(0.0, 0.0, -1.0), 0.0, 999999.0);
    Vector3dT       Ground  =BBCenter+Vector3dT(0.0, 0.0, -(Trace-0.2));
#endif
    unsigned long   LeafNr  =BspTree->WhatLeaf(Ground);

    // This is a relatively cheap (dumb) trick for dealing with problematic input (like some static detail models).
    if (!BspTree->Leaves[LeafNr].IsInnerLeaf)
    {
        const VectorT TestPoints[4]={ VectorT(Dimensions.Min.x, Dimensions.Min.y, Dimensions.Max.z)+Origin,
                                      VectorT(Dimensions.Min.x, Dimensions.Max.y, Dimensions.Max.z)+Origin,
                                      VectorT(Dimensions.Max.x, Dimensions.Min.y, Dimensions.Max.z)+Origin,
                                      VectorT(Dimensions.Max.x, Dimensions.Max.y, Dimensions.Max.z)+Origin };

        Ground.z=-999999.0;

        for (unsigned long TestNr=0; TestNr<4; TestNr++)
        {
#if 0
            // Same performance issue as above...
            CollModel->TraceRay(TestPoints[TestNr], Ray, MaterialT::Clip_Radiance, Result);     // BUG / FIXME: I think we have to re-init Result here!
            const Vector3dT     TestGround=TestPoints[TestNr]+Ray*Result.Fraction+Vector3dT(0, 0, 0.2);
#else
            const double        TestTrace =BspTree->ClipLine(TestPoints[TestNr], Vector3dT(0.0, 0.0, -1.0), 0.0, 999999.0);
            const Vector3dT     TestGround=TestPoints[TestNr]+Vector3dT(0.0, 0.0, -(TestTrace-0.2));
#endif
            const unsigned long TestLeafNr=BspTree->WhatLeaf(TestGround);

            if (!BspTree->Leaves[TestLeafNr].IsInnerLeaf) continue;

            if (TestGround.z>Ground.z)
            {
                Ground=TestGround;
                LeafNr=TestLeafNr;
            }
        }
    }

    for (unsigned long FNr=0; FNr<BspTree->Leaves[LeafNr].FaceChildrenSet.Size(); FNr++)
    {
        cf::SceneGraph::FaceNodeT* FaceNode=BspTree->FaceChildren[BspTree->Leaves[LeafNr].FaceChildrenSet[FNr]];
        Vector3fT                  AmbientLightColor;

        if (FaceNode->GetLightmapColorNearPosition(Ground, AmbientLightColor))
            return AmbientLightColor;
    }

    return Vector3fT(1.0f, 1.0f, 1.0f);
}


const ArrayT<unsigned long>& Ca3DEWorldT::GetAllEntityIDs() const
{
    static ArrayT<unsigned long> AllEntityIDs;

    AllEntityIDs.Overwrite();

    for (unsigned long EntityNr=0; EntityNr<m_EngineEntities.Size(); EntityNr++)
        if (m_EngineEntities[EntityNr]!=NULL)
            AllEntityIDs.PushBack(EntityNr);

    return AllEntityIDs;
}


IntrusivePtrT<GameEntityI> Ca3DEWorldT::GetGameEntityByID(unsigned long EntityID) const
{
    if (EntityID<m_EngineEntities.Size())
        if (m_EngineEntities[EntityID]!=NULL)
            return m_EngineEntities[EntityID]->GetGameEntity();

    return NULL;
}


const CafuModelT* Ca3DEWorldT::GetModel(const std::string& FileName) const
{
    return m_ModelMan.GetModel(FileName);
}


cf::GuiSys::GuiResourcesT& Ca3DEWorldT::GetGuiResources() const
{
    return m_GuiRes;
}
