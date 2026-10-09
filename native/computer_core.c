/* Special-event computer terminal (sp_evt.c semantics) as a native mechanism.

   The original keeps one COM_EVT_WORK in sys->com_exp and drives it from
   bhControlSpEvtComputer through three passes (init, main, exit). The main
   pass runs the window automaton (com_md1), the script interpreter and the
   soft keyboard, then builds a quad list that bhDrawSpEvtComputer submits with
   fixed priorities. This file carries the same work, the same state machine
   and the same priorities, and reports the result as a flat draw list plus the
   sound, flag and message requests of the pass. The eight scripts, their
   message tables and the accessory quads are data (computer_tables.c).

   Not modelled here: the global bhSetScreenFade of bhCheckSubTask/
   bhExitSpEvtComputer (the host keeps its own screen fade; the terminal's own
   fade below is the one the screen shows), the Ninja texture bookkeeping
   (njReleaseTexture/bhGarbageTexture/palbuf) and sys->memp, which have no
   native counterpart. */
#include "computer_core.h"
#include "ninja_math.h"
#include "core_random.h"
#include <math.h>
#include <string.h>

#define LINES 128

typedef struct {
    /* fade: bhSetSpEvtComFade/bhCalcSpEvtComFade */
    unsigned fcol; float fsrc,fdst,fcal,fcnt;
    int ct0;
    /* bhEntrySpEvtBoxLine: 128 slots, each alive for 240/30 passes */
    int lnfg[LINES]; float lpx[LINES],lpy[LINES],lvx[LINES],lvy[LINES]; short lnct[LINES];
    short mwin_alp,swin_alp,cwin_alp;
    short chr_no,mtn_no,frm_no;
    float cox,coy,coz;
    short cax,cay,caz,caxn,cayn,cazn;
    unsigned txt[RECVX_COM_ROWS][RECVX_COM_COLUMNS];
    unsigned txflg; int txwait,txmes;
    short tx,ty;
    unsigned txcol; int txsct,curct; unsigned lcflg;
    short kx,ky,khk,krep,kbn;
    signed char wordmax,wordn;
    short wdx,wdy;
    unsigned char word[2][16];
    /* ce->mesp / ce->scp / ce->sclb as offsets into the data tables */
    unsigned mesp,scp,sclb[32];
    int mes_no,mes_ln,flash;
    float bar[3][3]; int bar_ct[3];
} COM_WORK;

static COM_WORK ce;
static unsigned com_num,com_flg,com_md0,com_md1;
static RECVX_COM_SYSTEM com_sys;
static RECVX_COM_FRAME* out;

/* ---- frame output helpers ------------------------------------------- */
static void sound(unsigned se){if(out&&out->sound_count<16)out->sounds[out->sound_count++]=se;}
static void event_flag(unsigned no){if(out&&out->flag_count<16)out->flags[out->flag_count++]=no;}
static void quad(int texture,int trans,float x1,float y1,float x2,float y2,
                 float u1,float v1,float u2,float v2,float z,unsigned color){
    RECVX_COM_DRAW* d;
    if(!out)return;
    if(out->draw_count>=RECVX_COM_MAX_DRAW){out->draw_overflow++;return;}
    d=&out->draw[out->draw_count++];
    d->kind=0;d->trans=trans;d->texture=texture;
    d->x1=x1;d->y1=y1;d->x2=x2;d->y2=y2;d->u1=u1;d->v1=v1;d->u2=u2;d->v2=v2;d->z=z;d->color=color;
}
static void polygon(float x1,float y1,float x2,float y2,float z,unsigned color){
    RECVX_COM_DRAW* d;
    if(!out)return;
    if(out->draw_count>=RECVX_COM_MAX_DRAW){out->draw_overflow++;return;}
    d=&out->draw[out->draw_count++];
    d->kind=1;d->trans=1;d->texture=-1;
    d->x1=x1;d->y1=y1;d->x2=x2;d->y2=y2;d->u1=d->v1=d->u2=d->v2=0;d->z=z;d->color=color;
}

