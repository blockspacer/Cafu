/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2014 Carsten Fuchs Software.

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

#include "ServerWorld.hpp"
#include "ClientInfo.hpp"
#include "../EngineEntity.hpp"
#include "ConsoleCommands/ConVar.hpp"
#include "ConsoleCommands/Console.hpp"      // For cf::va().
#include "GameSys/HumanPlayer/CompCarriedWeapon.hpp"
#include "GameSys/HumanPlayer/CompInventory.hpp"
#include "GameSys/CompCollisionModel.hpp"
#include "GameSys/CompHumanPlayer.hpp"
#include "GameSys/CompModel.hpp"
#include "GameSys/CompPlayerPhysics.hpp"
#include "GameSys/CompScript.hpp"
#include "GameSys/CompSound.hpp"
#include "GameSys/Entity.hpp"
#include "GameSys/EntityCreateParams.hpp"
#include "GameSys/World.hpp"
#include "Math3D/Matrix3x3.hpp"
#include "Network/Network.hpp"
#include "SceneGraph/BspTreeNode.hpp"
#include "Win32/Win32PrintHelp.hpp"
#include "TypeSys.hpp"
#include "../NetConst.hpp"
#include "../Common/CompGameEntity.hpp"


CaServerWorldT::CaServerWorldT(const char* FileName, ModelManagerT& ModelMan, cf::GuiSys::GuiResourcesT& GuiRes)
    : Ca3DEWorldT(FileName, ModelMan, GuiRes, false, NULL),
      // Note that 0 is reserved for referring to the "baseline" (the state in which entities were created),
      // as opposed to the entity state at a specific server frame.
      // (The `ClientInfoT::LastKnownFrameReceived` start at 0, see ClientInfoT::InitForNewWorld() for details.)
      m_ServerFrameNr(1),
      m_IsThinking(false),
      m_EntityRemoveList()
{
    // Note that we must NOT modify anything about the entity states here --
    // all entity states at frame 1 must be EXACT matches on the client and the server!
}


// Die Clients bekommen unabhängig hiervon in einer SC1_DropClient Message explizit mitgeteilt, wenn ein Client (warum auch immer) den Server verläßt.
// Den dazugehörigen Entity muß der Client deswegen aber nicht unbedingt sofort und komplett aus seiner World entfernen,
// dies sollte vielmehr durch Wiederverwendung von EntityIDs durch den Server geschehen!
void CaServerWorldT::RemoveEntity(unsigned long EntityID)
{
    if (m_IsThinking)
    {
        // We're currently thinking, and EntityID might be the ID of the entity that currently thinks.
        // (That is, this entity is removing itself, as for example an exploded grenade.)
        // Thus, schedule this entity for removal until the thinking is finished.
        m_EntityRemoveList.PushBack(EntityID);
    }
    else
    {
        // Currently not thinking, so it should be safe to remove the entity immediately.
        if (EntityID >= m_EngineEntities.Size()) return;
        if (m_EngineEntities[EntityID] == NULL) return;

        IntrusivePtrT<cf::GameSys::EntityT> Ent = m_EngineEntities[EntityID]->GetEntity();

        if (Ent == m_ScriptWorld->GetRootEntity()) return;

        if (Ent->GetParent() != NULL)
            Ent->GetParent()->RemoveChild(Ent);

        Console->Print(cf::va("Entity %lu removed (\"%s\") in CaServerWorldT::RemoveEntity().\n", EntityID, m_EngineEntities[EntityID]->GetEntity()->GetBasics()->GetEntityName().c_str()));

        delete m_EngineEntities[EntityID];
        m_EngineEntities[EntityID]=NULL;
    }
}


