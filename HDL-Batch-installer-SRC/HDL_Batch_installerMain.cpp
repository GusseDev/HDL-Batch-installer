/***************************************************************
 * Name:      HDL_Batch_installerMain.cpp
 * Purpose:   Code for Application Frame
 * Author:    matias israelson (aka: El_isra) (tatochin-m@hotmail.com)
 * Created:   2021-03-31
 * Copyright: matias israelson (aka: El_isra) (https://github.com/israpps)
 * License:   GPL-3.0
 **************************************************************/
// FRAME & Dialog
#include "DnDFile.h"
#include "HDL_Batch_installerMain.h"
#include <wx/dir.h>
#include "About.h"
#include "Config.h"
#include "Component_error.h"
#include "ArtMan.h"
#include "DokanMan.h"
#include "Post_Install_Report.h"
#include "CopyHDD.h"
#include "NDBMan.h"
#include "HDDManager.h"
#include "HDDFomatMan.h"
#include "MD5Report.h"
#include "PFSShellBrowser.h"

#include "hdl-dump-recodes.h"
#include "gamename/parser.h" //includes both database & parser function
#include "MD5Man.h"
#include "PFSShell.h"
#include <wx/scrolwin.h>
#include <wx/wrapsizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/accel.h>
#include <wx/dnd.h>
#include <wx/dcbuffer.h>
#include <wx/process.h>
#include <wx/stream.h>
#include <algorithm>

#include "xpm/cd.xpm"
#include "xpm/dvdd.xpm"
#include "xpm/dvddl.xpm"
#include "xpm/info.xpm"

namespace CDXPM {
int CD;
int DVD;
int DVDDL;
}

#define PFSSHELL_DISABLED_WARNING() wxMessageBox(_("HDD Management features are temporarily disabled on 32bit versions because it can cause HDD corruption"), warning_caption, wxICON_WARNING)
#define PFSSHELL_ALLOWED_INT 0
#define PFSSHELL_ALLOWED ((BITS != 32) || (PFSSHELL_ALLOWED_INT == 1))
using namespace std;
bool first_init = false;

// Id du timer de re-priorisation des vignettes (distinct du LogTimer).
static const int ID_ART_TIMER = wxID_HIGHEST + 733;
// Source d'une vignette : cache disque (deja 24x24), asset local telecharge, ou lecture HDD.
enum ArtSource { ART_FROM_CACHE = 0, ART_FROM_LOCAL = 1, ART_FROM_HDD = 2 };

// Cible de glisser-deposer sur la liste d'installation : accepte les images de
// jeu (ISO/CUE/...) et les archives (zip/rar/7z), qui sont extraites puis ajoutees.
class IsoDropTarget : public wxFileDropTarget
{
public:
    IsoDropTarget(HDL_Batch_installerFrame* frame) : m_frame(frame) {}
    virtual bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override
    { if (m_frame) m_frame->AddGamesToList(filenames); return true; }
private:
    HDL_Batch_installerFrame* m_frame;
};

// Barre d'espace HDD personnalisee : portion utilisee coloree + pourcentage au
// centre + legende (Used / Free / Total) au-dessus.
class HddSpaceBar : public wxPanel
{
public:
    HddSpaceBar(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 48))
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &HddSpaceBar::OnPaint, this);
        Bind(wxEVT_SIZE,  &HddSpaceBar::OnSize,  this);
    }
    void SetUsage(long totalMB, long usedMB, long freeMB)
    { m_total = totalMB; m_used = usedMB; m_free = freeMB; m_has = true; Refresh(); }
    void Clear() { m_has = false; Refresh(); }
private:
    long m_total = 0, m_used = 0, m_free = 0;
    bool m_has = false;
    static wxString Fmt(long mb)
    { return (mb >= 1024) ? wxString::Format("%.0f GB", mb / 1024.0) : wxString::Format("%ld MB", mb); }
    void OnSize(wxSizeEvent&) { Refresh(); }
    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        wxSize sz = GetClientSize();
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        dc.SetFont(GetFont());
        wxString legend = m_has
            ? wxString::Format(_("Used %s     Free %s     Total %s"), Fmt(m_used), Fmt(m_free), Fmt(m_total))
            : wxString(_("No PS2 HDD detected - connect one and click \"Search ps2 HDD's\""));
        dc.SetTextForeground(wxColour(70, 70, 70));
        wxSize ts = dc.GetTextExtent(legend);
        dc.DrawText(legend, (sz.x - ts.x) / 2, 2);
        int barX = 3, barY = 22, barW = sz.x - 6, barH = sz.y - barY - 3;
        if (barW < 10 || barH < 6) return;
        dc.SetPen(wxPen(wxColour(170, 170, 170)));
        dc.SetBrush(wxBrush(wxColour(238, 238, 238)));
        dc.DrawRoundedRectangle(barX, barY, barW, barH, 4);
        if (m_has && m_total > 0)
        {
            double frac = (double)m_used / (double)m_total;
            if (frac < 0) frac = 0;
            if (frac > 1) frac = 1;
            int fillW = (int)(barW * frac);
            wxColour c = (frac < 0.80) ? wxColour(56, 162, 71)
                       : (frac < 0.95) ? wxColour(214, 154, 40)
                                       : wxColour(200, 66, 66);
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(c));
            if (fillW > 0) dc.DrawRoundedRectangle(barX, barY, (fillW < 8 ? 8 : fillW), barH, 4);
            wxString pct = wxString::Format("%d%%", (int)(frac * 100 + 0.5));
            dc.SetFont(GetFont().Bold());
            wxSize ps = dc.GetTextExtent(pct);
            dc.SetTextForeground((frac > 0.45) ? *wxWHITE : wxColour(60, 60, 60));
            dc.DrawText(pct, barX + (barW - ps.x) / 2, barY + (barH - ps.y) / 2);
        }
    }
};

// Barre de progression d'installation : meme style que la barre d'espace HDD
// (rectangle arrondi + remplissage colore + pourcentage au centre).
class MiniProgressBar : public wxPanel
{
public:
    MiniProgressBar(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(120, 16))
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        Bind(wxEVT_PAINT, &MiniProgressBar::OnPaint, this);
    }
    void SetValue(int pct)
    {
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        if (pct != m_pct) { m_pct = pct; Refresh(); Update(); }
    }
private:
    int m_pct = 0;
    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        wxSize sz = GetClientSize();
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        dc.SetPen(wxPen(wxColour(170, 170, 170)));
        dc.SetBrush(wxBrush(wxColour(238, 238, 238)));
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 3);
        int fillW = sz.x * m_pct / 100;
        if (fillW > 0)
        {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(wxColour(56, 162, 71))); // vert, comme la barre disque
            dc.DrawRoundedRectangle(0, 0, (fillW < 6 ? 6 : fillW), sz.y, 3);
        }
        dc.SetFont(GetFont());
        wxString t = wxString::Format("%d%%", m_pct);
        wxSize ts = dc.GetTextExtent(t);
        dc.SetTextForeground((m_pct > 45) ? *wxWHITE : wxColour(50, 50, 50));
        dc.DrawText(t, (sz.x - ts.x) / 2, (sz.y - ts.y) / 2);
    }
};

///CONFIG TABLE
namespace CFG
{
int         DBMODE = DB_INTERNAL; //INTERNAL || EXTERNAL (gamename.DB at Program location)
bool        DBENABLE = true;     //if the program should use any database. true=use database,false=arbitrarly use ISO filename
int         DEBUG_LEVEL;          //defines how many data will be written at the log
int         MINIOPL_WARNING;      //1=display warnings if boot.kelf (AKA: MiniOPL) is missing before begining installation
int         OSD_HIDE;             //use or not -hide switch on installation command
wxString    DEFAULT_ISO_PATH;
int         DMA;
bool        LOAD_CUSTOM_ICONS;
wxString    mountpoint;
wxString    default_OPLPART;
wxString    NBD_IP;
bool        SHARE_DATA;
bool        DISPATCH_SYSTEM_NOTIFICATIONS;
bool        ALLOW_EXPERIMENTAL;
bool        AUTO_ASSETS;          //telecharge auto les assets a l'ajout d'un jeu
bool        HDDManagerGameTitleDISP;
bool        HDDManagerSubPartDSP;
//    bool        UPDATE_AVAILABLE = false;
//    bool        UPDATE_WARNINGS;
}
wxString CFG_ARTURL;
bool CAN_COPY_HDD;
wxString versionTAG;
//wxString gserverTAG;
//Default path for game search event
///CONFIG TABLE

wxString Get_env(wxString ENV)
{
    wxString TMP;
    wxGetEnv(ENV,&TMP);
    return TMP;
}
/// common data declaration:
//wxFileName fname( wxTheApp->argv[0] );
wxFileName fname;
wxString EXEC_PATH = fname.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR);
wxString HDL_CACHE;
wxString MBR_CACHE;
wxString MiniOPL;
wxString ICONS_FOLDER;

std::string HDD_TOKEN;
PFSShell PFSSHELL;
bool PFSSHELL_USABLE = false;

extern string DMA_TABLE[8];
extern string DMA_ALIAS[8];
const std::string MiniOPL_URL = "https://github.com/israpps/HDL-Batch-installer/raw/main/Release/boot.kelf";
const wxString error_caption = _("error");
const wxString warning_caption = _("warning");
#define DOKAN_ENV "DokanLibrary1"
#define DOKAN_ENV2 "DokanLibrary2"
wxString HDLBINST_APPDATA;

/*
 * NOTE: lines encased between //(* and //*) are controlled by code::blocks wxwidgets project manager, changing code inside it is useless, and will be erased as sson as anyone changes the front-end
 */
//(*InternalHeaders(HDL_Batch_installerFrame)
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/font.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/settings.h>
#include <wx/string.h>
//*)
#include <wx/appprogress.h>


//(*IdInit(HDL_Batch_installerFrame)
const long HDL_Batch_installerFrame::ID_BUTTON2 = wxNewId();
const long HDL_Batch_installerFrame::ID_selected_hdd = wxNewId();
const long HDL_Batch_installerFrame::ID_TEXTCTRL1 = wxNewId();
const long HDL_Batch_installerFrame::ID_GAUGE1 = wxNewId();
const long HDL_Batch_installerFrame::ID_LISTCTRL1 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON1 = wxNewId();
const long HDL_Batch_installerFrame::ID_STATICLINE3 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON7 = wxNewId();
const long HDL_Batch_installerFrame::ID_STATICLINE1 = wxNewId();
const long HDL_Batch_installerFrame::ID_CHOICE1 = wxNewId();
const long HDL_Batch_installerFrame::ID_STATICLINE2 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON4 = wxNewId();
const long HDL_Batch_installerFrame::ID_STATICLINE4 = wxNewId();
const long HDL_Batch_installerFrame::ID_CHECKBOX2 = wxNewId();
const long HDL_Batch_installerFrame::ID_PANEL1 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON3 = wxNewId();
const long HDL_Batch_installerFrame::ID_TEXTCTRL2 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON8 = wxNewId();
const long HDL_Batch_installerFrame::ID_LISTCTRL2 = wxNewId();
const long HDL_Batch_installerFrame::ID_PANEL2 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON10 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON13 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON6 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON9 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON5 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON11 = wxNewId();
const long HDL_Batch_installerFrame::ID_BUTTON12 = wxNewId();
const long HDL_Batch_installerFrame::ID_PANEL3 = wxNewId();
const long HDL_Batch_installerFrame::ID_NOTEBOOK1 = wxNewId();
const long HDL_Batch_installerFrame::ID_PANEL5 = wxNewId();
const long HDL_Batch_installerFrame::idMenuQuit = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM13 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM15 = wxNewId();
const long HDL_Batch_installerFrame::SETTINGS = wxNewId();
const long HDL_Batch_installerFrame::idMenuAbout = wxNewId();
const long HDL_Batch_installerFrame::UPDT = wxNewId();
const long HDL_Batch_installerFrame::ISSUE = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM1 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM2 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM9 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM10 = wxNewId();
const long HDL_Batch_installerFrame::ID_PROGRESSDIALOG1 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM3 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM18 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM4 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM5 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM7 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM14 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM6 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM11 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM12 = wxNewId();
const long HDL_Batch_installerFrame::ID_MENUITEM8 = wxNewId();
const long HDL_Batch_installerFrame::DELETE_GAME_ID = wxNewId();
const long HDL_Batch_installerFrame::ID_PROGRESSDIALOG2 = wxNewId();
//*)

BEGIN_EVENT_TABLE(HDL_Batch_installerFrame,wxFrame)
    //(*EventTable(HDL_Batch_installerFrame)
    //*)
END_EVENT_TABLE()

