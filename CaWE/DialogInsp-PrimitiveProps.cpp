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

#include "DialogInsp-PrimitiveProps.hpp"
#include "CompMapEntity.hpp"
#include "MapBrush.hpp"
#include "MapBezierPatch.hpp"
#include "MapDocument.hpp"
#include "MapModel.hpp"
#include "MapPlant.hpp"
#include "MapTerrain.hpp"
#include "GameConfig.hpp"

#include "MapCommands/ChangePlantSeed.hpp"
#include "MapCommands/ChangePlantDescr.hpp"
#include "MapCommands/ModifyModel.hpp"
#include "MapCommands/SetBPSubdivs.hpp"

#include "wx/notebook.h"
#include "wx/propgrid/propgrid.h"
#include "wx/propgrid/manager.h"
#include "wx/propgrid/advprops.h"

#include "Models/Model_cmdl.hpp"
#include "TypeSys.hpp"


class GameFilePropertyT : public wxLongStringProperty
{
    public:

    GameFilePropertyT(const wxString& name=wxPG_LABEL,
                      const wxString& label=wxPG_LABEL,
                      const wxString& value=wxEmptyString,
                      MapDocumentT* MapDoc=NULL,
                      wxString Filter="All files (*.*)|*.*",
                      wxString SubDir=wxEmptyString)
        : wxLongStringProperty(name, label, value),
          m_MapDoc(MapDoc),
          m_Filter(Filter),
          m_SubDir(SubDir)
    {
    }

    // Shows the file selection dialog and makes the choosen file path relative.
    virtual bool OnButtonClick(wxPropertyGrid* propGrid, wxString& value)
    {
        wxString InitialDir =m_MapDoc->GetGameConfig()->ModDir+m_SubDir;
        wxString FileNameStr=wxFileSelector("Please select a file", InitialDir, "", "", m_Filter, wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if (FileNameStr=="") return false;

        wxFileName FileName(FileNameStr);
        FileName.MakeRelativeTo(m_MapDoc->GetGameConfig()->ModDir);
        value=FileName.GetFullPath(wxPATH_UNIX);
        return true;
    }


    private:

    MapDocumentT* m_MapDoc;
    wxString      m_Filter;
    wxString      m_SubDir;
};


wxSizer* InspDlgPrimitivePropsT::InspectorPrimitivePropsInit(wxWindow* parent, bool call_fit, bool set_sizer)
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    m_SelectionText=new wxStaticText(parent, -1, wxT("No map primitve is selected."), wxDefaultPosition, wxDefaultSize, 0);
    item0->Add(m_SelectionText, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2);

    // BEGIN CUSTOM code, not generated by wxDesigner.

    m_PropMan=new wxPropertyGridManager(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_BOLD_MODIFIED | wxPG_SPLITTER_AUTO_CENTER | wxPG_TOOLBAR | wxPG_DESCRIPTION);
    m_PropMan->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS | wxPG_EX_MODE_BUTTONS);

    m_PropMan->AddPage("Properties");

    // END CUSTOM code, not generated by wxDesigner.

    item0->Add( m_PropMan, 1, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 2 );

    if (set_sizer)
    {
        parent->SetSizer( item0 );
        if (call_fit)
            item0->SetSizeHints( parent );
    }

    return item0;
}


InspDlgPrimitivePropsT::InspDlgPrimitivePropsT(wxWindow* Parent, MapDocumentT* MapDoc)
    : wxPanel(Parent),
      m_MapDoc(MapDoc),
      m_PropMan(NULL),
      m_SelectionText(NULL),
      m_IsRecursiveSelfNotify(false)
{
    wxPropertyGridInterface::RegisterAdditionalEditors();

    InspectorPrimitivePropsInit(this);
    Bind(wxEVT_PG_CHANGED, &InspDlgPrimitivePropsT::OnPropertyGridChanged, this);
    m_MapDoc->RegisterObserver(this);
}


