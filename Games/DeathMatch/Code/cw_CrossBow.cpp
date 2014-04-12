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

#include "cw_CrossBow.hpp"
#include "../../PlayerCommand.hpp"
#include "../../GameWorld.hpp"
#include "HumanPlayer.hpp"
#include "Constants_AmmoSlots.hpp"
#include "Constants_WeaponSlots.hpp"
#include "PhysicsWorld.hpp"
#include "Libs/LookupTables.hpp"
#include "GameSys/CompPhysics.hpp"
#include "Models/ModelManager.hpp"

using namespace GAME_NAME;


CarriedWeaponCrossBowT::CarriedWeaponCrossBowT(ModelManagerT& ModelMan)
    : CarriedWeaponT(ModelMan.GetModel("Games/DeathMatch/Models/Weapons/DartGun/DartGun_v.cmdl"),
                     ModelMan.GetModel("Games/DeathMatch/Models/Weapons/DartGun/DartGun_p.cmdl"))
{
}


bool CarriedWeaponCrossBowT::ServerSide_PickedUpByEntity(EntHumanPlayerT* Player, IntrusivePtrT<cf::GameSys::ComponentHumanPlayerT> HumanPlayer) const
{
    EntityStateT& State=Player->GetState();

    // Consider if the entity already has this weapon.
    if (State.HaveWeapons & (1 << WEAPON_SLOT_CROSSBOW))
    {
        // If it also has the max. amount of ammo of this type, ignore the touch.
        if (State.HaveAmmo[AMMO_SLOT_ARROWS]==30) return false;

        // Otherwise pick the weapon up and let it have the ammo.
        State.HaveAmmo[AMMO_SLOT_ARROWS]+=10;
    }
    else
    {
        // This weapon is picked up for the first time.
        State.HaveWeapons|=1 << WEAPON_SLOT_CROSSBOW;
        State.ActiveWeaponSlot   =WEAPON_SLOT_CROSSBOW;
        State.ActiveWeaponSequNr =5;    // Draw
        State.ActiveWeaponFrameNr=0.0;

        State.HaveAmmoInWeapons[WEAPON_SLOT_CROSSBOW] =5;
        State.HaveAmmo         [AMMO_SLOT_ARROWS    ]+=5;
    }

    // Limit the amount of carryable ammo.
    if (State.HaveAmmo[AMMO_SLOT_ARROWS]>30) State.HaveAmmo[AMMO_SLOT_ARROWS]=30;

    return true;
}


void CarriedWeaponCrossBowT::ServerSide_Think(EntHumanPlayerT* Player, IntrusivePtrT<cf::GameSys::ComponentHumanPlayerT> HumanPlayer, const PlayerCommandT& PlayerCommand, bool ThinkingOnServerSide, unsigned long /*ServerFrameNr*/, bool AnimSequenceWrap) const
{
    EntityStateT& State=Player->GetState();

    switch (State.ActiveWeaponSequNr)
    {
        case 0: // Idle1
        case 1: // Idle2
        case 2: // Idle3
            if (PlayerCommand.Keys & PCK_Fire1)
            {
                State.ActiveWeaponSequNr =3;    // Fire
                State.ActiveWeaponFrameNr=0.0;

                if (ThinkingOnServerSide)
                {
                    // If we are on the server-side, find out what or who we hit.
                    const Vector3dT  ViewDir = Player->GetViewDir();
                    const RayResultT RayResult(HumanPlayer->TracePlayerRay(ViewDir));

                    if (RayResult.hasHit() && RayResult.GetHitPhysicsComp())
                        HumanPlayer->InflictDamage(RayResult.GetHitPhysicsComp()->GetEntity(), 20.0f, ViewDir);
                }
                break;
            }

            if (AnimSequenceWrap)
            {
                if (State.ActiveWeaponSequNr==2)
                {
                    State.ActiveWeaponSequNr=LookupTables::RandomUShort[PlayerCommand.Nr & 0xFFF] % 3;
                }
                else State.ActiveWeaponSequNr=2;    // Idle3 is the "best-looking" sequence.

                State.ActiveWeaponFrameNr=0.0;
            }
            break;

        case 3: // Fire
            if (AnimSequenceWrap)
            {
                State.ActiveWeaponSequNr =4;
                State.ActiveWeaponFrameNr=0.0;
            }
            break;

        case 4: // Reload
            if (AnimSequenceWrap)
            {
                State.ActiveWeaponSequNr =2;
                State.ActiveWeaponFrameNr=0.0;
            }
            break;

        case 5: // Draw (TakeOut)
            if (AnimSequenceWrap)
            {
                State.ActiveWeaponSequNr =0;
                State.ActiveWeaponFrameNr=0.0;
            }
            break;

        case 6: // Holster (PutAway)
            break;
    }
}
