/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2012 Carsten Fuchs Software.

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

#ifndef CAFU_GUISYS_ALL_COMPONENTS_HPP_INCLUDED
#define CAFU_GUISYS_ALL_COMPONENTS_HPP_INCLUDED

#include "TypeSys.hpp"


namespace cf
{
    namespace GuiSys
    {
        class WindowT;


        /// All classes in the ComponentBaseT hierarchy must register their TypeInfoT
        /// members with this TypeInfoManT instance.
        cf::TypeSys::TypeInfoManT& GetComponentTIM();


        /// Creation parameters for window components.
        class ComponentCreateParamsT : public cf::TypeSys::CreateParamsT
        {
            public:

            /// The constructor.
            /// @param Window   The window that the new component becomes a part of.
            ComponentCreateParamsT(WindowT& Window);

            WindowT& m_Window;  ///< The window that the new component becomes a part of.
        };
    }
}

#endif
