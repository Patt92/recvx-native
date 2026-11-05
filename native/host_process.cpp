#include "host_process.h"
#include <cstdlib>
#include <stdexcept>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
namespace recvx {
#if defined(_WIN32)
void HostProcess::start(const std::string& program,const std::vector<std::string>& arguments,const std::filesystem::path& cwd,const std::filesystem::path& log){
    stop();
    std::string command="\""+program+"\"";for(const auto& a:arguments)command+=" \""+a+"\"";
    SECURITY_ATTRIBUTES sa{};sa.nLength=sizeof sa;sa.bInheritHandle=TRUE;
    HANDLE out=CreateFileA(log.string().c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    STARTUPINFOA si{};si.cb=sizeof si;si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=out;si.hStdError=out;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    const BOOL ok=CreateProcessA(nullptr,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,cwd.string().c_str(),&si,&pi);
    if(out!=INVALID_HANDLE_VALUE)CloseHandle(out);
    if(!ok)throw std::runtime_error("cannot start "+program);
    CloseHandle(pi.hThread);handle_=reinterpret_cast<std::intptr_t>(pi.hProcess);pid_=long(pi.dwProcessId);alive_=true;exit_code_=-1;
}
bool HostProcess::running(){
    if(!alive_)return false;
    DWORD code=0;if(GetExitCodeProcess(reinterpret_cast<HANDLE>(handle_),&code)&&code!=STILL_ACTIVE){exit_code_=int(code);alive_=false;CloseHandle(reinterpret_cast<HANDLE>(handle_));handle_=0;}
    return alive_;
}
void HostProcess::stop(){
    if(!running())return;
    TerminateProcess(reinterpret_cast<HANDLE>(handle_),1);WaitForSingleObject(reinterpret_cast<HANDLE>(handle_),2000);CloseHandle(reinterpret_cast<HANDLE>(handle_));handle_=0;alive_=false;exit_code_=1;
}
std::string find_python(){
    for(const char* name:{"python3.exe","python.exe","py.exe"}){char found[MAX_PATH]{};if(SearchPathA(nullptr,name,nullptr,MAX_PATH,found,nullptr))return found;}
    return {};
}
#else
void HostProcess::start(const std::string& program,const std::vector<std::string>& arguments,const std::filesystem::path& cwd,const std::filesystem::path& log){
    stop();
    std::vector<std::string> storage{program};storage.insert(storage.end(),arguments.begin(),arguments.end());
    std::vector<char*> argv;for(auto& s:storage)argv.push_back(s.data());argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions,1,log.string().c_str(),O_WRONLY|O_CREAT|O_APPEND,0644);
    posix_spawn_file_actions_adddup2(&actions,1,2);
#if defined(__APPLE__) || defined(__GLIBC__)
    posix_spawn_file_actions_addchdir_np(&actions,cwd.string().c_str());
#endif
    pid_t pid=0;const int rc=posix_spawnp(&pid,program.c_str(),&actions,nullptr,argv.data(),environ);
    posix_spawn_file_actions_destroy(&actions);
    if(rc!=0)throw std::runtime_error("cannot start "+program+": "+std::string(std::strerror(rc)));
    pid_=long(pid);alive_=true;exit_code_=-1;
}
bool HostProcess::running(){
    if(!alive_)return false;
    int status=0;const pid_t r=waitpid(pid_t(pid_),&status,WNOHANG);
    if(r==pid_t(pid_)){alive_=false;exit_code_=WIFEXITED(status)?WEXITSTATUS(status):-1;}
    return alive_;
}
void HostProcess::stop(){
    if(!running())return;
    kill(pid_t(pid_),SIGTERM);for(int i=0;i<40&&running();++i)usleep(50000);
    if(running()){kill(pid_t(pid_),SIGKILL);int status=0;waitpid(pid_t(pid_),&status,0);alive_=false;exit_code_=-1;}
}
std::string find_python(){
    const char* path=std::getenv("PATH");std::string dirs=path?path:"/usr/local/bin:/usr/bin:/bin";
    for(const char* name:{"python3","python"}){std::size_t start=0;while(start<=dirs.size()){const auto end=dirs.find(':',start);const auto dir=dirs.substr(start,end==std::string::npos?std::string::npos:end-start);
        if(!dir.empty()){const auto candidate=std::filesystem::path(dir)/name;std::error_code ec;if(std::filesystem::is_regular_file(candidate,ec)&&access(candidate.c_str(),X_OK)==0)return candidate.string();}
        if(end==std::string::npos)break;start=end+1;}}
    return {};
}
#endif
}
