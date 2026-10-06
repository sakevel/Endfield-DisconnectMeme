// Synthetic GameAssembly test fixture
#include <Windows.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include "fixture.hpp"
#define EXPORT extern "C" __declspec(dllexport)
#define NOINLINE __declspec(noinline)
struct Class;
struct Object {Class* cls;bool active=false;bool failReconnect=false;};
struct Meta {void* code;const char* name;const char* ret;std::vector<const char*> args;uint32_t flags=0;};
struct Class {const char* assembly;const char* ns;const char* name;std::vector<Meta> methods;};
Class tcp{"Network.Beyond.dll","Beyond.Network","TcpIO",{}},
 session{"Network.Beyond.dll","Beyond.Network","HGNetSession",{}},
 base{"Network.Beyond.dll","Beyond.Network","HGNetBaseSession",{}},
 manager{"Entry.Beyond.dll","Beyond","LoginManager",{}},
 rootClass{"Entry.Beyond.dll","Beyond.Login","LoginRootPanel",{}},
 enter{"Entry.Beyond.dll","Beyond.Login","LoginEnterGamePanel",{}},
 menu{"Entry.Beyond.dll","Beyond.Login","LoginMenuPanel",{}},
 client{"Gameplay.Beyond.dll","Beyond.Network","NetClientManager",{}},
 gameClass{"Gameplay.Beyond.dll","Beyond.Gameplay","GameInstance",{}},
 systemClass{"UnityEngine.UI.dll","UnityEngine.EventSystems","EventSystem",{}};
Object ioA{&tcp},ioB{&tcp},ioOther{&tcp},sessionObj{&session},managerObj{&manager},
 rootObj{&rootClass},enterObj{&enter},menuObj{&menu},systemObj{&systemClass},clientObj{&client};
