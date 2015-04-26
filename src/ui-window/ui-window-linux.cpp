#include "ui-window.h"
#include "../perf-trace/perf-trace.h"
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <JavaScriptCore/JavaScript.h>
#include <iostream>
#include <functional>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

class UiWindowLinux : public UiWindow {
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
public:
    static GtkApplication* _app;
    UiWindowLinux();
    void ShowOsWindow();

    template<typename F> static void ExecOnMainThreadSync(F fn);
    static void ExecOnMainThread(GSourceFunc fn, void* param);
    static gboolean MainThreadFnCallback(gpointer arg);
private:
    WebKitWebView* _webView;
    GtkWidget* _window;
    GdkWindow* _gdkWindow;
    GtkWidget* _menu = NULL;
    bool _topmost = false;
    WindowRect _rect;

    static void WindowDestroyed(GtkWidget* widget, gpointer data);
    static void WebViewLoadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, gpointer data);
    static gboolean WebViewContextMenuRequested(WebKitWebView* webView, WebKitContextMenu* menu,
        GdkEvent* event, WebKitHitTestResult* hitTest, gpointer data);
    static void WebViewTitleChanged(GObject* object, GParamSpec* pspec, gpointer data);
    static gboolean WebViewScriptDialog(WebKitWebView* webView, WebKitScriptDialog* dialog, gpointer data);
    static void MsgExecutedCallback(GObject *object, GAsyncResult *res, gpointer data);
    static void MenuActivated(GtkWidget* widget, gpointer data);
    static gboolean PostMsgSync(gpointer arg);
    static JSValueRef PostMsgWebCallback(JSContextRef ctx, JSObjectRef func, JSObjectRef self,
        size_t argc, const JSValueRef argv[], JSValueRef* exception);
    static gboolean MsgCallbackSync(gpointer arg);

    void AddMenu(UiMenu* menu, GtkWidget* parentMenu);
    void AddFileMenuItems(GtkWidget* parent, bool hasItems);
    void AddEditMenuItems(GtkWidget* parent, bool hasItems);
    void AddHelpMenuItems(GtkWidget* parent, bool hasItems);
    void HandleInternalMenu(MENU_INTL menu);
};

struct PostMsgArg {
    UiWindowLinux* win;
    Utf8String* msg;
    v8::Persistent<v8::Function>* callback;
};

struct MsgCallbackArg {
    UiWindowLinux* win;
    long callback;
    Utf8String* result;
    Utf8String* error;
};

GtkApplication* UiWindowLinux::_app;

template<typename F> void
UiWindowLinux::ExecOnMainThreadSync(F fn) {
    uv_sem_t sem;
    uv_sem_init(&sem, 0);
    std::pair<F*, uv_sem_t*> arg;
    arg.first = &fn;
    arg.second = &sem;
    g_main_context_invoke(NULL, [](gpointer arg) -> gboolean {
        std::pair<F*, uv_sem_t*>* pair = (std::pair<F*, uv_sem_t*>*)arg;
        F fn = *((F*)pair->first);
        uv_sem_t* sem = pair->second;
        fn();
        uv_sem_post(sem);
        return FALSE;
    }, &arg);
    uv_sem_wait(&sem);
    uv_sem_destroy(&sem);
}

void UiWindowLinux::ExecOnMainThread(GSourceFunc fn, void* param) {
    auto arg = new std::pair<GSourceFunc, void*>();
    arg->first = fn;
    arg->second = param;
    g_main_context_invoke(NULL, MainThreadFnCallback, (gpointer)arg);
}

gboolean UiWindowLinux::MainThreadFnCallback(gpointer arg) {
    auto pairArg = (std::pair<GSourceFunc, void*>*)arg;
    auto fn = (GSourceFunc)pairArg->first;
    void* param = pairArg->second;
    fn(param);
    delete pairArg;
    return FALSE;
}

int UiWindow::Main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);
    gtk_main();
    return 0;
}

UiWindowLinux::UiWindowLinux() {
}

int UiWindow::Alert(Utf8String* msg, ALERT_TYPE type) {
    int result;
    UiWindowLinux::ExecOnMainThreadSync([msg, type, &result]() {
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                GTK_DIALOG_MODAL,
                type == ALERT_TYPE::ALERT_ERROR ? GTK_MESSAGE_ERROR : GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                (char*)*msg, "Error");
        gtk_window_set_title(GTK_WINDOW(dialog), "Error");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        delete msg;
        result = 0;
    });
    return result;
}

