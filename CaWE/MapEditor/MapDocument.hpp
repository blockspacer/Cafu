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

#ifndef CAFU_MAP_DOCUMENT_HPP_INCLUDED
#define CAFU_MAP_DOCUMENT_HPP_INCLUDED

#include "ObserverPattern.hpp"
#include "../DocumentAdapter.hpp"

#include "Plants/PlantDescrMan.hpp"
#include "Templates/Array.hpp"
#include "Templates/Pointer.hpp"
#include "SceneGraph/LightMapMan.hpp"
#include "UniScriptState.hpp"

#include "wx/event.h"


class ChildFrameT;
class CommandT;
class EditorMaterialI;
class GameConfigT;
class MapPrimitiveT;
class OrthoBspTreeT;
class wxProgressDialog;
namespace MapEditor { class CompMapEntityT; }
namespace cf { namespace GameSys { class EntityT; } }
namespace cf { namespace GameSys { class WorldT; } }


struct PtsPointT
{
    float          Time;
    Vector3fT      Pos;
    unsigned short Heading;
    wxString       Info;
};


/// This class represents a CaWE "map" document.
class MapDocumentT : public wxEvtHandler, public SubjectT
{
    public:

    static const unsigned int CMAP_FILE_VERSION;


    /// The constructor for creating a new, clean, empty map.
    /// @param GameConfig   The game configuration that will be used for the map.
    MapDocumentT(GameConfigT* GameConfig);

    /// The regular constructor for loading a cmap file from disk.
    /// @param GameConfig       The game configuration that will be used for the map.
    /// @param ProgressDialog   If non-NULL, this dialog is used to show the progress while loading the map.
    /// @param FileName         The name of the file to load the map from.
    MapDocumentT(GameConfigT* GameConfig, wxProgressDialog* ProgressDialog, const wxString& FileName);

    /// A named constructor for importing a map in HL1 map file format.
    /// @param GameConfig       The game configuration that will be used for the map.
    /// @param ProgressDialog   If non-NULL, this dialog is used to show the progress while loading the map.
    /// @param FileName         The name of the file to load the map from.
    static MapDocumentT* ImportHalfLife1Map(GameConfigT* GameConfig, wxProgressDialog* ProgressDialog, const wxString& FileName);

    /// A named constructor for importing a map in HL2 vmf file format.
    /// @param GameConfig       The game configuration that will be used for the map.
    /// @param ProgressDialog   If non-NULL, this dialog is used to show the progress while loading the map.
    /// @param FileName         The name of the file to load the map from.
    static MapDocumentT* ImportHalfLife2Vmf(GameConfigT* GameConfig, wxProgressDialog* ProgressDialog, const wxString& FileName);

    /// A named constructor for importing a map in Doom3 map file format.
    /// @param GameConfig       The game configuration that will be used for the map.
    /// @param ProgressDialog   If non-NULL, this dialog is used to show the progress while loading the map.
    /// @param FileName         The name of the file to load the map from.
    static MapDocumentT* ImportDoom3Map(GameConfigT* GameConfig, wxProgressDialog* ProgressDialog, const wxString& FileName);

    /// The destructor.
    ~MapDocumentT();


    // Inherited methods from the wxDocument base class.
    bool OnSaveDocument(const wxString& FileName, bool IsAutoSave); ///< Saves the document. Non-const, as it updates the m_FileName member.
    bool SaveAs();                                                  ///< Calls OnSaveDocument().
    bool Save();                                                    ///< Calls OnSaveDocument().

    void                           SetChildFrame(ChildFrameT* ChildFrame) { m_ChildFrame=ChildFrame; }   // This should be in the ctor!
    ChildFrameT*                   GetChildFrame() const { return m_ChildFrame; }
    const wxString&                GetFileName() const   { return m_FileName; }
    MapDocAdapterT&                GetAdapter() { return m_DocAdapter; }
    cf::GameSys::WorldT&           GetScriptWorld() { return *m_ScriptWorld; }

    /// For compatibility only. This method should be removed.
    bool CompatSubmitCommand(CommandT* Command);

    /// Returns all entities that exist in this map. The world entity is always first at index 0, followed by an arbitrary number of "regular" entities.
    const ArrayT< IntrusivePtrT<MapEditor::CompMapEntityT> >& GetEntities() const;

