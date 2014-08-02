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

#ifndef CAFU_MODELEDITOR_SET_MESH_TANGENTSPACE_METHOD_HPP_INCLUDED
#define CAFU_MODELEDITOR_SET_MESH_TANGENTSPACE_METHOD_HPP_INCLUDED

#include "../../CommandPattern.hpp"
#include "Models/Model_cmdl.hpp"


namespace ModelEditor
{
    class ModelDocumentT;

    class CommandSetMeshTSMethodT : public CommandT
    {
        public:

        CommandSetMeshTSMethodT(ModelDocumentT* ModelDoc, unsigned int MeshNr, CafuModelT::MeshT::TangentSpaceMethodT NewTSMethod);

        // CommandT implementation.
        bool Do();
        void Undo();
        wxString GetName() const;


        private:

        ModelDocumentT*                              m_ModelDoc;
        const unsigned int                           m_MeshNr;
        const CafuModelT::MeshT::TangentSpaceMethodT m_NewTSMethod;
        const CafuModelT::MeshT::TangentSpaceMethodT m_OldTSMethod;
    };
}

#endif