void UiWindowLinux::ShowOsWindow() {
    _window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (_parent) {
        gtk_window_set_transient_for(GTK_WINDOW(_window), GTK_WINDOW(((UiWindowLinux*)_parent)->_window));
        gtk_window_set_modal(GTK_WINDOW(_window), TRUE);
    }
    gtk_window_set_default_size(GTK_WINDOW(_window), _rect.Width, _rect.Height);
    
    auto windowBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);    
    if (_isMainWindow && _config->Menu) {
        _menu = gtk_menu_bar_new();
        AddMenu(_config->Menu, _menu);
        gtk_box_pack_start(GTK_BOX(windowBox), _menu, FALSE, FALSE, 0);
    }
    
    GtkWidget *scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(windowBox), scrolledWindow, TRUE, TRUE, 0);
    
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(_window), windowBox);
    gtk_window_set_title(GTK_WINDOW(_window), (char*)*_config->Title);
    gtk_window_move(GTK_WINDOW(_window), _rect.Left, _rect.Top);
    gtk_widget_set_size_request(GTK_WIDGET(_window), _rect.Width, _rect.Height);
    gtk_window_set_resizable(GTK_WINDOW(this->_window), _config->Resizable);
    gtk_window_set_decorated(GTK_WINDOW(this->_window), _config->Frame);
    gtk_window_set_keep_above(GTK_WINDOW(this->_window), _config->Topmost);
    gtk_widget_set_opacity(this->_window, _config->Opacity);
    g_signal_connect(_window, "destroy", G_CALLBACK(WindowDestroyed), this);

    PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_CREATE_WINDOW);

    _webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(scrolledWindow), GTK_WIDGET(_webView));
    gtk_widget_grab_focus(GTK_WIDGET(_webView));
    g_signal_connect(_webView, "load-changed", G_CALLBACK(WebViewLoadChanged), this);
    g_signal_connect(_webView, "context-menu", G_CALLBACK(WebViewContextMenuRequested), this);
    g_object_connect(_webView, "signal::notify::title", G_CALLBACK(WebViewTitleChanged), this, NULL);
    g_signal_connect(_webView, "script-dialog", G_CALLBACK(WebViewScriptDialog), this);
    PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_INIT_BROWSER_CONTROL);

    gtk_widget_show_all(_window);

    switch (_config->State) {
    case WINDOW_STATE::WINDOW_STATE_NORMAL:
        gtk_window_present(GTK_WINDOW(_window));
        break;
    case WINDOW_STATE::WINDOW_STATE_HIDDEN:
        gtk_widget_hide(this->_window);
        break;
    case WINDOW_STATE::WINDOW_STATE_MAXIMIZED:
        gtk_window_maximize(GTK_WINDOW(this->_window));
        gtk_window_present(GTK_WINDOW(_window));
        break;
    case WINDOW_STATE::WINDOW_STATE_MINIMIZED:
        gtk_window_iconify(GTK_WINDOW(this->_window));
        break;
    }
    
    while (gtk_events_pending())
        gtk_main_iteration();

    _gdkWindow = gtk_widget_get_window(_window);
    EmitEvent(new WindowEventData(WINDOW_EVENT_READY));
}

UiWindow* UiWindow::CreateUiWindow() {
    return new UiWindowLinux();
}

void UiWindowLinux::WindowDestroyed(GtkWidget* widget, gpointer data) {
    UiWindowLinux* win = (UiWindowLinux*)data;
    win->EmitEvent(new WindowEventData(WINDOW_EVENT_CLOSED));
    if (win->_isMainWindow)
        gtk_main_quit();
}

void UiWindowLinux::WebViewLoadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, gpointer data) {
    UiWindowLinux* win = (UiWindowLinux*)data;
    switch (loadEvent) {
    case WEBKIT_LOAD_STARTED:
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_1ST_REQUEST_SENT);
        break;
    case WEBKIT_LOAD_COMMITTED:
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_LOAD_INDEX_HTML);
        break;
    case WEBKIT_LOAD_FINISHED:
        win->EmitEvent(new WindowEventData(WINDOW_EVENT_DOCUMENT_COMPLETE));
        break;
    default:
        break;
    }
}

gboolean UiWindowLinux::WebViewContextMenuRequested(WebKitWebView* webView, WebKitContextMenu* menu,
        GdkEvent* event, WebKitHitTestResult* hitTest, gpointer data) {
    return TRUE;
}

