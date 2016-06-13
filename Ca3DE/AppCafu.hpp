/*
Cafu Engine, http://www.cafu.de/
Copyright (c) Carsten Fuchs and other contributors.
This project is licensed under the terms of the MIT license.
*/

/**************************************/
/***                                ***/
/***          Cafu Engine           ***/
/***                                ***/
/*** Dass ich erkenne, was die Welt ***/
/*** im Innersten zusammenhält.     ***/
/*** (Faust)                        ***/
/***                                ***/
/**************************************/

#ifndef CAFU_APP_CAFU_HPP_INCLUDED
#define CAFU_APP_CAFU_HPP_INCLUDED

#include "GameInfo.hpp"
#include "Templates/Array.hpp"
#include "wx/app.h"
#include "wx/display.h"


namespace cf { class CompositeConsoleT; }
namespace cf { class ConsoleFileT; }
namespace cf { class ConsoleStringBufferT; }
class MainFrameT;


/// This class represents the Cafu Engine application.
class AppCafuT : public wxApp
{
    public:

    AppCafuT();
    ~AppCafuT();

    /// Returns the composite console that is also available via the global Console pointer.
    cf::CompositeConsoleT& GetConComposite() const;

    /// Returns the console that buffers all output.
    cf::ConsoleStringBufferT& GetConBuffer() const { return *m_ConBuffer; }

    // /// Returns the console that logs all output into a file (can be NULL if not used).
    // cf::ConsoleFileT& GetConFile() const { return *m_ConFile; }

    /// Returns whether we successfully set a custom video mode (screen resolution) during initialization.
    bool IsCustomVideoMode() const { return m_IsCustomVideoMode; }

    /// This method returns the current video mode, which may be identical to the desktops video mode
    /// (in which case the mode as not switched at all at app init), or any custom mode.
    const wxVideoMode& GetCurrentMode() const { return m_CurrentMode; }

    /// Returns the main frame of the Cafu application.
    MainFrameT* GetMainFrame() const { return m_MainFrame; }

    bool OnInit();
    int  OnExit();


    private:

    void OnInitCmdLine(wxCmdLineParser& Parser);
    bool OnCmdLineParsed(wxCmdLineParser& Parser);

    wxLocale*                       m_Locale;
    cf::ConsoleStringBufferT*       m_ConBuffer;          ///< The console that buffers all output.
    cf::ConsoleFileT*               m_ConFile;            ///< The console that logs all output into a file (can be NULL if not used).
    ArrayT<GameInfoT>               m_AllGameInfos;       ///< The game infos for all games/MODs known and available to us.
    GameInfoT                       m_GameInfo;           ///< The info of the game that was elected to run (one of those in m_AllGameInfos).
    bool                            m_IsCustomVideoMode;  ///< Whether we successfully set a custom video mode (screen resolution) during initialization.
    wxVideoMode                     m_CurrentMode;        ///< The video mode that we're currently using.
    MainFrameT*                     m_MainFrame;          ///< The Cafu application main frame.
};


/// This macro provides the wxGetApp() function which returns a reference to our AppCafuT instance.
DECLARE_APP(AppCafuT)

#endif
