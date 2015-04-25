#include "ui-window.h"
#include "../perf-trace/perf-trace.h"
#include "../ui-module/ui-module.h"
#include "ie-json-polyfill.h"
#include <Windows.h>
#include <MsHTML.h>
#include <MsHtmHst.h>
#include <ExDisp.h>
#include <ExDispid.h>
#include <shlguid.h>
#include <Shlwapi.h>
#include <iostream>
#include <iomanip>

#define WM_USER_CREATE_WINDOW (WM_APP + 1)
#define WM_USER_NAVIGATE (WM_APP + 2)
#define WM_USER_POST_MSG (WM_APP + 3)
#define WM_USER_POST_MSG_CB (WM_APP + 4)

#define BASE_DPI 96

#pragma region Links

// https://msdn.microsoft.com/en-us/library/ie/hh772404(v=vs.85).aspx
// https://msdn.microsoft.com/en-us/library/aa770041(v=vs.85).aspx

#pragma endregion

#pragma region Interface

class UiWindowWin;

#pragma region IBrowserEventHandler

class IBrowserEventHandler {
public:
    virtual void DocumentComplete() = 0;
    virtual void NavigateComplete() = 0;
    virtual void TitleChange(BSTR title) = 0;
};

#pragma endregion

#pragma region IoUiContainer

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

#pragma endregion

#pragma region IoUiStorage

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

#pragma endregion

#pragma region IoUiFrame

class IoUiFrame : public IOleInPlaceFrame {
public:
    UiWindowWin* _host;
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
};

#pragma endregion

#pragma region IoUiSite

class IoUiSite : public IOleClientSite, public IOleInPlaceSite, public IDocHostUIHandler, public IDocHostShowUI, public IOleCommandTarget {
public:
    UiWindowWin* _host;

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
    STDMETHODIMP EnableModeless(
        /* [in ] */ BOOL fEnable);
    STDMETHODIMP FilterDataObject(
        /* [in ] */ IDataObject __RPC_FAR *pDO,
        /* [out] */ IDataObject __RPC_FAR *__RPC_FAR *ppDORet);
    STDMETHODIMP GetDropTarget(
        /* [in ] */ IDropTarget __RPC_FAR *pDropTarget,
        /* [out] */ IDropTarget __RPC_FAR *__RPC_FAR *ppDropTarget);
    STDMETHODIMP GetExternal(
        /* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDispatch);
    STDMETHODIMP GetHostInfo(
        /* [i/o] */ DOCHOSTUIINFO __RPC_FAR *pInfo);
    STDMETHODIMP GetOptionKeyPath(
        /* [out] */ LPOLESTR __RPC_FAR *pchKey,
        /* [in ] */ DWORD dwReserved);
    STDMETHODIMP HideUI(void);
    STDMETHODIMP OnDocWindowActivate(
        /* [in ] */ BOOL fActive);
    STDMETHODIMP OnFrameWindowActivate(
        /* [in ] */ BOOL fActive);
    STDMETHODIMP ResizeBorder(
        /* [in ] */ LPCRECT prcBorder,
        /* [in ] */ IOleInPlaceUIWindow __RPC_FAR *pUIWindow,
        /* [in ] */ BOOL fFrameWindow);
    STDMETHODIMP ShowContextMenu(
        /* [in ] */ DWORD dwID,
        /* [in ] */ POINT __RPC_FAR *ppt,
        /* [in ] */ IUnknown __RPC_FAR *pcmdtReserved,
        /* [in ] */ IDispatch __RPC_FAR *pdispReserved);
    STDMETHODIMP ShowUI(
        /* [in ] */ DWORD dwID,
        /* [in ] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject,
        /* [in ] */ IOleCommandTarget __RPC_FAR *pCommandTarget,
        /* [in ] */ IOleInPlaceFrame __RPC_FAR *pFrame,
        /* [in ] */ IOleInPlaceUIWindow __RPC_FAR *pDoc);
    STDMETHODIMP TranslateAccelerator(
        /* [in ] */ LPMSG lpMsg,
        /* [in ] */ const GUID __RPC_FAR *pguidCmdGroup,
        /* [in ] */ DWORD nCmdID);
    STDMETHODIMP TranslateUrl(
        /* [in ] */ DWORD dwTranslate,
        /* [in ] */ OLECHAR __RPC_FAR *pchURLIn,
        /* [out] */ OLECHAR __RPC_FAR *__RPC_FAR *ppchURLOut);
    STDMETHODIMP UpdateUI(void);

    // IDocHostShowUI
    STDMETHODIMP ShowMessage(HWND hwnd, LPOLESTR lpstrText, LPOLESTR lpstrCaption, DWORD dwType, LPOLESTR lpstrHelpFile, DWORD dwHelpContext, LRESULT *plResult);
    STDMETHODIMP ShowHelp(HWND hwnd, LPOLESTR pszHelpFile, UINT uCommand, DWORD dwData, POINT ptMouse, IDispatch *pDispatchObjectHit);

    // IOleCommandTarget
    STDMETHODIMP Exec(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut);
    STDMETHODIMP QueryStatus(const GUID *pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT *pCmdText);
};

#pragma endregion

#pragma region BrowserEventHandler

class BrowserEventHandler : public DWebBrowserEvents2 {
public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    // IDispatch
    STDMETHODIMP GetTypeInfoCount(UINT* pctinfo);
    STDMETHODIMP GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo);
    STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid);
    STDMETHODIMP Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr);

    BrowserEventHandler(IBrowserEventHandler* eventHandler);
    ~BrowserEventHandler();

private:
    IBrowserEventHandler* _eventHandler;
};

#pragma endregion

#pragma region IoUiExternal

