#include <Windows.h>
#include <MinHook.h>
#include "zml_plugin.h"
#include "keybinds_native.h"
#include "fixture.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>
#include <stdexcept>
#include <thread>
#include <chrono>
std::atomic<int> triggerVirtual=VK_F12;
std::atomic<bool> down=false, emergency=false, ownForeground=true;
std::atomic<ULONGLONG> timeNow=10000;
HWND window=nullptr;
SHORT WINAPI keys(int key){if(key==VK_CONTROL||key==VK_SHIFT)return (down||emergency)?SHORT(-32768):0;return ((key==triggerVirtual&&down)||(key==VK_F10&&emergency))?SHORT(-32768):0;}
HWND WINAPI foreground(){return ownForeground?window:nullptr;}
ULONGLONG WINAPI clockNow(){return timeNow;}
std::mutex eventsMutex;std::vector<std::string> events;
void log(void*,const char* message){std::lock_guard lock(eventsMutex);events.emplace_back(message);}
int transform(void*,const char*,ZmlLuaTransform,void*){return 1;}
bool seen(const char* e){std::lock_guard lock(eventsMutex);for(auto& s:events)if(s==e)return true;return false;}
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class T>T proc(HMODULE mod,const char* name){auto f=reinterpret_cast<T>(GetProcAddress(mod,name));require(f!=nullptr,name);return f;}
int wmain(int argc,wchar_t** argv){
 try {
    require(argc>=4,"paths required");
    bool reject=argc>=5;
    std::string tag=reject?std::filesystem::path(argv[4]).string():"";
    if(reject){SetEnvironmentVariableA("MEME_FIXTURE_REJECT",tag.c_str());}
    auto game=LoadLibraryW(argv[1]);require(game!=nullptr,"fixture load");
    auto call=proc<int(*)(int)>(game,"FixtureCall");
    auto count=proc<int(*)(int)>(game,"FixtureCounter");
    auto dialog=proc<const char*(*)()>(game,"FixtureDialog");
    // Owned message-only fixture window. No focus, keyboard injection, or UI
    // automation: private subprocess API shims supply synthetic input/time.
    window=CreateWindowExW(0,L"STATIC",L"fixture",0,0,0,0,0,HWND_MESSAGE,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(window!=nullptr,"message fixture");
    require(MH_Initialize()==MH_OK,"test MinHook init");
    void* originals[3]{};auto user=GetModuleHandleW(L"user32.dll");auto kernel=GetModuleHandleW(L"kernel32.dll");
    void* targets[]={reinterpret_cast<void*>(GetProcAddress(user,"GetAsyncKeyState")),reinterpret_cast<void*>(GetProcAddress(user,"GetForegroundWindow")),reinterpret_cast<void*>(GetProcAddress(kernel,"GetTickCount64"))};
    void* detours[]={reinterpret_cast<void*>(&keys),reinterpret_cast<void*>(&foreground),reinterpret_cast<void*>(&clockNow)};
    for(int i=0;i<3;++i){require(MH_CreateHook(targets[i],detours[i],&originals[i])==MH_OK,"test API shim");require(MH_EnableHook(targets[i])==MH_OK,"enable API shim");}
    auto path=std::filesystem::temp_directory_path()/(L"zml-meme-fixture-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(path);
    {std::ofstream f(path/L"config.ini",std::ios::binary);f<<"enabled=true\nhotkey=Ctrl+Shift+F12\nfast_failure=false\ntimeout=15\nmessage=假封禁测试\\n第二行\nshow_notice=false\n";}
    auto path8=path.u8string();std::string utf8(reinterpret_cast<const char*>(path8.data()),path8.size());
    if(tag!="dependency") {
        require(LoadLibraryW(argv[3])!=nullptr,"owned service fixture load");
        auto keys=LoadLibraryW(std::filesystem::path(ZML_KEYBINDS_DLL).c_str());require(keys!=nullptr,"real independent keybind DLL load");
        auto dir=std::filesystem::path(ZML_KEYBINDS_DLL).parent_path().u8string();
        std::string directory(reinterpret_cast<const char*>(dir.data()),dir.size());
        auto keyPath=(path/L"keybinds").u8string();std::string keyState(reinterpret_cast<const char*>(keyPath.data()),keyPath.size());
        ZmlHost keyHost{sizeof(ZmlHost),1,nullptr,directory.c_str(),keyState.c_str(),log,transform};
        require(proc<ZmlPluginEntry>(keys,"ZML_PluginV1")()->start(&keyHost)==1,"real keybind library start");
    }
    auto mod=LoadLibraryW(argv[2]);require(mod!=nullptr,"mod load");
    auto entry=proc<ZmlPluginEntry>(mod,"ZML_PluginV1");auto plugin=entry();
    ZmlHost host{sizeof(ZmlHost),1,nullptr,"fixture",utf8.c_str(),log,nullptr};
    auto accepted=plugin->start(&host);
    if(reject){
        require(accepted==0,"invalid contract rejected");
        call(Bind);call(GameClick);call(Tick);call(WriteA);
        require(count(BindCalls)==1&&count(Clicks)==1&&count(Writes)==1,"registration rollback preserved originals");
        std::cout<<"precise native rejection / rollback passed\n";return 0;
    }
    require(accepted==1&&seen("native_meme_ready"),"real native hooks initialized");
    require(plugin->start(&host)==0,"duplicate start rejected without unhooking active mod");
    call(Bind);call(LoginRoot);call(ValueActive);
    auto tickFor=[&](unsigned n){for(unsigned i=0;i<n;++i){call(Tick);Sleep(25);}};
    auto press=[&](){down=true;tickFor(5);down=false;tickFor(3);};
    press();require(!seen("network_cut_started"),"login screen cannot arm cut");
    call(GameClick);require(count(Clicks)==1,"unarmed click original");
    // Native game can KEEP the login root. No OnDestroy callback and no
    // direct HGNetSession.UpdateInGameThread invocation in this regression.
    call(EnterGameplay);call(SessionTick);
    require(seen("client_tick_seen")&&seen("game_io_identified"),"virtual manager tick identified actual session");
    ownForeground=false;press();require(!seen("network_cut_started"),"background ignores keys");ownForeground=true;
    press();require(seen("network_cut_started"),"foreground shortcut started cut");
    call(ValueActive);require(call(ReadA)==0,"retained active login view cannot cancel gameplay cut");
    int writes=count(Writes), reads=count(Reads), cws=count(CryptoWrites),crs=count(CryptoReads);
    call(WriteA);call(CryptoWriteA);require(call(ReadA)==0&&call(CryptoReadA)==0&&call(AvailableA)==0,"target I/O blocked both directions");
    require(count(Writes)==writes&&count(Reads)==reads&&count(CryptoWrites)==cws&&count(CryptoReads)==crs,"no blocked original was called");
    require(seen("game_io_suppressed"),"suppression observed on a real detour");
    call(WriteOther);call(CryptoWriteOther);require(call(ReadOther)==7&&call(CryptoReadOther)==8&&call(AvailableOther)==9,"unrelated I/O passes");
    call(ConnectedB);call(WriteB);require(call(ReadB)==0&&call(ReadA)==0,"new reconnect target and old stream remain blocked");
    int before=count(RootNetworkPassed);call(LoginRoot);
    require(count(RootNetworkPassed)==before+1&&call(ReadB)==7,"restored before native login init");
    call(ValueActive);int clicks=count(Clicks);call(GameClick);
    require(count(Clicks)==clicks&&count(Dialogs)==1,"first login click blocked / native dialog called");
    require(std::string(dialog())=="假封禁测试\n第二行","configured UTF-8 / newline dialog text");
    call(GameClick);require(count(Clicks)==clicks+1&&count(Dialogs)==1,"second click original login");
    // A second performance, then explicit emergency cancellation.
    call(EnterGameplay);call(SessionTick);press();require(call(ReadB)==0,"second cut");
    emergency=true;tickFor(5);emergency=false;tickFor(3);
    require(call(ReadB)==7&&seen("emergency_network_restored"),"emergency restored without main game tick");
    call(LoginRoot);call(ValueActive);clicks=count(Clicks);call(GameClick);require(count(Clicks)==clicks+1,"cancel has no fake login");
    // New saved library key wins over legacy private F12 immediately (no game
    // or mod restart); both actions still use the real independent DLL backend.
    std::filesystem::create_directories(path/L"keybinds");
    {std::ofstream f(path/L"keybinds/bindings.ini");f<<"ZML_KEYBINDS=1\n"<<"disconnect-meme:trigger=Ctrl+Shift+F11,None\n";}
    triggerVirtual=VK_F11;timeNow+=101;
    // Clock advance: no UI/session tick at all. Native transport deadline is
    // checked on every I/O call, independent of watchdog thread scheduling.
    call(EnterGameplay);call(SessionTick);press();require(call(ReadB)==0,"timeout scenario armed");
    timeNow+=15000;require(call(ReadB)==7,"timeout always fail-open");call(LoginRoot);call(ValueActive);
    clicks=count(Clicks);call(GameClick);require(count(Clicks)==clicks+1,"timeout clears fake login");
    // Dialog managed exception must fall back to the original click.
    call(EnterGameplay);call(SessionTick);press();call(LoginRoot);call(ValueActive);call(SetDialogFailure);
    clicks=count(Clicks);call(GameClick);require(count(Clicks)==clicks+1&&seen("dialog_failed_normal_login"),"dialog failure normal-login rollback");
    call(EnterGameplay);call(SessionTick);press();require(call(ReadB)==0,"hot-disable scenario armed");
    {std::ofstream f(path/L"config.ini",std::ios::binary);f<<"enabled=false\ntimeout=60.0\n";}
    timeNow+=1001;tickFor(6);require(call(ReadB)==7&&seen("disabled_network_restored"),"saved standard config immediately cancels cut");
    {std::ofstream f(path/L"config.ini",std::ios::binary);f<<"enabled=true\nfast_failure=false\ntimeout=15\n";}
    timeNow+=1001;tickFor(6);call(SessionTick);press();require(call(ReadB)==0,"config enabled again");
    {std::ofstream f(path/L"config.ini",std::ios::binary);f<<"message=<bad>\n";}
    timeNow+=1001;tickFor(6);require(call(ReadB)==7&&seen("config_rejected"),"bad config fail-open cancels networking");
    {std::ofstream f(path/L"config.ini",std::ios::binary);f<<"enabled=true\nfast_failure=false\ntimeout=15\n";}
    timeNow+=1001;tickFor(6);call(EnterGameplay);call(NoSession);call(SessionTick);press();
    require(call(ReadB)==7&&seen("trigger_ignored_session_disconnected"),"no current session cannot use stale I/O roots");
    call(SessionRestored);call(SessionTick);press();require(call(ReadB)==0,"session restoration recognized without login destroy");
    emergency=true;tickFor(5);emergency=false;tickFor(3);
    // Low-level close runs ONLY in native I/O callbacks off the game thread.
    // Synthetic session cleanup
    call(ClearDialogFailure);call(SlowSocketClose);
    auto ioThread=[&](int op){int result=0;std::thread t([&]{result=call(op);});t.join();return result;};
    {std::ofstream f(path/L"config.ini",std::ios::binary);f<<"enabled=true\ntimeout=120\nfast_failure=true\nfailure_delay=3\n";}
    timeNow+=1001;tickFor(6);call(SessionRestored);call(EnterGameplay);call(SessionTick);press();
    auto closes=count(SocketCloses);
    {std::ofstream f(path/L"config.ini",std::ios::binary);f<<"enabled=true\ntimeout=120\nfast_failure=true\nfailure_delay=10.0\n";}
    timeNow+=2999;tickFor(6);call(SessionTick);ioThread(ReadB);
    require(count(SocketCloses)==closes,"fast socket close not early");
    timeNow+=1;
    auto beforeTick=std::chrono::steady_clock::now();call(SessionTick);call(ReadB);
    require(std::chrono::steady_clock::now()-beforeTick<std::chrono::milliseconds(100)&&count(SocketCloses)==closes,"main thread never executes shutdown");
    std::thread slowClose([&]{call(ReadB);});
    for(int i=0;i<100&&count(SocketCloses)==closes;++i)Sleep(5);
    beforeTick=std::chrono::steady_clock::now();call(SessionTick);call(Tick);
    auto uiDuration=std::chrono::steady_clock::now()-beforeTick;
    slowClose.join();require(uiDuration<std::chrono::milliseconds(100),"slow socket close cannot hold main-thread lock");
    require(count(SocketCloses)==closes+1&&count(WorkerSocketCloses)==closes+1&&count(ReconnectFailures)==0,"worker-only socket close; no session cleanup invocation");
    require(call(SocketFlagB)==1&&call(SocketFlagA)==0&&call(ReadOther)==7,"native fail-reconnect flag scoped to current I/O");
    auto closedReads=count(Reads);
    require(seen("fast_io_close_finished")&&call(ReadB)==0&&count(Reads)==closedReads+1,"native closed-stream EOF passes through for task cleanup");
    ioThread(ReadB);ioThread(ReadB);call(SessionTick);require(count(SocketCloses)==closes+1,"one-shot physical close");
    call(LoginRoot);call(ValueActive);require(call(SocketFlagB)==0,"native test flag restored before login");
    auto clean=count(CleanAsyncConnects);ioThread(AsyncConnectB);require(count(CleanAsyncConnects)==clean+1,"normal reconnect no lingering testing flag");
    clicks=count(Clicks);auto dialogs=count(Dialogs);call(GameClick);call(GameClick);
    require(count(Clicks)==clicks+1&&count(Dialogs)==dialogs+1&&call(ReadB)==7,"fast flow first fake and second normal click");
    // Fixed empty-read backoff prevents unbounded busy polling on worker.
    {std::ofstream f(path/L"config.ini",std::ios::binary);f<<"enabled=true\ntimeout=120\nfast_failure=false\n";}
    timeNow+=1001;tickFor(6);call(SessionRestored);call(EnterGameplay);call(SessionTick);press();
    auto beginPoll=std::chrono::steady_clock::now();
    std::thread poll([&]{for(int i=0;i<50;++i)call(ReadB);});poll.join();
    require(std::chrono::steady_clock::now()-beginPoll>=std::chrono::milliseconds(90),"blocked empty reads are paced not spinning");
    emergency=true;tickFor(5);emergency=false;tickFor(3);
    // Restore state on exception
    {std::ofstream f(path/L"config.ini",std::ios::binary);f<<"enabled=true\ntimeout=120\nfast_failure=true\nfailure_delay=3\n";}
    timeNow+=1001;tickFor(6);call(SessionRestored);call(EnterGameplay);call(SessionTick);press();call(SetFastFailureException);
    timeNow+=3000;ioThread(ReadB);require(call(ReadB)==7&&seen("fast_failure_failed_restored")&&call(SocketFlagB)==0,"socket helper exception fail-open");
    ioThread(ReadB);require(count(SocketCloses)==closes+1,"no retry on exception");call(ClearFastFailureException);
    call(LoginRoot);call(ValueActive);clicks=count(Clicks);call(GameClick);require(count(Clicks)==clicks+1,"exception leaves no fake login");
    call(SessionRestored);call(EnterGameplay);call(SessionTick);press();call(NoSession);timeNow+=3000;call(SessionTick);
    require(call(ReadB)==7&&seen("fast_failure_target_unavailable_restored"),"lost session cancels pending worker close");
    // Verify close cancellation in flight
    // the flag must be restored even when native writes it AFTER cancellation.
    call(SessionRestored);call(EnterGameplay);call(SessionTick);press();timeNow+=3000;
    std::thread cancelDuringClose([&]{call(ReadB);});
    for(int i=0;i<100&&count(SocketCloses)==closes+1;++i)Sleep(5);
    emergency=true;tickFor(2);emergency=false;tickFor(2);press();cancelDuringClose.join();call(Tick);
    require(seen("trigger_ignored_cleanup_in_progress"),"cannot start another performance during native close");
    require(call(SocketFlagB)==0&&call(ReadB)==7,"in-flight cancellation restores late native flag");
    std::cout<<"DLL hooks: scope, crypto/read/write, reconnect, native login order, one-shot, UTF-8, emergency, deadline and dialog failure passed\n";
    // Files exist only in a self-owned fresh fixture folder; remove exact file,
    // Clean up test directory
    std::filesystem::remove(path/L"config.ini");
    std::filesystem::remove(path/L"keybinds/bindings.ini");
    std::filesystem::remove(path/L"keybinds");std::filesystem::remove(path);
    return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