unsigned long CaServerWorldT::InsertHumanPlayerEntityForNextFrame(const char* PlayerName, const char* ModelName, unsigned long ClientInfoNr)
{
    IntrusivePtrT<cf::GameSys::EntityT> NewEnt  = new cf::GameSys::EntityT(cf::GameSys::EntityCreateParamsT(*m_ScriptWorld));
    IntrusivePtrT<CompGameEntityT>      GameEnt = new CompGameEntityT();

    ArrayT< IntrusivePtrT<cf::GameSys::EntityT> > AllEnts;
    m_ScriptWorld->GetRootEntity()->GetAll(AllEnts);

    for (unsigned int EntNr = 0; EntNr < AllEnts.Size(); EntNr++)
        if (AllEnts[EntNr]->GetComponent("PlayerStart") != NULL)
        {
            NewEnt->GetTransform()->SetOriginWS(AllEnts[EntNr]->GetTransform()->GetOriginWS() + Vector3fT(0, 0, 40));
            NewEnt->GetTransform()->SetQuatWS(AllEnts[EntNr]->GetTransform()->GetQuatWS());
            break;
        }

    NewEnt->GetBasics()->SetEntityName(cf::va("Player_%lu", ClientInfoNr+1));
    NewEnt->SetApp(GameEnt);

    IntrusivePtrT<cf::GameSys::ComponentHumanPlayerT> HumanPlayerComp = new cf::GameSys::ComponentHumanPlayerT();
    HumanPlayerComp->SetMember("PlayerName", std::string(PlayerName));
    NewEnt->AddComponent(HumanPlayerComp);

    IntrusivePtrT<cf::GameSys::ComponentCollisionModelT> CollMdl = new cf::GameSys::ComponentCollisionModelT();
    // The player script code will set the details of the collision model itself.
    NewEnt->AddComponent(CollMdl);

    IntrusivePtrT<cf::GameSys::ComponentPlayerPhysicsT> PlayerPhysicsComp = new cf::GameSys::ComponentPlayerPhysicsT();
    PlayerPhysicsComp->SetMember("Dimensions", BoundingBox3dT(Vector3dT(-16.0, -16.0, -36.0), Vector3dT(16.0,  16.0, 36.0)));
    PlayerPhysicsComp->SetMember("StepHeight", 18.5);
    NewEnt->AddComponent(PlayerPhysicsComp);

    IntrusivePtrT<cf::GameSys::ComponentModelT> Model3rdPersonComp = new cf::GameSys::ComponentModelT();
    Model3rdPersonComp->SetMember("Name", std::string("Games/DeathMatch/Models/Players/") + ModelName + "/" + ModelName + ".cmdl");     // TODO... don't hardcode the path!
    NewEnt->AddComponent(Model3rdPersonComp);

    IntrusivePtrT<cf::GameSys::ComponentInventoryT> InvComp = new cf::GameSys::ComponentInventoryT();
    // The inventory's maxima e.g. for bullets, shells, etc. are set in `HumanPlayer.lua`.
    NewEnt->AddComponent(InvComp);

    IntrusivePtrT<cf::GameSys::ComponentScriptT> ScriptComp = new cf::GameSys::ComponentScriptT();
    ScriptComp->SetMember("Name", std::string("Games/DeathMatch/Scripts/HumanPlayer.lua"));
    NewEnt->AddComponent(ScriptComp);

    // Equip the player with components for all the weapons that he can possibly carry.
    // (What's about the Glock17 model, btw.? It seems we have a model, but no code for it?)
    {
        IntrusivePtrT<cf::GameSys::ComponentCarriedWeaponT> CompCW = NULL;

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("BattleScythe"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_BattleScythe.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(0));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("Beretta"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_Beretta.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(17));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("DesertEagle"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_DesertEagle.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(6));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("Shotgun"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_Shotgun.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(8));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("9mmAR"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_9mmAR.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(25));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(2));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("DartGun"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_DartGun.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(5));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("Bazooka"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_Bazooka.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(1));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("Gauss"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_Gauss.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(20));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("Egon"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_Egon.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(20));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("Grenade"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_Grenade.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(1));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);

        CompCW = new cf::GameSys::ComponentCarriedWeaponT();
        CompCW->SetMember("Label",            std::string("FaceHugger"));
        CompCW->SetMember("IsAvail",          false);
        CompCW->SetMember("Script",           std::string("Games/DeathMatch/Scripts/cw_FaceHugger.lua"));
        CompCW->SetMember("PrimaryAmmo",      uint16_t(0));
        CompCW->SetMember("MaxPrimaryAmmo",   uint16_t(1));
        CompCW->SetMember("SecondaryAmmo",    uint16_t(0));
        CompCW->SetMember("MaxSecondaryAmmo", uint16_t(0));
        NewEnt->AddComponent(CompCW);
    }

    // Add a camera as a child entity of NewEnt.
    {
        IntrusivePtrT<cf::GameSys::EntityT> CameraEnt = new cf::GameSys::EntityT(cf::GameSys::EntityCreateParamsT(*m_ScriptWorld));

        CameraEnt->GetBasics()->SetEntityName("Camera");
        CameraEnt->GetTransform()->SetOriginPS(Vector3fT(0.0f, 0.0f, 32.0f));   // TODO: Hardcoded values here and in the CompHumanPlayer code...

        NewEnt->AddChild(CameraEnt);
    }

    // Add another child entity for the 1st-person weapon models.
    // Note that the 1st-person models are *not* attached to the camera's origin.
    {
        IntrusivePtrT<cf::GameSys::EntityT> FirstPersonEnt = new cf::GameSys::EntityT(cf::GameSys::EntityCreateParamsT(*m_ScriptWorld));

        FirstPersonEnt->GetBasics()->SetEntityName("FirstPersonEnt");

        // The offset of -0.5 relative to the camera origin gives the weapon a nice
        // 'shifting' effect when the player looks up/down.
        FirstPersonEnt->GetTransform()->SetOriginPS(Vector3fT(0.0f, 0.0f, 31.5f));

        IntrusivePtrT<cf::GameSys::ComponentModelT> Model1stPersonComp = new cf::GameSys::ComponentModelT();
     // Model1stPersonComp->SetMember("Name", ...);     // Initialized and updated by the HumanPlayer code.
        Model1stPersonComp->SetMember("Show", false);
        Model1stPersonComp->SetMember("Is1stPerson", true);
        FirstPersonEnt->AddComponent(Model1stPersonComp);

        NewEnt->AddChild(FirstPersonEnt);

        // Add a child entity to "FirstPersonEnt" that holds a Sound component for the weapons.
        {
            IntrusivePtrT<cf::GameSys::EntityT> WeaponSoundEnt = new cf::GameSys::EntityT(cf::GameSys::EntityCreateParamsT(*m_ScriptWorld));

            WeaponSoundEnt->GetBasics()->SetEntityName("WeaponSoundEnt");

            // The offset along the view direction is the relevant reason for introducing the WeaponSoundEnt.
            WeaponSoundEnt->GetTransform()->SetOriginPS(Vector3fT(16.0f, 0.0f, 0.0f));

            IntrusivePtrT<cf::GameSys::ComponentSoundT> WeaponSoundComp = new cf::GameSys::ComponentSoundT();
         // WeaponSoundComp->SetMember("Name", ...);    // Initialized and updated by the HumanPlayer (weapon) code.
            WeaponSoundComp->SetMember("AutoPlay", false);
            WeaponSoundEnt->AddComponent(WeaponSoundComp);

            FirstPersonEnt->AddChild(WeaponSoundEnt);
        }
    }

    m_ScriptWorld->GetRootEntity()->AddChild(NewEnt);

    // As we're inserting a new entity into a live map, post-load stuff must be run here.
    ScriptComp->OnPostLoad(false);
    ScriptComp->CallLuaMethod("OnInit", 0);

    for (unsigned int i = 0; i < 99; i++)
    {
        IntrusivePtrT<cf::GameSys::ComponentCarriedWeaponT> CompCW =
            dynamic_pointer_cast<cf::GameSys::ComponentCarriedWeaponT>(NewEnt->GetComponent("CarriedWeapon", i));

        if (CompCW == NULL) break;

        CompCW->OnPostLoad(false);
        CompCW->CallLuaMethod("OnInit", 0);
    }

    return CreateNewEntityFromBasicInfo(GameEnt, m_ServerFrameNr + 1);
}