/* ---- data access ----------------------------------------------------- */
static const unsigned char* script(void){return recvx_com_script_tab[com_num&7];}
static unsigned script_size(void){return recvx_com_script_size[com_num&7];}
static unsigned char script_at(unsigned offset){return offset<script_size()?script()[offset]:0xFF;}
static unsigned char script_next(void){return script_at(ce.scp++);}
static const char* message(unsigned index){
    const unsigned n=recvx_com_message_count[com_num&7];
    return index<n?recvx_com_message_tab[com_num&7][index]:"";
}

/* ---- fade (bhSetSpEvtComFade / bhCalcSpEvtComFade / bhDrawSpEvtComFade) */
static void set_fade(float destination,float count){
    ce.fdst=destination;ce.fcal=(destination-ce.fsrc)/count;ce.fcnt=count;com_flg|=0x2u;
}
static void calc_fade(void){
    int nn;
    ce.fsrc+=ce.fcal;ce.fcnt-=1.0f;
    if(ce.fcnt<=0){com_flg&=~0x2u;ce.fsrc=ce.fdst;}
    nn=(int)(ce.fsrc*255.0f);
    if(nn<0)nn=0;
    if(nn>255)nn=255;
    ce.fcol=(unsigned)nn<<24;
}

/* ---- box lines (bhEntrySpEvtBoxLine / bhDrawSpEvtBoxLine) ------------- */
static void entry_box_line(float px,float py,float vx,float vy){
    int i;
    for(i=0;i<LINES;++i)if(!(ce.lnfg[i]&1)){
        ce.lnfg[i]=1;ce.lpx[i]=px;ce.lpy[i]=py;ce.lvx[i]=vx;ce.lvy[i]=vy;ce.lnct[i]=240;
        break;
    }
}
static void draw_box_lines(void){
    int i;
    for(i=0;i<LINES;++i)if(ce.lnfg[i]&1){
        /* The original builds the greyscale from the remaining life in all
           four channels, alpha included. */
        const unsigned c=(unsigned)ce.lnct[i]&0xFFu;
        polygon(ce.lpx[i],ce.lpy[i],ce.lpx[i]+ce.lvx[i],ce.lpy[i]+ce.lvy[i],-0.85f,
                (c<<24)|(c<<16)|(c<<8)|c);
        ce.lnct[i]=(short)(ce.lnct[i]-30);
        if(ce.lnct[i]<=0)ce.lnfg[i]=0;
    }
}

/* ---- text grid (bhClearComEvtText) ----------------------------------- */
static void clear_text(void){memset(ce.txt,0,sizeof ce.txt);}

/* ---- script prescan (bhInitComEvtScript) ------------------------------ */
static void init_script(void){
    unsigned p=ce.scp;
    int end=1;
    ce.txcol=0xFFFF00u;ce.txflg=0;
    while(end&&p<script_size()){
        const unsigned char cmd=script()[p++];
        switch(cmd){
        case 224:case 225:case 228:case 236:p+=1;break;
        case 227:p+=3;break;
        case 229:case 237:case 239:case 240:case 241:p+=2;break;
        case 234:p+=3;break;
        case 235:ce.sclb[script_at(p)&31]=p+1;p+=1;break;
        case 238:p+=4;break;
        case 255:end=0;break;
        default:break;
        }
    }
}

/* ---- soft keyboard (bhControlComEvtKeyboard) -------------------------- */
/* khk holds the direction the repeat lock is armed for; a direction moves on
   the pass it is first seen, then waits eight passes before repeating. */
