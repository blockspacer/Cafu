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

#include "SetCompVar.hpp"
#include "../GuiDocument.hpp"
#include "GuiSys/Window.hpp"
#include "Variables.hpp"


using namespace GuiEditor;


template<class T>
CommandSetCompVarT<T>::CommandSetCompVarT(GuiDocumentT* GuiDoc, cf::TypeSys::VarT<T>& Var, const T& NewValue)
    : m_GuiDoc(GuiDoc),
      m_Var(Var),
      m_OldValue(Var.Get()),
      m_NewValue(NewValue)
{
}


template<class T>
bool CommandSetCompVarT<T>::Do()
{
    wxASSERT(!m_Done);
    if (m_Done) return false;

    // If the new value isn't different from the old value, don't put this command into the command history.
    if (m_NewValue == m_OldValue) return false;

    m_Var.Set(m_NewValue);

    m_GuiDoc->UpdateAllObservers_Modified(m_Var);
    m_Done=true;
    return true;
}


template<class T>
void CommandSetCompVarT<T>::Undo()
{
    wxASSERT(m_Done);
    if (!m_Done) return;

    m_Var.Set(m_OldValue);

    m_GuiDoc->UpdateAllObservers_Modified(m_Var);
    m_Done=false;
}


template<class T>
wxString CommandSetCompVarT<T>::GetName() const
{
    return wxString("Set ") + m_Var.GetName();
}


template class CommandSetCompVarT<float>;
template class CommandSetCompVarT<double>;
template class CommandSetCompVarT<int>;
template class CommandSetCompVarT<std::string>;