void UiWindowLinux::WebViewTitleChanged(GObject* object, GParamSpec* pspec, gpointer data) {
    UiWindowLinux* win = (UiWindowLinux*)data;
    GValue value = G_VALUE_INIT;
    g_value_init(&value, G_TYPE_STRING);
    g_object_get_property(object, "title", &value);
    gtk_window_set_title(GTK_WINDOW(win->_window), g_value_get_string(&value));
    
    // TODO: is there a more proper way to execute some code before window.ready without this hack?
    auto script = "typeof window.backend === 'object' ? window.backend : window.backend = {"
        "postMessage: function(data, cb) { if (typeof cb === 'function') window.backend['_cb' + (++window.backend._lastCb)] = cb; alert('_pm:' + (cb ? window.backend._lastCb : 0) + JSON.stringify(data)); if (window.backend._lastCb >= 10000000) window.backend._lastCb = 0; },"
        "onMessage: null,"
        "_lastCb:0"
        "}";
    webkit_web_view_run_javascript(win->_webView, script, NULL, NULL, win);
}

gboolean UiWindowLinux::WebViewScriptDialog(WebKitWebView* webView, WebKitScriptDialog* dialog, gpointer data) {
    auto win = (UiWindowLinux*)data;
    auto type = webkit_script_dialog_get_dialog_type(dialog);
    auto msg = (char*)webkit_script_dialog_get_message(dialog);
    auto msgHeader = "_pm:";
    auto msgHeaderLen = strlen(msgHeader);
    if (type == WEBKIT_SCRIPT_DIALOG_ALERT && !strncmp(msg, msgHeader, msgHeaderLen)) {
        int cbNum = atoi(&msg[msgHeaderLen]);
        auto ix = msgHeaderLen;
        while (msg[ix] != '{') {
            ix++;
        }
        auto msgStr = new Utf8String(&msg[ix]);
        win->EmitEvent(new WindowEventData(WINDOW_EVENT_MESSAGE, (long)msgStr, (long)cbNum));
        return TRUE;
    }
    return FALSE;
}

// TODO: why does this throw an error?
/*void UiWindowLinux::InitScriptExecutedCallback(GObject *object, GAsyncResult *res, gpointer data) {
    auto win = (UiWindowLinux*)data;
    std::cerr << "initcb\n";
    GError* error = NULL;
    auto jsRes = webkit_web_view_run_javascript_finish(WEBKIT_WEB_VIEW(win->_webView), res, &error);
    if (!jsRes) {
        std::cerr << "err " << error->message << "\n";
        return;
    }
    auto context = webkit_web_view_get_javascript_global_context(win->_webView);
    auto backendObj = JSValueToObject(context, webkit_javascript_result_get_value(jsRes), NULL);
    std::cerr << "res " << backendObj << "\n";
    auto fnName = JSStringCreateWithUTF8CString("_pm");
    std::cerr << "_pm\n";
    auto fnValue = JSObjectMakeFunctionWithCallback(context, NULL, PostMsgWebCallback);
    std::cerr << "fn\n";
    JSValueRef ex;
    JSObjectSetProperty(context, backendObj, fnName, backendObj,
        kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum | kJSPropertyAttributeReadOnly, &ex);
    JSStringRelease(fnName);
    std::cerr << "set " << ex << "\n";
}*/

void UiWindowLinux::Show(WindowRect& rect) {
    this->_rect = rect;
    g_main_context_invoke(NULL, [](gpointer arg) -> gboolean {
        UiWindowLinux* _this = (UiWindowLinux*)arg;
        _this->ShowOsWindow();
        return FALSE;
    }, this);
}

UI_RESULT UiWindow::OsInitialize() {
    return UI_S_OK;
}

void UiWindowLinux::Close(void) {
    gtk_window_close(GTK_WINDOW(_window));
}

void UiWindowLinux::Navigate(Utf8String* url) {
    ExecOnMainThreadSync([this, url]() {
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_CALL_NAVIGATE);
        webkit_web_view_load_uri(this->_webView, *url);
    });
    delete url;
}

void UiWindowLinux::PostMsg(Utf8String* msg, v8::Persistent<v8::Function>* callback) {
    auto arg = new PostMsgArg();
    arg->win = this;
    arg->msg = msg;
    arg->callback = callback;
    ExecOnMainThread(PostMsgSync, arg);
}

