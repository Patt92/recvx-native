// event.c:10960-10990 Common_controll 25 Selektor 0: work->mnwP = sys->plmthp, mode1 = Bewegungsnummer.
// Statischer Skriptscan (tools/scan_script_reach.py, Arbeitsart je bhLoadWork): Selektor 0 steht nur am
// Spielerwerk (252 Stellen), Selektor 1 nur an Gegnerwerken (171), 2/3 nirgends. Die Bewegungsnummern der
// Selektor-0-Stellen sind {0,4,9,10,21,22,39,42,45,46,52}; jede muss in jeder Spielerbibliothek liegen
// (SYSTEM-Eintraege ply_id + 10 + costume * 4), sonst laese das Original hinter plmthp.
#include "actor.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("data required");
    const unsigned used[]={0,4,9,10,21,22,39,42,45,46,52};
    for(unsigned entry:{10u,11u,12u,13u,14u}){
        PlayerAsset p=PlayerAsset::load(argv[1],entry);
        for(unsigned m:used)if(!p.has_motion(m))throw std::runtime_error("SYSTEM "+std::to_string(entry)+" hat keine Bewegung "+std::to_string(m)+" (Bibliothek "+std::to_string(p.motions())+")");
        std::cout<<"SYSTEM "<<entry<<": "<<p.motions()<<" Bewegungen\n";
    }
    std::cout<<"common25 player motion PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"common25: "<<e.what()<<'\n';return 1;}}
