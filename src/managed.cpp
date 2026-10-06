#include "managed.hpp"
#include <cstring>
void Managed::initialize() {
    module_=GetModuleHandleW(L"GameAssembly.dll");
    if(!module_) throw std::runtime_error("GameAssembly unavailable");
#define EXPORT(member,name) load(member,"il2cpp_" name)
    EXPORT(domain_,"domain_get"); EXPORT(assemblies_,"domain_get_assemblies");
    EXPORT(image_,"assembly_get_image"); EXPORT(imageName_,"image_get_name"); EXPORT(class_,"class_from_name");
    EXPORT(methods_,"class_get_methods"); EXPORT(methodName_,"method_get_name"); EXPORT(argc_,"method_get_param_count");
    EXPORT(param_,"method_get_param"); EXPORT(ret_,"method_get_return_type"); EXPORT(flags_,"method_get_flags");
    EXPORT(typeName_,"type_get_name"); EXPORT(free_,"free"); EXPORT(objectClass,"object_get_class");
    EXPORT(field,"class_get_field_from_name"); EXPORT(getField,"field_get_value");
    EXPORT(setField,"field_set_value");
    EXPORT(fieldType_,"field_get_type"); EXPORT(fieldFlags_,"field_get_flags");
    EXPORT(invoke,"runtime_invoke"); EXPORT(stringNew,"string_new"); EXPORT(root,"gchandle_new");
    EXPORT(target,"gchandle_get_target"); EXPORT(unroot,"gchandle_free");
#undef EXPORT
    if(!domain_()) throw std::runtime_error("IL2CPP domain unavailable");
}
std::string Managed::type(const void* t) {
    char* name=typeName_(t); if(!name) throw std::runtime_error("Invalid IL2CPP type");
    std::string result(name);free_(name);return result;
}
bool Managed::executable(void* code) const {
    MEMORY_BASIC_INFORMATION info{};
    if(!code || !VirtualQuery(code,&info,sizeof(info)) || info.AllocationBase!=module_ || info.State!=MEM_COMMIT)return false;
    auto p=info.Protect&0xff;
    return !(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && (p==PAGE_EXECUTE || p==PAGE_EXECUTE_READ || p==PAGE_EXECUTE_READWRITE || p==PAGE_EXECUTE_WRITECOPY);
}
void* Managed::klass(const char* image,const char* ns,const char* name) {
    size_t count=0;auto list=assemblies_(domain_(),&count);void* found=nullptr;
    if(!list || count>2048)throw std::runtime_error("Invalid assembly list");
    for(size_t i=0;i<count;++i) {
        auto im=image_(list[i]); const char* n=im?imageName_(im):nullptr;
        if(!n || std::strcmp(n,image))continue;
        auto candidate=class_(im,ns,name);
        if(found && candidate)throw std::runtime_error("Ambiguous managed class");
        found=candidate;
    }
    if(!found)throw std::runtime_error("Required managed class missing: "+std::string(name));
    return found;
}
Method Managed::method(void* cls,const char* name,const char* result,bool isStatic,std::initializer_list<const char*> args) {
    Method found;void* iterator=nullptr;
    while(auto info=methods_(cls,&iterator)) {
        if(std::strcmp(methodName_(info),name) || argc_(info)!=args.size() || type(ret_(info))!=result)continue;
        uint32_t impl=0;
        if(bool(flags_(info,&impl)&0x10)!=isStatic)continue;
        bool match=true;uint32_t index=0;
        for(auto arg:args) if(type(param_(info,index++))!=arg)match=false;
        if(!match)continue;
        if(found.info)throw std::runtime_error("Ambiguous managed method");
        void* code=nullptr;std::memcpy(&code,info,sizeof(code));
        if(!executable(code))throw std::runtime_error("Managed entry is not executable GameAssembly memory");
        found={info,code};
    }
    if(!found.info)throw std::runtime_error("Managed method signature mismatch: "+std::string(name));
    return found;
}
void* Managed::booleanField(void* cls,const char* name) {
    void* f=field(cls,name);
    if(!f || type(fieldType_(f))!="System.Boolean" || (fieldFlags_(f)&0x10))throw std::runtime_error("Managed boolean field mismatch");
    return f;
}
void* Managed::referenceField(void* cls,const char* name,const char* expected) {
    void* f=field(cls,name);
    if(!f || type(fieldType_(f))!=expected || (fieldFlags_(f)&0x10))
        throw std::runtime_error("Managed reference field mismatch: "+std::string(name));
    return f;
}
void* Managed::mutableBooleanField(void* cls,const char* name) {
    auto f=booleanField(cls,name);
    if(fieldFlags_(f)&(0x20|0x40))throw std::runtime_error("Managed boolean field is not mutable");
    return f;
}
