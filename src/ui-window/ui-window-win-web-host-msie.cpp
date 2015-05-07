#include "ui-window-win-web-host.h"
#include "ie-json-polyfill.h"
#include <MsHTML.h>
#include <MsHtmHst.h>
#include <ExDisp.h>
#include <ExDispid.h>
#include <shlguid.h>
#include <Shlwapi.h>
#include <Shlobj.h>
#include <Shobjidl.h>
#include <iostream>
#include <iomanip>

#pragma region Links

// https://msdn.microsoft.com/en-us/library/ie/hh772404(v=vs.85).aspx
// https://msdn.microsoft.com/en-us/library/aa770041(v=vs.85).aspx

#pragma endregion

#pragma region Interface

class UiWindowWebHostMsie;

class IoUiContainer : public IOleContainer {
public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);
    // IParseDisplayName
    STDMETHODIMP ParseDisplayName(IBindCtx *pbc, LPOLESTR pszDisplayName, ULONG *pchEaten, IMoniker **ppmkOut);
    // IOleContainer
    STDMETHODIMP EnumObjects(DWORD grfFlags, IEnumUnknown **ppenum);
    STDMETHODIMP LockContainer(BOOL fLock);
};

class IoUiStorage : public IStorage {
public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);
    // IStorage
    STDMETHODIMP CreateStream(const WCHAR * pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStream ** ppstm);
    STDMETHODIMP OpenStream(const WCHAR * pwcsName, void * reserved1, DWORD grfMode, DWORD reserved2, IStream ** ppstm);
    STDMETHODIMP CreateStorage(const WCHAR * pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStorage ** ppstg);
    STDMETHODIMP OpenStorage(const WCHAR * pwcsName, IStorage * pstgPriority, DWORD grfMode, SNB snbExclude, DWORD reserved, IStorage ** ppstg);
    STDMETHODIMP CopyTo(DWORD ciidExclude, IID const * rgiidExclude, SNB snbExclude, IStorage * pstgDest);
    STDMETHODIMP MoveElementTo(const OLECHAR * pwcsName, IStorage * pstgDest, const OLECHAR* pwcsNewName, DWORD grfFlags);
    STDMETHODIMP Commit(DWORD grfCommitFlags);
    STDMETHODIMP Revert(void);
    STDMETHODIMP EnumElements(DWORD reserved1, void * reserved2, DWORD reserved3, IEnumSTATSTG ** ppenum);
    STDMETHODIMP DestroyElement(const OLECHAR * pwcsName);
    STDMETHODIMP RenameElement(const WCHAR * pwcsOldName, const WCHAR * pwcsNewName);
    STDMETHODIMP SetElementTimes(const WCHAR * pwcsName, FILETIME const * pctime, FILETIME const * patime, FILETIME const * pmtime);
    STDMETHODIMP SetClass(REFCLSID clsid);
    STDMETHODIMP SetStateBits(DWORD grfStateBits, DWORD grfMask);
    STDMETHODIMP Stat(STATSTG * pstatstg, DWORD grfStatFlag);
};

class IoUiFrame : public IOleInPlaceFrame {
public:
    IoUiFrame(UiWindowWebHostMsie* host);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);
    // IOleWindow
    STDMETHODIMP GetWindow(HWND FAR* lphwnd);
    STDMETHODIMP ContextSensitiveHelp(BOOL fEnterMode);
    // IOleInPlaceUIWindow
    STDMETHODIMP GetBorder(LPRECT lprectBorder);
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS pborderwidths);
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS pborderwidths);
    STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName);
    // IOleInPlaceFrame
    STDMETHODIMP InsertMenus(HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths);
    STDMETHODIMP SetMenu(HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject);
    STDMETHODIMP RemoveMenus(HMENU hmenuShared);
    STDMETHODIMP SetStatusText(LPCOLESTR pszStatusText);
    STDMETHODIMP EnableModeless(BOOL fEnable);
    STDMETHODIMP TranslateAccelerator(LPMSG lpmsg, WORD wID);
private:
    UiWindowWebHostMsie* _host;
};

