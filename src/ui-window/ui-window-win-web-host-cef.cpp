#include "ui-window-win-web-host.h"
#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_browser.h>
#include <include/cef_client.h>
#include <include/cef_command_line.h>
#include <include/cef_frame.h>
#include <include/cef_runnable.h>
#include <include/cef_web_plugin.h>
#include <include/cef_sandbox_win.h>
#include <include/wrapper/cef_closure_task.h>
#include <include/wrapper/cef_helpers.h>
#include <iostream>

#pragma region Interface

class UiWindowWebHostCef;

class IoUiCefApp : public CefApp, public CefBrowserProcessHandler, public CefRenderProcessHandler {
public:
    IoUiCefApp(UiWindowWebHostCef* host);
    bool Initialized;

    // CefApp
    virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
    virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;

    // CefBrowserProcessHandler
    virtual void OnContextInitialized() override;

    // CefRenderProcessHandler
    virtual void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;
    virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

private:
    UiWindowWebHostCef* _host;
    IMPLEMENT_REFCOUNTING(IoUiCefApp);
};

class IoUiBackendObjectPostMessageFn : public CefV8Handler {
public:
    IoUiBackendObjectPostMessageFn(CefBrowser* browser);
    ~IoUiBackendObjectPostMessageFn();
    virtual bool Execute(const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments,
        CefRefPtr<CefV8Value>& retval, CefString& exception) override;
private:
    CefBrowser* _browser;
    IMPLEMENT_REFCOUNTING(IoUiBackendObjectPostMessageFn);
};

class IoUiCefHandler : public CefClient, public CefDisplayHandler, public CefLifeSpanHandler, public CefLoadHandler,
    public CefRequestHandler, public CefJSDialogHandler {
public:
    IoUiCefHandler(UiWindowWebHostCef* host);

    // CefClient
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
    virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override;
    virtual CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override;
    virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

    // CefDisplayHandler
    virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

    // CefLifeSpanHandler
    virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefLoadHandler
    virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward) override;
    virtual void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) override;
    virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode);
    virtual void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) override;

    // CefRequestHandler

    // CefJSDialogHandler

private:
    UiWindowWebHostCef* _host;
    IMPLEMENT_REFCOUNTING(IoUiCefHandler);
};

class UiWindowWebHostCef : public IUiWindowWebHost {
public:
    UiWindowWebHostCef(IUiWindow* window);

    void CreateWebView();

    static void InitializeApp(UiWindowWebHostCef* mainWindowHost);

    static IoUiCefApp* App;

    IUiWindow* Window = NULL;
    IoUiCefHandler* CefHandler = NULL;
    CefBrowser* Browser = NULL;

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
};

IoUiCefApp* UiWindowWebHostCef::App = NULL;

IUiWindowWebHost* CreateCefWebHost(IUiWindow* window);

#pragma endregion

#pragma region IoUiCefHandler

IoUiCefHandler::IoUiCefHandler(UiWindowWebHostCef* host) : _host(host) {
}

CefRefPtr<CefDisplayHandler> IoUiCefHandler::GetDisplayHandler() { return this; }
CefRefPtr<CefLifeSpanHandler> IoUiCefHandler::GetLifeSpanHandler() { return this; }
CefRefPtr<CefLoadHandler> IoUiCefHandler::GetLoadHandler() { return this; }
CefRefPtr<CefRequestHandler> IoUiCefHandler::GetRequestHandler() { return this; }
CefRefPtr<CefJSDialogHandler> IoUiCefHandler::GetJSDialogHandler() { return this; }

bool IoUiCefHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) {
    if (message->GetName() == "pm") {
        auto args = message->GetArgumentList();
        auto msgStr = new WCHAR[args->GetString(0).length() + 1];
        lstrcpy(msgStr, args->GetString(0).c_str());
        auto callback = args->GetInt(1);
        _host->Window->PostMessageToBackend(msgStr, (void*)callback);
        delete[] msgStr;
        return true;
    } else if (message->GetName() == "pmc") {
        auto args = message->GetArgumentList();
        auto callback = args->GetInt(0);
        LPWSTR result = NULL;
        LPWSTR error = NULL;
        if (args->GetType(1) == CefValueType::VTYPE_STRING) {
            result = new WCHAR[args->GetString(1).length() + 1];
            lstrcpy(result, args->GetString(1).c_str());
        } else {
            error = new WCHAR[args->GetString(2).length() + 1];
            lstrcpy(error, args->GetString(2).c_str());
        }
        _host->Window->HandlePostMessageCallback((void*)callback, result, error);
        if (result) delete[] result;
        if (error) delete[] error;
        return true;
    }
    return false;
}

void IoUiCefHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    SetWindowText(*_host->Window, title.c_str());
}

bool IoUiCefHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    PostMessage(*_host->Window, WM_CLOSE, 0, 0);
    return true;
}

void IoUiCefHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
}

void IoUiCefHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
}

void IoUiCefHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward) {

}

void IoUiCefHandler::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
    if (frame->IsMain()) {
        _host->Window->DownloadBegin();
    }
}

void IoUiCefHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
    if (frame->IsMain()) {
        _host->Window->DownloadComplete();
        _host->Window->DocumentComplete();
    }
}

void IoUiCefHandler::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) {
    // TODO
}

#pragma endregion

#pragma region IoUiCefApp

IoUiCefApp::IoUiCefApp(UiWindowWebHostCef* host) : _host(host) {
}

CefRefPtr<CefBrowserProcessHandler> IoUiCefApp::GetBrowserProcessHandler() { return this; }
CefRefPtr<CefRenderProcessHandler> IoUiCefApp::GetRenderProcessHandler() { return this; }

void IoUiCefApp::OnContextInitialized() {
    Initialized = true;
    if (_host) {
        _host->CreateWebView();
    }
}

void IoUiCefApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
    frame->ExecuteJavaScript("if (!window.backend) window.backend = {"
        "postMessage: function(data, cb) {"
        "if (typeof cb === 'function') window.backend['_cb' + (++window.backend._lastCb)] = cb;"
        "window.backend._pm(JSON.stringify(data), cb ? window.backend._lastCb : 0);"
        "if (window.backend._lastCb >= 10000000) window.backend._lastCb = 0;"
        "},"
        "onMessage: null,"
        "_lastCb:0"
        "};", frame->GetURL(), 0);
    auto window = context->GetGlobal();
    auto backend = window->GetValue("backend");
    auto pmFn = window->CreateFunction("_pm", new IoUiBackendObjectPostMessageFn(browser));
    backend->SetValue("_pm", pmFn, CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_DONTDELETE);
}

bool IoUiCefApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) {
    if (message->GetName() == "pm") {
        auto args = message->GetArgumentList();
        auto callback = args->GetInt(1);
        auto script = new WCHAR[args->GetString(0).length() + 128];
        lstrcpy(script, L"JSON.stringify(typeof window.backend.onMessage === 'function' ? window.backend.onMessage(");
        lstrcat(script, args->GetString(0).c_str());
        lstrcat(script, L") : null)");
        auto ctx = browser->GetMainFrame()->GetV8Context();
        CefRefPtr<CefV8Value> ret;
        CefRefPtr<CefV8Exception> ex;
        bool success = ctx->Eval(script, ret, ex);
        delete[] script;
        auto msg = CefProcessMessage::Create("pmc");
        args = msg->GetArgumentList();
        args->SetInt(0, callback);
        if (success) args->SetString(1, ret->GetStringValue()); else args->SetNull(1);
        if (success) args->SetNull(2); else args->SetString(2, ex->GetMessageW());
        browser->SendProcessMessage(CefProcessId::PID_BROWSER, msg);
        return true;
    } else if (message->GetName() == "pmc") {
        auto args = message->GetArgumentList();
        auto callback = args->GetInt(0);
        auto len = args->GetType(1) == CefValueType::VTYPE_STRING ? args->GetString(1).length() : args->GetString(2).length();
        auto script = new WCHAR[len + 256];
        lstrcpy(script, L"if (typeof window.backend['_cb");
        _itow(callback, &script[lstrlen(script)], 10);
        lstrcat(script, L"'] === 'function') window.backend['_cb");
        _itow(callback, &script[lstrlen(script)], 10);
        lstrcat(script, L"'](");
        lstrcat(script, args->GetType(1) == CefValueType::VTYPE_STRING ? args->GetString(1).c_str() : L"null");
        lstrcat(script, L",");
        lstrcat(script, args->GetType(2) == CefValueType::VTYPE_STRING ? args->GetString(2).c_str() : L"null");
        lstrcat(script, L");delete window.backend['_cb");
        _itow(callback, &script[lstrlen(script)], 10);
        lstrcat(script, L"'];");
        CefRefPtr<CefV8Value> ret;
        CefRefPtr<CefV8Exception> ex;
        browser->GetMainFrame()->GetV8Context()->Eval(script, ret, ex);
        delete[] script;
        return true;
    }
    return false;
}

#pragma endregion

#pragma region IoUiBackendObjectPostMessageFn

IoUiBackendObjectPostMessageFn::IoUiBackendObjectPostMessageFn(CefBrowser* browser) : _browser(browser) {
    browser->AddRef();
}

IoUiBackendObjectPostMessageFn::~IoUiBackendObjectPostMessageFn() {
    _browser->Release();
}