class IoUiExternal : public IDispatch {
public:
    IoUiExternal();
    UiWindowWin* _host;

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
    typedef HRESULT(IoUiExternal::*ExternalObjectMethod)(WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo);
    struct MethodDesc {
        OLECHAR* Name;
        ExternalObjectMethod Method;
    public:
        MethodDesc() {};
        MethodDesc(OLECHAR* name, ExternalObjectMethod method) : Name(name), Method(method) {}
    };
    static const int METHODS_COUNT = 1;
    MethodDesc _methods[METHODS_COUNT];
};

#pragma endregion

#pragma region UiWindowWin

class UiWindowWin : public UiWindow, IBrowserEventHandler {
protected:
    virtual void Show(WindowRect& rect);
    virtual void Close();
    virtual void Navigate(Utf8String* url);
    virtual void PostMsg(Utf8String* msg, v8::Persistent<v8::Function>* callback);
    virtual void MsgCallback(void* callback, Utf8String* result, Utf8String* error);

    virtual void SetWindowRect(WindowRect& rect);
    virtual WindowRect GetWindowRect();
    virtual WindowRect GetScreenRect();
    virtual Utf8String* GetTitle();
    virtual WINDOW_STATE GetState();
    virtual void SetState(WINDOW_STATE state);
    virtual bool GetResizable();
    virtual void SetResizable(bool resizable);
    virtual bool GetFrame();
    virtual void SetFrame(bool frame);
    virtual bool GetTopmost();
    virtual void SetTopmost(bool frame);
    virtual double GetOpacity();
    virtual void SetOpacity(double opacity);

    friend class IoUiSite;
    friend class IoUiExternal;
public:
    UiWindowWin();
    void ShowOsWindow();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    HWND hwnd;

    BOOL HandleMessage(UINT, WPARAM, LPARAM);
    bool HandleAccelerator(MSG* msg);

    void CreateEmbeddedWebControl(void);
    void DestroyEmbeddedWebControl(void);

    static WCHAR _wndClassName[];
    static DWORD _mainThreadId;
    static int _dpi;

private:
    IoUiStorage _storage;
    IoUiSite _site;
    IoUiFrame _frame;
    IoUiExternal _external;
    IOleObject* _oleWebObject;
    IWebBrowser2* _webBrowser = NULL;
    BrowserEventHandler* _browserEventHandler;
    HMENU _hmenu = NULL;
    WindowRect _rect;

    bool _resizeMove = false;

    // IBrowserEventHandler
    void DocumentComplete();
    void NavigateComplete();
    void TitleChange(BSTR title);
    void SetStyle();
    void OnPaint(HDC hdc);

    void CreateWindowMenu();
    void AddMenu(UiMenu* menu, HMENU parent);
    void AddFileMenuItems(HMENU parent, bool hasItems);
    void AddEditMenuItems(HMENU parent, bool hasItems);
    void AddHelpMenuItems(HMENU parent, bool hasItems);
    void HandleInternalMenu(MENU_INTL menu);

    UI_RESULT ExecScript(WCHAR* script, LPVARIANT ret = NULL, LPEXCEPINFO ex = NULL);
};

#pragma endregion

#pragma region Data structures

struct MsgCallbackParam {
    void* Callback;
    Utf8String* Result;
    Utf8String* Error;
};

#pragma endregion

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
    else
    {
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
STDMETHODIMP IoUiSite::GetWindow(HWND FAR* lphwnd) { *lphwnd = _host->hwnd; return S_OK; }
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
    *ppFrame = &_host->_frame;
    *ppDoc = NULL;
    GetClientRect(_host->hwnd, prcPosRect);
    GetClientRect(_host->hwnd, prcClipRect);

    lpFrameInfo->fMDIApp = FALSE;
    lpFrameInfo->hwndFrame = _host->hwnd;
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
    HRESULT hr = this->_host->_webBrowser->QueryInterface(IID_IOleInPlaceObject, (void**)&inPlace);
    if (SUCCEEDED(hr)) {
        inPlace->SetObjectRects(lprcPosRect, lprcPosRect);
        inPlace->Release();
    }
    return S_OK;
}

STDMETHODIMP IoUiSite::EnableModeless(
    /* [in ] */ BOOL fEnable) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::FilterDataObject(
    /* [in ] */ IDataObject __RPC_FAR *pDO,
    /* [out] */ IDataObject __RPC_FAR *__RPC_FAR *ppDORet) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::GetDropTarget(
    /* [in ] */ IDropTarget __RPC_FAR *pDropTarget,
    /* [out] */ IDropTarget __RPC_FAR *__RPC_FAR *ppDropTarget) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::GetExternal(
    /* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDispatch) {
    *ppDispatch = &_host->_external;
    return S_OK;
}

STDMETHODIMP IoUiSite::GetHostInfo(
    /* [i/o] */ DOCHOSTUIINFO __RPC_FAR *pInfo) {
    //return E_NOTIMPL;
    pInfo->cbSize = sizeof(DOCHOSTUIINFO);
    pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_THEME | DOCHOSTUIFLAG_DISABLE_HELP_MENU | DOCHOSTUIFLAG_DPI_AWARE;
    pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
    return S_OK;
}

STDMETHODIMP IoUiSite::GetOptionKeyPath(
    /* [out] */ LPOLESTR __RPC_FAR *pchKey,
    /* [in ] */ DWORD dwReserved) {
    return S_OK;
}

STDMETHODIMP IoUiSite::HideUI(void) {
    return S_OK;
}

STDMETHODIMP IoUiSite::OnDocWindowActivate(
    /* [in ] */ BOOL fActive) {
    return S_OK;
}

STDMETHODIMP IoUiSite::OnFrameWindowActivate(
    /* [in ] */ BOOL fActive) {
    return S_OK;
}

STDMETHODIMP IoUiSite::ResizeBorder(
    /* [in ] */ LPCRECT prcBorder,
    /* [in ] */ IOleInPlaceUIWindow __RPC_FAR *pUIWindow,
    /* [in ] */ BOOL fFrameWindow) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::ShowContextMenu(
    /* [in ] */ DWORD dwID,
    /* [in ] */ POINT __RPC_FAR *ppt,
    /* [in ] */ IUnknown __RPC_FAR *pcmdtReserved,
    /* [in ] */ IDispatch __RPC_FAR *pdispReserved) {
    return S_OK;
}