class IoUiSite : public IOleClientSite, public IOleInPlaceSite, public IDocHostUIHandler, public IDocHostShowUI, public IOleCommandTarget {
public:
    IoUiSite(UiWindowWebHostMsie* host);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);
    // IOleClientSite
    STDMETHODIMP SaveObject();
    STDMETHODIMP GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker ** ppmk);
    STDMETHODIMP GetContainer(LPOLECONTAINER FAR* ppContainer);
    STDMETHODIMP ShowObject();
    STDMETHODIMP OnShowWindow(BOOL fShow);
    STDMETHODIMP RequestNewObjectLayout();
    // IOleWindow
    STDMETHODIMP GetWindow(HWND FAR* lphwnd);
    STDMETHODIMP ContextSensitiveHelp(BOOL fEnterMode);
    // IOleInPlaceSite methods
    STDMETHODIMP CanInPlaceActivate();
    STDMETHODIMP OnInPlaceActivate();
    STDMETHODIMP OnUIActivate();
    STDMETHODIMP GetWindowContext(LPOLEINPLACEFRAME FAR* lplpFrame, LPOLEINPLACEUIWINDOW FAR* lplpDoc, LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo);
    STDMETHODIMP Scroll(SIZE scrollExtent);
    STDMETHODIMP OnUIDeactivate(BOOL fUndoable);
    STDMETHODIMP OnInPlaceDeactivate();
    STDMETHODIMP DiscardUndoState();
    STDMETHODIMP DeactivateAndUndo();
    STDMETHODIMP OnPosRectChange(LPCRECT lprcPosRect);

    // IDocHostUIHandler
    STDMETHODIMP EnableModeless(BOOL fEnable);
    STDMETHODIMP FilterDataObject(IDataObject *pDO, IDataObject **ppDORet);
    STDMETHODIMP GetDropTarget(IDropTarget *pDropTarget, IDropTarget **ppDropTarget);
    STDMETHODIMP GetExternal(IDispatch **ppDispatch);
    STDMETHODIMP GetHostInfo(DOCHOSTUIINFO *pInfo);
    STDMETHODIMP GetOptionKeyPath(LPOLESTR *pchKey, DWORD dwReserved);
    STDMETHODIMP HideUI(void);
    STDMETHODIMP OnDocWindowActivate(BOOL fActive);
    STDMETHODIMP OnFrameWindowActivate(BOOL fActive);
    STDMETHODIMP ResizeBorder(LPCRECT prcBorder,IOleInPlaceUIWindow *pUIWindow, BOOL fFrameWindow);
    STDMETHODIMP ShowContextMenu(DWORD dwID, POINT *ppt,IUnknown *pcmdtReserved, IDispatch *pdispReserved);
    STDMETHODIMP ShowUI(DWORD dwID, IOleInPlaceActiveObject *pActiveObject, IOleCommandTarget *pCommandTarget, IOleInPlaceFrame *pFrame, IOleInPlaceUIWindow *pDoc);
    STDMETHODIMP TranslateAccelerator(LPMSG lpMsg, const GUID *pguidCmdGroup, DWORD nCmdID);
    STDMETHODIMP TranslateUrl(DWORD dwTranslate, OLECHAR *pchURLIn, OLECHAR **ppchURLOut);
    STDMETHODIMP UpdateUI(void);

    // IDocHostShowUI
    STDMETHODIMP ShowMessage(HWND hwnd, LPOLESTR lpstrText, LPOLESTR lpstrCaption, DWORD dwType, LPOLESTR lpstrHelpFile, DWORD dwHelpContext, LRESULT *plResult);
    STDMETHODIMP ShowHelp(HWND hwnd, LPOLESTR pszHelpFile, UINT uCommand, DWORD dwData, POINT ptMouse, IDispatch *pDispatchObjectHit);

    // IOleCommandTarget
    STDMETHODIMP Exec(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut);
    STDMETHODIMP QueryStatus(const GUID *pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT *pCmdText);
private:
    UiWindowWebHostMsie* _host;
};

class IoUiBrowserEventHandler : public DWebBrowserEvents2 {
public:
    IoUiBrowserEventHandler(UiWindowWebHostMsie* host);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    // IDispatch
    STDMETHODIMP GetTypeInfoCount(UINT* pctinfo);
    STDMETHODIMP GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo);
    STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid);
    STDMETHODIMP Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr);

    ~IoUiBrowserEventHandler();

private:
    UiWindowWebHostMsie* _host;
    void NavigateComplete();
};

class IoUiExternal : public IDispatch {
public:
    IoUiExternal(UiWindowWebHostMsie* host);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    // IDispatch
    STDMETHODIMP GetTypeInfoCount(UINT* pctinfo);
    STDMETHODIMP GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo);
    STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid);
    STDMETHODIMP Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr);

    HRESULT PostMsg(WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo);
private:
    UiWindowWebHostMsie* _host;
};

class UiWindowWebHostMsie : public IUiWindowWebHost {
public:
    UiWindowWebHostMsie(IUiWindow* window);

    IUiWindow* Window = NULL;
    IoUiStorage* Storage = NULL;
    IoUiSite* Site = NULL;
    IoUiFrame* Frame = NULL;
    IoUiExternal* External = NULL;
    IWebBrowser2* WebBrowser = NULL;
    IOleObject* OleWebObject = NULL;
    IoUiBrowserEventHandler* BrowserEventHandler = NULL;
private:
    IConnectionPoint* _connPoint = NULL;
    DWORD _adviseCookie = 0;

public:
    virtual void Initialize();
    virtual void Destroy();
    virtual void SetSize(int width, int height);
    virtual void Navigate(LPCWSTR url);
    virtual bool HandleAccelerator(MSG* msg);
    virtual void PostMessageToBrowser(LPCWSTR json, void* callback);
    virtual void HandlePostMessageCallback(void* callback, LPCWSTR result, LPCWSTR error);
    virtual void Undo();
    virtual void Redo();
    virtual void Cut();
    virtual void Copy();
    virtual void Paste();
    virtual void SelectAll();

