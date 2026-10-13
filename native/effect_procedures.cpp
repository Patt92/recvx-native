// Erzeugt von tools/generate_effect_procedures.py aus effsub0..6; danach als
// nativer Quelltext gepflegt. Aufruf von Hand, nicht aus dem Build.
#include "effect_core.h"
namespace recvx {
namespace {
void effset(EffectSprite& w, EffectUv* uvp, unsigned short num)
{
    static unsigned int kcol[10][4] = 
    {
        { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
        { 0xD0201010, 0x70302A2A, 0xD0201010, 0x70302A2A },
        { 0xFFE0E0E0, 0xFFA0A0A0, 0xFF808080, 0xFF404040 },
        { 0xFFC01000, 0xFF900000, 0xFF900000, 0xFF600000 },
        { 0xFFA0C090, 0xFF80A070, 0xFF609060, 0xFF506040 },
        { 0xFFF0F040, 0xFFD0C030, 0xFFC0B020, 0xFFA09010 },
        { 0xFF806040, 0xFF706040, 0xFF605030, 0xFF504030 },
        { 0xFFC0E0FF, 0xFFB0C0E0, 0xFFA0B0D0, 0xFF80A0B0 },
        { 0xD0201010, 0x70302A2A, 0xD0201010, 0x70302A2A },
        { 0xFF404040, 0xFF808080, 0xFFA0A0A0, 0xFFE0E0E0 }
    };
    
    w.sx = 4.0f * (w.sxb * uvp->xs);
    w.sy = 4.0f * (w.syb * uvp->ys);
    
    w.tv[0].col = kcol[num][0];
    w.tv[1].col = kcol[num][1];
    w.tv[2].col = kcol[num][2];
    w.tv[3].col = kcol[num][3];
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
}
void getuv(unsigned char type, EffectSprite& w)
{
	float tab_type[2][4][3] = 
    {
        {
            { -1.0f, -1.0f, 0.0f },
            {  1.0f, -1.0f, 0.0f },
            { -1.0f,  1.0f, 0.0f },
            {  1.0f,  1.0f, 0.0f }
        },
        {
            {  1.0f, -1.0f, 0.0f },
            { -1.0f, -1.0f, 0.0f },
            {  1.0f,  1.0f, 0.0f },
            { -1.0f,  1.0f, 0.0f }
        }
    };
    NJS_POINT3 vs, vd; 
    unsigned char i;

    for (i = 0; i < 4; i++)
    {
        vs.x = tab_type[type][i][0];
        vs.y = tab_type[type][i][1];
        vs.z = tab_type[type][i][2];
        
        njCalcVector(NULL, &vs, &vd);
    
        w.tv[i].x = vd.x;
        w.tv[i].y = vd.y;
        w.tv[i].z = vd.z;
    }
}
void bhEff150(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1;
        }
    }
    
    if (w.type != 0) 
    {
        effect_set_entry(0x97U, (unsigned short)w.type - 1, w);
        w.mode1 = 0;
        w.type = 0;
    }
}
void bhEff151(EffectSprite& w)
{
    EffectUv* uvp;
    static EffectUv uvinfo_koisi[11] = 
    {
        { 0.0f,  0.0f,  0.25f, 0.25f },
        { 0.25f, 0.0f,  0.25f, 0.25f },
        { 0.5f,  0.0f,  0.25f, 0.25f },
        { 0.75f, 0.0f,  0.25f, 0.25f },
        { 0.0f,  0.25f, 0.25f, 0.25f },
        { 0.25f, 0.25f, 0.25f, 0.25f },
        { 0.5f,  0.25f, 0.25f, 0.25f },
        { 0.75f, 0.25f, 0.25f, 0.25f },
        { 0.0f,  0.5f,  0.25f, 0.25f },
        { 0.25f, 0.5f,  0.25f, 0.25f },
        { -1.0f, 0.0f,  0.0f,  0.0f  }
    };
    static EffectUv uvinfo_mokuhen[11] = 
    {
        { 0.0f,  0.0f,  0.25f, 0.25f },
        { 0.25f, 0.0f,  0.25f, 0.25f },
        { 0.5f,  0.0f,  0.25f, 0.25f },
        { 0.75f, 0.0f,  0.25f, 0.25f },
        { 0.0f,  0.25f, 0.25f, 0.25f },
        { 0.25f, 0.25f, 0.25f, 0.25f },
        { 0.5f,  0.25f, 0.25f, 0.25f },
        { 0.75f, 0.25f, 0.25f, 0.25f },
        { 0.0f,  0.5f,  0.25f, 0.25f },
        { 0.25f, 0.5f,  0.25f, 0.25f },
        { -1.0f, 0.0f,  0.0f,  0.0f  }
    };

    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 50;
        
        effect_init(w);
        
        w.yn = -0.1f;
        
        w.spd = 0.02f * w.sz;

        switch (w.type)
        {
        case 1:
            w.exp0 = (unsigned char*)&uvinfo_mokuhen;
            
            w.ani_ct = w.type = 1;
            break;
        default:
            w.exp0 = (unsigned char*)&uvinfo_koisi;
            
            w.ani_ct = w.type = 0;
            break;
        }
        
        w.mode0 = 1;
    }
    
    if (w.ani_ct != w.type) 
    {
        w.ct0 = 0;

        switch (w.type) 
        { 
        case 1:
            w.exp0 = (unsigned char*)&uvinfo_mokuhen;
            
            w.ani_ct = 1;
            break;
        default: 
            w.exp0 = (unsigned char*)&uvinfo_koisi;
            
            w.ani_ct = w.type = 0;
            break;
        }
    }
    
    if (w.yn > -3.0f) 
    {
        w.yn -= w.spd;
    }
    
