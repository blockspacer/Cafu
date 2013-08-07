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

#include "ARGrenade.hpp"
#include "TypeSys.hpp"
#include "EntityCreateParams.hpp"
#include "Interpolator.hpp"
#include "_ResourceManager.hpp"
#include "Libs/Physics.hpp"
#include "SoundSystem/SoundSys.hpp"
#include "SoundSystem/Sound.hpp"
#include "SoundSystem/SoundShaderManager.hpp"
#include "../../GameWorld.hpp"
#include "Models/Model_cmdl.hpp"
#include "Network/State.hpp"
#include "ParticleEngine/ParticleEngineMS.hpp"

using namespace GAME_NAME;


// Implement the type info related code.
const cf::TypeSys::TypeInfoT* EntARGrenadeT::GetType() const
{
    return &TypeInfo;
 // return &EntARGrenadeT::TypeInfo;
}

void* EntARGrenadeT::CreateInstance(const cf::TypeSys::CreateParamsT& Params)
{
    return new EntARGrenadeT(*static_cast<const EntityCreateParamsT*>(&Params));
}

const cf::TypeSys::TypeInfoT EntARGrenadeT::TypeInfo(GetBaseEntTIM(), "EntARGrenadeT", "BaseEntityT", EntARGrenadeT::CreateInstance, NULL /*MethodsList*/);


EntARGrenadeT::EntARGrenadeT(const EntityCreateParamsT& Params)
    : BaseEntityT(Params,
                  BoundingBox3dT(Vector3dT( 3.0,  3.0, 6.0),
                                 Vector3dT(-3.0, -3.0, 0.0)),
                  NUM_EVENT_TYPES),
      m_Velocity(),
      m_LifeTime(0.0f),
      m_Physics(m_Origin, m_Velocity, m_Dimensions, ClipModel, GameWorld->GetClipWorld()),
      m_Model(Params.GameWorld->GetModel("Games/DeathMatch/Models/Weapons/Grenade/Grenade_w.cmdl")),
      m_FireSound(SoundSystem->CreateSound3D(SoundShaderManager->GetSoundShader("Weapon/Shotgun_dBarrel")))
{
    Register(new InterpolatorT<Vector3dT>(m_Origin));
}


EntARGrenadeT::~EntARGrenadeT()
{
    // Release sound.
    SoundSystem->DeleteSound(m_FireSound);
}


void EntARGrenadeT::DoSerialize(cf::Network::OutStreamT& Stream) const
{
    Stream << float(m_Velocity.x);
    Stream << float(m_Velocity.y);
    Stream << float(m_Velocity.z);
    Stream << m_LifeTime;
}


void EntARGrenadeT::DoDeserialize(cf::Network::InStreamT& Stream)
{
    float f = 0.0f;

    Stream >> f; m_Velocity.x = f;
    Stream >> f; m_Velocity.y = f;
    Stream >> f; m_Velocity.z = f;
    Stream >> m_LifeTime;
}


void EntARGrenadeT::Think(float FrameTime, unsigned long /*ServerFrameNr*/)
{
    const float OldTimer=m_LifeTime;

    // Let the detonation timer tick.
    m_LifeTime+=FrameTime;


    if (m_LifeTime<3.0)
    {
        bool OldWishJump=false;

        m_Physics.MoveHuman(FrameTime, m_Heading, VectorT() /*WishVelocity*/, VectorT() /*WishVelLadder*/, false /*WishJump*/, OldWishJump, 0.0);
    }
    else if (m_LifeTime<6.0)     // (3.0<=m_LifeTime<6.0)
    {
        if (OldTimer<3.0)
        {
            PostEvent(EVENT_TYPE_EXPLODE);

            // Damage all entities that are close enough.
            // TODO: In order to avoid damaging entities through thin walls, we should also perform a simple "visibility test".
            const ArrayT<unsigned long>& AllEntityIDs=GameWorld->GetAllEntityIDs();

            for (unsigned long EntityIDNr=0; EntityIDNr<AllEntityIDs.Size(); EntityIDNr++)
            {
                IntrusivePtrT<BaseEntityT> OtherEntity=static_pointer_cast<BaseEntityT>(GameWorld->GetGameEntityByID(AllEntityIDs[EntityIDNr]));

                if (OtherEntity.IsNull())  continue;
                if (OtherEntity->ID == ID) continue;    // We don't damage us ourselves.

                // Note that OtherOrigin=OtherEntity->GetOrigin() is not enough, it must be computed as shown in order to work in all cases:
                // a) With (e.g.) EntHumanPlayerTs, the Dimensions are static and the Origin moves, but
                // b) with EntRigidBodyTs, the Dimensions move while the Origin remains at (0, 0, 0).
                const Vector3dT OtherOrigin=OtherEntity->GetDimensions().GetCenter() + OtherEntity->GetOrigin();
                const Vector3dT Impact     =OtherOrigin - m_Origin;
                const double    Dist       =length(Impact);

                // if (Dist<5000.0) GameWorld->PrintDebug("OurID %2u, our Type %2u, Ent ID %2u, Type %2u, dist %.2f\n", ID, TypeID, OtherEntity->ID, OtherEntity->TypeID, Dist);

                     if (Dist< 40.0) OtherEntity->TakeDamage(this, 100                      , scale(Impact, 1.0/Dist));
                else if (Dist<200.0) OtherEntity->TakeDamage(this, 100-char((Dist-40.0)/1.6), scale(Impact, 1.0/Dist));
            }
        }
    }
    else
    {
        // Remove this entity (it exploded about 3 seconds ago).
        GameWorld->RemoveEntity(ID);
    }
}