    bool ExecScript(LPCWSTR script, LPWSTR* ret = NULL, LPWSTR* ex = NULL);
};

IUiWindowWebHost* CreateMsieWebHost(IUiWindow* window);

#pragma endregion

#pragma region Implementation

#pragma region IoUiStorage

STDMETHODIMP IoUiStorage::QueryInterface(REFIID riid, void ** ppvObject) { return E_NOTIMPL; }
STDMETHODIMP_(ULONG) IoUiStorage::AddRef(void) { return 1; }
STDMETHODIMP_(ULONG) IoUiStorage::Release(void) { return 1; }
STDMETHODIMP IoUiStorage::CreateStream(const WCHAR * pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStream ** ppstm) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::OpenStream(const WCHAR * pwcsName, void * reserved1, DWORD grfMode, DWORD reserved2, IStream ** ppstm) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::CreateStorage(const WCHAR * pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStorage ** ppstg) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::OpenStorage(const WCHAR * pwcsName, IStorage * pstgPriority, DWORD grfMode, SNB snbExclude, DWORD reserved, IStorage ** ppstg) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::CopyTo(DWORD ciidExclude, IID const * rgiidExclude, SNB snbExclude, IStorage * pstgDest) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::MoveElementTo(const OLECHAR * pwcsName, IStorage * pstgDest, const OLECHAR* pwcsNewName, DWORD grfFlags) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::Commit(DWORD grfCommitFlags) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::Revert(void) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::EnumElements(DWORD reserved1, void * reserved2, DWORD reserved3, IEnumSTATSTG ** ppenum) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::DestroyElement(const OLECHAR * pwcsName) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::RenameElement(const WCHAR * pwcsOldName, const WCHAR * pwcsNewName) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::SetElementTimes(const WCHAR * pwcsName, FILETIME const * pctime, FILETIME const * patime, FILETIME const * pmtime) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::SetClass(REFCLSID clsid) { return S_OK; }
STDMETHODIMP IoUiStorage::SetStateBits(DWORD grfStateBits, DWORD grfMask) { return E_NOTIMPL; }
STDMETHODIMP IoUiStorage::Stat(STATSTG * pstatstg, DWORD grfStatFlag) { return E_NOTIMPL; }

#pragma endregion

#pragma region IoUiSite

IoUiSite::IoUiSite(UiWindowWebHostMsie* host) : _host(host) {
}

STDMETHODIMP IoUiSite::QueryInterface(REFIID riid, void ** ppvObject) {
    if (riid == IID_IUnknown || riid == IID_IOleClientSite)
        *ppvObject = (IOleClientSite*)this;
    else if (riid == IID_IOleInPlaceSite) // || riid == IID_IOleInPlaceSiteEx || riid == IID_IOleInPlaceSiteWindowless)
        *ppvObject = (IOleInPlaceSite*)this;
    else if (riid == IID_IDocHostUIHandler)
        *ppvObject = (IDocHostUIHandler*)this;
    else if (riid == IID_IOleCommandTarget)
        *ppvObject = (IOleCommandTarget*)this;
    else if (riid == IID_IDocHostShowUI)
        *ppvObject = (IDocHostShowUI*)this;
    else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    return S_OK;
}

STDMETHODIMP_(ULONG) IoUiSite::AddRef(void) { return 1; }
STDMETHODIMP_(ULONG) IoUiSite::Release(void) { return 1; }

STDMETHODIMP IoUiSite::SaveObject() { return E_NOTIMPL; }
STDMETHODIMP IoUiSite::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker ** ppmk) { return E_NOTIMPL; }
STDMETHODIMP IoUiSite::GetContainer(LPOLECONTAINER FAR* ppContainer) { *ppContainer = NULL; return E_NOINTERFACE; }
STDMETHODIMP IoUiSite::ShowObject() { return NOERROR; }
STDMETHODIMP IoUiSite::OnShowWindow(BOOL fShow) { return E_NOTIMPL; }
STDMETHODIMP IoUiSite::RequestNewObjectLayout() { return E_NOTIMPL; }
STDMETHODIMP IoUiSite::GetWindow(HWND FAR* lphwnd) { *lphwnd = *_host->Window; return S_OK; }
STDMETHODIMP IoUiSite::ContextSensitiveHelp(BOOL fEnterMode) { return E_NOTIMPL; }
STDMETHODIMP IoUiSite::CanInPlaceActivate() { return S_OK; }
STDMETHODIMP IoUiSite::OnInPlaceActivate() { return S_OK; }
STDMETHODIMP IoUiSite::OnUIActivate() { return S_OK; }

STDMETHODIMP IoUiSite::GetWindowContext(
    LPOLEINPLACEFRAME FAR* ppFrame,
    LPOLEINPLACEUIWINDOW FAR* ppDoc,
    LPRECT prcPosRect,
    LPRECT prcClipRect,
    LPOLEINPLACEFRAMEINFO lpFrameInfo) {
    *ppFrame = _host->Frame;
    *ppDoc = NULL;
    GetClientRect(*_host->Window, prcPosRect);
    GetClientRect(*_host->Window, prcClipRect);

    lpFrameInfo->fMDIApp = FALSE;
    lpFrameInfo->hwndFrame = *_host->Window;
    lpFrameInfo->haccel = NULL;
    lpFrameInfo->cAccelEntries = 0;

    return S_OK;
}

