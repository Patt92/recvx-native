// effsub6.c:18 bhEff085 (Raumeffekt 57 in RM_9370) auf den Effektdiensten ohne Raum: mode0 0 nullt
// mode1..3 (ein vorher gesetztes mode1 geht verloren), mode1 1 tauscht ene[lkno].mlwP->texP->
// textures[3].texaddr gegen das erste Bild der Effektgruppe op->id und sichert den alten in exp0,
// mode1 2 schreibt exp0 zurueck, beide setzen mode1 = 0; jeder andere mode1 bleibt stehen. Nur
// rom->effp[op->type].lkflg 2 (Raumgegner) wirkt. Der Satz zeichnet nie. Keine Zufallsaufrufe:
// die Folge wird gegen den unveraenderten Zufallszustand geprueft.
#include "effect_sprites.h"
#include "core_random.h"
#include "ninja_math.h"
#include <iostream>
#include <memory>
#include <map>
#include <stdexcept>
#include <vector>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static SceneTexture texture(unsigned width){SceneTexture t;t.width=width;t.height=1;return t;}
int main(){try{_Make_SinTable();
    check(supported_effect_sprite(85),"85 nicht als Handport registriert");
    check(ported_effect_atlas(85),"Effektgruppe 85 (sys->ef_tn[85]) wird nicht geladen");
    // Raumtabelle: Satz 57 -> lkflg 2, lkno 1 (RM_9370 rom->effp[57]); Satz 3 -> lkflg 3 (Objekt).
    std::map<unsigned,std::pair<int,int>> table{{57,{2,1}},{3,{3,0}}};
    // ene[1].mlwP->texP: vier Eintraege, Breite = Kennung; Gruppe 85: erstes Bild Breite 850.
    std::vector<SceneTexture> enemy1{texture(10),texture(11),texture(12),texture(13)};
    unsigned sets=0;
    {EffectWorldHooks h;
     h.room_effect_link=[&](unsigned type,int& lkflg,int& lkno){const auto at=table.find(type);if(at==table.end())return false;lkflg=at->second.first;lkno=at->second.second;return true;};
     h.enemy_texaddr_get=[&](unsigned enemy,unsigned index)->std::shared_ptr<const SceneTexture>{if(enemy!=1||index>=enemy1.size())return nullptr;return std::make_shared<const SceneTexture>(enemy1[index]);};
     h.enemy_texaddr_set=[&](unsigned enemy,unsigned index,const std::shared_ptr<const SceneTexture>& t){if(enemy!=1||index>=enemy1.size()||!t)return false;enemy1[index]=*t;++sets;return true;};
     h.effect_texaddr=[&](unsigned group)->std::shared_ptr<const SceneTexture>{if(group!=85)return nullptr;return std::make_shared<const SceneTexture>(texture(850));};
     set_effect_world_hooks(std::move(h));}
    EffectContext ctx;set_effect_context(ctx);take_effect_output();
    recvx_core_random_seed(0x1234u);const int random_first=recvx_core_random_next();recvx_core_random_seed(0x1234u);
    EffectSpawn spawn;spawn.flg=1;spawn.id=85;spawn.type=57;
    EffectSprite s;initialize_effect_sprite(s,spawn);
    const auto pass=[&](EffectSprite& e){bool q=true;check(step_effect_sprite(e,ctx,&q),"85: Prozedur nicht ausgefuehrt");check(!q,"85: der Satz darf in keine Zeichenliste");};
    // Pass 1 (mode0 0, :24-33): ein vor dem ersten Pass gesetztes mode1 wird genullt, kein Tausch.
    s.mode1=1;s.mode2=5;s.mode3=6;s.stflg=0x1000000u;
    pass(s);
    check(s.mode0==1&&s.mode1==0&&s.mode2==0&&s.mode3==0&&s.stflg==0&&(s.flg&1u),"85 mode0 0: flg|1, stflg 0, mode0 1, mode1..3 0");
    check(sets==0&&enemy1[3].width==13,"85 mode0 0: kein Texturtausch");
    // Pass 2/3: mode1 0 und 3 wirken nicht und bleiben stehen (:35/:52 ohne else).
    pass(s);check(s.mode1==0&&sets==0,"85 mode1 0 wirkt");
    s.mode1=3;pass(s);check(s.mode1==3&&sets==0&&enemy1[3].width==13,"85 mode1 3 darf weder tauschen noch zuruecksetzen");
    // Pass 4: 0x91 setzt mode1 1 (RM_9370 Ereignis 2 @0fb0) -> textures[3] = Gruppe 85, exp0 = alter Eintrag.
    s.mode1=1;pass(s);
    check(s.mode1==0&&enemy1[3].width==850&&s.exp0_texaddr&&s.exp0_texaddr->width==13,"85 mode1 1: textures[3] <- ef_tn[85], exp0 <- alter texaddr, mode1 0");
    check(enemy1[0].width==10&&enemy1[1].width==11&&enemy1[2].width==12,"85 mode1 1 beruehrt nur textures[3]");
    // Pass 5: 0x91 mode1 2 (Ereignis 0 @07ee) -> zurueck; danach 0x91 mode1 0 (@07f2) wirkt nicht.
    s.mode1=2;pass(s);check(s.mode1==0&&enemy1[3].width==13,"85 mode1 2: textures[3] <- exp0, mode1 0");
    s.mode1=0;pass(s);check(enemy1[3].width==13&&sets==2,"85 mode1 0 nach dem Zuruecksetzen wirkt");
    // Zweimal mode1 1: das zweite sichert den schon getauschten Eintrag; mode1 2 stellt dann 85 her (Original).
    s.mode1=1;pass(s);s.mode1=1;pass(s);check(s.exp0_texaddr->width==850,"85: zweites mode1 1 sichert den getauschten Eintrag");
    s.mode1=2;pass(s);check(enemy1[3].width==850,"85: mode1 2 schreibt exp0 so zurueck, wie es gesichert wurde");
    check(take_effect_output().gaps.empty(),"85: Luecke im Regelpfad gemeldet");
    // lkflg 3 (Objekt): weder Tausch noch Sicherung, mode1 wird trotzdem 0.
    {enemy1[3]=texture(13);const auto before=sets;spawn.type=3;EffectSprite o;initialize_effect_sprite(o,spawn);pass(o);o.mode1=1;pass(o);
     check(o.mode1==0&&!o.exp0_texaddr&&sets==before&&enemy1[3].width==13,"85 lkflg 3: kein Tausch, mode1 0");
     o.mode1=2;pass(o);check(o.mode1==0&&sets==before,"85 lkflg 3 mode1 2: kein Schreiben");}
    // mode1 2 ohne vorheriges 1: exp0 ist NULL (AllocOwork) -> benannte Luecke statt eines Ersatzes.
    {spawn.type=57;EffectSprite n;initialize_effect_sprite(n,spawn);pass(n);n.mode1=2;pass(n);
     const auto gaps=take_effect_output().gaps;
     check(n.mode1==0&&enemy1[3].width==13&&gaps.size()==1&&gaps.begin()->find("texaddr NULL")!=std::string::npos,"85 mode1 2 mit exp0 NULL: Luecke, kein Schreiben");}
    // type ausserhalb der Raumtabelle: Luecke.
    {spawn.type=200;EffectSprite x;initialize_effect_sprite(x,spawn);pass(x);x.mode1=1;pass(x);
     check(x.mode1==0&&!take_effect_output().gaps.empty(),"85 type ausserhalb rom->eff_n: keine Luecke");}
    check(recvx_core_random_next()==random_first,"85 darf rand() nicht aufrufen");
    std::cout<<"bhEff085: mode0 0 nullt mode1, mode1 1 tauscht textures[3] gegen ef_tn[85] und sichert exp0, mode1 2 stellt her; nur lkflg 2; kein rand().\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
