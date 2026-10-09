#include "player_push.h"
#include <iostream>
#include <stdexcept>
using namespace recvx;
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test prüfte die originale
// Spielerkollision und den Schiebeablauf aus player_push.inc — bhCheckWall,
// bhCheckWallType, bhCheckFloorP, bhCheckDansa, bhObj001 (inklusive der
// dynamischen ETC-/Boden-Einträge der Kisten), bhCPM2_act_psh mit
// MTN49/MTN50/Rücklauf, die Attributaktion 0x40 sowie die darauf aufbauenden
// RoomWorld-Prüfungen (htp-Besitz, RM_0100/RM_2070-Kisten, geteilter
// Effektpool). Generator und Include sind Quarantäne, siehe
// native/player_push_core.c.
//
// Diese Abdeckung muss zurückkehren, sobald die Kollision eigenständig
// nachgebaut ist. Geprüft bleibt nur, dass die Lücke sich wie dokumentiert
// verhält: keine Kollisionsauflösung (die vorgeschlagene Zielposition bleibt
// unverändert), keine beweglichen Wände/Stufen/Bodenflächen, keine
// Schiebeaktion und kein unerreichter Dienst.
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    _Make_SinTable();check(argc==2,"data path required");
    const auto data=std::filesystem::path(argv[1]);auto asset=PlayerAsset::load(data);
    std::array<float,32> floors{};floors[0]=-10000;floors[1]=-1000;floors[3]=1000;
    const CollisionWall solid{1,0,0,0,0,{0,0,0},{10,0,10}};
    const std::vector<RECVX_PUSH_OBJECT> objects{{4,1,0,0,0,0,0,0}};
    PlayerPush push({solid},100,floors,objects,asset,{});
    // bhCheckWall fehlt: die Kandidatenposition wird unverändert gemeldet.
    const auto resolved=push.resolve_wall({-1,0,5},0,0,3.5f,16.5f);
    check(resolved.x==-1.f&&resolved.y==0.f&&resolved.z==5.f,"wall gap must return the candidate position unchanged");
    check(!push.state().status&&!push.state().unsupported_services,"wall gap reported a hit or an unbound service");
    // bhObj001 fehlt: keine beweglichen Wände, Stufen oder Bodenflächen.
    check(push.walls().empty()&&push.steps().empty()&&push.floors().empty(),"object gap published mobile collision records");
    // Die Objektposition ist reine Buchführung über die Host-Eingabe.
    check(push.object(0).x==0.f&&push.object(0).z==0.f,"object gap invented a crate position");
    push.update_object(0,{30,0,40},false);
    check(push.object(0).x==30.f&&push.object(0).z==40.f,"object bookkeeping lost the host position");
    // bhCPM2_act_psh fehlt: der Kontaktzähler bleibt null, der Schiebeablauf
    // startet nie, und die Kollision bewegt den Spieler nicht.
    SceneSpawn actor;actor.position={0,0,8.3f};actor.yaw=0;std::size_t motion=0;std::int64_t frame=5*65536;
    for(unsigned t=0;t<30;++t){const NJS_POINT3 from=actor.position,to{from.x,from.y,from.z-.5f};
        const auto next=push.contact(from,to,0,0,true,0,0,motion,frame);
        check(next.x==to.x&&next.y==to.y&&next.z==to.z,"contact gap altered the candidate position");
        actor.position=next;}
    check(!push.active()&&!push.state().contact,"push gap entered the original push state");
    check(!push.tick(true,0,0,0,actor,motion,frame),"inactive push gap ran a tick");
    check(!push.state().unsupported_services,"push gap reported an unbound service");
    std::cout<<"Player collision/push gap reports the documented neutral state; the original mechanism is not ported.\n";
    return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