STDMETHODIMP IoUiSite::Scroll(SIZE scrollExtent) { return E_NOTIMPL; }
STDMETHODIMP IoUiSite::OnUIDeactivate(BOOL fUndoable) { return S_OK; }
STDMETHODIMP IoUiSite::OnInPlaceDeactivate() { return S_OK; }
STDMETHODIMP IoUiSite::DiscardUndoState() { return E_NOTIMPL; }
STDMETHODIMP IoUiSite::DeactivateAndUndo() { return E_NOTIMPL; }

STDMETHODIMP IoUiSite::OnPosRectChange(LPCRECT lprcPosRect) {
    IOleInPlaceObject* inPlace;
    HRESULT hr = _host->WebBrowser->QueryInterface(IID_IOleInPlaceObject, (void**)&inPlace);
    if (SUCCEEDED(hr)) {
        inPlace->SetObjectRects(lprcPosRect, lprcPosRect);
        inPlace->Release();
    }
    return S_OK;
}

STDMETHODIMP IoUiSite::EnableModeless(BOOL fEnable) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::FilterDataObject(IDataObject *pDO, IDataObject **ppDORet) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::GetDropTarget(IDropTarget *pDropTarget, IDropTarget **ppDropTarget) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::GetExternal(IDispatch **ppDispatch) {
    *ppDispatch = _host->External;
    return S_OK;
}

STDMETHODIMP IoUiSite::GetHostInfo(DOCHOSTUIINFO *pInfo) {
    //return E_NOTIMPL;
    pInfo->cbSize = sizeof(DOCHOSTUIINFO);
    pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_THEME | DOCHOSTUIFLAG_DISABLE_HELP_MENU | DOCHOSTUIFLAG_DPI_AWARE;
    pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
    return S_OK;
}

STDMETHODIMP IoUiSite::GetOptionKeyPath(LPOLESTR *pchKey, DWORD dwReserved) {
    return S_OK;
}

STDMETHODIMP IoUiSite::HideUI(void) {
    return S_OK;
}

STDMETHODIMP IoUiSite::OnDocWindowActivate(BOOL fActive) {
    return S_OK;
}

STDMETHODIMP IoUiSite::OnFrameWindowActivate(BOOL fActive) {
    return S_OK;
}

STDMETHODIMP IoUiSite::ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow *pUIWindow, BOOL fFrameWindow) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::ShowContextMenu(DWORD dwID, POINT *ppt, IUnknown *pcmdtReserved, IDispatch *pdispReserved) {
    return S_OK;
}

STDMETHODIMP IoUiSite::ShowUI(DWORD dwID, IOleInPlaceActiveObject *pActiveObject, IOleCommandTarget *pCommandTarget, IOleInPlaceFrame *pFrame, IOleInPlaceUIWindow *pDoc) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::TranslateAccelerator(LPMSG lpMsg, const GUID *pguidCmdGroup,DWORD nCmdID) {
    return S_FALSE;
}

STDMETHODIMP IoUiSite::TranslateUrl(DWORD dwTranslate, OLECHAR *pchURLIn, OLECHAR **ppchURLOut) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::UpdateUI(void) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::Exec(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut) {
    switch (nCmdID) {
    case OLECMDID_OPEN:
    case OLECMDID_NEW:
    case OLECMDID_SAVE:
    case OLECMDID_SAVEAS:
    case OLECMDID_SAVECOPYAS:
    case OLECMDID_PRINT:
    case OLECMDID_PRINTPREVIEW:
    case OLECMDID_PAGESETUP:
    case OLECMDID_SPELL:
    case OLECMDID_PROPERTIES:
    case OLECMDID_ZOOM:
    case OLECMDID_UPDATECOMMANDS:
    case OLECMDID_REFRESH:
    case OLECMDID_STOP:
    case OLECMDID_HIDETOOLBARS:
    case OLECMDID_ONTOOLBARACTIVATED:
    case OLECMDID_FIND:
    case OLECMDID_PREREFRESH:
    case OLECMDID_SHOWSCRIPTERROR:
    case OLECMDID_SHOWMESSAGE:
    case OLECMDID_SHOWFIND:
    case OLECMDID_SHOWPAGESETUP:
    case OLECMDID_SHOWPRINT:
    case OLECMDID_ALLOWUILESSSAVEAS:
    case OLECMDID_DONTDOWNLOADCSS:
    case OLECMDID_PRINT2:
    case OLECMDID_PRINTPREVIEW2:
    case OLECMDID_SETPRINTTEMPLATE:
    case OLECMDID_GETPRINTTEMPLATE:
    case OLECMDID_SHOWPAGEACTIONMENU:
    case OLECMDID_ADDTRAVELENTRY:
    case OLECMDID_UPDATETRAVELENTRY:
    case OLECMDID_UPDATEBACKFORWARDSTATE:
    case OLECMDID_OPTICAL_ZOOM:
    case 68://OLECMDID_SHOWTASKDLG:
    case 73://OLECMDID_USER_OPTICAL_ZOOM:
        return S_OK;
    case OLECMDID_CLOSE:
        SendMessage(*_host->Window, WM_CLOSE, 0, 0);
        return S_OK;
    default:
        return OLECMDERR_E_NOTSUPPORTED;
    }
}

