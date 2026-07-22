/***************************************************************
 * Name:      HDL_Batch_installerMain.h
 * Purpose:   Defines Application Frame
 * Author:    matias israelson (aka: El_isra) (tatochin-m@hotmail.com)
 * Created:   2021-03-31
 * Copyright: matias israelson (aka: El_isra) (https://github.com/israpps)
 * License:
 **************************************************************/

#ifndef HDL_BATCH_INSTALLERMAIN_H
#define HDL_BATCH_INSTALLERMAIN_H
#include "version.h"
//#include "classes/alternative_program_caller.h"
//(*Headers(HDL_Batch_installerFrame)
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/frame.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/progdlg.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
//*)
#include <wx/timer.h>
#include <wx/bitmap.h>
#include <wx/gauge.h>
#include <wx/stattext.h>
#include <map>
#include <vector>
#include <deque>
#include <thread>
#include <atomic>
#include <mutex>
//#include "md5/md5.h"
//#include <wx/xml/xml.h>
#include <wx/time.h>
#include <wx/regex.h>
#include <wx/config.h>
#include <wx/arrstr.h> //used to recover all the ISO names from dialog
#include <wx/utils.h>
#include <wx/fileconf.h>
#include <fstream>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/fileconf.h>
#include <wx/process.h>
#include <wx/filefn.h>
#include <stdio.h>
#include <wx/app.h>
#include <wx/file.h>
#include <wx/msgdlg.h>
#include <wx/choicdlg.h>
#include <wx/dirdlg.h>
#include <wx/textdlg.h>
#include <wx/url.h>
#include <wx/window.h>
#include <wx/dir.h>
#include <stdlib.h>
#include "include/macro-vault.h"
#include "flags.h"
#include "include/locale_table.h"

class HddSpaceBar;     // barre d'espace HDD personnalisee (definie dans le .cpp)
class MiniProgressBar; // barre de progression d'install (meme style, definie dans le .cpp)
struct InstalledGameRow { wxString name; wxString elf; wxString size; int media; };