InspDlgPrimitivePropsT::~InspDlgPrimitivePropsT()
{
    m_PropMan->Clear();
    if (m_MapDoc) m_MapDoc->UnregisterObserver(this);
}


void InspDlgPrimitivePropsT::NotifySubjectChanged_Selection(SubjectT* Subject, const ArrayT<MapElementT*>& OldSelection, const ArrayT<MapElementT*>& NewSelection)
{
    if (m_IsRecursiveSelfNotify) return;

    UpdateGrid();
}


void InspDlgPrimitivePropsT::NotifySubjectChanged_Deleted(SubjectT* Subject, const ArrayT<MapPrimitiveT*>& Primitives)
{
    // No need to act on deleted objects.
    // If the deletion of an object changes the current selection the inspector is notified in the NotifySubjectChanged_Selection method.
}


void InspDlgPrimitivePropsT::NotifySubjectChanged_Modified(SubjectT* Subject, const ArrayT<MapElementT*>& MapElements, MapElemModDetailE Detail)
{
    if (m_IsRecursiveSelfNotify) return;

    if (Detail!=MEMD_PRIMITIVE_PROPS_CHANGED && Detail!=MEMD_GENERIC) return;

    for (unsigned long i=0; i<MapElements.Size(); i++)
    {
        if (MapElements[i]->IsSelected())
        {
            UpdateGrid();
            break;
        }
    }
}


void InspDlgPrimitivePropsT::NotifySubjectChanged_Modified(SubjectT* Subject, const ArrayT<MapElementT*>& MapElements, MapElemModDetailE Detail, const ArrayT<BoundingBox3fT>& OldBounds)
{
    if (m_IsRecursiveSelfNotify) return;

    if (Detail!=MEMD_PRIMITIVE_PROPS_CHANGED) return;

    for (unsigned long i=0; i<MapElements.Size(); i++)
    {
        if (MapElements[i]->IsSelected())
        {
            UpdateGrid();
            break;
        }
    }
}


void InspDlgPrimitivePropsT::NotifySubjectDies(SubjectT* dyingSubject)
{
    wxASSERT(dyingSubject==m_MapDoc);
    m_MapDoc=NULL;
}