namespace
{
    bool ParticleFunction_ARGrenadeExplosionMain(ParticleMST* Particle, float Time)
    {
        const float FPS   =20.0f;        // The default value is 20.0.
        const float MaxAge=55.0f/FPS;    // 55 frames at 20 FPS.

        Particle->RenderMat=ResMan.RenderMats[ResMan.PARTICLE_EXPLOSIONVERT_FRAME1+(unsigned long)(Particle->Age*FPS)];

        Particle->Age+=Time;
        if (Particle->Age>=MaxAge) return false;

        return true;
    }


    bool ParticleFunction_ARGrenadeExplosionSmall(ParticleMST* Particle, float Time)
    {
        const float MaxAge=3.0f;

        Particle->Age+=Time;
        if (Particle->Age>MaxAge) return false;

        Particle->Velocity[0]-=Particle->Velocity[0]*Time;  // Physically, this line is (mostly) nonsense.
        Particle->Velocity[1]-=Particle->Velocity[1]*Time;  // Physically, this line is (mostly) nonsense.
        Particle->Velocity[2]-=392.4f*Time;                 // v=a*t    9810.0 / 25.0 == 392.4

        Particle->Origin[0]+=Particle->Velocity[0]*Time;    // s=v*t
        Particle->Origin[1]+=Particle->Velocity[1]*Time;
        Particle->Origin[2]+=Particle->Velocity[2]*Time;

        if (Particle->Origin[2]<Particle->AuxData[0])
        {
            // Particle hit the ground.
            Particle->Origin[2]=Particle->AuxData[0];
            Particle->Velocity[2]=-Particle->Velocity[2]*0.5f;
        }

        Particle->Color[0]=char(255.0*(MaxAge-Particle->Age)/MaxAge);
        Particle->Color[1]=char(255.0*(MaxAge-Particle->Age)/MaxAge*(MaxAge-Particle->Age)/MaxAge);
        return true;
    }
}


void EntARGrenadeT::ProcessEvent(unsigned int /*EventType*/, unsigned int /*NumEvents*/)
{
    // We only receive a single event here ("Detonation!"), thus there is no need to look at 'EventID'.
    // Update sound position.
    m_FireSound->SetPosition(m_Origin);

    // Play the fire sound.
    m_FireSound->Play();

    // Register explosion particles.
    static ParticleMST NewParticle;

    NewParticle.Origin[0]=float(m_Origin.x);
    NewParticle.Origin[1]=float(m_Origin.y);
    NewParticle.Origin[2]=float(m_Origin.z+80.0-20.0);

    NewParticle.Age=0.0;
    NewParticle.Color[0]=255;
    NewParticle.Color[1]=255;
    NewParticle.Color[2]=255;
    NewParticle.Color[3]=255;

    NewParticle.Radius=40.0;
    NewParticle.StretchY=2.0;
    NewParticle.RenderMat=ResMan.RenderMats[ResMan.PARTICLE_EXPLOSIONVERT_FRAME1];
    NewParticle.MoveFunction=ParticleFunction_ARGrenadeExplosionMain;

    ParticleEngineMS::RegisterNewParticle(NewParticle);

    for (char i=0; i<20; i++)
    {
        NewParticle.Origin[0]=float(m_Origin.x);
        NewParticle.Origin[1]=float(m_Origin.y);
        NewParticle.Origin[2]=float(m_Origin.z);

        NewParticle.Velocity[0]=(rand()-int(RAND_MAX/2))/16.0f / 25.0f;
        NewParticle.Velocity[1]=(rand()-int(RAND_MAX/2))/16.0f / 25.0f;
        NewParticle.Velocity[2]=rand()/4.0f / 25.0f;

        NewParticle.Age=0.0;
        NewParticle.Color[0]=255;
        NewParticle.Color[1]=255;
        NewParticle.Color[2]=0;
        NewParticle.Color[3]=0;

        NewParticle.Radius=4.0;
        NewParticle.StretchY=1.0;
        NewParticle.RenderMat=ResMan.RenderMats[ResMan.PARTICLE_GENERIC1];
        NewParticle.MoveFunction=ParticleFunction_ARGrenadeExplosionSmall;
        NewParticle.AuxData[0]=float(m_Origin.z);

        ParticleEngineMS::RegisterNewParticle(NewParticle);
    }
}


bool EntARGrenadeT::GetLightSourceInfo(unsigned long& DiffuseColor, unsigned long& SpecularColor, VectorT& Position, float& Radius, bool& CastsShadows) const
{
    const float Duration=0.8f;

    if (m_LifeTime< 3.0f         ) return false;
    if (m_LifeTime>=3.0f+Duration) return false;

    const float Amount=1.0f-(m_LifeTime-3.0f)/Duration;

    const unsigned long Red  =(unsigned long)(0xFF*Amount);
    const unsigned long Green=(unsigned long)(0xFF*Amount*Amount);
    const unsigned long Blue =(unsigned long)(0xFF*Amount*Amount*Amount);

    DiffuseColor =(Blue << 16)+(Green << 8)+Red;
    SpecularColor=DiffuseColor;
    Position     =m_Origin;
    Radius       =400.0;
    CastsShadows =true;

    return true;
}


void EntARGrenadeT::Draw(bool /*FirstPersonView*/, float LodDist) const
{
    if (m_LifeTime>=3.0) return;

    AnimPoseT* Pose=m_Model->GetSharedPose(m_Model->GetAnimExprPool().GetStandard(0, 0.0f));
    Pose->Draw(-1 /*default skin*/, LodDist);
}
