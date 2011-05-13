/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2011 Carsten Fuchs Software.

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

#ifndef _MODEL_LOADER_HPP_
#define _MODEL_LOADER_HPP_

#include "Model_cmdl.hpp"


/// The base class for importing arbitrary model files into Cafu models.
/// The concrete derived classes determine which specific file format is imported.
class ModelLoaderT
{
    public:

    class UserCallbacksI;
    class LoadErrorT;


    /// The constructor.
    ModelLoaderT(const std::string& FileName);

    /// Returns the file name of the imported model.
    const std::string& GetFileName() const { return m_FileName; }

    /// Some (static, non-animated) model file formats may bring all their tangent space data with them.
    /// For such files, there is no need for the Cafu model to recompute that data from the vertices.
    /// This method lets the Cafu model know whether the fixed, given tangent space should be used,
    /// or whether the Cafu model should recompute the tangent space (possibly after animation) itself.
    virtual bool UseGivenTS() const=0;

    /// Actually loads the file data into the appropriate parts of the Cafu model.
    virtual void Load(ArrayT<CafuModelT::JointT>& Joints, ArrayT<CafuModelT::MeshT>& Meshes, ArrayT<CafuModelT::AnimT>& Anims, MaterialManagerImplT& MaterialMan)=0;

    /// Loads the locations where GUIs can be attached to the Cafu model.
    virtual void Load(ArrayT<CafuModelT::GuiLocT>& GuiLocs)=0;


    protected:

    /// An auxiliary function that computes the bounding box for the model with the given
    /// joints and meshes at the given anim sequence at the given frame number.
    BoundingBox3fT GetBB(const ArrayT<CafuModelT::JointT>& Joints, const ArrayT<CafuModelT::MeshT>& Meshes, const CafuModelT::AnimT& Anim, unsigned long FrameNr) const;

    /// An auxiliary function that cleans loaded meshes.
    void Clean(ArrayT<CafuModelT::MeshT>& Meshes);

    /// An auxiliary function that creates and returns a fail-safe wire-frame material with the given name,
    /// for use when a material with more detailed or more specific settings is not available.
    MaterialT CreateDefaultMaterial(const std::string& MatName, bool EditorSave=true) const;

    const std::string m_FileName;
};


/// An interface for user callbacks.
/// A concrete model loader may require e.g. asking the user for a password to open the file.
/// The calling code can implement this interface and pass it to the concrete loader in order to achieve the desired functionality.
class ModelLoaderT::UserCallbacksI
{
    public:

    /// Asks the user for a password to open the model file.
    /// @returns The entered password, or the empty string for cancel/none.
    virtual std::string GetPasswordFromUser(const std::string& Message, const std::string& Caption="Enter password")=0;

    // /// Returns an output stream for any log messages that the loader wants to present to the user.
    // std::ostream& GetLog()=0;

    // /// Tells the implementation how much of the model has already been loaded.
    // void SetProgress(float Percent)=0;
};


/// A class that is thrown on model load errors.
class ModelLoaderT::LoadErrorT : public std::runtime_error
{
    public:

    LoadErrorT(const std::string& Message);
};

#endif