    /// Returns the root "map" entity of the map.
    IntrusivePtrT<MapEditor::CompMapEntityT> GetRootMapEntity() const;

    /// Adds all elements in this map (entity representations and primitives) to the given array.
    /// The `MapEntRepresT` instance of the world entity is always the first element that is added to the list.
    void GetAllElems(ArrayT<MapElementT*>& Elems) const;

    /// Inserts the given entity into the map.
    /// Callers should never attempt to insert an element into the world in a way other than calling this method,
    /// as it also inserts the element into the internal BSP tree that is used for rendering and culling.
    void Insert(IntrusivePtrT<cf::GameSys::EntityT> Entity, IntrusivePtrT<cf::GameSys::EntityT> Parent, unsigned long Pos=0xFFFFFFFF);

    /// Inserts the given primitive into the map, as a child of the given entity (the world or a custom entity).
    /// Callers should never attempt to insert an element into the world in a way other than calling this method,
    /// as it also inserts the element into the internal BSP tree that is used for rendering and culling.
    void Insert(MapPrimitiveT* Prim, IntrusivePtrT<MapEditor::CompMapEntityT> ParentEnt);

    /// Removes the given entity from the map.
    /// The entity cannot be the root entity (the world).
    void Remove(IntrusivePtrT<cf::GameSys::EntityT> Entity);

    /// Removes the given primitive from the map.
    void Remove(MapPrimitiveT* Prim);

    ArrayT<MapElementT*> GetElementsIn(const BoundingBox3fT& Box, bool InsideOnly, bool CenterOnly) const;

    /// Determines all materials that are currently being used in the world (in brushes, Bezier patches and terrains),
    /// and returns the whole list via the UsedMaterials reference parameter.
    void GetUsedMaterials(ArrayT<EditorMaterialI*>& UsedMaterials) const;

    OrthoBspTreeT*                 GetBspTree() const    { return m_BspTree; }
    GameConfigT*                   GetGameConfig() const { return m_GameConfig; }
    cf::SceneGraph::LightMapManT&  GetLightMapMan()      { return m_LightMapMan; }
    PlantDescrManT&                GetPlantDescrMan()    { return m_PlantDescrMan; }

    const ArrayT<PtsPointT>& GetPointFilePoints() const { return m_PointFilePoints; }
    const ArrayT<wxColour>&  GetPointFileColors() const { return m_PointFileColors; }

    bool      IsSnapEnabled() const   { return m_SnapToGrid; }      ///< Returns whether or not grid snap is enabled. Called by the tools and views to determine snap behavior.
    int       GetGridSpacing() const  { return m_GridSpacing>0 ? m_GridSpacing : 1; }
    bool      Is2DGridEnabled() const { return m_ShowGrid; }
    float     SnapToGrid(float f, bool Toggle) const;                               ///< Returns the given number f   snapped to the grid if the grid is active, rounded to the nearest integer otherwise. If Toggle is true, the grid activity is considered toggled.
    Vector3fT SnapToGrid(const Vector3fT& Pos, bool Toggle, int AxisNoSnap) const;  ///< Returns the given vector Pos snapped to the grid if the grid is active, rounded to the nearest integer otherwise. If Toggle is true, the grid activity is considered toggled. If AxisNoSnap is -1, all components of Pos are snapped to the grid. If AxisNoSnap is 0, 1 or 2, the corresponding component of Pos is returned unchanged.

    /// Methods for managing the set of currently selected map elements.
    //@{
    void                        SetSelection(const ArrayT<MapElementT*>& NewSelection);
    const ArrayT<MapElementT*>& GetSelection() const { return m_Selection; }
    ArrayT< IntrusivePtrT<cf::GameSys::EntityT> > GetSelectedEntities() const;
    const BoundingBox3fT&       GetMostRecentSelBB() const;     ///< Returns the most recent bounding-box of the selection. That is, it returns the bounding-box of the current selection, or (if nothing is selected) the bounding-box of the previous selection.
    //@}


    private:

    MapDocumentT(const MapDocumentT&);          ///< Use of the Copy    Constructor is not allowed.
    void operator = (const MapDocumentT&);      ///< Use of the Assignment Operator is not allowed.

