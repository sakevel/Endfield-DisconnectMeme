#include "zml_lua_service.h"
namespace {int add(void*,ZmlLuaSource p,void*){return p!=nullptr;}
const ZmlLuaServicesV1 api{sizeof(ZmlLuaServicesV1),1,add};}
extern "C" __declspec(dllexport) const ZmlLuaServicesV1* ZML_GetLuaServicesV1(){return &api;}
