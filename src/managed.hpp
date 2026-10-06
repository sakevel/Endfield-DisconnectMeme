#pragma once
#include <Windows.h>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>

struct Method {const void* info=nullptr; void* code=nullptr;};
class Managed {
    HMODULE module_ = nullptr;
    template<class T> void load(T& fn,const char* name) {
        fn=reinterpret_cast<T>(GetProcAddress(module_,name));
        if(!fn) throw std::runtime_error("Required IL2CPP export missing");
    }
    void* (*domain_)()=nullptr;
    const void** (*assemblies_)(void*,size_t*)=nullptr;
    void* (*image_)(const void*)=nullptr;
    const char* (*imageName_)(void*)=nullptr;
    void* (*class_)(void*,const char*,const char*)=nullptr;
    const void* (*methods_)(void*,void**)=nullptr;
    const char* (*methodName_)(const void*)=nullptr;
    uint32_t (*argc_)(const void*)=nullptr;
    const void* (*param_)(const void*,uint32_t)=nullptr;
    const void* (*ret_)(const void*)=nullptr;
    uint32_t (*flags_)(const void*,uint32_t*)=nullptr;
    char* (*typeName_)(const void*)=nullptr;
    void (*free_)(void*)=nullptr;
    const void* (*fieldType_)(void*)=nullptr;
    int (*fieldFlags_)(void*)=nullptr;
    bool executable(void* code) const;
    std::string type(const void* t);
public:
    void* (*objectClass)(void*)=nullptr;
    void* (*field)(void*,const char*)=nullptr;
    void (*getField)(void*,void*,void*)=nullptr;
    void (*setField)(void*,void*,void*)=nullptr;
    void* (*invoke)(const void*,void*,void**,void**)=nullptr;
    void* (*stringNew)(const char*)=nullptr;
    uint32_t (*root)(void*,bool)=nullptr;
    void* (*target)(uint32_t)=nullptr;
    void (*unroot)(uint32_t)=nullptr;
    void initialize();
    void* klass(const char* image,const char* ns,const char* name);
    Method method(void* klass,const char* name,const char* result,bool isStatic,std::initializer_list<const char*> args);
    void* booleanField(void* klass,const char* name);
    void* mutableBooleanField(void* klass,const char* name);
    void* referenceField(void* klass,const char* name,const char* expected);
    void* reference(void* object,void* descriptor) const {void* value=nullptr;getField(object,descriptor,&value);return value;}
    bool boolean(void* object,void* descriptor) const {bool value=false;getField(object,descriptor,&value);return value;}
};
