/*
Cafu Engine, http://www.cafu.de/
Copyright (c) Carsten Fuchs and other contributors.
This project is licensed under the terms of the MIT license.
*/

#ifndef CAFU_MODELEDITOR_CHANNEL_INSPECTOR_HPP_INCLUDED
#define CAFU_MODELEDITOR_CHANNEL_INSPECTOR_HPP_INCLUDED

#include "ObserverPattern.hpp"
#include "wx/wx.h"
#include "wx/propgrid/manager.h"


namespace ModelEditor
{
    class ChildFrameT;
    class ModelDocumentT;

    class ChannelInspectorT : public wxPropertyGridManager, public ObserverT
    {
        public:

        ChannelInspectorT(ChildFrameT* Parent, const wxSize& Size);
        ~ChannelInspectorT();

        // ObserverT implementation.
        void Notify_SelectionChanged(SubjectT* Subject, ModelElementTypeT Type, const ArrayT<unsigned int>& OldSel, const ArrayT<unsigned int>& NewSel);
     // void Notify_Joint Added / Deleted();
        void Notify_ChannelChanged(SubjectT* Subject, unsigned int ChannelNr);
        void Notify_SubjectDies(SubjectT* dyingSubject);


        private:

        void RefreshPropGrid();
        void OnPropertyGridChanging(wxPropertyGridEvent& Event);

        DECLARE_EVENT_TABLE()

        ModelDocumentT* m_ModelDoc;
        ChildFrameT*    m_Parent;
        bool            m_IsRecursiveSelfNotify;
    };
}

#endif