bool IoUiBackendObjectPostMessageFn::Execute(const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments,
    CefRefPtr<CefV8Value>& retval, CefString& exception) {
    retval = CefV8Value::CreateUndefined();
    auto msg = CefProcessMessage::Create("pm");
    msg->GetArgumentList()->SetString(0, arguments[0]->GetStringValue());
    msg->GetArgumentList()->SetInt(1, arguments[1]->GetIntValue());
    _browser->SendProcessMessage(CefProcessId::PID_BROWSER, msg);
    return true;
}

#pragma endregion

#pragma region UiWindowWebHostCef

IUiWindowWebHost* CreateCefWebHost(IUiWindow* window) {
    auto host = new UiWindowWebHostCef(window);
    if (!UiWindowWebHostCef::App) {
        UiWindowWebHostCef::InitializeApp(host);
    }
    return host;
}

void UiWindowWebHostCef::InitializeApp(UiWindowWebHostCef* mainWindowHost) {
    UiWindowWebHostCef::App = new IoUiCefApp(mainWindowHost);
    CefRefPtr<IoUiCefApp> app(UiWindowWebHostCef::App);
    CefSettings appSettings;
    appSettings.single_process = true;
    appSettings.no_sandbox = true;
    appSettings.log_severity = LOGSEVERITY_DISABLE;
    CefMainArgs mainArgs(GetModuleHandle(NULL));
    CefInitialize(mainArgs, appSettings, app.get(), NULL);
}

UiWindowWebHostCef::UiWindowWebHostCef(IUiWindow* window) : Window(window) {
}

void UiWindowWebHostCef::CreateWebView() {
    if (CefHandler) {
        return;
    }
    CefHandler = new IoUiCefHandler(this);
    CefHandler->AddRef();
    RECT rect;
    GetClientRect(*Window, &rect);
    CefWindowInfo windowInfo;
    windowInfo.SetAsChild(*Window, rect);
    CefRefPtr<IoUiCefHandler> handler(CefHandler);
    CefBrowserSettings browserSettings;
    browserSettings.java = STATE_DISABLED;
    browserSettings.plugins = STATE_DISABLED;
    browserSettings.web_security = STATE_DISABLED;
    auto browser = CefBrowserHost::CreateBrowserSync(windowInfo, handler.get(), "", browserSettings, NULL);
    Browser = browser.get();
    Browser->AddRef();
}

void UiWindowWebHostCef::Initialize() {
    if (UiWindowWebHostCef::App->Initialized) {
        CreateWebView();
    }
}

void UiWindowWebHostCef::Destroy() {
    if (Browser) {
        Browser->StopLoad();
        Browser->GetHost()->CloseBrowser(true);
        Browser->Release();
    }
    if (CefHandler) {
        CefHandler->Release();
    }
}

void UiWindowWebHostCef::SetSize(int width, int height) {
    if (!IsIconic(*Window)) {
        RECT rect;
        GetClientRect(*Window, &rect);
        auto browserHwnd = Browser->GetHost()->GetWindowHandle();
        SetWindowPos(browserHwnd, NULL,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOZORDER);
        Browser->GetHost()->WasResized();
        Browser->GetHost()->Invalidate(CefBrowserHost::PaintElementType::PET_VIEW);
        Browser->GetHost()->NotifyMoveOrResizeStarted();
        RedrawWindow(browserHwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
    }
}

void UiWindowWebHostCef::Navigate(LPCWSTR url) {
    Browser->GetMainFrame()->LoadURL(url);
}

bool UiWindowWebHostCef::HandleAccelerator(MSG* msg) {
    return false;
}

void UiWindowWebHostCef::PostMessageToBrowser(LPCWSTR json, void* callback) {
    auto msg = CefProcessMessage::Create("pm");
    auto args = msg->GetArgumentList();
    args->SetString(0, json);
    args->SetInt(1, (int)callback);
    Browser->SendProcessMessage(CefProcessId::PID_RENDERER, msg);
}

void UiWindowWebHostCef::HandlePostMessageCallback(void* callback, LPCWSTR result, LPCWSTR error) {
    auto msg = CefProcessMessage::Create("pmc");
    auto args = msg->GetArgumentList();
    args->SetInt(0, (int)callback);
    if (result) args->SetString(1, result); else args->SetNull(1);
    if (error) args->SetString(2, error); else args->SetNull(2);
    Browser->SendProcessMessage(CefProcessId::PID_RENDERER, msg);
}

void UiWindowWebHostCef::Undo() {
    Browser->GetMainFrame()->Undo();
}

void UiWindowWebHostCef::Redo() {
    Browser->GetMainFrame()->Redo();
}

void UiWindowWebHostCef::Cut() {
    Browser->GetMainFrame()->Cut();
}

void UiWindowWebHostCef::Copy() {
    Browser->GetMainFrame()->Copy();
}

void UiWindowWebHostCef::Paste() {
    Browser->GetMainFrame()->Paste();
}

void UiWindowWebHostCef::SelectAll() {
    Browser->GetMainFrame()->SelectAll();
}

#pragma endregion
