#include "zml_plugin.h"
#include "keybinds_native.h"
#include "managed.hpp"
#include "config.hpp"
#include "state.hpp"
#include <MinHook.h>
#include <atomic>
#include <thread>
#include <vector>

namespace {
Managed api; Performance performance;
ZmlHost host{};
std::filesystem::path configPath;
std::mutex configMutex, ioMutex;
Config config;
const ZmlKeybindsV1* keybinds=nullptr;
uint64_t triggerKey=0,cancelKey=0;
std::atomic<bool> request{false};
std::atomic<bool> started{false};
std::atomic<bool> suppressionLogged{false};
std::atomic<uint64_t> connectedAt{0};
std::atomic<DWORD> mainThreadId{0};
uint32_t managerRoot=0;
struct IoRoot {uint32_t handle; bool flagOwned=false, originalFlag=false, inFlight=false, closed=false;};
std::vector<IoRoot> ioRoots;
void* tcpClass=nullptr; void* sessionClass=nullptr; void* activeField=nullptr; void* sessionField=nullptr;
void* reconnectFlag=nullptr;
Method getIo, running, alert, inGameplay, testClose;
using Void=void(*)(void*,const void*);
using PreTick=void(*)(void*,float,const void*);
using Pair=void(*)(void*,void*,const void*);
using Write=void(*)(void*,void*,int,int,const void*);
using Read=int(*)(void*,void*,int,int,const void*);
using Available=int(*)(void*,const void*);
using ConnectAsync=void*(*)(void*,void*,int,int,int,int,const void*);
ConnectAsync connectAsyncOriginal=nullptr;
Void tickOriginal=nullptr;
PreTick preTickOriginal=nullptr;
Pair bindOriginal=nullptr, rootOriginal=nullptr, valueOriginal=nullptr,
     clickOriginal=nullptr, accountOriginal=nullptr, connectedOriginal=nullptr;
Write writeOriginal=nullptr, cryptoWriteOriginal=nullptr;
Read readOriginal=nullptr, cryptoReadOriginal=nullptr;
Available availableOriginal=nullptr;
std::vector<void*> hooks;
void event(const char* text) {if(host.log) host.log(host.owner,text);}
Config settings() {std::lock_guard lock(configMutex);return config;}
void cancel(const char* reason) {if(performance.cancel())event(reason);}

// Scoped socket test state
void restoreFlagLocked(IoRoot& r) {
    if(!r.flagOwned || r.inFlight)return;
    auto object=api.target(r.handle);
    if(object && api.boolean(object,reconnectFlag))api.setField(object,reconnectFlag,&r.originalFlag);
    r.flagOwned=false;
}
void restoreFlags() {
    if(performance.blocked(GetTickCount64()))return;
    std::lock_guard lock(ioMutex);for(auto& r:ioRoots)restoreFlagLocked(r);
}
// Filter I/O instances belonging to HGNetSession
void noteIo(void* object) {
    if(!object || api.objectClass(object)!=tcpClass) {cancel("io_contract_rejected");return;}
    std::lock_guard lock(ioMutex);
    if(!performance.blocked(GetTickCount64())) {
        for(auto it=ioRoots.begin();it!=ioRoots.end();) {
            restoreFlagLocked(*it);
            if(!it->inFlight && api.target(it->handle)!=object){api.unroot(it->handle);it=ioRoots.erase(it);}
            else ++it;
        }
    }
    for(auto& r:ioRoots)if(api.target(r.handle)==object)return;
    if(ioRoots.size()>=32) {cancel("io_limit_restored");return;}
    auto handle=api.root(object,false);
    if(!handle){cancel("io_root_failed");return;}
    ioRoots.push_back({handle});event("game_io_identified");
}
bool blocked(void* object) {
    bool gate=performance.blocked(GetTickCount64()), close=false;
    {
        std::lock_guard lock(ioMutex);
        for(auto& r:ioRoots)if(api.target(r.handle)==object) {
            if(!gate){restoreFlagLocked(r);return false;}
            // Forward EOF/errors to native reader
            if(r.closed)return false;
            if(!suppressionLogged.exchange(true))event("game_io_suppressed");
            // Socket close worker callback
            auto main=mainThreadId.load();
            if(main && GetCurrentThreadId()!=main && performance.takeFailure(GetTickCount64())) {
                r.originalFlag=api.boolean(object,reconnectFlag);
                r.flagOwned=!r.originalFlag;r.inFlight=true;close=true;
            }
            gate=true;goto tagged;
        }
        return false;
    }
tagged:
    if(close) {
        event("fast_io_close_begin");
        bool failReconnect=true;void* args[]={&failReconnect};void* exception=nullptr;
        // Close client socket via TestCloseNetIO
        api.invoke(testClose.info,object,args,&exception);
        if(exception)cancel("fast_failure_failed_restored");
        {
            std::lock_guard lock(ioMutex);
            for(auto& r:ioRoots)if(api.target(r.handle)==object) {
                r.inFlight=false;
                r.closed=!exception;
                if(!performance.blocked(GetTickCount64()))restoreFlagLocked(r);
                break;
            }
        }
        event(exception?"fast_io_close_failed":"fast_io_close_finished");
    }
    return close?false:performance.blocked(GetTickCount64());
}
void yieldEmptyRead() {
    auto main=mainThreadId.load();
    if(main && GetCurrentThreadId()!=main)Sleep(2);
}
void* connectAsyncHook(void* o,void* ip,int port,int timeout,int recv,int send,const void* m) {
    restoreFlags();
    return connectAsyncOriginal(o,ip,port,timeout,recv,send,m);
}
void writeHook(void* o,void* b,int offset,int size,const void* m) {
    if(!blocked(o))writeOriginal(o,b,offset,size,m);
}
void cryptoWriteHook(void* o,void* b,int offset,int size,const void* m) {
    if(!blocked(o))cryptoWriteOriginal(o,b,offset,size,m);
}
int readHook(void* o,void* b,int offset,int size,const void* m) {
    if(blocked(o)){yieldEmptyRead();return 0;}
    return readOriginal(o,b,offset,size,m);
}
int cryptoReadHook(void* o,void* b,int offset,int size,const void* m) {
    if(blocked(o)){yieldEmptyRead();return 0;}
    return cryptoReadOriginal(o,b,offset,size,m);
}
int availableHook(void* o,const void* m) {
    if(blocked(o)){yieldEmptyRead();return 0;}
    return availableOriginal(o,m);
}
void restoreAtLogin() {
    connectedAt=0;
    if(performance.login(GetTickCount64()))event("login_reached_network_restored");
    restoreFlags();
}
void bindHook(void* o,void* component,const void* m) {
    if(!managerRoot || api.target(managerRoot)!=o) {
        auto next=api.root(o,false);
        if(next){if(managerRoot)api.unroot(managerRoot);managerRoot=next;}
    }
    bindOriginal(o,component,m);
}
void rootHook(void* o,void* context,const void* m) {
    // Restore connection on login scene entry
    restoreAtLogin();rootOriginal(o,context,m);
}
bool isPlaying() {
    return reinterpret_cast<bool(*)(const void*)>(inGameplay.code)(inGameplay.info);
}
void valueHook(void* o,void* value,const void* m) {
    valueOriginal(o,value,m);
    // Check active panel state
    if(!isPlaying() && api.boolean(o,activeField))restoreAtLogin();
}
bool fakeClick() {
    if(!performance.click(GetTickCount64()))return false;
    auto manager=managerRoot?api.target(managerRoot):nullptr;
    if(!manager){event("dialog_manager_unavailable_normal_login");return false;}
    // Consume one-shot dialog state
    auto cfg=settings();auto text=cfg.text();
    void* desc=api.stringNew(text.c_str());
    auto handle=desc?api.root(desc,false):0;
    if(!handle){event("dialog_text_unavailable_normal_login");return false;}
    void* args[]={desc,nullptr};void* exception=nullptr;
    api.invoke(alert.info,manager,args,&exception);
    api.unroot(handle);
    if(exception){event("dialog_failed_normal_login");return false;}
    event("fake_dialog_shown");return true;
}
void clickHook(void* o,void* evt,const void* m) {
    if(!api.boolean(o,activeField) || !fakeClick())clickOriginal(o,evt,m);
}
void accountHook(void* o,void* evt,const void* m) {if(!fakeClick())accountOriginal(o,evt,m);}
void connectedHook(void* o,void* io,const void* m) {noteIo(io);connectedOriginal(o,io,m);}
void preTickHook(void* o,float dt,const void* m) {
    mainThreadId=GetCurrentThreadId();
    // NetClientManager PreTick hook
    static bool seen=false;
    if(!seen){seen=true;event("client_tick_seen");}
    auto session=api.reference(o,sessionField);
    if(session && api.objectClass(session)==sessionClass) {
        auto io=reinterpret_cast<void*(*)(void*,const void*)>(getIo.code)(session,getIo.info);
        if(io)noteIo(io);
        auto isRunning=reinterpret_cast<bool(*)(void*,const void*)>(running.code)(session,running.info);
        if(isRunning)connectedAt=GetTickCount64();else connectedAt=0;
    } else {
        connectedAt=0;
        if(performance.failureDue(GetTickCount64()))cancel("fast_failure_target_unavailable_restored");
    }
    restoreFlags();
    preTickOriginal(o,dt,m);
}
bool foreground();
void tickHook(void* o,const void* m) {
    mainThreadId=GetCurrentThreadId();
    restoreFlags();
    if(request.exchange(false)) {
        auto cfg=settings();auto now=GetTickCount64();auto last=connectedAt.load();
        bool tagged=false,inFlight=false;
        {std::lock_guard lock(ioMutex);tagged=!ioRoots.empty();for(auto& r:ioRoots)inFlight|=r.inFlight;}
        auto playing=isPlaying();
        if(!cfg.enabled)event("trigger_ignored_disabled");
        else if(!foreground())event("trigger_ignored_background");
        else if(inFlight)event("trigger_ignored_cleanup_in_progress");
        else if(!playing)event("trigger_ignored_not_gameplay");
        else if(!last || now-last>=2000)event("trigger_ignored_session_disconnected");
        else if(!tagged)event("trigger_ignored_game_io_missing");
        else if(performance.begin(now,cfg.timeout,cfg.fastFailure?cfg.failureDelay:0)){
            {std::lock_guard lock(ioMutex);for(auto& r:ioRoots)r.closed=false;}
            suppressionLogged=false;event("network_cut_started");
        }
        else event("trigger_ignored_already_active");
    }
    tickOriginal(o,m);
}
bool foreground() {
    DWORD pid=0;auto window=GetForegroundWindow();
    if(window)GetWindowThreadProcessId(window,&pid);
    return pid==GetCurrentProcessId();
}
void worker() {
    bool rejected=false;uint64_t refresh=0;
    for(;;) {
        auto now=GetTickCount64();
        if(now>=refresh) {
            Config next;
            if(read_config(configPath,next)) {
                {std::lock_guard lock(configMutex);config=next;}
                if(!next.enabled){request=false;cancel("disabled_network_restored");}
                rejected=false;
            } else {
                {std::lock_guard lock(configMutex);config.enabled=false;}
                request=false;cancel("invalid_config_network_restored");
                if(!rejected){event("config_rejected");rejected=true;}
            }
            refresh=now+1000;
        }
        auto cfg=settings();bool down=keybinds->pressed(triggerKey)!=0,stop=keybinds->pressed(cancelKey)!=0;
        if(foreground()) {
            if(stop){request=false;cancel("emergency_network_restored");}
            else if(down && cfg.enabled) {
                if(performance.phase(now)!=Performance::Phase::idle){request=false;cancel("toggle_network_restored");}
                else request=true;
            }
        }
        // Connection timeout watchdog
        (void)performance.phase(now);
        Sleep(20); // Poll interval
    }
}
template<class F> void install(Method method,F detour,F& original) {
    if(MH_CreateHook(method.code,reinterpret_cast<void*>(detour),reinterpret_cast<void**>(&original))!=MH_OK)
        throw std::runtime_error("Native hook creation failed");
    hooks.push_back(method.code);
}
int start(const ZmlHost* value) {
    if(!value || value->size<sizeof(ZmlHost) || value->abi!=1 || !value->state_directory || !value->log)return 0;
    if(started.exchange(true))return 0;
    host=*value;
    try {
        configPath=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(value->state_directory)))/L"config.ini";
        if(!read_config(configPath,config))throw std::runtime_error("Invalid private config");
        auto library=GetModuleHandleW(L"Keybinds.dll");
        auto entry=library?reinterpret_cast<ZmlKeybindsEntry>(GetProcAddress(library,"ZML_KeybindsV1")):nullptr;
        keybinds=entry?entry():nullptr;
        if(!keybinds || keybinds->size!=sizeof(ZmlKeybindsV1) || keybinds->abi!=1 ||
           !keybinds->register_action || !keybinds->unregister_action || !keybinds->pressed)
            throw std::runtime_error("Required keybinds native API1 unavailable");
        std::string legacy="Ctrl+Shift+F"+std::to_string(config.key-VK_F1+1);
        ZmlKeyActionV1 trigger{sizeof(ZmlKeyActionV1),"disconnect-meme","trigger","开始/取消封禁演出",legacy.c_str(),"None"};
        ZmlKeyActionV1 stop{sizeof(ZmlKeyActionV1),"disconnect-meme","cancel","紧急恢复客户端通信","Ctrl+Shift+F10","None"};
        triggerKey=keybinds->register_action(&trigger);cancelKey=keybinds->register_action(&stop);
        if(!triggerKey || !cancelKey)throw std::runtime_error("Keybinds action registration failed");
        api.initialize();
        sessionClass=api.klass("Network.Beyond.dll","Beyond.Network","HGNetSession");
        auto client=api.klass("Gameplay.Beyond.dll","Beyond.Network","NetClientManager");
        auto game=api.klass("Gameplay.Beyond.dll","Beyond.Gameplay","GameInstance");
        auto base=api.klass("Network.Beyond.dll","Beyond.Network","HGNetBaseSession");
        tcpClass=api.klass("Network.Beyond.dll","Beyond.Network","TcpIO");
        auto manager=api.klass("Entry.Beyond.dll","Beyond","LoginManager");
        auto root=api.klass("Entry.Beyond.dll","Beyond.Login","LoginRootPanel");
        auto enter=api.klass("Entry.Beyond.dll","Beyond.Login","LoginEnterGamePanel");
        auto menu=api.klass("Entry.Beyond.dll","Beyond.Login","LoginMenuPanel");
        auto system=api.klass("UnityEngine.UI.dll","UnityEngine.EventSystems","EventSystem");
        activeField=api.booleanField(enter,"m_isPanelActive");
        sessionField=api.referenceField(client,"m_netSession","Beyond.Network.HGNetSession");
        inGameplay=api.method(game,"get_isInGameplay","System.Boolean",true,{});
        getIo=api.method(base,"GetIO","Beyond.Network.INetIO",false,{});
        running=api.method(base,"get_isRunningAndConnected","System.Boolean",false,{});
        testClose=api.method(tcpClass,"TestCloseNetIO","System.Void",false,{"System.Boolean"});
        reconnectFlag=api.mutableBooleanField(tcpClass,"m_bIsTestReconnectFailed");
        alert=api.method(manager,"AlertDialog","System.Void",false,{"System.String","System.Action"});
        // Resolve metadata before installing hooks
        auto tick=api.method(system,"Update","System.Void",false,{});
        auto preTick=api.method(client,"PreTick","System.Void",false,{"System.Single"});
        auto connect=api.method(sessionClass,"OnConnectedSucceed","System.Void",false,{"Beyond.Network.INetIO"});
        auto bind=api.method(manager,"SceneComponentOnly_Bind","System.Void",false,{"Beyond.Login.LoginSceneComponent"});
        auto init=api.method(root,"Init","System.Void",false,{"Beyond.LoginContext"});
        auto change=api.method(enter,"OnValueChanged","System.Void",false,{"Beyond.Login.LoginViewModel"});
        auto click=api.method(enter,"_OnEnterGameClicked","System.Void",false,{"UnityEngine.EventSystems.PointerEventData"});
        auto account=api.method(menu,"_OnLoginClicked","System.Void",false,{"UnityEngine.EventSystems.PointerEventData"});
        auto wr=api.method(tcpClass,"WriteData","System.Void",false,{"System.Byte[]","System.Int32","System.Int32"});
        auto cw=api.method(tcpClass,"WriteCryptoData","System.Void",false,{"System.Byte[]","System.Int32","System.Int32"});
        auto rd=api.method(tcpClass,"ReadData","System.Int32",false,{"System.Byte[]","System.Int32","System.Int32"});
        auto cr=api.method(tcpClass,"ReadCryptoData","System.Int32",false,{"System.Byte[]","System.Int32","System.Int32"});
        auto av=api.method(tcpClass,"Available","System.Int32",false,{});
        auto async=api.method(tcpClass,"ConnectAsync","System.Threading.Tasks.Task",false,{"System.String","System.Int32","System.Int32","System.Int32","System.Int32"});
        if(MH_Initialize()!=MH_OK)throw std::runtime_error("Private MinHook init failed");
        install(tick,tickHook,tickOriginal);install(preTick,preTickHook,preTickOriginal);
        install(connect,connectedHook,connectedOriginal);install(bind,bindHook,bindOriginal);
        install(init,rootHook,rootOriginal);
        install(change,valueHook,valueOriginal);install(click,clickHook,clickOriginal);install(account,accountHook,accountOriginal);
        install(wr,writeHook,writeOriginal);install(cw,cryptoWriteHook,cryptoWriteOriginal);
        install(async,connectAsyncHook,connectAsyncOriginal);
        install(rd,readHook,readOriginal);install(cr,cryptoReadHook,cryptoReadOriginal);install(av,availableHook,availableOriginal);
        for(auto h:hooks)if(MH_EnableHook(h)!=MH_OK)throw std::runtime_error("Native hook activation failed");
        HMODULE pinned=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
             reinterpret_cast<LPCWSTR>(&start),&pinned))throw std::runtime_error("Plugin lifetime pin failed");
        std::thread(worker).detach();
        event("native_meme_ready");return 1;
    } catch(const std::exception& e) {
        performance.cancel();
        if(keybinds){if(triggerKey)keybinds->unregister_action(triggerKey);if(cancelKey)keybinds->unregister_action(cancelKey);}
        triggerKey=cancelKey=0;
        for(auto h:hooks){MH_DisableHook(h);MH_RemoveHook(h);}hooks.clear();MH_Uninitialize();
        event(e.what());started=false;return 0;
    }
}
const ZmlPlugin plugin{sizeof(ZmlPlugin),1,"disconnect-meme",start};
}
extern "C" __declspec(dllexport) const ZmlPlugin* ZML_PluginV1(){return &plugin;}