void InspDlgPrimitivePropsT::UpdateGrid()
{
    ArrayT<MapPrimitiveT*> SelectedPrimitives;

    // Find the map primitives in the selection (ignore all entities).
    // There is no need to bother with the entities, as it's up to the selection tool
    // to explicitly add their items/children to the selection - or not.
    for (unsigned long i=0; i<m_MapDoc->GetSelection().Size(); i++)
    {
        MapPrimitiveT* Prim=dynamic_cast<MapPrimitiveT*>(m_MapDoc->GetSelection()[i]);

        if (Prim)
            SelectedPrimitives.PushBack(Prim);
    }

    // Update the selection description text.
    if (SelectedPrimitives.Size()==0)
        m_SelectionText->SetLabel("No map primitive selected.");
    else if (SelectedPrimitives.Size()==1)
        m_SelectionText->SetLabel("1 map primitive selected.");
    else
        m_SelectionText->SetLabel(wxString::Format("%lu map primitives selected.", SelectedPrimitives.Size()));

    m_PropMan->ClearSelection();    // Clear the property grid.
    m_PropMan->ClearPage(0);        // At the moment we only got one page, so we only need to delete this page.

    static const wxColour HEADING_COLOR("#CCCCCC");

    // Create a category for every selected map primitve and fill it with the primitves properties.
    for (unsigned long i = 0; i < SelectedPrimitives.Size(); i++)
    {
        const int      EntityNr = m_MapDoc->GetEntities().Find(SelectedPrimitives[i]->GetParent());
        const int      ItemNr   = EntityNr >= 0 ? m_MapDoc->GetEntities()[EntityNr]->GetPrimitives().Find(SelectedPrimitives[i]) : -1;
        const wxString ItemStr  = wxString::Format(" (item %i in entity %i)", ItemNr, EntityNr);

        if (dynamic_cast<MapBrushT*>(SelectedPrimitives[i])!=NULL)
        {
            wxPGProperty* Cat=m_PropMan->Append(new wxStringProperty("Brush" + ItemStr, wxPG_LABEL, "<composed>"));

            // Must do this last, as new children pick up the color of their parent.
            m_PropMan->SetPropertyBackgroundColour(Cat, HEADING_COLOR, 0 /*don't recurse*/);
        }

        if (dynamic_cast<MapBezierPatchT*>(SelectedPrimitives[i])!=NULL)
        {
            wxPGProperty* Cat=m_PropMan->Append(new wxStringProperty("Bezier Patch" + ItemStr, wxPG_LABEL, "<composed>"));
            MapBezierPatchT* BP=(MapBezierPatchT*)SelectedPrimitives[i];

            wxPGProperty* SdHorzIntProp=new wxIntProperty("Subdivs horz", wxPG_LABEL, BP->GetSubdivsHorz());
            m_PropMan->AppendIn(Cat, SdHorzIntProp)->SetClientData(BP);
            m_PropMan->SetPropertyEditor(SdHorzIntProp, wxPGEditor_SpinCtrl);

            wxPGProperty* SdVertIntProp=new wxIntProperty("Subdivs vert", wxPG_LABEL, BP->GetSubdivsVert());
            m_PropMan->AppendIn(Cat, SdVertIntProp)->SetClientData(BP);
            m_PropMan->SetPropertyEditor(SdVertIntProp, wxPGEditor_SpinCtrl);

            // Must do this last, as new children pick up the color of their parent.
            m_PropMan->SetPropertyBackgroundColour(Cat, HEADING_COLOR, 0 /*don't recurse*/);
        }

        if (dynamic_cast<MapTerrainT*>(SelectedPrimitives[i])!=NULL)
        {
            wxPGProperty* Cat=m_PropMan->Append(new wxStringProperty("Terrain" + ItemStr, wxPG_LABEL, "<composed>"));

            // Must do this last, as new children pick up the color of their parent.
            m_PropMan->SetPropertyBackgroundColour(Cat, HEADING_COLOR, 0 /*don't recurse*/);
        }

        if (dynamic_cast<MapPlantT*>(SelectedPrimitives[i])!=NULL)
        {
            wxPGProperty* Cat=m_PropMan->Append(new wxStringProperty("Plant" + ItemStr, wxPG_LABEL, "<composed>"));
            MapPlantT* Plant=(MapPlantT*)SelectedPrimitives[i];

            m_PropMan->AppendIn(Cat, new GameFilePropertyT("Plant Description", wxPG_LABEL, Plant->m_DescrFileName, m_MapDoc, "Plant Description Files (*.cpd)|*.cpd|All Files (*.*)|*.*", "/Plants/"))->SetClientData(Plant);
            m_PropMan->AppendIn(Cat, new wxIntProperty("Random Seed", wxPG_LABEL, Plant->m_RandomSeed))->SetClientData(Plant);

            // Must do this last, as new children pick up the color of their parent.
            m_PropMan->SetPropertyBackgroundColour(Cat, HEADING_COLOR, 0 /*don't recurse*/);
        }

        if (dynamic_cast<MapModelT*>(SelectedPrimitives[i])!=NULL)
        {
            wxPGProperty* Cat=m_PropMan->Append(new wxStringProperty("Model" + ItemStr, wxPG_LABEL, "<composed>"));
            MapModelT* Model=(MapModelT*)SelectedPrimitives[i];

            m_PropMan->AppendIn(Cat, new GameFilePropertyT("Model",           wxPG_LABEL, Model->m_Model->GetFileName(), m_MapDoc, "All Files (*.*)|*.*|Model files (*.mdl)|*.mdl|Model Files (*.ase)|*.ase|Model Files (*.dlod)|*.dlod", "/Models/"))->SetClientData(Model);
            m_PropMan->AppendIn(Cat, new GameFilePropertyT("Collision Model", wxPG_LABEL, Model->m_CollModelFileName, m_MapDoc, "Collision Model (*.cmap)|*.cmap|All Files (*.*)|*.*", "/Models/"))->SetClientData(Model);
            m_PropMan->AppendIn(Cat, new wxStringProperty ("Label",           wxPG_LABEL, Model->m_Label))->SetClientData(Model);
            m_PropMan->AppendIn(Cat, new wxFloatProperty  ("Scale",           wxPG_LABEL, Model->m_Scale))->SetClientData(Model);
            m_PropMan->AppendIn(Cat, new wxIntProperty    ("Sequence Number", wxPG_LABEL, Model->m_AnimExpr->GetSequNr()))->SetClientData(Model);
            m_PropMan->AppendIn(Cat, new wxFloatProperty  ("Frame Offset",    wxPG_LABEL, Model->m_FrameOffset))->SetClientData(Model);
            m_PropMan->AppendIn(Cat, new wxFloatProperty  ("Frame Scale",     wxPG_LABEL, Model->m_FrameTimeScale))->SetClientData(Model);

            wxPGProperty* AnimateProp=m_PropMan->AppendIn(Cat, new wxBoolProperty("Animated", wxPG_LABEL, Model->m_Animated));
            AnimateProp->SetClientData(Model);
            m_PropMan->SetPropertyAttributeAll(wxPG_BOOL_USE_CHECKBOX, true); // Use checkbox instead of choice.

            // Must do this last, as new children pick up the color of their parent.
            m_PropMan->SetPropertyBackgroundColour(Cat, HEADING_COLOR, 0 /*don't recurse*/);
        }
    }

    m_PropMan->RefreshGrid();
}