static int keyboard_step(int id,int dx,int dy){
    if(ce.khk==id){if(ce.krep<8){ce.krep++;return 0;}}
    else {ce.khk=(short)id;ce.krep=0;}
    if(id==0&&ce.kx==9&&ce.ky==3)ce.ky-=2;          /* up out of the tall Enter key */
    else if(id==4&&ce.kx==9&&ce.ky==2)ce.ky+=2;     /* down out of the tall Enter key */
    else {ce.kx=(short)(ce.kx+dx);ce.ky=(short)(ce.ky+dy);}
    sound(0x8000026Fu);
    return 1;
}
static void keyboard(const RECVX_COM_INPUT* in){
    int i,sel,adt;
    switch(in->pad_on&0xF){
    case 1: keyboard_step(0, 0,-1);break;
    case 9: keyboard_step(1, 1,-1);break;
    case 8: keyboard_step(2, 1, 0);break;
    case 10:keyboard_step(3, 1, 1);break;
    case 2: keyboard_step(4, 0, 1);break;
    case 6: keyboard_step(5,-1, 1);break;
    case 4: keyboard_step(6,-1, 0);break;
    case 5: keyboard_step(7,-1,-1);break;
    default:ce.khk=-1;ce.krep=0;break;
    }
    if(ce.kx<0)ce.kx=9;
    if(ce.kx>9)ce.kx=0;
    if(ce.ky<0)ce.ky=3;
    if(ce.ky>3)ce.ky=0;
    if(in->pad_ps&0x800u){
        ce.txflg|=0x10u;                            /* one pass of key-press travel */
        sel=ce.kx+ce.ky*10;
        if(sel==10){                                /* ESC */
            ce.ct0=0;com_md1=20;
        } else if(sel==19){                         /* BS */
            if(ce.kbn>0){
                sound(0x8000026Cu);
                ce.kbn--;
                ce.word[ce.wordn&1][ce.kbn]=0;
                ce.tx--;
                ce.txt[ce.ty][ce.tx]=ce.txcol;
            }
        } else if(sel==29||sel==39){                /* Enter spans both rows */
            sound(0x8000026Du);
            ce.txflg&=~0x8u;
            ce.wordn++;
            ce.kbn=0;
        } else {
            sound(0x8000026Bu);
            if(ce.kbn<ce.wordmax){
                if(sel<10)adt=((sel+1)%10)+16;      /* 1..9,0 */
                else {
                    if(sel<19)adt=sel-11;           /* A..H */
                    else if(sel<29)adt=sel-12;      /* I..Q */
                    else adt=sel-13;                /* R..Z */
                    adt+=33;
                }
                ce.txt[ce.ty][ce.tx]=ce.txcol|(unsigned)adt;
                if(ce.wordn>=0&&ce.wordn<2&&ce.kbn+1<16){
                    ce.word[ce.wordn][ce.kbn]=(unsigned char)(adt+32);
                    ce.word[ce.wordn][ce.kbn+1]=0;
                }
                ce.tx++;
                ce.kbn++;
            }
        }
    }
    if(com_flg&0x400u)for(i=0;i<ce.wordmax;++i)
        if((ce.txt[ce.wdy][ce.wdx+i]&0xFFu)==0)ce.txt[ce.wdy][ce.wdx+i]=ce.txcol|60u;
}

/* ---- script interpreter (bhControlComEvtScript) ----------------------- */
/* One opcode per 30 Hz pass, as in the original; printing a message consumes
   one pass per character. */