STDMETHODIMP IoUiSite::ShowUI(
    /* [in ] */ DWORD dwID,
    /* [in ] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject,
    /* [in ] */ IOleCommandTarget __RPC_FAR *pCommandTarget,
    /* [in ] */ IOleInPlaceFrame __RPC_FAR *pFrame,
    /* [in ] */ IOleInPlaceUIWindow __RPC_FAR *pDoc) {
    return E_NOTIMPL;
}

STDMETHODIMP IoUiSite::TranslateAccelerator(
    /* [in ] */ LPMSG lpMsg,
    /* [in ] */ const GUID __RPC_FAR *pguidCmdGroup,
    /* [in ] */ DWORD nCmdID) {
    return S_FALSE;
}

STDMETHODIMP IoUiSite::TranslateUrl(
    /* [in ] */ DWORD dwTranslate,
    /* [in ] */ OLECHAR __RPC_FAR *pchURLIn,
    /* [out] */ OLECHAR __RPC_FAR *__RPC_FAR *ppchURLOut) {
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
    //case OLECMDID_CUT:
    //case OLECMDID_COPY:
    //case OLECMDID_PASTE:
    //case OLECMDID_PASTESPECIAL:
    //case OLECMDID_UNDO:
    //case OLECMDID_REDO:
    //case OLECMDID_SELECTALL:
    //case OLECMDID_CLEARSELECTION:
    case OLECMDID_ZOOM:
    //case OLECMDID_GETZOOMRANGE:
    case OLECMDID_UPDATECOMMANDS:
    case OLECMDID_REFRESH:
    case OLECMDID_STOP:
    case OLECMDID_HIDETOOLBARS:
    //case OLECMDID_SETPROGRESSMAX:
    //case OLECMDID_SETPROGRESSPOS:
    //case OLECMDID_SETPROGRESSTEXT:
    //case OLECMDID_SETTITLE:
    //case OLECMDID_SETDOWNLOADSTATE:
    //case OLECMDID_STOPDOWNLOAD:
    case OLECMDID_ONTOOLBARACTIVATED:
    case OLECMDID_FIND:
    //case OLECMDID_DELETE:
    //case OLECMDID_HTTPEQUIV:
    //case OLECMDID_HTTPEQUIV_DONE:
    //case OLECMDID_ENABLE_INTERACTION:
    //case OLECMDID_ONUNLOAD:
    //case OLECMDID_PROPERTYBAG2:
    case OLECMDID_PREREFRESH:
    case OLECMDID_SHOWSCRIPTERROR:
    case OLECMDID_SHOWMESSAGE:
    case OLECMDID_SHOWFIND:
    case OLECMDID_SHOWPAGESETUP:
    case OLECMDID_SHOWPRINT:
    case OLECMDID_CLOSE:
    case OLECMDID_ALLOWUILESSSAVEAS:
    case OLECMDID_DONTDOWNLOADCSS:
    //case OLECMDID_UPDATEPAGESTATUS:
    case OLECMDID_PRINT2:
    case OLECMDID_PRINTPREVIEW2:
    case OLECMDID_SETPRINTTEMPLATE:
    case OLECMDID_GETPRINTTEMPLATE:
    //case OLECMDID_PAGEACTIONBLOCKED:
    //case OLECMDID_PAGEACTIONUIQUERY:
    //case OLECMDID_FOCUSVIEWCONTROLS:
    //case OLECMDID_FOCUSVIEWCONTROLSQUERY:
    case OLECMDID_SHOWPAGEACTIONMENU:
    case OLECMDID_ADDTRAVELENTRY:
    case OLECMDID_UPDATETRAVELENTRY:
    case OLECMDID_UPDATEBACKFORWARDSTATE:
    case OLECMDID_OPTICAL_ZOOM:
    //case OLECMDID_OPTICAL_GETZOOMRANGE:
    //case OLECMDID_WINDOWSTATECHANGED:
    //case OLECMDID_ACTIVEXINSTALLSCOPE:
    //case OLECMDID_UPDATETRAVELENTRY_DATARECOVERY:
    case 68://OLECMDID_SHOWTASKDLG:
    //case OLECMDID_POPSTATEEVENT:
    //case OLECMDID_VIEWPORT_MODE:
    //case OLECMDID_LAYOUT_VIEWPORT_WIDTH:
    //case OLECMDID_VISUAL_VIEWPORT_EXCLUDE_BOTTOM:
    case 73://OLECMDID_USER_OPTICAL_ZOOM:
    //case OLECMDID_PAGEAVAILABLE:
    //case OLECMDID_GETUSERSCALABLE:
    //case OLECMDID_UPDATE_CARET:
    //case OLECMDID_ENABLE_VISIBILITY:
    //case OLECMDID_MEDIA_PLAYBACK:
    //case OLECMDID_SETFAVICON:
    //case OLECMDID_SET_HOST_FULLSCREENMODE:
    //case OLECMDID_EXITFULLSCREEN:
    //case OLECMDID_SCROLLCOMPLETE:
    //case OLECMDID_ONBEFOREUNLOAD:
        return S_OK;
    default:
        return OLECMDERR_E_NOTSUPPORTED;
    }
}

STDMETHODIMP IoUiSite::QueryStatus(const GUID *pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT *pCmdText) {
    return S_OK;
}

STDMETHODIMP IoUiSite::ShowMessage(HWND hwnd, LPOLESTR lpstrText, LPOLESTR lpstrCaption, DWORD dwType, LPOLESTR lpstrHelpFile, DWORD dwHelpContext, LRESULT *plResult) {
    auto* title = _host->GetTitle();
    *plResult = MessageBox(_host->hwnd, lpstrText, *title, dwType);
    delete title;
    return S_OK;
}

