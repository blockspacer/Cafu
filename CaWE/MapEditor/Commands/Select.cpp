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

#include "Select.hpp"

#include "../CompMapEntity.hpp"
#include "../MapDocument.hpp"
#include "../MapEntRepres.hpp"
#include "../MapPrimitive.hpp"


using namespace MapEditor;


CommandSelectT* CommandSelectT::Clear(MapDocumentT* MapDocument)
{
    ArrayT<MapElementT*> EmptySelection;

    return new CommandSelectT(MapDocument, MapDocument->GetSelection(), EmptySelection);
}


CommandSelectT* CommandSelectT::Add(MapDocumentT* MapDocument, const ArrayT<MapElementT*>& MapElements)
{
    ArrayT<MapElementT*> OldSelection(MapDocument->GetSelection());
    ArrayT<MapElementT*> NewSelection(MapDocument->GetSelection());

    // For each MapElement, check if it is already part of the current selection.
    for (unsigned long MapElemNr=0; MapElemNr<MapElements.Size(); MapElemNr++)
    {
        unsigned long SelectionNr=0;

        for (SelectionNr=0; SelectionNr<OldSelection.Size(); SelectionNr++)
            if (MapElements[MapElemNr]==OldSelection[SelectionNr]) break;

        // MapElement is not part of the current selection.
        if (SelectionNr==OldSelection.Size()) NewSelection.PushBack(MapElements[MapElemNr]);
    }

    return new CommandSelectT(MapDocument, OldSelection, NewSelection);
}


CommandSelectT* CommandSelectT::Add(MapDocumentT* MapDocument, MapElementT* MapElement)
{
    ArrayT<MapElementT*> AddSelection;
    AddSelection.PushBack(MapElement);

    return CommandSelectT::Add(MapDocument, AddSelection);
}


CommandSelectT* CommandSelectT::Remove(MapDocumentT* MapDocument, const ArrayT<MapElementT*>& MapElements)
{
    ArrayT<MapElementT*> NewSelection(MapDocument->GetSelection());

    // For each MapElement, check if it is already part of the current selection.
    for (unsigned long MapElemNr=0; MapElemNr<MapElements.Size(); MapElemNr++)
    {
        for (unsigned long SelectionNr=0; SelectionNr<NewSelection.Size(); SelectionNr++)
        {
            // MapElement is part of the current selection.
            if (MapElements[MapElemNr]==NewSelection[SelectionNr])
            {
                NewSelection.RemoveAtAndKeepOrder(SelectionNr);
                SelectionNr--; // The current position has to be checked again.
                break;
            }
        }
    }

    return new CommandSelectT(MapDocument, MapDocument->GetSelection(), NewSelection);
}


CommandSelectT* CommandSelectT::Remove(MapDocumentT* MapDocument, MapElementT* MapElement)
{
    ArrayT<MapElementT*> RemoveSelection;
    RemoveSelection.PushBack(MapElement);

    return CommandSelectT::Remove(MapDocument, RemoveSelection);
}


CommandSelectT* CommandSelectT::Set(MapDocumentT* MapDocument, const ArrayT<MapElementT*>& MapElements)
{
    return new CommandSelectT(MapDocument, MapDocument->GetSelection(), MapElements);
}


CommandSelectT* CommandSelectT::Set(MapDocumentT* MapDocument, MapElementT* MapElement)
{
    ArrayT<MapElementT*> SetSelection;
    SetSelection.PushBack(MapElement);

    return CommandSelectT::Set(MapDocument, SetSelection);
}