static void control_script(const RECVX_COM_INPUT* in){
    unsigned char r,g,b;
    unsigned bit,flgno;
    int wa,wb;
    if(ce.txflg&0x2u)return;                        /* scrolling */
    if((com_flg&0x10u)&&(ce.txflg&0x8u))return;     /* the keyboard has the pass */
    if(ce.txflg&0x4u){                              /* 226: wait for OK */
        if(in->pad_ps&0x800u)ce.txflg&=~0x4u;
        return;
    }
    if(ce.txflg&0x1u){                              /* 224: print one character */
        const char* text=message((unsigned)ce.txmes);
        if(text[ce.mesp]){                          /* guard: an empty table slot prints nothing */
            ce.curct=16384;
            ce.txt[ce.ty][ce.tx]=ce.txcol|(unsigned)((unsigned char)text[ce.mesp++]-32u);
            sound(0x8000026Eu);
            ce.tx++;
            if(text[ce.mesp])return;
        }
        ce.txflg&=~0x1u;
    }
    if(ce.txwait!=0){ce.txwait--;return;}
    switch(script_next()){
    case 224:
        ce.txflg|=0x1u;ce.txmes=script_next();ce.mesp=0;
        break;
    case 225:
        ce.txwait=script_next();
        break;
    case 226:
        ce.txflg|=0x4u;
        break;
    case 227:
        r=script_next();g=script_next();b=script_next();
        ce.txcol=((unsigned)(r&0xF8u)<<16)|((unsigned)(g&0xFCu)<<11)|((unsigned)(b&0xF8u)<<5);
        break;
    case 228:
        ce.txt[ce.ty][ce.tx]=ce.txcol|(unsigned)(script_next()-32u);
        sound(0x8000026Eu);
        ce.tx++;
        break;
    case 229:
        ce.wordmax=(signed char)script_next();
        if(script_next()!=0)com_flg|=0x400u;else com_flg&=~0x400u;
        ce.wdx=ce.tx;ce.wdy=ce.ty;
        com_flg|=0x10u;
        ce.txflg&=0x7u;ce.txflg|=0x8u;
        ce.kx=1;ce.ky=1;ce.khk=-1;ce.krep=0;ce.kbn=0;ce.wordn=0;
        ce.ct0=0;
        sound(0x8000026Au);
        com_md1=3;
        break;
    case 230:
        com_flg&=~0x10u;ce.ct0=16384;sound(0x8000026Au);com_md1=19;
        break;
    case 231:
        ce.ct0=0;sound(0x8000026Au);com_md1=5;
        break;
    case 232:
        com_flg&=~0x60u;ce.ct0=16384;sound(0x8000026Au);com_md1=18;
        break;
    case 233:
        ce.txflg|=0x8u;
        break;
    case 234:
        wa=script_next();wb=script_next();bit=script_next();
        if(strcmp((const char*)ce.word[wa&1],message((unsigned)wb))==0)ce.lcflg|=1u<<(bit&31);
        else ce.lcflg&=~(1u<<(bit&31));
        break;
    case 235:
        ce.scp++;
        break;
    case 236:
        ce.scp=ce.sclb[script_next()&31];
        break;
    case 237:
        bit=script_next();
        if(ce.lcflg&(1u<<(bit&31)))ce.scp=ce.sclb[script_next()&31];
        else ce.scp++;
        break;
    case 238:
        com_flg|=0x40u;
        ce.chr_no=script_next();
        ce.caxn=(short)((signed char)script_next()<<8);
        ce.cayn=(short)((signed char)script_next()<<8);
        ce.cazn=(short)((signed char)script_next()<<8);
        ce.cox=0;ce.coy=0.1f;ce.coz=-1.4f;
        ce.cax=16384;ce.cay=0;ce.caz=0;
        break;
    case 239:
        ce.mtn_no=script_next();ce.frm_no=script_next();
        break;
    case 240:
        /* bhStFlg(sys->ev_flg, flgno); the operand is little endian. */
        flgno=script_next();flgno|=(unsigned)script_next()<<8;
        event_flag(flgno);
        break;
    case 241:
        ce.mes_no=script_next();ce.mes_ln=script_next();com_flg|=0x80u;
        break;
    case 242:
        com_flg&=~0x80u;
        break;
    case 243:
        ce.lcflg=0;
        break;
    case 244:
        com_flg&=~0x200u;
        break;
    case 245:
        com_flg|=0x200u;
        break;
    case 254:
        sound(0x8000026Eu);
        ce.tx=0;ce.ty++;
        if(ce.ty>=RECVX_COM_ROWS){ce.ty--;ce.txflg|=0x2u;ce.txsct=0;}
        break;
    case 0xFF:
        ce.ct0=0;com_md1=20;
        break;
    default:
        /* The original switch ignores every other byte; count it so a stray
           operand cannot pass unnoticed. */
        if(out)out->unknown_opcode++;
        break;
    }
}