STDMETHODIMP IoUiSite::ShowHelp(HWND hwnd, LPOLESTR pszHelpFile, UINT uCommand, DWORD dwData, POINT ptMouse, IDispatch *pDispatchObjectHit) {
    return E_NOTIMPL;
}

#pragma endregion

#pragma region IoUiFrame

STDMETHODIMP IoUiFrame::QueryInterface(REFIID riid, void ** ppvObject) { return E_NOTIMPL; }
STDMETHODIMP_(ULONG) IoUiFrame::AddRef(void) { return 1; }
STDMETHODIMP_(ULONG) IoUiFrame::Release(void) { return 1; }
STDMETHODIMP IoUiFrame::GetWindow(HWND FAR* lphwnd) { *lphwnd = this->_host->hwnd; return S_OK; }
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

#pragma region BrowserEventHandler

BrowserEventHandler::BrowserEventHandler(IBrowserEventHandler* eventHandler) { _eventHandler = eventHandler; }
BrowserEventHandler::~BrowserEventHandler() {}

STDMETHODIMP BrowserEventHandler::QueryInterface(REFIID riid, void ** ppvObject) {
    if (riid == DIID_DWebBrowserEvents2) {
        *ppvObject = (DWebBrowserEvents2*)this;
    }
    else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

STDMETHODIMP_(ULONG) BrowserEventHandler::AddRef(void) { return 1; }
STDMETHODIMP_(ULONG) BrowserEventHandler::Release(void) { return 1; }
STDMETHODIMP BrowserEventHandler::GetTypeInfoCount(UINT* pctinfo) { *pctinfo = 1; return S_OK; }
STDMETHODIMP BrowserEventHandler::GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo) { return E_NOTIMPL; }
STDMETHODIMP BrowserEventHandler::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid) { return E_NOTIMPL; }

STDMETHODIMP BrowserEventHandler::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags,
        DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr) {
    // https://msdn.microsoft.com/en-us/library/aa768283(v=vs.85).aspx
    switch (dispidMember) {
    case DISPID_DOCUMENTCOMPLETE:
        _eventHandler->DocumentComplete();
        return S_OK;
    case DISPID_DOWNLOADBEGIN:
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_1ST_REQUEST_SENT);
        return S_OK;
    case DISPID_NAVIGATECOMPLETE2:
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_LOAD_INDEX_HTML);
        _eventHandler->NavigateComplete();
        return S_OK;
    case DISPID_TITLECHANGE:
        _eventHandler->TitleChange(pdispparams->rgvarg[0].bstrVal);
        return S_OK;
    case DISPID_NEWWINDOW3:
        *pdispparams->rgvarg[3].pboolVal = VARIANT_TRUE; // cancel = true
        return S_OK;
    default:
        return E_NOTIMPL;
    }
}

#pragma endregion

#pragma region IoUiExternal

IoUiExternal::IoUiExternal() {
    _methods[0] = MethodDesc(L"pm", &IoUiExternal::PostMsg);
};

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
STDMETHODIMP_(ULONG) IoUiExternal::Release(void) { return 1;  }

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
        for (UINT j = 0; j < METHODS_COUNT; j++) {
            if (!wcscmp(_methods[j].Name, rgszNames[i])) {
                rgdispid[i] = j + 1;
                break;
            }
        }
        if (rgdispid[i] == DISPID_UNKNOWN) {
            result = DISP_E_UNKNOWNNAME;
        }
    }
    return S_OK;
}

STDMETHODIMP IoUiExternal::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr) {
    if (dispidMember < 0 || dispidMember > METHODS_COUNT) {
        return E_NOTIMPL;
    }
    ExternalObjectMethod method = _methods[dispidMember - 1].Method;
    return (this->*method)(wFlags, pdispparams, pvarResult, pexcepinfo);
}

HRESULT IoUiExternal::PostMsg(WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo) {
    if ((wFlags & DISPATCH_METHOD) != DISPATCH_METHOD) {
        return E_NOTIMPL;
    }
    Utf8String* msg = new Utf8String(pdispparams->rgvarg[1].bstrVal);
    IDispatch* callback = NULL;
    if (pdispparams->rgvarg[0].vt == VT_DISPATCH) {
        callback = pdispparams->rgvarg[0].pdispVal;
        callback->AddRef();
    }
    _host->EmitEvent(new WindowEventData(WINDOW_EVENT_MESSAGE, (long)msg, (long)callback));
    if (pvarResult) {
        pvarResult->vt = VT_EMPTY;
    }
    return S_OK;
}

#pragma endregion

#pragma region UiWindowWin

WCHAR UiWindowWin::_wndClassName[] = L"IoUiWnd";
DWORD UiWindowWin::_mainThreadId = 0;
int UiWindowWin::_dpi = BASE_DPI;