STDMETHODIMP IoUiSite::QueryStatus(const GUID *pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT *pCmdText) {
    return S_OK;
}

STDMETHODIMP IoUiSite::ShowMessage(HWND hwnd, LPOLESTR lpstrText, LPOLESTR lpstrCaption, DWORD dwType, LPOLESTR lpstrHelpFile, DWORD dwHelpContext, LRESULT *plResult) {
    if ((dwType & MB_YESNO) && (dwType & MB_ICONQUESTION)) {
        // window.close() => Do you want to close this window? Yes/No
        *plResult = IDYES;
        return S_OK;
    }
    auto len = GetWindowTextLength(hwnd);
    WCHAR* title = new WCHAR[len + 1];
    int res = GetWindowText(hwnd, title, len + 1);
    *plResult = MessageBox(*_host->Window, lpstrText, title, dwType);
    delete[] title;
    return S_OK;
}

STDMETHODIMP IoUiSite::ShowHelp(HWND hwnd, LPOLESTR pszHelpFile, UINT uCommand, DWORD dwData, POINT ptMouse, IDispatch *pDispatchObjectHit) {
    return E_NOTIMPL;
}

#pragma endregion

#pragma region IoUiFrame

IoUiFrame::IoUiFrame(UiWindowWebHostMsie* host) : _host(host) { }
STDMETHODIMP IoUiFrame::QueryInterface(REFIID riid, void ** ppvObject) { return E_NOTIMPL; }
STDMETHODIMP_(ULONG) IoUiFrame::AddRef(void) { return 1; }
STDMETHODIMP_(ULONG) IoUiFrame::Release(void) { return 1; }
STDMETHODIMP IoUiFrame::GetWindow(HWND FAR* lphwnd) { *lphwnd = *_host->Window; return S_OK; }
STDMETHODIMP IoUiFrame::ContextSensitiveHelp(BOOL fEnterMode) { return E_NOTIMPL; }
STDMETHODIMP IoUiFrame::GetBorder(LPRECT lprectBorder) { return E_NOTIMPL; }
STDMETHODIMP IoUiFrame::RequestBorderSpace(LPCBORDERWIDTHS pborderwidths) { return E_NOTIMPL; }
STDMETHODIMP IoUiFrame::SetBorderSpace(LPCBORDERWIDTHS pborderwidths) { return E_NOTIMPL; }
STDMETHODIMP IoUiFrame::SetActiveObject(IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName) { return S_OK; }
STDMETHODIMP IoUiFrame::InsertMenus(HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths) { return S_OK; }
STDMETHODIMP IoUiFrame::SetMenu(HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject) { return S_OK; }
STDMETHODIMP IoUiFrame::RemoveMenus(HMENU hmenuShared) { return E_NOTIMPL; }
STDMETHODIMP IoUiFrame::SetStatusText(LPCOLESTR pszStatusText) { return S_OK; }
STDMETHODIMP IoUiFrame::EnableModeless(BOOL fEnable) { return S_OK; }
STDMETHODIMP IoUiFrame::TranslateAccelerator(LPMSG lpmsg, WORD wID) { return S_FALSE; }

#pragma endregion

#pragma region IoUiBrowserEventHandler

IoUiBrowserEventHandler::IoUiBrowserEventHandler(UiWindowWebHostMsie* host) : _host(host) {}
IoUiBrowserEventHandler::~IoUiBrowserEventHandler() {}

STDMETHODIMP IoUiBrowserEventHandler::QueryInterface(REFIID riid, void ** ppvObject) {
    if (riid == DIID_DWebBrowserEvents2) {
        *ppvObject = (DWebBrowserEvents2*)this;
    } else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

STDMETHODIMP_(ULONG) IoUiBrowserEventHandler::AddRef(void) { return 1; }
STDMETHODIMP_(ULONG) IoUiBrowserEventHandler::Release(void) { return 1; }
STDMETHODIMP IoUiBrowserEventHandler::GetTypeInfoCount(UINT* pctinfo) { *pctinfo = 1; return S_OK; }
STDMETHODIMP IoUiBrowserEventHandler::GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo) { return E_NOTIMPL; }
STDMETHODIMP IoUiBrowserEventHandler::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid) { return E_NOTIMPL; }