void CaServerWorldT::NotifyHumanPlayerEntityOfClientCommand(unsigned long HumanPlayerEntityID, const PlayerCommandT& PlayerCommand)
{
    if (HumanPlayerEntityID < m_EngineEntities.Size())
        if (m_EngineEntities[HumanPlayerEntityID] != NULL)
        {
            IntrusivePtrT<cf::GameSys::ComponentHumanPlayerT> CompHP =
                dynamic_pointer_cast<cf::GameSys::ComponentHumanPlayerT>(m_EngineEntities[HumanPlayerEntityID]->GetEntity()->GetComponent("HumanPlayer"));

            if (CompHP != NULL)
                CompHP->GetPlayerCommands().PushBack(PlayerCommand);
        }
}


void CaServerWorldT::Think(float FrameTime)
{
    // Zuerst die Nummer des nächsten Frames 'errechnen'.
    // Die Reihenfolge ist wichtig, denn wenn ein neuer Entity geschaffen wird,
    // muß dieser korrekt wissen, zu welchem Frame er ins Leben gerufen wurde.
    m_ServerFrameNr++;

    // Jetzt das eigentliche Denken durchführen.
    // Heraus kommt eine Aussage der Form: "Zum Frame Nummer 'm_ServerFrameNr' ist die World in diesem Zustand!"
    if (m_IsThinking) return;

    m_IsThinking=true;

    // Beachte:
    // - Neu geschaffene Entities sollen nicht gleich 'Think()'en!
    //   Zur Erreichung vergleiche dazu die Implementation von EngineEntityT::Think().
    //   DÜRFTEN sie trotzdem gleich Think()en??? (JA!) Die OldStates kämen dann evtl. durcheinander!? (NEIN!)
    //   Allerdings übertragen wir mit BaseLines grundsätzlich KEINE Events (??? PRÜFEN!), Think()en macht insofern also nur eingeschränkt Sinn.
    // - EntityIDs sollten wohl besser NICHT wiederverwendet werden, da z.B. Parents die IDs ihrer Children speichern usw.
    // - Letzteres führt aber zu zunehmend vielen NULL-Pointern im m_EngineEntities-Array.
    // - Dies könnte sich evtl. mit einem weiteren Array von 'active EntityIDs' lösen lassen.
    for (unsigned long EntityNr=0; EntityNr<m_EngineEntities.Size(); EntityNr++)
        if (m_EngineEntities[EntityNr]!=NULL)
            m_EngineEntities[EntityNr]->PreThink(m_ServerFrameNr);

    // Must never move this above the PreThink() calls above, because ...(?)
    m_PhysicsWorld.Think(FrameTime);

    m_ScriptWorld->GetScriptState().RunPendingCoroutines(FrameTime);    // Should do this early: new coroutines are usually added "during" thinking.

    for (unsigned long EntityNr=0; EntityNr<m_EngineEntities.Size(); EntityNr++)
        if (m_EngineEntities[EntityNr]!=NULL)
            m_EngineEntities[EntityNr]->Think(FrameTime, m_ServerFrameNr);

    m_IsThinking=false;


    // If entities removed other entities (or even themselves!) while thinking, remove them now.
    for (unsigned long EntNr = 0; EntNr < m_EngineEntities.Size(); EntNr++)
    {
        if (m_EngineEntities[EntNr] == NULL) continue;

        IntrusivePtrT<cf::GameSys::EntityT> Ent = m_EngineEntities[EntNr]->GetEntity();

        if (Ent != m_ScriptWorld->GetRootEntity() && Ent->GetParent().IsNull())
        {
            Console->Print(cf::va("Entity %lu removed (\"%s\"), it no longer has a parent.\n", EntNr, m_EngineEntities[EntNr]->GetEntity()->GetBasics()->GetEntityName().c_str()));
            delete m_EngineEntities[EntNr];
            m_EngineEntities[EntNr]=NULL;
        }
    }

    for (unsigned long RemoveNr=0; RemoveNr<m_EntityRemoveList.Size(); RemoveNr++)
    {
        const unsigned long EntityID=m_EntityRemoveList[RemoveNr];

        Console->Print(cf::va("Entity %lu removed (\"%s\") via m_EntityRemoveList.\n", EntityID, m_EngineEntities[EntityID]->GetEntity()->GetBasics()->GetEntityName().c_str()));
        delete m_EngineEntities[EntityID];
        m_EngineEntities[EntityID]=NULL;
    }

    m_EntityRemoveList.Overwrite();


    // If entities (script code) created new entities:
    //   - they only exist in the script world, but not yet in m_EngineEntities,
    //   - they have no App component, which we can use to identify them.
    ArrayT< IntrusivePtrT<cf::GameSys::EntityT> > AllEnts;

    m_ScriptWorld->GetRootEntity()->GetAll(AllEnts);

    for (unsigned int EntNr = 0; EntNr < AllEnts.Size(); EntNr++)
    {
        if (AllEnts[EntNr]->GetApp() == NULL)
        {
            IntrusivePtrT<CompGameEntityT> GameEnt = new CompGameEntityT();

            AllEnts[EntNr]->SetApp(GameEnt);
            CreateNewEntityFromBasicInfo(GameEnt, m_ServerFrameNr);

            const ArrayT< IntrusivePtrT<cf::GameSys::ComponentBaseT> >& Components = AllEnts[EntNr]->GetComponents();

            for (unsigned int CompNr = 0; CompNr < Components.Size(); CompNr++)
            {
                // The components belong to an entity that was newly added to a live map.
                // Consequently, we must run the post-load stuff here.
                Components[CompNr]->OnPostLoad(false);
                Components[CompNr]->CallLuaMethod("OnInit", 0);
            }
        }
    }
}