HDL_Batch_installerFrame::HDL_Batch_installerFrame(wxWindow* parent, wxLocale& locale, long Custom_Styles, long ctor_flags_ctor, wxWindowID id)
    :  m_locale(locale) ///Initialize Translations
    , CTOR_FLAGS(ctor_flags_ctor) ///Pass init flags into class member
{
    COLOR(0f)
    cout << "welcome to HDL Batch Installer [" << versionTAG <<"]\n";
    COLOR(07)
#ifdef RELEASE
    Create(parent, wxID_ANY, "HDL Batch Installer | By Matias Israelson (El_isra)", wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER|Custom_Styles|wxCAPTION|wxSYSTEM_MENU|wxCLOSE_BOX|wxMINIMIZE_BOX|wxCLIP_CHILDREN, _T("wxID_ANY"));
#endif
    //(*Initialize(HDL_Batch_installerFrame)
    wxBoxSizer* BoxSizer10;
    wxBoxSizer* BoxSizer1;
    wxBoxSizer* BoxSizer2;
    wxBoxSizer* BoxSizer3;
    wxBoxSizer* BoxSizer4;
    wxBoxSizer* BoxSizer5;
    wxBoxSizer* BoxSizer6;
    wxBoxSizer* BoxSizer7;
    wxBoxSizer* BoxSizer8;
    wxBoxSizer* BoxSizer9;
    wxFlexGridSizer* FlexGridSizer1;
    wxFlexGridSizer* FlexGridSizer2;
    wxFlexGridSizer* FlexGridSizer3;
    wxFlexGridSizer* FlexGridSizer4;
    wxFlexGridSizer* FlexGridSizer5;
    wxFlexGridSizer* FlexGridSizer6;
    wxFlexGridSizer* FlexGridSizer7;
    wxFlexGridSizer* FlexGridSizer8;
    wxMenu* Menu1;
    wxMenu* Menu2;
    wxMenu* Menu3;
    wxMenuBar* MenuBar1;
    wxMenuItem* MenuItem1;
    wxMenuItem* MenuItem2;
    wxMenuItem* MenuItem3;

    Create(parent, wxID_ANY, _("HDL Batch Installer"), wxDefaultPosition, wxDefaultSize, wxCAPTION|wxSYSTEM_MENU|wxRESIZE_BORDER|wxCLOSE_BOX|wxMAXIMIZE_BOX|wxMINIMIZE_BOX|wxCLIP_CHILDREN, _T("wxID_ANY"));
    SetClientSize(wxSize(537,651));
    Move(wxPoint(-1,-1));
    SetMinSize(wxSize(537,681));
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_MENU));
    wxFont thisFont(8,wxFONTFAMILY_SWISS,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL,false,_T("Arial"),wxFONTENCODING_DEFAULT);
    SetFont(thisFont);
    SetIcon( wxICON(HDL_BATCH_ICON));
    Panel5 = new wxPanel(this, ID_PANEL5, wxPoint(0,0), wxDefaultSize, wxTAB_TRAVERSAL, _T("ID_PANEL5"));
    Panel5->SetMinSize(wxSize(557,691));
    FlexGridSizer1 = new wxFlexGridSizer(3, 1, 0, 0);
    FlexGridSizer1->AddGrowableCol(0);
    FlexGridSizer1->AddGrowableRow(2);
    FlexGridSizer2 = new wxFlexGridSizer(0, 1, 0, 0);
    FlexGridSizer2->AddGrowableCol(0);
    BoxSizer1 = new wxBoxSizer(wxHORIZONTAL);
    Button1 = new wxButton(Panel5, ID_BUTTON2, _("Search ps2 HDD\'s"), wxDefaultPosition, wxSize(144,24), 0, wxDefaultValidator, _T("ID_BUTTON2"));
    Button1->SetFocus();
    BoxSizer1->Add(Button1, 1, wxALIGN_TOP, 0);
    selected_hdd = new wxChoice(Panel5, ID_selected_hdd, wxDefaultPosition, wxSize(148,24), 0, 0, 0, wxDefaultValidator, _T("ID_selected_hdd"));
    BoxSizer1->Add(selected_hdd, 2, wxEXPAND, 0);
    FlexGridSizer2->Add(BoxSizer1, 1, wxALL|wxEXPAND, 5);
    FlexGridSizer1->Add(FlexGridSizer2, 1, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 5);
    FlexGridSizer3 = new wxFlexGridSizer(0, 1, 0, 0);
    FlexGridSizer3->AddGrowableCol(0);
    hdd_used_space = new wxTextCtrl(Panel5, ID_TEXTCTRL1, wxEmptyString, wxDefaultPosition, wxSize(400,23), wxTE_READONLY|wxTE_CENTRE, wxDefaultValidator, _T("ID_TEXTCTRL1"));
    FlexGridSizer3->Add(hdd_used_space, 0, wxEXPAND, 0);
    Gauge1 = new wxGauge(Panel5, ID_GAUGE1, 100, wxDefaultPosition, wxSize(464,24), wxGA_SMOOTH, wxDefaultValidator, _T("ID_GAUGE1"));
    FlexGridSizer3->Add(Gauge1, 0, wxEXPAND, 0);
    FlexGridSizer3->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer1->Add(FlexGridSizer3, 1, wxBOTTOM|wxLEFT|wxRIGHT|wxEXPAND, 5);
    FlexGridSizer4 = new wxFlexGridSizer(1, 3, 0, 0);
    FlexGridSizer4->AddGrowableCol(0);
    FlexGridSizer4->AddGrowableRow(0);
    Notebook1 = new wxNotebook(Panel5, ID_NOTEBOOK1, wxPoint(-1,-1), wxSize(520,440), 0, _T("ID_NOTEBOOK1"));
    Notebook1->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    Panel1 = new wxPanel(Notebook1, ID_PANEL1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("ID_PANEL1"));
    FlexGridSizer6 = new wxFlexGridSizer(0, 3, 3, 4);
    FlexGridSizer6->AddGrowableCol(0);
    FlexGridSizer6->AddGrowableRow(0);
    game_list__ = new wxListCtrl(Panel1, ID_LISTCTRL1, wxDefaultPosition, wxSize(388,447), wxLC_REPORT|wxLC_SINGLE_SEL|wxLC_HRULES|wxLC_VRULES|wxBORDER_SUNKEN|wxVSCROLL, wxDefaultValidator, _T("ID_LISTCTRL1"));
    game_list__->SetMinSize(wxSize(377,415));
    game_list__->SetToolTip(_("Games marked in blue are ZSO"));
    wxListItem _col0;
    _col0.SetId(0);
    _col0.SetText( _("Game ") );
    _col0.SetWidth(370);
    game_list__->InsertColumn(0, _col0);
    FlexGridSizer6->Add(game_list__, 0, wxALL|wxEXPAND, 0);
    FlexGridSizer6->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer7 = new wxFlexGridSizer(0, 1, 0, 0);
    FlexGridSizer7->AddGrowableCol(0);
    SEARCH_ISO = new wxButton(Panel1, ID_BUTTON1, _("Search Games"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON1"));
    FlexGridSizer7->Add(SEARCH_ISO, 0, wxLEFT|wxRIGHT|wxEXPAND, 2);
    StaticLine3 = new wxStaticLine(Panel1, ID_STATICLINE3, wxDefaultPosition, wxSize(0,0), wxLI_HORIZONTAL, _T("ID_STATICLINE3"));
    FlexGridSizer7->Add(StaticLine3, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer7->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    clear_iso_list = new wxButton(Panel1, ID_BUTTON7, _("Clear list"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON7"));
    FlexGridSizer7->Add(clear_iso_list, 0, wxLEFT|wxRIGHT|wxEXPAND, 2);
    StaticLine1 = new wxStaticLine(Panel1, ID_STATICLINE1, wxDefaultPosition, wxSize(10,-1), wxLI_HORIZONTAL, _T("ID_STATICLINE1"));
    FlexGridSizer7->Add(StaticLine1, 1, wxALL|wxEXPAND, 5);
    FlexGridSizer7->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer7->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    dma_choice = new wxChoice(Panel1, ID_CHOICE1, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE1"));
    dma_choice->Disable();
    for (int X=0 ; X <=7 ; X++)
    { dma_choice->Append(DMA_ALIAS[X]); }
    dma_choice->SetSelection(7);
    FlexGridSizer7->Add(dma_choice, 0, wxLEFT|wxRIGHT|wxEXPAND, 2);
    StaticLine2 = new wxStaticLine(Panel1, ID_STATICLINE2, wxDefaultPosition, wxSize(0,0), wxLI_HORIZONTAL, _T("ID_STATICLINE2"));
    FlexGridSizer7->Add(StaticLine2, 1, wxALL|wxEXPAND, 5);
    FlexGridSizer7->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    install = new wxButton(Panel1, ID_BUTTON4, _("Install"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON4"));
    install->Disable();
    FlexGridSizer7->Add(install, 0, wxLEFT|wxRIGHT|wxEXPAND, 2);
    StaticLine4 = new wxStaticLine(Panel1, ID_STATICLINE4, wxDefaultPosition, wxSize(10,-1), wxLI_HORIZONTAL, _T("ID_STATICLINE4"));
    FlexGridSizer7->Add(StaticLine4, 1, wxALL|wxEXPAND, 5);
    FlexGridSizer7->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    use_database = new wxCheckBox(Panel1, ID_CHECKBOX2, _("Use Database"), wxDefaultPosition, wxSize(110,20), 0, wxDefaultValidator, _T("ID_CHECKBOX2"));
    use_database->SetValue(true);
    use_database->SetToolTip(_("When enabled, the program will try to automatically assign the original title for the games by searching the game ID on a database with more than 14700 titles"));
    FlexGridSizer7->Add(use_database, 0, wxLEFT|wxRIGHT|wxEXPAND, 2);
    FlexGridSizer6->Add(FlexGridSizer7, 1, wxALL|wxEXPAND, 5);
    Panel1->SetSizer(FlexGridSizer6);
    FlexGridSizer6->Fit(Panel1);
    FlexGridSizer6->SetSizeHints(Panel1);
    Panel2 = new wxPanel(Notebook1, ID_PANEL2, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("ID_PANEL2"));
    FlexGridSizer8 = new wxFlexGridSizer(0, 1, 0, 0);
    FlexGridSizer8->AddGrowableCol(0);
    FlexGridSizer8->AddGrowableRow(1);
    BoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
    Parse_hdl_toc = new wxButton(Panel2, ID_BUTTON3, _("Get List"), wxDefaultPosition, wxSize(80,23), 0, wxDefaultValidator, _T("ID_BUTTON3"));
    Parse_hdl_toc->Disable();
    BoxSizer2->Add(Parse_hdl_toc, 2, wxALL|wxEXPAND, 1);
    GameCountDisplay = new wxTextCtrl(Panel2, ID_TEXTCTRL2, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY|wxBORDER_NONE, wxDefaultValidator, _T("ID_TEXTCTRL2"));
    BoxSizer2->Add(GameCountDisplay, 2, wxALL|wxEXPAND, 5);
    Button3 = new wxButton(Panel2, ID_BUTTON8, _("\?"), wxDefaultPosition, wxSize(16,23), 0, wxDefaultValidator, _T("ID_BUTTON8"));
    BoxSizer2->Add(Button3, 1, wxALL|wxALIGN_TOP|wxSHAPED, 1);
    FlexGridSizer8->Add(BoxSizer2, 1, wxEXPAND, 5);
    Installed_game_list = new wxListCtrl(Panel2, ID_LISTCTRL2, wxDefaultPosition, wxSize(509,378), wxLC_REPORT|wxLC_HRULES|wxLC_VRULES|wxBORDER_SUNKEN|wxVSCROLL, wxDefaultValidator, _T("ID_LISTCTRL2"));
    Installed_game_list->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
    wxListItem col0;
    col0.SetId(0);
    col0.SetText( _("Game ") );
    col0.SetWidth(200);//180
    Installed_game_list->InsertColumn(0, col0);

    // Add second column
    wxListItem col1;
    col1.SetId(1);
    col1.SetText( _("ID") );
    col1.SetWidth(110);
    Installed_game_list->InsertColumn(1, col1);

    // Add third column
    wxListItem col2;
    col2.SetId(2);
    col2.SetText( _("Size (MB / GB)") );
    col2.SetWidth(130);
    Installed_game_list->InsertColumn(2, col2);

    // Add 4th column
    wxListItem col3;
    col3.SetId(3);
    col3.SetText( _("Media") );
    col3.SetWidth(50);
    Installed_game_list->InsertColumn(3, col3);
    FlexGridSizer8->Add(Installed_game_list, 0, wxEXPAND, 0);
    Panel2->SetSizer(FlexGridSizer8);
    FlexGridSizer8->Fit(Panel2);
    FlexGridSizer8->SetSizeHints(Panel2);
    Panel3 = new wxPanel(Notebook1, ID_PANEL3, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("ID_PANEL3"));
    Panel3->SetMaxSize(wxSize(-1,-1));
    FlexGridSizer5 = new wxFlexGridSizer(0, 0, 5, 5);
    FlexGridSizer5->AddGrowableCol(0);
    FlexGridSizer5->AddGrowableRow(0);
    BoxSizer3 = new wxBoxSizer(wxVERTICAL);
    BoxSizer10 = new wxBoxSizer(wxHORIZONTAL);
    HDDManagerButton = new wxButton(Panel3, ID_BUTTON10, _("HDD Manager"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON10"));
    HDDManagerButton->Disable();
    BoxSizer10->Add(HDDManagerButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 0);
    BoxSizer3->Add(BoxSizer10, 1, wxALL|wxEXPAND, 5);
    BoxSizer9 = new wxBoxSizer(wxHORIZONTAL);
    mass_header_injection = new wxButton(Panel3, ID_BUTTON13, _("Inject OPL Launcher"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON13"));
    mass_header_injection->Disable();
    mass_header_injection->SetToolTip(_("This will inject OPL Launcher (boot.kelf on program folder) into every PS2 game found on this HDD.\nUsefull to install/update OPL Launcher into your games"));
    BoxSizer9->Add(mass_header_injection, 0, wxALIGN_CENTER_VERTICAL, 0);
    BoxSizer3->Add(BoxSizer9, 1, wxALL|wxEXPAND, 5);
    BoxSizer8 = new wxBoxSizer(wxHORIZONTAL);
    modify_header_event = new wxButton(Panel3, ID_BUTTON6, _("Modify Header"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON6"));
    modify_header_event->Disable();
    BoxSizer8->Add(modify_header_event, 0, wxALIGN_CENTER_VERTICAL, 0);
    BoxSizer3->Add(BoxSizer8, 1, wxALL|wxEXPAND, 5);
    BoxSizer7 = new wxBoxSizer(wxHORIZONTAL);
    MBRExtractRequest = new wxButton(Panel3, ID_BUTTON9, _("Recover MBR"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON9"));
    MBRExtractRequest->Disable();
    BoxSizer7->Add(MBRExtractRequest, 0, wxALIGN_CENTER_VERTICAL, 0);
    BoxSizer3->Add(BoxSizer7, 1, wxALL|wxEXPAND, 5);
    BoxSizer6 = new wxBoxSizer(wxHORIZONTAL);
    MBR_EVENT = new wxButton(Panel3, ID_BUTTON5, _("Inject MBR"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON5"));
    MBR_EVENT->Disable();
    BoxSizer6->Add(MBR_EVENT, 0, wxALIGN_CENTER_VERTICAL, 0);
    BoxSizer3->Add(BoxSizer6, 1, wxALL|wxEXPAND, 5);
    BoxSizer5 = new wxBoxSizer(wxHORIZONTAL);
    FUSE = new wxButton(Panel3, ID_BUTTON11, _("Mount hdd Partition"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON11"));
    FUSE->Disable();
    BoxSizer5->Add(FUSE, 0, wxALIGN_CENTER_VERTICAL, 0);
    BoxSizer3->Add(BoxSizer5, 1, wxALL|wxEXPAND, 5);
    BoxSizer4 = new wxBoxSizer(wxHORIZONTAL);
    PFSBrowserCall = new wxButton(Panel3, ID_BUTTON12, _("PFS FileBrowser"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON12"));
    BoxSizer4->Add(PFSBrowserCall, 0, wxALIGN_CENTER_VERTICAL, 5);
    BoxSizer3->Add(BoxSizer4, 1, wxALL|wxEXPAND, 5);
    FlexGridSizer5->Add(BoxSizer3, 1, wxALL|wxEXPAND, 5);
    Panel3->SetSizer(FlexGridSizer5);
    FlexGridSizer5->Fit(Panel3);
    FlexGridSizer5->SetSizeHints(Panel3);
    Notebook1->AddPage(Panel1, _("Install"), false);
    Notebook1->AddPage(Panel2, _("Browse"), false);
    Notebook1->AddPage(Panel3, _("HDD Management"), false);
    FlexGridSizer4->Add(Notebook1, 0, wxEXPAND, 0);
    FlexGridSizer1->Add(FlexGridSizer4, 4, wxALL|wxEXPAND, 5);
    Panel5->SetSizer(FlexGridSizer1);
    FlexGridSizer1->Fit(Panel5);
    FlexGridSizer1->SetSizeHints(Panel5);
    MenuBar1 = new wxMenuBar();
    Menu1 = new wxMenu();
    MenuItem1 = new wxMenuItem(Menu1, idMenuQuit, _("Quit\tAlt-F4"), _("Quit the application"), wxITEM_NORMAL);
    Menu1->Append(MenuItem1);
    COPYHDD = new wxMenuItem(Menu1, ID_MENUITEM13, _("Massive game transfer"), _("Transfer all games installed on currently selected HDD into another one"), wxITEM_NORMAL);
    Menu1->Append(COPYHDD);
    COPYHDD->Enable(false);
    MenuHDDFormat = new wxMenuItem(Menu1, ID_MENUITEM15, _("Format HDD"), _("Format any device into PS2 HDD"), wxITEM_NORMAL);
    Menu1->Append(MenuHDDFormat);
    MenuBar1->Append(Menu1, _("&Main"));
    Menu3 = new wxMenu();
    MenuItem3 = new wxMenuItem(Menu3, SETTINGS, _("Settings\tF2"), _("configure program"), wxITEM_NORMAL);
    Menu3->Append(MenuItem3);
    MenuBar1->Append(Menu3, _("Config"));
    Menu2 = new wxMenu();
    MenuItem2 = new wxMenuItem(Menu2, idMenuAbout, _("About\tF1"), _("Show version and credits"), wxITEM_NORMAL);
    Menu2->Append(MenuItem2);
    MenuItem5 = new wxMenuItem(Menu2, UPDT, _("Update Program"), wxEmptyString, wxITEM_NORMAL);
    Menu2->Append(MenuItem5);
    MenuItem6 = new wxMenuItem(Menu2, ISSUE, _("Report Issue"), wxEmptyString, wxITEM_NORMAL);
    Menu2->Append(MenuItem6);
    MenuBar1->Append(Menu2, _("About"));
    Menu4 = new wxMenu();
    MenuItem4 = new wxMenuItem(Menu4, ID_MENUITEM1, _("Update OPL Launcher"), _("update OPL Launcher KELF"), wxITEM_NORMAL);
    Menu4->Append(MenuItem4);
    MenuItem7 = new wxMenuItem(Menu4, ID_MENUITEM2, _("Update HDL-Dump"), _("Update the game installation tool"), wxITEM_NORMAL);
    Menu4->Append(MenuItem7);
    MenuItem14 = new wxMenuItem(Menu4, ID_MENUITEM9, _("Update game title database"), _("Update game title database"), wxITEM_NORMAL);
    Menu4->Append(MenuItem14);
    MenuItem15 = new wxMenuItem(Menu4, ID_MENUITEM10, _("Download Icons Package"), _("Update HDD-OSD icons package"), wxITEM_NORMAL);
    Menu4->Append(MenuItem15);
    MenuBar1->Append(Menu4, _("Downloads"));
    SetMenuBar(MenuBar1);
    MBR_search = new wxFileDialog(this, _("Search MBR.KELF"), wxEmptyString, _("MBR.KELF"), _("*.KELF"), wxFD_DEFAULT_STYLE|wxFD_OPEN|wxFD_FILE_MUST_EXIST, wxDefaultPosition, wxDefaultSize, _T("wxFileDialog"));
    //nedeaaa
    MenuItem8 = new wxMenuItem((&about_2_install_menu), ID_MENUITEM3, _("Open File location"), wxEmptyString, wxITEM_NORMAL);
    MenuItem8->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_FOLDER_OPEN")),wxART_BUTTON));
    about_2_install_menu.Append(MenuItem8);
    Redump_search = new wxMenuItem((&about_2_install_menu), ID_MENUITEM18, _("Calculate MD5 Hash"), wxEmptyString, wxITEM_NORMAL);
    Redump_search->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_REPORT_VIEW")),wxART_BUTTON));
    about_2_install_menu.Append(Redump_search);
    MenuItem9 = new wxMenuItem((&about_2_install_menu), ID_MENUITEM4, _("Remove from List"), wxEmptyString, wxITEM_NORMAL);
    MenuItem9->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_CROSS_MARK")),wxART_BUTTON));
    about_2_install_menu.Append(MenuItem9);
    MenuItem10 = new wxMenuItem((&Browser_menu), ID_MENUITEM5, _("Extract Game(s)"), wxEmptyString, wxITEM_NORMAL);
    MenuItem10->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_FILE_OPEN")),wxART_BUTTON));
    Browser_menu.Append(MenuItem10);
    MenuItem12 = new wxMenuItem((&Browser_menu), ID_MENUITEM7, _("Download assets"), wxEmptyString, wxITEM_NORMAL);
    MenuItem12->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_GO_DOWN")),wxART_BUTTON));
    Browser_menu.Append(MenuItem12);
    MenuItem18 = new wxMenuItem((&Browser_menu), ID_MENUITEM14, _("Transfer game(s) to another PS2 HDD"), wxEmptyString, wxITEM_NORMAL);
    MenuItem18->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_COPY")),wxART_BUTTON));
    Browser_menu.Append(MenuItem18);
    MenuItem18->Enable(false);
    Browser_menu.AppendSeparator();
    MenuItem11 = new wxMenuItem((&Browser_menu), ID_MENUITEM6, _("Rename"), wxEmptyString, wxITEM_NORMAL);
    MenuItem11->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_FIND_AND_REPLACE")),wxART_BUTTON));
    Browser_menu.Append(MenuItem11);
    MenuItem16 = new wxMenuItem((&Browser_menu), ID_MENUITEM11, _("Inject OPL Launcher"), wxEmptyString, wxITEM_NORMAL);
    MenuItem16->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_FILE_SAVE")),wxART_BUTTON));
    Browser_menu.Append(MenuItem16);
    MenuItem17 = new wxMenuItem((&Browser_menu), ID_MENUITEM12, _("Load Custom Icon"), wxEmptyString, wxITEM_NORMAL);
    Browser_menu.Append(MenuItem17);
    Browser_menu.AppendSeparator();
    MenuItem13 = new wxMenuItem((&Browser_menu), ID_MENUITEM8, _("Information"), wxEmptyString, wxITEM_NORMAL);
    MenuItem13->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_INFORMATION")),wxART_BUTTON));
    Browser_menu.Append(MenuItem13);
    DeleteGameMenuItem = new wxMenuItem((&Browser_menu), DELETE_GAME_ID, _("Delete game"), wxEmptyString, wxITEM_NORMAL);
    DeleteGameMenuItem->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_CROSS_MARK")),wxART_BUTTON));
    Browser_menu.Append(DeleteGameMenuItem);
    Center();

    Connect(ID_BUTTON2,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnButton1Click);
    Connect(ID_selected_hdd,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::Onselected_hddSelect);
    Connect(ID_TEXTCTRL1,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnTextCtrl1Text);
    Connect(ID_LISTCTRL1,wxEVT_COMMAND_LIST_BEGIN_DRAG,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnListCtrl1BeginDrag);
    Connect(ID_LISTCTRL1,wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK,(wxObjectEventFunction)&HDL_Batch_installerFrame::onItemRightClick);
    Connect(ID_LISTCTRL1,wxEVT_COMMAND_LIST_COL_CLICK,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnListCtrl1BeginDrag);
    Connect(ID_BUTTON1,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnSEARCH_ISOClick);
    Connect(ID_BUTTON7,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::Onclear_iso_listClick);
    Connect(ID_CHOICE1,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::Ondma_choiceSelect);
    Connect(ID_BUTTON4,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OninstallClick);
    Connect(ID_CHECKBOX2,wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnCheckBox1Click1);
    Connect(ID_BUTTON3,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnParse_hdl_tocClick);
    Connect(ID_BUTTON8,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnButton3Click1);
    Connect(ID_LISTCTRL2,wxEVT_COMMAND_LIST_BEGIN_DRAG,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnListCtrl1BeginDrag1);
    Connect(ID_LISTCTRL2,wxEVT_COMMAND_LIST_ITEM_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::Selected_games);
    Connect(ID_LISTCTRL2,wxEVT_COMMAND_LIST_ITEM_DESELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::UnSelected_games);
    Connect(ID_LISTCTRL2,wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnInstalled_game_listItemRClick1);
    Connect(ID_BUTTON10,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnButton2Click3);
    Connect(ID_BUTTON13,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::Onmass_header_injectionClick);
    Connect(ID_BUTTON6,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnButton2Click);
    Connect(ID_BUTTON9,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnMBRExtractRequestClick);
    Connect(ID_BUTTON5,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnMBR_EVENTClick);
    Connect(ID_BUTTON11,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnButton4Click);
    Connect(ID_BUTTON12,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnPFSBrowserCallClick);
    Connect(ID_NOTEBOOK1,wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnNotebook1PageChanged);
    Connect(idMenuQuit,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnQuit);
    Connect(ID_MENUITEM13,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnCOPYHDDSelected);
    Connect(ID_MENUITEM15,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnHDDFormatMenuRequest);
    Connect(SETTINGS,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnSettings);
    Connect(idMenuAbout,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnAbout);
    Connect(UPDT,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnUpdateRequest);
    Connect(ISSUE,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnIssueReport);
    Connect(ID_MENUITEM1,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::On_MiniOPL_Update_request);
    Connect(ID_MENUITEM2,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnHDL_DumpUpdateRequest);
    Connect(ID_MENUITEM9,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::On_GameNameDatabaseDownloadRequest);
    Connect(ID_MENUITEM10,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnIconsPackageRequest);
    Connect(ID_MENUITEM3,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnItemListShowRequest);
    Connect(ID_MENUITEM18,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnCalculateMD5Selected);
    Connect(ID_MENUITEM4,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::RemoveISOfromList);
    Connect(ID_MENUITEM5,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnExtractInstalledGameRequest);
    Connect(ID_MENUITEM7,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnInstalledGameAssetsDownloadRequest);
    Connect(ID_MENUITEM14,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnSelectiveGameMigration);
    Connect(ID_MENUITEM6,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnInstalledGameRenameRequest);
    Connect(ID_MENUITEM11,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnManualInjectionRequest);
    Connect(ID_MENUITEM12,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnLoadCustomIcon2InstalledGameRequest);
    Connect(ID_MENUITEM8,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnGameInfoRequest);
    Connect(DELETE_GAME_ID,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnGameDeletionRequest);
    Connect(wxID_ANY,wxEVT_CLOSE_WINDOW,(wxObjectEventFunction)&HDL_Batch_installerFrame::OnClose);
    //*)
    // --- Titre de la fenetre avec le numero de version ---
    SetTitle(wxString::Format("HDL Batch Installer %s | By Matias Israelson (El_isra)", versionTAG));
    // --- Barre d'espace HDD personnalisee (remplace la jauge + le texte) ---
    m_spaceBar = new HddSpaceBar(Panel5);
    FlexGridSizer3->Detach(hdd_used_space); hdd_used_space->Hide();
    FlexGridSizer3->Replace(Gauge1, m_spaceBar); Gauge1->Hide();

    // --- Suppression des onglets : vue unique empilee (installes en haut, file en bas) ---
    Notebook1->RemovePage(0);
    Notebook1->RemovePage(0);
    Notebook1->RemovePage(0);
    // NB: le notebook masque les pages inactives -> il FAUT les re-afficher.
    Panel1->Reparent(Panel5); Panel1->Show(); // Install (file d'attente)
    Panel2->Reparent(Panel5); Panel2->Show(); // Browse (jeux installes)
    Panel3->Reparent(Panel5); Panel3->Hide(); // HDD Management -> deplace dans un menu
    FlexGridSizer1->Detach(FlexGridSizer4);
    Notebook1->Destroy(); Notebook1 = nullptr;
    // Vue UNIFIEE : une seule liste (jeux installes + jeux en attente en haut).
    // On masque l'ancienne file separee et on ramene ses controles sous la liste.
    Panel1->Hide();
    game_list__->Hide();
    // detacher de l'ancien sizer AVANT de reparenter/re-ajouter (sinon assert wx)
    FlexGridSizer7->Detach(install);
    FlexGridSizer7->Detach(clear_iso_list);
    FlexGridSizer7->Detach(dma_choice);
    FlexGridSizer7->Detach(use_database);
    install->Reparent(Panel2);
    clear_iso_list->Reparent(Panel2);
    dma_choice->Reparent(Panel2);
    use_database->Reparent(Panel2);
    {
        wxBoxSizer* bottomBar = new wxBoxSizer(wxHORIZONTAL);
        wxButton* addGamesBtn = new wxButton(Panel2, wxID_ANY, _("Add games"));
        Connect(addGamesBtn->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnSEARCH_ISOClick);
        bottomBar->Add(addGamesBtn, 0, wxALL, 3);
        bottomBar->Add(install, 0, wxALL, 3);
        bottomBar->Add(clear_iso_list, 0, wxALL, 3);
        bottomBar->Add(new wxStaticText(Panel2, wxID_ANY, _("DMA:")), 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 8);
        bottomBar->Add(dma_choice, 0, wxALL, 3);
        bottomBar->Add(use_database, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 8);
        FlexGridSizer8->Add(bottomBar, 0, wxEXPAND|wxTOP, 4);
    }
    install->Show(false); // apparaitra quand des jeux seront en attente
    // La liste occupe la ligne extensible (row 1) mais avait une hauteur mini de 378px :
    // quand la barre de progression s'affiche, faute de place, la barre du bas (Add games/
    // Install/DMA) etait rognee. On abaisse le mini pour qu'elle reste toujours visible.
    Installed_game_list->SetMinSize(wxSize(-1, 140));
    {
        wxBoxSizer* stack = new wxBoxSizer(wxVERTICAL);
        stack->Add(Panel2, 1, wxEXPAND|wxALL, 6);
        FlexGridSizer1->Add(stack, 1, wxEXPAND, 0);
    }

    // --- Menu "HDD Management" (anciens boutons de l'onglet HDD Management) ---
    m_hddMenu = new wxMenu();
    m_hddMenu->Append(ID_BUTTON10, _("HDD Manager"));
    m_hddMenu->Append(ID_BUTTON11, _("Mount HDD Partition"));
    m_hddMenu->Append(ID_BUTTON12, _("PFS File Browser"));
    m_hddMenu->AppendSeparator();
    m_hddMenu->Append(ID_BUTTON13, _("Inject OPL Launcher (all games)"));
    m_hddMenu->Append(ID_BUTTON5,  _("Inject MBR"));
    m_hddMenu->Append(ID_BUTTON9,  _("Recover MBR"));
    m_hddMenu->Append(ID_BUTTON6,  _("Modify Header"));
    GetMenuBar()->Insert(1, m_hddMenu, _("HDD Management"));
    m_hddMenu->Enable(ID_BUTTON10, false);
    m_hddMenu->Enable(ID_BUTTON11, false);
    m_hddMenu->Enable(ID_BUTTON13, false);
    m_hddMenu->Enable(ID_BUTTON5,  false);
    m_hddMenu->Enable(ID_BUTTON9,  false);
    Connect(ID_BUTTON10, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnButton2Click3);
    Connect(ID_BUTTON11, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnButton4Click);
    Connect(ID_BUTTON12, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnPFSBrowserCallClick);
    Connect(ID_BUTTON13, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&HDL_Batch_installerFrame::Onmass_header_injectionClick);
    Connect(ID_BUTTON5,  wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnMBR_EVENTClick);
    Connect(ID_BUTTON9,  wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnMBRExtractRequestClick);
    Connect(ID_BUTTON6,  wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnButton2Click);
    // "Get List" -> petit bouton icone "refresh"
    Parse_hdl_toc->SetLabel(wxEmptyString);
    Parse_hdl_toc->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_REDO")), wxART_BUTTON));
    Parse_hdl_toc->SetToolTip(_("Refresh list"));
    Parse_hdl_toc->SetMinSize(wxSize(34, 26));
    Parse_hdl_toc->SetMaxSize(wxSize(40, 28));
    // Champ de recherche pour filtrer la liste
    m_searchField = new wxTextCtrl(Panel2, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(180, 24));
    m_searchField->SetHint(_("Search..."));
    m_searchField->Bind(wxEVT_TEXT, &HDL_Batch_installerFrame::OnSearchText, this);
    BoxSizer2->Insert(1, m_searchField, 2, wxALL|wxALIGN_CENTER_VERTICAL, 3);
    Panel2->Layout();
    Panel5->Layout();
    // --- Console embarquee en bas de la fenetre (remplace la console detachee) ---
    LogPanel = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 150),
                              wxTE_MULTILINE|wxTE_READONLY|wxTE_DONTWRAP|wxHSCROLL);
    LogPanel->SetBackgroundColour(wxColour(18, 18, 18));
    LogPanel->SetForegroundColour(wxColour(210, 210, 210));
    {
        wxFont lf = LogPanel->GetFont();
        lf.SetFamily(wxFONTFAMILY_TELETYPE);
        LogPanel->SetFont(lf);
    }
    // --- Barre de progression du chargement des vignettes (+ bouton Interrompre) ---
    m_artProgPanel = new wxPanel(this, wxID_ANY);
    {
        wxBoxSizer* ps = new wxBoxSizer(wxHORIZONTAL);
        m_artProgLabel = new wxStaticText(m_artProgPanel, wxID_ANY, _("Loading artwork"));
        m_artBar = new MiniProgressBar(m_artProgPanel); // meme style que la barre de capacite disque
        m_artBar->SetMinSize(wxSize(-1, 18));
        m_artCancelBtn = new wxButton(m_artProgPanel, wxID_ANY, _("Stop"), wxDefaultPosition, wxSize(70, 24));
        m_artCancelBtn->Bind(wxEVT_BUTTON, &HDL_Batch_installerFrame::OnArtCancel, this);
        ps->Add(m_artProgLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);
        ps->Add(m_artBar, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        ps->Add(m_artCancelBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        m_artProgPanel->SetSizer(ps);
    }
    m_artProgPanel->Hide();
    {
        wxBoxSizer* RootSizer = new wxBoxSizer(wxVERTICAL);
        RootSizer->Add(Panel5, 1, wxEXPAND, 0);
        RootSizer->Add(m_artProgPanel, 0, wxEXPAND | wxTOP | wxBOTTOM, 2);
        RootSizer->Add(LogPanel, 0, wxEXPAND);
        SetSizer(RootSizer);
        RootSizer->SetSizeHints(this);
        Layout();
        Centre();
    }
    LogTimer = new wxTimer(this);
    Bind(wxEVT_TIMER, &HDL_Batch_installerFrame::OnLogTimer, this);
    LogTimer->Start(250);
    // Timer dedie au chargement progressif des vignettes (id propre pour ne pas croiser LogTimer)
    m_artTimer = new wxTimer(this, ID_ART_TIMER);
    Bind(wxEVT_TIMER, &HDL_Batch_installerFrame::OnArtTimer, this, ID_ART_TIMER);
    // Double-clic sur un jeu installe -> galerie des medias trouves sur le HDD
    Connect(ID_LISTCTRL2, wxEVT_COMMAND_LIST_ITEM_ACTIVATED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnInstalledGameActivated);
    // Clic sur un en-tete de colonne -> tri
    Installed_game_list->Bind(wxEVT_COMMAND_LIST_COL_CLICK, &HDL_Batch_installerFrame::OnListColClick, this);
    // Entree de menu contextuel : copier les assets telecharges (Downloads/) vers le HDD (+OPL)
    {
        wxMenuItem* copyAssetsItem = new wxMenuItem(&Browser_menu, wxID_ANY, _("Copy downloaded assets to HDD (+OPL)"), wxEmptyString, wxITEM_NORMAL);
        copyAssetsItem->SetBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_GO_UP")),wxART_BUTTON));
        Browser_menu.Insert(2, copyAssetsItem); // juste apres "Download assets"
        Connect(copyAssetsItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnCopyAssetsToHDD);
    }
    // F5 = rafraichir la liste des jeux installes
    {
        wxAcceleratorEntry acc(wxACCEL_NORMAL, WXK_F5, wxID_REFRESH);
        SetAcceleratorTable(wxAcceleratorTable(1, &acc));
        Connect(wxID_REFRESH, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&HDL_Batch_installerFrame::OnRefreshListHotkey);
    }
    // Glisser-deposer d'ISO / d'archives (mecanisme WM_DROPFILES : plus fiable
    // qu'OLE pour une app elevee/admin).
    Installed_game_list->DragAcceptFiles(true);
    Installed_game_list->Bind(wxEVT_DROP_FILES, &HDL_Batch_installerFrame::OnDropFilesEvent, this);
    Panel2->DragAcceptFiles(true);
    Panel2->Bind(wxEVT_DROP_FILES, &HDL_Batch_installerFrame::OnDropFilesEvent, this);
#ifdef __WXMSW__
    // L'app tourne en admin (manifeste) : Windows (UIPI) bloque par defaut le
    // glisser-deposer venant de l'explorateur non-eleve (curseur "interdit").
    // On autorise explicitement les messages de drop pour ce process.
    {
        typedef BOOL (WINAPI *PCWMF)(UINT, DWORD);
        HMODULE u32 = GetModuleHandleW(L"user32.dll");
        PCWMF pChangeFilter = u32 ? (PCWMF)GetProcAddress(u32, "ChangeWindowMessageFilter") : nullptr;
        if (pChangeFilter)
        {
            pChangeFilter(WM_DROPFILES, 1 /*MSGFLT_ADD*/);
            pChangeFilter(WM_COPYDATA, 1);
            pChangeFilter(0x0049 /*WM_COPYGLOBALDATA*/, 1);
        }
    }
#endif
    wxImageList* CDTLIST = new wxImageList(24, 24, true);
    CDXPM::CD = CDTLIST->Add(wxIcon(cd_xpm));
    CDXPM::DVD = CDTLIST->Add(wxIcon(dvd_xpm));
    CDXPM::DVDDL = CDTLIST->Add(wxIcon(dvddl_xpm));
    Installed_game_list->SetImageList(CDTLIST, wxIMAGE_LIST_SMALL);
    game_list__->SetImageList(CDTLIST, wxIMAGE_LIST_SMALL);
#if !PFSSHELL_ALLOWED
    PFSBrowserCall->Enable(false);
#endif
    std::cout << "Standard error: "<< cat_errdump() << "\n";
    ACTIVATE_DEBUG_LOG()
    wxFileName fname( wxTheApp->argv[0] );
    wxString config_file = fname.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR) + "Common\\config.INI";
    wxFileConfig * main_config = new wxFileConfig( wxEmptyString, wxEmptyString, config_file);
    if(wxFileExists(config_file))
    {
        COLOR(08) cout << "> Config File Loaded!\n";
    }
    else
    {
        COLOR(0c) cerr << "Can't load config file!\n  Loading default values\n";
    }
    main_config->Read("Installation/DataBase_Mode", &CFG::DBMODE, DB_INTERNAL);
    main_config->Read("Artwork/DataBaseURL", &CFG_ARTURL, "https://archive.org/download/OPLM_ART_2023_07/OPLM_ART_2023_07.zip/PS2%2F");
    main_config->Read("Init/Debug_level", &CFG::DEBUG_LEVEL, 5);
    main_config->Read("Installation/MiniOPL", &CFG::MINIOPL_WARNING, 1);
    main_config->Read("Installation/OSD_Hide", &CFG::OSD_HIDE, 0);
    main_config->Read("Game_search/Default_iso_path", &CFG::DEFAULT_ISO_PATH, "C:\\");
    main_config->Read("Installation/Default_dma", &CFG::DMA, 7);
    main_config->Read("Installation/Custom_icons", &CFG::LOAD_CUSTOM_ICONS, true);
    main_config->Read("FUSE/default_mountpoint", &CFG::mountpoint, "X");
    main_config->Read("FUSE/opl_partition", &CFG::default_OPLPART, "+OPL");
    main_config->Read("Installation/inform_unknown_ID", &CFG::SHARE_DATA, false);
    main_config->Read("HDDManager/display_games_titles", &CFG::HDDManagerGameTitleDISP, true);
    main_config->Read("HDDManager/display_subpartition", &CFG::HDDManagerSubPartDSP, false);
    main_config->Read("FEATURES/allow_experimental", &CFG::ALLOW_EXPERIMENTAL, false);
    main_config->Read("Installation/auto_download_assets", &CFG::AUTO_ASSETS, false);

//  main_config->Read("Init/Check_for_Updates",&CFG::UPDATE_WARNINGS,false);
    COLOR(08)
    cout <<"database mode="     << CFG::DBMODE                                     <<endl;
    cout <<"debug level="       << CFG::DEBUG_LEVEL                                <<endl;
    cout <<"MiniOPL="           << CFG::MINIOPL_WARNING                            <<endl;
    cout <<"OSD_Hide="          << CFG::OSD_HIDE                                   <<endl;
    cout <<"Default_iso_path="  << std::string(CFG::DEFAULT_ISO_PATH.mb_str())     <<endl;
    cout <<"Default DMA Mode="  << DMA_TABLE[CFG::DMA]                             <<endl;
    cout <<"Custom Icon Loader="<< CFG::LOAD_CUSTOM_ICONS                          <<endl;
    cout <<"Artwork URL="       << CFG_ARTURL                                      <<endl;
    cout <<"Game Title Database, "<<GAME_AMOUNT<<" ID's registered\n";
    COLOR(07)
    delete main_config;
    fname = wxTheApp->argv[0];

    EXEC_PATH   = fname.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR);
    HDL_CACHE   = EXEC_PATH + "info.sys";
    MBR_CACHE   = EXEC_PATH + "MBR.KELF";
    MiniOPL     = EXEC_PATH + "boot.kelf";
    ICONS_FOLDER= EXEC_PATH + "Common\\HDD-OSD-Icon-Database-main\\ico";
    first_init = true;
    if (!wxDirExists(ICONS_FOLDER)) wxMkDir(ICONS_FOLDER);
    // Auto-detection du HDD au demarrage (lit directement le disque ; silencieux si aucun).
    CallAfter([this]()
    {
        m_startupDetect = true;
        wxCommandEvent dummy;
        OnButton1Click(dummy);
        m_startupDetect = false;
    });
}

void cache_cleanup(void)
{
    if (wxFileExists(HDL_CACHE))
    {
        wxRemoveFile(HDL_CACHE);
    }
    if (wxFileExists(MBR_CACHE))
    {
        wxRemoveFile(MBR_CACHE);
    }
    if (wxFileExists("errdump.txt"))
    {
        wxRemoveFile("errdump.txt");
    }
}

HDL_Batch_installerFrame::~HDL_Batch_installerFrame()
{
    // Joint le thread de vignettes s'il tourne encore (evite std::terminate)
    m_artStop = true;
    if (m_artThread.joinable()) m_artThread.join();
    //(*Destroy(HDL_Batch_installerFrame)
    //*)
}

void HDL_Batch_installerFrame::OnQuit(wxCommandEvent& event)
{
    StopArtPrefetch(); // joint le thread de vignettes avant de quitter
    cache_cleanup();
    exit(0);
}


bool HDL_Batch_installerFrame::is_PS2(wxString path, int* disct)
{
    wxString CMD = wxString::Format("HDL.EXE cdvd_info2 \"%s\"",path), result_;
    wxArrayString result, errorBuffer;
    std::string couterr;
    long program_return_value = wxExecute(CMD,result,errorBuffer);
    if (program_return_value == 0)
    {
        if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) )
        {
            for (size_t x=0; x<result.GetCount(); x++)
            {
                result_ += result.Item(x);   //pass contents of array into a single string
            }
            COLOR(08)
            cout << ">\t["<< result_ << "]\n";
        }
        if (disct != nullptr) {
            if (result_.find("CD") != wxNOT_FOUND) *disct = CDXPM::CD;
            if (result_.find("DVD") != wxNOT_FOUND) *disct = CDXPM::DVD;
            if (result_.find("dual layer") != wxNOT_FOUND) *disct = CDXPM::DVDDL;
        }
        COLOR(0a)
        cout << "is a PS2 Game\n";
        COLOR(07)
        return true;
    }
    else
    {
        COLOR(0c) std::cerr << " Error\n";
        for (size_t x=0; x<errorBuffer.GetCount(); x++)
        {
            couterr += errorBuffer.Item(x);
        }
        cerr <<"\t" << couterr <<"\n";
        COLOR(07)
        return false;
    }
}

void HDL_Batch_installerFrame::OnAbout(wxCommandEvent& event)
{
    wxString msg;
    msg.Printf("version %i.%i.%i [%s]-[%s]\n\n Revision: %i\n build date: %s-%s-%s",
               AutoVersion::MAJOR,
               AutoVersion::MINOR,
               AutoVersion::BUILD,
               AutoVersion::STATUS,
#if (BITS == 32)
               "32 bits",
#elif (BITS == 64)
               "64 bits",
#endif
               AutoVersion::REVISION,
               AutoVersion::DATE,
               AutoVersion::MONTH,
               AutoVersion::YEAR
              );
    wxMessageBox(msg, "HDL Batch Installer");
    About About_DLG(this);
    About_DLG.ShowModal();
}

void HDL_Batch_installerFrame::OnRadioButton1Select(wxCommandEvent& event)
{
    cout << "selected Local HDD\n";
    selected_hdd->Enable();
}

void HDL_Batch_installerFrame::OnNotebook1PageChanged(wxNotebookEvent& event)
{}

void HDL_Batch_installerFrame::OnTextCtrl1Text(wxCommandEvent& event)
{}

void HDL_Batch_installerFrame::OnListBox1Select(wxCommandEvent& event)
{}

void HDL_Batch_installerFrame::OnCheckListBox1Toggled(wxCommandEvent& event)
{}

void HDL_Batch_installerFrame::OnListCtrl1BeginDrag(wxListEvent& event)
{
    int W;
    game_list__->GetClientSize(&W, NULL);
    game_list__->SetColumnWidth(0, W);
}

void HDL_Batch_installerFrame::OnSEARCH_ISOClick(wxCommandEvent& event)
{
    if((wxFileExists(MiniOPL)==false) && (CFG::MINIOPL_WARNING==1))
    {
        wxMessageBox(_("OPL Launcher is missing.\n you must get a copy of MiniOPL from the GitHub repo and place it back with the program before Installing games,\n otherwise... your PS2 games won't be executed from HDD-OSD"), warning_caption, wxICON_WARNING);
        COLOR(0e) cout << "> WARNING, boot.kelf is Missing...\n";
        COLOR(07)
    }
    wxBeginBusyCursor();

    COLOR(0f) cout << "Loading ISO's------------------------\n";
    COLOR(07)
    ISO_SEARCH_DIALOG = new wxFileDialog(this,
                                         _("Select your PS2 games"),
                                         CFG::DEFAULT_ISO_PATH,
                                         "",
                                         wxString::Format("%s|*.ISO;*.CUE;*.NRG;*.GI;*.IML;*.ZSO;*.zip;*.rar;*.7z|%s|*.zip;*.rar;*.7z|%s|*.ISO|%s|*.cue|%s|*.nrg|%s|*.gi|%s|*.iml|%s|*.zso",
                                                 _("All supported formats (ISO/CUE/NRG/GI/IML/ZSO + archives)"),
                                                 _("Archives (*.zip;*.rar;*.7z)"),
                                                 _("ISO 9660 (*.ISO)"),
                                                 _("CDRWIN cuesheets (*.cue)"),
                                                 _("Nero images (*.nrg)"),
                                                 _("Global images (*.gi)"),
                                                 _("IML files (*.iml)"),
                                                 _("lz4 Compressed Image Files (*.zso)") ),
                                         wxFD_DEFAULT_STYLE|wxFD_OPEN|wxFD_FILE_MUST_EXIST|wxFD_MULTIPLE,
                                         wxDefaultPosition,
                                         wxDefaultSize,
                                         _T("wxFileDialog")
                                        );
    wxArrayString ISO_PATHS;
    int valid_gamecount = 0, invalid_gamecount = 0;
    wxString path;
    if (ISO_SEARCH_DIALOG->ShowModal() == wxID_OK)
    {
        bool warned_about_ZSO_usage = false;
        ISO_SEARCH_DIALOG->GetPaths(ISO_PATHS);
        std::cout << "loaded ISO's: " << ISO_PATHS.GetCount() <<std::endl;
        for (size_t game_count = 0; game_count < ISO_PATHS.GetCount(); game_count++)
        {
            bool isZSO=false;
            path = ISO_PATHS.Item(game_count);
            {
                wxString aext = path.AfterLast('.').Lower();
                if (aext == "zip" || aext == "rar" || aext == "7z")
                {
                    m_pending.Add(path); InsertPendingRow(path); // archive: extraite a l'install
                    valid_gamecount++;
                    continue;
                }
            }
            wxString extension = path.Right(4);
            if (!extension.CmpNoCase(".zso")) // ZSO: make sure HDL Dump will be able to process the file, by using the original ISO placed on the same folder.
            {
                path.RemoveLast(3);
                path += "iso";
                if (!wxFileExists(path))
                {
                    COLOR(0c)
                    std::cerr << "! Cannot find '" <<path<< "' to install ZSO game\n";
                    COLOR(07)
                    if (!warned_about_ZSO_usage)
                    {
                        wxMessageBox(_("In order to install ZSO games, the original ISO must be placed in the same folder keeping the same filename.\n"
                                   "This is done to recover information needed to install the game\n\n"
                                   "Example: if your game is named 'God_Of_War.ZSO', put the original ISO on the same folder named 'God_Of_war.ISO'"),error_caption,wxICON_ERROR|wxCENTRE);
                        warned_about_ZSO_usage = true;

                    }
                    invalid_gamecount++;
                    continue;
                } else {
                    std::cout << "using '" <<path<< "' to install ZSO\n";
                    isZSO = true;
                }
            }
            std::cout << path << "\n";
            int type = -1;
            if (is_PS2(path, &type))
            {
                m_pending.Add(path); InsertPendingRow(path);
                valid_gamecount++;
            }
            else
            {
                invalid_gamecount++;
            }
        }
        if (invalid_gamecount > 0)
        {
            wxString msg;
            if (valid_gamecount > 0)
            {
                msg.append(_("Loaded:"));
                msg.append(wxString::Format(" %i ", valid_gamecount) );
                msg.append(_("PS2 Games"));
            }
            msg.append("\n");
            msg.append(_("discarded"));
            msg.append(wxString::Format(" %i ", invalid_gamecount) );
            msg.append(_("invalid ISO's"));
            wxMessageBox(msg, _("Information:"));
        }
    }
    COLOR(0f) cout << "Loaded ISO's------------------------\n";
    COLOR(07)
    UpdateInstallButton();
    wxEndBusyCursor();
}

void HDL_Batch_installerFrame::OnTextCtrl1Text1(wxCommandEvent& event)
{
}

void HDL_Batch_installerFrame::OnClose(wxCloseEvent& event)
{
    StopArtPrefetch(); // joint le thread de vignettes avant de quitter
    CleanIsoStage();
    cache_cleanup();
    exit(0);
}
void HDL_Batch_installerFrame::Onselected_hddSelect(wxCommandEvent& event)
{
    Update_hdd_data();
}

void HDL_Batch_installerFrame::OnButton1Click(wxCommandEvent& event)
{
    const wxString command = "HDL.EXE query";
    wxArrayString result;
    wxString line, hdd_number;
    wxFileName fname( wxTheApp->argv[0] );
    wxString HDL_DUMP = fname.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR) + "HDL.EXE";

    if(!wxFileExists(HDL_DUMP))
    {
        Component_error error_dlg(this);
        error_dlg.ShowModal();
    }
    wxBeginBusyCursor();

    selected_hdd->Clear();
    int HDDCount = 0;
    wxExecute(command,result);
    for (size_t index = 0; index < result.GetCount(); index++)
    {
        line = result.Item(index);
        if (line.find("hdd") == NOT_FOUND) continue;
        hdd_number = line.Mid(4,1);
        if (!(line.find("Playstation") == NOT_FOUND)) // if "playstation" found, we've encountered a ps2 hdd
        {
            selected_hdd->Append(line.substr(1,line.find_first_of(':')));

            COLOR(0a) cout << line <<endl; COLOR(07)

            HDDCount++;
        }
        else
        {
            cout << line <<endl;
        }
    }
    wxEndBusyCursor();

    if (HDDCount == 0 && !m_startupDetect)
    {
        wxMessageBox(_("no PS2 HDD's Found"),_("warning"),wxICON_EXCLAMATION);
    }
    if (selected_hdd->GetCount() == 1)
    {
        selected_hdd->SetSelection(0);
        Update_hdd_data();
    }
    CAN_COPY_HDD = (HDDCount > 1);
    COPYHDD->Enable(CAN_COPY_HDD);
}

void HDL_Batch_installerFrame::OnListCtrl1BeginDrag1(wxListEvent& event) {}

void HDL_Batch_installerFrame::OnParse_hdl_tocClick(wxCommandEvent& event)
{
    List_refresh_request();
}

// Process qui capture sa sortie et retient son code de fin (install non bloquante).
class CapturingProcess : public wxProcess
{
public:
    CapturingProcess() { Redirect(); }
    bool ended = false; int code = -1;
    virtual void OnTerminate(int, int status) override { code = status; ended = true; }
};

// Extrait le dernier "NN%" d'un texte (format HDL.EXE : "[...] 12%, ... remaining").
static bool ParseLastPercent(const wxString& s, int& out)
{
    int pos = s.Find('%', true);
    if (pos <= 0) return false;
    int end = pos - 1;
    while (end >= 0 && s[end] == ' ') end--;
    int beg = end;
    while (beg >= 0 && ((s[beg] >= '0' && s[beg] <= '9') || s[beg] == '.')) beg--;
    if (beg == end) return false;
    double v;
    if (!s.Mid(beg + 1, end - beg).ToDouble(&v)) return false;
    out = (int)(v + 0.5);
    if (out < 0) out = 0;
    if (out > 100) out = 100;
    return true;
}

// Lance HDL.EXE en process CACHE, draine sa sortie EN DIRECT dans la console
// integree, et met a jour la jauge (m_rowGauge) a partir du pourcentage parse.
long HDL_Batch_installerFrame::RunInstallCaptured(const wxString& command)
{
    CapturingProcess proc;
    long pid = wxExecute(command, wxEXEC_ASYNC | wxEXEC_HIDE_CONSOLE, &proc);
    if (!pid) { std::cout << "! failed to launch HDL.EXE\n"; return -1; }
    auto drain = [this](wxInputStream* s)
    {
        if (!s) return;
        char buf[4096];
        s->Read(buf, sizeof(buf) - 1);
        size_t n = s->LastRead();
        if (!n) return;
        buf[n] = 0;
        wxString chunk = wxString::FromUTF8(buf, n);
        if (chunk.empty()) chunk = wxString(buf, wxConvLibc, n);
        int pct;
        if (m_rowGauge && ParseLastPercent(chunk, pct)) m_rowGauge->SetValue(pct);
        chunk.Replace("\r", "\n");
        if (LogPanel)
        {
            LogPanel->AppendText(chunk);
            if (LogPanel->GetNumberOfLines() > 3000)
                LogPanel->Remove(0, LogPanel->XYToPosition(0, LogPanel->GetNumberOfLines() - 1500));
        }
    };
    while (!proc.ended)
    {
        while (proc.IsInputAvailable()) drain(proc.GetInputStream());
        while (proc.IsErrorAvailable()) drain(proc.GetErrorStream());
        wxYield();
        wxMilliSleep(35);
    }
    while (proc.IsInputAvailable()) drain(proc.GetInputStream());
    while (proc.IsErrorAvailable()) drain(proc.GetErrorStream());
    return proc.code;
}

void HDL_Batch_installerFrame::OninstallClick(wxCommandEvent& event)
{
    if (m_installing) { m_pauseRequested = true; install->SetLabel(_("Pausing...")); return; }
    StopArtPrefetch(); // libere le device : l'install pompe les evenements pendant HDL.EXE (device attache = crash HDL Dump)
    int not_enough_space_count = 0;
    wxString messagebuffer, HIDE_SWITCH, strr, msg, command1;
    wxString hddd = selected_hdd->GetString(selected_hdd->GetSelection());
    long original_item_count = m_pending.GetCount();
    long installation_retcode;

    wxBeginBusyCursor();

    HIDE_SWITCH = (CFG::OSD_HIDE) ? " -hide" : "";
    if (original_item_count == 0)// IF gamelist is empty, get out
    {
        wxMessageBox(_("no games selected for installation"), error_caption,wxICON_EXCLAMATION);
        wxEndBusyCursor();
        return;
    }

    if ( (!wxFileExists(EXEC_PATH+"Common\\HDD-OSD-Icon-Database-main\\README.md")) && (CFG::LOAD_CUSTOM_ICONS))
    {
        wxEndBusyCursor();
        ask_2_download_icons();
        wxBeginBusyCursor();
    }
#if PFSSHELL_ALLOWED
    PFSSHELL.CloseDevice(); //PFSShell with device attached will make HDL Dump write features crash
#endif
    std::cout <<"game count: "<< original_item_count<<std::endl;
    cout << "> begining installation...\n";
    // Installation NON bloquante : la fenetre principale reste visible (console + liste).
    m_installing = true; m_pauseRequested = false;
    install->SetLabel(_("Pause")); install->Enable(true);
    clear_iso_list->Enable(false);
    if (!m_rowGauge) m_rowGauge = new MiniProgressBar(Installed_game_list);
    m_rowGauge->Hide();
    /// /////////////////////////////////MAIN INSTALL LOOP///////////////////////////////// ///
    //for (int current_index = 0; current_index < game_list__->GetItemCount(); current_index++)
    int report_counter = 0;
    wxArrayString _filepath,
                  _reason,
                  _ELF,
                  _media
//                   _DBA,
                  ;
    wxAppProgressIndicator *toolbar_progress = new wxAppProgressIndicator(this, original_item_count);
    while (!m_pending.IsEmpty())
    {
        command1.clear();
        if (m_pauseRequested) break;
        int current_index = (original_item_count - (long)m_pending.GetCount());
        toolbar_progress->SetValue(current_index);
        strr = m_pending.Item(0);
        wxString origName = strr.AfterLast('\\').AfterLast('/'); // nom affiche dans la liste (avant extraction archive)
        wxString archiveStage; // si archive: dossier de staging a supprimer apres l'install reussie
        {
            wxString aext = strr.AfterLast('.').Lower();
            if (aext == "zip" || aext == "rar" || aext == "7z")
            {
                std::cout << "> archive detected, extracting before install: " << strr.ToStdString() << "\n";
                wxArrayString extracted = ExtractArchiveGames(strr);
                if (extracted.IsEmpty())
                {
                    report_counter++;
                    _filepath.Add(strr);
                    _ELF.Add(wxEmptyString);
                    _media.Add("archive");
                    _reason.Add(_("No PS2 game image was found inside the archive."));
                    m_pending.RemoveAt(0);
                    continue;
                }
                archiveStage = EXEC_PATH + "_iso_stage\\" + wxFileName(strr).GetName();
                strr = extracted.Item(0); // installe la premiere image extraite
            }
        }
        wxString resultt;
        std::cout << "\n>index: " << current_index <<std::endl;
        msg = _("installing: ");
        msg.append(strr);
        // positionne la jauge de progression sur la ligne du jeu en cours
        {
            long grow = Installed_game_list->FindItem(-1, origName);
            wxRect rr;
            if (grow >= 0 && m_rowGauge && Installed_game_list->GetItemRect(grow, rr))
            {
                int gw = 120, gh = 16;
                if (gw > rr.width - 8) gw = rr.width - 8;
                m_rowGauge->SetSize(rr.x + rr.width - gw - 4, rr.y + (rr.height - gh) / 2, gw, gh);
                m_rowGauge->SetValue(0);
                m_rowGauge->Show();
                Installed_game_list->EnsureVisible(grow);
            }
        }
        wxString ELF, title;
        std::string inject_mode;
        ///-------------------------------PULL DATA
        wxArrayString result;
        wxString command0;
        command0.clear();
        command0.Printf("HDL.EXE cdvd_info2 \"%s\"",strr);
        result.Clear();
        wxExecute(command0,result);
        ///------------------------------/PULL DATA
        resultt.clear();//clear buffer
        cout << result.Item(0) << endl;    ///write output to log
        resultt = result.Item(0); //pass data from array to single string
        if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) ) cout << "count(cdvd_info2): " << result.GetCount()<<std::endl;

        inject_mode = (resultt.StartsWith("CD")) ? "inject_cd " : "inject_dvd ";
        std::cout << "media: " << inject_mode <<std::endl;
        ///ID
        size_t z;
        z = resultt.find_first_of("\""); //1st one
        z = resultt.find_first_of("\"",z + 1); //2nd one
        z = resultt.find_first_of("\"",z + 1); //3rd one
        ELF = resultt.Mid(z+1); ///DEJO ACAz
        ELF = ELF.substr(0,ELF.find_last_of("\""));
        /// READ DATABASE
        title = get_gamename(std::string(ELF.mb_str()), CFG::DBMODE, CFG::DBENABLE); //retrieve gamename from Database
        if (title == NO_MATCH) //if not found || databse usage disabled
        {
            title = strr.substr(strr.find_last_of("\\") + 1); //use Filename instead
            title = title.substr(0, title.find_last_of('.') ); //cut extension
        }
        while (title.EndsWith(' '))
            title.RemoveLast(1); //clean trailing whitespaces
        int DD = dma_choice->GetSelection();
        ///           hdl_dump    !disctype!   !hdlhdd!      "!title!"         "!filename!"  !gameid!       *u4             -hide
        //command1 = "HDL.EXE " + inject_mode + hddd + " \"" + title + "\" \"" + strr + "\" " + ELF + " " + DMA_TABLE[DD] + HIDE_SWITCH;
        if (CFG::LOAD_CUSTOM_ICONS)
            Load_custom_icon(ELF);
        command1.Printf("HDL.EXE %s %s \"%s\" \"%s\" %s %s %s",
                        inject_mode, hddd, title, strr, ELF, DMA_TABLE[DD], HIDE_SWITCH);
        COLOR(0f )std::cout << "---\n"
                            << " inject_mode: [" << inject_mode
                            << "]\n hdd_target: [" << hddd
                            << "]\n title: [" << title << "]"
                            << "\n ISO_PATH: ["<< strr << "] "
                            << "\n ELF ["<< ELF <<"]"
                            << "\n DMA: "<< DMA_TABLE[DD]
                            << "\n Hide_switch: " << HIDE_SWITCH
                            << "\n---\n"
                            << command1 <<"\n";
        COLOR(0d)
        installation_retcode = RunInstallCaptured(command1);
        if (m_rowGauge) m_rowGauge->Hide();
        COLOR(08)
        //if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) )
        cout << "\n>returned value [" << installation_retcode <<"]\n";
        COLOR(07)

        if (installation_retcode != 0)
        {
            report_counter++;
            _filepath.Add(strr);
            _ELF.Add(ELF);
            _media.Add(wxString::Format("%il",installation_retcode));
            if( installation_retcode == RET_NO_SPACE)
            {
                _reason.Add(_("There is not enough space on the HDD to install this game"));
                not_enough_space_count++;
            }
            else if( installation_retcode == -1) {
                _reason.Add(wxString::Format(_("Standard error: %s"), cat_errdump()));
            }

            else if( installation_retcode == RET_NO_MEM)
                _reason.Add(_("HDL-Dump reported \"Out of memory\"."));

            else if( installation_retcode == -1073741819)
                _reason.Add(_("HDL-Dump Crashed"));

            else if( installation_retcode == RET_NOT_APA)
                _reason.Add(_("not a PlayStation 2 HDD."));

            else if ( installation_retcode == RET_PART_EXISTS)
                _reason.Add(_("A game with this name is already installed"));

            else if ( installation_retcode == RET_BAD_SYSCNF)
                _reason.Add(_("The game has a corrupt SYSTEM.CNF file"));

            else if ( installation_retcode == RET_FILE_NOT_FOUND)
                _reason.Add(_("File could not be found (or accesed?)"));

            else if ( installation_retcode == RET_BROKEN_LINK)
                _reason.Add(_("CUE or IML have a missing linked file"));

            else if ( installation_retcode == RET_MULTITRACK)
                _reason.Add(_("bin/cue with multiple .bin files are not supported, combine them into a single bin"));

            else
                _reason.Add("Unhandled error. Check program log for more details...");
        }
        if (installation_retcode == 0 && CFG::AUTO_ASSETS)
        {
            std::cout << "> downloading assets for " << ELF.ToStdString() << " ...\n";
            DownloadAssetsForELF(ELF);
        }
        m_pending.RemoveAt(0);
        if (!archiveStage.IsEmpty() && installation_retcode == 0)
            wxFileName::Rmdir(archiveStage, wxPATH_RMDIR_RECURSIVE); // ISO extrait supprime apres install reussie

        if (not_enough_space_count > 3)
        {
            wxMessageBox(_("Installation process aborted, HDD is running out of space"), warning_caption, wxICON_WARNING);
            break;
        }

    }/// /////////////////////////////////MAIN INSTALL LOOP///////////////////////////////// ///
    bool wasPaused = m_pauseRequested;
    m_installing = false;
    install->SetLabel(_("Install"));
    clear_iso_list->Enable(true);
    if (m_rowGauge) m_rowGauge->Hide();
    COLOR(08) std::cout << endl << (wasPaused ? "> installation paused.\n" : "> installation process finished.\n");
    delete toolbar_progress;
    COLOR(07)
    if (wasPaused && !m_pending.IsEmpty())
        wxMessageBox(wxString::Format(_("Installation paused.\n\n%d game(s) remaining. Click 'Install' to resume."),
                                      (int)m_pending.GetCount()), _("Paused"), wxICON_INFORMATION);
    ///
    if (report_counter != 0)
    {
        Post_Install_Report* REPORT = new Post_Install_Report(this,_filepath,_reason,_ELF,_media);
        REPORT->ShowModal();
        delete REPORT;
    }
    std::cout << "> Updating HDD information...\n";
    Update_hdd_data();
    if (wxFileExists(HDL_CACHE))//Clean BBNav cache
    {
        COLOR(08) cout << "> erasing \"info.sys\" \n";
        COLOR(07)
        wxRemoveFile(HDL_CACHE);
    }
    if (wxFileExists(EXEC_PATH + "list.ico"))
    {
        COLOR(08) cout << "> erasing \"list.ico\" \n";
        COLOR(07)
        wxRemoveFile(EXEC_PATH + "list.ico");
    }
    if (m_pending.IsEmpty()) CleanIsoStage(); // ISO extraits d'archives supprimes une fois tout installe
    UpdateInstallButton();
    wxEndBusyCursor();
    wxBell();
}

void HDL_Batch_installerFrame::OnCheckBox1Click(wxCommandEvent& event)
{}

void HDL_Batch_installerFrame::OnSettings(wxCommandEvent& event)
{
    Config configmenu(this);
    configmenu.ShowModal();
}

void HDL_Batch_installerFrame::OnPaint(wxPaintEvent& event)
{
}

void HDL_Batch_installerFrame::OnCheckBox1Click1(wxCommandEvent& event)
{
    CFG::DBENABLE = use_database->IsChecked();
    std::cout <<"> use database ="<<CFG::DBENABLE<<std::endl;
}

void HDL_Batch_installerFrame::OnMBR_EVENTClick(wxCommandEvent& event)
{
    ArtPause _artpause(this); // ecriture HDL.EXE : device doit etre detache -> pause du loader
    if (wxMessageBox(_("This feature will rewrite the PS2 BOOTSTRAP PROGRAM for this HDD.\n"
                       "Please make sure you know what you are doing.\n"
                       "MBR Programs must be headerless binaries, uncompressed, compiled with fixed load adress at 0x100000 and encrypted with KELFTool\n\n"
                       "Do you want to continue with this operation?"),
                     _("IMPORTANT WARNING"),
                     wxICON_WARNING|wxYES_NO|wxNO_DEFAULT, this) == wxNO)
        return;
    wxString MBR_PATH, command, output_string;
    wxArrayString output;
    wxFileName fname( wxTheApp->argv[0] );
    wxString MBR_HOME = fname.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR) + "MBR.KELF";
    wxString label = selected_hdd->GetString(selected_hdd->GetSelection());
    if (MBR_search->ShowModal() == wxID_OK)
    {
        MBR_PATH = MBR_search->GetPath();
        cout << "copying\n" << MBR_PATH <<"\n to:\n" <<MBR_HOME<<"\n";
        if (wxCopyFile(MBR_PATH,MBR_HOME,true)==false) //if copy failed...
            wxMessageBox(_("can't Load MBR.KELF"), "ERROR", wxICON_ERROR);
        else
        {
            command = "HDL.EXE inject_mbr " + label + " MBR.KELF";
            wxExecute(command,output);
            for (size_t x=0; x< output.GetCount(); x++)
            {
                output_string += output.Item(x);
                output_string += "\n";
            }
            cout << output_string << "\n";
        }
    }
    else
    {
        COLOR(08) cout<<"> operation cancelled\n";
    }

    if (wxFileExists("./MBR.KELF"))
        {
            COLOR(08) cout <<"> Cleaning MBR.KELF cache.\n";
            wxRemoveFile("./MBR.KELF");
        }
    COLOR(07)
}

void HDL_Batch_installerFrame::OnButton2Click(wxCommandEvent& event)
{
    wxMessageBox(_("work in progress"),"sorry",wxICON_WARNING);
    // modify_header mhw(this);
    // mhw.ShowModal();
}

void HDL_Batch_installerFrame::Onclear_iso_listClick(wxCommandEvent& event)
{
    if (m_pending.IsEmpty())
    {
        wxMessageBox(_("The install queue is already empty"),"",wxICON_EXCLAMATION);
        return;
    }
    if (wxMessageBox("",_("Clear the install queue??"),wxICON_QUESTION | wxYES_NO) == wxYES)
    {
        m_pending.Clear();
        CleanIsoStage(); // supprime aussi les ISO extraits d'archives
        if (selected_hdd->GetSelection() != wxNOT_FOUND) List_refresh_request();
        else Installed_game_list->DeleteAllItems();
        UpdateInstallButton();
        COLOR(08) cout<<"> cleared install queue\n"; COLOR(07)
    }
}

void HDL_Batch_installerFrame::OnUpdateRequest(wxCommandEvent& event)
{
    wxLaunchDefaultBrowser("https://github.com/israpps/HDL-Batch-installer/releases");
}

void HDL_Batch_installerFrame::OnIssueReport(wxCommandEvent& event)
{
    wxLaunchDefaultBrowser("https://github.com/israpps/HDL-Batch-installer/issues");
}


void HDL_Batch_installerFrame::Selected_games(wxListEvent& event)
{
}

void HDL_Batch_installerFrame::UnSelected_games(wxListEvent& event)
{
}

void HDL_Batch_installerFrame::Onpull_isoClick(wxCommandEvent& event)
{
}

void HDL_Batch_installerFrame::Ondma_choiceSelect(wxCommandEvent& event)
{
    if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) )
    {
        int X = dma_choice->GetSelection();
        COLOR(08) cout << "> Selected DMA Mode "<< X <<" ["<< DMA_TABLE[X] <<"]\n";
        COLOR(07)
    }
}
void HDL_Batch_installerFrame::Enable_HDD_dependant_objects(bool WTF_should_I_do)
{
    if (WTF_should_I_do)
    {
        Parse_hdl_toc->Enable();
        install->Enable();
        MBR_EVENT->Enable();
        dma_choice->Enable();
        mass_header_injection->Enable();
        MBRExtractRequest->Enable();
        FUSE->Enable();
#if PFSSHELL_ALLOWED
        HDDManagerButton->Enable(PFSSHELL_USABLE);
#else
        HDDManagerButton->Enable(true);
#endif
        if (m_hddMenu)
        {
            m_hddMenu->Enable(ID_BUTTON5,  true);  // Inject MBR
            m_hddMenu->Enable(ID_BUTTON13, true);  // Inject OPL Launcher (all games)
            m_hddMenu->Enable(ID_BUTTON9,  true);  // Recover MBR
            m_hddMenu->Enable(ID_BUTTON11, true);  // Mount HDD Partition
            m_hddMenu->Enable(ID_BUTTON10, PFSSHELL_USABLE); // HDD Manager
        }
        ///this one has nothing to do
        Installed_game_list->DeleteAllItems();
    }
    else
    {
        if (m_hddMenu)
        {
            m_hddMenu->Enable(ID_BUTTON5,  false);
            m_hddMenu->Enable(ID_BUTTON13, false);
            m_hddMenu->Enable(ID_BUTTON9,  false);
            m_hddMenu->Enable(ID_BUTTON11, false);
            m_hddMenu->Enable(ID_BUTTON10, false);
        }
        if (m_spaceBar) m_spaceBar->Clear();
        Parse_hdl_toc->Disable();
        install->Disable();
        MBR_EVENT->Disable();
        dma_choice->Disable();
        mass_header_injection->Disable();
        MBRExtractRequest->Disable();
        FUSE->Disable();
        HDDManagerButton->Disable();
    }
}

void HDL_Batch_installerFrame::Update_hdd_data(void)
{
    StopArtPrefetch(); // usage exclusif de PFSSHELL ici ; un rafraichissement de liste relancera le loader
    Enable_HDD_dependant_objects(false);///Temporarly disble just in case data parsing fails
    ///MAKE SURE TO RE-ENABLE EVERYTHING THAT WAS DISABLEd HERE INSIDE THE 'if (toc_ret == 0)' BLOCK
    wxString command;
    wxString label = selected_hdd->GetString(selected_hdd->GetSelection());
    HDD_TOKEN = wxString::Format("\\\\.\\PHYSICALDRIVE%s", label.SubString(3, label.find(':')-1));
    // N'invalide le cache memoire des vignettes QUE si le HDD a reellement change
    // (sinon un simple re-select rechargerait tout inutilement ; le cache disque reste, lui, valable).
    if (m_artCacheHdd != HDD_TOKEN) { m_artCache.clear(); m_artCacheHdd = HDD_TOKEN; }
    cout << "selected "<< label <<endl;
    command.Printf("HDL.EXE toc %s",label);
    wxArrayString result,std_error;
    long toc_ret = wxExecute(command,result,std_error);
    wxString line, TMP;
    long int size_total, size_used, size_free;
    if (toc_ret == 0)
    {
        for(size_t x=0; x < result.GetCount(); x++) //Parse line-to-line :D
        {
            line = result.Item(x);


            if (line.find(", used") == NOT_FOUND)
                continue;
            else
            {
                if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) )
                    cout << line <<"\n";
                TMP =  line.SubString(line.find_first_of(":") + 2,line.find_first_of("MB") - 1);
                TMP.ToLong(&size_total);
                TMP.clear();
                int A = line.find_first_of(",") + 8,
                    B = line.find_first_of("MB",A)-1;
                TMP  = line.SubString(A, B);
                TMP.ToLong(&size_used);
                TMP.clear();
                TMP  = line.SubString(line.find_last_of(":") + 2,line.find_last_of("MB") - 2);
                TMP.ToLong(&size_free);
            }
        }
        std::cout << "total: ["<<size_total<<"]\nused:  ["<<size_used<<"]\nfree:  ["<<size_free<<"]\n";
        m_spaceBar->SetUsage(size_total, size_used, size_free);
#if PFSSHELL_ALLOWED
        std::cout << "initializing libPS2HDD...\n";
        if (!PFSSHELL.SelectDevice(HDD_TOKEN.c_str()))
        {
            PFSSHELL_USABLE = true;
        } else {
            PFSSHELL_USABLE = false;
            wxMessageBox(("Error initializing libps2hdd service\n\nCheck log for more details\n\nHDD formatting and HDD Manager disabled"), wxMessageBoxCaptionStr, wxICON_WARNING);
            PFSBrowserCall->Enable(false);
        }
        PFSSHELL.CloseDevice();
        MenuHDDFormat->Enable(PFSSHELL_USABLE);
#endif
        Enable_HDD_dependant_objects(true); //re-enable & clean installed game list
        List_refresh_request(); //auto: lister les jeux des qu'un HDD est selectionne (plus besoin de "get list")
    }
    else
    {
        COLOR(0c)
        cerr << "ERROR ---\n";
        for (size_t x = 0; x < std_error.GetCount(); x++)
            cerr << std::string(std_error.Item(x)) << "\n";
        cerr << "---------"<<std::endl;
        COLOR(07)
        if (toc_ret == RET_BAD_APA)
            wxMessageBox(_("The HDD is corrupted or your hard drive connection has issues"), _("APA Partition is broken"), wxICON_ERROR);
    }
}

void HDL_Batch_installerFrame::OnButton2Click1(wxCommandEvent& event)
{
}

void HDL_Batch_installerFrame::List_refresh_request()
{
    StopArtPrefetch(); // stoppe un chargement de vignettes en cours (device libere avant HDL.EXE)
    // 1) recupere la liste des jeux installes (donnees seulement)
    m_installedGames.clear();
    wxArrayString result;
    wxString command = "HDL.EXE hdl_toc " + selected_hdd->GetString(selected_hdd->GetSelection());
    std::cout << "listing games of " << selected_hdd->GetString(selected_hdd->GetSelection()) << "\n\n";
    wxExecute(command, result);
    for (size_t x = 0; x < result.GetCount(); x++) cout << result.Item(x) << "\n";

    wxString line;
    for (size_t x = 0; x < result.GetCount(); x++)
    {
        line = result.Item(x);
        if (line.Mid(0,1) == 't') continue; // premiere ligne = entete
        int media = (line.Mid(0,1) == 'C') ? MEDIA_CD : MEDIA_DVD;
        long gamesize = wxAtoi(line.Mid(4, line.find_first_of("B") - 5));
        wxString ELF, Gamename;
        if (line.find('*') == NOT_FOUND)
        {
            ELF = line.SubString(34, line.find_first_of(" ",34)-1);
            Gamename = line.substr(line.find_first_of(" ",34) + 2);
        }
        else
        {
            ELF = line.SubString(line.find('*')+4, line.find_first_of(" ",line.find('*')+4)-1);
            Gamename = line.substr(line.find_first_of(" ",line.find('*')+4) + 2);
        }
        InstalledGameRow g;
        g.name = Gamename; g.elf = ELF;
        g.size = wxString::Format("%i", (int)(gamesize / 1024));
        g.media = media;
        m_installedGames.push_back(g);
    }
    if (result.GetCount() <= 2) wxMessageBox(_("This HDD has no PS2 Games inside"), error_caption);
    // 2) affichage IMMEDIAT (icones CD/DVD par defaut) -> pas d'attente meme sur grosse ludotheque
    RebuildFromCache();
    // 3) chargement des vignettes OPL en arriere-plan, par petits lots (UI reactive)
    StartArtPrefetch();
}

// (Re)construit l'affichage : jeux en attente (haut, jaune) + jeux installes
// filtres par le champ de recherche. Sans I/O (utilise m_installedGames + m_artCache).
void HDL_Batch_installerFrame::RebuildFromCache()
{
    Installed_game_list->Freeze();
    Installed_game_list->DeleteAllItems();
    wxImageList* il = Installed_game_list->GetImageList(wxIMAGE_LIST_SMALL);
    if (il) for (int k = il->GetImageCount() - 1; k >= 3; k--) il->Remove(k);

    wxString q = m_searchField ? m_searchField->GetValue().Lower() : wxString();
    for (int p = (int)m_pending.GetCount() - 1; p >= 0; p--) InsertPendingRow(m_pending.Item(p)); // en attente en haut

    for (size_t k = 0; k < m_installedGames.size(); k++)
    {
        const InstalledGameRow& g = m_installedGames[k];
        if (!q.IsEmpty() && !g.name.Lower().Contains(q) && !g.elf.Lower().Contains(q)) continue;
        long i = Installed_game_list->InsertItem(Installed_game_list->GetItemCount(), g.name);
        Installed_game_list->SetItem(i, 1, g.elf);
        long mb = 0; g.size.ToLong(&mb);
        Installed_game_list->SetItem(i, 2, wxString::Format("%ld MB (%.2f GB)", mb, mb / 1024.0));
        Installed_game_list->SetItem(i, 3, g.media == MEDIA_CD ? "CD" : "DVD");
        wxString elfT = g.elf; elfT.Trim();
        std::map<wxString, wxBitmap>::iterator it = m_artCache.find(elfT);
        if (il && it != m_artCache.end()) Installed_game_list->SetItemImage(i, il->Add(it->second));
        else Installed_game_list->SetItemImage(i, g.media == MEDIA_CD ? CDXPM::CD : CDXPM::DVD);
    }
    GameCountDisplay->Clear();
    GameCountDisplay->AppendText(wxString::Format(_("%d games"), (int)m_installedGames.size()));
    int size_ELFID = Installed_game_list->GetColumnWidth(1);
    int size_SIZE = Installed_game_list->GetColumnWidth(2);
    int size_MEDIA = Installed_game_list->GetColumnWidth(3);
    int TOTAL; Installed_game_list->GetClientSize(&TOTAL, NULL);
    Installed_game_list->SetColumnWidth(0, TOTAL - (size_ELFID + size_SIZE + size_MEDIA));
    UpdateInstallButton();
    Installed_game_list->Thaw();
}

void HDL_Batch_installerFrame::OnSearchText(wxCommandEvent& event)
{
    RebuildFromCache();
}

// Tri de la liste au clic sur un en-tete de colonne (re-clic = inverse le sens).
void HDL_Batch_installerFrame::OnListColClick(wxListEvent& event)
{
    int col = event.GetColumn();
    if (col < 0) return;
    if (col == m_sortCol) m_sortAsc = !m_sortAsc;
    else { m_sortCol = col; m_sortAsc = true; }
    Installed_game_list->ShowSortIndicator(m_sortCol, m_sortAsc); // fleche native dans l'en-tete
    int c = m_sortCol; bool asc = m_sortAsc;
    std::sort(m_installedGames.begin(), m_installedGames.end(),
        [c, asc](const InstalledGameRow& a, const InstalledGameRow& b)
        {
            int r;
            if (c == 1)      r = a.elf.CmpNoCase(b.elf);
            else if (c == 2) { long sa = 0, sb = 0; a.size.ToLong(&sa); b.size.ToLong(&sb); r = (sa < sb) ? -1 : (sa > sb) ? 1 : 0; }
            else if (c == 3) r = a.media - b.media;
            else             r = a.name.CmpNoCase(b.name);
            return asc ? (r < 0) : (r > 0);
        });
    RebuildFromCache();
}

void HDL_Batch_installerFrame::On_MiniOPL_Update_request(wxCommandEvent& event)
{
    wxString cmd = "common\\wget.exe -q --show-progress " + MiniOPL_URL + " -O \"" + MiniOPL + "\"";
    COLOR(0d)
    if (wxExecute(wxString::Format("common\\wget.exe -q --spider --no-cache \"%s\"", MiniOPL_URL)) == 0)
        wxExecute(cmd,wxEXEC_SYNC);
    COLOR(07)
}

void HDL_Batch_installerFrame::OnButton2Click3(wxCommandEvent& event)
{
#if PFSSHELL_ALLOWED
    ArtPause _artpause(this); // le HDD Manager (modal) monopolise le device : pause du loader (reprise a la fermeture)
    HDDManager *MANAGER = new HDDManager(this, HDD_TOKEN, CFG::HDDManagerGameTitleDISP, CFG::HDDManagerSubPartDSP);
    MANAGER->ShowModal();
    delete MANAGER;
#else
    PFSSHELL_DISABLED_WARNING();
#endif
}

void HDL_Batch_installerFrame::OnHDL_DumpUpdateRequest(wxCommandEvent& event)
{
    wxString HDL_DUMP       = fname.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR) + "HDL.EXE";
    wxString HDL_DUMP_URL   = "https://github.com/israpps/hdl-dump/releases/download/hdlinst/HDL.EXE";
    wxString cmd = "common\\wget.exe -q --show-progress " + HDL_DUMP_URL + " -O \"" + HDL_DUMP + "\"";
    COLOR(0d)
    if (wxExecute(wxString::Format("common\\wget.exe -q --spider --no-cache \"%s\"", HDL_DUMP_URL)) == 0)
        wxExecute(cmd,wxEXEC_SYNC);
    COLOR(07)
}

void HDL_Batch_installerFrame::Onart_requestClick(wxCommandEvent& event)
{}

void HDL_Batch_installerFrame::Onprint_partition_tableClick(wxCommandEvent& event)
{
}


void HDL_Batch_installerFrame::Onmass_header_injectionClick(wxCommandEvent& event)
{
    ArtPause _artpause(this); // ecriture HDL.EXE (modify_header) : pause du loader
    wxString system_cnf  = EXEC_PATH + "system.cnf";
    wxString icon_sys    = EXEC_PATH + "icon.sys";
    wxString icon_icn    = EXEC_PATH + "list.ico";
    wxString logo_raw    = EXEC_PATH + "logo.raw";
    wxString boot_kirx   = EXEC_PATH + "boot.kirx";
    wxString boot_elf    = EXEC_PATH + "boot.elf";
    wxString boot_kelf   = EXEC_PATH + "boot.kelf";
    wxString partition, inject_header_cmd;
    int partcount = 0;

    if(!wxFileExists(boot_kelf))
    {
        COLOR(0c)
        wxMessageBox( _("Warning\n\tCan't find boot.kelf\n\nAborting APA Injection"), error_caption,wxICON_WARNING);
        cerr << "no boot.kelf to inject\n";
        COLOR(07)
        return;
    }

    long KELF_size;
    if (wxFileExists(boot_kelf))  /// IF boot.kelf exists check if it's size is ok
    {
        KELF_size = GetFileSize("boot.kelf");
        if (KELF_size >= 2031616)///Size limitation taken from HDL-Dump readme [https://github.com/israpps/hdl-dump#modify_header]
        {
            COLOR(0c)
            wxMessageBox(_("file boot.kelf is too big!\n\nit's size can't be larger than 2,031,616 bytes"), error_caption, wxICON_ERROR);
            cerr << "boot.kelf is too big!\n\tExpected file to be smaller than 2031616 bytes\n\tprovided file is[" << KELF_size << "] bytes in length\n";
            COLOR(07)
            return;
        }
    }
    if( wxFileExists(system_cnf) )
    {
        wxRemoveFile(system_cnf);
    }
    if( wxFileExists(icon_sys)   )
    {
        wxRemoveFile(icon_sys);
    }
    if( wxFileExists(logo_raw)   )
    {
        wxRemoveFile(logo_raw);
    }
    if( wxFileExists(boot_kirx)  )
    {
        wxRemoveFile(boot_kirx);
    }
    if( wxFileExists(boot_elf)   )
    {
        wxRemoveFile(boot_elf);
    }
    if( wxFileExists(icon_icn)   )
    {
        wxRemoveFile(icon_icn);
    }

    wxBeginBusyCursor();
    wxString command = "HDL.EXE toc " + selected_hdd->GetString( selected_hdd->GetSelection() );
    wxString line,partname;
    wxString HDD = selected_hdd->GetString( selected_hdd->GetSelection() );
    wxArrayString output,partitions;
    COLOR(08) cout << "> Obtaining partition table...\n";
    COLOR(07)
    wxExecute(command,output,wxEXEC_SYNC);
    for (size_t x=0; x<output.GetCount(); x++)//parse partition table looking for HDL Partitions (AKA: games)
    {
        line = output.Item(x);
        if ( line.find("0x1337") != NOT_FOUND)
        {
            if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) )
            {
                cout << "found game\n";
            }
            partname = line.substr( line.find_first_of("B") +2);   ///get partition name, it should be two chars after the 'B', it needs testing
            /*           while (partname.EndsWith(' '))///strip whitespaces
                       {
                           partname.RemoveLast(1);
                       }*/ // not needed now since I made a PR to hdl-dump that removes the whitespace filling from partition listing
            partitions.Add(partname);
            partcount++;
        }
        else
        {
            if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) )
            {
                std::cout << "skipping [" << line << "]\n";
            }
        }
    }
    wxAppProgressIndicator *toolbar_progress = new wxAppProgressIndicator(this, partcount);
    wxProgressDialog* DLG = new wxProgressDialog(_("Injecting OPL Launcher to..."), wxEmptyString, partcount, this);
    COLOR(08) cout <<"> writing headers...\n";
    COLOR(07)
    //DLG->ShowModal();
    for (size_t x=0; x<partitions.GetCount(); x++)//traverse the list of partitions to inject
    {
        partition = partitions.Item(x);
        DLG->Update(x, partition);
        toolbar_progress->SetValue(x);
        cout << "\t[" <<partition <<"]\n";
        inject_header_cmd = "HDL.EXE modify_header " + HDD + " \"" + partition + "\"";
        if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) )
        {
            COLOR(08)
            cout << inject_header_cmd << endl;
        }
        COLOR(0d)
        wxExecute(inject_header_cmd, wxEXEC_SYNC);
        COLOR(07)
    }
    wxEndBusyCursor();
    delete toolbar_progress;
    delete DLG;
    wxMessageBox( _("Header Injection finished"),"",wxICON_INFORMATION );
}

void HDL_Batch_installerFrame::OnmodifyClick(wxCommandEvent& event)
{
}

void HDL_Batch_installerFrame::onItemRightClick(wxListEvent& event)
{
    //wxContextMenuEvent();
    //wxContextMenuEvent()

    PopupMenu(&about_2_install_menu);
}

void HDL_Batch_installerFrame::OnGoToFileLocationRequest(wxString victim)
{
    wxExecute( wxString::Format("explorer.exe \"/select,%s\"", victim) );
}

void HDL_Batch_installerFrame::OnItemListShowRequest(wxCommandEvent& event)
{
    long itemIndex = -1;
    if ( (itemIndex = game_list__->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
        OnGoToFileLocationRequest( game_list__->GetItemText(itemIndex));
}

void HDL_Batch_installerFrame::OnTakeOutFromTheListRequest(wxListCtrl* ListCtrl, long itemIndex)
{
    ListCtrl->DeleteItem(itemIndex);
}

void HDL_Batch_installerFrame::RemoveISOfromList(wxCommandEvent& event)
{
    long itemIndex = -1;
    if ( (itemIndex = game_list__->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
        OnTakeOutFromTheListRequest(game_list__, itemIndex);
}

void HDL_Batch_installerFrame::OnExtractInstalledGameRequest(wxCommandEvent& event)
{
    ArtPause _artpause(this); // HDL.EXE dump (acces disque brut) : pause du loader
    long itemIndex = -1;
    int ripcount = 0, currrip=0;
    while ((itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
        ripcount++;
    itemIndex = -1;//reset index counter for the real iteration

    wxString game_title, game_title2,extraction_path, full_extraction_path;
    wxString hdd = selected_hdd->GetString(selected_hdd->GetSelection());
    wxString command;

    dump_folder = new wxDirDialog(this, _("Choose path to extract selected games"), wxEmptyString, wxDD_DEFAULT_STYLE, wxDefaultPosition, wxDefaultSize, _T("wxDirDialog"));

    if (dump_folder->ShowModal() == wxID_OK)
    {
        wxAppProgressIndicator *toolbar_progress = new wxAppProgressIndicator(this, ripcount);
        wxProgressDialog* DLG = new wxProgressDialog(_("extracting game..."), "", ripcount, this);
        extraction_path = dump_folder->GetPath();
        cout << std::string(extraction_path.mb_str()) <<"\n";
        while ((itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
        {
            full_extraction_path.clear();
            game_title.clear();
            game_title = Installed_game_list->GetItemText(itemIndex);// Got the selected item index
            if (game_title[0]==' ') game_title = game_title.Mid(1);
            DLG->Update(currrip++, game_title);
            toolbar_progress->SetValue(currrip);
            cout <<"\nExtracting game ["<< game_title <<"]\n";
            game_title2 = game_title;
            COLOR(08) cout << "> Filtering illegal characters...\n";
            ///Filter windows illegal characters for filename
            game_title2.Replace(':', '_',true);
            game_title2.Replace('/', '_',true);
            game_title2.Replace('\\','_',true);
            game_title2.Replace('<', '_',true);
            game_title2.Replace('>', '_',true);
            game_title2.Replace('*', '_',true);
            game_title2.Replace('"', '_',true);
            game_title2.Replace('|', '_',true);
            game_title2.Replace('?', '_',true);
            // wxString::Replace(
            //-----------------------------------
            full_extraction_path = extraction_path + "\\" + game_title2 + ".iso";
            cout <<"  to: ["<< full_extraction_path <<"]\n";
            command = "HDL.EXE dump \"" + std::string(game_title.mb_str()) + "@" + std::string(hdd.mb_str()) + "\" \"" + std::string(full_extraction_path.mb_str()) + "\"";
            if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) )
            {
                COLOR(08)cout <<command<<"\n";
                COLOR(07)
            }
            COLOR(0d)
            wxExecute(command,wxEXEC_SYNC);
            //crude_SystemCapture(command);
            COLOR(07)
            if (wxFileExists(full_extraction_path))
            {
                char HBUF[6];
                std::ifstream extracted_game(full_extraction_path.ToStdString());
                if(extracted_game.is_open())
                {
                    extracted_game.read(HBUF, sizeof(HBUF));
                    if (!memcmp(HBUF, "ZISO", sizeof("ZISO")))
                    {
                        wxString newpath = full_extraction_path;
                        newpath.RemoveLast(3);
                        newpath += "zso";
                        wxRenameFile(full_extraction_path, newpath);
                    }
                    extracted_game.close();
                }
            }
        }
        delete toolbar_progress;
        delete DLG;
    }

}

void HDL_Batch_installerFrame::OnInstalledGameRenameRequest(wxCommandEvent& event)
{
    ArtPause _artpause(this); // ecriture HDL.EXE (modify) : pause du loader
    long itemIndex = -1;
    wxString title,title_backup;
    wxString game_title, game_title2,extraction_path, full_extraction_path;
    wxString hdd = selected_hdd->GetString(selected_hdd->GetSelection());
    while ((itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
    {
        title = Installed_game_list->GetItemText(itemIndex);
        title_backup = title;
        rename_game = new wxTextEntryDialog(this, title_backup, "Rename game2",title_backup );
        if (rename_game->ShowModal() == wxID_OK)
        {
            title = rename_game->GetValue();
            wxArrayString stdout_,stderr_;
            wxString command = "HDL.EXE modify " + hdd + " \"" + title_backup + "\" \"" + title + "\"" ;
            COLOR(08) cout << command <<endl;
            COLOR(07)
            long ret = wxExecute(command, stdout_, stderr_);
            if (ret == 0)
            {
                for (size_t x=0; x < stdout_.GetCount(); x++)
                {
                    cout << stdout_.Item(x) <<endl;
                }
            }
            else
            {
                for (size_t x=0; x < stderr_.GetCount(); x++)
                {
                    cout << stderr_.Item(x) <<endl;
                }
            }
        }
        delete rename_game;
    }
    List_refresh_request();
}

void HDL_Batch_installerFrame::OnInstalledGameAssetsDownloadRequest(wxCommandEvent& event)
{
    long itemIndex = -1;
    wxArrayString ELF;
    wxString CURRENT_ELF;
    while ((itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
    {
        CURRENT_ELF = Installed_game_list->GetItemText(itemIndex,1);
        if (CURRENT_ELF.EndsWith(' '))
        {
            CURRENT_ELF.RemoveLast(1);
        }
        ELF.Add(CURRENT_ELF);// populate array with selected ELF's
    }
    ArtMan artman_dlg(this,ELF);///Create instance of download manager outside loop, the dialog will iterate the list on it's own
    artman_dlg.ShowModal();
    // Envoi direct des assets telecharges vers la partition +OPL du HDD (si l'option a ete cochee)
    if (artman_dlg.pushToHDD && artman_dlg.pushToHDD->GetValue())
        TransferDownloadsToOPL();
}

void HDL_Batch_installerFrame::TransferDownloadsToOPL(void)
{
    ArtPause _artpause(this); // acces PFSSHELL exclusif (copie vers +OPL) : pause du loader
#if PFSSHELL_ALLOWED
    if (HDD_TOKEN.empty())
    {
        wxMessageBox(_("No HDD is selected, cannot transfer assets to the HDD."), error_caption, wxICON_ERROR);
        return;
    }
    const char* MNT = "hdd0:+OPL"; // partition de donnees OPL sur le HDD
    struct AssetSet { const char* sub; const char* filter; };
    const AssetSet SETS[] = {
        { "ART", "*.*"   },
        { "CHT", "*.cht" },
        { "CFG", "*.cfg" },
        { "VMC", "*.bin" },
    };
    wxBeginBusyCursor();
    PFSSHELL.CloseDevice(); // au cas ou un device serait deja ouvert
    if (PFSSHELL.SelectDevice(HDD_TOKEN) != 0)
    {
        wxEndBusyCursor();
        wxMessageBox(_("Could not open the selected HDD (see console log for details)."), error_caption, wxICON_ERROR);
        return;
    }
    // Verifie la presence de la partition +OPL via un montage-test
    int probe = PFSSHELL.Mount(MNT);
    PFSSHELL.UMount();
    if (probe < 0)
    {
        PFSSHELL.CloseDevice();
        wxEndBusyCursor();
        wxMessageBox(_("The +OPL partition was not found on this HDD.\n\nThe downloaded assets were kept in the local 'Downloads' folder."),
                     error_caption, wxICON_ERROR);
        return;
    }
    int copied = 0, failed = 0;
    for (const AssetSet& s : SETS)
    {
        wxString localdir = EXEC_PATH + "Downloads\\" + s.sub;
        if (!wxDirExists(localdir)) continue;
        PFSSHELL.pfs_mkdir(MNT, "/", s.sub); // cree le sous-dossier (erreur ignoree s'il existe deja)
        wxArrayString files;
        wxDir::GetAllFiles(localdir, &files, s.filter, wxDIR_FILES);
        for (size_t i = 0; i < files.GetCount(); i++)
        {
            wxString full = files.Item(i);
            wxString name = full.substr(full.find_last_of("\\/") + 1);
            wxString dest = wxString("/") + s.sub + "/" + name;
            std::cout << "PUSH -> " << MNT << ":pfs:" << dest << "\n";
            if (PFSSHELL.copyto(MNT, dest.mb_str(), full.mb_str()) == 0)
            {
                copied++;
                wxRemoveFile(full); // on retire la copie locale apres succes (comme le transfert Dokan)
            }
            else failed++;
        }
    }
    PFSSHELL.CloseDevice();
    wxEndBusyCursor();
    wxMessageBox(wxString::Format(_("Transfer to +OPL finished.\n\nCopied: %d\nFailed: %d"), copied, failed),
                 wxMessageBoxCaptionStr, (failed > 0) ? wxICON_WARNING : wxICON_INFORMATION);
    m_artCache.clear(); ClearArtDiskCache();                                  // l'art a change -> invalide cache memoire + disque
    m_artCacheHdd.clear();                                                    // force la reconstruction au prochain Update_hdd_data
    if (selected_hdd->GetSelection() != wxNOT_FOUND) List_refresh_request();  // rafraichit la liste (rechargera depuis Downloads/ART)
#else
    wxMessageBox(_("Direct HDD transfer is not available in this build."), error_caption, wxICON_WARNING);
#endif
}

void HDL_Batch_installerFrame::OnLogTimer(wxTimerEvent& event)
{
    if (event.GetId() == ID_ART_TIMER) return; // ce handler est bind sans id : ignorer le timer des vignettes
    if (!LogPanel) return;
    if (!wxFileExists("logs/console.log")) return;
    wxFile f;
    if (!f.Open("logs/console.log", wxFile::read)) return;
    wxFileOffset len = f.Length();
    if (len < m_logOffset) m_logOffset = 0;      // fichier tronque / recree
    if (len <= m_logOffset) { f.Close(); return; }
    size_t toRead = (size_t)(len - m_logOffset);
    wxCharBuffer buf(toRead);
    f.Seek(m_logOffset);
    ssize_t got = f.Read(buf.data(), toRead);
    f.Close();
    if (got <= 0) return;
    m_logOffset += got;
    wxString chunk = wxString::FromUTF8(buf.data(), (size_t)got);
    if (chunk.empty()) chunk = wxString(buf.data(), wxConvLibc, (size_t)got); // repli si non-UTF8
    LogPanel->AppendText(chunk);
}

// --- Helpers medias -------------------------------------------------------
static wxString RegionFromSerial(const wxString& serial)
{
    wxString s = serial.Upper();
    if (s.StartsWith("SLUS") || s.StartsWith("SCUS") || s.StartsWith("PBPX")) return "USA (NTSC-U/C)";
    if (s.StartsWith("SLES") || s.StartsWith("SCES") || s.StartsWith("SLED") || s.StartsWith("SCED")) return "Europe (PAL)";
    if (s.StartsWith("SLPS") || s.StartsWith("SLPM") || s.StartsWith("SCPS") || s.StartsWith("SCAJ") || s.StartsWith("SLAJ")) return "Japan (NTSC-J)";
    if (s.StartsWith("SLKA") || s.StartsWith("SCKA")) return "Korea";
    return _("Unknown");
}
static wxString ArtLabelFromPath(const wxString& path)
{
    wxString f = path.AfterLast('\\').AfterLast('/').Upper();
    if (f.Contains("_COV2"))   return _("Cover (back)");
    if (f.Contains("_COV"))    return _("Cover");
    if (f.Contains("_BG"))     return _("Background");
    if (f.Contains("_LAB"))    return _("Disc / Label");
    if (f.Contains("_ICO"))    return _("Icon");
    if (f.Contains("_LGO"))    return _("Logo");
    if (f.Contains("_SCR_00")) return _("Screenshot 1");
    if (f.Contains("_SCR_01")) return _("Screenshot 2");
    if (f.Contains("_SCR"))    return _("Screenshot");
    return f;
}

// --- Chargement des vignettes OPL dans un THREAD de fond ---
// La liste s'affiche instantanement (icones CD/DVD). Un thread lit les _ICO.png du
// HDD pendant que l'UI reste 100% fluide ; le thread principal ne fait que decoder
// la petite icone et la poser sur sa ligne. Barre de progression + bouton "Stop".
void HDL_Batch_installerFrame::StartArtPrefetch()
{
#if PFSSHELL_ALLOWED
    StopArtPrefetch();
    if (HDD_TOKEN.empty() || m_installedGames.empty()) return;
    m_artW = 24; m_artH = 24;
    wxImageList* il = Installed_game_list->GetImageList(wxIMAGE_LIST_SMALL);
    if (il) il->GetSize(0, m_artW, m_artH);
    const wxString cacheDir = EXEC_PATH + "Downloads\\_artcache\\"; // vignettes 24x24 persistantes
    const wxString artDir   = EXEC_PATH + "Downloads\\ART\\";       // assets telecharges localement
    if (!wxDirExists(EXEC_PATH + "Downloads")) wxMkdir(EXEC_PATH + "Downloads");
    if (!wxDirExists(cacheDir)) wxMkdir(cacheDir);
    if (!wxDirExists(EXEC_PATH + "Downloads\\_artbg")) wxMkdir(EXEC_PATH + "Downloads\\_artbg");

    // 1) Resolution LOCALE d'abord (cache disque + assets telecharges) : AUCUN acces HDD.
    std::deque<wxString> q;         // a charger (source resolue par le worker)
    std::vector<wxString> needHdd;  // jeux sans source locale -> peut-etre sur le HDD
    for (size_t k = 0; k < m_installedGames.size(); k++)
    {
        wxString elf = m_installedGames[k].elf; elf.Trim();
        if (elf.IsEmpty() || m_artCache.count(elf)) continue;
        if (wxFileExists(cacheDir + elf + ".png") || wxFileExists(artDir + elf + "_ICO.png"))
            q.push_back(elf);        // dispo en local (cache ou asset telecharge)
        else
            needHdd.push_back(elf);  // a verifier sur le HDD
    }
    // 2) Ne lister +OPL/ART (acces HDD) QUE s'il reste des jeux sans source locale.
    //    -> au redemarrage avec un cache complet, zero acces au HDD.
    m_artNames.Clear();
    if (!needHdd.empty())
    {
        PFSSHELL.CloseDevice();
        if (PFSSHELL.SelectDevice(HDD_TOKEN) == 0)
        {
            std::vector<iox_dirent_t> artdir;
            if (PFSSHELL.ls("hdd0:+OPL", "/ART", &artdir) == 0)
                for (size_t d = 0; d < artdir.size(); d++) m_artNames.Add(wxString::FromUTF8(artdir[d].name));
            PFSSHELL.CloseDevice();
        }
        for (size_t k = 0; k < needHdd.size(); k++)
            if (m_artNames.Index(needHdd[k] + "_ICO.png", false) != wxNOT_FOUND) q.push_back(needHdd[k]);
    }
    if (q.empty()) return; // tout est deja en cache / rien a charger
    { std::lock_guard<std::mutex> lk(m_artQMutex); m_artQueue = q; }
    m_artTotal = (int)q.size();
    m_artDoneCount = 0;
    // Barre de progression (style barre de capacite)
    if (m_artBar) m_artBar->SetValue(0);
    if (m_artProgLabel) m_artProgLabel->SetLabel(wxString::Format(_("Loading artwork: %d / %d"), 0, m_artTotal));
    if (m_artProgPanel) { m_artProgPanel->Show(); Layout(); }
    ReprioritizeVisible();                       // priorite aux lignes visibles
    m_artStop = false;
    m_artLoaderActive = true;
    int ep = ++m_artEpoch;
    m_artThread = std::thread(&HDL_Batch_installerFrame::ArtWorkerRun, this, ep);
    m_artLastTop = -1;
    if (m_artTimer) m_artTimer->Start(150, wxTIMER_CONTINUOUS); // re-priorisation pendant le scroll (aucune I/O)
#endif
}

// Stoppe+joint le thread, libere le device, vide la file et masque la barre.
// Appele avant toute operation PFS/HDL du thread principal (via ArtPause) : apres
// join, le worker ne touche plus PFSSHELL et le device est referme.
void HDL_Batch_installerFrame::StopArtPrefetch()
{
#if PFSSHELL_ALLOWED
    ++m_artEpoch;                                 // invalide les callbacks encore en vol
    m_artStop = true;
    if (m_artThread.joinable()) m_artThread.join();
    m_artStop = false;
    m_artLoaderActive = false;
    if (m_artTimer && m_artTimer->IsRunning()) m_artTimer->Stop();
    { std::lock_guard<std::mutex> lk(m_artQMutex); m_artQueue.clear(); }
    if (m_artProgPanel && m_artProgPanel->IsShown()) { m_artProgPanel->Hide(); Layout(); }
#endif
}

// Corps du thread de fond : lit les _ICO.png du HDD vers des fichiers temporaires et
// notifie le thread principal. N'appelle AUCUNE fonction GUI (uniquement PFS + I/O).
void HDL_Batch_installerFrame::ArtWorkerRun(int epoch)
{
#if PFSSHELL_ALLOWED
    const wxString cacheDir = EXEC_PATH + "Downloads\\_artcache\\";
    const wxString artDir   = EXEC_PATH + "Downloads\\ART\\";
    const wxString bg       = EXEC_PATH + "Downloads\\_artbg\\";
    bool devOpen = false; // le device n'est ouvert QUE si une lecture HDD est reellement necessaire
    for (;;)
    {
        if (m_artStop) break;
        wxString elf;
        { std::lock_guard<std::mutex> lk(m_artQMutex);
          if (m_artQueue.empty()) break;
          elf = m_artQueue.front(); m_artQueue.pop_front(); }
        wxString path; int source = ART_FROM_HDD;
        wxString cf = cacheDir + elf + ".png";       // cache disque (deja 24x24)
        wxString la = artDir + elf + "_ICO.png";      // asset telecharge localement
        if (wxFileExists(cf))      { path = cf; source = ART_FROM_CACHE; }
        else if (wxFileExists(la)) { path = la; source = ART_FROM_LOCAL; }
        else                                          // lecture depuis le HDD
        {
            if (!devOpen)
            {
                if (PFSSHELL.SelectDevice(HDD_TOKEN) != 0)
                { wxString e = elf; CallAfter([this, epoch, e]{ OnArtResult(epoch, e, wxString(), ART_FROM_HDD); }); continue; }
                devOpen = true;
            }
            wxString dst = bg + elf + "_ICO.png";
            wxString src = "/ART/" + elf + "_ICO.png";
            if (PFSSHELL.recoverfile("hdd0:+OPL", src.mb_str(), dst.mb_str()) == 0) { path = dst; source = ART_FROM_HDD; }
        }
        wxString e = elf, p = path; int s = source;
        CallAfter([this, epoch, e, p, s]{ OnArtResult(epoch, e, p, s); }); // decode + pose sur le thread UI
    }
    if (devOpen) PFSSHELL.CloseDevice();
    CallAfter([this, epoch]{ ArtLoaderFinished(epoch); });
#endif
}

// Thread UI : decode la petite icone (sans popup libpng), la pose sur la ligne, avance la
// barre, et ecrit la vignette 24x24 dans le cache disque (sauf si elle en vient deja).
void HDL_Batch_installerFrame::OnArtResult(int epoch, const wxString& elf, const wxString& path, int source)
{
    if (epoch != m_artEpoch) { if (source == ART_FROM_HDD && !path.IsEmpty()) wxRemoveFile(path); return; } // perime
    if (!path.IsEmpty())
    {
        wxImage img;
        { wxLogNull noLog; img.LoadFile(path, wxBITMAP_TYPE_ANY); } // avale les avertissements libpng (iCCP)
        if (img.IsOk())
        {
            if (img.GetWidth() != m_artW || img.GetHeight() != m_artH)
                img.Rescale(m_artW, m_artH, wxIMAGE_QUALITY_HIGH); // le cache est deja a la bonne taille
            wxBitmap bmp(img);
            m_artCache[elf] = bmp;
            UpdateRowIcon(elf, bmp);
            if (source != ART_FROM_CACHE) // persiste la vignette pour les prochains lancements
            {
                wxString cf = EXEC_PATH + "Downloads\\_artcache\\" + elf + ".png";
                wxLogNull noLog; img.SaveFile(cf, wxBITMAP_TYPE_PNG);
            }
        }
        if (source == ART_FROM_HDD) wxRemoveFile(path); // fichier temporaire
    }
    int done = ++m_artDoneCount;
    if (m_artBar) m_artBar->SetValue(m_artTotal > 0 ? (int)(100LL * done / m_artTotal) : 100);
    if (m_artProgLabel) m_artProgLabel->SetLabel(wxString::Format(_("Loading artwork: %d / %d"), done, m_artTotal));
}

// Vide le cache disque des vignettes (appele quand l'art du HDD a change).
void HDL_Batch_installerFrame::ClearArtDiskCache()
{
    wxString dir = EXEC_PATH + "Downloads\\_artcache";
    if (!wxDirExists(dir)) return;
    wxArrayString files;
    wxDir::GetAllFiles(dir, &files, "*.png", wxDIR_FILES);
    for (size_t i = 0; i < files.GetCount(); i++) wxRemoveFile(files[i]);
}

// Thread UI : fin du chargement -> joint le thread et masque la barre.
void HDL_Batch_installerFrame::ArtLoaderFinished(int epoch)
{
    if (epoch != m_artEpoch) return;              // fin d'un chargement deja remplace/annule
    if (m_artThread.joinable()) m_artThread.join();
    m_artLoaderActive = false;
    if (m_artTimer && m_artTimer->IsRunning()) m_artTimer->Stop();
    if (m_artProgPanel && m_artProgPanel->IsShown()) { m_artProgPanel->Hide(); Layout(); }
}

// Bouton "Stop" : interrompt le chargement des vignettes.
void HDL_Batch_installerFrame::OnArtCancel(wxCommandEvent& event)
{
    StopArtPrefetch();
}

// Remonte les lignes actuellement visibles en tete de file (priorite au regard).
void HDL_Batch_installerFrame::ReprioritizeVisible()
{
    long top = Installed_game_list->GetTopItem();
    long per = Installed_game_list->GetCountPerPage();
    long cnt = Installed_game_list->GetItemCount();
    std::vector<wxString> vis;
    for (long i = top; i <= top + per + 1 && i < cnt; i++)
    {
        wxString e = Installed_game_list->GetItemText(i, 1); e.Trim();
        if (!e.IsEmpty()) vis.push_back(e);
    }
    if (vis.empty()) return;
    std::lock_guard<std::mutex> lk(m_artQMutex);
    for (int k = (int)vis.size() - 1; k >= 0; k--) // insere en tete, ordre d'affichage preserve
    {
        std::deque<wxString>::iterator it = std::find(m_artQueue.begin(), m_artQueue.end(), vis[k]);
        if (it != m_artQueue.end() && it != m_artQueue.begin())
        {
            m_artQueue.erase(it);
            m_artQueue.push_front(vis[k]);
        }
    }
}

// Timer (pendant le chargement) : si la vue a defile, re-prioriser les lignes visibles.
void HDL_Batch_installerFrame::OnArtTimer(wxTimerEvent& event)
{
    if (!m_artLoaderActive) return;
    long top = Installed_game_list->GetTopItem();
    if (top == m_artLastTop) return;
    m_artLastTop = top;
    ReprioritizeVisible();
}

// Pose la vignette sur la ligne du jeu correspondant (si elle est visible avec le
// filtre courant). Ne touche ni au scroll ni a la selection.
void HDL_Batch_installerFrame::UpdateRowIcon(const wxString& elf, const wxBitmap& bmp)
{
    if (!bmp.IsOk()) return;
    wxImageList* il = Installed_game_list->GetImageList(wxIMAGE_LIST_SMALL);
    if (!il) return;
    int count = Installed_game_list->GetItemCount();
    for (int i = 0; i < count; i++)
    {
        wxString e = Installed_game_list->GetItemText(i, 1); e.Trim();
        if (e == elf) { Installed_game_list->SetItemImage(i, il->Add(bmp)); break; }
    }
}

// Double-clic sur un jeu installe : extrait tous les arts trouves sur le HDD
// (+OPL/ART) et les affiche dans une galerie. Si rien : message "aucun media".
void HDL_Batch_installerFrame::OnInstalledGameActivated(wxListEvent& event)
{
    ArtPause _artpause(this); // acces PFSSHELL exclusif (extraction galerie) : pause du loader (reprise a la fermeture)
#if PFSSHELL_ALLOWED
    long item = event.GetIndex();
    if (item < 0) return;
    wxString elf    = Installed_game_list->GetItemText(item, 1); elf.Trim();
    wxString gname  = Installed_game_list->GetItemText(item, 0);
    wxString gsize  = Installed_game_list->GetItemText(item, 2);
    wxString gmedia = Installed_game_list->GetItemText(item, 3);
    if (HDD_TOKEN.empty()) { wxMessageBox(_("No HDD selected."), error_caption, wxICON_ERROR); return; }

    static const char* SUFFIXES[] = {
        "_COV.jpg", "_COV2.jpg", "_BG.jpg", "_LAB.jpg",
        "_ICO.png", "_LGO.png", "_SCR_00.jpg", "_SCR_01.jpg"
    };
    wxString cachedir = EXEC_PATH + "Downloads\\_mediacache";
    if (!wxDirExists(EXEC_PATH + "Downloads")) wxMkdir(EXEC_PATH + "Downloads");
    if (!wxDirExists(cachedir)) wxMkdir(cachedir);

    wxArrayString found;
    wxBeginBusyCursor();
    PFSSHELL.CloseDevice();
    if (PFSSHELL.SelectDevice(HDD_TOKEN) == 0)
    {
        const char* MNT = "hdd0:+OPL";
        for (size_t s = 0; s < sizeof(SUFFIXES) / sizeof(SUFFIXES[0]); s++)
        {
            wxString dst = cachedir + "\\" + elf + SUFFIXES[s];
            if (wxFileExists(dst)) wxRemoveFile(dst);
            wxString src = "/ART/" + elf + SUFFIXES[s];
            if (PFSSHELL.recoverfile(MNT, src.mb_str(), dst.mb_str()) == 0
                && wxFileExists(dst) && wxFileName::GetSize(dst) > 0)
                found.Add(dst);
            else if (wxFileExists(dst))
                wxRemoveFile(dst);
        }
        PFSSHELL.CloseDevice();
    }
    wxEndBusyCursor();

    wxString info = wxString::Format(_("Name:    %s\nSerial:  %s\nRegion:  %s\nSize:    %s MB\nMedia:   %s"),
                                     gname, elf, RegionFromSerial(elf), gsize, gmedia);
    if (found.IsEmpty())
        info += "\n\n" + wxString(_("No media found on the HDD for this game."));
    ShowMediaGallery(gname, info, found);
#endif
}

// Fenetre galerie : en-tete d'infos du jeu + images (chacune avec son libelle),
// dans une zone defilante.
void HDL_Batch_installerFrame::ShowMediaGallery(const wxString& title, const wxString& infoText, const wxArrayString& images)
{
    wxDialog dlg(this, wxID_ANY, title, wxDefaultPosition, wxSize(760, 600),
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    root->Add(new wxStaticText(&dlg, wxID_ANY, infoText), 0, wxALL, 8);
    root->Add(new wxStaticLine(&dlg, wxID_ANY), 0, wxEXPAND | wxLEFT | wxRIGHT, 4);
    wxScrolledWindow* scroll = new wxScrolledWindow(&dlg, wxID_ANY);
    scroll->SetScrollRate(10, 10);
    wxWrapSizer* ws = new wxWrapSizer(wxHORIZONTAL);
    for (size_t i = 0; i < images.GetCount(); i++)
    {
        wxImage img;
        bool okimg;
        { wxLogNull noLog; okimg = img.LoadFile(images[i], wxBITMAP_TYPE_ANY); } // pas de popup libpng (iCCP)
        if (!okimg || !img.IsOk()) continue;
        int w = img.GetWidth(), h = img.GetHeight();
        if (w > 0 && h > 0)
        {
            double sc = 240.0 / w;
            if (h * sc > 320.0) sc = 320.0 / h;
            if (sc < 1.0) img.Rescale((int)(w * sc), (int)(h * sc), wxIMAGE_QUALITY_HIGH);
        }
        wxBoxSizer* cell = new wxBoxSizer(wxVERTICAL);
        cell->Add(new wxStaticBitmap(scroll, wxID_ANY, wxBitmap(img)), 0, wxALIGN_CENTER_HORIZONTAL);
        cell->Add(new wxStaticText(scroll, wxID_ANY, ArtLabelFromPath(images[i])), 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, 3);
        ws->Add(cell, 0, wxALL, 8);
    }
    scroll->SetSizer(ws);
    root->Add(scroll, 1, wxEXPAND | wxALL, 4);
    dlg.SetSizer(root);
    dlg.ShowModal();
}

void HDL_Batch_installerFrame::OnCopyAssetsToHDD(wxCommandEvent& event)
{
    // Meme action que la case "Copy to HDD" du gestionnaire de telechargements :
    // pousse les assets locaux (Downloads/) vers la partition +OPL du HDD.
    TransferDownloadsToOPL();
}

void HDL_Batch_installerFrame::OnRefreshListHotkey(wxCommandEvent& event)
{
    if (selected_hdd->GetSelection() == wxNOT_FOUND) return;
    if (selected_hdd->GetStringSelection().IsEmpty()) return;
    List_refresh_request();
}

void HDL_Batch_installerFrame::CleanIsoStage()
{
    wxString stage = EXEC_PATH + "_iso_stage";
    if (wxDirExists(stage))
    {
        COLOR(08) std::cout << "> cleaning extracted-ISO staging folder\n"; COLOR(07)
        wxFileName::Rmdir(stage, wxPATH_RMDIR_RECURSIVE);
    }
}

// Extrait une archive vers un sous-dossier de staging via Common\7z.exe et renvoie
// la liste des images de jeu trouvees dedans.
wxArrayString HDL_Batch_installerFrame::ExtractArchiveGames(const wxString& archive)
{
    wxArrayString result;
    wxString stage = EXEC_PATH + "_iso_stage";
    if (!wxDirExists(stage)) wxMkdir(stage);
    wxFileName af(archive);
    wxString sub = stage + "\\" + af.GetName();
    if (!wxDirExists(sub)) wxMkdir(sub);

    wxString cmd = wxString::Format("Common\\7z.exe x -o\"%s\" -y \"%s\"", sub, archive);
    std::cout << "> extracting archive: " << archive.ToStdString() << "\n";
    wxArrayString out, err;
    wxExecute(cmd, out, err, wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE); // sans fenetre, sortie -> console embarquee
    for (size_t i = 0; i < out.GetCount(); i++) std::cout << out.Item(i).ToStdString() << "\n";
    for (size_t i = 0; i < err.GetCount(); i++) std::cout << err.Item(i).ToStdString() << "\n";

    wxArrayString files;
    wxDir::GetAllFiles(sub, &files); // recursif
    for (size_t i = 0; i < files.GetCount(); i++)
    {
        wxString ext = files.Item(i).AfterLast('.').Lower();
        if (ext == "iso" || ext == "cue" || ext == "nrg" || ext == "gi" || ext == "iml" || ext == "zso")
            result.Add(files.Item(i));
    }
    return result;
}

void HDL_Batch_installerFrame::OnDropFilesEvent(wxDropFilesEvent& event)
{
    wxArrayString files;
    int n = event.GetNumberOfFiles();
    wxString* f = event.GetFiles();
    for (int i = 0; i < n && f; i++) files.Add(f[i]);
    if (!files.IsEmpty()) AddGamesToList(files);
}

// Ajoute des fichiers a la liste d'installation. Les images de jeu (ISO/CUE/...)
// sont validees (is_PS2) et ajoutees. Les archives (zip/rar/7z) sont ajoutees
// TELLES QUELLES : elles seront extraites au moment de l'installation, puis l'ISO
// extrait supprime apres une install reussie (voir OninstallClick).
// Telecharge les assets (art + cfg + cht) d'un jeu dans Downloads/ pour son serial.
// Appele APRES une installation reussie quand l'option auto-assets est active.
void HDL_Batch_installerFrame::DownloadAssetsForELF(const wxString& elfIn)
{
    wxString elf = elfIn; elf.Trim();
    if (elf.IsEmpty()) return;
    if (!wxDirExists("Downloads"))        wxMkdir("Downloads");
    if (!wxDirExists("Downloads\\ART"))   wxMkdir("Downloads\\ART");
    if (!wxDirExists("Downloads\\CFG"))   wxMkdir("Downloads\\CFG");
    if (!wxDirExists("Downloads\\CHT"))   wxMkdir("Downloads\\CHT");
    struct A { const char* suf; const char* urlsuf; };
    static const A arts[] = {
        {"_COV.jpg","_COV.jpg"}, {"_COV2.jpg","_COV2.jpg"}, {"_BG.jpg","_BG_00.jpg"},
        {"_ICO.png","_ICO.png"}, {"_LGO.png","_LGO.png"},   {"_LAB.jpg","_LAB.jpg"},
        {"_SCR_00.jpg","_SCR_00.jpg"}, {"_SCR_01.jpg","_SCR_01.jpg"}
    };
    wxArrayString o, e;
    for (size_t k = 0; k < sizeof(arts) / sizeof(arts[0]); k++)
    {
        wxString cmd = wxString::Format("common\\wget.exe -q %s%s%%2F%s%s -O \"Downloads\\ART\\%s%s\"",
                                        CFG_ARTURL, elf, elf, arts[k].urlsuf, elf, arts[k].suf);
        o.Clear(); e.Clear(); wxExecute(cmd, o, e, wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE);
    }
    wxExecute("common\\wget.exe -q https://raw.githubusercontent.com/israpps/PS2-OPL-CFG-Database/master/CFG_en/" + elf + ".cfg -O \"Downloads\\CFG\\" + elf + ".cfg\"", o, e, wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE);
    wxExecute("common\\wget.exe -q https://raw.githubusercontent.com/PS2-Widescreen/OPL-Widescreen-Cheats/main/CHT/" + elf + ".cht -O \"Downloads\\CHT\\" + elf + ".cht\"", o, e, wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE);
}

void HDL_Batch_installerFrame::AddGamesToList(const wxArrayString& paths)
{
    // Met le chargement des vignettes en pause pendant l'ajout : le probing des ISO
    // lance HDL.EXE et l'insertion touche la liste -> on evite tout conflit avec le
    // thread, puis le chargement reprend automatiquement. L'ajout reste donc dispo
    // meme pendant que les icones se chargent.
    ArtPause _artpause(this);
    wxBeginBusyCursor();
    int added = 0, discarded = 0;
    for (size_t i = 0; i < paths.GetCount(); i++)
    {
        wxString p = paths.Item(i);
        wxString ext = p.AfterLast('.').Lower();
        if (ext == "zip" || ext == "rar" || ext == "7z")
        { m_pending.Add(p); InsertPendingRow(p); added++; }
        else if (ext == "iso" || ext == "cue" || ext == "nrg" || ext == "gi" || ext == "iml" || ext == "zso")
        {
            int type = -1;
            if (is_PS2(p, &type)) { m_pending.Add(p); InsertPendingRow(p); added++; }
            else discarded++;
        }
    }
    wxEndBusyCursor();
    UpdateInstallButton();
    std::cout << "> add: " << added << " item(s) queued, " << discarded << " discarded\n";
    if (added == 0)
        wxMessageBox(_("No valid PS2 game or archive was found in the dropped file(s)."), _("Information:"), wxICON_INFORMATION);
}

// Ajoute une ligne "en attente" (fond jaune pale) en haut de la liste unifiee.
void HDL_Batch_installerFrame::InsertPendingRow(const wxString& path)
{
    wxString name = path.AfterLast('\\').AfterLast('/');
    long i = Installed_game_list->InsertItem(0, name);
    Installed_game_list->SetItemBackgroundColour(i, wxColour(255, 249, 196));
    Installed_game_list->SetItem(i, 3, _("pending"));
}

// Affiche/masque le bouton Install selon qu'il y a des jeux en attente.
void HDL_Batch_installerFrame::UpdateInstallButton()
{
    bool has = !m_pending.IsEmpty();
    install->Show(has);
    install->Enable(has && selected_hdd->GetSelection() != wxNOT_FOUND);
    wxWindow* p = install->GetParent();
    if (p) p->Layout();
}

void HDL_Batch_installerFrame::OnInstalled_game_listItemRClick1(wxListEvent& event)
{
    PopupMenu(&Browser_menu);
}

void HDL_Batch_installerFrame::OnButton3Click1(wxCommandEvent& event)
{
    wxMessageBox(_("Game related operations can be used by selecting games from the list and right clicking them..."),"",wxICON_INFORMATION);
}


void HDL_Batch_installerFrame::OnGameHashReques(wxCommandEvent& event)
{

}

long HDL_Batch_installerFrame::GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void HDL_Batch_installerFrame::OnGameInfoRequest(wxCommandEvent& event)
{
    wxString command, title, game_information;
    wxArrayString output;
    long itemIndex = wxNOT_FOUND;
    itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (itemIndex != wxNOT_FOUND)
    {
        title = Installed_game_list->GetItemText(itemIndex);
        command = wxString::Format("HDL.EXE info %s \"%s\"",
                                   selected_hdd->GetString(selected_hdd->GetSelection()), title );
        wxExecute(command, output, wxEXEC_SYNC);
        for (size_t x=0; x<output.GetCount(); x++)
        {
            game_information << output.Item(x) << "\n";
        }
        wxMessageBox(game_information, wxString::Format(_("Report for [%s]"), title), wxICON_INFORMATION);
    }
}

void HDL_Batch_installerFrame::OnMBRExtractRequestClick(wxCommandEvent& event)
{
    ArtPause _artpause(this); // HDL.EXE dump_mbr (acces disque) : pause du loader
    long KELF_size, retcode;
    if (!wxDirExists("Extracted_MBR"))
        wxMkdir("Extracted_MBR");

    wxString kelfpath = "RECOVERED_MBR.KELF", command;
    if (wxFileExists(kelfpath)) kelfpath = wxString::Format("%dRECOVERED_MBR.KELF",rand());
    command = wxString::Format("HDL.EXE dump_mbr %s \"%s\"",
                               selected_hdd->GetString(selected_hdd->GetSelection()),kelfpath);
    cout << "saving MBR.KELF to ["<<kelfpath<<"]\n";
    retcode = wxExecute(command, wxEXEC_SYNC);
    if (retcode != 0)
    {
        wxMessageBox(_("MBR extraction failed!!\n\nsee console log to see error code..."),error_caption,wxICON_ERROR);
        return;
    }
    KELF_size = GetFileSize(std::string(kelfpath));
    if (KELF_size == 0)
        wxMessageBox(_("KELF size is 0 bytes!\nIt seems that this HDD had no MBR program installed...\n\n(or something went wrong)"),error_caption,wxICON_WARNING);
    else
        wxCopyFile(kelfpath, wxString::Format("Extracted_MBR\\%s", kelfpath));
    wxRemoveFile(kelfpath);
    cout << "Kelf size was: "<<KELF_size<<"\n";
}

void HDL_Batch_installerFrame::On_GameNameDatabaseDownloadRequest(wxCommandEvent& event)
{
    int database_mode;
    if (wxFileExists("gamename.DB"))
    {
        if (wxMessageBox(_("A local database already exists.\nDownloading the database from the repository will delete the old one...\n\n Continue?"),warning_caption,wxICON_WARNING|wxYES_NO) == wxNO)
            return;
    }

    if (wxFileExists("gamename.DB"))
        wxRemoveFile("gamename.DB");

    if (wxExecute("common\\wget.exe -q --spider --no-cache https://raw.githubusercontent.com/israpps/HDL-Batch-installer/main/Database/gamename.csv",wxEXEC_SYNC)==0)
        wxExecute("common\\wget.exe -q --show-progress https://raw.githubusercontent.com/israpps/HDL-Batch-installer/main/Database/gamename.csv -O gamename.DB",wxEXEC_SYNC);

    wxFileName fname( wxTheApp->argv[0] );
    wxString ini_filename = fname.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR) + "Common\\config.INI";
    wxFileConfig * main_config = new wxFileConfig( wxEmptyString, wxEmptyString, ini_filename);
    main_config->Read("Installation/DataBase_Mode", &database_mode, DB_INTERNAL);
    if (database_mode == DB_INTERNAL)
        wxMessageBox( _("The program is currently configured to use it's internal database.\nChange it on the settings menu in order to use the database you just downloaded"), warning_caption, wxICON_WARNING);
}
bool HDL_Batch_installerFrame::Load_custom_icon(wxString ELF)
{
    if (!wxFileExists(EXEC_PATH+"Common\\HDD-OSD-Icon-Database-main\\README.md"))
    {
        ask_2_download_icons();
    }
    std::cout << "Searching custom icon: ";
    const wxString icon_icn = EXEC_PATH + "list.ico";
    if (wxFileExists(icon_icn)) wxRemoveFile(icon_icn);
    wxString icon_from_database = "\\" + ELF + ".ico";

    if (wxFileExists(ICONS_FOLDER+icon_from_database))
    {
        COLOR(0a)
        std::cout << "Loading icon ["<<icon_from_database<<"]\n";
        COLOR(07)
        wxCopyFile(ICONS_FOLDER+icon_from_database,icon_icn);
    }
    else
    {
        COLOR(0c) std::cerr << wxString::Format("> WARNING: Couldn't find icon specified by database\nELF:%s\nFILE:%s\n",ELF, icon_from_database);
        COLOR(07) return false;
    }
    return true;
}

void HDL_Batch_installerFrame::OnICONS_DOWNLOAD(void)
{
    wxArrayString unused_buffer;
    long ping = -1;
    ping = wxExecute("ping google.com", unused_buffer, wxEXEC_SYNC);
    if (ping != 0)
    {
        wxMessageBox(_("No internet connection!"), "", wxICON_WARNING);
        std::cerr << "PING Result ["<<ping<<"]\n";
        return;
    }
    const wxString DOWNLOAD_COMMAND    = "Common\\wget -q --show-progress https://github.com/CosmicScale/HDD-OSD-Icon-Database/archive/refs/heads/main.zip -O Common\\ICONDB.ZIP",
                   EXTRACTION_COMMAND = "Common\\7z.exe x -oCommon -bso0 -y -pPDPA Common\\ICONDB.ZIP";
    COLOR(0e)
    std::cout << "Downloading package...\n";
    COLOR(0d)
    long retcode = wxExecute(DOWNLOAD_COMMAND,wxEXEC_SYNC);
    if (retcode != 0)
    {
        COLOR(0c)
        wxMessageBox(_("The download of the icons package failed!"), "", wxICON_ERROR);
        COLOR(07)
        return;
    }
    COLOR(07)
    retcode = wxExecute(EXTRACTION_COMMAND, wxEXEC_SYNC);
    if (retcode != 0)
    {
        wxMessageBox(_("The package extraction failed!"), "", wxICON_ERROR);
    }
    return;
}

void HDL_Batch_installerFrame::OnIconsPackageRequest(wxCommandEvent& event)
{
    OnICONS_DOWNLOAD();
}

void HDL_Batch_installerFrame::ask_2_download_icons(void)
{
    if(wxMessageBox(_("Can't find custom icons package\n\nDownload now?"),"",wxICON_QUESTION|wxYES_NO)==wxYES)
        OnICONS_DOWNLOAD();
}

bool Dokan_is_installed(void)
{
    return wxGetEnv(DOKAN_ENV, NULL) || wxGetEnv(DOKAN_ENV2, NULL);
}

void HDL_Batch_installerFrame::OnButton4Click(wxCommandEvent& event)
{
#if PFSSHELL_ALLOWED
    PFSSHELL.CloseDevice(); //PFSShell with device attached will make HDL Dump write features crash
#endif
    if (!wxFileExists(HDLBINST_APPDATA+"\\dokan_and_fuse.ini"))
    {
        wxMessageBox(_("This is the first time you use PFSFUSE\n\nPLEASE, Keep in mind that, if you mount a partition you must unmount it when you finish using this program, otherwise, you could corrupt the mounted partition or the whole HDD"),
                     warning_caption,
                     wxICON_INFORMATION);
    }
    if (!Dokan_is_installed())
    {
        if(
            wxMessageBox(
                wxString::Format(_("Can't find the enviroment variables \"%s\" or \"%s\" used to locate the Dokan Library\n\n It seems like Dokan was unproperly installed (or it isn't installed)\n\nGo to Dokan download website?"),DOKAN_ENV, DOKAN_ENV2)
                ,error_caption,
                wxICON_ERROR|wxYES_NO
            )==wxYES) wxLaunchDefaultBrowser("https://github.com/dokan-dev/dokany/releases");

        return;
    }
    wxBeginBusyCursor();
    wxString HDD = selected_hdd->GetString( selected_hdd->GetSelection() );
    wxString command = "HDL.EXE toc " + HDD;
    wxString winapi_device_token = wxString::Format("\\\\.\\PHYSICALDRIVE%s", HDD.substr(3,HDD.find_first_of(':')-3));
    wxString line,partname;
    wxArrayString output,pfs_partitions;
    if (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL)
        std::cout <<"winapi_device_token["<<winapi_device_token<<"]\n";
    COLOR(08) cout << "Obtaining partition table...\n";
    COLOR(07)
    wxExecute(command,output,wxEXEC_SYNC);
    for (size_t x=0; x<output.GetCount(); x++)//parse partition table looking for HDL Partitions (AKA: games)
    {
        line = output.Item(x);
        if ( line.find("0x0100") != NOT_FOUND) //If PFS partition identifier detected, process data and load partition to array
        {
            partname = line.substr( line.find_first_of("B") +2,32);   ///get partition name, it should be two chars after the 'B', it needs testing
            /*while (partname.EndsWith(' '))///strip whitespaces
            {
                partname.RemoveLast(1);
            }*/ //not needed since I made a PR to hdl-dump to remove whitespace filling
            cout << "found Partition\n\t["<<partname<<"]\n";
            ///                      from B+2                  up to       1st whitespace after B+2
            pfs_partitions.Add(partname);
        }
        else
        {
            if (CFG::DEBUG_LEVEL > 5 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL) )
            {
                std::cout << "skipping this [" << line << "]\n";
            }
        }
    }
    wxEndBusyCursor();

    if (pfs_partitions.GetCount() < 1)
    {
        wxMessageBox(_("You need at least one PFS partition to use this menu"), _("Could not find PFS Partitions"), wxICON_WARNING);
        return;
    }
#if PFSSHELL_ALLOWED
    PFSSHELL.CloseDevice(); //PFSShell with device attached will make HDL Dump write features crash
#endif
    DokanMan* DOKAN_WIZARD = new DokanMan(this, pfs_partitions, winapi_device_token);
    DOKAN_WIZARD->ShowModal();
    delete DOKAN_WIZARD;
}

void HDL_Batch_installerFrame::OnIP4NBDUpdate(wxCommandEvent& event)
{/*
    wxRegEx regxIPAddr("^(([0-9]{1}|[0-9]{2}|[0-1][0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]{1}|[0-9]{2}|[0-1][0-9]{2}|2[0-4][0-9]|25[0-5])$");
    wxString TMP = NBD4IP->GetValue();
    if (regxIPAddr.Matches(TMP))
    {
        NBDConnect->Enable();
    }
    else
    {
        NBDConnect->Disable();
    }*/
}

void HDL_Batch_installerFrame::OnNBDConnectClick(wxCommandEvent& event)
{/*
    if (!wxFileExists(HDLBINST_APPDATA+"\\NBD.ini"))
    {
        wxMessageBox(
            _("This is the first time you try to use NBD Server\nKeep in mind that you must disconnect the server when you finish using the Device\nOtherwise... There might be risk of HDD Damage")
            ,""
            ,wxICON_WARNING
        );
    }
    wxBeginBusyCursor();
    wxString command;
    command.Printf("Common\\WNBD\\wnbd-client.exe map PS2HDD %s", NBD4IP->GetValue());
    wxExecute(command,wxEXEC_SYNC);
    wxEndBusyCursor();*/
}

void HDL_Batch_installerFrame::OnNBDDisconnectClick(wxCommandEvent& event)
{
    wxString command = "Common\\WNBD\\wnbd-client.exe unmap PS2HDD";
    wxExecute(command);
}

void HDL_Batch_installerFrame::OnButton4Click1(wxCommandEvent& event)
{
    NDBMan* NBDManager= new NDBMan(this);
    NBDManager->ShowModal();
    delete NBDManager;
}

void HDL_Batch_installerFrame::OnManualInjectionRequest(wxCommandEvent& event)
{
    ArtPause _artpause(this); // ecriture HDL.EXE (modify_header) : pause du loader
    wxString HDD = selected_hdd->GetString( selected_hdd->GetSelection() );
    wxString system_cnf  = EXEC_PATH + "system.cnf";
    wxString icon_sys    = EXEC_PATH + "icon.sys";
    wxString icon_icn    = EXEC_PATH + "list.ico";
    wxString logo_raw    = EXEC_PATH + "logo.raw";
    wxString boot_kirx   = EXEC_PATH + "boot.kirx";
    wxString boot_elf    = EXEC_PATH + "boot.elf";
    wxString boot_kelf   = EXEC_PATH + "boot.kelf";

    if( wxFileExists(system_cnf) )
    {
        wxRemoveFile(system_cnf);
    }
    if( wxFileExists(icon_sys)   )
    {
        wxRemoveFile(icon_sys);
    }
    if( wxFileExists(logo_raw)   )
    {
        wxRemoveFile(logo_raw);
    }
    if( wxFileExists(boot_kirx)  )
    {
        wxRemoveFile(boot_kirx);
    }
    if( wxFileExists(boot_elf)   )
    {
        wxRemoveFile(boot_elf);
    }
    if( wxFileExists(icon_icn)   )
    {
        wxRemoveFile(icon_icn);
    }


    long itemIndex = -1, retcode;
    int prevcount = 0, x = 0;
    wxString title, command;
    while ((itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
        prevcount++;
    itemIndex = -1;//reset counter for the real iteration
    wxProgressDialog* DLG = new wxProgressDialog(_("Injecting OPL Launcher to..."), wxEmptyString, prevcount, this);
    while ((itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
    {
        title = Installed_game_list->GetItemText(itemIndex);
        DLG->Update(x, title);
        command.clear();
        command.Printf("HDL.EXE modify_header %s \"%s\"", HDD, title);
        std::cout << "injecting ["<<title<<"]\n";
        COLOR(0d);
        retcode = wxExecute(command,wxEXEC_SYNC);
        COLOR(07);
        if (CFG::DEBUG_LEVEL > 6 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL))
            std::cout << "injection result: "<<retcode<<"\n";
        x++;
    }
    delete DLG;
}

void HDL_Batch_installerFrame::OnLoadCustomIcon2InstalledGameRequest(wxCommandEvent& event)
{
    ArtPause _artpause(this); // ecriture HDL.EXE / acces PFSSHELL : pause du loader
    wxString HDD = selected_hdd->GetString( selected_hdd->GetSelection() );
    wxString system_cnf  = EXEC_PATH + "system.cnf";
    wxString icon_sys    = EXEC_PATH + "icon.sys";
    wxString icon_icn    = EXEC_PATH + "list.ico";
    wxString logo_raw    = EXEC_PATH + "logo.raw";
    wxString boot_kirx   = EXEC_PATH + "boot.kirx";
    wxString boot_elf    = EXEC_PATH + "boot.elf";
    wxString boot_kelf   = EXEC_PATH + "boot.kelf";

    if( wxFileExists(system_cnf) )
    {
        wxRemoveFile(system_cnf);
    }
    if( wxFileExists(icon_sys)   )
    {
        wxRemoveFile(icon_sys);
    }
    if( wxFileExists(logo_raw)   )
    {
        wxRemoveFile(logo_raw);
    }
    if( wxFileExists(boot_kirx)  )
    {
        wxRemoveFile(boot_kirx);
    }
    if( wxFileExists(boot_elf)   )
    {
        wxRemoveFile(boot_elf);
    }
    if( wxFileExists(icon_icn)   )
    {
        wxRemoveFile(icon_icn);
    }


    long itemIndex = -1, retcode;
    int prevcount = 0, x = 0;
    wxString title, ELF, command;

    while ((itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
        prevcount++;
    itemIndex = -1;//reset counter for the real iteration
    wxProgressDialog* DLG = new wxProgressDialog(_("Injecting custom icon to..."), wxEmptyString, prevcount, this);
    wxAppProgressIndicator *toolbar_progress = new wxAppProgressIndicator(this, prevcount);

    while ((itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
    {
        title = Installed_game_list->GetItemText(itemIndex,0);
        DLG->Update(x, title);
        toolbar_progress->SetValue(x);
        ELF = Installed_game_list->GetItemText(itemIndex,1);
        if( wxFileExists(icon_icn)   )
        {
            wxRemoveFile(icon_icn);
        }
        if (Load_custom_icon(ELF))
        {
            command.clear();
            command.Printf("HDL.EXE modify_header %s \"%s\"", HDD, title);
            std::cout << "injecting ["<<title<<"] @"<<ELF<<"\n";
            COLOR(0d);
            retcode = wxExecute(command,wxEXEC_SYNC);
            COLOR(07);
            if (CFG::DEBUG_LEVEL > 6 || (CTOR_FLAGS & FORCE_HIGH_DEBUG_LEVEL))
                std::cout << "injection result: "<<retcode<<"\n";
        }
        x++;
    }
    delete DLG;
    delete toolbar_progress;
    if( wxFileExists(icon_icn)   )
    {
        std::cout << "> Cleaning stray icon\n";
        wxRemoveFile(icon_icn);
    }
    wxMessageBox(_("Injection Finished!"),"",wxICON_INFORMATION);
}

void HDL_Batch_installerFrame::OnMD5HashRequest(wxCommandEvent& event)
{
    std::string HASH, FILEPATH;
    long itemIndex = -1;
    if((itemIndex = game_list__->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
    {
        FILEPATH = std::string(game_list__->GetItemText(itemIndex).mb_str());
    }
    wxBeginBusyCursor();
    std::cout << "Collecting MD5 hash for ["<<FILEPATH<<"]\n";
    HASH = MD5digest_file(FILEPATH);
    std::cout << "\tCollected hash: ["<<HASH<<"]\n";
    wxEndBusyCursor();
    wxMessageBox(HASH, _("Collected hash:"),wxICON_INFORMATION);
}

void HDL_Batch_installerFrame::OnCOPYHDDSelected(wxCommandEvent& event)
{
    if (selected_hdd->GetCount() < 2)
    {
        wxMessageBox(_("You need at least two PlayStation 2 formatted HDDs to use this operation!"),"", wxICON_WARNING);
        return;
    }
    wxArrayString HDDS;
    for (size_t x=0; x < selected_hdd->GetCount(); x++)
        HDDS.Add(selected_hdd->GetString(x));

    CopyHDD* HDDCOPYER = new CopyHDD(this, HDDS, -1, wxEmptyString);

    HDDCOPYER->ShowModal();
    delete HDDCOPYER;
}

void HDL_Batch_installerFrame::OnSelectiveGameMigration(wxCommandEvent& event)
{
    if (selected_hdd->GetCount() < 2)
    {
        wxMessageBox(_("You need at least two PlayStation 2 formatted HDDs to use this operation!"),"", wxICON_WARNING);
        return;
    }

    wxString FLAGS;
    long itemIndex = -1;

    while ((itemIndex = Installed_game_list->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_DONTCARE)) != wxNOT_FOUND)
    {
        FLAGS.append( (Installed_game_list->GetItemState(itemIndex, wxLIST_STATE_SELECTED)) ? "y":"n");
    }
    std::cout <<"The command string is: ["<<FLAGS<<"]\n";
    wxArrayString HDDS;
    for (size_t x=0; x < selected_hdd->GetCount(); x++)
        HDDS.Add(selected_hdd->GetString(x));

    CopyHDD* HDDCOPYER = new CopyHDD(this, HDDS, selected_hdd->GetSelection(), FLAGS);

    HDDCOPYER->ShowModal();
    delete HDDCOPYER;
}

void HDL_Batch_installerFrame::OnGameDeletionRequest(wxCommandEvent& event)
{
    wxMessageBox(_("To delete your games, please use the 'HDD Manager' button found on the 'HDD Management' tab\n"
                   "Inside there, locate the desired game partition, right click it and delete it."),
                 wxEmptyString,wxCENTRE|wxICON_INFORMATION);
}

void HDL_Batch_installerFrame::OnFrameResize(wxSizeEvent& event)
{
    /* // TODO: FIND A WAY TO MAKE THIS WORK WITHOUT FUCKING THE *sizer objects auto resizing
    int listwidth = Installed_game_list->GetSize().x;
    int listwidth2 = game_list__->GetSize().x;

    Installed_game_list->SetColumnWidth(0, listwidth);
    game_list__->SetColumnWidth(0, listwidth2); */
}

int wxCALLBACK hdlbinst_listctrl_compare(wxIntPtr item1, wxIntPtr item2, wxIntPtr WXUNUSED(sortData))
{
    if(item1<item2) return -1;
    if(item1>item2) return 1;
    return 0; // if both items are equal...
}

void HDL_Batch_installerFrame::OnHDDFormatMenuRequest(wxCommandEvent& event)
{
#if PFSSHELL_ALLOWED
    HDDFomatMan *MAN = new HDDFomatMan(this);
    MAN->ShowModal();
    delete MAN;
#else
    PFSSHELL_DISABLED_WARNING();
#endif
}

void HDL_Batch_installerFrame::OnRedump_searchSelected(wxCommandEvent& event)
{
}

void HDL_Batch_installerFrame::OnCalculateMD5Selected(wxCommandEvent& event)
{
    long itemIndex = -1;
    if ( (itemIndex = game_list__->GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND)
        {
            wxString HASH = MD5digest_file(game_list__->GetItemText(itemIndex).ToStdString());
            MD5Report *REPORT = new MD5Report(this, game_list__->GetItemText(itemIndex), HASH);
            REPORT->ShowModal();
            delete REPORT;
        }
}

wxString HDL_Batch_installerFrame::cat_errdump()
{
    const char* ERRDUMP = "errdump.txt";
    wxString DUMP = _("Cannot open error report, read the terminal logs to find the issue");
    if (wxFileExists(ERRDUMP)) {
        wxFile A(ERRDUMP, wxFile::read);
        if (A.IsOpened()) {
            A.ReadAll(&DUMP);
            DUMP.Replace('\n', ' ',true);
            A.Close();
            wxRemoveFile(ERRDUMP);
        }
    }
    return DUMP;
}

void HDL_Batch_installerFrame::OnPFSBrowserCallClick(wxCommandEvent& event)
{
#if PFSSHELL_ALLOWED
    PFSShellBrowser* BROWSER = new PFSShellBrowser(this);
    BROWSER->ShowModal();
    delete BROWSER;
#endif
}