int UiWindow::Main(int argc, char* argv[]) {
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        //std::cerr << "MSG " << std::hex << std::setw(8) << msg.message << std::setw(12) << msg.lParam << std::setw(12) << msg.wParam << std::setw(12) << msg.hwnd << std::endl;
        if (msg.message == WM_USER_CREATE_WINDOW) {
            UiWindowWin* window = (UiWindowWin*)msg.wParam;
            window->ShowOsWindow();
            continue;
        }
        if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST || msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST) {
            HWND rootHwnd = GetAncestor(msg.hwnd, GA_ROOT);
            UiWindowWin* _this = (UiWindowWin*)GetWindowLong(rootHwnd, GWL_USERDATA);
            if (_this && _this->HandleAccelerator(&msg))
                continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

UiWindow* UiWindow::CreateUiWindow() {
    return new UiWindowWin();
}

void UiWindowWin::Show(WindowRect& rect) {
    _rect = rect;
    PostThreadMessage(_mainThreadId, WM_USER_CREATE_WINDOW, (WPARAM)this, 0);
}

UI_RESULT UiWindow::OsInitialize() {
    HINSTANCE hAppInstance = GetModuleHandle(NULL);
    WNDCLASSEX wc =
    {
        sizeof(wc),
        0,
        UiWindowWin::WindowProc,
        0, 0,
        hAppInstance,
        LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        NULL,
        UiWindowWin::_wndClassName,
        LoadIcon(NULL, IDI_APPLICATION)
    };
    RegisterClassEx(&wc);
    UiWindowWin::_mainThreadId = GetCurrentThreadId();

    HDC hdc = GetDC(NULL);
    if (hdc) {
        UiWindowWin::_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    }
    return UI_S_OK;
}

UiWindowWin::UiWindowWin() {
    _site._host = this;
    _frame._host = this;
    _external._host = this;
}

int UiWindow::Alert(Utf8String* msg, ALERT_TYPE type) {
    MessageBox(HWND_DESKTOP, *msg, L"Error",
        (type == ALERT_TYPE::ALERT_ERROR ? MB_ICONERROR : MB_ICONINFORMATION)
        | MB_OK);
    return 0;
}

LRESULT CALLBACK UiWindowWin::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    UiWindowWin* _this;
    if (uMsg == WM_CREATE && (_this = (UiWindowWin*)(LPCREATESTRUCT(lParam))->lpCreateParams)) {
        SetWindowLong(hwnd, GWL_USERDATA, (long)_this);
        _this->hwnd = hwnd;
    }
    else {
        _this = (UiWindowWin*)GetWindowLong(hwnd, GWL_USERDATA);
    }

    BOOL fDoDef = !(_this && _this->HandleMessage(uMsg, wParam, lParam));

    return fDoDef ? DefWindowProc(hwnd, uMsg, wParam, lParam) : 0;
}

BOOL UiWindowWin::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        EmitEvent(new WindowEventData(WINDOW_EVENT_CLOSED));
        DestroyEmbeddedWebControl();
        SetWindowLong(hwnd, GWL_USERDATA, (long)NULL);
        if (_isMainWindow)
            PostQuitMessage(0);
        if (_parent)
            EnableWindow(((UiWindowWin*)_parent)->hwnd, TRUE);
        return TRUE;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return true;
    case WM_CREATE:
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_CREATE_WINDOW);
        CreateEmbeddedWebControl();
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_INIT_BROWSER_CONTROL);
        return TRUE;
    case  WM_ENTERSIZEMOVE:
        _resizeMove = true;
        return TRUE;
    case  WM_EXITSIZEMOVE:
        _resizeMove = false;
        return TRUE;
    case WM_SIZE: {
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        _webBrowser->put_Width(x);
        _webBrowser->put_Height(y);
        if (_resizeMove) {
            EmitEvent(new WindowEventData(WINDOW_EVENT_RESIZE, x, y));
        }
        else {
            int state = 0;
            EmitEvent(new WindowEventData(WINDOW_EVENT_STATE, state));
        }
        return TRUE;
    }
    case WM_MOVE: {
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        EmitEvent(new WindowEventData(WINDOW_EVENT_MOVE, x, y));
        return TRUE;
    }
    case WM_MENUCOMMAND: {
        int ix = wParam;
        HMENU hMenu = (HMENU)lParam;
        MENUITEMINFO mii = { 0 };
        mii.fMask = MIIM_DATA;
        mii.cbSize = sizeof(mii);
        if (GetMenuItemInfo(hMenu, ix, TRUE, &mii) && mii.dwItemData) {
            UiMenu* uiMenu = (UiMenu*)mii.dwItemData;
            EmitEvent(new WindowEventData(WINDOW_EVENT_MENU, mii.dwItemData));
            if (uiMenu && uiMenu->InternalMenuId > 0) {
                HandleInternalMenu(uiMenu->InternalMenuId);
            }
            return TRUE;
        }
        return FALSE;
    }
    case WM_USER_NAVIGATE: {
        Utf8String* url = (Utf8String*)wParam;

        VARIANT vURL;
        vURL.vt = VT_BSTR;
        vURL.bstrVal = SysAllocString(*url);
        VARIANT ve1, ve2, ve3, ve4;
        ve1.vt = VT_EMPTY;
        ve2.vt = VT_EMPTY;
        ve3.vt = VT_EMPTY;
        ve4.vt = VT_EMPTY;

        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_CALL_NAVIGATE);
        _webBrowser->Navigate2(&vURL, &ve1, &ve2, &ve3, &ve4);
        VariantClear(&vURL);
        delete url;
        return TRUE;
    }
    case WM_USER_POST_MSG: {
        Utf8String* msg = (Utf8String*)wParam;
        WCHAR* script = new WCHAR[wcslen(*msg) + 128];
        wcscpy(script, L"JSON.stringify(typeof backend.onMessage === 'function' ? backend.onMessage(");
        wcscat(script, *msg);
        wcscat(script, L"):null)");
        delete msg;
        VARIANT ret = { 0 };
        VariantInit(&ret);
        EXCEPINFO ex = { 0 };
        if (UI_SUCCEEDED(ExecScript(script, &ret, &ex)) && lParam) {
            Utf8String* result = ret.vt == VT_BSTR && ret.bstrVal != NULL ? new Utf8String(ret.bstrVal) : NULL;
            Utf8String* error = result == NULL && ex.bstrDescription != NULL ? new Utf8String(ex.bstrDescription) : NULL;
            VariantClear(&ret);
            EmitEvent(new WindowEventData(WINDOW_EVENT_MESSAGE_CALLBACK, (long)lParam, (long)result, (long)error));
        }
        delete script;
        return TRUE;
    }
    case WM_USER_POST_MSG_CB:
        MsgCallbackParam* cb = (MsgCallbackParam*)wParam;
        if (cb->Callback) {
            IDispatch* dispCallback = (IDispatch*)cb->Callback;
            DISPPARAMS params = { 0 };
            params.cArgs = 2;
            params.rgvarg = new VARIANTARG[2];
            VariantInit(&params.rgvarg[0]);
            VariantInit(&params.rgvarg[1]);
            if (cb->Result) {
                params.rgvarg[1].vt = VT_BSTR;
                params.rgvarg[1].bstrVal = SysAllocString(*cb->Result);
            }
            else {
                params.rgvarg[1].vt = VT_NULL;
            }
            if (cb->Error) {
                params.rgvarg[0].vt = VT_BSTR;
                params.rgvarg[0].bstrVal = SysAllocString(*cb->Error);
            }
            else {
                params.rgvarg[0].vt = VT_NULL;
            }
            HRESULT hr = dispCallback->Invoke(DISPID_VALUE, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, NULL, NULL, NULL);
            dispCallback->Release();
        }
        if (cb->Result)
            delete cb->Result;
        if (cb->Error)
            delete cb->Error;
        delete cb;
        return TRUE;
    }
    return FALSE;
}