    w.py += w.yn;
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f)
    {
        w.flg = 0;
    }
    else
    {
        effset(w, uvp, 0);
    
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff152(EffectSprite& w)
{
    NJS_POINT3 vs, vd;  
    unsigned short i, j;
    float px, py, pz; // not from DWARF

    w.flg |= 0x1000000;
    
    if ((w.type == 0) && (w.mode1 != 0))
    {
        w.type = w.mode1;
    }
    
    if (w.type != 0) 
    {
        vs.y = 0;
        vs.x = 0;
        vs.z = w.sz;
        
        njUnitMatrix(NULL);
        
        njRotateXYZ(NULL, w.ax, w.ay, 0);
        njCalcVector(NULL, &vs, &vd);
        
        px = w.px;
        py = w.py;
        pz = w.pz;
        
        j = ++w.ct1;
        
        if (j >= 12) 
        {
            w.ct1 = 0;
            
            j = 0;
        }
        
        for (i = 0; i < w.type; i++)
        {
            w.px = (px + (vd.x * i)) + (2.0f * ((-effect_random() / -2.1474836E9f) - (-effect_random() / -2.1474836E9f)));
            w.py = (py + (vd.y * i)) + (2.0f * ((-effect_random() / -2.1474836E9f) - (-effect_random() / -2.1474836E9f)));
            w.pz = (pz + (vd.z * i)) + (2.0f * ((-effect_random() / -2.1474836E9f) - (-effect_random() / -2.1474836E9f)));
            
            effect_request_template().flg = 0x4100001;
            
            effect_request_template().id = 153;
            effect_request_template().type = j;
            
            effect_request_template().flr_no = w.lkono;
            
            effect_request_template().mdlver = 0;
            
            effect_request_template().px = w.px;
            effect_request_template().py = w.py;
            effect_request_template().pz = w.pz;
            
            effect_request_template().sx = w.sx;
            effect_request_template().sy = w.sy;
            effect_request_template().sz = w.sz;
            
            effect_request_template().ax = w.ax;
            effect_request_template().ay = w.ay;
            
            effect_request(effect_request_template(),NULL,NULL,0);

            j = !j ? 11 : --j; 
        }
        
        w.px = px;
        w.py = py;
        w.pz = pz;
        
        w.mode1 = 0;
    }
}
void bhEff153(EffectSprite& w)
{
    unsigned int argb; // not from DWARF
    static EffectUv uvinfo_geki[12] = 
    {
        { 0.0f,    0.0f, 0.3125f, 0.5f },
        { 0.0f,    0.0f, 0.3125f, 0.5f },
        { 0.3125f, 0.0f, 0.3125f, 0.5f },
        { 0.3125f, 0.0f, 0.3125f, 0.5f },
        { 0.625f,  0.0f, 0.3125f, 0.5f },
        { 0.625f,  0.0f, 0.3125f, 0.5f },
        { 0.0f,    0.5f, 0.3125f, 0.5f },
        { 0.0f,    0.5f, 0.3125f, 0.5f },
        { 0.3125f, 0.5f, 0.3125f, 0.5f },
        { 0.3125f, 0.5f, 0.3125f, 0.5f },
        { 0.625f,  0.5f, 0.3125f, 0.5f },
        { 0.625f,  0.5f, 0.3125f, 0.5f }
    };
    
    switch (w.mode0)
    { 
    case 0:
        w.tex_id = 51;
        
        effect_init(w);
        
        w.exp0 = (unsigned char*)&uvinfo_geki[w.type];
        
        w.mode0 = 1;
        break;
    case 1:
        w.mode0 = 2;
        break;
    case 2:
        w.flg = 0;
        return;
    }
    
    effset(w, (EffectUv*)w.exp0, 0);
    
    if (w.flr_no >= 20) 
    {
        argb = 0;
    }
    else 
    {
        argb = 255 - (unsigned int)((char)w.flr_no * 255) / 20;
    }
    
    w.tv[0].col = w.tv[1].col = w.tv[2].col = w.tv[3].col = (argb << 24) + 0xFFFFFF;
    
    effect_queue_translucent(w);
}
void bhEff154(EffectSprite& w)
{
    int ct0; // not from DWARF
    
    w.flg |= 0x1000000;
    
    if ((w.type == 0) && (w.mode1 != 0)) 
    {
        w.type = w.mode1;
    }
    
    if (w.type != 0) 
    {
        if ((w.ct1 < 1) || (w.ct1 > 7)) 
        {
            switch (w.ct0) 
            {
            case 1:
                if ((-effect_random() / -2.1474836E9f) >= 0.5f) 
                { 
                    ct0 = 2;
                }
                else 
                { 
                    ct0 = 0;
                }
                
                w.ct0 = ct0;
                break;
            case 2:
                if ((-effect_random() / -2.1474836E9f) >= 0.5f) 
                { 
                    ct0 = 1;
                }
                else 
                { 
                    ct0 = 0;
                }
                
                w.ct0 = ct0;
                break;
            default:
                if ((-effect_random() / -2.1474836E9f) >= 0.5f) 
                { 
                    ct0 = 2;
                }
                else 
                { 
                    ct0 = 1; 
                }
                
                w.ct0 = ct0;
                break;
            }
            
            effect_set_entry(155, w.ct0, w);
            
            w.mode1 = 0;
            
            w.ct1 = 7;
        }
        
        w.ct1--;
    }
}
void bhEff155(EffectSprite& w)
{
    EffectUv* uvp;
    static EffectUv uvinfo_hibana_0[8] = 
    {
        { 0.0f,    0.0f,    0.1875f, 0.1875f },
        { 0.1875f, 0.0f,    0.1875f, 0.1875f },
        { 0.375f,  0.0f,    0.1875f, 0.1875f },
        { 0.5625f, 0.0f,    0.1875f, 0.1875f },
        { 0.75f,   0.0f,    0.1875f, 0.1875f },
        { 0.0f,    0.1875f, 0.1875f, 0.1875f },
        { 0.1875f, 0.1875f, 0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
	static EffectUv uvinfo_hibana_1[8] = 
    {
        { 0.375f,  0.1875f, 0.1875f, 0.1875f },
        { 0.5625f, 0.1875f, 0.1875f, 0.1875f },
        { 0.75f,   0.1875f, 0.1875f, 0.1875f },
        { 0.0f,    0.375f,  0.1875f, 0.1875f },
        { 0.1875f, 0.375f,  0.1875f, 0.1875f },
        { 0.375f,  0.375f,  0.1875f, 0.1875f },
        { 0.5625f, 0.375f,  0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
    static EffectUv uvinfo_hibana_2[8] = 
    {
        { 0.75f,   0.375f,  0.1875f, 0.1875f },
        { 0.0f,    0.5625f, 0.1875f, 0.1875f },
        { 0.1875f, 0.5625f, 0.1875f, 0.1875f },
        { 0.375f,  0.5625f, 0.1875f, 0.1875f },
        { 0.5625f, 0.5625f, 0.1875f, 0.1875f },
        { 0.75f,   0.5625f, 0.1875f, 0.1875f },
        { 0.0f,    0.75f,   0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };

    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 52;
        
        effect_init(w);
        
        switch (w.type)
        { 
        case 1:
            w.exp0 = (unsigned char*)uvinfo_hibana_1;
            break;
        case 2:
            w.exp0 = (unsigned char*)uvinfo_hibana_2;
            break;
        default:
            w.exp0 = (unsigned char*)uvinfo_hibana_0;
            break;
        }
        
        w.mode0 = 1;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
    }
    else
    {
        effset(w, uvp, 0);
        
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff156(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1;
        }
    }
    
    if (w.type != 0) 
    {
        effect_set_entry(0x9DU, w.type, w);
        w.mode1 = 0;
        w.type = 0;
    }
}
void bhEff157(EffectSprite& w)
{
    EffectUv* uvp;
    NJS_POINT3 vs, vd;
    static EffectUv uvinfo_mos[13] = 
    {
        { 0.0f,     0.0f,     0.125f,   0.125f   },
        { 0.0f,     0.125f,   0.15625f, 0.15625f },
        { 0.15625f, 0.0f,     0.15625f, 0.15625f },
        { 0.3125f,  0.0f,     0.1875f,  0.1875f  },
        { 0.5f,     0.0f,     0.1875f,  0.1875f  },
        { 0.0f,     0.28125f, 0.21875f, 0.21875f },
        { 0.21875f, 0.1875f,  0.21875f, 0.21875f },
        { 0.4375f,  0.1875f,  0.21875f, 0.21875f },
        { 0.75f,    0.0f,     0.25f,    0.25f    },
        { 0.75f,    0.25f,    0.25f,    0.25f    },
        { 0.75f,    0.5f,     0.25f,    0.25f    },
        { 0.75f,    0.75f,    0.25f,    0.25f    },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };

    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 53;
        
        effect_init(w);
        
        vs.y = 0;
        vs.x = 0;
        vs.z = w.sz;
        
        njUnitMatrix(NULL);
        
        njRotateXYZ(NULL, w.ax, w.ay, 0);
        
        njCalcVector(NULL, &vs, &vd);
        
        w.xn = vd.x;
        w.yn = vd.y;
        w.zn = vd.z;
        
        w.exp0 = (unsigned char*)&uvinfo_mos;
        
        w.mode0 = 1;
        break;
    }
    
    w.px += w.xn;
    w.py += w.yn;
    w.pz += w.zn;
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
    }
    else
    {
        effset(w, uvp, 0);
        
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff158(EffectSprite& w)
{
    int type; // not from DWARF
    
    w.flg |= 0x1000000;
    
    if ((w.type == 0) && (w.mode1 != 0))
    {
        w.type = w.mode1;
    }
    
    if (w.type != 0) 
    {
        if ((unsigned int)w.type == 2) 
        {
            type = 1;
        }
        else 
        {
            type = 0;
        }
        
        effect_set_entry(159, type, w);
        
        w.mode1 = 0;
        w.type = 0;
    }
}
void bhEff159(EffectSprite& w)
{
    static EffectUv uvinfo_exp_0[25] = 
    {
        { 0.09375f, 0.15625f, 0.0625f,  0.0625f  },
        { 0.0f,     0.125f,   0.09375f, 0.09375f },
        { 0.0f,     0.0f,     0.125f,   0.125f   },
        { 0.125f,   0.0f,     0.15625f, 0.15625f },
        { 0.28125f, 0.0f,     0.15625f, 0.15625f },
        { 0.4375f,  0.0f,     0.1875f,  0.1875f  },
        { 0.625f,   0.0f,     0.1875f,  0.1875f  },
        { 0.8125f,  0.0f,     0.1875f,  0.1875f  },
        { 0.0f,     0.21875f, 0.1875f,  0.1875f  },
        { 0.1875f,  0.1875f,  0.21875f, 0.21875f },
        { 0.40625f, 0.1875f,  0.21875f, 0.21875f },
        { 0.625f,   0.1875f,  0.21875f, 0.21875f },
        { 0.0f,     0.40625f, 0.25f,    0.25f    },
        { 0.25f,    0.40625f, 0.25f,    0.25f    },
        { 0.5f,     0.40625f, 0.25f,    0.25f    },
        { 0.75f,    0.40625f, 0.25f,    0.25f    },
        { 0.0f,     0.65625f, 0.25f,    0.25f    },
        { 0.25f,    0.65625f, 0.25f,    0.25f    },
        { 0.5f,     0.65625f, 0.25f,    0.25f    },
        { 0.75f,    0.65625f, 0.25f,    0.25f    },
        { 0.0f,     0.75f,    0.25f,    0.25f    },
        { 0.25f,    0.75f,    0.25f,    0.25f    },
        { 0.5f,     0.75f,    0.25f,    0.25f    },
        { 0.75f,    0.75f,    0.25f,    0.25f    },
        { -1.0f,    0.0f,     0.0f,     0.0f     },
    };
    static EffectUv uvinfo_exp_1[27] = 
    {
        { 0.09375f, 0.15625f, 0.0625f,  0.0625f  },
        { 0.0f,     0.125f,   0.09375f, 0.09375f },
        { 0.0f,     0.0f,     0.125f,   0.125f   },
        { 0.125f,   0.0f,     0.15625f, 0.15625f },
        { 0.28125f, 0.0f,     0.15625f, 0.15625f },
        { 0.4375f,  0.0f,     0.1875f,  0.1875f  },
        { 0.625f,   0.0f,     0.1875f,  0.1875f  },
        { 0.8125f,  0.0f,     0.1875f,  0.1875f  },
        { 0.0f,     0.21875f, 0.1875f,  0.1875f  },
        { 0.1875f,  0.1875f,  0.21875f, 0.21875f },
        { 0.40625f, 0.1875f,  0.21875f, 0.21875f },
        { 0.625f,   0.1875f,  0.21875f, 0.21875f },
        { 0.0f,     0.40625f, 0.25f,    0.25f    },
        { 0.25f,    0.40625f, 0.25f,    0.25f    },
        { 0.0f,     0.0f,     0.25f,    0.25f    },
        { 0.25f,    0.0f,     0.25f,    0.25f    },
        { 0.5f,     0.0f,     0.25f,    0.25f    },
        { 0.75f,    0.0f,     0.25f,    0.25f    },
        { 0.0f,     0.25f,    0.25f,    0.25f    },
        { 0.25f,    0.25f,    0.25f,    0.25f    },
        { 0.5f,     0.25f,    0.25f,    0.25f    },
        { 0.75f,    0.25f,    0.25f,    0.25f    },
        { 0.0f,     0.5f,     0.25f,    0.25f    },
        { 0.25f,    0.5f,     0.25f,    0.25f    },
        { 0.5f,     0.5f,     0.25f,    0.25f    },
        { 0.75f,    0.5f,     0.25f,    0.25f    },
        { -1.0f,    0.0f,     0.0f,     0.0f     },
    };
    int exp_bank0[24] = 
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3, 3, 3, 3
    };
    int exp_bank1[26] = 
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
    };
    EffectUv* uvp;

    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 54;
        
        effect_init(w);
        
        switch (w.type) 
        {
        case 1:
            w.exp0 = (unsigned char*)uvinfo_exp_1;
            break;
        case 2:
            w.exp0 = (unsigned char*)uvinfo_exp_0;
            
            w.type -= 2;
            
            w.xn = (w.sz * ((-effect_random() / -2.1474836E9f) - 0.5f)) / 8.0f;
            w.yn = (w.sz * ((-effect_random() / -2.1474836E9f) - 0.5f)) / 8.0f;
            w.zn = (w.sz * ((-effect_random() / -2.1474836E9f) - 0.5f)) / 8.0f;
            break;
        default:
            w.exp0 = (unsigned char*)uvinfo_exp_0;
            
            w.type = 0;
            break;
        }
        
        w.mode0 = 1;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
        
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
    }
    else 
    {
        switch (w.type)
        {
        case 1:
            w.ani_ct = exp_bank1[w.ct0];
            break;
        default:
            w.ani_ct = exp_bank0[w.ct0];
            break;
        }
        
        w.px += w.xn;
        w.py += w.yn;
        w.pz += w.zn;
        
        effset(w, uvp, 0);
        
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff160(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1;
        }
    }

    if (w.type != 0) 
    {
        effect_set_entry(0xA1U, (unsigned short) w.type - 1, w);
        w.mode1 = 0;
        w.type = 0;
    }
}
void bhEff161(EffectSprite& w)
{
    NJS_POINT3 vs, vd;
    EffectUv* uvp;
    static EffectUv uvinfo_sw_0[11] = 
    {
        { 0.625f,   0.625f,   0.15625f, 0.15625f },
        { 0.0f,     0.0f,     0.3125f,  0.3125f  },
        { 0.3125f,  0.0f,     0.3125f,  0.3125f  },
        { 0.625f,   0.0f,     0.3125f,  0.3125f  },
        { 0.0f,     0.3125f,  0.3125f,  0.3125f  },
        { 0.3125f,  0.3125f,  0.3125f,  0.3125f  },
        { 0.625f,   0.3125f,  0.3125f,  0.3125f  },
        { 0.0f,     0.625f,   0.3125f,  0.3125f  },
        { 0.3125f,  0.625f,   0.3125f,  0.3125f  },
        { 0.78125f, 0.78125f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
	static EffectUv uvinfo_sw_1[16] = 
    {
        { 0.0f,     0.0f,     0.09375f, 0.09375f },
        { 0.09375f, 0.0f,     0.125f,   0.125f   },
        { 0.21875f, 0.0f,     0.125f,   0.125f   },
        { 0.34375f, 0.0f,     0.15625f, 0.15625f },
        { 0.5f,     0.0f,     0.15625f, 0.15625f },
        { 0.65625f, 0.0f,     0.15625f, 0.15625f },
        { 0.0f,     0.15625f, 0.1875f,  0.1875f  },
        { 0.1875f,  0.15625f, 0.1875f,  0.1875f  },
        { 0.375f,   0.15625f, 0.1875f,  0.1875f  },
        { 0.5625f,  0.15625f, 0.1875f,  0.1875f  },
        { 0.75f,    0.15625f, 0.1875f,  0.1875f  },
        { 0.0f,     0.34375f, 0.1875f,  0.1875f  },
        { 0.1875f,  0.34375f, 0.1875f,  0.1875f  },
        { 0.375f,   0.34375f, 0.1875f,  0.1875f  },
        { 0.5625f,  0.34375f, 0.1875f,  0.1875f  },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_sw_2[11] = 
    {
        { 0.0f,     0.53125f, 0.09375f, 0.09375f },
        { 0.09375f, 0.53125f, 0.125f,   0.125f   },
        { 0.21875f, 0.53125f, 0.15625f, 0.15625f },
        { 0.375f,   0.53125f, 0.1875f,  0.1875f  },
        { 0.5625f,  0.53125f, 0.1875f,  0.1875f  },
        { 0.75f,    0.53125f, 0.1875f,  0.1875f  },
        { 0.0f,     0.71875f, 0.21875f, 0.21875f },
        { 0.21875f, 0.71875f, 0.21875f, 0.21875f },
        { 0.4375f,  0.71875f, 0.21875f, 0.21875f },
        { 0.65625f, 0.71875f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 55;
        
        effect_init(w);
        
        vs.y = 0;
        vs.x = 0;
        vs.z = w.sz;
        
        njUnitMatrix(NULL);
        
        njRotateXYZ(NULL, w.ax, w.ay, 0);
        njCalcVector(NULL, &vs, &vd);
        
        w.xn = vd.x;
        w.yn = vd.y;
        w.zn = vd.z;
        
        switch (w.type % 10)
        {
        case 1:
            w.exp0 = (unsigned char*)&uvinfo_sw_1;
            
            w.ani_ct = 1;
            break;
        case 2:
            w.exp0 = (unsigned char*)&uvinfo_sw_2;
            
            w.ani_ct = 1;
            break;
        default:
            w.exp0 = (unsigned char*)&uvinfo_sw_0;
            break;
        }
        
        w.spd = w.type / 500.0f;
        
        w.ct1 = 0;
        
        w.mode0 = 1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
    }
    else
    {
        w.px += w.xn;
        w.py += w.yn;
        w.pz += w.zn;
        
        w.py -= w.spd * w.ct1;
        
        w.ct1++;
        
        effset(w, uvp, 0);
        
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff162(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1;
        }
    }

    if (w.type != 0) 
    {
        effect_set_entry(0xA3U, (unsigned short) w.type - 1, w);
        w.mode1 = 0;
        w.type = 0;
    }
}
void bhEff163(EffectSprite& w)
{
    EffectUv* uvp;
    NJS_POINT3 vs, vd;
    static EffectUv uvinfo_bub_0[18] = 
    {
        { 0.0f,     0.0625f,  0.03125f, 0.03125f },
        { 0.03125f, 0.0625f,  0.03125f, 0.03125f },
        { 0.0f,     0.0f,     0.0625f,  0.0625f  },
        { 0.0625f,  0.0f,     0.0625f,  0.0625f  },
        { 0.125f,   0.0f,     0.0625f,  0.0625f  },
        { 0.1875f,  0.0f,     0.0625f,  0.0625f  },
        { 0.25f,    0.0f,     0.09375f, 0.09375f },
        { 0.34375f, 0.0f,     0.09375f, 0.09375f },
        { 0.4375f,  0.0f,     0.09375f, 0.09375f },
        { 0.53125f, 0.0f,     0.09375f, 0.09375f },
        { 0.625f,   0.0f,     0.09375f, 0.09375f },
        { 0.71875f, 0.0f,     0.09375f, 0.09375f },
        { -3.0f,    6.0f,     0.0f,     0.0f     },
        { 0.5625f,  0.09375f, 0.09375f, 0.09375f },
        { 0.65625f, 0.09375f, 0.09375f, 0.09375f },
        { 0.75f,    0.09375f, 0.09375f, 0.09375f },
        { 0.84375f, 0.09375f, 0.09375f, 0.09375f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
	static EffectUv uvinfo_bub_1[14] = 
    {
        { 0.0f,   0.1875f, 0.125f, 0.125f },
        { 0.125f, 0.1875f, 0.125f, 0.125f },
        { 0.25f,  0.1875f, 0.125f, 0.125f },
        { 0.375f, 0.1875f, 0.125f, 0.125f },
        { 0.5f,   0.1875f, 0.125f, 0.125f },
        { 0.625f, 0.1875f, 0.125f, 0.125f },
        { 0.75f,  0.1875f, 0.125f, 0.125f },
        { 0.875f, 0.1875f, 0.125f, 0.125f },
        { -4.0f,  0.0f,    0.0f,   0.0f   },
        { -4.0f,  0.0f,    0.0f,   0.0f   },
        { -4.0f,  0.0f,    0.0f,   0.0f   },
        { -4.0f,  0.0f,    0.0f,   0.0f   },
        { -4.0f,  0.0f,    0.0f,   0.0f   },
        { -2.0f,  0.0f,    0.0f,   0.0f   }
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 56;
        
        effect_init(w);
        
        switch (w.type)
        {
        case 1:
            w.exp0 = (unsigned char*)&uvinfo_bub_1;
            
            njUnitMatrix(NULL);
            
            njRotateXYZ(NULL, 0, w.ay, -w.ax);
            
            getuv(0, w);
            break;
        default:
            w.exp0 = (unsigned char*)&uvinfo_bub_0;
            
            w.type = 0;
            
            vs.z = 0;
            vs.x = 0;
            vs.y = 0.5f;
            
            njUnitMatrix(NULL);
            
            njRotateXYZ(NULL, w.ax, w.ay, 0);
            njCalcVector(NULL, &vs, &vd);
            
            w.xn = vd.x;
            w.yn = vd.y;
            w.zn = vd.z;
            break;
        }
        
        w.ct1 = w.sz;
        
        w.mode0 = 1;
    }
    
    if ((w.type == 0) && (w.ct0 < 13)) 
    {
        w.px += w.xn;
        w.py += w.yn;
        w.pz += w.zn;
    }

    uvp = (EffectUv*)w.exp0 + w.ct0;

    if (uvp->u == -1.0f)
    {
        w.flg = 0;
        return;
    }
    else if (uvp->u == -2.0f) 
    {
        w.ct0 = 0;
    }
    else if (uvp->u == -3.0f)
    {       
        if (--w.ct1 > 0) 
        {
            w.ct0 = uvp->v;
        }
        else 
        {
            w.ct0++;
        }
    }
    else if (uvp->u == -4.0f) 
    {
        w.ct0++;
        return;
    }

    effset(w, (EffectUv*)w.exp0 + w.ct0, 0);
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff164(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1;
            w.mode1 = 0;
        }
    }

    if ((w.type != 0) && (w.ct0 == 0)) 
    {
        w.ct0 = w.type & 0xFFFF;
    }

    if (w.ct0 != 0) 
    {
        effect_set_entry(0x9FU, 2U, w);
        w.ct0 -= 1;
        if (w.ct0 == 0) 
        {
            w.type = 0;
        }
    }
}
void bhEff166(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1 & 0xFF;
        }
    }

    if (w.type != 0) 
    {
        if ((w.ct1 <= 0) || (w.ct1 > 0xD))  
        {
            effect_set_entry(0xA7U, 1U, w);
            w.mode1 = 0;
            w.ct1 = 0xD;
        } 
        
        w.ct1 -= 1;
    }
}
void bhEff167(EffectSprite& w)
{
	EffectUv* uvp;
    static EffectUv uvinfo_e29[14] = 
    {
        { 0.0f,    0.0f,  0.1875f, 0.25f },
        { 0.1875f, 0.0f,  0.1875f, 0.25f },
        { 0.375f,  0.0f,  0.1875f, 0.25f },
        { 0.5625f, 0.0f,  0.1875f, 0.25f },
        { 0.75f,   0.0f,  0.1875f, 0.25f },
        { 0.0f,    0.25f, 0.1875f, 0.25f },
        { 0.1875f, 0.25f, 0.1875f, 0.25f },
        { 0.375f,  0.25f, 0.1875f, 0.25f },
        { 0.5625f, 0.25f, 0.1875f, 0.25f },
        { 0.75f,   0.25f, 0.1875f, 0.25f },
        { 0.0f,    0.5f,  0.1875f, 0.25f },
        { 0.1875f, 0.5f,  0.1875f, 0.25f },
        { 0.375f,  0.5f,  0.1875f, 0.25f },
        { -1.0f,   0.0f,  0.0f,    0.0f  }
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 57;
        
        effect_init(w);
        
        njUnitMatrix(NULL);
        
        njRotateXYZ(NULL, 0, w.ay, -w.ax);
        
        getuv(0, w);
        
        w.exp0 = (unsigned char*)&uvinfo_e29;
        
        w.mode0 = 1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
    }
    else
    {
        effset(w, uvp, 0);
        
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff168(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1 & 0xFF;
        }
    }
    
    if (w.type != 0) 
    {
        effect_set_entry(0xA9U, (unsigned short) w.type - 1, w);
        w.mode1 = 0;
        w.type = 0;
    }
}
void bhEff169(EffectSprite& w)
{
    EffectUv* uvp;
	static EffectUv uvinfo_e23_0[11] = 
    {
        { 0.0f,    0.0f,    0.1875f, 0.1875f },
        { 0.1875f, 0.0f,    0.1875f, 0.1875f },
        { 0.375f,  0.0f,    0.1875f, 0.1875f },
        { 0.5625f, 0.0f,    0.1875f, 0.1875f },
        { 0.75f,   0.0f,    0.1875f, 0.1875f },
        { 0.0f,    0.1875f, 0.1875f, 0.1875f },
        { 0.1875f, 0.1875f, 0.1875f, 0.1875f },
        { 0.375f,  0.1875f, 0.1875f, 0.1875f },
        { 0.5625f, 0.1875f, 0.1875f, 0.1875f },
        { 0.75f,   0.1875f, 0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
	static EffectUv uvinfo_e23_1[15] = 
    {
        { 0.0f,    0.375f,  0.1875f, 0.1875f },
        { 0.1875f, 0.375f,  0.1875f, 0.1875f },
        { 0.375f,  0.375f,  0.1875f, 0.1875f },
        { 0.5625f, 0.375f,  0.1875f, 0.1875f },
        { 0.75f,   0.375f,  0.1875f, 0.1875f },
        { 0.0f,    0.5625f, 0.1875f, 0.1875f },
        { 0.1875f, 0.5625f, 0.1875f, 0.1875f },
        { 0.375f,  0.5625f, 0.1875f, 0.1875f },
        { 0.5625f, 0.5625f, 0.1875f, 0.1875f },
        { 0.75f,   0.5625f, 0.1875f, 0.1875f },
        { 0.0f,    0.75f,   0.1875f, 0.1875f },
        { 0.1875f, 0.75f,   0.1875f, 0.1875f },
        { 0.375f,  0.75f,   0.1875f, 0.1875f },
        { 0.5625f, 0.75f,   0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
    
    switch (w.mode0)
    {
    case 0:
        w.tex_id = 64;
        
        effect_init(w);
        
        njUnitMatrix(NULL);
        
        njRotateXYZ(NULL, 0, w.ay, -w.ax);
        
        switch (w.type) 
        {
        case 1:
            w.exp0 = (unsigned char*)&uvinfo_e23_1;
            
            getuv(0, w);
            break;
        case 2:
            w.exp0 = (unsigned char*)&uvinfo_e23_1;
            
            getuv(1, w);
            break;
        default:
            w.exp0 = (unsigned char*)&uvinfo_e23_0;
            
            w.type = 0;
            
            getuv(0, w);
            break;
        }
        
        w.mode0 = 1;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
    }
    else
    {
        effset(w, uvp, 0);
        
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff171(EffectSprite& w)
{
    EffectUv* uvp;
    static EffectUv uvinfo_sw_0[11] = 
    {
        { 0.625f,   0.625f,   0.15625f, 0.15625f },
        { 0.0f,     0.0f,     0.3125f,  0.3125f  },
        { 0.3125f,  0.0f,     0.3125f,  0.3125f  },
        { 0.625f,   0.0f,     0.3125f,  0.3125f  },
        { 0.0f,     0.3125f,  0.3125f,  0.3125f  },
        { 0.3125f,  0.3125f,  0.3125f,  0.3125f  },
        { 0.625f,   0.3125f,  0.3125f,  0.3125f  },
        { 0.0f,     0.625f,   0.3125f,  0.3125f  },
        { 0.3125f,  0.625f,   0.3125f,  0.3125f  },
        { 0.78125f, 0.78125f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 55;
        
        effect_init(w);
        
        w.exp0 = (unsigned char*)&uvinfo_sw_0;
        
        w.ct1 = w.type;
        
        w.mode0 = 1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.ct1--;

        if (w.ct1 != 0)
        {
            w.ct0 = 0;
            
            uvp = (EffectUv*)w.exp0 + w.ct0;
        }
        else
        {
            w.flg = 0;
            return;
        }
    }
    
    effset(w, uvp, 0);
        
    w.ct0++;
        
    effect_queue_translucent(w);
}
void bhEff172(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1;
        }
    }

    if (w.type != 0) 
    {
        effect_set_entry(0xAD, (unsigned short)w.type - 1, w);
        w.mode1 = 0;
        w.type = 0;
    }
}
void bhEff173(EffectSprite& w)
{
    EffectUv* uvp;
    static EffectUv uvinfo_e23_0[2] = 
    {
        { 0.0f,  0.0f, 0.99609375f, 0.99609375f },
        { -1.0f, 0.0f, 0.0f,        0.0f        }
    };
    static int bltbl[10] = 
    {
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 70;
        
        effect_init(w);
        
        w.exp0 = (unsigned char*)&uvinfo_e23_0;
        
        w.mode0 = 1;
        break;
    }
    
    w.bl_src = bltbl[(w.type / 10) + (w.type >> 31)];
    w.bl_dst = bltbl[w.type % 10];
    
    w.sxb = 1.0f;
    w.syb = 1.0f;
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.ct0 = 0;
        
        uvp = (EffectUv*)w.exp0 + w.ct0;
    }
    
    effset(w, uvp, 0);
        
    w.ct0++;
        
    effect_queue_translucent(w);
}
void bhEff175(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1;
        }
    }
    
    if (w.type != 0) 
    {
        effect_set_entry(0xB0, 0, w);
        w.mode1 = 0;
        w.type = 0;
    }
}
void bhEff176(EffectSprite& w)
{
	static EffectUv uvinfo_moon[1] = 
    { 
        0.0f, 0.0f, 1.0f, 1.0f 
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 68;
        
        effect_init(w);
        
        w.exp0 = (unsigned char*)&uvinfo_moon;
        
        w.mode0 = 1;
        break;
    }
    
    effset(w, (EffectUv*)w.exp0 + w.ct0, 0);
        
    effect_queue_translucent(w);
}
void bhEff178(EffectSprite& w)
{
    NJS_POINT3 vs, vd;
    EffectUv* uvp;
    int type; // not from DWARF
    static EffectUv uvinfo_hiko[7] = 
    {
        { 0.0f,    0.0f,     0.3125f, 0.34375f },
        { 0.3125f, 0.0f,     0.3125f, 0.34375f },
        { 0.625f,  0.0f,     0.3125f, 0.34375f },
        { 0.0f,    0.34375f, 0.3125f, 0.34375f },
        { 0.3125f, 0.34375f, 0.3125f, 0.34375f },
        { 0.625f,  0.34375f, 0.3125f, 0.34375f },
        { -1.0f,   0.0f,     0.0f,    0.0f     }
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 69;
        
        effect_init(w);
        
        w.spd = w.sz;

        type = w.type;
        
        if (w.type >= 6) 
        {
            type = 0;
        }
        
        w.ct0 = type;
        w.ct1 = 0;
        
        w.exp0 = (unsigned char*)&uvinfo_hiko;
        
        w.mode0 = 1;
    }
    
    w.ct1++;
    
    if (w.lkono <= w.ct1) 
    {
        w.flg = 0;
    }
    else 
    {
        vs.y = 0;
        vs.x = 0;
        vs.z = w.spd;
        
        njUnitMatrix(NULL);
        
        njRotateXYZ(NULL, w.ax, w.ay, 0);
        
        njCalcVector(NULL, &vs, &vd);
        
        w.px += vd.x;
        w.py += vd.y;
        w.pz += vd.z;
        
        uvp = (EffectUv*)w.exp0 + w.ct0;
        
        if (uvp->u == -1.0f) 
        {
            w.ct0 = 0;
            
            uvp = (EffectUv*)w.exp0 + w.ct0;
        }
        
        effset(w, uvp, 0);
        
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff179(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1;
        }
    }
    
    if (w.type != 0) 
    {
        if (!w.sz) 
        {
            effect_set_entry(0xB4, (unsigned short)w.type - 1, w);
            w.mode1 = 0;
            w.type = 0;
            return;
        }

        if (w.ct1 <= 0) 
        {
            effect_set_entry(0xB4, (unsigned short)w.type - 1, w);
            w.ct1 = (int) (w.sz);
        }
        w.ct1 -= 1;
    }
}
void bhEff180(EffectSprite& w)
{
    EffectUv* uvp;
    static EffectUv uvinfo_hiba2_0[11] = 
    {
        { 0.0f,     0.0f,     0.15625f, 0.15625f },
        { 0.15625f, 0.0f,     0.15625f, 0.15625f },
        { 0.3125f,  0.0f,     0.15625f, 0.15625f },
        { 0.46875f, 0.0f,     0.15625f, 0.15625f },
        { 0.625f,   0.0f,     0.15625f, 0.15625f },
        { 0.78125f, 0.0f,     0.15625f, 0.15625f },
        { 0.0f,     0.15625f, 0.15625f, 0.15625f },
        { 0.15625f, 0.15625f, 0.15625f, 0.15625f },
        { 0.3125f,  0.15625f, 0.15625f, 0.15625f },
        { 0.46875f, 0.15625f, 0.15625f, 0.15625f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_hiba2_1[11] = 
    {
        { 0.0f,     0.3125f,  0.15625f, 0.15625f },
        { 0.15625f, 0.3125f,  0.15625f, 0.15625f },
        { 0.3125f,  0.3125f,  0.15625f, 0.15625f },
        { 0.46875f, 0.3125f,  0.15625f, 0.15625f },
        { 0.625f,   0.3125f,  0.15625f, 0.15625f },
        { 0.78125f, 0.3125f,  0.15625f, 0.15625f },
        { 0.0f,     0.46875f, 0.15625f, 0.15625f },
        { 0.15625f, 0.46875f, 0.15625f, 0.15625f },
        { 0.3125f,  0.46875f, 0.15625f, 0.15625f },
        { 0.46875f, 0.46875f, 0.15625f, 0.15625f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 408;
        
        effect_init(w);

        switch (w.type)
        {
        case 1:
            w.exp0 = (unsigned char*)&uvinfo_hiba2_1;
            
            w.ani_ct = 1;
            break;
        default:
            w.exp0 = (unsigned char*)&uvinfo_hiba2_0;
            break;
        }
        
        w.mode0 = 1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
    }
    else 
    {
        effset(w, uvp, 0);
        
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff181(EffectSprite& w)
{
	unsigned short typ;

    w.flg |= 0x1000000;
    
    if ((w.type == 0) && (w.mode1 != 0))
    {
        w.type = w.mode1;
    }
    
    if (w.type != 0) 
    {
        typ = w.type - 1;
        
        if (!floorf(w.sz)) 
        {
            effect_set_entry(182, typ, w);
            
            w.mode1 = 0;
            
            w.type = 0;
            return;
        }
        
        if (w.ct1 < 1) 
        {
            effect_set_entry(182, typ, w);
            
            w.ct1 = floorf(w.sz);
        }
        
        w.ct1--;
    }
}
void bhEff182(EffectSprite& w)
{
    unsigned short ktype, kcolor;
    NJS_POINT3 vs, vd;   
	EffectUv* uvp;
    static EffectUv uvinfo_kemu0[21] = 
    {
        { 0.0f,     0.0f,     0.09375f, 0.09375f  },
        { 0.09375f, 0.0f,     0.09375f, 0.09375f  },
        { 0.1875f,  0.0f,     0.09375f, 0.09375f  },
        { 0.28125f, 0.0f,     0.09375f, 0.09375f  },
        { 0.375f,   0.0f,     0.09375f, 0.09375f  },
        { 0.46875f, 0.0f,     0.125f,   0.125f    },
        { 0.59375f, 0.0f,     0.125f,   0.125f    },
        { 0.71875f, 0.0f,     0.125f,   0.125f    },
        { 0.84375f, 0.0f,     0.125f,   0.125f    },
        { 0.0f,     0.09375f, 0.125f,   0.125f    },
        { 0.125f,   0.09375f, 0.15625f, 0.15625f  },
        { 0.28125f, 0.09375f, 0.15625f, 0.15625f  },
        { 0.4375f,  0.125f,   0.15625f, 0.15625f  },
        { 0.59375f, 0.125f,   0.15625f, 0.15625f  },
        { 0.75f,    0.125f,   0.15625f, 0.15625f  },
        { 0.0f,     0.25f,    0.15625f, 0.15625f  },
        { 0.15625f, 0.25f,    0.15625f, 0.15625f  },
        { 0.3125f,  0.28125f, 0.15625f, 0.125f    },
        { 0.46875f, 0.28125f, 0.15625f, 0.125f    },
        { 0.625f,   0.28125f, 0.125f,   0.125f    },
        { -1.0f,    0.0f,     0.0f,     0.0f      }
    };
    static EffectUv uvinfo_kemu1[23] = 
    {
        { 0.0f,     0.40625f, 0.125f,   0.125f   },
        { 0.125f,   0.40625f, 0.15625f, 0.15625f },
        { 0.28125f, 0.40625f, 0.15625f, 0.15625f },
        { 0.4375f,  0.40625f, 0.15625f, 0.15625f },
        { 0.59375f, 0.40625f, 0.15625f, 0.15625f },
        { 0.75f,    0.40625f, 0.15625f, 0.15625f },
        { 0.0f,     0.5625f,  0.15625f, 0.15625f },
        { 0.15625f, 0.5625f,  0.15625f, 0.15625f },
        { 0.3125f,  0.5625f,  0.15625f, 0.15625f },
        { 0.46875f, 0.5625f,  0.15625f, 0.15625f },
        { 0.625f,   0.5625f,  0.15625f, 0.15625f },
        { 0.78125f, 0.5625f,  0.15625f, 0.15625f },
        { 0.0f,     0.71875f, 0.15625f, 0.15625f },
        { 0.15625f, 0.71875f, 0.15625f, 0.15625f },
        { 0.3125f,  0.71875f, 0.15625f, 0.15625f },
        { 0.46875f, 0.71875f, 0.15625f, 0.15625f },
        { 0.625f,   0.71875f, 0.15625f, 0.15625f },
        { 0.78125f, 0.71875f, 0.15625f, 0.15625f },
        { 0.0f,     0.875f,   0.15625f, 0.125f   },
        { 0.15625f, 0.875f,   0.15625f, 0.125f   },
        { 0.3125f,  0.875f,   0.125f,   0.125f   },
        { 0.4375f,  0.875f,   0.125f,   0.125f   },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_kemu2[25] = 
    {
        { 0.0f,    0.0f,    0.1875f, 0.1875f },
        { 0.1875f, 0.0f,    0.1875f, 0.1875f },
        { 0.375f,  0.0f,    0.1875f, 0.1875f },
        { 0.5625f, 0.0f,    0.1875f, 0.1875f },
        { 0.75f,   0.0f,    0.1875f, 0.1875f },
        { 0.0f,    0.1875f, 0.1875f, 0.1875f },
        { 0.1875f, 0.1875f, 0.1875f, 0.1875f },
        { 0.375f,  0.1875f, 0.1875f, 0.1875f },
        { 0.5625f, 0.1875f, 0.1875f, 0.1875f },
        { 0.75f,   0.1875f, 0.1875f, 0.1875f },
        { 0.0f,    0.375f,  0.1875f, 0.1875f },
        { 0.1875f, 0.375f,  0.1875f, 0.1875f },
        { 0.375f,  0.375f,  0.1875f, 0.1875f },
        { 0.5625f, 0.375f,  0.1875f, 0.1875f },
        { 0.75f,   0.375f,  0.1875f, 0.1875f },
        { 0.0f,    0.5625f, 0.1875f, 0.1875f },
        { 0.1875f, 0.5625f, 0.1875f, 0.1875f },
        { 0.375f,  0.5625f, 0.1875f, 0.1875f },
        { 0.5625f, 0.5625f, 0.1875f, 0.1875f },
        { 0.75f,   0.5625f, 0.1875f, 0.1875f },
        { 0.0f,    0.75f,   0.1875f, 0.1875f },
        { 0.1875f, 0.75f,   0.1875f, 0.1875f },
        { 0.375f,  0.75f,   0.1875f, 0.1875f },
        { 0.5625f, 0.75f,   0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
    static EffectUv uvinfo_kemu3[25] = 
    {
        { 0.0f,    0.0f,    0.1875f, 0.1875f },
        { 0.1875f, 0.0f,    0.1875f, 0.1875f },
        { 0.375f,  0.0f,    0.1875f, 0.1875f },
        { 0.5625f, 0.0f,    0.1875f, 0.1875f },
        { 0.75f,   0.0f,    0.1875f, 0.1875f },
        { 0.0f,    0.1875f, 0.1875f, 0.1875f },
        { 0.1875f, 0.1875f, 0.1875f, 0.1875f },
        { 0.375f,  0.1875f, 0.1875f, 0.1875f },
        { 0.5625f, 0.1875f, 0.1875f, 0.1875f },
        { 0.75f,   0.1875f, 0.1875f, 0.1875f },
        { 0.0f,    0.375f,  0.1875f, 0.1875f },
        { 0.1875f, 0.375f,  0.1875f, 0.1875f },
        { 0.375f,  0.375f,  0.1875f, 0.1875f },
        { 0.5625f, 0.375f,  0.1875f, 0.1875f },
        { 0.75f,   0.375f,  0.1875f, 0.1875f },
        { 0.0f,    0.5625f, 0.1875f, 0.1875f },
        { 0.1875f, 0.5625f, 0.1875f, 0.1875f },
        { 0.375f,  0.5625f, 0.1875f, 0.1875f },
        { 0.5625f, 0.5625f, 0.1875f, 0.1875f },
        { 0.75f,   0.5625f, 0.1875f, 0.1875f },
        { 0.0f,    0.75f,   0.1875f, 0.1875f },
        { 0.1875f, 0.75f,   0.1875f, 0.1875f },
        { 0.375f,  0.75f,   0.1875f, 0.1875f },
        { 0.5625f, 0.75f,   0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
    static EffectUv uvinfo_kemu4[17] = 
    {
        { 0.0f,     0.0f,     0.21875f, 0.21875f },
        { 0.21875f, 0.0f,     0.21875f, 0.21875f },
        { 0.4375f,  0.0f,     0.21875f, 0.21875f },
        { 0.65625f, 0.0f,     0.21875f, 0.21875f },
        { 0.0f,     0.21875f, 0.21875f, 0.21875f },
        { 0.21875f, 0.21875f, 0.21875f, 0.21875f },
        { 0.4375f,  0.21875f, 0.21875f, 0.21875f },
        { 0.65625f, 0.21875f, 0.21875f, 0.21875f },
        { 0.0f,     0.4375f,  0.21875f, 0.21875f },
        { 0.21875f, 0.4375f,  0.21875f, 0.21875f },
        { 0.4375f,  0.4375f,  0.21875f, 0.21875f },
        { 0.65625f, 0.4375f,  0.21875f, 0.21875f },
        { 0.0f,     0.65625f, 0.21875f, 0.21875f },
        { 0.21875f, 0.65625f, 0.21875f, 0.21875f },
        { 0.4375f,  0.65625f, 0.21875f, 0.21875f },
        { 0.65625f, 0.65625f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_kemu5[21] = 
    {
        { 0.0f,     0.0f,     0.125f,   0.125f   },
        { 0.0f,     0.125f,   0.125f,   0.125f   },
        { 0.0f,     0.25f,    0.125f,   0.125f   },
        { 0.0f,     0.375f,   0.125f,   0.125f   },
        { 0.125f,   0.0f,     0.1875f,  0.1875f  },
        { 0.125f,   0.1875f,  0.1875f,  0.1875f  },
        { 0.125f,   0.375f,   0.1875f,  0.1875f  },
        { 0.125f,   0.5625f,  0.1875f,  0.1875f  },
        { 0.3125f,  0.0f,     0.21875f, 0.21875f },
        { 0.3125f,  0.21875f, 0.21875f, 0.21875f },
        { 0.3125f,  0.4375f,  0.21875f, 0.21875f },
        { 0.3125f,  0.65625f, 0.21875f, 0.21875f },
        { 0.53125f, 0.0f,     0.21875f, 0.21875f },
        { 0.53125f, 0.21875f, 0.21875f, 0.21875f },
        { 0.53125f, 0.4375f,  0.21875f, 0.21875f },
        { 0.53125f, 0.65625f, 0.21875f, 0.21875f },
        { 0.75f,    0.0f,     0.21875f, 0.21875f },
        { 0.75f,    0.21875f, 0.21875f, 0.21875f },
        { 0.75f,    0.4375f,  0.21875f, 0.21875f },
        { 0.75f,    0.65625f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_kemu6[18] = 
    {
        { 0.0f,     0.0f,     0.15625f, 0.15625f },
        { 0.15625f, 0.0f,     0.15625f, 0.15625f },
        { 0.3125f,  0.0f,     0.15625f, 0.15625f },
        { 0.46875f, 0.0f,     0.1875f,  0.1875f  },
        { 0.65625f, 0.0f,     0.1875f,  0.1875f  },
        { 0.0f,     0.15625f, 0.21875f, 0.21875f },
        { 0.21875f, 0.15625f, 0.21875f, 0.21875f },
        { 0.4375f,  0.1875f,  0.21875f, 0.21875f },
        { 0.65625f, 0.1875f,  0.21875f, 0.21875f },
        { 0.0f,     0.375f,   0.21875f, 0.21875f },
        { 0.21875f, 0.375f,   0.21875f, 0.21875f },
        { 0.4375f,  0.40625f, 0.21875f, 0.21875f },
        { 0.65625f, 0.40625f, 0.21875f, 0.21875f },
        { 0.0f,     0.59375f, 0.21875f, 0.21875f },
        { 0.21875f, 0.59375f, 0.21875f, 0.21875f },
        { 0.4375f,  0.625f,   0.21875f, 0.21875f },
        { 0.65625f, 0.625f,   0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_kemu7[17] = 
    {
        { 0.0f,     0.0f,     0.15625f, 0.15625f },
        { 0.15625f, 0.0f,     0.15625f, 0.15625f },
        { 0.3125f,  0.0f,     0.15625f, 0.15625f },
        { 0.46875f, 0.0f,     0.1875f,  0.1875f  },
        { 0.65625f, 0.0f,     0.1875f,  0.1875f  },
        { 0.0f,     0.15625f, 0.21875f, 0.21875f },
        { 0.21875f, 0.15625f, 0.21875f, 0.21875f },
        { 0.4375f,  0.1875f,  0.21875f, 0.21875f },
        { 0.65625f, 0.1875f,  0.21875f, 0.21875f },
        { 0.0f,     0.375f,   0.25f,    0.25f    },
        { 0.25f,    0.40625f, 0.25f,    0.25f    },
        { 0.5f,     0.40625f, 0.25f,    0.25f    },
        { 0.0f,     0.625f,   0.25f,    0.25f    },
        { 0.25f,    0.65625f, 0.25f,    0.25f    },
        { 0.5f,     0.65625f, 0.25f,    0.25f    },
        { 0.75f,    0.40625f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_kemu8[18] = 
    {
        { 0.0f,     0.0f,     0.1875f,  0.1875f  },
        { 0.1875f,  0.0f,     0.1875f,  0.1875f  },
        { 0.375f,   0.0f,     0.1875f,  0.1875f  },
        { 0.5625f,  0.0f,     0.1875f,  0.1875f  },
        { 0.75f,    0.0f,     0.1875f,  0.1875f  },
        { 0.0f,     0.1875f,  0.21875f, 0.21875f },
        { 0.21875f, 0.1875f,  0.21875f, 0.21875f },
        { 0.4375f,  0.1875f,  0.21875f, 0.21875f },
        { 0.65625f, 0.1875f,  0.21875f, 0.21875f },
        { 0.0f,     0.40625f, 0.21875f, 0.21875f },
        { 0.21875f, 0.40625f, 0.21875f, 0.21875f },
        { 0.4375f,  0.40625f, 0.21875f, 0.21875f },
        { 0.65625f, 0.40625f, 0.21875f, 0.21875f },
        { 0.0f,     0.625f,   0.21875f, 0.21875f },
        { 0.21875f, 0.625f,   0.21875f, 0.21875f },
        { 0.4375f,  0.625f,   0.21875f, 0.21875f },
        { 0.65625f, 0.625f,   0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static int texid_kemu[10] = 
    {
        400, 400, 401, 402, 403, 404, 405, 406, 407, 400
    };
    static EffectUv* uvinfo_kemu[10] = 
    {
        uvinfo_kemu0,
        uvinfo_kemu1,
        uvinfo_kemu2,
        uvinfo_kemu3,
        uvinfo_kemu4,
        uvinfo_kemu5,
        uvinfo_kemu6,
        uvinfo_kemu7,
        uvinfo_kemu8,
        uvinfo_kemu0
    }; 

    kcolor = w.type % 10;
    ktype = w.type / 10;
    
    switch (w.mode0)
    {
    case 0:
        w.tex_id = texid_kemu[kcolor];
        
        effect_init(w);
        
        w.exp0 = (unsigned char*)uvinfo_kemu[kcolor];
        
        w.mode0 = 1;
        break;
    }
    
    vs.y = 0;
    vs.x = 0;
    vs.z = njFraction(w.sz);
    
    njUnitMatrix(NULL);
    
    njRotateXYZ(NULL, w.ax, w.ay, 0);
    njCalcVector(NULL, &vs, &vd);
    
    w.px += vd.x;
    w.py += vd.y;
    w.pz += vd.z;
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
        
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
        return;
    }
    
    effset(w, uvp, ktype);
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff183(EffectSprite& w) 
{
    w.flg |= 0x01000000;
    if (w.type == 0) 
    {
        if (w.mode1 != 0) 
        {
            w.type = w.mode1;
        }
    }

    if (w.type != 0) 
    {
        effect_set_entry(0xB8, (unsigned short)w.type - 1, w);
    }
}
void bhEff184(EffectSprite& w)
{
    EffectUv* uvp;
    NJS_POINT3 vs, vd;
    static EffectUv uvinfo_mizu_0[16] = 
    {
        { 0.0f,  0.0f,   0.25f, 0.25f  },
        { 0.25f, 0.0f,   0.25f, 0.25f  },
        { 0.5f,  0.0f,   0.25f, 0.25f  },
        { 0.75f, 0.0f,   0.25f, 0.25f  },
        { 0.0f,  0.25f,  0.25f, 0.25f  },
        { 0.25f, 0.25f,  0.25f, 0.25f  },
        { 0.5f,  0.25f,  0.25f, 0.25f  },
        { 0.75f, 0.25f,  0.25f, 0.25f  },
        { 0.0f,  0.5f,   0.25f, 0.25f  },
        { 0.25f, 0.5f,   0.25f, 0.25f  },
        { -3.0f, 2.0f,   0.0f,  0.0f   },
        { 0.5f,  0.5f,   0.25f, 0.125f },
        { 0.75f, 0.5f,   0.25f, 0.125f },
        { 0.5f,  0.625f, 0.25f, 0.125f },
        { 0.75f, 0.625f, 0.25f, 0.125f },
        { -1.0f, 0.0f,   0.0f,  0.0f   }
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 33;
        
        effect_init(w);
        
        w.exp0 = (unsigned char*)uvinfo_mizu_0;
            
        vs.y = 0;
        vs.x = 0;
        vs.z = w.sz / 10.0f;
            
        njUnitMatrix(NULL);
        
        njRotateXYZ(NULL, w.ax, w.ay, 0);
        njCalcVector(NULL, &vs, &vd);
            
        w.xn = vd.x;
        w.yn = vd.y;
        w.zn = vd.z;
            
        w.spd = 0.01f;
        
        w.ct1 = 99;
        
        w.mode0 = 1;
        break;
    }
    
    if (w.ct0 <= 10) 
    {
        w.px += w.xn + (((-effect_random() / -2.1474836E9f) - (-effect_random() / -2.1474836E9f)) / 16.0f);
        w.py += w.yn + (((-effect_random() / -2.1474836E9f) - (-effect_random() / -2.1474836E9f)) / 16.0f);
        w.pz += w.zn + (((-effect_random() / -2.1474836E9f) - (-effect_random() / -2.1474836E9f)) / 16.0f);
        
        w.py -= w.spd;
        
        w.spd += 0.01f;
    }
    
    if ((w.ct0 <= 9) && (w.type >= w.py)) 
    {
        w.py = w.type; // bug? 
        
        w.ct0 = 11;
        
        w.sxb *= 4.0f;
        w.syb *= 4.0f;
    }

    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1)
    {
        w.flg = 0;
        return;
    }
    else if (uvp->u == -2) 
    {
        w.ct0 = 0;
    }
    else if (uvp->u == -3)
    {
        if (--w.ct1 >= 0) 
        {
            w.ct0 = uvp->v;
        }
        else 
        {
            w.ct0++;
        }
    }
    else if (uvp->u == -4)
    {
        w.ct0++;
        return;
    }

    effset(w, (EffectUv*)w.exp0 + w.ct0, 0);
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff185(EffectSprite& w)
{
    EffectUv* uvp;
    int ct0; // not from DWARF
    static EffectUv uvinfo_button0[5] = 
    {
        { 0.0f,      0.0f, 0.234375f, 0.15625f },
        { 0.234375f, 0.0f, 0.234375f, 0.15625f },
        { 0.46875f,  0.0f, 0.234375f, 0.15625f },
        { 0.234375f, 0.0f, 0.234375f, 0.15625f },
        { -1.0f,     0.0f, 0.0f,      0.0f     }
    };
    static EffectUv uvinfo_button1[5] = 
    {
        { 0.0f,      0.15625f, 0.234375f, 0.15625f },
        { 0.234375f, 0.15625f, 0.234375f, 0.15625f },
        { 0.46875f,  0.15625f, 0.234375f, 0.15625f },
        { 0.234375f, 0.15625f, 0.234375f, 0.15625f },
        { -1.0f,     0.0f,     0.0f,      0.0f     }
    };
    static EffectUv uvinfo_button2[5] = 
    {
        { 0.0f,      0.3125f, 0.234375f, 0.15625f },
        { 0.234375f, 0.3125f, 0.234375f, 0.15625f },
        { 0.46875f,  0.3125f, 0.234375f, 0.15625f },
        { 0.234375f, 0.3125f, 0.234375f, 0.15625f },
        { -1.0f,     0.0f,    0.0f,      0.0f     }
    };
    static EffectUv uvinfo_button3[5] = 
    {
        { 0.0f,      0.46875f, 0.234375f, 0.15625f },
        { 0.234375f, 0.46875f, 0.234375f, 0.15625f },
        { 0.46875f,  0.46875f, 0.234375f, 0.15625f },
        { 0.234375f, 0.46875f, 0.234375f, 0.15625f },
        { -1.0f,     0.0f,     0.0f,      0.0f     }
    };
    static EffectUv uvinfo_button4[5] = 
    {
        { 0.0f,      0.625f, 0.234375f, 0.15625f },
        { 0.234375f, 0.625f, 0.234375f, 0.15625f },
        { 0.46875f,  0.625f, 0.234375f, 0.15625f },
        { 0.234375f, 0.625f, 0.234375f, 0.15625f },
        { -1.0f,     0.0f,   0.0f,      0.0f     }
    };
    static EffectUv uvinfo_button5[5] = 
    {
        { 0.0f,      0.78125f, 0.234375f, 0.15625f },
        { 0.234375f, 0.78125f, 0.234375f, 0.15625f },
        { 0.46875f,  0.78125f, 0.234375f, 0.15625f },
        { 0.234375f, 0.78125f, 0.234375f, 0.15625f },
        { -1.0f,     0.0f,     0.0f,      0.0f     }
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 410;
        
        w.flg |= 0x4000000;
        
        w.bl_src = 8;
        w.bl_dst = 6;
        
        w.ani_ct = 0;
        
        w.ct0 = 0;
        
        w.mode0 = 1;
        break;
    }
    
    switch (w.type)
    {
    case 1:
        w.exp0 = (unsigned char*)uvinfo_button0;
        break;
    case 2:
        w.exp0 = (unsigned char*)uvinfo_button1;
        break;
    case 3:
        w.exp0 = (unsigned char*)uvinfo_button2;
        break;
    case 4:
        w.exp0 = (unsigned char*)uvinfo_button3;
        break;
    case 5:
        w.exp0 = (unsigned char*)uvinfo_button4;
        break;
    case 6:
        w.exp0 = (unsigned char*)uvinfo_button5;
        break;
    default:
        return;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;

    w.tv[3].col = w.tv[2].col = w.tv[1].col = w.tv[0].col = 0xFFFFFFFF;
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;

    effect_queue_translucent(w);
    
    if (w.ct0 == 3)
    {
        ct0 = 0;
    }
    else
    {
        ct0 = w.ct0 + 1;
        
        w.ct0 = ct0;
    }

    w.ct0 = ct0;
}
void bhEff187(EffectSprite& w)
{
    unsigned char dt_interval0[10] = 
    {
        17, 22, 27, 32, 37, 42, 47, 52, 57, 62
    };
    unsigned char dt_interval1[10] = 
    {
        10, 15, 20, 25, 30, 35, 40, 45, 50, 55
    }; 
    
    w.flg |= 0x1000000;
    
    if ((w.type == 0) && (w.mode1 != 0))
    {
        w.type = w.mode1;
    }
    
    if (w.type != 0) 
    {
        switch ((unsigned short)w.type)
        {
        case 2:
            effect_set_entry(188, 1, w);
            
            w.mode1 = 0;
            w.type = 0;
            break;
        case 3:
            if (w.ct1 <= 0) 
            {
                effect_set_entry(188, 0, w);
                
                w.ct1 = dt_interval0[(unsigned char)(10.0f * (-effect_random() / -2.1474836E9f))];
            }
            
            w.ct1--;
            break;
        case 4:
            if (w.ct1 <= 0) 
            {
                effect_set_entry(188, 1, w);
                
                w.ct1 = dt_interval1[(unsigned char)(10.0f * (-effect_random() / -2.1474836E9f))];
            }
            
            w.ct1--;
            break;
        default:
            effect_set_entry(188, 0, w);
            
            w.mode1 = 0;
            w.type = 0;
            break;
        }
    }
}
void bhEff188(EffectSprite& w)
{
    EffectUv* uvp;
    static EffectUv uvinfo_houden_0[13] = 
    {
        { 0.0f,    0.0f,    0.1875f, 0.1875f },
        { 0.1875f, 0.0f,    0.1875f, 0.1875f },
        { 0.375f,  0.0f,    0.1875f, 0.1875f },
        { 0.5625f, 0.0f,    0.1875f, 0.1875f },
        { 0.75f,   0.0f,    0.1875f, 0.1875f },
        { 0.0f,    0.1875f, 0.1875f, 0.1875f },
        { 0.1875f, 0.1875f, 0.1875f, 0.1875f },
        { 0.375f,  0.1875f, 0.1875f, 0.1875f },
        { 0.5625f, 0.1875f, 0.1875f, 0.1875f },
        { 0.75f,   0.1875f, 0.1875f, 0.1875f },
        { 0.0f,    0.375f,  0.1875f, 0.1875f },
        { 0.1875f, 0.375f,  0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }   
    };
	static EffectUv uvinfo_houden_1[6] = 
    {
        { 0.0f,    0.5625f, 0.1875f, 0.1875f },
        { 0.1875f, 0.5625f, 0.1875f, 0.1875f },
        { 0.375f,  0.5625f, 0.1875f, 0.1875f },
        { 0.5625f, 0.5625f, 0.1875f, 0.1875f },
        { 0.75f,   0.5625f, 0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }   
    };

    switch (w.mode0)
    {
    case 0:
        w.tex_id = 412;
        
        effect_init(w);

        switch (w.type)
        {
        case 1:
            w.exp0 = (unsigned char*)&uvinfo_houden_1;
            break;
        default:
            w.exp0 = (unsigned char*)&uvinfo_houden_0;
            break;
        }
        
        w.mode0 = 1;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
    }
    else 
    {
        effset(w, uvp, 0);
        
        w.ct0++;
        
        effect_queue_translucent(w);
    }
}
void bhEff190(EffectSprite& w)
{
	EffectRequest* pEffect;

    if ((w.type == 0) && (w.mode1 != 0))
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    switch (w.mode0) 
    {
    case 0:
        w.ct0 = 6;
        
        w.mode0 = 1;
        break;
    }
    
    w.ct0++;
    
    if (w.ct0 >= 6) 
    {
        pEffect = &effect_request_template();
        
        pEffect->flg = 1;
        
        pEffect->id = 191;
        pEffect->type = 0;
        
        pEffect->flr_no = 0;
        
        pEffect->mdlver = 0;
        
        pEffect->px = w.px;
        pEffect->py = w.py;
        pEffect->pz = w.pz;
        
        pEffect->sx = w.sx;
        pEffect->sy = w.sy;
        pEffect->sz = w.sz;
        
        pEffect->ay = w.ay;
        pEffect->ax = w.ax;
        
        effect_request(*pEffect,NULL,NULL,0xFF);
        
        w.ct0 = 0;
    }
}
void bhEff192(EffectSprite& w)
{
    EffectRequest* pEffect;
	int lLoop;

    if ((w.type == 0) && (w.mode1 != 0))
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    pEffect = &effect_request_template();
    
    pEffect->flg = 1;
    
    pEffect->id = 193;
    pEffect->type = 0;
    
    pEffect->flr_no = 0;
    
    pEffect->mdlver = 0;
    
    pEffect->px = w.px;
    pEffect->py = w.py;
    pEffect->pz = w.pz;
    
    pEffect->sx = w.sx;
    pEffect->sy = w.sy;
    pEffect->sz = w.sz;
    
    pEffect->ay = w.ay;
    pEffect->ax = w.ax;
    
    for (lLoop = 0; lLoop < w.lkono; lLoop++)
    {
        effect_request(*pEffect,NULL,NULL,0xFF);
    }
    
    w.type = w.mode0 = w.mode1 = 0;
}
void bhEff194(EffectSprite& w)
{
	EffectRequest* pEffect;

    if ((w.type == 0) && (w.mode1 != 0))
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;

    pEffect = &effect_request_template();
    
    pEffect->flg = 1;
    
    pEffect->id = 195;
    pEffect->type = (w.type - 1) % 3;
    
    pEffect->mdlver = 0;
    
    pEffect->flr_no = 0;
    
    pEffect->px = w.px;
    pEffect->py = w.py;
    pEffect->pz = w.pz;
    
    pEffect->sx = w.sx;
    pEffect->sy = w.sy;
    pEffect->sz = w.sz;
    
    pEffect->ay = w.ay;
    pEffect->ax = w.ax;
    
    effect_request(*pEffect,NULL,NULL,0);
    
    w.type = w.mode1 = 0;
}
void bhEff002(EffectSprite& w) 
{
    EffectUv* uvp;
	static EffectUv uvinfo_v0[14] = 
    {
        { 0.0625f,  0.0f,     0.0625f,  0.0625f  },
        { 0.0625f,  0.0625f,  0.0625f,  0.0625f  },
        { 0.25f,    0.0f,     0.125f,   0.125f   },
        { 0.5f,     0.0f,     0.125f,   0.125f   },
        { 0.75f,    0.0f,     0.125f,   0.125f   },
        { 0.0f,     0.125f,   0.1875f,  0.1875f  },
        { 0.375f,   0.125f,   0.1875f,  0.1875f  },
        { 0.1875f,  0.3125f,  0.1875f,  0.1875f  },
        { 0.0f,     0.5f,     0.1875f,  0.1875f  },
        { 0.375f,   0.5f,     0.1875f,  0.1875f  },
        { 0.5625f,  0.125f,   0.21875f, 0.21875f },
        { 0.78125f, 0.125f,   0.21875f, 0.21875f },
        { 0.5625f,  0.34375f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_v1[7] = 
    {
        { 0.0f,     0.5f,     0.1875f,  0.1875f  },
        { 0.1875f,  0.5f,     0.1875f,  0.1875f  },
        { 0.375f,   0.5f,     0.1875f,  0.1875f  },
        { 0.5625f,  0.125f,   0.21875f, 0.21875f },
        { 0.78125f, 0.125f,   0.21875f, 0.21875f },
        { 0.5625f,  0.34375f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_v2[25] = 
    {
        { 0.0f,     0.0f,     0.0625f,  0.0625f  },
        { 0.0625f,  0.0f,     0.0625f,  0.0625f  },
        { 0.0f,     0.0625f,  0.0625f,  0.0625f  },
        { 0.0625f,  0.0625f,  0.0625f,  0.0625f  },
        { 0.125f,   0.0f,     0.125f,   0.125f   },
        { 0.25f,    0.0f,     0.125f,   0.125f   },
        { 0.375f,   0.0f,     0.125f,   0.125f   },
        { 0.5f,     0.0f,     0.125f,   0.125f   },
        { 0.625f,   0.0f,     0.125f,   0.125f   },
        { 0.75f,    0.0f,     0.125f,   0.125f   },
        { 0.875f,   0.0f,     0.125f,   0.125f   },
        { 0.0f,     0.125f,   0.1875f,  0.1875f  },
        { 0.1875f,  0.125f,   0.1875f,  0.1875f  },
        { 0.375f,   0.125f,   0.1875f,  0.1875f  },
        { 0.0f,     0.3125f,  0.1875f,  0.1875f  },
        { 0.1875f,  0.3125f,  0.1875f,  0.1875f  },
        { 0.0f,     0.5f,     0.1875f,  0.1875f  },
        { 0.1875f,  0.5f,     0.1875f,  0.1875f  },
        { 0.375f,   0.5f,     0.1875f,  0.1875f  },
        { 0.0f,     0.6875f,  0.1875f,  0.1875f  },
        { 0.5625f,  0.125f,   0.21875f, 0.21875f },
        { 0.78125f, 0.125f,   0.21875f, 0.21875f },
        { 0.5625f,  0.34375f, 0.21875f, 0.21875f },
        { 0.78125f, 0.34375f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_v3[14] = 
    {
        { 0.0f,     0.0f,     0.0625f,  0.0625f  },
        { 0.0f,     0.0625f,  0.0625f,  0.0625f  },
        { 0.125f,   0.0f,     0.125f,   0.125f   },
        { 0.375f,   0.0f,     0.125f,   0.125f   },
        { 0.625f,   0.0f,     0.125f,   0.125f   },
        { 0.875f,   0.0f,     0.125f,   0.125f   },
        { 0.1875f,  0.125f,   0.1875f,  0.1875f  },
        { 0.0f,     0.3125f,  0.1875f,  0.1875f  },
        { 0.375f,   0.3125f,  0.1875f,  0.1875f  },
        { 0.1875f,  0.5f,     0.1875f,  0.1875f  },
        { 0.5625f,  0.125f,   0.21875f, 0.21875f },
        { 0.78125f, 0.125f,   0.21875f, 0.21875f },
        { 0.5625f,  0.34375f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_v4[8] = 
    {
        { 0.875f,   0.0f,     0.125f,   0.125f   },
        { 0.75f,    0.0f,     0.125f,   0.125f   },
        { 0.625f,   0.0f,     0.125f,   0.125f   },
        { 0.5f,     0.0f,     0.125f,   0.125f   },
        { 0.375f,   0.0f,     0.125f,   0.125f   },
        { 0.25f,    0.0f,     0.125f,   0.125f   },
        { 0.125f,   0.0f,     0.125f,   0.125f   },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_v8[5] = 
    {
        { 0.375f,   0.3125f,  0.1875f,  0.1875f  },
        { 0.1875f,  0.5f,     0.1875f,  0.1875f  },
        { 0.5625f,  0.125f,   0.21875f, 0.21875f },
        { 0.5625f,  0.34375f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_v9[7] = 
    {
        { 0.0f,     0.3125f,  0.1875f,  0.1875f  },
        { 0.1875f,  0.3125f,  0.1875f,  0.1875f  },
        { 0.375f,   0.3125f,  0.1875f,  0.1875f  },
        { 0.0f,     0.5f,     0.1875f,  0.1875f  },
        { 0.1875f,  0.5f,     0.1875f,  0.1875f  },
        { 0.5625f,  0.125f,   0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_vz[6] = 
    {
        { 0.0f,     0.0f,     0.0625f,  0.0625f  },
        { 0.0f,     0.0f,     0.0625f,  0.0625f  },
        { 0.0625f,  0.0f,     0.0625f,  0.0625f  },
        { 0.0625f,  0.0f,     0.0625f,  0.0625f  },
        { 0.0f,     0.0625f,  0.0625f,  0.0625f  },
        { 0.0625f,  0.0625f,  0.0625f,  0.0625f  }
    };
        
    switch (w.mode0) 
    {                              
    case 0:                                         
        w.tex_id = 2;
        
        w.tv[0].col = 0x80FFFFFF;
        w.tv[1].col = 0x80FFFFFF;
        w.tv[2].col = 0x80FFFFFF;
        w.tv[3].col = 0x80FFFFFF;
        
        switch (w.type) 
        {                        
        case 2:                                     
            w.tv[0].col = 0x60C0C0C0;
            w.tv[1].col = 0x60C0C0C0;
            w.tv[2].col = 0x60C0C0C0;
            w.tv[3].col = 0x60C0C0C0;
        case 0:                                     
        case 6:                                     
            w.bl_src = 8;
            w.bl_dst = 6;
            
            w.yn = 0;
            break;
        case 3:                                     
            w.xn = (-effect_random()        / -2147483648.0f) * 0.1 - 0.05f;
            w.yn = 0.1f + (-effect_random() / -2147483648.0f) * 0.1;
            w.zn = (-effect_random()        / -2147483648.0f) * 0.1 - 0.05f;
        case 5:                                     
            if (w.mdlver == 11) 
            {
                w.tv[0].col = 0x40FFFFFF;
                w.tv[1].col = 0x40FFFFFF;
                w.tv[2].col = 0x40FFFFFF;
                w.tv[3].col = 0x40FFFFFF;
            } 
            else 
            {
                w.tv[0].col = 0x40C0C0C0;
                w.tv[1].col = 0x40C0C0C0;
                w.tv[2].col = 0x40C0C0C0;
                w.tv[3].col = 0x40C0C0C0;
            }
            
            w.ani_ct = 1;
            
            w.bl_src = 8;
            w.bl_dst = 6;
            break;
        case 4:                                     
            w.bl_src = 8;
            w.bl_dst = 6;
            
            w.yn = 0;
            
            w.ani_ct = 1;
            break;
        case 7:                                     
            w.tv[0].col = 0x40FFFFFF;
            w.tv[1].col = 0x40FFFFFF;
            w.tv[2].col = 0x40FFFFFF;
            w.tv[3].col = 0x40FFFFFF;
            
            w.bl_src = 8;
            w.bl_dst = 10;
            
            w.spd = 64.0f;
            
            w.ani_ct = 1;
            break;
        case 8:                                     
            w.xn = w.lox;
            w.yn = w.loy;
            w.zn = w.loz;
            
            w.tv[0].col = 0x40FFFFFF;
            w.tv[1].col = 0x40FFFFFF;
            w.tv[2].col = 0x40FFFFFF;
            w.tv[3].col = 0x40FFFFFF;
            
            w.ani_ct = 1;
            
            w.bl_src = 8;
            w.bl_dst = 10;
            break;
        case 1:
            w.flg = 0x4100001;
            
            w.bl_src = 11;
            w.bl_dst = 3;
            
            w.xn = (-effect_random()        / -2147483648.0f) * 0.2 - 0.1f;
            w.yn = 0.4f + (-effect_random() / -2147483648.0f) * 0.3;
            w.zn = (-effect_random()        / -2147483648.0f) * 0.2 - 0.1f;
            
            w.ani_ct = 1;
            break;
        case 9:
            w.xn = ((-effect_random() / -2147483648.0f) * 0.1) - 0.05f;
            w.yn = w.sz;
            w.zn = ((-effect_random() / -2147483648.0f) * 0.1) - 0.05f;
            
            w.tv[0].col = 0xC0202020;
            w.tv[1].col = 0xC0202020;
            w.tv[2].col = 0xC0202020;
            w.tv[3].col = 0xC0202020;
            
            w.ani_ct = 1;
            
            w.bl_src = 8;
            w.bl_dst = 6;
            break;
        case 10:                                    
            w.bl_src = 8;
            w.bl_dst = 6;
            
            w.ct0 = w.lkono;
            w.ct3 = 128;
            break;
        }
        
        switch (w.mdlver) 
        {                        
        case 1:                                     
            uvp = uvinfo_v1;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 2:                                     
            uvp = uvinfo_v2;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 3:                                     
            uvp = uvinfo_v3;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 4:                                     
            uvp = uvinfo_v4;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 8:                                     
            uvp = uvinfo_v8;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 9:                                     
            uvp = uvinfo_v9;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 10:                                    
            uvp = uvinfo_vz;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 11:                                    
            uvp = uvinfo_v2;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        default:                                    
            uvp = uvinfo_v0;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        }
        
        w.mode0 = 1;
        
        if (w.type < 9)
        {
            return;
        }
        
        break;
    case 1:
        switch (w.type) 
        {
        case 0:
            if (w.ct0 < 4) 
            {
                effect_request_template().id = 2;
                
                effect_request_template().flg = 0x4100001;
                
                effect_request_template().type = 10;
                
                effect_request_template().flr_no = 0;
                
                effect_request_template().mdlver = 10;
                
                effect_request_template().px = w.px;
                effect_request_template().py = w.py;
                effect_request_template().pz = w.pz;
                
                effect_request_template().sx = w.sxb;
                effect_request_template().sy = w.syb;
                effect_request_template().sz = w.szb;
                
                effect_request_template().ay = effect_request_template().ax = 0;
                
                effect_request(effect_request_template(),NULL,NULL,w.ct0);
            }
        case 6:
            w.px += w.lox;
            w.py += w.loy;
            w.pz += w.loz;
            
            w.lox *= 0.9f;
            w.loy *= 0.9f;
            w.loz *= 0.9f;
            
            w.py += w.yn;
            
            if (w.yn < 0.1f) 
            {
                w.yn += 0.01f;
            }
            
            break;
        case 3:
        case 8:
            if ((w.flg & 0x80))
            {
                effect_request_template().id = 2;
                
                effect_request_template().flg = 0x4100001;
                
                if (w.type == 9) 
                {
                    effect_request_template().type   = 9;
                    effect_request_template().mdlver = 3;
                } 
                else
                {
                    effect_request_template().type   = 3;
                    effect_request_template().mdlver = 11;
                }
                
                effect_request_template().flr_no = 0;
                
                effect_request_template().px = w.px;
                effect_request_template().py = w.py;
                effect_request_template().pz = w.pz;
                
                effect_request_template().sx = w.sxb;
                effect_request_template().sy = w.syb;
                effect_request_template().sz = w.szb;
                
                effect_request_template().ay = effect_request_template().ax = 0;
                
                effect_request(effect_request_template(),NULL,NULL,w.ct0);
                
                w.flr_no--;
                
                if (w.flr_no <= 0) 
                {
                    w.flg = 0;
                }
                
                return; 
            } 
            else 
            {
                w.px += w.xn;
                w.py += w.yn;
                w.pz += w.zn;
                
                w.yn = w.yn * 0.9f;
            }

            break; 
        case 4:
            if (w.ct0 < 6) 
            {
                effect_request_template().id = 2;
                
                effect_request_template().flg = 0x4100001;
                
                effect_request_template().type = 10;
                
                effect_request_template().flr_no = 0;
                
                effect_request_template().mdlver = 3;
                
                effect_request_template().px = w.px;
                effect_request_template().py = w.py;
                effect_request_template().pz = w.pz;
                
                effect_request_template().sx = w.sxb;
                effect_request_template().sy = w.syb;
                effect_request_template().sz = w.szb;
                
                effect_request_template().ay = effect_request_template().ax = 0;
                
                effect_request(effect_request_template(),NULL,NULL,w.ct0);
            }
            
            w.px += w.lox;
            w.py += w.loy;
            w.pz += w.loz;
            
            w.lox *= 0.9f;
            w.loy *= 0.9f;
            w.loz *= 0.9f;
            break;
        case 1:
        case 9:
            w.px += w.xn;
            w.py += w.yn;
            w.pz += w.zn;
            
            w.yn = w.yn * 0.9f;
            break;
        case 7:
            w.tv[0].col = ((unsigned int)w.spd << 24) | 0xFFFFFF;
            w.tv[1].col = ((unsigned int)w.spd << 24) | 0xFFFFFF;
            w.tv[2].col = ((unsigned int)w.spd << 24) | 0xFFFFFF;
            w.tv[3].col = ((unsigned int)w.spd << 24) | 0xFFFFFF;
            
            w.spd *= 0.9f;
            break;
        case 10:
            w.tv[0].col = (w.ct3 << 24) | 0xFFFFFF;
            w.tv[1].col = (w.ct3 << 24) | 0xFFFFFF;
            w.tv[2].col = (w.ct3 << 24) | 0xFFFFFF;
            w.tv[3].col = (w.ct3 << 24) | 0xFFFFFF;
            
            w.ct3 -= 32;
            
            if (w.ct3 <= 0)
            {
                w.flg = 0;
                return;
            }
            
            break;
        }
    }

    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
        return;
    }
    
    w.sx = 4.0f * (w.sxb * uvp->xs);
    w.sy = 4.0f * (w.syb * uvp->ys);
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    if (w.type < 10)
    {
        w.ct0++;
    }
    
    effect_queue_translucent(w);
}
void bhEff005(EffectSprite& w)
{
    EffectUv* uvp; 
	static EffectUv uvinfo_l[13] = 
    {
        { 0.3125f,  0.5f,     0.0625f,  0.0625f  },
        { 0.375f,   0.4375f,  0.125f,   0.125f   },
        { 0.5f,     0.4375f,  0.15625f, 0.15625f },
        { 0.65625f, 0.4375f,  0.15625f, 0.15625f },
        { 0.8125f,  0.4375f,  0.15625f, 0.15625f },
        { 0.0f,     0.5625f,  0.1875f,  0.1875f  },
        { 0.1875f,  0.5625f,  0.1875f,  0.1875f  },
        { 0.375f,   0.59375f, 0.1875f,  0.1875f  },
        { 0.5625f,  0.59375f, 0.1875f,  0.1875f  },
        { 0.0f,     0.78125f, 0.21875f, 0.21875f },
        { 0.21875f, 0.78125f, 0.21875f, 0.21875f },
        { 0.4375f,  0.78125f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_m[12] = 
    {
        { 0.0f,     0.25f,    0.0625f,  0.0625f  },
        { 0.0f,     0.15625f, 0.09375f, 0.09375f },
        { 0.09375f, 0.15625f, 0.125f,   0.125f   },
        { 0.21875f, 0.15625f, 0.15625f, 0.15625f },
        { 0.375f,   0.15625f, 0.15625f, 0.15625f },
        { 0.0f,     0.0f,     0.15625f, 0.15625f },
        { 0.15625f, 0.0f,     0.15625f, 0.15625f },
        { 0.3125f,  0.0f,     0.15625f, 0.15625f },
        { 0.46875f, 0.0f,     0.15625f, 0.15625f },
        { 0.625f,   0.0f,     0.15625f, 0.15625f },
        { 0.78125f, 0.125f,   0.15625f, 0.15625f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_s[14] = 
    {
        { 0.0f,    0.3125f, 0.0625f, 0.0625f  },
        { 0.0f,    0.5f,    0.0625f, 0.0625f  },
        { 0.0f,    0.375f,  0.0625f, 0.0625f  },
        { 0.0f,    0.4375f, 0.0625f, 0.0625f  },
        { 0.0625f, 0.3125f, 0.125f,  0.125f   },
        { 0.1875f, 0.3125f, 0.125f,  0.125f   },
        { 0.3125f, 0.3125f, 0.125f,  0.125f   },
        { 0.4375f, 0.3125f, 0.125f,  0.125f   },
        { 0.5625f, 0.3125f, 0.125f,  0.125f   },
        { 0.6875f, 0.3125f, 0.125f,  0.125f   },
        { 0.8125f, 0.3125f, 0.125f,  0.125f   },
        { 0.0625f, 0.4375f, 0.125f,  0.125f   },
        { 0.1875f, 0.4375f, 0.125f,  0.125f   },
        { -1.0f,   0.0f,    0.0f,    0.0f     }
    };
    
    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 5;
        
        w.flg |= 0x4100000;
        
        w.tv[0].col = -1;
        w.tv[1].col = -1;
        w.tv[2].col = -1;
        w.tv[3].col = -1;
        
        w.bl_src = 8;
        w.bl_dst = 6;
        
        w.ct0 = 0;
        
        w.ani_ct = w.mdlver;
        
        w.mode0 = 1;
        break;
    }
    
    switch (w.type) 
    {                            
    case 0:
        uvp = &uvinfo_s[w.ct0];
        break;
    case 1:
        uvp = &uvinfo_m[w.ct0];
        break;
    case 2:
        uvp = &uvinfo_l[w.ct0];
        break;
    }
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
        return;
    }
    
    w.sx = 8.0f * (w.sxb * uvp->xs);
    w.sy = 8.0f * (w.syb * uvp->ys);
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff008(EffectSprite& w) 
{
    EffectUv* uvp;
    static EffectUv uvinfo_f0[11] = 
    {
        { 0.0f,    0.0f,    0.1875f, 0.1875f },
        { 0.1875f, 0.0f,    0.1875f, 0.1875f },
        { 0.375f,  0.0f,    0.1875f, 0.1875f },
        { 0.5625f, 0.0f,    0.1875f, 0.1875f },
        { 0.75f,   0.0f,    0.1875f, 0.1875f },
        { 0.0f,    0.1875f, 0.1875f, 0.1875f },
        { 0.1875f, 0.1875f, 0.1875f, 0.1875f },
        { 0.375f,  0.1875f, 0.1875f, 0.1875f },
        { 0.5625f, 0.1875f, 0.1875f, 0.1875f },
        { 0.75f,   0.1875f, 0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
    static EffectUv uvinfo_fst[11] = 
    {
        { 0.0f,    0.375f,  0.1875f, 0.1875f },
        { 0.1875f, 0.375f,  0.1875f, 0.1875f },
        { 0.375f,  0.375f,  0.1875f, 0.1875f },
        { 0.5625f, 0.375f,  0.1875f, 0.1875f },
        { 0.75f,   0.375f,  0.1875f, 0.1875f },
        { 0.0f,    0.5625f, 0.1875f, 0.1875f },
        { 0.1875f, 0.5625f, 0.1875f, 0.1875f },
        { 0.375f,  0.5625f, 0.1875f, 0.1875f },
        { 0.5625f, 0.5625f, 0.1875f, 0.1875f },
        { 0.75f,   0.5625f, 0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
    static EffectUv uvinfo_f1[9] = 
    {
        { 0.0f,   0.875f, 0.125f, 0.125f },
        { 0.125f, 0.875f, 0.125f, 0.125f },
        { 0.25f,  0.875f, 0.125f, 0.125f },
        { 0.375f, 0.875f, 0.125f, 0.125f },
        { 0.5f,   0.875f, 0.125f, 0.125f },
        { 0.625f, 0.875f, 0.125f, 0.125f },
        { 0.75f,  0.875f, 0.125f, 0.125f },
        { 0.875f, 0.875f, 0.125f, 0.125f },
        { -1.0f,  0.0f,   0.0f,   0.0f   }
    };
    
    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 8;
        
        w.flg |= 0x4100000;
        
        w.tv[0].x = -1.0f;
        w.tv[0].y = -2.0f;
        w.tv[0].z = 0;
        
        w.tv[1].x = 1.0f;
        w.tv[1].y = -2.0f;
        w.tv[1].z = 0;
        
        w.tv[2].x = -1.0f;
        w.tv[2].y = 0;
        w.tv[2].z = 0;
        
        w.tv[3].x = 1.0f;
        w.tv[3].y = 0;
        w.tv[3].z = 0;
        
        w.tv[0].col = 0xFFE0E0E0;
        w.tv[1].col = 0xFFE0E0E0;
        w.tv[2].col = 0xFFE0E0E0;
        w.tv[3].col = 0xFFE0E0E0;
        
        w.bl_src = 8;
        w.bl_dst = 6;
        
        w.ani_ct = 0;
        
        w.ct0 = 0;
        
        switch (w.type) 
        {                          
        case 1:
            uvp = uvinfo_f1;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 2:
            uvp = uvinfo_fst;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        default:
            uvp = uvinfo_f0;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        }
        
        w.mode0 = 1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        if (w.type == 2) 
        {
            w.exp0 = (unsigned char*)uvinfo_f1;
            
            w.type = 0;
        }
        
        w.ct0 = 0;
        
        uvp = (EffectUv*)w.exp0 + w.ct0;
    }
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff010(EffectSprite& w) 
{
    EffectUv* uvp;
	static EffectUv uvinfo_f0[5] = 
    {
        { 0.0f,     0.640625f, 0.21875f, 0.21875f },
        { 0.21875f, 0.640625f, 0.21875f, 0.21875f },
        { 0.4375f,  0.640625f, 0.1875f,  0.15625f },
        { 0.625f,   0.65625f,  0.1875f,  0.15625f },
        { -1.0f,    0.0f,      0.0f,     0.0f     }
    };
    
    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 19;
        
        w.flg |= 0x4100000;
        
        w.tv[0].x = -1.0f;
        w.tv[0].y = -2.0f;
        w.tv[0].z = 0;
        
        w.tv[1].x = 1.0f;
        w.tv[1].y = -2.0f;
        w.tv[1].z = 0;
        
        w.tv[2].x = -1.0f;
        w.tv[2].y = 0;
        w.tv[2].z = 0;
        
        w.tv[3].x = 1.0f;
        w.tv[3].y = 0;
        w.tv[3].z = 0;
        
        w.tv[0].col = -1;
        w.tv[1].col = -1;
        w.tv[2].col = -1;
        w.tv[3].col = -1;
        
        if (w.type < 2) 
        {
            w.bl_src = 11;
            w.bl_dst = 3;
        } 
        else 
        {
            w.bl_src = 8;
            w.bl_dst = 6;
        }
        
        w.yn = w.py;
        
        w.ani_ct = 0;
        
        w.ct0 = 0;
            
        w.exp0 = (unsigned char*)uvinfo_f0;
        
        w.mode0 = 1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        if (w.type != 0) 
        {
            w.flg = 0;
            return;
        }
        
        w.py = w.yn;
        
        w.ct0 = 0;
        
        uvp = (EffectUv*)w.exp0 + w.ct0;
    }
    
    if (w.type < 2) 
    {
        w.py += 0.8f;
    }
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff011(EffectSprite& w)
{
    EffectUv* uvp;
	static EffectUv uvinfo_t0[18] = 
    {
        { 0.28125f,   0.0f,       0.03125f,    0.03125f    },
        { 0.3125f,    0.0f,       0.0625f,     0.0625f     },
        { 0.375f,     0.0f,       0.0703125f,  0.0703125f  },
        { 0.4453125f, 0.0f,       0.08984375f, 0.08984375f },
        { 0.5390625f, 0.0f,       0.08984375f, 0.08984375f },
        { 0.15625f,   0.0f,       0.125f,      0.125f      },
        { 0.75f,      0.0f,       0.125f,      0.125f      },
        { 0.0f,       0.0f,       0.15625f,    0.15625f    },
        { 0.0f,       0.15625f,   0.171875f,   0.171875f   },
        { 0.171875f,  0.125f,     0.171875f,   0.171875f   },
        { 0.34375f,   0.125f,     0.1953125f,  0.1953125f  },
        { 0.5390625f, 0.1015625f, 0.2109375f,  0.2109375f  },
        { 0.09375f,   0.3203125f, 0.21875f,    0.203125f   },
        { 0.3125f,    0.3203125f, 0.21875f,    0.203125f   },
        { 0.53125f,   0.3203125f, 0.21875f,    0.203125f   },
        { 0.75f,      0.125f,     0.21875f,    0.203125f   },
        { 0.75f,      0.328125f,  0.21875f,    0.203125f   },
        { -1.0f,      0.0f, 0.0f, 0.0f                     }
    };
    static EffectUv uvinfo_tb[17] = 
    {
        { 0.0f,     0.0f,     0.21875f, 0.21875f },
        { 0.21875f, 0.0f,     0.21875f, 0.21875f },
        { 0.4375f,  0.0f,     0.21875f, 0.21875f },
        { 0.65625f, 0.0f,     0.21875f, 0.21875f },
        { 0.0f,     0.21875f, 0.21875f, 0.21875f },
        { 0.21875f, 0.21875f, 0.21875f, 0.21875f },
        { 0.4375f,  0.21875f, 0.21875f, 0.21875f },
        { 0.65625f, 0.21875f, 0.21875f, 0.21875f },
        { 0.0f,     0.4375f,  0.21875f, 0.21875f },
        { 0.21875f, 0.4375f,  0.21875f, 0.21875f },
        { 0.4375f,  0.4375f,  0.21875f, 0.21875f },
        { 0.65625f, 0.4375f,  0.21875f, 0.21875f },
        { 0.0f,     0.65625f, 0.21875f, 0.21875f },
        { 0.21875f, 0.65625f, 0.21875f, 0.21875f },
        { 0.4375f,  0.65625f, 0.21875f, 0.21875f },
        { 0.65625f, 0.65625f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_c0[10] = 
    {
        { 0.0f,     0.4375f, 0.125f,   0.125f   },
        { 0.125f,   0.4375f, 0.125f,   0.125f   },
        { 0.25f,    0.4375f, 0.125f,   0.125f   },
        { 0.15625f, 0.5625f, 0.15625f, 0.15625f },
        { 0.3125f,  0.5625f, 0.15625f, 0.15625f },
        { 0.46875f, 0.5625f, 0.15625f, 0.15625f },
        { 0.625f,   0.625f,  0.1875f,  0.1875f  },
        { 0.625f,   0.4375f, 0.1875f,  0.1875f  },
        { 0.8125f,  0.4375f, 0.1875f,  0.1875f  },
        { -1.0f,    0.0f,    0.0f,     0.0f     }
    };
    static EffectUv uvinfo_c1[11] = 
    {
        { 0.09375f, 0.0f,     0.09375f, 0.09375f },
        { 0.1875f,  0.0f,     0.09375f, 0.09375f },
        { 0.28125f, 0.0f,     0.09375f, 0.09375f },
        { 0.0f,     0.09375f, 0.09375f, 0.09375f },
        { 0.09375f, 0.09375f, 0.09375f, 0.09375f },
        { 0.1875f,  0.09375f, 0.125f,   0.125f   },
        { 0.375f,   0.0f,     0.125f,   0.125f   },
        { 0.5f,     0.0f,     0.15625f, 0.15625f },
        { 0.65625f, 0.0f,     0.15625f, 0.15625f },
        { 0.8125f,  0.0f,     0.15625f, 0.15625f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_c2[11] = 
    {
        { 0.09375f, 0.21875f, 0.09375f, 0.09375f },
        { 0.1875f,  0.21875f, 0.09375f, 0.09375f },
        { 0.28125f, 0.21875f, 0.09375f, 0.09375f },
        { 0.0f,     0.3125f,  0.09375f, 0.09375f },
        { 0.09375f, 0.3125f,  0.09375f, 0.09375f },
        { 0.1875f,  0.3125f,  0.125f,   0.125f   },
        { 0.375f,   0.21875f, 0.125f,   0.125f   },
        { 0.5f,     0.15625f, 0.15625f, 0.15625f },
        { 0.65625f, 0.15625f, 0.15625f, 0.15625f },
        { 0.8125f,  0.15625f, 0.15625f, 0.15625f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_c3[6] = 
    {
        { 0.5f,   0.3125f, 0.125f, 0.125f },
        { 0.625f, 0.3125f, 0.125f, 0.125f },
        { 0.75f,  0.3125f, 0.125f, 0.125f },
        { 0.875f, 0.3125f, 0.125f, 0.125f },
        { 0.5f,   0.4375f, 0.125f, 0.125f },
        { -1.0f,  0.0f,    0.0f,   0.0f   }
    };
    static EffectUv uvinfo_c4[11] = 
    {
        { 0.0f,     0.71875f, 0.125f,   0.125f   },
        { 0.125f,   0.71875f, 0.125f,   0.125f   },
        { 0.25f,    0.71875f, 0.125f,   0.125f   },
        { 0.0f,     0.84375f, 0.15625f, 0.15625f },
        { 0.15625f, 0.84375f, 0.15625f, 0.15625f },
        { 0.3125f,  0.84375f, 0.15625f, 0.15625f },
        { 0.46875f, 0.84375f, 0.15625f, 0.15625f },
        { 0.625f,   0.8125f,  0.1875f,  0.1875f  },
        { 0.8125f,  0.8125f,  0.1875f,  0.1875f  },
        { 0.8125f,  0.625f,   0.1875f,  0.1875f  },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_smk[9] = 
    {
        { 0.0f,     0.5f,     0.1875f,  0.1875f  },
        { 0.1875f,  0.5f,     0.1875f,  0.1875f  },
        { 0.375f,   0.5f,     0.1875f,  0.1875f  },
        { 0.0f,     0.6875f,  0.1875f,  0.1875f  },
        { 0.5625f,  0.125f,   0.21875f, 0.21875f },
        { 0.78125f, 0.125f,   0.21875f, 0.21875f },
        { 0.5625f,  0.34375f, 0.21875f, 0.21875f },
        { 0.78125f, 0.34375f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_oil[13] = 
    {
        { 0.0f,     0.0f,    0.0625f,  0.0625f  },
        { 0.0625f,  0.0f,    0.09375f, 0.09375f },
        { 0.15625f, 0.0f,    0.125f,   0.125f   },
        { 0.28125f, 0.0f,    0.125f,   0.125f   },
        { 0.40625f, 0.0f,    0.15625f, 0.15625f },
        { 0.5625f,  0.0f,    0.1875f,  0.1875f  },
        { 0.75f,    0.0f,    0.1875f,  0.1875f  },
        { 0.0f,     0.1875f, 0.1875f,  0.1875f  },
        { 0.1875f,  0.1875f, 0.1875f,  0.1875f  },
        { 0.375f,   0.1875f, 0.1875f,  0.1875f  },
        { 0.5625f,  0.1875f, 0.1875f,  0.1875f  },
        { 0.75f,    0.1875f, 0.1875f,  0.1875f  },
        { -1.0f,    0.0f,    0.0f,     0.0f     }
    };
    
    switch (w.mode0) 
    {
    case 0:
        w.flg = 0x4100001;
        
        w.tv[0].col = -1;
        w.tv[1].col = -1;
        w.tv[2].col = -1;
        w.tv[3].col = -1;
        
        if (w.lkono == 1) 
        {
            w.tv[0].x = 1.0f;
            w.tv[0].y = -1.0f;
            w.tv[0].z = 0;
            
            w.tv[1].x = -1.0f;
            w.tv[1].y = -1.0f;
            w.tv[1].z = 0;
            
            w.tv[2].x = 1.0f;
            w.tv[2].y = 1.0f;
            w.tv[2].z = 0;
            
            w.tv[3].x = -1.0f;
            w.tv[3].y = 1.0f;
            w.tv[3].z = 0;
        }
        
        switch (w.mdlver) 
        {                          
        case 0:                                     
            w.tex_id = 34;
            
            w.bl_src = 8;
            w.bl_dst = 10;
            
            w.px -= 0.5f * njSin(w.ay);
            w.pz -= 0.5f * njCos(w.ay);
            
            w.ani_ct = 0;
            w.ct0    = 0;
            
            switch (w.type) 
            {                    
            case 0:                                 
                w.xn = 0;
                
                if (w.lkono < 2)
                {
                    w.yn = 0.4f;
                }
                else 
                {
                    w.yn = 0.5f;
                }
                
                w.exp0 = (unsigned char*)uvinfo_tb;
                
                w.ani_ct = 1;
                break;
            case 1:                                 
                w.xn = 0;
                
                if (w.lkono < 2) 
                {
                    w.yn = 0.8f;
                }
                else 
                {
                    w.yn = 1.0f;
                }
                
                w.exp0 = (unsigned char*)uvinfo_tb;
                
                w.ani_ct = 1;
                break;
            case 2:                                 
                w.tex_id = 36;
                
                w.xn = 0;
                
                if (w.lkono < 2) 
                {
                    w.yn = 0.4f;
                } 
                else 
                {
                    w.yn = 0.5f;
                }
                
                w.exp0 = (unsigned char*)uvinfo_oil;
                break;
            case 10:                                
                w.xn = 0;
                
                if (w.lkono < 2) 
                {
                    w.yn = 0.2f;
                }
                else 
                {
                    w.yn = 0.25f;
                }
                
                w.exp0 = (unsigned char*)uvinfo_t0;
                break;
            case 11:                                
                w.xn = 0;
                
                if (w.lkono < 2) 
                {
                    w.yn = 0.4f;
                } 
                else 
                {
                    w.yn = 0.5f;
                }
                
                w.exp0 = (unsigned char*)uvinfo_t0;
                break;
            }
            
            break;
        case 1:                                     
            w.tex_id = 11;
            
            if (w.type != 3) 
            {
                w.bl_src = 8;
                w.bl_dst = 6;
            } 
            else 
            {
                w.bl_src = 8;
                w.bl_dst = 10;
            }
            
            w.ct0 = 0;
            
            switch (w.flr_no) 
            {
            case 1:
                w.px -= 0.5f * njSin(w.ay);
                w.pz -= 0.5f * njCos(w.ay);
                
                w.xn = 0.02f  * w.sx;
                w.yn = 0.333f * w.sy;
                break;
            default:
                w.xn = 0;
                w.yn = 0;
                break;
            }
            
            w.ani_ct = w.type;
            
            switch (w.type) 
            {                      
            case 0:                                 
                w.exp0 = (unsigned char*)uvinfo_c0;
                break;
            case 1:                                 
                w.exp0 = (unsigned char*)uvinfo_c1;
                break;
            case 2:                                 
                w.exp0 = (unsigned char*)uvinfo_c2;
                break;
            case 3:                                 
                w.exp0 = (unsigned char*)uvinfo_c3;
                break;
            case 4:                                 
                w.exp0 = (unsigned char*)uvinfo_c4;
                break;
            default:                                
                w.bl_src = 8;
                w.bl_dst = 10;
                
                w.exp0 = (unsigned char*)uvinfo_smk;
                
                w.tex_id = 2;
                
                w.tv[0].col = 0xC0FFDCC0;
                w.tv[1].col = 0xC0FFDCC0;
                w.tv[2].col = 0xC0FFDCC0;
                w.tv[3].col = 0xC0FFDCC0;
                
                w.ani_ct = 1;
                break;
            }
            
            break;
        }
        
        w.mode0 = 1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
        return;
    }
    
    w.sx = 8.0f * (w.sxb * uvp->xs);
    w.sy = 8.0f * (w.syb * uvp->ys);
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    switch (w.mdlver) 
    {                            
    case 0:                                         
        w.px -= w.xn * njSin(w.ay);
        w.pz -= w.xn * njCos(w.ay);
        
        switch (w.type) 
        {                        
        case 0:      
        case 2:   
            w.py += w.yn;
            
            w.xn += 0.02f;
            w.yn -= 0.06f;
            break;
        case 1:                                     
            w.py += w.yn;
            
            w.xn += 0.03f;
            w.yn -= 0.15f;
            break;
        case 10:                                    
            w.py += w.yn;
            
            w.xn += 0.02f;
            w.yn -= 0.02f;
            break;
        case 11:                                    
            w.py += w.yn;
            
            w.xn += 0.03f;
            w.yn -= 0.035f;
            break;
        }
        
        break;
    case 1:                                         
        if (w.flr_no != 0) 
        {
            w.px -= w.xn * njSin(w.ay);
            w.pz -= w.xn * njCos(w.ay);
            w.py += w.yn;
            
            w.yn -= 0.03f;
        } 
        else
        {
            w.px += w.lox;
            w.py += w.loy;
            w.pz += w.loz;
            
            w.lox *= 0.7f;
            w.loy *= 0.7f;
            w.loz *= 0.7f;
        }
        
        break;
    }
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff014(EffectSprite& w) 
{
    EffectUv* uvp;
    static EffectUv uvinfo_f0[8] = 
    {
        { 0.0f,      0.484375f, 0.09375f, 0.09375f },
        { 0.109375f, 0.484375f, 0.09375f, 0.09375f },
        { 0.21875f,  0.484375f, 0.125f,   0.125f   },
        { 0.359375f, 0.484375f, 0.15625f, 0.15625f },
        { 0.53125f,  0.484375f, 0.15625f, 0.15625f },
        { 0.6875f,   0.484375f, 0.15625f, 0.15625f },
        { 0.84375f,  0.484375f, 0.15625f, 0.15625f },
        { -1.0f,     0.0f,      0.0f,     0.0f     }
    };
    static EffectUv uvinfo_f1[4] = 
    {
        { 0.15625f,  0.578125f, 0.0625f, 0.0625f },
        { 0.078125f, 0.578125f, 0.0625f, 0.0625f },
        { 0.0f,      0.578125f, 0.0625f, 0.0625f },
        { -1.0f,     0.0f,      0.0f,    0.0f    }
    };
    static EffectUv uvinfo_f2[6] = 
    {
        { 0.40625f,  0.15625f, 0.0625f,  0.0625f  },
        { 0.484375f, 0.125f,   0.09375f, 0.09375f },
        { 0.59375f,  0.125f,   0.09375f, 0.09375f },
        { 0.703125f, 0.125f,   0.09375f, 0.09375f },
        { 0.8125f,   0.125f,   0.09375f, 0.09375f },
        { -1.0f,     0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_f3[1] = 
    {
        { 0.5625f, 0.0625f, 0.0625f, 0.0625f }
    };
    
    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 3;
        
        w.tv[0].col = 0xA0FFFFFF;
        w.tv[1].col = 0xA0FFFFFF;
        w.tv[2].col = 0xA0FFFFFF;
        w.tv[3].col = 0xA0FFFFFF;
        
        w.bl_src = 8;
        w.bl_dst = 10;
        
        w.ani_ct = 0;
        w.ct0    = 0;
        
        switch (w.type) 
        {                         
        case 0:
            uvp = uvinfo_f0;
            
            w.ani_ct = 2;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 1:
            uvp = uvinfo_f1;
            
            w.ani_ct = 2;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 2:
            uvp = uvinfo_f2;
            
            w.ani_ct = 0;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 3:
            w.tv[0].x = 0;
            w.tv[0].y = -1.0f;
            w.tv[0].z = -1.0f;
            
            w.tv[1].x = 0;
            w.tv[1].y = 1.0f;
            w.tv[1].z = -1.0f;
            
            w.tv[2].x = 0;
            w.tv[2].y = -1.0f;
            w.tv[2].z = 1.0f;
            
            w.tv[3].x = 0;
            w.tv[3].y = 1.0f;
            w.tv[3].z = 1.0f;
            
            w.ct1 = 200;
            
            uvp = uvinfo_f3;
            
            w.ani_ct = 0;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        }
        
        w.mode0 = 1;
        break;
    }
    
    if (w.type == 2)
    {
        w.px += w.lox;
        w.py += w.loy;
        w.pz += w.loz;
        
        w.lox *= 0.8f;
        w.loy *= 0.8f;
        w.loz *= 0.8f;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (((w.type != 3) && (uvp->u == -1.0f)) || ((w.type == 3) && (w.ct1 <= 0))) 
    {
        w.flg = 0;
        return;
    }
    
    if (w.type != 3) 
    {
        w.sx = 8.0f * (w.sxb * uvp->xs);
        w.sy = 8.0f * (w.syb * uvp->ys);
        
        w.ct0++;
    } 
    else 
    {
        w.tv[0].col = (w.ct1 << 24) | 0xFFFFFF;
        w.tv[1].col = (w.ct1 << 24) | 0xFFFFFF;
        w.tv[2].col = (w.ct1 << 24) | 0xFFFFFF;
        w.tv[3].col = (w.ct1 << 24) | 0xFFFFFF;
        
        w.ct1 -= 4;
    }
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    effect_queue_translucent(w);
}
void bhEff015(EffectSprite& w)
{
    EffectUv* uvp; 
    float xn;     
	static EffectUv uvinfo_t0[11] = 
    {
        { 0.0f,  0.0f,  0.25f, 0.25f },
        { 0.25f, 0.0f,  0.25f, 0.25f },
        { 0.5f,  0.0f,  0.25f, 0.25f },
        { 0.75f, 0.0f,  0.25f, 0.25f },
        { 0.0f,  0.25f, 0.25f, 0.25f },
        { 0.25f, 0.25f, 0.25f, 0.25f },
        { 0.5f,  0.25f, 0.25f, 0.25f },
        { 0.75f, 0.25f, 0.25f, 0.25f },
        { 0.0f,  0.5f,  0.25f, 0.25f },
        { 0.25f, 0.5f,  0.25f, 0.25f },
        { -1.0f, 0.0f,  0.0f,  0.0f  }
    };
    static EffectUv uvinfo_t1[5] = 
    {
        { 0.5f,  0.5f,   0.25f, 0.125f },
        { 0.75f, 0.5f,   0.25f, 0.125f },
        { 0.5f,  0.625f, 0.25f, 0.125f },
        { 0.75f, 0.625f, 0.25f, 0.125f },
        { -1.0f, 0.0f,   0.0f,  0.0f   }
    };
    static EffectUv uvinfo_t2[12] = 
    {
        { 0.09375f, 0.90625f, 0.125f,   0.09375f },
        { 0.21875f, 0.84375f, 0.125f,   0.15625f },
        { 0.34375f, 0.8125f,  0.125f,   0.1875f  },
        { 0.46875f, 0.6875f,  0.125f,   0.3125f  },
        { 0.59375f, 0.625f,   0.1875f,  0.375f   },
        { 0.78125f, 0.5625f,  0.21875f, 0.4375f  },
        { 0.0f,     0.53125f, 0.1875f,  0.375f   },
        { 0.1875f,  0.53125f, 0.15625f, 0.28125f },
        { 0.34375f, 0.53125f, 0.125f,   0.25f    },
        { 0.46875f, 0.53125f, 0.125f,   0.15625f },
        { 0.59375f, 0.53125f, 0.125f,   0.09375f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    static EffectUv uvinfo_oil[9] = 
    {
        { 0.375f,  0.375f,  0.1875f, 0.1875f },
        { 0.5625f, 0.375f,  0.1875f, 0.1875f },
        { 0.0f,    0.5625f, 0.1875f, 0.1875f },
        { 0.1875f, 0.5625f, 0.1875f, 0.1875f },
        { 0.375f,  0.5625f, 0.1875f, 0.1875f },
        { 0.5625f, 0.5625f, 0.1875f, 0.1875f },
        { 0.0f,    0.75f,   0.1875f, 0.1875f },
        { 0.1875f, 0.75f,   0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
    static EffectUv uvinfo_oilh[5] = 
    {
        { 0.375f,  0.75f,    0.1875f, 0.09375f },
        { 0.5625f, 0.75f,    0.1875f, 0.09375f },
        { 0.375f,  0.84375f, 0.1875f, 0.09375f },
        { 0.5625f, 0.84375f, 0.1875f, 0.09375f },
        { -1.0f,   0.0f,     0.0f,    0.0f     }
    };

    switch (w.mode0) 
    {
    case 0:
        w.flg = 0x4100001;
        
        w.tex_id = 33;
        
        w.tv[0].col = 0xA0FFFFFF;
        w.tv[1].col = 0xA0FFFFFF;
        w.tv[2].col = 0xA0FFFFFF;
        w.tv[3].col = 0xA0FFFFFF;
        
        w.bl_src = 8;
        w.bl_dst = 6;
        
        w.ani_ct = 0;
        
        switch (w.type) 
        {                          
        case 0:                                     
            w.xn = 0.7f;
            w.yn = 0.2f;
            
            uvp = uvinfo_t0;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 1:                                     
            uvp = uvinfo_t1;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 2:                                     
            w.tex_id = 34;
            
            w.bl_src = 8;
            w.bl_dst = 10;
            
            w.tv[0].x = -1.0f;
            w.tv[0].y = -2.0f;
            w.tv[0].z = 0;
            
            w.tv[1].x = 1.0f;
            w.tv[1].y = -2.0f;
            w.tv[1].z = 0;
            
            w.tv[2].x = -1.0f;
            w.tv[2].y = 0;
            w.tv[2].z = 0;
            
            w.tv[3].x = 1.0f;
            w.tv[3].y = 0;
            w.tv[3].z = 0;
            
            uvp = uvinfo_t2;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 3:                                     
            w.tex_id = 31;
            
            w.xn = 0.4f;
            w.yn = 0.3f;
            
            uvp = uvinfo_t0;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 4:                                     
            w.tex_id = 31;
            
            uvp = uvinfo_t1;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 5:                                     
            w.tex_id = 36;
            
            uvp = uvinfo_oil;
            
            w.gpx = w.px;
            w.gpy = w.py;
            w.gpz = w.pz;
            
            w.xn = w.sz;
            w.yn = w.sy;
            
            w.sx = w.sxb = w.sx;
            w.sy = w.syb = w.sx;
            w.sz = w.szb = w.sx;
            
            w.ct1 = w.mdlver;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        case 6:                                     
            w.tex_id = 36;
            
            uvp = uvinfo_oilh;
            
            w.exp0 = (unsigned char*)uvp;
            break;
        }
        
        w.ct0   = 0;
        w.mode0 = 1;
        break;
    }
    
    if (w.type == 2) 
    {
        if (w.flr_no > 0) 
        {
            w.flg |= 0x1000000;
            
            w.flr_no--;
            return;
        }
        
        w.flg &= ~0x1000000; 
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        if (w.type == 5) 
        {
            w.ct0 = 0;
            
            uvp = (EffectUv*)w.exp0 + w.ct0;
        }
        else 
        {
            w.flg = 0;
            
            if ((w.type == 0) || (w.type == 3)) 
            {
                effect_request_template().id = 15;
                
                effect_request_template().flg = 0x4100001;
                
                effect_request_template().mdlver = 0;
                
                effect_request_template().type = w.type + 1;
                
                effect_request_template().sx = 2.0f;
                effect_request_template().sy = 2.0f;
                effect_request_template().sz = 2.0f;
                
                effect_request_template().px = w.px + ((2.0f * (-effect_random() / -2.1474836E9f)) - 1.0f);
                effect_request_template().py = w.py;
                effect_request_template().pz = w.pz + ((2.0f * (-effect_random() / -2.1474836E9f)) - 1.0f);
                
                effect_request_template().ay = 0;
                
                effect_request(effect_request_template(),NULL,NULL,0);
            }
            
            return;
        }
    }
    
    if (w.type == 2) 
    {
        w.sx = 8.0f * (w.sxb * uvp->xs);
        w.sy = 8.0f * (w.syb * uvp->ys);
    }
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    switch (w.type) 
    {                        
    case 0:  
    case 3:
        w.sx += 0.1f;
        w.sy += 0.1f;
        
        w.px -= w.xn * njSin(w.ay);
        w.pz -= w.xn * njCos(w.ay);
        w.py += w.yn;
        
        w.xn -= 0.07f * w.xn;
        w.yn -= 0.15f; 
        break; 
    case 5:   
        xn = w.xn * -njSin(w.ct1);
        
        w.px = w.gpx + (xn     * njSin(w.ay));
        w.pz = w.gpz + (xn     * njCos(w.ay));
        w.py = w.gpy + (w.yn * njCos(w.ct1));
        
        w.sx += 0.001f;
        
        w.ct1 += 512;
        
        if (w.ct1 >= 16384) 
        {
            w.flg = 0;
            
            effect_request_template().id = 15;
            
            effect_request_template().flg = 0x4100001;
            
            effect_request_template().mdlver = 0;
            
            effect_request_template().type = w.type + 1;
            
            effect_request_template().sx = 1.2f * w.sx;
            effect_request_template().sy = 1.2f * w.sy;
            effect_request_template().sz = 1.2f * w.sz;
            
            effect_request_template().px = w.px + ((-effect_random() / -2.1474836E9f) - 0.5f);
            effect_request_template().py = w.py;
            effect_request_template().pz = w.pz + ((-effect_random() / -2.1474836E9f) - 0.5f);
            
            effect_request_template().ay = 0;
            
            effect_request(effect_request_template(),NULL,NULL,0);
            return;
        }
        
        break;
    case 2:
        break;
    }
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff029(EffectSprite& w)
{
	w.ct0 = (w.ct0 + 1) & 0x1F;

    if (w.ct0 == 0)
	{
        ((int*)w.lkwkp)[1] |= 0x40; // TODO: find the actual struct getting parsed to here

    	w.flg = 0;
    }
}
void bhEff101(EffectSprite& w)
{
    w.flg |= 0x1000000;
    
    if ((w.stflg & 0x1000000)) 
    {
        w.flg = 0;
        return;
    }
    
    effect_request_template().id = 107;
    
    effect_request_template().flg  = 1;
    effect_request_template().type = 0;
    
    effect_request_template().px = 0;
    effect_request_template().py = 0;
    effect_request_template().pz = 0;
    
    effect_request_template().sx = w.sx;
    effect_request_template().sy = w.sy;
    effect_request_template().sz = w.sz;
    
    effect_request_template().ay = 0;
    
    effect_request(effect_request_template(),NULL,NULL,0);
}
void bhEff103(EffectSprite& w)
{
    w.flg |= 0x1000000;
    
    switch (w.type) 
    {                            
    case 0:
        if ((-effect_random() / -2.1474836E9f) < 0.3f) 
        {
            effect_request_template().id = 10;
            
            effect_request_template().flg = 0x100001;
            
            effect_request_template().type = 1;

            effect_request_template().px = w.px + ((3.0f * (-effect_random() / -2.1474836E9f)) - 1.5f);
            effect_request_template().py = w.py + ((3.0f * (-effect_random() / -2.1474836E9f)) - 1.5f);
            effect_request_template().pz = w.pz + ((3.0f * (-effect_random() / -2.1474836E9f)) - 1.5f);
            
            effect_request_template().sx = w.sx;
            effect_request_template().sy = w.sy;
            effect_request_template().sz = w.sz;
            
            effect_request_template().ay = 0;
            
            effect_request(effect_request_template(),NULL,NULL,0);
        }
        
        break;
    case 1:
        w.ct0 = (w.ct0 + 1) & 3;
        
        if (w.ct0 == 0) 
        {
            effect_request_template().id = 119;
            
            effect_request_template().flg = 0x4100001;
            
            effect_request_template().type = 0;
            
            effect_request_template().px = w.px;
            effect_request_template().py = w.py;
            effect_request_template().pz = w.pz;
            
            effect_request_template().sx = w.sx;
            effect_request_template().sy = w.sy;
            effect_request_template().sz = w.sz;
            
            effect_request_template().ay = w.ay + (int)(2048.0f * njSin(w.ct1));
            
            w.ct1 += 2048;
            
            effect_request(effect_request_template(),NULL,NULL,0);
        }
        
        break;
    case 2:
        w.ct0 = (w.ct0 + 1) & 1;
        
        if (w.ct0 == 0) 
        {
            effect_request_template().id = 119;
            
            effect_request_template().flg = 0x4100001;
            
            effect_request_template().type = 1;
            
            effect_request_template().px = w.px;
            effect_request_template().py = w.py;
            effect_request_template().pz = w.pz;
            
            effect_request_template().sx = w.sx;
            effect_request_template().sy = w.sy;
            effect_request_template().sz = w.sz;
            
            effect_request_template().ay = 0;
            
            effect_request(effect_request_template(),NULL,NULL,0);
        }
        
        break;
    }
}
void bhEff104(EffectSprite& w)
{
    int i, j;
    
    w.flg |= 0x1000000;
    
    for (i = 0, j = w.type; i < j; i++) 
    {
        effect_request_template().id = 12;
        
        effect_request_template().flg = 0x100001;
        
        effect_request_template().mdlver = 0;
        
        if (w.flr_no == -1) 
        {
            effect_request_template().type = 1;
        }
        else 
        {
            effect_request_template().type = 0;
        }
        
        effect_request_template().px = w.px + ((2.0f * (-effect_random() / -2.1474836E9f)) - 1.0f);
        effect_request_template().py = w.py + ((2.0f * (-effect_random() / -2.1474836E9f)) - 1.0f);
        effect_request_template().pz = w.pz + ((2.0f * (-effect_random() / -2.1474836E9f)) - 1.0f);
        
        effect_request_template().sx = 0.1f;
        effect_request_template().sy = 0.1f;
        effect_request_template().sz = 0.1f;
        
        effect_request_template().ay = w.ay + ((int)(182.04445f * ((90.0f * (-effect_random() / -2.1474836E9f)) - 45.0f)) & 0xFFFF);
        
        effect_request(effect_request_template(),NULL,NULL,0);
    } 
    
    w.ct0++;
    
    if (w.ct0 > w.mdlver) 
    {
        w.flg = 0;
    }
}
void bhEff116(EffectSprite& w) 
{
    w.flg |= 0x1000000;
    
    w.ct3 = (w.ct3 + 1) & 0x3;
    
    if (w.ct3 != 0) 
    {
        return;
    }
    
    switch (w.type) 
    {
    case 0:
        effect_request_template().id = 15;
        
        effect_request_template().flg = 0x4100001;
        
        effect_request_template().mdlver = w.ct3 & 0x7;
        
        effect_request_template().type = 5;
        
        effect_request_template().sx = w.sx;
        effect_request_template().sy = w.sy;
        effect_request_template().sz = w.sz;
        
        effect_request_template().ay = w.ay;
        
        effect_request_template().px = w.px;
        effect_request_template().py = w.py;
        effect_request_template().pz = w.pz;
        
        effect_request(effect_request_template(),NULL,NULL,0);
		break;
    }
}
void bhEff117(EffectSprite& w)
{
    EffectUv* uvp;
    static EffectUv uvinfo_p0[4] =
    {
        { 0.0f,    0.0f, 0.1875f, 0.1875f },
        { 0.1875f, 0.0f, 0.1875f, 0.1875f },
        { 0.375f,  0.0f, 0.1875f, 0.1875f },
        { -1.0f,   0.0f, 0.0f,    0.0f    }
    };
    static EffectUv uvinfo_p1[4] = 
    {
        { 0.5625f, 0.0f,    0.1875f, 0.1875f },
        { 0.75f,   0.0f,    0.1875f, 0.1875f },
        { 0.0f,    0.1875f, 0.1875f, 0.1875f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
    
    switch (w.mode0)
    {                            
    case 0:
        w.tex_id = 38;
        
        w.flg |= 0x4100000;
        
        w.tv[0].col = -1;
        w.tv[1].col = -1;
        w.tv[2].col = -1;
        w.tv[3].col = -1;
        
        w.bl_src = 8;
        w.bl_dst = 6;
        
        w.ani_ct = 0;
        
        w.ct0 = 0;
        w.ct1 = (int)(4.0f * (-effect_random() / -2.1474836E9f)) + 1;
        
        if (w.type == 0) 
        {
            uvp = uvinfo_p0;
            
            w.exp0 = (unsigned char*)uvp;
        } 
        else 
        {
            uvp = uvinfo_p1;
            
            w.exp0 = (unsigned char*)uvp;
        }
        
        w.mode0 = 1;
        break;
    case 2:
        w.flg = 0;
        return;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.ct0 = 0;
        
        uvp = (EffectUv*)w.exp0 + w.ct0;
    }
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff119(EffectSprite& w) 
{
    EffectUv* uvp;
	static EffectUv uvinfo_p0[16] =
    {
        { 0.0f,     0.0f,     0.0625f,  0.0625f  },
        { 0.0f,     0.0625f,  0.0625f,  0.0625f  },
        { 0.0625f,  0.0f,     0.09375f, 0.09375f },
        { 0.15625f, 0.0f,     0.125f,   0.125f   },
        { 0.28125f, 0.0f,     0.125f,   0.125f   },
        { 0.40625f, 0.0f,     0.1875f,  0.1875f  },
        { 0.59375f, 0.0f,     0.1875f,  0.1875f  },
        { 0.78125f, 0.0f,     0.1875f,  0.1875f  },
        { 0.0f,     0.125f,   0.21875f, 0.21875f },
        { 0.0f,     0.34375f, 0.25f,    0.25f    },
        { 0.25f,    0.34375f, 0.25f,    0.25f    },
        { 0.5f,     0.34375f, 0.25f,    0.25f    },
        { 0.75f,    0.34375f, 0.25f,    0.25f    },
        { 0.0f,     0.59375f, 0.25f,    0.25f    },
        { 0.25f,    0.59375f, 0.25f,    0.25f    },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };
    
    switch (w.mode0)
    {
    case 0:
        w.tex_id = 39;
        
        w.flg |= 0x4100000;
        
        w.tv[0].col = -1;
        w.tv[1].col = -1;
        w.tv[2].col = -1;
        w.tv[3].col = -1;
        
        w.bl_src = 11;
        w.bl_dst = 3;
        
        w.gpy = w.py;
        
        w.ani_ct = 0;
        w.ct0    = 0;
        
        w.xn = (0.4f * (-effect_random() / -2.1474836E9f)) - 0.2f;
        w.zn = (0.4f * (-effect_random() / -2.1474836E9f)) - 0.2f;
        
        switch (w.type)
        {                        
        case 0:                                     
            w.tv[0].col = 0xC0FFFFFF;
            w.tv[1].col = 0xC0FFFFFF;
            w.tv[2].col = 0xC0FFFFFF;
            w.tv[3].col = 0xC0FFFFFF;
            
            w.ct2 = 192;
            
            w.exp0 = (unsigned char*)uvinfo_p0;
            
            w.spd = 0.1f * w.sz;
            break;
        case 1:                                     
            w.exp0 = (unsigned char*)&uvinfo_p0[5];
            break;
        }
        
        w.mode0 = 1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f) 
    {
        w.flg = 0;
        return;
    }
    
    switch (w.type) 
    {                            
    case 0:                                         
        w.tv[0].col = (w.ct2 << 24) | 0xFFFFFF;
        w.tv[1].col = (w.ct2 << 24) | 0xFFFFFF;
        w.tv[2].col = (w.ct2 << 24) | 0xFFFFFF;
        w.tv[3].col = (w.ct2 << 24) | 0xFFFFFF;
        
        w.spd *= 0.9f;
        
        w.px -= w.spd * njSin(w.ay);
        w.pz -= w.spd * njCos(w.ay);
        w.py += 0.5f;
        
        w.ct2 -= 8;
        break;
    case 1:                                         
        w.px += w.xn;
        w.pz += w.zn;
        w.py = w.gpy + (w.sz * njSin(w.ct0 * 1024));
        
        w.sx = 8.0f * (w.sxb * uvp->xs);
        w.sy = 8.0f * (w.syb * uvp->ys);
        break;
    }
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff121(EffectSprite& w) 
{
    EffectUv* uvp;
	static EffectUv uvinfo_p0[9] =
    {
        { 0.0f,  0.0f, 0.25f, 0.5f },
        { 0.0f,  0.5f, 0.25f, 0.5f },
        { 0.25f, 0.0f, 0.25f, 0.5f },
        { 0.25f, 0.5f, 0.25f, 0.5f },
        { 0.5f,  0.0f, 0.25f, 0.5f },
        { 0.5f,  0.5f, 0.25f, 0.5f },
        { 0.75f, 0.0f, 0.25f, 0.5f },
        { 0.75f, 0.5f, 0.25f, 0.5f },
        { -1.0f, 0.0f, 0.0f,  0.0f }
    };
    
    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 40;
        
        w.flg |= 0x4000000;
        
        w.tv[0].x = -1.0f;
        w.tv[0].y = 0;
        w.tv[0].z = 0;
        
        w.tv[1].x = 1.0f;
        w.tv[1].y = 0;
        w.tv[1].z = 0;
        
        w.tv[2].x = -1.0f;
        w.tv[2].y = -2.0f;
        w.tv[2].z = 0;
        
        w.tv[3].x = 1.0f;
        w.tv[3].y = -2.0f;
        w.tv[3].z = 0;
        
        w.tv[0].col = -1;
        w.tv[1].col = -1;
        w.tv[2].col = -1;
        w.tv[3].col = -1;
        
        w.bl_src = 8;
        w.bl_dst = 6;
        
        w.ani_ct = 0;
        
        w.ct0 = 0;
        
        w.exp0 = (unsigned char*)uvinfo_p0;
        
        w.mode0 = 1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f)
    {
        w.ct0 = 0;
        
        uvp = (EffectUv*)w.exp0 + w.ct0;
    }
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff122(EffectSprite& w)
{
    EffectUv* uvp;
	static EffectUv uvinfo_p0[3] = 
    {
        { 0.0f,   0.0f, 0.125f, 0.125f },
        { 0.125f, 0.0f, 0.125f, 0.125f },
        { 0.25f,  0.0f, 0.125f, 0.125f }
    };
    
    switch (w.mode0) 
    {
    case 0:
        w.tex_id = 41;
        
        w.flg |= 0x4000000;
        
        w.bl_src = 8;
        w.bl_dst = 6;
        
        w.tv[0].col = w.tv[1].col = w.tv[2].col = w.tv[3].col = -1;
        
        w.ct0 = 0;
        
        w.exp0 = (unsigned char*)uvinfo_p0;
        
        w.mode0 = 1;
        break;
    }
    
    w.ani_ct = w.type;
    
    uvp = (EffectUv*)w.exp0 + w.mode1;
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    effect_queue_translucent(w);
}
void bhEff123(EffectSprite& w) 
{
    EffectUv* uvp;
	static EffectUv uvinfo_p0[4] = 
    {
        { 0.0f,    0.875f, 0.0625f, 0.0625f },
        { 0.0625f, 0.875f, 0.0625f, 0.0625f },
        { 0.125f,  0.875f, 0.0625f, 0.0625f },
        { 0.1875f, 0.875f, 0.0625f, 0.0625f }
    };
        
    switch (w.mode0) 
    {                             
    case 0:                                         
        w.tex_id = 2;
        
        w.flg |= 0x4100000;
        
        w.bl_src = 8;
        w.bl_dst = 10;
        
        w.ani_ct = 0;
        
        w.ct0 = 0;
        
        w.exp0 = (unsigned char*)uvinfo_p0;
        
        w.ct2 = 0;
        
        w.mode0 = 1;
        break;
    case 1:                                         
        w.ct1++;
        
        if ((w.ct1 > w.ct2) || (w.mode1 != 0)) 
        {
            w.ct1 = 0;
            w.ct2 = 20.0f + (10.0f * (-effect_random() / -2.1474836E9f));
            
            w.mode1 = 0;
            w.mode0 = 2;
        }
        
        return;
    }
    
    switch (w.type) 
    {                          
    case 1:
        w.tv[0].col = 0xFFFF0000;
        w.tv[1].col = 0xFFFF0000;
        w.tv[2].col = 0xFFFF0000;
        w.tv[3].col = 0xFFFF0000;
        break;
    case 2:                                     
        w.tv[0].col = 0xFF00FF00;
        w.tv[1].col = 0xFF00FF00;
        w.tv[2].col = 0xFF00FF00;
        w.tv[3].col = 0xFF00FF00;
        break;
    case 3:                                     
        w.tv[0].col = 0xFF0000FF;
        w.tv[1].col = 0xFF0000FF;
        w.tv[2].col = 0xFF0000FF;
        w.tv[3].col = 0xFF0000FF;
        break;
    case 4:                                     
        w.tv[0].col = 0xFF00FFFF;
        w.tv[1].col = 0xFF00FFFF;
        w.tv[2].col = 0xFF00FFFF;
        w.tv[3].col = 0xFF00FFFF;
        break;
    case 5:                                     
        w.tv[0].col = ~0xFF;
        w.tv[1].col = ~0xFF;
        w.tv[2].col = ~0xFF;
        w.tv[3].col = ~0xFF;
        break;
    default: 
        w.tv[0].col = -1;
        w.tv[1].col = -1;
        w.tv[2].col = -1;
        w.tv[3].col = -1;
        break;
    }
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    w.az = w.ct0 * 4096;
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    w.ct0 = (w.ct0 + 1) & 3;
    
    if (w.ct0 == 0) 
    {
        w.mode0 = 1;
    }
    
    effect_queue_translucent(w);
}
void bhEff128(EffectSprite& w) 
{
	unsigned int argb;
    float x, y;
    
    switch (w.mode0) 
    {
    case 0:
        switch (w.type) 
        {
        case 0:
            w.tex_id = 2;
            
            w.ani_ct = 1;
            
            w.tv[0].u = 0.1875f;
            w.tv[0].v = 0.3125f;
            
            w.tv[1].u = 0.375f;
            w.tv[1].v = 0.3125f;
            
            w.tv[2].u = 0.1875f;
            w.tv[2].v = 0.5f;
            
            w.tv[3].u = 0.375f;
            w.tv[3].v = 0.5f; 
            break;
        default:
            w.tex_id = 43;
            
            x = 0.5f * (w.flr_no % 2);
            y = 0.5f * (w.flr_no / 2);
            
            w.tv[0].u = x;
            w.tv[0].v = y;
            
            w.tv[1].u = 0.5f + x;
            w.tv[1].v = y;
            
            w.tv[2].u = x;
            w.tv[2].v = 0.5f + y;
            
            w.tv[3].u = 0.5f + x;
            w.tv[3].v = 0.5f + y;
            break;
        }
        
        w.tv[0].x = -1.0f;
        w.tv[0].y = 0;
        w.tv[0].z = -1.0f;
        
        w.tv[1].x = 1.0f;
        w.tv[1].y = 0;
        w.tv[1].z = -1.0f;
        
        w.tv[2].x = -1.0f;
        w.tv[2].y = 0;
        w.tv[2].z = 1.0f;
        
        w.tv[3].x = 1.0f;
        w.tv[3].y = 0;
        w.tv[3].z = 1.0f;
        
        w.xn = 0.1f * w.sx;
        
        w.bl_src = 8;
        w.bl_dst = 6;
        
        w.ct1 = 192;
        
        w.mode0 = 1;
		break;
    }
    
    if (w.type == 0) 
    {
        w.ct1 -= 4;
        
        if (w.ct1 <= 0)
        {
            w.flg = 0;
            return;
        }
        
        w.sx += w.xn;
        w.sz += w.xn;
    }
    else
    {
        w.ay += (int)(182.04445f * w.sy) & 0xFFFF;
        
        w.ct1 = 160.0f * njSin(w.ct0);
        w.ct0 = (w.ct0 + 512) & 0x7FFF;
        
        if (w.ct0 == 0)
        {
            w.flg = 0;
            return;
        }
    }
    
    switch (w.mdlver) 
    {                             
    case 0:
        argb = (w.ct1 << 24) | 0xC8B440;
        break;
    case 1:
        argb = (w.ct1 << 24) | 0xFFFFFF;
        break;
    }
    
    w.tv[0].col = argb;
    w.tv[1].col = argb;
    w.tv[2].col = argb;
    w.tv[3].col = argb;
    
    effect_queue_translucent(w);
}
void bhEff131(EffectSprite& w) 
{
    EffectUv* uvp;
	static EffectUv uvinfo_p0[14] = 
    {
        { 0.0625f,  0.0f,     0.0625f,  0.0625f  },
        { 0.0625f,  0.0625f,  0.0625f,  0.0625f  },
        { 0.25f,    0.0f,     0.125f,   0.125f   },
        { 0.5f,     0.0f,     0.125f,   0.125f   },
        { 0.75f,    0.0f,     0.125f,   0.125f   },
        { 0.0f,     0.125f,   0.1875f,  0.1875f  },
        { 0.375f,   0.125f,   0.1875f,  0.1875f  },
        { 0.1875f,  0.3125f,  0.1875f,  0.1875f  },
        { 0.0f,     0.5f,     0.1875f,  0.1875f  },
        { 0.375f,   0.5f,     0.1875f,  0.1875f  },
        { 0.5625f,  0.125f,   0.21875f, 0.21875f },
        { 0.78125f, 0.125f,   0.21875f, 0.21875f },
        { 0.5625f,  0.34375f, 0.21875f, 0.21875f },
        { -1.0f,    0.0f,     0.0f,     0.0f     }
    };

    switch (w.mode0) 
    {
    case 0:
        w.flg |= 0x4100000;
        
        w.tex_id = 2;
        
        w.tv[0].col = 0x8010C040;
        w.tv[1].col = 0x8010C040;
        w.tv[2].col = 0x8010C040;
        w.tv[3].col = 0x8010C040;
        
        w.bl_src = 8;
        w.bl_dst = 10;
        
        w.exp0 = (unsigned char*)uvinfo_p0;
        
        w.mode0 = 1;
        break;
    }
    
    w.px += w.lox;
    w.py += w.loy;
    w.pz += w.loz;
    
    w.lox *= 0.9f;
    w.loy *= 0.9f;
    w.loz *= 0.9f;
    
    uvp = (EffectUv*)w.exp0 + w.ct0;
    
    if (uvp->u == -1.0f)
    {
        w.flg = 0;
        return;
    }
    
    w.sx = 4.0f * (w.sxb * uvp->xs);
    w.sy = 4.0f * (w.syb * uvp->ys);
    
    w.tv[0].u = uvp->u;
    w.tv[0].v = uvp->v;
    
    w.tv[1].u = uvp->u + uvp->xs;
    w.tv[1].v = uvp->v;
    
    w.tv[2].u = uvp->u;
    w.tv[2].v = uvp->v + uvp->ys;
    
    w.tv[3].u = uvp->u + uvp->xs;
    w.tv[3].v = uvp->v + uvp->ys;
    
    w.ct0++;
    
    effect_queue_translucent(w);
}
void bhEff132(EffectSprite& w)
{
    int i;
	EffectUv* uvp;
	static EffectUv uvinfo_p0[8] = 
    {
        { 0.1875f, 0.125f,  0.1875f, 0.1875f },
        { 0.875f,  0.0f,    0.125f,  0.125f  },
        { 0.625f,  0.0f,    0.125f,  0.125f  },
        { 0.375f,  0.0f,    0.125f,  0.125f  },
        { 0.125f,  0.0f,    0.125f,  0.125f  },
        { 0.0625f, 0.0625f, 0.0625f, 0.0625f },
        { 0.0f,    0.0625f, 0.0625f, 0.0625f },
        { -1.0f,   0.0f,    0.0f,    0.0f    }
    };
    static EffectUv uvinfo_p1[9] = 
    {
        { 0.0f,   0.875f, 0.125f, 0.125f },
        { 0.125f, 0.875f, 0.125f, 0.125f },
        { 0.25f,  0.875f, 0.125f, 0.125f },
        { 0.375f, 0.875f, 0.125f, 0.125f },
        { 0.5f,   0.875f, 0.125f, 0.125f },
        { 0.625f, 0.875f, 0.125f, 0.125f },
        { 0.75f,  0.875f, 0.125f, 0.125f },
        { 0.875f, 0.875f, 0.125f, 0.125f },
        { -1.0f,  0.0f,   0.0f,   0.0f   }
    };
    static EffectUv uvinfo_p2[12] = 
    {
        { 0.0625f,  0.0625f, 0.0625f,  0.0625f  },
        { 0.25f,    0.0f,    0.125f,   0.125f   },
        { 0.5f,     0.0f,    0.125f,   0.125f   },
        { 0.75f,    0.0f,    0.125f,   0.125f   },
        { 0.0f,     0.125f,  0.1875f,  0.1875f  },
        { 0.375f,   0.125f,  0.1875f,  0.1875f  },
        { 0.1875f,  0.3125f, 0.1875f,  0.1875f  },
        { 0.0f,     0.5f,    0.1875f,  0.1875f  },
        { 0.375f,   0.5f,    0.1875f,  0.1875f  },
        { 0.5625f,  0.125f,  0.21875f, 0.21875f },
        { 0.78125f, 0.125f,  0.21875f, 0.21875f },
        { -1.0f,    0.0f,    0.0f,     0.0f     }
    };
    static EffectUv uvinfo_p3[9] = 
    {
        { 0.59375f, 0.875f, 0.09375f, 0.09375f },
        { 0.59375f, 0.875f, 0.09375f, 0.09375f },
        { 0.59375f, 0.875f, 0.09375f, 0.09375f },
        { 0.5f,     0.875f, 0.09375f, 0.09375f },
        { 0.40625f, 0.875f, 0.09375f, 0.09375f },
        { 0.3125f,  0.875f, 0.09375f, 0.09375f },
        { 0.21875f, 0.875f, 0.09375f, 0.09375f },
        { 0.125f,   0.875f, 0.09375f, 0.09375f },
        { -1.0f,    0.0f,   0.0f,     0.0f     }
    };
    static short apat_p3[9] = 
    {
        208, 208, 208, 208, 192, 176, 160, 144, 0
    };
    
    switch (w.mode0) 
    {
    case 0:
        w.flg |= 0x4100000;
        
        switch (w.type) 
        {                          
        case 0:                                     
            w.tex_id = 2;
            
            w.ani_ct = 0;
            
            w.bl_src = 8;
            w.bl_dst = 10;
            
            w.exp0 = (unsigned char*)uvinfo_p0;
            
            w.tv[0].col = 0xFF20E0FF;
            w.tv[1].col = 0xFF20E0FF;
            w.tv[2].col = 0xFF20E0FF;
            w.tv[3].col = 0xFF20E0FF;
            break;
        case 1:                                     
            w.tex_id = 10;
            
            w.ani_ct = 2;
            
            w.bl_src = 8;
            w.bl_dst = 10;
            
            w.exp0 = (unsigned char*)uvinfo_p1;
            
            w.tv[0].u = 0.875f;
            w.tv[0].v = 0.25f;
            
            w.tv[1].u = 1.0f;
            w.tv[1].v = 0.25f;
            
            w.tv[2].u = 0.875f;
            w.tv[2].v = 0.375f;
            
            w.tv[3].u = 1.0f;
            w.tv[3].v = 0.375f;
            
            w.tv[0].col = -1;
            w.tv[1].col = -1;
            w.tv[2].col = -1;
            w.tv[3].col = -1;
            break;
        case 2:                                     
            w.tex_id = 2;
            
            w.ani_ct = 1;
            
            w.bl_src = 9;
            w.bl_dst = 10;
            
            w.exp0 = (unsigned char*)uvinfo_p2;
            
            w.ct3 = 192;
            break;
        case 3:                                     
            w.tex_id = 19;
            
            w.ani_ct = 0;
            
            w.bl_src = 9;
            w.bl_dst = 10;
            
            w.gpx = w.px;
            w.gpy = w.py;
            w.gpz = w.pz;
            
            w.xn = w.lox;
            w.yn = w.loy;
            w.zn = w.loz;
            
            w.spd = 0.9f;
            
            w.exp0 = (unsigned char*)uvinfo_p3;
            break;
        }
        
        w.mode0 = 1;
        break;
    }
    
    switch (w.type) 
    {                            
    case 0:                                        
        uvp = (EffectUv*)w.exp0 + w.ct0;
        
        if (uvp->u == -1.0f)
        {
            w.flg = 0;
            return;
        }
        
        w.sx = 4.0f * (w.sxb * uvp->xs);
        w.sy = 4.0f * (w.syb * uvp->ys);
        
        w.tv[0].u = uvp->u;
        w.tv[0].v = uvp->v;
        
        w.tv[1].u = uvp->u + uvp->xs;
        w.tv[1].v = uvp->v;
        
        w.tv[2].u = uvp->u;
        w.tv[2].v = uvp->v + uvp->ys;
        
        w.tv[3].u = uvp->u + uvp->xs;
        w.tv[3].v = uvp->v + uvp->ys;
        
        w.ct0++;
        break;
    case 1:                                        
        w.px += w.lox;
        w.py += w.loy;
        w.pz += w.loz;
        
        w.lox *= 0.8f;
        w.loy *= 0.8f;
        w.loz *= 0.8f;
        
        w.sx *= 0.8f;
        w.sy *= 0.8f;
        
        if (w.sx < 0.1f) 
        {
            w.flg = 0;
            return;
        }
        
        break;
    case 2:                                        
        i = (w.ct3 << 24) | 0xFFFFFF;
        
        w.tv[0].col = i;
        w.tv[1].col = i;
        w.tv[2].col = i;
        w.tv[3].col = i;
        
        w.ct3 -= 14;
        
        w.px += w.xn;
        w.py += w.yn;
        w.pz += w.zn;
        
        w.xn += w.lox;
        w.yn += w.loy;
        w.zn += w.loz;
        
        uvp = (EffectUv*)w.exp0 + w.ct0;
        
        if (uvp->u == -1.0f) 
        {
            w.flg = 0;
            return;
        }
        
        w.sx = 4.0f * (w.sxb * uvp->xs);
        w.sy = 4.0f * (w.syb * uvp->ys);
        
        w.tv[0].u = uvp->u;
        w.tv[0].v = uvp->v;
        
        w.tv[1].u = uvp->u + uvp->xs;
        w.tv[1].v = uvp->v;
        
        w.tv[2].u = uvp->u;
        w.tv[2].v = uvp->v + uvp->ys;
        
        w.tv[3].u = uvp->u + uvp->xs;
        w.tv[3].v = uvp->v + uvp->ys;
        
        w.ct0++;
        break;
    case 3:                                        
        if (((w.ct0 > 0) && (w.ct0 < 3)) && (w.mdlver == 0)) 
        {
            effect_request_template().id   = 132;
            effect_request_template().type = 3;
            
            effect_request_template().flr_no = 0;
            
            effect_request_template().mdlver = 1;
            
            effect_request_template().ax = 0;
            effect_request_template().ay = 0;
            
            effect_request_template().flg = 0x4100001;
            
            effect_request_template().sx = w.sxb * w.spd;
            effect_request_template().sy = w.syb * w.spd;
            effect_request_template().sz = 1.0f;
            
            effect_request_template().px = w.gpx;
            effect_request_template().py = w.gpy;
            effect_request_template().pz = w.gpz;
            
            effect_request(effect_request_template(),(NJS_POINT3*)&w.xn,NULL,0);
            
            w.spd -= 0.1f;
        }
        
        i = (apat_p3[w.ct0] << 24) | 0xFFFFFF;
        
        w.tv[0].col = i;
        w.tv[1].col = i;
        w.tv[2].col = i;
        w.tv[3].col = i;
        
        w.px += w.lox;
        w.py += w.loy;
        w.pz += w.loz;
        
        w.lox *= 0.6f;
        w.loy *= 0.6f;
        w.loz *= 0.6f;
        
        uvp = (EffectUv*)w.exp0 + w.ct0;
        
        if (uvp->u == -1.0f)
        {
            w.flg = 0;
            return;
        }
        
        w.sx = 4.0f * (w.sxb * uvp->xs);
        w.sy = 4.0f * (w.syb * uvp->ys);
        
        w.sxb *= 1.1f;
        w.syb *= 1.1f;
        
        w.tv[0].u = uvp->u;
        w.tv[0].v = uvp->v;
        
        w.tv[1].u = uvp->u + uvp->xs;
        w.tv[1].v = uvp->v;
        
        w.tv[2].u = uvp->u;
        w.tv[2].v = uvp->v + uvp->ys;
        
        w.tv[3].u = uvp->u + uvp->xs;
        w.tv[3].v = uvp->v + uvp->ys;
        
        w.ct0++;
        break;
    }

    effect_queue_translucent(w);
}
void bhEff206(EffectSprite& w) 
{
    EffectRequest* pEffect;
    
    if ((w.type == 0) && (w.mode1 != 0))
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    switch (w.mode0) 
    {                          
    case 0:
        w.ct0   = 0;
        w.mode0 = 1;
        break;
    case 1:
        w.ct0++;
        
        if (w.ct0 >= 6) 
        {
            pEffect = &effect_request_template();
            
            pEffect->flg = 1;
            
            pEffect->id = 207;
            
            pEffect->type = w.type - 1;
            
            pEffect->flr_no = 0;
            
            pEffect->mdlver = 0;
            
            pEffect->px = w.px;
            pEffect->py = w.py;
            pEffect->pz = w.pz;
            
            pEffect->sx = w.sx;
            pEffect->sy = w.sy;
            pEffect->sz = w.sz;
            
            pEffect->ay = w.ay;
            pEffect->ax = w.ax;
            
            effect_request(*pEffect,NULL,NULL,0xFF);
            
            w.ct0 = 0;
        }
        
        break;
    }
}
void bhEff208(EffectSprite& w) 
{
    EffectRequest* pEffect;
    
    if ((w.type == 0) && (w.mode1 != 0))
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    switch (w.mode0) 
    {                          
    case 0:
        w.ct0   = 0;
        w.mode0 = 1;
        break;
    case 1:
        w.ct0++;
        
        if (w.ct0 >= 6) 
        {
            pEffect = &effect_request_template();
            
            pEffect->flg = 1;
            
            pEffect->id = 209;
            
            pEffect->type = 0;
            
            pEffect->flr_no = 0;
            
            pEffect->mdlver = 0;
            
            pEffect->px = w.px;
            pEffect->py = w.py;
            pEffect->pz = w.pz;
            
            pEffect->sx = w.sx;
            pEffect->sy = w.sy;
            pEffect->sz = w.sz;
            
            pEffect->ay = w.ay;
            pEffect->ax = w.ax;
            
            effect_request(*pEffect,NULL,NULL,0xFF);
            
            w.ct0 = 0;
        }
        
        break;
    }
}
void bhEff214(EffectSprite& w) 
{
    EffectRequest* pEffect;
    
    if ((w.type == 0) && (w.mode1 != 0)) 
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    pEffect = &effect_request_template();
    
    pEffect->flg = 1;
    
    pEffect->id = 11;
    
    pEffect->type = 0;
    
    pEffect->flr_no = 0;
    
    pEffect->mdlver = 0;
    
    pEffect->px = w.px;
    pEffect->py = w.py;
    pEffect->pz = w.pz;
    
    pEffect->sx = w.sx;
    pEffect->sy = w.sy;
    pEffect->sz = w.sz;
    
    pEffect->ay = w.ay;
    pEffect->ax = w.ax;
    
    effect_request(*pEffect,NULL,NULL,0xFF);
    
    w.type = w.mode1 = 0;
}
void bhEff215(EffectSprite& w)
{
    EffectRequest* pEffect;
    
    if ((w.type == 0) && (w.mode1 != 0)) 
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    pEffect = &effect_request_template();
    
    w.type = (w.type - 1) % 9;
    
    if (w.type < 6) 
    {
        pEffect->id   = 216;
        pEffect->type = w.type;
    } 
    else 
    {
        pEffect->id   = 217;
        pEffect->type = w.type - 6;
    }
    
    pEffect->flg = 1;
    
    pEffect->flr_no = 0;
    
    pEffect->mdlver = 0;
    
    pEffect->px = w.px;
    pEffect->py = w.py;
    pEffect->pz = w.pz;
    
    pEffect->sx = w.sx;
    pEffect->sy = w.sy;
    pEffect->sz = w.sz;
    
    pEffect->ay = w.ay;
    pEffect->ax = w.ax;
    
    effect_request(*pEffect,NULL,NULL,0xFF);
    
    w.type = w.mode1 = 0;
}
void bhEff222(EffectSprite& w) 
{
    EffectRequest* pEffect;
    
    if ((w.type == 0) && (w.mode1 != 0)) 
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    if (w.sz <= 0) 
    {
        w.sz = 0.1f;
    }
    
    switch (w.mode0) 
    {                            
    case 0:
        w.ct0   = 0;
        w.mode0 = 1;
    case 1:
        w.ct0++;
        
        if (w.lkono <= w.ct0) 
        {
            pEffect = &effect_request_template();
            
            pEffect->flg = 1;
            
            pEffect->id = 223;
            
            pEffect->type = (w.type - 1) % 4;
            
            pEffect->flr_no = 0;
            
            pEffect->mdlver = 0;
            
            pEffect->px = w.px;
            pEffect->py = (w.py + (6.0f * (-effect_random() / -2.1474836E9f))) - 3.0f;
            pEffect->pz = (w.pz + (6.0f * (-effect_random() / -2.1474836E9f))) - 3.0f;
            
            pEffect->sx = w.sx;
            pEffect->sy = w.sy;
            pEffect->sz = w.sz / 10.0f;
            
            pEffect->ay = w.ay;
            pEffect->ax = w.ax;
            
            effect_request(*pEffect,NULL,NULL,0);
            
            w.ct0 = 0;
        }
        
        break;
    }
}
void bhEff224(EffectSprite& w)
{
    EffectRequest* pEffect;
    
    if ((w.type == 0) && (w.mode1 != 0)) 
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    pEffect = &effect_request_template();
    
    pEffect->flg = 1;
    
    pEffect->id = 225;
    
    pEffect->type = (w.type - 1) % 4; 
    
    pEffect->mdlver = 0;

    pEffect->flr_no = 0;
    
    pEffect->px = w.px;
    pEffect->py = w.py;
    pEffect->pz = w.pz;
    
    pEffect->sx = w.sx;
    pEffect->sy = w.sy;
    pEffect->sz = w.sz;
    
    pEffect->ay = w.ay;
    pEffect->ax = w.ax;
    
    effect_request(*pEffect,NULL,NULL,0);
    
    w.type = w.mode1 = 0;
}
void bhEff231(EffectSprite& w) 
{
    EffectRequest* pEffect;
    
    if ((w.type == 0) && (w.mode1 != 0)) 
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    pEffect = &effect_request_template();
    
    pEffect->flg = 1;
    
    pEffect->id = 232;
    
    pEffect->type = (w.type - 1) % 4; 
    
    pEffect->flr_no = 0;

    pEffect->mdlver = 0;
    
    pEffect->px = w.px;
    pEffect->py = w.py;
    pEffect->pz = w.pz;
    
    pEffect->sx = w.sx;
    pEffect->sy = w.sy;
    pEffect->sz = 1.0f;
    
    pEffect->ay = w.ay;
    pEffect->ax = w.ax;
    
    if (!w.sz) 
    {
        effect_request(*pEffect,NULL,NULL,0);
        
        w.mode1 = w.type = 0;
        return; 
    }
    
    if (w.ct1 <= 0) 
    {
        effect_request(*pEffect,NULL,NULL,0);
        
        w.ct1 = w.sz;
    }
    
    w.ct1--;
}
void bhEff241(EffectSprite& w) 
{
    EffectRequest* pEffect;
    int lLoop;

    if ((w.type == 0) && (w.mode1 != 0)) 
    {
        w.type = w.mode1;
    }
    
    if (w.type == 0) 
    {
        w.flg |= 0x1000000;
        return;
    }
    
    w.flg &= ~0x1000000;
    
    for (lLoop = 0; lLoop < w.lkono; lLoop++)
    {
        pEffect = &effect_request_template();
    
        pEffect->flg = 1;
        
        pEffect->id = 242;

        pEffect->type = w.type - 1; 
    
        pEffect->flr_no = 0;

        pEffect->mdlver = 0;
    
        pEffect->ax = w.ax;
        pEffect->ay = w.ay;
        
        pEffect->px = w.px;
        pEffect->py = w.py;
        pEffect->pz = w.pz;
        
        pEffect->sx = w.sx;
        pEffect->sy = w.sy;
        pEffect->sz = w.sz;
        
        effect_request(*pEffect,NULL,NULL,0);
    }
    
    w.mode1 = 0;
    
    w.type = w.mode0 = 0;
}
void bhEff144(EffectSprite& w)
{
    switch (w.mode0) 
    {
    case 0:
        w.ct0 = 0;
        w.ct1 = 0;
        
        w.ani_ct = w.mdlver;
        
        w.mode0 = 1;
        break;
    case 1:
        w.ct1++;
        
        if (w.ct1 < 6) 
        {
            effect_queue_translucent(w);
            return;
        }
        
        break;
    }
    
    w.ct1 = 0;
    
    effect_queue_translucent(w);
}
}

// effect.c:869 bhControlEffect: die Prozedur je Effektkennung.
const EffectProcedure original_effect_procedures[]={
    {150,bhEff150},
    {151,bhEff151},
    {152,bhEff152},
    {153,bhEff153},
    {154,bhEff154},
    {155,bhEff155},
    {156,bhEff156},
    {157,bhEff157},
    {158,bhEff158},
    {159,bhEff159},
    {160,bhEff160},
    {161,bhEff161},
    {162,bhEff162},
    {163,bhEff163},
    {164,bhEff164},
    {166,bhEff166},
    {167,bhEff167},
    {168,bhEff168},
    {169,bhEff169},
    {171,bhEff171},
    {172,bhEff172},
    {173,bhEff173},
    {175,bhEff175},
    {176,bhEff176},
    {178,bhEff178},
    {179,bhEff179},
    {180,bhEff180},
    {181,bhEff181},
    {182,bhEff182},
    {183,bhEff183},
    {184,bhEff184},
    {185,bhEff185},
    {187,bhEff187},
    {188,bhEff188},
    {190,bhEff190},
    {192,bhEff192},
    {194,bhEff194},
    {2,bhEff002},
    {5,bhEff005},
    {8,bhEff008},
    {10,bhEff010},
    {11,bhEff011},
    {14,bhEff014},
    {15,bhEff015},
    {29,bhEff029},
    {101,bhEff101},
    {103,bhEff103},
    {104,bhEff104},
    {116,bhEff116},
    {117,bhEff117},
    {119,bhEff119},
    {121,bhEff121},
    {122,bhEff122},
    {123,bhEff123},
    {128,bhEff128},
    {131,bhEff131},
    {132,bhEff132},
    {206,bhEff206},
    {208,bhEff208},
    {214,bhEff214},
    {215,bhEff215},
    {222,bhEff222},
    {224,bhEff224},
    {231,bhEff231},
    {241,bhEff241},
    {144,bhEff144},
};
const std::size_t original_effect_procedure_count=sizeof(original_effect_procedures)/sizeof(original_effect_procedures[0]);
}