/* ---- quad list (bhEntrySpEvtComputer + bhDrawSpEvtComputer) ----------- */
static void build_quads(void){
    int i,j,accessories=0;
    float cy;
    unsigned a;
    const RECVX_COM_QUAD* tbl;

    /* background, both variants of the Umbrella Workstation desktop */
    if(!(com_flg&0x4u))quad(0,0, 0,0,640,480, 0,0,0.625f,0.46875f, 0.1f,0xFF808080u);
    else               quad(0,0, 0,0,640,480, 0,0.46875f,0.625f,0.9375f, 0.1f,0xFF808080u);
    /* main window frame and its interior fill (3x3 opaque texel of page 1) */
    quad(1,1, 32,32,544,288, 0,0.125f,1.0f,0.625f, 0.25f,((unsigned)ce.mwin_alp<<24)|0xFFFFFFu);
    quad(1,1, 46,46,530,274, 0,0,0.005859375f,0.005859375f, 0.2f,
         (unsigned)(int)(0.75f*ce.mwin_alp)<<24);

    if(com_flg&0x10u){
        float on,x1,y1,u1,v1;
        quad(1,1, 96,272,608,464, 0,0.625f,1.0f,1.0f, 0.3f,((unsigned)ce.swin_alp<<24)|0xFFFFFFu);
        if(ce.txflg&0x10u){ce.txflg&=~0x10u;on=2.0f;}else on=0;
        if(ce.kx==9&&(ce.ky==2||ce.ky==3)){
            x1=527.0f+on;y1=367.0f+on;u1=0.8417969f;v1=0.8105469f;
            a=(unsigned)(128.0f*fabsf(njSin(ce.curct*2)))+64u;
            quad(1,1, x1,y1,44.0f+x1-on,80.0f+y1-on, u1,v1,0.0859375f+u1,0.15625f+v1, 0.31f,
                 ((unsigned)ce.swin_alp<<24)|(a<<16)|(a<<8)|a);
        } else {
            x1=on+(35.0f+(96.0f+ce.kx*44));y1=on+(15.0f+(272.0f+ce.ky*40));
            u1=(ce.kx*44+35)/512.0f;v1=(ce.ky*40+335)/512.0f;
            a=(unsigned)(128.0f*fabsf(njSin(ce.curct*2)))+64u;
            quad(1,1, x1,y1,44.0f+x1-on,40.0f+y1-on, u1,v1,0.0859375f+u1,0.078125f+v1, 0.31f,
                 ((unsigned)ce.swin_alp<<24)|(a<<16)|(a<<8)|a);
        }
    }
    if(com_flg&0x20u){
        quad(1,1, 448,256,608,416, 0,0,0.005859375f,0.005859375f, 0.26f,
             (unsigned)(ce.cwin_alp/2)<<24);
        quad(2,1, 440,240,624,432, 0,0,0.359375f,0.375f, 0.27f,
             ((unsigned)ce.cwin_alp<<24)|0xFFFFFFu);
    }

    /* text scroll: five pixels a pass until one row (20) has gone */
    if(ce.txflg&0x2u){
        ce.txsct+=5;
        if(ce.txsct>=20){
            cy=0;ce.txflg&=~0x2u;
            memmove(ce.txt[0],ce.txt[1],sizeof(ce.txt[0])*(RECVX_COM_ROWS-1));
            memset(ce.txt[RECVX_COM_ROWS-1],0,sizeof ce.txt[0]);
        } else cy=(float)-ce.txsct;
    } else cy=0;

    for(i=0;i<RECVX_COM_ROWS;++i)for(j=0;j<RECVX_COM_COLUMNS;++j){
        const unsigned cell=ce.txt[i][j];
        const unsigned asc=cell&0xFFu;
        unsigned alpha,dat;
        float x1,y1;
        /* Font page: 32 glyph columns of 16x20, the cell code is ASCII - 32. */
        const unsigned gx=(asc&0x1Fu)*16u,gy=(asc/32u)*20u;
        if(!asc)continue;
        x1=24.0f+(32.0f+j*16);y1=cy+(18.0f+(32.0f+i*20));
        alpha=(cell&0xFF000000u)>>24;
        /* txcol packs r in 19..23, g in 13..18, b in 8..12 */
        quad(1,1, x1,y1,16.0f+x1,20.0f+y1,
             gx/512.0f,gy/512.0f,(gx+16u)/512.0f,(gy+20u)/512.0f, 0.21f,
             (alpha<<24)|(((cell&0xF80000u)>>16)<<16)|(((cell&0x7E000u)>>11)<<8)|((cell&0x1F00u)>>5));
        if(alpha<0xFFu){
            dat=alpha+64u;
            if(dat>0xFFu)dat=0xFFu;
            ce.txt[i][j]=(cell&0xFFFFFFu)|(dat<<24);
        }
    }

    /* text cursor */
    {
        const float x1=24.0f+(32.0f+ce.tx*16),y1=cy+(18.0f+(32.0f+ce.ty*20));
        a=(unsigned)(254.0f*fabsf(njSin(ce.curct)));
        quad(1,1, x1,y1,16.0f+x1,20.0f+y1, 0.9375f,0.0390625f,0.96875f,0.078125f, 0.22f,
             ((com_flg&0x8u)&&!(ce.txflg&0x2u))?((a<<24)|0xFFFFFFu):0u);
        ce.curct=(ce.curct+2048)&0xFFFF;
    }

    /* accessory group: the two taskbar strips first, then the table of the
       active terminal. Entry 0 flashes, entries with x1 == 0 are skipped. */
    quad(2,1, 0,440,128,480, 0,0.84375f,0.25f,0.921875f, 0.11f,0xFFFFFFFFu);
    quad(2,1, 128,440,640,480, 0,0.921875f,1.0f,1.0f, 0.111f,0xFFFFFFFFu);
    accessories=2;
    tbl=recvx_com_acs_tab[com_num&7];
    for(i=0;tbl[i].x1>=0;++i){
        if(tbl[i].x1==0)continue;
        quad(2,1, tbl[i].x1,tbl[i].y1,tbl[i].x2,tbl[i].y2, tbl[i].u1,tbl[i].v1,tbl[i].u2,tbl[i].v2,
             0.11f+0.001f*accessories,
             i==0?((ce.flash&1)?0xFFC0C0C0u:0xFFA0A0A0u):0xFFC0C0C0u);
        accessories++;
    }
    ce.flash++;
}