bool UiWindowWin::HandleAccelerator(MSG* msg) {
    if (!_webBrowser)
        return false;
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
        case 0:
            // TODO: dispatch synthetic events
            if (GetKeyState(VK_CONTROL) & 0x8000)
                return true;
        }
    }
    IOleInPlaceActiveObject* pIOIPAO = NULL;
    HRESULT hr = _webBrowser->QueryInterface(IID_IOleInPlaceActiveObject, (void**)&pIOIPAO);
    if (SUCCEEDED(hr)) {
        hr = pIOIPAO->TranslateAccelerator(msg);
        pIOIPAO->Release();
    }
    return hr == S_OK;
}

void UiWindowWin::ShowOsWindow() {
    DWORD style = WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
    if (!_config->Resizable) style &= ~(WS_MAXIMIZEBOX | WS_SIZEBOX);
    if (!_config->Frame) style &= ~(WS_CAPTION);

    HWND parentHwnd = _parent ? ((UiWindowWin*)_parent)->hwnd : HWND_DESKTOP;
    HINSTANCE hinst = GetModuleHandle(NULL);
    CreateWindowMenu();
    hwnd = CreateWindowEx(0, _wndClassName, *_config->Title,
        style,
        this->_rect.Left * _dpi / BASE_DPI, this->_rect.Top * _dpi / BASE_DPI,
        this->_rect.Width * _dpi / BASE_DPI, this->_rect.Height * _dpi / BASE_DPI,
        parentHwnd, _hmenu, hinst, this);
    if (_parent)
        EnableWindow(parentHwnd, FALSE);

    UpdateWindow(hwnd);
    SetState(_config->State);
    if (_config->Opacity < 1)
        SetOpacity(_config->Opacity);
    if (_config->Topmost)
        SetTopmost(_config->Topmost);
    if (!_config->Frame)
        SetFrame(_config->Frame);
    EmitEvent(new WindowEventData(WINDOW_EVENT_READY));
}

void UiWindowWin::CreateEmbeddedWebControl(void) {
    HRESULT hresult;
    OleCreate(CLSID_WebBrowser, IID_IOleObject, OLERENDER_DRAW, 0, &_site, &_storage, (void**)&_oleWebObject);

    _oleWebObject->SetHostNames(L"io-ui", L"webview");

    OleSetContainedObject(_oleWebObject, TRUE);

    RECT rect;
    GetClientRect(hwnd, &rect);

    _oleWebObject->DoVerb(OLEIVERB_SHOW, NULL, &_site, -1, hwnd, &rect);

    _oleWebObject->QueryInterface(IID_IWebBrowser2, (void**)&_webBrowser);

    _webBrowser->put_Left(0);
    _webBrowser->put_Top(0);
    _webBrowser->put_Width(rect.right);
    _webBrowser->put_Height(rect.bottom);
    _webBrowser->put_Silent(VARIANT_TRUE);

    IConnectionPointContainer* pConnectionPointContainer;
    hresult = _oleWebObject->QueryInterface(IID_IConnectionPointContainer, (void**)&pConnectionPointContainer);
    if (SUCCEEDED(hresult)) {
        IConnectionPoint* pConnPoint;
        hresult = pConnectionPointContainer->FindConnectionPoint(DIID_DWebBrowserEvents2, &pConnPoint);
        if (SUCCEEDED(hresult)) {
            _browserEventHandler = new BrowserEventHandler(this);
            DWORD cookie;
            hresult = pConnPoint->Advise(_browserEventHandler, &cookie);
            pConnPoint->Release();
        }
        pConnectionPointContainer->Release();
    }
}


void UiWindowWin::DestroyEmbeddedWebControl(void) {
    _oleWebObject->Close(OLECLOSE_NOSAVE);
    _oleWebObject->Release();
    _webBrowser->Release();
}

void UiWindowWin::Close(void) {
    SendMessage(this->hwnd, WM_CLOSE, 0, 0);
}

void UiWindowWin::Navigate(Utf8String* url) {
    if (!wcslen(*url))
        return;
    PostMessage(hwnd, WM_USER_NAVIGATE, (WPARAM)url, 0);
}

void UiWindowWin::PostMsg(Utf8String* msg, v8::Persistent<v8::Function>* callback) {
    PostMessage(hwnd, WM_USER_POST_MSG, (WPARAM)msg, (LPARAM)callback);
}

void UiWindowWin::MsgCallback(void* callback, Utf8String* result, Utf8String* error) {
    auto cb = new MsgCallbackParam();
    cb->Callback = callback;
    cb->Result = result;
    cb->Error = error;
    PostMessage(hwnd, WM_USER_POST_MSG_CB, (WPARAM)cb, 0);
}

void UiWindowWin::DocumentComplete() {
    EmitEvent(new WindowEventData(WINDOW_EVENT_DOCUMENT_COMPLETE));
}