STDMETHODIMP IoUiBrowserEventHandler::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags,
    DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr) {
    // https://msdn.microsoft.com/en-us/library/aa768283(v=vs.85).aspx
    switch (dispidMember) {
    case DISPID_DOCUMENTCOMPLETE:
        _host->Window->DocumentComplete();
        return S_OK;
    case DISPID_DOWNLOADBEGIN:
        _host->Window->DownloadBegin();
        return S_OK;
    case DISPID_DOWNLOADCOMPLETE:
        _host->Window->DownloadComplete();
        return S_OK;
    case DISPID_NAVIGATECOMPLETE2:
        NavigateComplete();
        return S_OK;
    case DISPID_TITLECHANGE:
        SetWindowText(*_host->Window, pdispparams->rgvarg[0].bstrVal);
        return S_OK;
    case DISPID_NEWWINDOW3:
        if (pdispparams->rgvarg[2].intVal & NWMF_USERREQUESTED) {
            // user has shift-clicked a link or opened a new window
            *pdispparams->rgvarg[3].pboolVal = VARIANT_TRUE; // cancel = true
        } else {
            *pdispparams->rgvarg[3].pboolVal = VARIANT_TRUE;
            // *pdispparams->rgvarg[4].pdispVal = TODO: open new window
        }
        return S_OK;
    default:
        return E_NOTIMPL;
    }
}

void IoUiBrowserEventHandler::NavigateComplete() {
    RECT rect;
    GetClientRect(*_host->Window, &rect);
    _host->OleWebObject->DoVerb(OLEIVERB_UIACTIVATE, NULL, _host->Site, -1, *_host->Window, &rect);

    _host->ExecScript(L"window.backend = {"
        L"postMessage: function(data, cb) { external.pm(JSON.stringify(data), cb ? function(res, err) { if (typeof cb === 'function') cb(JSON.parse(res), err); } : null); },"
        L"onMessage: null"
        L"};");
    if (atoi(_host->Window->GetEngineVersion()) <= 7) {
        int len = MultiByteToWideChar(CP_ACP, 0, IE_JSON_POLYFILL_JS_CODE, -1, NULL, 0);
        WCHAR* jsonPolyfill = new WCHAR[len];
        MultiByteToWideChar(CP_ACP, 0, IE_JSON_POLYFILL_JS_CODE, -1, jsonPolyfill, len);
        _host->ExecScript(jsonPolyfill);
        delete[] jsonPolyfill;
    }
}

#pragma endregion

#pragma region IoUiExternal

IoUiExternal::IoUiExternal(UiWindowWebHostMsie* host) : _host(host) {
}

STDMETHODIMP IoUiExternal::QueryInterface(REFIID riid, void ** ppvObject) {
    if (riid == IID_IUnknown || riid == IID_IDispatch)
        *ppvObject = (IUnknown*)this;
    else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

STDMETHODIMP_(ULONG) IoUiExternal::AddRef(void) { return 1; }
STDMETHODIMP_(ULONG) IoUiExternal::Release(void) { return 1; }

// IDispatch
STDMETHODIMP IoUiExternal::GetTypeInfoCount(UINT* pctinfo) {
    pctinfo = 0;
    return S_OK;
}

STDMETHODIMP IoUiExternal::GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiExternal::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid) {
    HRESULT result = S_OK;
    for (UINT i = 0; i < cNames; i++) {
        rgdispid[i] = DISPID_UNKNOWN;
        if (!wcscmp(L"pm", rgszNames[i])) {
            rgdispid[i] = 1;
            break;
        }
        if (rgdispid[i] == DISPID_UNKNOWN) {
            result = DISP_E_UNKNOWNNAME;
        }
    }
    return S_OK;
}

STDMETHODIMP IoUiExternal::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr) {
    if (dispidMember < 0 || dispidMember > 1) {
        return E_NOTIMPL;
    }
    switch (dispidMember) {
    case 1:
        return PostMsg(wFlags, pdispparams, pvarResult, pexcepinfo);
    default:
        return 0;
    }
}

HRESULT IoUiExternal::PostMsg(WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo) {
    if ((wFlags & DISPATCH_METHOD) != DISPATCH_METHOD) {
        return E_NOTIMPL;
    }
    auto msg = pdispparams->rgvarg[1].bstrVal;
    IDispatch* callback = NULL;
    if (pdispparams->rgvarg[0].vt == VT_DISPATCH) {
        callback = pdispparams->rgvarg[0].pdispVal;
        callback->AddRef();
    }
    _host->Window->PostMessageToBackend(msg, callback);
    if (pvarResult) {
        pvarResult->vt = VT_EMPTY;
    }
    return S_OK;
}

#pragma endregion

#pragma region UiWindowWebHostMsie

IUiWindowWebHost* CreateMsieWebHost(IUiWindow* window) {
    return new UiWindowWebHostMsie(window);
}

UiWindowWebHostMsie::UiWindowWebHostMsie(IUiWindow* window) : Window(window) {
    Storage = new IoUiStorage();
    Site = new IoUiSite(this);
    Frame = new IoUiFrame(this);
    BrowserEventHandler = new IoUiBrowserEventHandler(this);
    External = new IoUiExternal(this);
}

