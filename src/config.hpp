#pragma once
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <cmath>
#include <sstream>
#include <string>

struct Config {
    bool enabled = true;
    int key = VK_F9; // Default hotkey
    unsigned timeout = 120;
    bool fastFailure = true;
    unsigned failureDelay = 3;
    bool notice = true;
    std::string message = "由于您的账号存在违规行为，此账号已被封禁至\\n2099/12/31 23:59 (UTC+8)。如有疑问，请联系客服。\\n封禁理由：【骑乘摩托车未佩戴头盔】";
    std::string text() const {
        std::string out;
        for (size_t i=0; i<message.size(); ++i) {
            if (message[i]=='\\' && i+1<message.size() && message[i+1]=='n') {out+='\n'; ++i;}
            else out+=message[i];
        }
        if (notice) out += "\n（视频演出 · 非真实封禁）";
        return out;
    }
};
inline std::string trim(std::string s) {
    auto a=s.find_first_not_of(" \t\r");
    if(a==s.npos) return {};
    return s.substr(a,s.find_last_not_of(" \t\r")-a+1);
}
inline bool parse_config(const std::string& bytes, Config& out) {
    if(bytes.size()>65536) return false;
    Config next; std::string line, section; std::map<std::string,std::string> fields;
    std::istringstream input(bytes);
    while(std::getline(input,line)) {
        line=trim(line);
        if(line.empty() || line[0]==';' || line[0]=='#') continue;
        if(line.front()=='[' && line.back()==']') {section=line.substr(1,line.size()-2);continue;}
        if(!section.empty() && section!="config") return false;
        auto at=line.find('='); if(at==line.npos) return false;
        auto key=trim(line.substr(0,at)), value=trim(line.substr(at+1));
        if(!fields.emplace(key,value).second) return false;
    }
    for(auto& [key,value]:fields) {
        if(key=="enabled" || key=="show_notice" || key=="fast_failure") {
            if(value!="true" && value!="false") return false;
            (key=="enabled" ? next.enabled : key=="fast_failure" ? next.fastFailure : next.notice) = value=="true";
        } else if(key=="hotkey") {
            if(value=="Ctrl+Shift+F9") next.key=VK_F9;
            else if(value=="Ctrl+Shift+F11") next.key=VK_F11;
            else if(value=="Ctrl+Shift+F12") next.key=VK_F12;
            else return false;
        } else if(key=="timeout" || key=="failure_delay") {
            try {size_t consumed=0; auto number=std::stod(value,&consumed);
                auto minimum=key=="timeout"?15:1, maximum=key=="timeout"?180:15;
                if(consumed!=value.size() || !std::isfinite(number) || number<minimum || number>maximum || std::abs(number-std::round(number))>1e-6) return false;
                (key=="timeout" ? next.timeout : next.failureDelay)=static_cast<unsigned>(number);
            } catch(...) {return false;}
        } else if(key=="message") {
            if(value.empty() || value.size()>900) return false;
            for(unsigned char c:value) if(c<32 || c==127 || c=='<' || c=='>') return false;
            if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0)) return false;
            next.message=value;
        }
    }
    out=next; return true;
}
inline bool read_config(const std::filesystem::path& path, Config& out) {
    std::error_code ec;
    if(!std::filesystem::exists(path,ec)) {if(ec)return false;out=Config{};return true;}
    auto size=std::filesystem::file_size(path,ec); if(ec || size>65536) return false;
    std::ifstream f(path,std::ios::binary); if(!f)return false;
    std::string bytes((std::istreambuf_iterator<char>(f)),{});
    if(f.bad() || bytes.size()!=size)return false;
    return parse_config(bytes,out);
}
