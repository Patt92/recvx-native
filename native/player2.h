#pragma once
namespace recvx {
// User-requested host extension, disabled by default. These commands feed the
// existing native player action port; a driver supplies intent, never a pose.
enum class Player2Mode {Off,Local,Npc};
struct Player2Command {int move=0,turn=0;bool run=false;};
}