void UiWindowWebHostMsie::Initialize() {
    HRESULT hresult;
    OleCreate(CLSID_WebBrowser, IID_IOleObject, OLERENDER_DRAW, 0, Site, Storage, (void**)&OleWebObject);

    OleWebObject->SetHostNames(L"io-ui", L"webview");

    OleSetContainedObject(OleWebObject, TRUE);

    RECT rect;
    GetClientRect(*Window, &rect);

    OleWebObject->DoVerb(OLEIVERB_SHOW, NULL, Site, -1, *Window, &rect);

    OleWebObject->QueryInterface(IID_IWebBrowser2, (void**)&WebBrowser);

    WebBrowser->put_Left(0);
    WebBrowser->put_Top(0);
    WebBrowser->put_Width(rect.right);
    WebBrowser->put_Height(rect.bottom);
    WebBrowser->put_Silent(VARIANT_TRUE);

    IConnectionPointContainer* pConnectionPointContainer;
    hresult = OleWebObject->QueryInterface(IID_IConnectionPointContainer, (void**)&pConnectionPointContainer);
    if (SUCCEEDED(hresult)) {
        hresult = pConnectionPointContainer->FindConnectionPoint(DIID_DWebBrowserEvents2, &_connPoint);
        if (SUCCEEDED(hresult)) {
            DWORD cookie;
            hresult = _connPoint->Advise(BrowserEventHandler, &cookie);
            if (SUCCEEDED(hresult)) {
                _adviseCookie = cookie;
            }
        }
        pConnectionPointContainer->Release();
    }
}

void UiWindowWebHostMsie::Destroy() {
    if (WebBrowser) {
        // http://stackoverflow.com/questions/8273910/how-to-cleanly-destroy-webbrowser-control
        if (_connPoint) {
           if (_adviseCookie) {
                _connPoint->Unadvise(_adviseCookie);
            }
            _connPoint->Release();
        }
        IOleInPlaceObject* oleInPlaceObject;
        HRESULT hr = WebBrowser->QueryInterface(IID_IOleInPlaceObject, (void**)&oleInPlaceObject);
        if (FAILED(hr)) oleInPlaceObject = NULL;
        WebBrowser->Stop();
        //WebBrowser->ExecWB(OLECMDID_CLOSE, OLECMDEXECOPT_DONTPROMPTUSER, 0, 0);
        WebBrowser->put_Visible(VARIANT_FALSE);
        WebBrowser->Release();
        if (oleInPlaceObject) {
            oleInPlaceObject->InPlaceDeactivate();
            oleInPlaceObject->Release();
        }
        OleWebObject->DoVerb(OLEIVERB_HIDE, NULL, Site, 0, *Window, NULL);
        OleWebObject->Close(OLECLOSE_NOSAVE);
        OleSetContainedObject(OleWebObject, FALSE);
        OleWebObject->SetClientSite(NULL);
        CoDisconnectObject(OleWebObject, NULL);
        OleWebObject->Release();
    }
    delete External;
    delete Frame;
    delete Site;
    delete Storage;
}

void UiWindowWebHostMsie::SetSize(int width, int height) {
    WebBrowser->put_Width(width);
    WebBrowser->put_Height(height);
}

void UiWindowWebHostMsie::Navigate(LPCWSTR url) {
    VARIANT vURL;
    vURL.vt = VT_BSTR;
    vURL.bstrVal = SysAllocString(url);
    VARIANT ve1, ve2, ve3, ve4;
    ve1.vt = VT_EMPTY;
    ve2.vt = VT_EMPTY;
    ve3.vt = VT_EMPTY;
    ve4.vt = VT_EMPTY;

    WebBrowser->Navigate2(&vURL, &ve1, &ve2, &ve3, &ve4);
    VariantClear(&vURL);
}

bool UiWindowWebHostMsie::HandleAccelerator(MSG* msg) {
    if (msg->message == WM_KEYDOWN) {
        switch (msg->wParam) {
        case VK_BROWSER_BACK:
        case VK_BROWSER_FORWARD:
        case VK_BROWSER_REFRESH:
        case VK_BROWSER_STOP:
        case VK_BROWSER_SEARCH:
        case VK_BROWSER_FAVORITES:
        case VK_BROWSER_HOME:
            return true;
        case 'L':
        case 'N':
        case 'O':
        case VK_OEM_PLUS:
        case VK_ADD:
        case VK_OEM_MINUS:
        case VK_SUBTRACT:
            // TODO: dispatch synthetic events
            if (GetKeyState(VK_CONTROL) & 0x8000)
                return true;
        }
    }
    IOleInPlaceActiveObject* pIOIPAO = NULL;
    HRESULT hr = WebBrowser->QueryInterface(IID_IOleInPlaceActiveObject, (void**)&pIOIPAO);
    if (SUCCEEDED(hr)) {
        hr = pIOIPAO->TranslateAccelerator(msg);
        pIOIPAO->Release();
    }
    return hr == S_OK;
}

void UiWindowWebHostMsie::PostMessageToBrowser(LPCWSTR json, void* callback) {
    WCHAR* script = new WCHAR[wcslen(json) + 128];
    wcscpy(script, L"JSON.stringify(typeof backend.onMessage === 'function' ? backend.onMessage(");
    wcscat(script, json);
    wcscat(script, L"):null)");
    LPWSTR ret;
    LPWSTR ex;
    if (ExecScript(script, &ret, &ex)) {
        Window->HandlePostMessageCallback(callback, ret, ex);
    }
    if (ret) delete[] ret;
    if (ex) delete[] ex;
    delete[] script;
}