void InspDlgPrimitivePropsT::OnPropertyGridChanged(wxPropertyGridEvent& Event)
{
    MapElementT* Object=(MapElementT*)Event.GetProperty()->GetClientData();

    if (Object==NULL) return;

    if (Object->GetType()==&MapBrushT::TypeInfo)
    {
        // TODO.
    }
    else if (Object->GetType()==&MapBezierPatchT::TypeInfo)
    {
        MapBezierPatchT*    BP      =(MapBezierPatchT*)Object;
        const wxPGProperty* Prop    =Event.GetProperty();
        const wxString      PropName=Prop->GetName().AfterLast('.');
        CommandT*           Command =NULL;

        if (PropName=="Subdivs horz")
            Command=new CommandSetBPSubdivsT(m_MapDoc, BP, Prop->GetValue().GetLong(), HORIZONTAL);
        else if (PropName=="Subdivs vert")
            Command=new CommandSetBPSubdivsT(m_MapDoc, BP, Prop->GetValue().GetLong(), VERTICAL);

        if (Command)
        {
            m_IsRecursiveSelfNotify=true;
            m_MapDoc->GetHistory().SubmitCommand(Command);
            m_IsRecursiveSelfNotify=false;
        }
    }
    else if (Object->GetType()==&MapTerrainT::TypeInfo)
    {
        // TODO.
    }
    else if (Object->GetType()==&MapPlantT::TypeInfo)
    {
        MapPlantT* Plant=(MapPlantT*)Object;

        // Property names are of the kind "MapPlantT[NUMBER].RandomSeed" to identify different map primitives with the
        // same properties. Since we have a pointer identifying the map primitive to modify, we can cut of the first
        // part of the property name.
        if (Event.GetProperty()->GetName().AfterLast('.')=="Random Seed")
        {
            // Command to change the random seed.
            CommandT* Command=new CommandChangePlantSeedT(*m_MapDoc, Plant, Event.GetProperty()->GetValue().GetLong());

            m_IsRecursiveSelfNotify=true;
            m_MapDoc->GetHistory().SubmitCommand(Command);
            m_IsRecursiveSelfNotify=false;
        }

        if (Event.GetProperty()->GetName().AfterLast('.')=="Plant Description")
        {
            // Command to change the plant description.
            CommandT* Command=new CommandChangePlantDescrT(*m_MapDoc, Plant, Event.GetProperty()->GetValueAsString());

            m_IsRecursiveSelfNotify=true;
            m_MapDoc->GetHistory().SubmitCommand(Command);
            m_IsRecursiveSelfNotify=false;
        }
    }
    else if (Object->GetType()==&MapModelT::TypeInfo)
    {
        const wxPGProperty* Prop       =Event.GetProperty();
        const wxString      PropName   =Prop->GetName().AfterLast('.');
        double              PropValueD =0.0;
        const float         PropValueF =Prop->GetValue().Convert(&PropValueD) ? float(PropValueD) : 0.0f;

        MapModelT*          Model   =(MapModelT*)Object;
        CommandT*           Command =NULL;

        if (PropName=="Model")
            Command=new CommandModifyModelT(*m_MapDoc, Model, Prop->GetValueAsString(), Model->m_CollModelFileName, Model->m_Label, Model->m_Scale, Model->m_AnimExpr, Model->m_FrameOffset, Model->m_FrameTimeScale, Model->m_Animated);
        else if (PropName=="Collision Model")
            Command=new CommandModifyModelT(*m_MapDoc, Model, Model->m_ModelFileName, Prop->GetValueAsString(), Model->m_Label, Model->m_Scale, Model->m_AnimExpr, Model->m_FrameOffset, Model->m_FrameTimeScale, Model->m_Animated);
        else if (PropName=="Label")
            Command=new CommandModifyModelT(*m_MapDoc, Model, Model->m_ModelFileName, Model->m_CollModelFileName, Prop->GetValueAsString(), Model->m_Scale, Model->m_AnimExpr, Model->m_FrameOffset, Model->m_FrameTimeScale, Model->m_Animated);
        else if (PropName=="Scale")
            Command=new CommandModifyModelT(*m_MapDoc, Model, Model->m_ModelFileName, Model->m_CollModelFileName, Model->m_Label, PropValueF, Model->m_AnimExpr, Model->m_FrameOffset, Model->m_FrameTimeScale, Model->m_Animated);
        else if (PropName=="Sequence Number")
            Command=new CommandModifyModelT(*m_MapDoc, Model, Model->m_ModelFileName, Model->m_CollModelFileName, Model->m_Label, Model->m_Scale, Model->m_Model->GetAnimExprPool().GetStandard(Prop->GetValue().GetLong(), 0.0f), Model->m_FrameOffset, Model->m_FrameTimeScale, Model->m_Animated);
        else if (PropName=="Frame Offset")
            Command=new CommandModifyModelT(*m_MapDoc, Model, Model->m_ModelFileName, Model->m_CollModelFileName, Model->m_Label, Model->m_Scale, Model->m_AnimExpr, PropValueF, Model->m_FrameTimeScale, Model->m_Animated);
        else if (PropName=="Frame Scale")
            Command=new CommandModifyModelT(*m_MapDoc, Model, Model->m_ModelFileName, Model->m_CollModelFileName, Model->m_Label, Model->m_Scale, Model->m_AnimExpr, Model->m_FrameOffset, PropValueF, Model->m_Animated);
        else if (PropName=="Animated")
            Command=new CommandModifyModelT(*m_MapDoc, Model, Model->m_ModelFileName, Model->m_CollModelFileName, Model->m_Label, Model->m_Scale, Model->m_AnimExpr, Model->m_FrameOffset, Model->m_FrameTimeScale, Prop->GetValue().GetBool());

        if (Command)
        {
            m_IsRecursiveSelfNotify=true;
            m_MapDoc->GetHistory().SubmitCommand(Command);
            m_IsRecursiveSelfNotify=false;
        }
    }
    else
        // Should never happen.
        wxASSERT(false);
}