/* ---- clock and bars --------------------------------------------------- */
static void draw_value(int value,float px,float py){
    /* Digit strip of page 2: eight 16x24 cells a row, 0..9 then '-' and ':'. */
    const int cx=((value%8)*16)+204,cy=((value/8)*24)+208;
    const float u1=cx/512.0f,v1=cy/512.0f;
    quad(2,1, px,py,16.0f+px,20.0f+py, u1,v1,0.03125f+u1,0.046875f+v1, 0.15f,0xFFC0C0C0u);
}
static void draw_clock(const RECVX_COM_INPUT* in){
    int hour=21,minute=35,second=12;
    if(com_flg&0x100u){hour=in->hour;minute=in->minute;second=in->second;}
    draw_value(hour/10,364,304);draw_value(hour%10,380,304);
    draw_value(minute/10,400,304);draw_value(minute%10,416,304);
    draw_value(11,432,304);
    draw_value(second/10,440,304);draw_value(second%10,456,304);
}
static float random_unit(void){return (float)recvx_core_random_next()/2.1474836E9f;}
static void draw_bar(int barno,int col0,int col1){
    const float x1=364.0f,y1=(float)(barno*20+344);
    quad(2,1, x1,y1,x1+ce.bar[barno][2],12.0f+y1,
         0.40039063f,((col1*12)+156)/512.0f,0.6796875f,((col1*12)+168)/512.0f, 0.15f,0xFFC0C0C0u);
    quad(2,1, x1,y1,x1+ce.bar[barno][0],12.0f+y1,
         0.40039063f,((col0*12)+156)/512.0f,0.6796875f,((col0*12)+168)/512.0f, 0.151f,0xFFC0C0C0u);
}
static void draw_bars(void){
    int i;
    for(i=0;i<3;++i){
        if(ce.bar_ct[i]&1)ce.bar[i][1]=((7-i)*6)+(((i+1)*30)*random_unit());
        ce.bar[i][0]+=0.5f*(ce.bar[i][1]-ce.bar[i][0]);
        ce.bar_ct[i]=(ce.bar_ct[i]+1)&0x3F;
        if(ce.bar_ct[i]==0)ce.bar[i][2]=ce.bar[i][0];
        else if(ce.bar[i][2]<ce.bar[i][0])ce.bar[i][2]=ce.bar[i][0];
    }
    draw_bar(0,0,2);draw_bar(1,1,3);draw_bar(2,3,0);
}

/* ---- character window (bhDrawSpEvtComCharacter) ----------------------- */
static void request_character(void){
    RECVX_COM_CHARACTER* c;
    if(!out||!(com_flg&0x40u))return;
    c=&out->character;
    c->active=1;c->chr_no=ce.chr_no;c->mtn_no=ce.mtn_no;c->frm_no=ce.frm_no;
    c->px=ce.cox;c->py=ce.coy;c->pz=ce.coz;c->scale=0.05f;
    c->ax=ce.cax;c->ay=ce.cay;c->az=ce.caz;
    c->dist=600.0f;c->width=608.0f;c->height=416.0f;c->cx=528.0f;c->cy=336.0f;
    /* njUserClipping(2, {14,8},{18,12}) in 32-pixel tiles */
    c->clip_x1=448;c->clip_y1=256;c->clip_x2=608;c->clip_y2=416;
    ce.cax=(short)(ce.cax+ce.caxn);
    ce.cay=(short)(ce.cay+ce.cayn);
    ce.caz=(short)(ce.caz+ce.cazn);
}