gboolean UiWindowLinux::PostMsgSync(gpointer arg) {auto pa = (PostMsgArg*)arg;
    char* script = new char[strlen(*pa->msg) + 128];
    strcpy(script, "JSON.stringify(typeof backend.onMessage === 'function' ? backend.onMessage(");
    strcat(script, *pa->msg);
    strcat(script, "):null)");
    delete pa->msg;
    webkit_web_view_run_javascript(pa->win->_webView, script, NULL, MsgExecutedCallback, pa);
    delete script;
    return false;
}

void UiWindowLinux::MsgExecutedCallback(GObject *object, GAsyncResult *res, gpointer data) {
    auto pa = (PostMsgArg*)data;
    Utf8String* resultStr = NULL;
    Utf8String* errorStr = NULL;
    GError* err = NULL;
    auto jsRes = webkit_web_view_run_javascript_finish(pa->win->_webView, res, &err);
    if (jsRes) {
        auto context = webkit_javascript_result_get_global_context(jsRes);
        auto value = webkit_javascript_result_get_value(jsRes);
        auto jsStrRef = JSValueToStringCopy(context, value, NULL);
        auto strLength = JSStringGetMaximumUTF8CStringSize(jsStrRef);
        auto strVal = new gchar[strLength];
        JSStringGetUTF8CString(jsStrRef, strVal, strLength);
        JSStringRelease(jsStrRef);
        resultStr = new Utf8String(strVal);
        delete strVal;
    } else {
        errorStr = new Utf8String(err->message);
        g_error_free(err);
    }
    webkit_javascript_result_unref(jsRes);
    pa->win->EmitEvent(new WindowEventData(WINDOW_EVENT_MESSAGE_CALLBACK, (long)pa->callback,
        (long)resultStr, (long)errorStr));
    delete pa;
}

void UiWindowLinux::MsgCallback(void* callback, Utf8String* result, Utf8String* error) {
    auto arg = new MsgCallbackArg();
    arg->win = this;
    arg->callback = (long)callback;
    arg->result = result;
    arg->error = error;
    ExecOnMainThread(MsgCallbackSync, arg);
}

gboolean UiWindowLinux::MsgCallbackSync(gpointer data) {
    auto arg = (MsgCallbackArg*)data;
    if (arg->callback) {
        auto len = strlen(arg->result ? *arg->result : *arg->error);
        char* script = new char[len + 128];
        sprintf(script, "window.backend._cb%ld(%s,%s);delete window.backend._cb%ld;",
            arg->callback,
            arg->result ? (char*)*arg->result : "null",
            arg->error ? (char*)*arg->error : "null",
            arg->callback
        );
        webkit_web_view_run_javascript(arg->win->_webView, script, NULL, NULL, NULL);
        delete script;
    }
    if (arg->result)
        delete arg->result;
    if (arg->error)
        delete arg->error;
    delete arg;
    return false;
}

void UiWindowLinux::AddMenu(UiMenu* menu, GtkWidget* parentMenu) {
    bool isFileMenu = !menu->Parent && menu->Title && (!strcmp(*menu->Title, "File") || !strcmp(*menu->Title, "&File"));
    bool isEditMenu = !menu->Parent && menu->Title && (!strcmp(*menu->Title, "Edit") || !strcmp(*menu->Title, "&Edit"));
    bool isHelpMenu = !menu->Parent && menu->Title && (!strcmp(*menu->Title, "Help") || !strcmp(*menu->Title, "&Help")
        || !strcmp(*menu->Title, "?") || !strcmp(*menu->Title, "&?"));
    bool isStandardMenu = isFileMenu || isEditMenu || isHelpMenu;
    char* label = menu->Title ? (char*)*menu->Title : (char*)"";
    if (label) {
        for (int i = strlen(label); i >= 0; --i) {
            if (label[i] == '&')
                label[i] = '_';
        }
    }
    if (menu->Type == MENU_TYPE::MENU_TYPE_SEPARATOR) {
        auto item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(parentMenu), item);
    }
    else if (isStandardMenu || (menu->Type == MENU_TYPE::MENU_TYPE_SIMPLE && menu->Items)) {
        auto itemMenu = gtk_menu_new();
        auto item = gtk_menu_item_new_with_mnemonic(label);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), itemMenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(parentMenu), item);
        if (isEditMenu) {
            AddEditMenuItems(itemMenu, menu->Items != NULL);
        }
        if (menu->Items) {
            AddMenu(menu->Items, itemMenu);
        }
        if (isHelpMenu) {
            AddHelpMenuItems(itemMenu, menu->Items != NULL);
        }
        if (isFileMenu) {
            AddFileMenuItems(itemMenu, menu->Items != NULL);
        }
    }
    else {
        auto item = gtk_menu_item_new_with_mnemonic(label);
        gtk_menu_shell_append(GTK_MENU_SHELL(parentMenu), item);
        g_signal_connect(item, "activate", G_CALLBACK(MenuActivated), menu);
        g_object_set_data(G_OBJECT(item), "_wnd", this);
    }
    if (menu->Next) {
        AddMenu(menu->Next, parentMenu);
    }
}