unsigned long CaServerWorldT::WriteClientNewBaseLines(unsigned long OldBaseLineFrameNr, ArrayT< ArrayT<char> >& OutDatas) const
{
    const unsigned long SentClientBaseLineFrameNr = OldBaseLineFrameNr;

    for (unsigned long EntityNr=0; EntityNr < m_EngineEntities.Size(); EntityNr++)
        if (m_EngineEntities[EntityNr]!=NULL)
            m_EngineEntities[EntityNr]->WriteNewBaseLine(SentClientBaseLineFrameNr, OutDatas);

    return m_ServerFrameNr;
}


void CaServerWorldT::UpdateFrameInfo(ClientInfoT& ClientInfo) const
{
    const unsigned int TEMP_MAX_OLDSTATES = 16 + 1;     // (?)

    if (ClientInfo.OldStatesPVSEntityIDs.Size() < TEMP_MAX_OLDSTATES)
    {
        ClientInfo.OldStatesPVSEntityIDs.PushBackEmpty();

        ClientInfo.CurrentStateIndex = ClientInfo.OldStatesPVSEntityIDs.Size() - 1;
    }
    else
    {
        ClientInfo.CurrentStateIndex++;
        if (ClientInfo.CurrentStateIndex >= TEMP_MAX_OLDSTATES) ClientInfo.CurrentStateIndex = 0;

        ClientInfo.OldStatesPVSEntityIDs[ClientInfo.CurrentStateIndex].Overwrite();
    }

    const EngineEntityT* EE = m_EngineEntities[ClientInfo.EntityID];

    if (!EE) return;

    const cf::SceneGraph::BspTreeNodeT* BspTree = m_World->m_StaticEntityData[0]->m_BspTree;
    ArrayT<unsigned long>& NewStatePVSEntityIDs = ClientInfo.OldStatesPVSEntityIDs[ClientInfo.CurrentStateIndex];
    const unsigned long    ClientLeafNr         = BspTree->WhatLeaf(EE->GetEntity()->GetTransform()->GetOriginWS().AsVectorOfDouble());

    // Determine all entities that are relevant for (in the PVS of) this client.
    for (unsigned long EntityNr = 0; EntityNr < m_EngineEntities.Size(); EntityNr++)
        if (m_EngineEntities[EntityNr] != NULL)
        {
            BoundingBox3dT EntityBB = m_EngineEntities[EntityNr]->GetEntity()->GetCullingBB(true /*WorldSpace*/).AsBoxOfDouble();

            if (!EntityBB.IsInited())
            {
                // If the entity has no visual representation, add its origin point in order to "compensate" this.
                // At this time, accounting for such "invisible" entities is useful, because e.g. we have no explicit
                // "potentially audible set" for sound sources. This will also cover entities that the client *really*
                // cannot see, e.g. player starting points and other purely informational entities, but that's ok for now.
                EntityBB += m_EngineEntities[EntityNr]->GetEntity()->GetTransform()->GetOriginWS().AsVectorOfDouble();
            }

            if (BspTree->IsInPVS(EntityBB, ClientLeafNr)) NewStatePVSEntityIDs.PushBack(EntityNr);
        }

    // Make sure that NewStatePVSEntityIDs is in sorted order. At the time of this writing, this is trivially the
    // case, but it is also easy to foresee changes to the above loop that unintentionally break this rule, which is
    // an important requirement for the code below and whose violations may cause hard to diagnose problems.
    for (unsigned int i = 1; i < NewStatePVSEntityIDs.Size(); i++)
        assert(NewStatePVSEntityIDs[i - 1] < NewStatePVSEntityIDs[i]);
}


