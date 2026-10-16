// objitm.c:1213 bhObj005 (bhJumpObject[5]): die Wasserflaeche eines Raums. Semantikreferenz
// ist die C-Fassung mit den beiden Inline-Asm-Bloecken (gegen das ELF geprueft,
// tools/disasm_ps2.py bhObj005 0x285C14-0x285D88) und die VU0-Mikroprogramme
// VU0_WAVE_INIT/VU0_WAVE_CALC (ps2_vu0.vsm:482-505):
//   INIT  vf04 = vf05 = (0, -1, 0, 0)
//   CALC  vf04.x = h[c+zp]   vf05.x = -h[c-zp]   vf04.z = -h[c-1]   vf05.z = h[c+1]
//         vf10 = vf04 + vf05 = (h[c+zp]-h[c-zp], -2, h[c+1]-h[c-1])
//         Q = 1 / (|vf10.x| + |vf10.y| + |vf10.z|);  vf10.xz = -vf10.xz;  (EE) vf10.xyz *= Q
// Der EE-Block schreibt je Durchlauf die Normale eines Eckpunkts (sqc2 auf +16 der Kopie)
// und das y des naechsten (Original-y + Zellhoehe); gerade Passes (gfrm_ct & 1 == 0)
// beginnen mit dem y von Eckpunkt 0 allein. Danach klingt die Karte in 16er-Gruppen um
// 0.2 je Pass auf mindestens 0 ab; der Rest hinter der letzten vollen Gruppe wird gelesen,
// aber nicht zurueckgeschrieben (.L00285D6C ohne sqc2).
#include "world.h"
#include <algorithm>
#include <cmath>
namespace recvx {
namespace {
// objitm.c:1322-1372: npSetOffsetUV2(model, u, v) je ct3; 256/1024 ist eine Kachel eines 4x4-Atlas.
const int kUvStep[16][2]={{256,0},{256,0},{256,0},{-768,256},{256,0},{256,0},{256,0},{-768,256},
                          {256,0},{256,0},{256,0},{-768,256},{256,0},{256,0},{256,0},{-768,-768}};
}
const WaterWaveWork* RoomWorld::water_work(unsigned object)const{
    const auto e=entities_.find({2,object});return e!=entities_.end()&&e->second.water?e->second.water.get():nullptr;
}
void RoomWorld::control_water_objects(){
    ++object_pass_count_; // system.c:556 sys->gfrm_ct++ je Hauptschleife
    // objitm.c:289-362 bhControlObjItm: nur mit sp_flg 0x4; mit cb_flg 0x40000000 die ersten
    // vier Objekte; ein Werk laeuft ohne stflg 0x1000000 (oder mit pl_sleep_cnt) und mit flg 0x1.
    if(!(simulation_flags_&4u))return;
    const unsigned limit=collision_events_&&(collision_events_->flags[10][0]&0x40000000u)?4u:~0u;
    for(auto& pair:entities_){if(pair.first.first!=2)continue;auto& e=pair.second;const unsigned index=pair.first.second;
        if((e.type&0xffu)!=5||index>=limit||(entity_hidden(2,index)&&!pl_sleep_cnt_)||!(e.object_flags&1u))continue;
        if(!e.water)e.water=std::make_shared<WaterWaveWork>(); // AllocOwork: mode0/ct0..ct3 = 0
        control_water_object(index,e);
    }
}
void RoomWorld::control_water_object(unsigned index,WorldEntity& e){
    auto& w=*e.water;auto& wt=water_wave();
    unsigned* st_flg=collision_events_?&collision_events_->flags[5][0]:nullptr;
    switch(w.mode0){
    case 0:{
        if(!st_flg){unbound_enemy_.insert("bhObj005: sys->st_flg ohne EventVm");return;}
        if(*st_flg&0x40u){
            // objitm.c:1230-1235: mit stehendem st_flg 0x40 nur npSetMemoryL(wt_wvp, wt_nbpt, 0)
            // und mode0 1 - ohne eigene exp1/exp2 (mode 1 liest im Original NULL).
            if(wt.allocated)std::fill(wt.wvp.begin(),wt.wvp.end(),0.0f);
            else unbound_enemy_.insert("bhObj005: npSetMemoryL auf wt_wvp des Vorraums (objitm.c:1232)");
            w.orphan=true;w.mode0=1;unbound_enemy_.insert("bhObj005: zweites Wasserobjekt ohne exp1/exp2 (objitm.c:1230)");return;}
        const Model* model=nullptr;for(const auto& m:models_)if(m.kind==2&&m.index==index)model=&m;
        if(!model||model->actor){unbound_enemy_.insert("bhObj005: Wasserobjekt ohne Chunkmodell");return;}
        const auto chunk=model_object_vertices(room_,model->source_model,0);
        if(!chunk||chunk->positions.empty()){unbound_enemy_.insert("bhObj005: objP[0].model->vlist fehlt");return;}
        // objitm.c:1244-1268: usIndexMax Eckpunkte, xn/zn das groesste px+x bzw. pz+z.
        const auto& ps=chunk->positions;const int nb=int(ps.size());w.ct2=nb;w.chunk_type=chunk->chunk_type;
        const float px=e.transform.position.x,pz=e.transform.position.z;
        w.xn=w.zn=-10000.0f;w.ct0=w.ct1=0;
        for(int i=0;i<nb;++i){if(w.xn<px+ps[i].x){w.xn=px+ps[i].x;w.ct0++;}if(w.zn<pz+ps[i].z){w.zn=pz+ps[i].z;w.ct1++;}}
        // objitm.c:1270-1289: Karte (xn-px+2) x (zn-pz+2), bhGetFreeMemory loescht (pwksub.c:29).
        const int wt_xp=int(w.xn-px)+2,wt_zp=int(w.zn-pz)+2;
        wt.nbpt=wt_xp*wt_zp;wt.px=px;wt.pz=pz;wt.xp=wt_xp;wt.zp=wt_zp;wt.minx=0;wt.minz=0;wt.maxx=wt_xp;wt.maxz=wt_zp;
        wt.wvp.assign(std::size_t(std::max(wt.nbpt,0)),0.0f);wt.allocated=true;
        // objitm.c:1291-1315: exp1 = Original-vlist, exp2 = Kopie (gezeichnet), je Eckpunkt
        // exp1->x = &wt_wvp[(int)z + (int)x * wt_zp].
        w.base=ps;w.base_normals=chunk->normals;w.vertices=ps;w.normals=chunk->normals;
        w.cell.resize(ps.size());
        for(int i=0;i<nb;++i){const int xp=int(ps[i].x),zp=int(ps[i].z);w.cell[i]=zp+(xp*wt.zp);}
        // objitm.c:1325 npSetOffsetUV2 auf objP[0].model: sein Lauf ueber den plist.
        const auto walk=model_object_uv_walk(room_,model->source_model,0);
        if(walk&&walk->found&&walk->proper&&walk->usr_num==0){w.uv_ok=true;w.uv_chunk=walk->chunk;}
        else unbound_enemy_.insert(!walk||!walk->found?"bhObj005: npSetOffsetUV2 findet keinen Chunk 66 (njplus.c:1666)"
                                   :!walk->proper?"bhObj005: npSetOffsetUV2 landet auf einem Fremdkopf (njplus.c:1662)"
                                   :"bhObj005: npSetOffsetUV2 mit usr_num != 0 (njplus.c:1699, 6-Byte-Schritt)");
        *st_flg|=0x40u;w.mode0=1;break;}
    case 1:{
        if(w.orphan)return;
        // objitm.c:1322-1374: UV-Versatz je ct3 (short-Arithmetik), dann ct3 weiter.
        if(w.uv_ok){w.uv_u=std::int16_t(w.uv_u+kUvStep[w.ct3&15][0]);w.uv_v=std::int16_t(w.uv_v+kUvStep[w.ct3&15][1]);}
        w.ct3=(w.ct3+1)&0xF;
        // Erster Asm-Block: j = gfrm_ct & 1, t0 = nb >> 1, t1 = wt_zp Zellen je x-Schritt.
        const int nb=w.ct2;const unsigned j=(object_pass_count_-1u)&1u;
        const auto height=[&](int cell)->float{
            if(cell>=0&&cell<wt.nbpt)return wt.wvp[std::size_t(cell)];
            // Nachbarzelle vor/hinter wt_wvp: das Original liest den Nachbarblock des Heaps.
            if(!w.neighbour_gap){w.neighbour_gap=true;unbound_enemy_.insert("bhObj005: Nachbarzelle ausserhalb wt_wvp (Original liest fremden Speicher)");}
            return 0.0f;};
        const auto in_range=[&](int v)->bool{
            if(v>=0&&v<nb)return true;
            if(!w.range_gap){w.range_gap=true;unbound_enemy_.insert("bhObj005: Eckpunktpaar hinter usIndexMax (Original liest hinter dem vlist)");}
            return false;};
        int t0=nb>>1;int v=0;
        if(j==0){if(in_range(0))w.vertices[0].y=w.base[0].y+height(w.cell[0]);t0--;v=1;}
        do{
            // VU0_WAVE_CALC fuer Eckpunkt v, y fuer Eckpunkt v+1 (do-while wie .L00285C6C).
            if(in_range(v)){const int c=w.cell[v];
                const float a=height(c-1),b=height(c+wt.zp),cc=height(c+1),d=height(c-wt.zp);
                const float x=b-d,z=cc-a;const float l1=std::fabs(x)+2.0f+std::fabs(z);const float q=1.0f/l1;
                w.normals[std::size_t(v)]={-x*q,-2.0f*q,-z*q};}
            if(in_range(v+1))w.vertices[std::size_t(v+1)].y=w.base[std::size_t(v+1)].y+height(w.cell[std::size_t(v+1)]);
            v+=2;--t0;
        }while(t0>0);
        // Zweiter Asm-Block: Gruppen zu 16 Zellen (do-while .L00285D1C), h = max(h - 0.2, 0).
        int groups=(wt.nbpt>>2)>>2;int g=0;
        do{for(int k=0;k<16;++k){const int c=g*16+k;if(c<wt.nbpt)wt.wvp[std::size_t(c)]=std::max(wt.wvp[std::size_t(c)]-0.2f,0.0f);
                else if(!w.range_gap){w.range_gap=true;unbound_enemy_.insert("bhObj005: Abklinggruppe hinter wt_nbpt (Original schreibt hinter wt_wvp)");}}
            ++g;--groups;}while(groups>0);
        break;}
    }
}
}