void UiWindowWin::NavigateComplete() {
    RECT rect;
    GetClientRect(hwnd, &rect);
    _oleWebObject->DoVerb(OLEIVERB_UIACTIVATE, NULL, &_site, -1, hwnd, &rect);

    ExecScript(L"window.backend = {"
        L"postMessage: function(data, cb) { external.pm(JSON.stringify(data), cb ? function(res, err) { if (typeof cb === 'function') cb(JSON.parse(res), err); } : null); },"
        L"onMessage: null"
        L"};");
    if (atoi(UiModule::GetEngineVersion()) <= 7) {
        int len = MultiByteToWideChar(CP_ACP, 0, IE_JSON_POLYFILL_JS_CODE, -1, NULL, 0);
        WCHAR* jsonPolyfill = new WCHAR[len];
        MultiByteToWideChar(CP_ACP, 0, IE_JSON_POLYFILL_JS_CODE, -1, jsonPolyfill, len);
        ExecScript(jsonPolyfill);
        delete jsonPolyfill;
    }
}

void UiWindowWin::TitleChange(BSTR title) {
    SetWindowText(hwnd, title);
}

UI_RESULT UiWindowWin::ExecScript(WCHAR* code, LPVARIANT ret, LPEXCEPINFO ex) {
    IDispatch* doc;
    IDispatch* script = NULL;
    HRESULT hr = _webBrowser->get_Document(&doc);
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
    if (!script) {
        return UI_E_FAIL;
    }
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
        return UI_E_FAIL;
    }
    VARIANTARG args[1] = {};
    DISPPARAMS params = { 0 };
    params.cArgs = 1;
    params.rgvarg = args;
    VariantInit(&args[0]);
    args[0].vt = VT_BSTR;
    args[0].bstrVal = SysAllocString(code);
    hr = script->Invoke(evalDispId, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, ret, ex, NULL);
    SysFreeString(args[0].bstrVal);
    script->Release();
    return SUCCEEDED(hr) ? UI_S_OK : UI_E_FAIL;
}

void UiWindowWin::CreateWindowMenu() {
    if (_isMainWindow && _config->Menu) {
        _hmenu = CreateMenu();
        MENUINFO mi = { 0 };
        mi.cbSize = sizeof(mi);
        mi.fMask = MIM_STYLE;
        mi.dwStyle = MNS_NOTIFYBYPOS;
        SetMenuInfo(_hmenu, &mi);

        AddMenu(_config->Menu, _hmenu);
    }
}

void UiWindowWin::AddMenu(UiMenu* menu, HMENU parent) {
    bool isFileMenu = !menu->Parent && menu->Title && (!lstrcmpi(*menu->Title, L"File") || !lstrcmpi(*menu->Title, L"&File"));
    bool isEditMenu = !menu->Parent && menu->Title && (!lstrcmpi(*menu->Title, L"Edit") || !lstrcmpi(*menu->Title, L"&Edit"));
    bool isHelpMenu = !menu->Parent && menu->Title && (!lstrcmpi(*menu->Title, L"Help") || !lstrcmpi(*menu->Title, L"&Help")
        || !lstrcmpi(*menu->Title, L"?") || !lstrcmpi(*menu->Title, L"&?"));
    bool isStandardMenu = isFileMenu || isEditMenu || isHelpMenu;
    if (menu->Type == MENU_TYPE::MENU_TYPE_SEPARATOR) {
        AppendMenu(parent, MF_SEPARATOR, NULL, NULL);
    }
    else if (isStandardMenu || ((menu->Type == MENU_TYPE::MENU_TYPE_SIMPLE) && menu->Items)) {
        HMENU popupMenu = CreatePopupMenu();
        AppendMenu(parent, MF_STRING | MF_POPUP, (UINT_PTR)popupMenu, *menu->Title);
        MENUINFO mi = { 0 };
        mi.cbSize = sizeof(mi);
        mi.fMask = MIM_STYLE;
        mi.dwStyle = MNS_NOTIFYBYPOS;
        SetMenuInfo(popupMenu, &mi);
        if (isEditMenu) {
            AddEditMenuItems(popupMenu, menu->Items != NULL);
        }
        if (menu->Items) {
            AddMenu(menu->Items, popupMenu);
        }
        if (isHelpMenu) {
            AddHelpMenuItems(popupMenu, menu->Items != NULL);
        }
        if (isFileMenu) {
            AddFileMenuItems(popupMenu, menu->Items != NULL);
        }
    }
    else {
        MENUITEMINFO info = { 0 };
        info.cbSize = sizeof(info);
        info.fMask = MIIM_DATA | MIIM_STRING;
        info.cch = wcslen(*menu->Title);
        info.dwTypeData = *menu->Title;
        info.dwItemData = (ULONG_PTR)menu;
        InsertMenuItem(parent, -1, TRUE, &info);
    }
    if (menu->Next) {
        AddMenu(menu->Next, parent);
    }
}

void UiWindowWin::AddFileMenuItems(HMENU parent, bool hasItems) {
    if (hasItems)
        AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String(L"E&xit"), MENU_INTL_FILE_EXIT), parent);
}

void UiWindowWin::AddEditMenuItems(HMENU parent, bool hasItems) {
    AddMenu(new UiMenu(new Utf8String(L"&Undo"), MENU_INTL_EDIT_UNDO), parent);
    AddMenu(new UiMenu(new Utf8String(L"&Redo"), MENU_INTL_EDIT_REDO), parent);
    AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String(L"Cu&t"), MENU_INTL_EDIT_CUT), parent);
    AddMenu(new UiMenu(new Utf8String(L"&Copy"), MENU_INTL_EDIT_COPY), parent);
    AddMenu(new UiMenu(new Utf8String(L"&Paste"), MENU_INTL_EDIT_PASTE), parent);
    AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String(L"Select &All"), MENU_INTL_EDIT_SELECT_ALL), parent);
    if (hasItems)
        AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
}

