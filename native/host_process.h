#pragma once
// Host helper: start and stop a child process (the AI-play driver started from the debug menu).
// No game logic; stdout/stderr go to a log file, the child runs detached from the game tick.
#include <filesystem>
#include <string>
#include <vector>
namespace recvx {
class HostProcess {
public:
    HostProcess()=default;
    ~HostProcess(){stop();}
    HostProcess(const HostProcess&)=delete;HostProcess& operator=(const HostProcess&)=delete;
    // Throws when the program cannot be started. `log` receives stdout and stderr (appended).
    void start(const std::string& program,const std::vector<std::string>& arguments,const std::filesystem::path& cwd,const std::filesystem::path& log);
    bool running();   // reaps a finished child
    void stop();      // terminates the child if it still runs
    int exit_code()const{return exit_code_;}
private:
    std::intptr_t handle_=0;long pid_=0;bool alive_=false;int exit_code_=-1;
};
// python3 on PATH (or python on Windows) for the driver; empty when none is found.
std::string find_python();
}
