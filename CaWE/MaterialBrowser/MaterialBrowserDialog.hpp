/*
=================================================================================
This file is part of Cafu, the open-source game and graphics engine for
multiplayer, cross-platform, real-time 3D action.
$Id$

Copyright (C) 2002-2010 Carsten Fuchs Software.

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

#ifndef _MATERIAL_BROWSER_DIALOG_HPP_
#define _MATERIAL_BROWSER_DIALOG_HPP_

#include "Templates/Array.hpp"

#include "wx/dialog.h"
#include "wx/aui/framemanager.h"


class EditorMaterialI;
class ScrolledMaterialWindowT;
class ControlsBarT;
class MaterialTreeT;
class MaterialPropertiesT;
class FilterSettingsT;
class wxWindow;
namespace MaterialBrowser { class DocAccessI; }

using namespace MaterialBrowser;    // TODO: Should instead move other related classes (like MaterialBrowserDialogT) into this namespace as well!


class MaterialBrowserDialogT : public wxDialog
{
    public:

    /// The constructor.
    MaterialBrowserDialogT(wxWindow* Parent, const DocAccessI& DocAccess, EditorMaterialI* InitialMaterial, const wxString& InitialNameFilter_, bool OnlyShowUsed_);

    /// The destructor.
    ~MaterialBrowserDialogT();

    /// Returns the currently selected material.
    EditorMaterialI* GetCurrentMaterial() const;


    private:

    const DocAccessI&         m_DocAccess;          ///< The (access interface to the) document that this dialog is for.
    wxAuiManager              m_AUIManager;

    ScrolledMaterialWindowT*  m_ScrolledMatWin;
    ControlsBarT*             m_ControlsBar;
    MaterialTreeT*            m_MaterialTree;
    MaterialPropertiesT*      m_MaterialProperties;
    FilterSettingsT*          m_FilterSettings;

    wxString                  MatFolderFilter;

    int                       DisplaySize;          ///< Maintained by the OnChoice_DisplaySize handler.
    EditorMaterialI*          m_CurrentMaterial;    ///< Currently selected material.
    ArrayT<EditorMaterialI*>* m_UsedMaterialsList;  ///< Maintained by the OnCheckbox_OnlyShowUsed handler.


    const ArrayT<wxString>& GetNameFilterHistory();
    void SelectMaterial(EditorMaterialI* Material);


    // Helper functions.
    void Init(const ArrayT<EditorMaterialI*>& Materials, const wxString& InitialNameFilter, bool OnlyShowUsed);
    void SaveAndQuitDialog(int ReturnValue);

    // Event handlers.
    void OnChoice_DisplaySize(wxCommandEvent& Event);
    void OnButton_Mark(wxCommandEvent& Event);
    void OnButton_Replace(wxCommandEvent& Event);
    void OnButton_ExportDiffMaps(wxCommandEvent& Event);
    void OnButton_Cancel(wxCommandEvent& Event);
    void OnCombobox_NameFilterSelection(wxCommandEvent& Event);
    void OnCombobox_NameFilterTextChange(wxCommandEvent& Event);
    void OnCheckbox_OnlyShowUsed(wxCommandEvent& Event);
    void OnCheckbox_OnlyShowEditor(wxCommandEvent& Event);

    // IDs for the controls in whose events we are interested.
    enum
    {
        ID_SCROLLED_MaterialWindow=wxID_HIGHEST+1,
        ID_CHOICE_DisplaySize,
        ID_BUTTON_Mark,
        ID_BUTTON_Replace,
        ID_BUTTON_ExportDiffMaps,
        ID_COMBO_NameFilter,
        ID_COMBO_KeywordFilter,
        ID_CHECKBOX_OnlyShowUsed,
        ID_CHECKBOX_OnlyShowEditor,
    };

    DECLARE_EVENT_TABLE()

    friend class ScrolledMaterialWindowT;
    friend class ControlsBarT;
    friend class FilterSettingsT;
    friend class MaterialTreeT;
};

#endif