void CaServerWorldT::WriteClientDeltaUpdateMessages(const ClientInfoT& ClientInfo, NetDataT& OutData) const
{
    const unsigned long ClientFrameNr = ClientInfo.LastKnownFrameReceived;
    const ArrayT<unsigned long>* NewStatePVSEntityIDs = &ClientInfo.OldStatesPVSEntityIDs[ClientInfo.CurrentStateIndex];
    const ArrayT<unsigned long>* OldStatePVSEntityIDs = NULL;

    unsigned long DeltaFrameNr;     // Kann dies entfernen, indem der Packet-Header direkt im if-else-Teil geschrieben wird!

    if (ClientFrameNr == 0 || ClientFrameNr >= m_ServerFrameNr || ClientFrameNr + ClientInfo.OldStatesPVSEntityIDs.Size() - 1 < m_ServerFrameNr)
    {
        // Erläuterung der obigen if-Bedingung:
        // a) Der erste  Teil 'ClientFrameNr==0' ist klar! (Echt?? Vermutlich war gemeint, dass der Client in der letzten CS1_FrameInfoACK Nachricht explizit "0" geschickt und damit Baseline angefordert hat.)
        // b) Der zweite Teil 'ClientFrameNr>=ServerFrameNr' ist nur zur Sicherheit und sollte NIEMALS anspringen!
        // c) Der dritte Teil ist äquivalent zu 'ServerFrameNr-ClientFrameNr>=ClientOldStatesPVSEntityIDs.Size()'!
        static ArrayT<unsigned long> EmptyArray;

        // Entweder will der Client explizit ein retransmit haben (bei neuer World oder auf User-Wunsch (no-delta mode) oder nach Problemen),
        // oder beim Client ist schon länger keine verwertbare Nachricht mehr angekommen. Daher delta'en wir bzgl. der BaseLine!
        DeltaFrameNr         = 0;
        OldStatePVSEntityIDs = &EmptyArray;
    }
    else
    {
        // Nach obiger if-Bedingung ist FrameDiff auf jeden Fall in [1 .. ClientOldStatesPVSEntityIDs.Size()-1].
        const unsigned long FrameDiff = m_ServerFrameNr-ClientFrameNr;

        DeltaFrameNr         = ClientFrameNr;
        OldStatePVSEntityIDs = &ClientInfo.OldStatesPVSEntityIDs[FrameDiff <= ClientInfo.CurrentStateIndex ? ClientInfo.CurrentStateIndex - FrameDiff : ClientInfo.OldStatesPVSEntityIDs.Size() + ClientInfo.CurrentStateIndex - FrameDiff];
    }


    OutData.WriteByte(SC1_FrameInfo);
    OutData.WriteLong(m_ServerFrameNr);     // What we are delta'ing to   (Frame, für das wir Informationen schicken)
    OutData.WriteLong(DeltaFrameNr);        // What we are delta'ing from (Frame, auf das wir uns beziehen (0 für BaseLine))
    OutData.WriteLong(ClientInfo.LastPlayerCommandNr);  // The number of the last player command that has been received (and accounted for in m_ServerFrameNr).


    unsigned long OldIndex = 0;
    unsigned long NewIndex = 0;

    while (OldIndex < OldStatePVSEntityIDs->Size() || NewIndex < NewStatePVSEntityIDs->Size())
    {
        const unsigned long OldEntityID = OldIndex < OldStatePVSEntityIDs->Size() ? (*OldStatePVSEntityIDs)[OldIndex] : 0x99999999;
        const unsigned long NewEntityID = NewIndex < NewStatePVSEntityIDs->Size() ? (*NewStatePVSEntityIDs)[NewIndex] : 0x99999999;

        // Consider the following situation:
        // There are (or were) A REALLY BIG NUMBER of entities in the PVS of the current client entity ('m_EngineEntities[ClientEntityID]').
        // Each of them would cause data to be written into 'OutData'.
        // Unfortunately, if the network protocol detects later that 'OutData.Data.Size()' exceeds the maximum possible size,
        // the entire content is dropped, as it is only considered "unreliable data".
        // That's not inherently dangerous, BUT it tends to happen repeatedly (assuming the client does not move into another leaf with fewer entities).
        // Therefore, the client gets *no* updates anymore, and the truly problematic fact is that its *own* update is among those that get dropped!
        // The client-side prediction continues to let the client move for a while, but eventually it gets stuck.
        // (Theoretically, the client could try to move "blindly" into a leaf with few enough entities such that updates get through again
        //  (server operation in not tampered by this condition!), but until then, the screen seems to indicate that it is stuck/frozen.)
        // Okay, but what can we do? Carefully think about it, and you will find that NOTHING can be done without introducing NEW problems!
        // Why? The spec. / code simply requires the consistency of this function! ANY changes here cause trouble with delta updates later!
        // It seems that the only reasonable solution is to omit update messages of "other" entities:
        // Better risk not-updated fields of other entities, than getting stuck with the own entity!
        // In order to achieve this, the introduction of the 'SkipEntity' variable is the best solution I could think of.
        // (Also see constant GameProtocol1T::MAX_MSG_SIZE that defines the maximum package size.)
        const bool SkipEntity = OutData.Data.Size() > 4096 && NewEntityID != ClientInfo.EntityID;

        if (OldEntityID == NewEntityID)
        {
            // Diesen Entity gab es schon im alten Frame.
            // Hierhin kommen wir nur, wenn 'OldStatePVSEntityIDs' nicht leer ist, vergleiche mit obigem Code!
            // PRINZIPIELL geht dann die Differenz 'ServerFrameNr-ClientFrameNr' in Ordnung, ebenfalls nach obigem Code.
            // TATSÄCHLICH wäre noch denkbar, daß der Entity mit ID 'NewEntityID' erst neu erschaffen wurde,
            // d.h. jünger ist als der Client, und deshalb die Differenz doch zu groß ist!
            // Dies wird aber von der Logik hier vermieden, denn ein solcher neuer Entity kann ja nicht schon im alten Frame vorgekommen sein!
            // Mit anderen Worten: Der folgende Aufruf sollte NIEMALS scheitern, falls doch, ist das ein fataler Fehler, der intensives Debugging erfordert.
            // Dennoch ist es wahrscheinlich (??) nicht notwendig, den Client bei Auftreten dieses Fehler zu disconnecten.
            if (!SkipEntity)
                if (!m_EngineEntities[NewEntityID]->WriteDeltaEntity(false /* send from baseline? */, ClientFrameNr, OutData, false))
                    EnqueueString("SERVER ERROR: %s, L %u: NewEntityID %u, ServerFrameNr %u, ClientFrameNr %u\n", __FILE__, __LINE__, NewEntityID, m_ServerFrameNr, ClientFrameNr);

            OldIndex++;
            NewIndex++;
            continue;
        }

        if (OldEntityID > NewEntityID)
        {
            // Dies ist ein neuer Entity, sende ihn von der BaseLine aus.
            // Deswegen kann der folgende Aufruf (gemäß der Spezifikation von WriteDeltaEntity()) auch nicht scheitern!
            if (!SkipEntity)
                m_EngineEntities[NewEntityID]->WriteDeltaEntity(true /* send from baseline? */, 0, OutData, true);

            NewIndex++;
            continue;
        }

        if (OldEntityID < NewEntityID)
        {
            // Diesen Entity gibt es im neuen Frame nicht mehr.
            if (!SkipEntity)
            {
                OutData.WriteByte(SC1_EntityRemove);
                OutData.WriteLong(OldEntityID);
            }

            OldIndex++;
            continue;
        }
    }
}