class HDL_Batch_installerFrame: public wxFrame
{
public:
    HDL_Batch_installerFrame(wxWindow* parent, wxLocale& m_locale, long Custom_Styles = 0, long ctor_flags_ctor = 0, wxWindowID id = -1);
    virtual ~HDL_Batch_installerFrame();
private:
    //(*Handlers(HDL_Batch_installerFrame)
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnRadioButton1Select(wxCommandEvent& event);
    void Onb_networkSelect(wxCommandEvent& event);
    void OnNotebook1PageChanged(wxNotebookEvent& event);
    void OnTextCtrl1Text(wxCommandEvent& event);
    void OnListBox1Select(wxCommandEvent& event);
    void OnCheckListBox1Toggled(wxCommandEvent& event);
    void OnListCtrl1BeginDrag(wxListEvent& event);
    void OnSEARCH_ISOClick(wxCommandEvent& event);
    void OnTextCtrl1Text1(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void Onselected_hddSelect(wxCommandEvent& event);
    void OnButton1Click(wxCommandEvent& event);
    void OnListCtrl1BeginDrag1(wxListEvent& event);
    void OnParse_hdl_tocClick(wxCommandEvent& event);
    void OninstallClick(wxCommandEvent& event);
    void OnCheckBox1Click(wxCommandEvent& event);
    void OnSettings(wxCommandEvent& event);
    void OnPaint(wxPaintEvent& event);
    void OnCheckBox1Click1(wxCommandEvent& event);
    void OnMBR_EVENTClick(wxCommandEvent& event);
    void OnButton2Click(wxCommandEvent& event);
    void Onclear_iso_listClick(wxCommandEvent& event);
    void OnUpdateRequest(wxCommandEvent& event);
    void OnIssueReport(wxCommandEvent& event);
    void OnLanguageChangeRequest(wxCommandEvent& event);
    void Selected_games(wxListEvent& event);
    void UnSelected_games(wxListEvent& event);
    void Onpull_isoClick(wxCommandEvent& event);
    void Onmodify_requestClick(wxCommandEvent& event);
    void Ondma_choiceSelect(wxCommandEvent& event);
    void Update_hdd_data(wxCommandEvent& event);
    void OnButton2Click1(wxCommandEvent& event);
    void List_refresh_request(wxListEvent& event);
    void On_MiniOPL_Update_request(wxCommandEvent& event);
    void OnButton2Click2(wxCommandEvent& event);
    void OnButton2Click3(wxCommandEvent& event);
    void OnHDL_DumpUpdateRequest(wxCommandEvent& event);
    void Onart_requestClick(wxCommandEvent& event);
    void Onprint_partition_tableClick(wxCommandEvent& event);
    void OnButton3Click(wxCommandEvent& event);
    void Onmass_header_injectionClick(wxCommandEvent& event);
    void OnmodifyClick(wxCommandEvent& event);
    void onItemRightClick(wxListEvent& event);
    void OnGoToFileLocationRequest(wxCommandEvent& event);
    void OnItemListShowRequest(wxCommandEvent& event);
    void RemoveISOfromList(wxCommandEvent& event);
    void OnExtractInstalledGameRequest(wxCommandEvent& event);
    void OnInstalled_game_listItemRClick(wxListEvent& event);
    void OnInstalledGameRenameRequest(wxCommandEvent& event);
    void OnInstalledGameAssetsDownloadRequest(wxCommandEvent& event);
    void OnInstalled_game_listItemRClick1(wxListEvent& event);
    void OnButton3Click1(wxCommandEvent& event);
    void OnGameHashReques(wxCommandEvent& event);
    void OnGameInfoRequest(wxCommandEvent& event);
    void OnMBRExtractRequestClick(wxCommandEvent& event);
    void On_GameNameDatabaseDownloadRequest(wxCommandEvent& event);
    void OnIconsPackageRequest(wxCommandEvent& event);
    void OnButton4Click(wxCommandEvent& event);
    void OnIP4NBDUpdate(wxCommandEvent& event);
    void OnNBDConnectClick(wxCommandEvent& event);
    void OnNBDDisconnectClick(wxCommandEvent& event);
    void OnButton4Click1(wxCommandEvent& event);
    void OnManualInjectionRequest(wxCommandEvent& event);
    void OnLoadCustomIcon2InstalledGameRequest(wxCommandEvent& event);
    void OnMD5HashRequest(wxCommandEvent& event);
    void OnCOPYHDDSelected(wxCommandEvent& event);
    void OnSelectiveGameMigration(wxCommandEvent& event);
    void OnGameDeletionRequest(wxCommandEvent& event);
    void OnFrameResize(wxSizeEvent& event);
    void OnHDDFormatMenuRequest(wxCommandEvent& event);
    void OnRedump_searchSelected(wxCommandEvent& event);
    void OnCalculateMD5Selected(wxCommandEvent& event);
    void OnPFSBrowserCallClick(wxCommandEvent& event);
    //*)
    void OnGoToFileLocationRequest(wxString victim);
    void OnTakeOutFromTheListRequest(wxListCtrl* ListCtrl, long itemIndex);

    //(*Identifiers(HDL_Batch_installerFrame)
    static const long ID_BUTTON2;
    static const long ID_selected_hdd;
    static const long ID_TEXTCTRL1;
    static const long ID_GAUGE1;
    static const long ID_LISTCTRL1;
    static const long ID_BUTTON1;
    static const long ID_STATICLINE3;
    static const long ID_BUTTON7;
    static const long ID_STATICLINE1;
    static const long ID_CHOICE1;
    static const long ID_STATICLINE2;
    static const long ID_BUTTON4;
    static const long ID_STATICLINE4;
    static const long ID_CHECKBOX2;
    static const long ID_PANEL1;
    static const long ID_BUTTON3;
    static const long ID_TEXTCTRL2;
    static const long ID_BUTTON8;
    static const long ID_LISTCTRL2;
    static const long ID_PANEL2;
    static const long ID_BUTTON10;
    static const long ID_BUTTON13;
    static const long ID_BUTTON6;
    static const long ID_BUTTON9;
    static const long ID_BUTTON5;
    static const long ID_BUTTON11;
    static const long ID_BUTTON12;
    static const long ID_PANEL3;
    static const long ID_NOTEBOOK1;
    static const long ID_PANEL5;
    static const long idMenuQuit;
    static const long ID_MENUITEM13;
    static const long ID_MENUITEM15;
    static const long SETTINGS;
    static const long idMenuAbout;
    static const long UPDT;
    static const long ISSUE;
    static const long ID_MENUITEM1;
    static const long ID_MENUITEM2;
    static const long ID_MENUITEM9;
    static const long ID_MENUITEM10;
    static const long ID_PROGRESSDIALOG1;
    static const long ID_MENUITEM3;
    static const long ID_MENUITEM18;
    static const long ID_MENUITEM4;
    static const long ID_MENUITEM5;
    static const long ID_MENUITEM7;
    static const long ID_MENUITEM14;
    static const long ID_MENUITEM6;
    static const long ID_MENUITEM11;
    static const long ID_MENUITEM12;
    static const long ID_MENUITEM8;
    static const long DELETE_GAME_ID;
    static const long ID_PROGRESSDIALOG2;
    //*)

    //(*Declarations(HDL_Batch_installerFrame)
    wxButton* Button1;
    wxButton* Button3;
    wxButton* FUSE;
    wxButton* HDDManagerButton;
    wxButton* MBRExtractRequest;
    wxButton* MBR_EVENT;
    wxButton* PFSBrowserCall;
    wxButton* Parse_hdl_toc;
    wxButton* SEARCH_ISO;
    wxButton* clear_iso_list;
    wxButton* install;
    wxButton* mass_header_injection;
    wxButton* modify_header_event;
    wxCheckBox* use_database;
    wxChoice* dma_choice;
    wxChoice* selected_hdd;
    wxFileDialog* MBR_search;
    wxGauge* Gauge1;
    wxListCtrl* Installed_game_list;
    wxListCtrl* game_list__;
    wxMenu Browser_menu;
    wxMenu about_2_install_menu;
    wxMenu* Menu4;
    wxMenuItem* COPYHDD;
    wxMenuItem* DeleteGameMenuItem;
    wxMenuItem* MenuHDDFormat;
    wxMenuItem* MenuItem10;
    wxMenuItem* MenuItem11;
    wxMenuItem* MenuItem12;
    wxMenuItem* MenuItem13;
    wxMenuItem* MenuItem14;
    wxMenuItem* MenuItem15;
    wxMenuItem* MenuItem16;
    wxMenuItem* MenuItem17;
    wxMenuItem* MenuItem18;
    wxMenuItem* MenuItem4;
    wxMenuItem* MenuItem5;
    wxMenuItem* MenuItem6;
    wxMenuItem* MenuItem7;
    wxMenuItem* MenuItem8;
    wxMenuItem* MenuItem9;
    wxMenuItem* Redump_search;
    wxNotebook* Notebook1;
    wxPanel* Panel1;
    wxPanel* Panel2;
    wxPanel* Panel3;
    wxPanel* Panel5;
    wxProgressDialog* HashProgress;
    wxProgressDialog* install_progress;
    wxStaticLine* StaticLine1;
    wxStaticLine* StaticLine2;
    wxStaticLine* StaticLine3;
    wxStaticLine* StaticLine4;
    wxTextCtrl* GameCountDisplay;
    wxTextCtrl* hdd_used_space;
    //*)
    void Update_hdd_data(void);
    wxFileDialog* ISO_SEARCH_DIALOG;
    wxDirDialog* dump_folder;
    wxTextEntryDialog* rename_game;
    void List_refresh_request();
    bool is_PS2(wxString path, int* disct);
    long GetFileSize(std::string filename);
    void Enable_HDD_dependant_objects(bool WTF_should_I_do);
    bool Load_custom_icon(wxString ELF);
    void OnICONS_DOWNLOAD(void);
    void ask_2_download_icons(void);
    wxString cat_errdump();
    void TransferDownloadsToOPL(void); // envoi direct des assets telecharges vers la partition +OPL du HDD
    // --- Console embarquee en bas de la fenetre ---
    wxTextCtrl* LogPanel = nullptr;
    wxTimer* LogTimer = nullptr;
    long m_logOffset = 0;
    void OnLogTimer(wxTimerEvent& event);
    // --- Medias du jeu (art depuis +OPL/ART sur le HDD) ---
    void OnInstalledGameActivated(wxListEvent& event); // double-clic -> galerie des medias
    // --- Chargement des vignettes OPL dans un THREAD de fond (UI fluide) ---
    // Le thread lit les _ICO.png du HDD ; le thread UI ne fait que decoder la petite
    // icone + la poser sur la ligne. Barre de progression + bouton d'interruption.
    void StartArtPrefetch();                           // (re)construit la file et lance le thread de fond
    void StopArtPrefetch();                            // stoppe+joint le thread, libere le device, masque la barre
    void ArtWorkerRun(int epoch);                      // corps du thread de fond (I/O PFS uniquement, pas de GUI)
    void OnArtResult(int epoch, const wxString& elf, const wxString& path, int source); // thread UI: decode + pose + progression + cache disque
    void ClearArtDiskCache();                          // vide le cache disque des vignettes (invalidation)
    void ArtLoaderFinished(int epoch);                 // thread UI: fin du chargement -> masque la barre, joint le thread
    void OnArtCancel(wxCommandEvent& event);           // bouton "Stop"
    void OnArtTimer(wxTimerEvent& event);              // poll du scroll -> priorite aux lignes visibles
    void ReprioritizeVisible();                        // remonte les lignes visibles en tete de file
    void UpdateRowIcon(const wxString& elf, const wxBitmap& bmp); // pose la vignette sur la ligne du jeu (si visible)
    wxTimer* m_artTimer = nullptr;                      // timer de re-priorisation (scroll) pendant le chargement
    wxArrayString m_artNames;                          // contenu de +OPL/ART liste une seule fois
    std::string m_artCacheHdd;                          // HDD pour lequel le cache memoire est valide (evite de tout recharger si HDD inchange)
    int m_artW = 24, m_artH = 24;                      // taille des vignettes (depuis l'image list)
    long m_artLastTop = -1;                            // derniere position de scroll (detection du defilement)
    std::thread m_artThread;                            // thread de fond
    std::mutex m_artQMutex;                             // protege m_artQueue
    std::deque<wxString> m_artQueue;                    // elfs a charger (ordre = priorite, front d'abord)
    std::atomic<bool> m_artStop{false};                // demande d'arret du thread
    std::atomic<int> m_artDoneCount{0};                // vignettes traitees (pour la barre)
    int m_artTotal = 0;                                // total a charger
    int m_artEpoch = 0;                                // generation courante (ignore les callbacks perimes)
    bool m_artLoaderActive = false;                    // un chargement est-il en cours ?
    wxPanel* m_artProgPanel = nullptr;                 // barre de progression (conteneur)
    MiniProgressBar* m_artBar = nullptr;               // meme style que la barre de capacite disque
    wxStaticText* m_artProgLabel = nullptr;
    wxButton* m_artCancelBtn = nullptr;
    // RAII : met le loader en pause le temps d'une operation PFS/HDL du thread principal
    struct ArtPause {
        HDL_Batch_installerFrame* f; bool was;
        ArtPause(HDL_Batch_installerFrame* f_) : f(f_) { was = f->m_artLoaderActive; if (was) f->StopArtPrefetch(); }
        ~ArtPause() { if (was) f->StartArtPrefetch(); }
    };
    void RebuildFromCache();                           // (re)affiche la liste depuis m_installedGames + filtre de recherche
    std::vector<InstalledGameRow> m_installedGames;    // jeux installes (donnees, pour le filtrage)
    wxTextCtrl* m_searchField = nullptr;               // champ de recherche
    void OnSearchText(wxCommandEvent& event);          // filtre la liste au fil de la frappe
    bool m_startupDetect = false;                      // detection HDD au demarrage (popup "aucun HDD" supprime)
    int m_sortCol = -1;                                // colonne de tri courante (-1 = aucune)
    bool m_sortAsc = true;                             // sens du tri
    void OnListColClick(wxListEvent& event);           // tri au clic sur l'en-tete de colonne
    void ShowMediaGallery(const wxString& title, const wxString& infoText, const wxArrayString& images);
    void OnCopyAssetsToHDD(wxCommandEvent& event);   // menu contextuel: pousser Downloads/ vers +OPL
    void OnRefreshListHotkey(wxCommandEvent& event); // F5 -> rafraichir la liste
    std::map<wxString, wxBitmap> m_artCache;         // cache des vignettes _LAB (par serial)
    HddSpaceBar* m_spaceBar = nullptr;               // barre d'espace HDD dessinee
    wxMenu* m_hddMenu = nullptr;                      // menu "HDD Management" (ex-onglet)
    wxArrayString m_pending;                          // jeux en attente d'installation (chemins ISO/archive)
    bool m_installing = false;                        // installation en cours (bouton Install fait office de Pause)
    bool m_pauseRequested = false;
    MiniProgressBar* m_rowGauge = nullptr;             // barre de progression posee sur la ligne du jeu en cours
    long RunInstallCaptured(const wxString& command);  // lance HDL.EXE cache, sortie -> console, % -> jauge
    void InsertPendingRow(const wxString& path);     // ajoute une ligne "en attente" (fond jaune) en haut de la liste
    void DownloadAssetsForELF(const wxString& elf);   // telecharge les assets d'un serial (apres install reussie)
    void UpdateInstallButton();                       // affiche/masque le bouton Install selon m_pending
    // --- Glisser-deposer / archives ---
    wxArrayString ExtractArchiveGames(const wxString& archive); // extrait une archive, renvoie les images de jeu trouvees
    void CleanIsoStage();                                       // supprime le dossier de staging des ISO extraits
public:
    void AddGamesToList(const wxArrayString& paths);            // ajoute ISO, ou extrait archives (drop / dialogue)
    void OnDropFilesEvent(wxDropFilesEvent& event);            // WM_DROPFILES -> AddGamesToList
private:
    DECLARE_EVENT_TABLE()
public:
    wxLocale& m_locale;
    long CTOR_FLAGS;
};
int wxCALLBACK hdlbinst_listctrl_compare(wxIntPtr item1, wxIntPtr item2, wxIntPtr WXUNUSED(sortData));




#endif // HDL_BATCH_INSTALLERMAIN_H