    void Init();
    void PostLoadEntityAlign(unsigned int cmapFileVersion, const ArrayT< IntrusivePtrT<MapEditor::CompMapEntityT> >& AllMapEnts);

    ChildFrameT*                       m_ChildFrame;          ///< The child frame within which this document lives.
    wxString                           m_FileName;            ///< This documents file name.
    MapDocAdapterT                     m_DocAdapter;          ///< Kept here because it sometimes needs the same lifetime as the MapDocumentT itself, e.g. when referenced by a "material" property of the Entity Inspector, or by commands in the command history.
    cf::UniScriptStateT                m_ScriptState;         ///< The script state that the script world (`m_ScriptWorld`) lives in.
    IntrusivePtrT<cf::GameSys::WorldT> m_ScriptWorld;         ///< The "script world" contains the entity hierarchy and their components.
    OrthoBspTreeT*                     m_BspTree;             ///< The BSP tree that spatially organizes the map elements in the m_MapWorld.
    GameConfigT*                       m_GameConfig;          ///< The game configuration that is used with this map.
    cf::SceneGraph::LightMapManT       m_LightMapMan;         ///< The light map manager that is used with this map.
    PlantDescrManT                     m_PlantDescrMan;       ///< The plant description manager that is used with this map.

    ArrayT<MapElementT*>               m_Selection;           ///< The currently selected map elements.
    mutable BoundingBox3fT             m_SelectionBB;         ///< The bounding-box of the current selection, or if there is no selection, the bounding-box of the previous selection.
    ArrayT<PtsPointT>                  m_PointFilePoints;     ///< The points of the currently loaded point file.
    ArrayT<wxColour>                   m_PointFileColors;     ///< The colors for items (columns) of a point in the pointfile. A color can be invalid if the associated column should not be visualized at all.

    bool                               m_SnapToGrid;          ///< Snap things to grid.      Kept here because the other two are kept here as well. ;-)
    int                                m_GridSpacing;         ///< The spacing of the grid.  Could also be kept in the related ChildFrameT, but as the observers depent on it, its properly stored here in the MapDocumentT.
    bool                               m_ShowGrid;            ///< Show or hide the 2D grid. Could also be kept in the related ChildFrameT, but as the observers depent on it, its properly stored here in the MapDocumentT.


    /*************************************************************/
    /*** Event handlers for >>document specific<< menu events. ***/
    /*************************************************************/

    void OnSelectionApplyMaterial      (wxCommandEvent& CE);
    void OnUpdateSelectionApplyMaterial(wxUpdateUIEvent& UE);

    void OnMapSnapToGrid               (wxCommandEvent& CE);
    void OnMapToggleGrid2D             (wxCommandEvent& CE);
    void OnMapFinerGrid                (wxCommandEvent& CE);
    void OnMapCoarserGrid              (wxCommandEvent& CE);
    void OnMapGotoPrimitive            (wxCommandEvent& CE);
    void OnMapShowInfo                 (wxCommandEvent& CE);
    void OnMapCheckForProblems         (wxCommandEvent& CE);
    void OnMapLoadPointFile            (wxCommandEvent& CE);
    void OnMapUnloadPointFile          (wxCommandEvent& CE);

    void OnViewShowEntityInfo          (wxCommandEvent& CE);
    void OnViewShowEntityTargets       (wxCommandEvent& CE);

    void OnUpdateViewShowEntityInfo    (wxUpdateUIEvent& UE);
    void OnUpdateViewShowEntityTargets (wxUpdateUIEvent& UE);

    void OnToolsCarve                  (wxCommandEvent& CE);
    void OnToolsHollow                 (wxCommandEvent& CE);
    void OnToolsAssignPrimToEntity     (wxCommandEvent& CE);
    void OnToolsReplaceMaterials       (wxCommandEvent& CE);
    void OnToolsMaterialLock           (wxCommandEvent& CE);
    void OnToolsSnapSelectionToGrid    (wxCommandEvent& CE);
    void OnToolsTransform              (wxCommandEvent& CE);
    void OnToolsAlign                  (wxCommandEvent& CE);
    void OnToolsMirror                 (wxCommandEvent& CE);

    void OnUpdateToolsMaterialLock     (wxUpdateUIEvent& UE);

    DECLARE_EVENT_TABLE()
};

#endif