void UiWindowWin::AddHelpMenuItems(HMENU parent, bool hasItems) {
    if (hasItems)
        AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String(L"&About"), MENU_INTL_HELP_ABOUT), parent);
}

void UiWindowWin::HandleInternalMenu(MENU_INTL menu) {
    switch (menu) {
    case MENU_INTL_FILE_EXIT:
        Close();
        break;
    case MENU_INTL_EDIT_UNDO:
        _webBrowser->ExecWB(OLECMDID_UNDO, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
        break;
    case MENU_INTL_EDIT_REDO:
        _webBrowser->ExecWB(OLECMDID_REDO, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
        break;
    case MENU_INTL_EDIT_CUT:
        _webBrowser->ExecWB(OLECMDID_CUT, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
        break;
    case MENU_INTL_EDIT_COPY:
        _webBrowser->ExecWB(OLECMDID_COPY, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
        break;
    case MENU_INTL_EDIT_PASTE:
        _webBrowser->ExecWB(OLECMDID_PASTE, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
        break;
    case MENU_INTL_EDIT_SELECT_ALL:
        _webBrowser->ExecWB(OLECMDID_SELECTALL, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
        break;
    case MENU_INTL_HELP_ABOUT:
        break;
    }
}

WindowRect UiWindowWin::GetWindowRect() {
    RECT rect;
    ::GetWindowRect(hwnd, &rect);
    return WindowRect(rect.left * BASE_DPI / _dpi, rect.top * BASE_DPI / _dpi,
        (rect.right - rect.left) * BASE_DPI / _dpi, (rect.bottom - rect.top) * BASE_DPI / _dpi);
}

WindowRect UiWindowWin::GetScreenRect() {
    RECT rect;
    HWND hDesktop = GetDesktopWindow();
    ::GetWindowRect(hDesktop, &rect);
    return WindowRect(rect.left * BASE_DPI / _dpi, rect.top * BASE_DPI / _dpi,
        (rect.right - rect.left) * BASE_DPI / _dpi, (rect.bottom - rect.top) * BASE_DPI / _dpi);
}

void UiWindowWin::SetWindowRect(WindowRect& rect) {
    MoveWindow(hwnd, rect.Left * _dpi / BASE_DPI, rect.Top * _dpi / BASE_DPI,
        rect.Width * _dpi / BASE_DPI, rect.Height * _dpi / BASE_DPI, true);
}

Utf8String* UiWindowWin::GetTitle() {
    const int MAX_LEN = 2048;
    WCHAR* title = new WCHAR[MAX_LEN];
    int res = GetWindowText(hwnd, title, MAX_LEN);
    if (!res) {
        delete title;
        return NULL;
    }
    return new Utf8String(title);
}

WINDOW_STATE UiWindowWin::GetState() {
    if (!IsWindowVisible(hwnd))
        return WINDOW_STATE::WINDOW_STATE_HIDDEN;
    if (IsIconic(hwnd))
        return WINDOW_STATE::WINDOW_STATE_MINIMIZED;
    if (IsZoomed(hwnd))
        return WINDOW_STATE::WINDOW_STATE_MAXIMIZED;
    // TODO return WINDOW_STATE::WINDOW_STATE_FULLSCREEN;
    return WINDOW_STATE::WINDOW_STATE_NORMAL;
}

void UiWindowWin::SetState(WINDOW_STATE state) {
    switch (state) {
    case WINDOW_STATE::WINDOW_STATE_NORMAL:
        ShowWindow(hwnd, SW_SHOWNORMAL);
        break;
    case WINDOW_STATE::WINDOW_STATE_HIDDEN:
        ShowWindow(hwnd, SW_HIDE);
        break;
    case WINDOW_STATE::WINDOW_STATE_MAXIMIZED:
        ShowWindow(hwnd, SW_SHOWMAXIMIZED);
        break;
    case WINDOW_STATE::WINDOW_STATE_MINIMIZED:
        ShowWindow(hwnd, SW_SHOWMINIMIZED);
        break;
    }
}

bool UiWindowWin::GetResizable() {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    return (dwStyle & WS_SIZEBOX) == WS_SIZEBOX;
}

void UiWindowWin::SetResizable(bool resizable) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (resizable) {
        dwStyle |= (WS_MAXIMIZEBOX | WS_SIZEBOX);
    }
    else {
        dwStyle &= ~(WS_MAXIMIZEBOX | WS_SIZEBOX);
    }
    SetWindowLong(hwnd, GWL_STYLE, dwStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

bool UiWindowWin::GetFrame() {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    return (dwStyle & WS_CAPTION) == WS_CAPTION;
}

void UiWindowWin::SetFrame(bool frame) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (frame) {
        dwStyle |= (WS_CAPTION);
    }
    else {
        dwStyle &= ~(WS_CAPTION);
    }
    SetWindowLong(hwnd, GWL_STYLE, dwStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

bool UiWindowWin::GetTopmost() {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    return (dwStyle & WS_EX_TOPMOST) == WS_EX_TOPMOST;
}

void UiWindowWin::SetTopmost(bool topmost) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (topmost) {
        dwStyle |= (WS_EX_TOPMOST);
    }
    else {
        dwStyle &= ~(WS_EX_TOPMOST);
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, dwStyle);
    SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

double UiWindowWin::GetOpacity() {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if ((dwStyle & WS_EX_LAYERED) == 0) {
        return 1;
    }
    BYTE alpha;
    GetLayeredWindowAttributes(hwnd, NULL, &alpha, NULL);
    return alpha / 255.;
}

void UiWindowWin::SetOpacity(double opacity) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    bool opaque = opacity == 1;
    if (opaque) {
        dwStyle &= ~(WS_EX_LAYERED);
    }
    else {
        dwStyle |= (WS_EX_LAYERED);
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, dwStyle);
    SetLayeredWindowAttributes(hwnd, NULL, (BYTE)(opacity * 255), LWA_ALPHA);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

#pragma endregion

#pragma endregion
