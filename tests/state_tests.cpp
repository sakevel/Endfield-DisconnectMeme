#include "state.hpp"
#include "config.hpp"
#include <iostream>
#include <stdexcept>
void require(bool value){if(!value)throw std::runtime_error("Test failed");}
int main() {
    try {
        Performance p;
        require(!p.click(0));require(!p.login(0));require(p.begin(100,120));
        require(!p.begin(110,120));require(p.blocked(111));require(!p.click(112));
        require(p.login(200));require(!p.blocked(200));require(!p.login(201));
        require(p.click(202));require(!p.click(203));require(!p.blocked(204));
        require(p.begin(1000,15));require(p.blocked(15999));require(!p.blocked(16000));
        require(!p.login(16001));require(!p.click(16002));
        require(p.begin(20000,120));require(p.cancel());require(!p.blocked(20001));require(!p.click(20002));
        require(p.begin(30000,120));require(p.login(30001));require(!p.click(330001));
        require(p.begin(400000,120,3));require(!p.takeFailure(402999));require(p.takeFailure(403000));
        require(!p.takeFailure(403001));require(p.blocked(403002));require(p.login(403003));require(!p.takeFailure(403004));require(p.click(403005));
        require(p.begin(500000,15,3));require(p.cancel());require(!p.takeFailure(503000));
        require(p.begin(600000,15,3));require(!p.takeFailure(615000));
        require(p.begin(700000,120));require(!p.takeFailure(704000));require(p.cancel());
        Config c;
        require(parse_config("enabled=true\nhotkey=Ctrl+Shift+F11\ntimeout=15\nmessage=测试\\n换行\nshow_notice=false\n",c));
        require(c.key==VK_F11 && c.timeout==15 && c.text()=="测试\n换行");
        require(!parse_config("message=<b>bad</b>\n",c));require(!parse_config("timeout=181\n",c));
        require(!parse_config("timeout=60.5\n",c));require(!parse_config("hotkey=F10\n",c));
        require(parse_config("timeout=60.0\n",c)&&c.timeout==60);
        require(c.fastFailure&&c.failureDelay==3);
        require(parse_config("fast_failure=false\nfailure_delay=5.0\n",c)&&!c.fastFailure&&c.failureDelay==5);
        require(!parse_config("failure_delay=0\n",c));require(!parse_config("failure_delay=16\n",c));
        require(!parse_config("failure_delay=1.5\n",c));require(!parse_config("fast_failure=maybe\n",c));
        require(!parse_config("enabled=true\nenabled=false\n",c));
        require(!parse_config("message=\xff\n",c));require(!parse_config("[unknown]\nenabled=true\n",c));
        require(parse_config("enabled=false\n",c) && !c.enabled);
        require(c.text().find("非真实封禁")!=std::string::npos);
        std::cout<<"one-shot, login restoration, timeout, cancellation, stale arm and config validation passed\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