CommandSelectT* CommandSelectT::Set(MapDocumentT* MapDocument, const ArrayT< IntrusivePtrT<cf::GameSys::EntityT> >& Entities, bool WithEntPrims)
{
    ArrayT<MapElementT*> Elems;

    for (unsigned long EntNr = 0; EntNr < Entities.Size(); EntNr++)
    {
        IntrusivePtrT<CompMapEntityT> MapEnt = GetMapEnt(Entities[EntNr]);

        Elems.PushBack(MapEnt->GetRepres());

        if (WithEntPrims)
            for (unsigned long PrimNr = 0; PrimNr < MapEnt->GetPrimitives().Size(); PrimNr++)
                Elems.PushBack(MapEnt->GetPrimitives()[PrimNr]);
    }

    return CommandSelectT::Set(MapDocument, Elems);
}


CommandSelectT* CommandSelectT::Set(MapDocumentT* MapDocument, const ArrayT<MapPrimitiveT*>& Primitives)
{
    ArrayT<MapElementT*> Elems;

    for (unsigned long PrimNr = 0; PrimNr < Primitives.Size(); PrimNr++)
        Elems.PushBack(Primitives[PrimNr]);

    return CommandSelectT::Set(MapDocument, Elems);
}


CommandSelectT* CommandSelectT::Set(MapDocumentT* MapDocument, const ArrayT< IntrusivePtrT<cf::GameSys::EntityT> >& Entities, bool WithEntPrims, const ArrayT<MapPrimitiveT*>& Primitives)
{
    ArrayT<MapElementT*> Elems;

    for (unsigned long EntNr = 0; EntNr < Entities.Size(); EntNr++)
    {
        IntrusivePtrT<CompMapEntityT> MapEnt = GetMapEnt(Entities[EntNr]);

        Elems.PushBack(MapEnt->GetRepres());

        if (WithEntPrims)
            for (unsigned long PrimNr = 0; PrimNr < MapEnt->GetPrimitives().Size(); PrimNr++)
                Elems.PushBack(MapEnt->GetPrimitives()[PrimNr]);
    }

    for (unsigned long PrimNr = 0; PrimNr < Primitives.Size(); PrimNr++)
        Elems.PushBack(Primitives[PrimNr]);

    return CommandSelectT::Set(MapDocument, Elems);
}


// This function counts the individual elements in Selection -- elements that were selected "as a group" are only counted once.
static int CountMouseClicks(const ArrayT<MapElementT*>& Selection)
{
    int Count = 0;

    for (unsigned long ElemNr = 0; ElemNr < Selection.Size(); ElemNr++)
    {
        MapElementT*                  Elem = Selection[ElemNr];
        IntrusivePtrT<CompMapEntityT> Top  = Elem->GetTopmostGroupSel();

        // Skip all Elems that are part of a group (Top != NULL)
        // but are not the groups "repres" element (Top->GetRepres() != Elem).
        if (Top != NULL && Top->GetRepres() != Elem) continue;

        Count++;
    }

    return Count;
}


CommandSelectT::CommandSelectT(MapDocumentT* MapDocument, const ArrayT<MapElementT*>& OldSelection, const ArrayT<MapElementT*>& NewSelection)
    : CommandT(abs(CountMouseClicks(OldSelection)-CountMouseClicks(NewSelection)) > 3, false),  // Only show the selection command in the undo/redo history if the selection difference is greater 3.
      m_MapDocument(MapDocument),
      m_OldSelection(OldSelection),
      m_NewSelection(NewSelection)
{
}


CommandSelectT::~CommandSelectT()
{
}


bool CommandSelectT::Do()
{
    wxASSERT(!m_Done);

    if (m_Done) return false;

    m_MapDocument->SetSelection(m_NewSelection);

    m_MapDocument->UpdateAllObservers_SelectionChanged(m_OldSelection, m_NewSelection);

    m_Done=true;

    return true;
}


void CommandSelectT::Undo()
{
    wxASSERT(m_Done);

    if (!m_Done) return;

    m_MapDocument->SetSelection(m_OldSelection);

    m_MapDocument->UpdateAllObservers_SelectionChanged(m_NewSelection, m_OldSelection);

    m_Done=false;
}


wxString CommandSelectT::GetName() const
{
    return "Selection change";
}