Object* currentIo=&ioA;
std::atomic<int> counts[18];
bool disconnected=false,dialogFailure=false;
bool playing=false,hasSession=true;
bool fastFailureException=false;
bool slowSocketClose=false;
DWORD mainThread=0;
std::string dialogText;
std::mutex rootMutex;
std::map<uint32_t,void*> roots;uint32_t nextHandle=1;
NOINLINE void wr(void*,void*,int,int,const void*){++counts[Writes];}
NOINLINE void cw(void*,void*,int,int,const void*){++counts[CryptoWrites];}
NOINLINE int rd(void* o,void* buffer,int offset,int size,const void*){++counts[Reads];if(static_cast<Object*>(o)->failReconnect)return 0;if(size)static_cast<char*>(buffer)[offset]='x';return 7;}
NOINLINE int cr(void* o,void* buffer,int offset,int size,const void*){++counts[CryptoReads];if(static_cast<Object*>(o)->failReconnect)return 0;if(size)static_cast<char*>(buffer)[offset]='y';return 8;}
NOINLINE int av(void* o,const void*){++counts[AvailableCalls];return static_cast<Object*>(o)->failReconnect?0:9;}
NOINLINE void tick(void*,const void*){++counts[TickCalls];}
NOINLINE void update(void*,const void*){++counts[TickCalls];++counts[TickCalls];}
NOINLINE void preTick(void*,float,const void*){++counts[TickCalls];++counts[BindCalls];}
NOINLINE bool isGameplay(const void*){return playing;}
NOINLINE void destroy(void*,const void*){++counts[TickCalls];++counts[TickCalls];++counts[TickCalls];}
NOINLINE void bind(void*,void*,const void*){++counts[BindCalls];}
NOINLINE void rootInit(void*,void*,const void*){playing=false;char buf='?';wr(currentIo,&buf,0,1,nullptr);if(rd(currentIo,&buf,0,1,nullptr)==7)++counts[RootNetworkPassed];}
NOINLINE void changed(void* o,void*,const void*){++counts[BindCalls];if(static_cast<Object*>(o)->active)++counts[BindCalls];}
NOINLINE void click(void*,void*,const void*){++counts[Clicks];}
NOINLINE void account(void*,void*,const void*){++counts[AccountClicks];}
NOINLINE void connect(void*,void* io,const void*){currentIo=static_cast<Object*>(io);++counts[BindCalls];}
NOINLINE void* getIo(void*,const void*){return currentIo;}
NOINLINE bool running(void*,const void*){return !disconnected;}
NOINLINE void alert(void*,void* text,void*,const void*){++counts[Dialogs];dialogText=*static_cast<std::string*>(text);}
NOINLINE void socketTest(void* o,bool fail,const void*) {
 ++counts[SocketCloses];if(GetCurrentThreadId()!=mainThread)++counts[WorkerSocketCloses];
 if(slowSocketClose)Sleep(300);
 static_cast<Object*>(o)->failReconnect=fail;disconnected=true;
}
NOINLINE void* asyncConnect(void* o,void*,int,int,int,int,const void*) {
 ++counts[AsyncConnects];if(!static_cast<Object*>(o)->failReconnect)++counts[CleanAsyncConnects];return &sessionObj;
}
NOINLINE void exhausted(void* o,void* io,const void*) {
 Sleep(300); // Regression: old main-thread session cleanup is deliberately slow.
 ++counts[ReconnectFailures];
 if(o==&sessionObj && io==currentIo)++counts[CorrectFailureArgs];
 if(GetCurrentThreadId()==mainThread)++counts[MainThreadFailures];
 disconnected=true;
}
// Test registration rollback on unsupported prologue
NOINLINE void unsupported(){volatile int value=1;value=value+1;(void)value;}
void initialize(){
 static bool done=false;if(done)return;done=true;
 mainThread=GetCurrentThreadId();
 auto add=[](Class& c,void* code,const char* name,const char* ret,std::initializer_list<const char*> args){c.methods.push_back({code,name,ret,args,0});};
 add(tcp,reinterpret_cast<void*>(&wr),"WriteData","System.Void",{"System.Byte[]","System.Int32","System.Int32"});
 add(tcp,reinterpret_cast<void*>(&cw),"WriteCryptoData","System.Void",{"System.Byte[]","System.Int32","System.Int32"});
 add(tcp,reinterpret_cast<void*>(&rd),"ReadData","System.Int32",{"System.Byte[]","System.Int32","System.Int32"});
 add(tcp,reinterpret_cast<void*>(&cr),"ReadCryptoData","System.Int32",{"System.Byte[]","System.Int32","System.Int32"});
 add(tcp,reinterpret_cast<void*>(&av),"Available","System.Int32",{});
 add(tcp,reinterpret_cast<void*>(&socketTest),"TestCloseNetIO","System.Void",{"System.Boolean"});
 add(tcp,reinterpret_cast<void*>(&asyncConnect),"ConnectAsync","System.Threading.Tasks.Task",{"System.String","System.Int32","System.Int32","System.Int32","System.Int32"});
 add(base,reinterpret_cast<void*>(&getIo),"GetIO","Beyond.Network.INetIO",{});
 add(base,reinterpret_cast<void*>(&running),"get_isRunningAndConnected","System.Boolean",{});
 add(session,reinterpret_cast<void*>(&update),"UpdateInGameThread","System.Void",{});
 add(session,reinterpret_cast<void*>(&connect),"OnConnectedSucceed","System.Void",{"Beyond.Network.INetIO"});
 add(session,reinterpret_cast<void*>(&exhausted),"OnReconnectTimesOver","System.Void",{"Beyond.Network.INetIO"});
 add(manager,reinterpret_cast<void*>(&bind),"SceneComponentOnly_Bind","System.Void",{"Beyond.Login.LoginSceneComponent"});
 add(manager,reinterpret_cast<void*>(&alert),"AlertDialog","System.Void",{"System.String","System.Action"});
 add(rootClass,reinterpret_cast<void*>(&rootInit),"Init","System.Void",{"Beyond.LoginContext"});
 add(rootClass,reinterpret_cast<void*>(&destroy),"OnDestroy","System.Void",{});
 add(enter,reinterpret_cast<void*>(&changed),"OnValueChanged","System.Void",{"Beyond.Login.LoginViewModel"});
 add(enter,reinterpret_cast<void*>(&click),"_OnEnterGameClicked","System.Void",{"UnityEngine.EventSystems.PointerEventData"});
 add(menu,reinterpret_cast<void*>(&account),"_OnLoginClicked","System.Void",{"UnityEngine.EventSystems.PointerEventData"});
 add(systemClass,reinterpret_cast<void*>(&tick),"Update","System.Void",{});
 add(client,reinterpret_cast<void*>(&preTick),"PreTick","System.Void",{"System.Single"});
 add(gameClass,reinterpret_cast<void*>(&isGameplay),"get_isInGameplay","System.Boolean",{});
 gameClass.methods[0].flags=0x10;
 char reject[32]{};GetEnvironmentVariableA("MEME_FIXTURE_REJECT",reject,sizeof(reject));
 if(!strcmp(reject,"signature"))tcp.methods[1].args[2]="System.Int64";
 if(!strcmp(reject,"fastsignature"))tcp.methods[5].args[0]="System.Int32";
 if(!strcmp(reject,"ambiguous"))tcp.methods.push_back(tcp.methods[1]);
 if(!strcmp(reject,"entry"))tcp.methods[1].code=&tcp;
 if(!strcmp(reject,"rollback")){
   auto p=reinterpret_cast<void*>(&unsupported);DWORD old=0;VirtualProtect(p,32,PAGE_EXECUTE_READWRITE,&old);
   memset(p,0xf0,32);FlushInstructionCache(GetCurrentProcess(),p,32);VirtualProtect(p,32,old,&old);
   tcp.methods[4].code=p;
 }
}
struct Image {const char* name;};
Image images[]={{"Network.Beyond.dll"},{"Entry.Beyond.dll"},{"UnityEngine.UI.dll"},{"Gameplay.Beyond.dll"}};
const void* assemblyList[]={&images[0],&images[1],&images[2],&images[3]};
Class* classes[]={&tcp,&session,&base,&manager,&rootClass,&enter,&menu,&systemClass,&client,&gameClass};
EXPORT void* il2cpp_domain_get(){initialize();return &manager;}
EXPORT const void** il2cpp_domain_get_assemblies(void*,size_t* count){*count=4;return assemblyList;}
EXPORT void* il2cpp_assembly_get_image(const void* a){return const_cast<void*>(a);}
EXPORT const char* il2cpp_image_get_name(void* a){return static_cast<Image*>(a)->name;}
EXPORT void* il2cpp_class_from_name(void* im,const char* ns,const char* name){for(auto c:classes)if(!strcmp(c->assembly,static_cast<Image*>(im)->name)&&!strcmp(c->ns,ns)&&!strcmp(c->name,name))return c;return nullptr;}
EXPORT const void* il2cpp_class_get_methods(void* c,void** iterator){auto& v=static_cast<Class*>(c)->methods;auto i=reinterpret_cast<size_t>(*iterator);if(i>=v.size())return nullptr;*iterator=reinterpret_cast<void*>(i+1);return &v[i];}
EXPORT const char* il2cpp_method_get_name(const void* m){return static_cast<const Meta*>(m)->name;}
EXPORT uint32_t il2cpp_method_get_param_count(const void* m){return static_cast<uint32_t>(static_cast<const Meta*>(m)->args.size());}
EXPORT const void* il2cpp_method_get_param(const void* m,uint32_t i){return static_cast<const Meta*>(m)->args.at(i);}
EXPORT const void* il2cpp_method_get_return_type(const void* m){return static_cast<const Meta*>(m)->ret;}
EXPORT uint32_t il2cpp_method_get_flags(const void* m,uint32_t* impl){*impl=0;return static_cast<const Meta*>(m)->flags;}
EXPORT char* il2cpp_type_get_name(const void* t){return _strdup(static_cast<const char*>(t));}
EXPORT void il2cpp_free(void* p){free(p);}
EXPORT void* il2cpp_object_get_class(void* o){return static_cast<Object*>(o)->cls;}
EXPORT void* il2cpp_class_get_field_from_name(void* c,const char* name){if(c==&client&&!strcmp(name,"m_netSession"))return &client;if(c==&tcp&&!strcmp(name,"m_bIsTestReconnectFailed"))return &tcp;return c==&enter&&!strcmp(name,"m_isPanelActive")?&enter:nullptr;}
EXPORT const void* il2cpp_field_get_type(void* f){char reject[32]{};GetEnvironmentVariableA("MEME_FIXTURE_REJECT",reject,sizeof(reject));if(f==&client)return !strcmp(reject,"sessionfield")?"System.Object":"Beyond.Network.HGNetSession";return !strcmp(reject,"field")?"System.String":"System.Boolean";}
EXPORT int il2cpp_field_get_flags(void* f){char r[32]{};GetEnvironmentVariableA("MEME_FIXTURE_REJECT",r,sizeof(r));return f==&tcp&&!strcmp(r,"fastfieldreadonly")?0x21:1;}
EXPORT void il2cpp_field_get_value(void* o,void* f,void* out){if(f==&client)*static_cast<void**>(out)=hasSession?&sessionObj:nullptr;else *static_cast<bool*>(out)=f==&tcp?static_cast<Object*>(o)->failReconnect:static_cast<Object*>(o)->active;}
EXPORT void il2cpp_field_set_value(void* o,void* f,void* value){if(f==&tcp)static_cast<Object*>(o)->failReconnect=*static_cast<bool*>(value);}
EXPORT void* il2cpp_runtime_invoke(const void* m,void* object,void** args,void** exception){*exception=nullptr;if(m==&manager.methods[1]){if(dialogFailure){*exception=&manager;return nullptr;}alert(object,args[0],args[1],m);}else if(m==&tcp.methods[5]){if(fastFailureException){*exception=&tcp;return nullptr;}socketTest(object,*static_cast<bool*>(args[0]),m);}else if(m==&session.methods[2]){if(fastFailureException){*exception=&session;return nullptr;}exhausted(object,args[0],m);}return nullptr;}
EXPORT void* il2cpp_string_new(const char* text){return new std::string(text);}
EXPORT uint32_t il2cpp_gchandle_new(void* o,bool){std::lock_guard lock(rootMutex);auto id=nextHandle++;roots[id]=o;return id;}
EXPORT void* il2cpp_gchandle_get_target(uint32_t id){std::lock_guard lock(rootMutex);auto i=roots.find(id);return i==roots.end()?nullptr:i->second;}
EXPORT void il2cpp_gchandle_free(uint32_t id){std::lock_guard lock(rootMutex);roots.erase(id);}
EXPORT int FixtureCall(int op){char buffer='?';switch(op){
 case Bind:bind(&managerObj,&rootObj,nullptr);break;
 case LoginRoot:rootInit(&rootObj,nullptr,nullptr);break;
 case DestroyRoot:destroy(&rootObj,nullptr);break;
 case ValueActive:enterObj.active=true;changed(&enterObj,nullptr,nullptr);break;
 case ValueInactive:enterObj.active=false;changed(&enterObj,nullptr,nullptr);break;
 case Tick:tick(&systemObj,nullptr);break;
 case SessionTick:preTick(&clientObj,0.016f,nullptr);break;
 case ConnectedB:connect(&sessionObj,&ioB,nullptr);break;
 case GameClick:click(&enterObj,nullptr,nullptr);break;
 case AccountClick:account(&menuObj,nullptr,nullptr);break;
 case WriteA:wr(&ioA,&buffer,0,1,nullptr);break;
 case WriteB:wr(&ioB,&buffer,0,1,nullptr);break;
 case WriteOther:wr(&ioOther,&buffer,0,1,nullptr);break;
 case CryptoWriteA:cw(&ioA,&buffer,0,1,nullptr);break;
 case CryptoWriteOther:cw(&ioOther,&buffer,0,1,nullptr);break;
 case ReadA:return rd(&ioA,&buffer,0,1,nullptr);
 case ReadB:return rd(&ioB,&buffer,0,1,nullptr);
 case ReadOther:return rd(&ioOther,&buffer,0,1,nullptr);
 case CryptoReadA:return cr(&ioA,&buffer,0,1,nullptr);
 case CryptoReadOther:return cr(&ioOther,&buffer,0,1,nullptr);
 case AvailableA:return av(&ioA,nullptr);
 case AvailableOther:return av(&ioOther,nullptr);
 case SetDialogFailure:dialogFailure=true;break;
 case SetDisconnected:disconnected=true;break;
 case EnterGameplay:playing=true;break;
 case NoSession:hasSession=false;break;
 case SessionRestored:hasSession=true;disconnected=false;break;
 case SetFastFailureException:fastFailureException=true;break;
 case ClearFastFailureException:fastFailureException=false;break;
 case ClearDialogFailure:dialogFailure=false;break;
 case SlowSocketClose:slowSocketClose=true;break;
 case SocketFlagA:return ioA.failReconnect;
 case SocketFlagB:return ioB.failReconnect;
 case AsyncConnectB:asyncConnect(&ioB,nullptr,1,1,1,1,nullptr);break;
 }return 0;}
EXPORT int FixtureCounter(int i){return counts[i].load();}
EXPORT const char* FixtureDialog(){return dialogText.c_str();}