bool UiWindowWebHostMsie::ExecScript(LPCWSTR code, LPWSTR* ret, LPWSTR* ex) {
    IDispatch* doc;
    IDispatch* script = NULL;
    HRESULT hr = WebBrowser->get_Document(&doc);
    if (SUCCEEDED(hr)) {
        IHTMLDocument2* htmlDoc;
        hr = doc->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDoc);
        if (SUCCEEDED(hr)) {
            hr = htmlDoc->get_Script(&script);
            if (FAILED(hr)) {
                script = NULL;
            }
            htmlDoc->Release();
        }
        doc->Release();
    }
    if (!script)
        return false;
    DISPID evalDispId;
    OLECHAR FAR* evalName = L"eval";
    hr = script->GetIDsOfNames(IID_NULL, &evalName, 1, LOCALE_USER_DEFAULT, &evalDispId);
    if (FAILED(hr)) {
        evalName = L"execScript";
        hr = script->GetIDsOfNames(IID_NULL, &evalName, 1, LOCALE_USER_DEFAULT, &evalDispId);
        if (FAILED(hr)) {
            evalDispId = 0;
        }
        // old IE doesn't initialize eval on window load but execScript is available
        // execScript is worse because it doesn't give a result but it's ok to run init scripts
    }
    if (!evalDispId) {
        script->Release();
        return false;
    }
    VARIANTARG args[1] = {};
    DISPPARAMS params = { 0 };
    params.cArgs = 1;
    params.rgvarg = args;
    VariantInit(&args[0]);
    args[0].vt = VT_BSTR;
    args[0].bstrVal = SysAllocString(code);

    VARIANT retVar = { 0 };
    VariantInit(&retVar);
    EXCEPINFO exInfo = { 0 };

    hr = script->Invoke(evalDispId, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, &retVar, &exInfo, NULL);
    SysFreeString(args[0].bstrVal);
    if (SUCCEEDED(hr)) {
        if (hr == S_OK && retVar.vt == VT_BSTR && retVar.bstrVal != NULL) {
            if (ret) {
                *ret = new WCHAR[lstrlen(retVar.bstrVal) + 1];
                lstrcpy(*ret, retVar.bstrVal);
            }
            VariantClear(&retVar);
        } else if (ret) {
            *ret = NULL;
        }
        if (hr == DISP_E_EXCEPTION) {
            if (ex) {
                if (exInfo.bstrDescription != NULL) {
                    *ex = new WCHAR[lstrlen(exInfo.bstrDescription) + 1];
                    lstrcpy(*ex, exInfo.bstrDescription);
                }
                if (exInfo.bstrDescription)
                    SysFreeString(exInfo.bstrDescription);
                if (exInfo.bstrSource)
                    SysFreeString(exInfo.bstrSource);
                if (exInfo.bstrHelpFile)
                    SysFreeString(exInfo.bstrHelpFile);
            }
        } else if (ex) {
            *ex = NULL;
        }
    }
    script->Release();
    return SUCCEEDED(hr);
}

void UiWindowWebHostMsie::HandlePostMessageCallback(void* callback, LPCWSTR result, LPCWSTR error) {
    IDispatch* dispCallback = (IDispatch*)callback;
    DISPPARAMS params = { 0 };
    params.cArgs = 2;
    params.rgvarg = new VARIANTARG[2];
    VariantInit(&params.rgvarg[0]);
    VariantInit(&params.rgvarg[1]);
    if (result) {
        params.rgvarg[1].vt = VT_BSTR;
        params.rgvarg[1].bstrVal = SysAllocString(result);
    } else {
        params.rgvarg[1].vt = VT_NULL;
    }
    if (error) {
        params.rgvarg[0].vt = VT_BSTR;
        params.rgvarg[0].bstrVal = SysAllocString(error);
    } else {
        params.rgvarg[0].vt = VT_NULL;
    }
    HRESULT hr = dispCallback->Invoke(DISPID_VALUE, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, NULL, NULL, NULL);
    dispCallback->Release();
}

void UiWindowWebHostMsie::Undo() {
    WebBrowser->ExecWB(OLECMDID_UNDO, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

void UiWindowWebHostMsie::Redo() {
    WebBrowser->ExecWB(OLECMDID_REDO, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

void UiWindowWebHostMsie::Cut() {
    WebBrowser->ExecWB(OLECMDID_CUT, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

void UiWindowWebHostMsie::Copy() {
    WebBrowser->ExecWB(OLECMDID_COPY, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

void UiWindowWebHostMsie::Paste() {
    WebBrowser->ExecWB(OLECMDID_PASTE, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

void UiWindowWebHostMsie::SelectAll() {
    WebBrowser->ExecWB(OLECMDID_SELECTALL, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

#pragma endregion

#pragma endregion
