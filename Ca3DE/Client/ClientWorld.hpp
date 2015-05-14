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

#ifndef CAFU_CACLIENTWORLD_HPP_INCLUDED
#define CAFU_CACLIENTWORLD_HPP_INCLUDED

#include "../Ca3DEWorld.hpp"
#include "GameSys/CompTransform.hpp"

#if defined(_WIN32) && _MSC_VER<1600
#include "pstdint.h"            // Paul Hsieh's portable implementation of the stdint.h header.
#else
#include <stdint.h>
#endif


struct PlayerCommandT;
class  NetDataT;


/// This class reflects which entities are relevant for this client at the given server frame.
class FrameInfoT
{
    public:

    FrameInfoT() : IsValid(false) { }

    bool                  IsValid;          ///< Is this a properly received and thus usable frame info at all?
    unsigned long         ServerFrameNr;    ///< The number of the server frame that this info is about.
    ArrayT<unsigned long> EntityIDsInPVS;   ///< The IDs of the entities that are relevant for this client at the given server frame.
};


class CaClientWorldT : public Ca3DEWorldT
{
    public:

    // Constructor
    CaClientWorldT(const char* FileName, ModelManagerT& ModelMan, cf::GuiSys::GuiResourcesT& GuiRes, WorldT::ProgressFunctionT ProgressFunction, unsigned long OurEntityID_) /*throw (WorldT::LoadErrorT)*/;

    unsigned long GetOurEntityID() const { return OurEntityID; }  // AUFLÖSEN!?

    // Erzeugt einen neuen Entity durch das zu Ende Lesen einer SC1_EntityBaseLine Message. Gibt 'true' zurück bei Erfolg, sonst 'false'.
    // Letzteres passiert nur nach einem fatalen Fehler, nämlich wenn 'InData' einen unbekannten Entity-Typ beschreibt (Entity-TypeID).
    bool ReadEntityBaseLineMessage(NetDataT& InData);

    unsigned long ReadServerFrameMessage(NetDataT& InData);

    void OurEntity_Predict(const PlayerCommandT& PlayerCommand, unsigned int PlayerCommandNr);

    /// Returns the camera details of "our" entity that the client should use to render the world.
    /// This is typically called for the local human player from whose perspective the world is rendered.
    ///
    /// @returns `NULL` if "our" entity was not available (or no camera details could be retrieved),
    ///          the Transform component of the camera entity on success.
    IntrusivePtrT<const cf::GameSys::ComponentTransformT> OurEntity_GetCamera() const;

    void ComputeBFSPath(const VectorT& Start, const VectorT& End);
    void Draw(float FrameTime, IntrusivePtrT<const cf::GameSys::ComponentTransformT> CameraTrafo) const;


    private:

    CaClientWorldT(const CaClientWorldT&);      // Use of the Copy    Constructor is not allowed.
    void operator = (const CaClientWorldT&);    // Use of the Assignment Operator is not allowed.

    // Ruft 'EngineEntityT::ParseServerDeltaUpdateMessage()' für den EngineEntityT mit der ID 'EntityID' auf (siehe Dokumentation dieser Funktion!).
    // Das Rückgabeergebnis entspricht dem dieser Funktion, ein Scheitern kann nun aber zusätzlich vorkommen, falls 'EntityID' nicht existiert.
    bool ParseServerDeltaUpdateMessage(unsigned long EntityID, unsigned long DeltaFrameNr, unsigned long ServerFrameNr,
                                       const ArrayT<uint8_t>* DeltaMessage);

    // Please see the corresponding function in EngineEntityT for documentation.
    bool GetLightSourceInfo(unsigned long EntityID, unsigned long& DiffuseColor, unsigned long& SpecularColor, VectorT& Position, float& Radius, bool& CastsShadows) const;

    // Draws all entities whose ID is contained in the 'EntityIDs' array.
    // The entity with ID 'OurEntityID' specifies "our" entity.
    // (Everything else is drawn from the viewpoint from this entity, and it is necessary to let the
    //  render code know that, such that (for example) it can prevent that we see the inside of our own body!)
    // Note that the Material Systems global per-lightsource lighting parameters (light-source and eye position etc.)
    // should have been set before calling this function. Ambient light color is however set within this function (per entity).
    void DrawEntities(unsigned long OurEntityID, bool SkipOurEntity, const VectorT& ViewerPos, const ArrayT<unsigned long>& EntityIDs) const;

    // Calls the 'PostDraw()' methods of all entities whose ID is contained in the 'EntityIDs' array.
    // The calls are ordered such that the call to the entity with ID 'OurEntityID' is made last.
    // The 'FrameTime' is passed to each call of 'PostDraw()'.
    // All this provides opportunities for entities to render HUDs, employ simple "mini-prediction",
    // triggers sounds, register particles, do other server-independent eye-candy, and so on.
    void PostDrawEntities(float FrameTime, const ArrayT<unsigned long>& EntityIDs) const;


    const unsigned long    OurEntityID;

    ArrayT<FrameInfoT>     m_FrameInfos;        ///< The last frame infos as received from the server. Past frame infos are kept for the delta decompression.
    unsigned long          m_ServerFrameNr;     ///< The number of the latest server frame that we have been updated to (received an SC1_FrameInfo message for).

    ArrayT<PlayerCommandT> m_PlayerCommands;    ///< The last player commands, kept for the reprediction that is applied after each frame update from the server.
    unsigned int           m_PlayerCommandNr;   ///< The number of the latest player command in m_PlayerCommands.

    ArrayT<unsigned long>  BFS_Tree;
    ArrayT<VectorT>        BFS_TreePoints;
    unsigned long          BFS_EndLeafNr;
};

#endif