/* ---- passes ----------------------------------------------------------- */
static void init_pass(const RECVX_COM_INPUT* in){
    int i;
    com_flg&=0x1u;
    memset(&ce,0,sizeof ce);
    com_md0=1;com_md1=0;
    ce.scp=0;
    init_script();
    ce.fcol=0xFF000000u;ce.fsrc=1.0f;
    set_fade(0,10.0f);
    for(i=0;i<3;++i){
        ce.bar[i][0]=ce.bar[i][1]=(float)((7-(i-1))*6)+(((i+1)*30)*random_unit());
        ce.bar_ct[i]=(int)(30.0f*random_unit());
    }
    if(com_num==1)com_flg|=0x4u;
    if(in&&in->clock_valid)com_flg|=0x100u;
}

static void main_pass(const RECVX_COM_INPUT* in){
    switch(com_md1){
    case 0:
        if(!(com_flg&0x2u))break;
        com_md1++;
        sound(0x8000026Au);
        ce.ct0=0;
        /* fall through: the first frame of the opening box already draws */
    case 1:
        entry_box_line(32,32,512.0f*njSin(ce.ct0*2048),256.0f*njSin(ce.ct0*2048));
        if(++ce.ct0>8){com_flg|=0x8u;com_md1++;ce.ct0=0;}
        break;
    case 2:
        ce.ct0+=2048;
        ce.mwin_alp=(short)(255.0f*njSin(ce.ct0));
        if(ce.ct0>=16384){com_md1=10;ce.mwin_alp=255;}
        break;
    case 3:
        entry_box_line(96,272,512.0f*njSin(ce.ct0*2048),192.0f*njSin(ce.ct0*2048));
        if(++ce.ct0>8){com_md1++;ce.ct0=0;}
        break;
    case 4:
        ce.ct0+=2048;
        ce.swin_alp=(short)(255.0f*njSin(ce.ct0));
        if(ce.ct0>=16384){com_md1=10;ce.swin_alp=255;}
        break;
    case 5:
        entry_box_line(448,256,160.0f*njSin(ce.ct0*2048),160.0f*njSin(ce.ct0*2048));
        if(++ce.ct0>8){com_flg|=0x20u;com_md1++;ce.ct0=0;}
        break;
    case 6:
        ce.ct0+=2048;
        ce.cwin_alp=(short)(255.0f*njSin(ce.ct0));
        if(ce.ct0>=16384){com_md1=10;ce.cwin_alp=255;}
        break;
    case 10:
        control_script(in);
        if((com_flg&0x10u)&&(ce.txflg&0x8u))keyboard(in);
        if((in->pad_ps&0x1000u)&&!(com_flg&0x200u)){ce.ct0=0;com_md1=20;}
        break;
    case 18:
        ce.ct0-=2048;
        entry_box_line(448,256,160.0f*njSin(ce.ct0),160.0f*njSin(ce.ct0));
        ce.cwin_alp=(short)(255.0f*njSin(ce.ct0));
        if(ce.ct0<=0){com_md1=10;ce.cwin_alp=0;}
        break;
    case 19:
        ce.ct0-=2048;
        entry_box_line(96,272,512.0f*njSin(ce.ct0),192.0f*njSin(ce.ct0));
        ce.swin_alp=(short)(255.0f*njSin(ce.ct0));
        if(ce.ct0<=0){com_md1=10;ce.swin_alp=0;}
        break;
    case 20:
        com_flg&=~0x80u;
        ce.ct0=16384;
        sound(0x8000026Au);
        com_md1=(com_flg&0x10u)?21:22;
        break;
    case 21:
        ce.ct0-=2048;
        entry_box_line(96,272,512.0f*njSin(ce.ct0),192.0f*njSin(ce.ct0));
        ce.swin_alp=(short)(255.0f*njSin(ce.ct0));
        if(ce.ct0<=0){com_md1++;ce.swin_alp=0;ce.ct0=16384;}
        break;
    case 22:
        if(!(com_flg&0x20u)){com_md1=24;ce.ct0=16384;sound(0x8000026Au);}
        else {com_flg&=~0x40u;ce.ct0=16384;sound(0x8000026Au);com_md1++;}
        break;
    case 23:
        ce.ct0-=2048;
        entry_box_line(448,256,160.0f*njSin(ce.ct0),160.0f*njSin(ce.ct0));
        ce.cwin_alp=(short)(255.0f*njSin(ce.ct0));
        if(ce.ct0<=0){com_md1++;ce.cwin_alp=0;ce.ct0=16384;sound(0x8000026Au);}
        break;
    case 24:
        if(com_flg&0x8u){com_flg&=~0x8u;clear_text();}
        ce.ct0-=2048;
        entry_box_line(32,32,512.0f*njSin(ce.ct0),256.0f*njSin(ce.ct0));
        ce.mwin_alp=(short)(255.0f*njSin(ce.ct0));
        if(ce.ct0<=0){set_fade(1.0f,5.0f);com_md1=30;ce.mwin_alp=0;ce.ct0=0;}
        break;
    case 30:
        if(!(com_flg&0x2u)){com_md0=2;return;}
        break;
    default:
        break;
    }
    build_quads();
    if(com_flg&0x2u)calc_fade();
    if(ce.fcol!=0)polygon(0,0,640,480,-0.8f,ce.fcol);
    draw_box_lines();
    if(com_flg&0x40u)request_character();
    draw_clock(in);
    draw_bars();
    if(com_flg&0x80u&&out){
        out->message_visible=1;out->message_number=ce.mes_no;out->message_lines=ce.mes_ln;
    }
}

