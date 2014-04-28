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

#ifndef CAFU_COMPONENT_GAME_ENTITY_HPP_INCLUDED
#define CAFU_COMPONENT_GAME_ENTITY_HPP_INCLUDED

#include "GameSys/CompBase.hpp"
#include "GameSys/Entity.hpp"       // For GetGameEnt() only.


namespace cf { namespace ClipSys { class ClipModelT; } }
class StaticEntityDataT;


/// This component houses the "engine-specific" parts of its entity.
/// It is intended for use by the implementing applications only (map compilers, engine), that is,
/// as the "App" component of `cf::GameSys::EntityT`s. It is not intended for use in game scripts.
/// As such, it doesn't integrate with the TypeSys, and thus isn't available for scripting and
/// whereever else we need the related meta-data.
class CompGameEntityT : public cf::GameSys::ComponentBaseT
{
    public:

    /// The constructor.
    CompGameEntityT(StaticEntityDataT* SED = NULL);

    /// The copy constructor. It creates a new component as a copy of another component.
    /// @param Comp   The component to copy-construct this component from.
    CompGameEntityT(const CompGameEntityT& Comp);

    /// The destructor.
    ~CompGameEntityT();

    const StaticEntityDataT* GetStaticEntityData() const { return m_StaticEntityData; }
    StaticEntityDataT* GetStaticEntityData() { return m_StaticEntityData; }

    // Base class overrides.
    CompGameEntityT* Clone() const;
    const char* GetName() const { return "GameEntity"; }
    void UpdateDependencies(cf::GameSys::EntityT* Entity);
    BoundingBox3fT GetVisualBB() const;
    void GetCollisionBB(BoundingBox3fT& BB) const;


    private:

    void DoDeserialize(cf::Network::InStreamT& Stream, bool IsIniting) /*override*/;
    void DoServerFrame(float t) /*override*/;
    void UpdateClipModel();

    StaticEntityDataT*       m_StaticEntityData;
    const bool               m_DeleteSED;

    cf::ClipSys::ClipModelT* m_ClipModel;       ///< The clip model for the m_StaticEntityData->m_CollModel (the collision model made of the map primitives), NULL for none.
    Vector3fT                m_ClipPrevOrigin;
    cf::math::QuaternionfT   m_ClipPrevQuat;
};


inline IntrusivePtrT<CompGameEntityT> GetGameEnt(IntrusivePtrT<cf::GameSys::EntityT> Entity)
{
    return dynamic_pointer_cast<CompGameEntityT>(Entity->GetApp());
}

#endif