void UiWindowLinux::AddFileMenuItems(GtkWidget* parent, bool hasItems) {
    if (hasItems)
        AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String("&Quit"), MENU_INTL_FILE_EXIT), parent);
}

void UiWindowLinux::AddEditMenuItems(GtkWidget* parent, bool hasItems) {
    AddMenu(new UiMenu(new Utf8String("&Undo"), MENU_INTL_EDIT_UNDO), parent);
    AddMenu(new UiMenu(new Utf8String("&Redo"), MENU_INTL_EDIT_REDO), parent);
    AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String("Cu&t"), MENU_INTL_EDIT_CUT), parent);
    AddMenu(new UiMenu(new Utf8String("&Copy"), MENU_INTL_EDIT_COPY), parent);
    AddMenu(new UiMenu(new Utf8String("&Paste"), MENU_INTL_EDIT_PASTE), parent);
    AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String("Select &All"), MENU_INTL_EDIT_SELECT_ALL), parent);
    if (hasItems)
        AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
}

void UiWindowLinux::AddHelpMenuItems(GtkWidget* parent, bool hasItems) {
    if (hasItems)
        AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String("&About"), MENU_INTL_HELP_ABOUT), parent);
}

void UiWindowLinux::HandleInternalMenu(MENU_INTL menu) {
    switch (menu) {
    case MENU_INTL_FILE_EXIT:
        Close();
        break;
    case MENU_INTL_EDIT_UNDO:
        webkit_web_view_execute_editing_command(_webView, WEBKIT_EDITING_COMMAND_UNDO);
        break;
    case MENU_INTL_EDIT_REDO:
        webkit_web_view_execute_editing_command(_webView, WEBKIT_EDITING_COMMAND_REDO);
        break;
    case MENU_INTL_EDIT_CUT:
        webkit_web_view_execute_editing_command(_webView, WEBKIT_EDITING_COMMAND_CUT);
        break;
    case MENU_INTL_EDIT_COPY:
        webkit_web_view_execute_editing_command(_webView, WEBKIT_EDITING_COMMAND_COPY);
        break;
    case MENU_INTL_EDIT_PASTE:
        webkit_web_view_execute_editing_command(_webView, WEBKIT_EDITING_COMMAND_PASTE);
        break;
    case MENU_INTL_EDIT_SELECT_ALL:
        webkit_web_view_execute_editing_command(_webView, WEBKIT_EDITING_COMMAND_SELECT_ALL);
        break;
    case MENU_INTL_HELP_ABOUT:
        break;
    default:
        break;
    }
}

void UiWindowLinux::MenuActivated(GtkWidget* item, gpointer data) {
    UiMenu* menu = (UiMenu*)data;
    UiWindowLinux* _this = (UiWindowLinux*)g_object_get_data(G_OBJECT(item), "_wnd");
    _this->EmitEvent(new WindowEventData(WINDOW_EVENT_MENU, (long)menu));
    if (menu->InternalMenuId > 0) {
        _this->HandleInternalMenu(menu->InternalMenuId);
    }
}

WindowRect UiWindowLinux::GetWindowRect() {
    WindowRect rect;
    ExecOnMainThreadSync([this, &rect]() {
        gtk_window_get_position(GTK_WINDOW(_window), &rect.Left, &rect.Top);
        gtk_window_get_size(GTK_WINDOW(_window), &rect.Width, &rect.Height);
    });
    return rect;
}

WindowRect UiWindowLinux::GetScreenRect() {
    WindowRect rect;
    ExecOnMainThreadSync([this, &rect]() {
        GdkScreen* screen = gdk_screen_get_default();
        rect.Width = gdk_screen_get_width(screen);
        rect.Height = gdk_screen_get_height(screen);
    });
    return rect;
}

void UiWindowLinux::SetWindowRect(WindowRect& rect) {
    _rect = rect;
    ExecOnMainThreadSync([this, &rect]() {
        gtk_window_move(GTK_WINDOW(this->_window), rect.Left, rect.Top);
        gtk_window_resize(GTK_WINDOW(this->_window), rect.Width, rect.Height);
        gtk_widget_set_size_request(GTK_WIDGET(this->_window), rect.Width, rect.Height);
    });
}