static void exit_pass(void){
    /* bhExitSpEvtComputer */
    com_sys.gm_flg|=0x4u;
    com_sys.sp_flg=~0u;
    com_sys.gm_flg|=0x8000u;
    com_sys.st_flg&=~0x4000000u;
    com_sys.cb_flg&=~0x400000u;
    com_sys.ts_flg|=0x20000u;
    com_sys.ts_flg&=~0x80u;
    if(out)out->stand_player=1;
}

void recvx_com_begin(unsigned number,const RECVX_COM_SYSTEM* system){
    com_num=number&7;
    com_flg=0x1u;        /* bhKeepSpEvtComputer allocated the work */
    com_md0=0;com_md1=0;
    memset(&ce,0,sizeof ce);
    if(system)com_sys=*system;
    else memset(&com_sys,0,sizeof com_sys);
}

int recvx_com_tick(const RECVX_COM_INPUT* input,RECVX_COM_FRAME* frame){
    static const RECVX_COM_INPUT idle={0,0,0,0,0,0};
    const RECVX_COM_INPUT* in=input?input:&idle;
    int i,j;
    if(!frame)return 0;
    memset(frame,0,sizeof *frame);
    out=frame;
    frame->number=com_num;
    /* bhCtrSpEvtCom_mode0[com_md0]: exactly one of init / main / exit a pass,
       so the exit runs on the pass after the main pass asked for it. */
    if(com_md0>=2){
        exit_pass();
        com_md0=3;
        frame->active=0;frame->finished=1;frame->system=com_sys;
        frame->com_md0=2;frame->com_md1=com_md1;frame->com_flg=com_flg;
        out=0;
        return 0;
    }
    if(com_md0==0)init_pass(in);
    else main_pass(in);
    frame->active=1;
    frame->system=com_sys;
    frame->com_md0=com_md0;frame->com_md1=com_md1;frame->com_flg=com_flg;
    frame->txflg=ce.txflg;frame->lcflg=ce.lcflg;
    frame->tx=ce.tx;frame->ty=ce.ty;frame->kx=ce.kx;frame->ky=ce.ky;frame->wordn=ce.wordn;
    for(i=0;i<RECVX_COM_ROWS;++i)for(j=0;j<RECVX_COM_COLUMNS;++j)frame->text[i][j]=ce.txt[i][j];
    memcpy(frame->word,ce.word,sizeof frame->word);
    out=0;
    return 1;
}