Utf8String* UiWindowLinux::GetTitle() {
    Utf8String* title;
    ExecOnMainThreadSync([this, &title]() {
        title = new Utf8String((char*)gtk_window_get_title(GTK_WINDOW(this->_window)));
    });
    return title;
}

WINDOW_STATE UiWindowLinux::GetState() {
    WINDOW_STATE state;
    ExecOnMainThreadSync([this, &state]() {
        gint st = gdk_window_get_state(GDK_WINDOW(_gdkWindow));
        if (!gtk_widget_is_visible(_window)) {
            state = WINDOW_STATE::WINDOW_STATE_HIDDEN;
        } else if (st & GDK_WINDOW_STATE_MAXIMIZED) {
            state = WINDOW_STATE::WINDOW_STATE_MAXIMIZED;
        } else if (st & GDK_WINDOW_STATE_ICONIFIED) {
            state = WINDOW_STATE::WINDOW_STATE_MINIMIZED;
        } else {
            state = WINDOW_STATE::WINDOW_STATE_NORMAL;
        }
    });
    return state;
}

void UiWindowLinux::SetState(WINDOW_STATE state) {
    ExecOnMainThreadSync([this, state]() {
        gint curState = gdk_window_get_state(GDK_WINDOW(this->_gdkWindow));
        bool visible = gtk_widget_is_visible(this->_window);
        if (!visible && (state != WINDOW_STATE::WINDOW_STATE_HIDDEN)) {
            gtk_widget_show(this->_window);
            gtk_window_move(GTK_WINDOW(_window), _rect.Left, _rect.Top);
        }
        if ((curState & GDK_WINDOW_STATE_MAXIMIZED) && (state != WINDOW_STATE::WINDOW_STATE_MAXIMIZED))
            gtk_window_unmaximize(GTK_WINDOW(this->_window));
        if ((curState & GDK_WINDOW_STATE_ICONIFIED) && (state != WINDOW_STATE::WINDOW_STATE_MINIMIZED))
            gtk_window_deiconify(GTK_WINDOW(this->_window));
        switch (state) {
        case WINDOW_STATE::WINDOW_STATE_NORMAL:
            gtk_window_present(GTK_WINDOW(_window));
            break;
        case WINDOW_STATE::WINDOW_STATE_HIDDEN:
            gtk_widget_hide(this->_window);
            break;
        case WINDOW_STATE::WINDOW_STATE_MAXIMIZED:
            gtk_window_maximize(GTK_WINDOW(this->_window));
            gtk_window_present(GTK_WINDOW(_window));
            break;
        case WINDOW_STATE::WINDOW_STATE_MINIMIZED:
            gtk_window_iconify(GTK_WINDOW(this->_window));
            break;
        }
    });
}

bool UiWindowLinux::GetResizable() {
    double resizable;
    ExecOnMainThreadSync([this, &resizable](){ resizable = gtk_window_get_resizable(GTK_WINDOW(this->_window)); });
    return resizable;
}

void UiWindowLinux::SetResizable(bool resizable) {
    ExecOnMainThreadSync([this, resizable]() {
        gtk_widget_set_size_request(GTK_WIDGET(_window), _rect.Width, _rect.Height);
        gtk_window_set_resizable(GTK_WINDOW(this->_window), resizable);
    });
}

bool UiWindowLinux::GetFrame() {
    double frame;
    ExecOnMainThreadSync([this, &frame](){ frame = gtk_window_get_decorated(GTK_WINDOW(this->_window)); });
    return frame;
}

void UiWindowLinux::SetFrame(bool frame) {
    ExecOnMainThreadSync([this, frame](){ gtk_window_set_decorated(GTK_WINDOW(this->_window), frame); });
}

bool UiWindowLinux::GetTopmost() {
    return _topmost;
}

void UiWindowLinux::SetTopmost(bool topmost) {
    _topmost = topmost;
    ExecOnMainThreadSync([this, topmost](){ gtk_window_set_keep_above(GTK_WINDOW(this->_window), topmost); });
}

double UiWindowLinux::GetOpacity() {
    double opacity;
    ExecOnMainThreadSync([this, &opacity](){ opacity = gtk_widget_get_opacity(this->_window); });
    return opacity;
}

void UiWindowLinux::SetOpacity(double opacity) {
    ExecOnMainThreadSync([this, opacity](){ gtk_widget_set_opacity(this->_window, opacity); });
}
