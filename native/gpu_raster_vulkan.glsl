// Shader des Vulkan-Backends (native/gpu_raster_vulkan.cpp). Zur Buildzeit mit glslc nach SPIR-V uebersetzt,
// je Stufe mit einem Makro: RASTER_VS, RASTER_FS, MASK_VS, MASK_FS, FULL_VS, COMPOSITE_FS, RESOLVE_CS.
// Rechnungen wie native/gpu_raster_opengl.cpp (dort Abschnittsweise begruendet) und renderer.cpp rasterize();
// das Lesen-Aendern-Schreiben desselben Pixels ordnet VK_EXT_fragment_shader_interlock in Primitivreihenfolge,
// das Gegenstueck zu den Rasterizer Ordered Views des D3D11-Backends. Alles, was nicht vom Zielpixel abhaengt
// (Abdeckung, Attribute, Texel, Verwerfen), liegt vor dem kritischen Abschnitt; darin nur Lesen und Schreiben.
#version 450
#if defined(RASTER_FS)||defined(MASK_FS)||defined(COMPOSITE_FS)
#extension GL_ARB_fragment_shader_interlock : require
#endif
// IEEE-Arithmetik wie die Bandstufe (-ffp-contract=off) in jeder Stufe: SignedZeroInfNanPreserve (SPIR-V
// Ausfuehrungsart 4461, Faehigkeit 4466, 32 Bit) verbietet dem Treiber Annahmen ueber Inf/NaN/-0. MoltenVK
// uebersetzt sonst mit Metal-Fast-Math (isnan/isinf in div_rn und Rundung waeren nicht mehr gesichert);
// das Metal-Backend setzt entsprechend MTLMathModeSafe.
#extension GL_EXT_spirv_intrinsics : require
spirv_execution_mode(extensions=["SPV_KHR_float_controls"],capabilities=[4466],4461,32);

struct Setup {
    vec4 p0,p1,p2;   // x, y, 1/z, transportierte Tiefe
    vec4 uv0;        // u/z0, v/z0, u/z1, v/z1
    vec4 uv1;        // u/z2, v/z2, area, Wx
    vec4 grad;       // Ux, Uy, Vx, Vy
    vec4 l0,l1,l2;   // Licht xyz, Eckalpha
    vec4 fog;        // F0, F1, F2, Wy
    uvec4 info;      // Farbe, Flags, Nebelfarbe, Divisor | max_alpha << 16
    uvec4 blend;     // GS ALPHA lo, hi, Art (0 undurchsichtig, 1 additiv, 2 GS), 0
    ivec4 box;       // minx, miny, maxx, maxy
};
struct ShadowTri { vec4 v01; vec4 v2a; uvec4 depth; ivec4 box; }; // depth.w = Ebene << 1 | subtract
layout(std430,set=0,binding=0) readonly buffer Setups { Setup setups[]; };
layout(std430,set=0,binding=1) readonly buffer Tris { ShadowTri tris[]; };
// size: Zielbild W x H; mask_w/h: Groesse der transportierten Tiefe; rect/fog_power/layer: Komposit;
// scale/out_w/out_h: Aufloesung.
layout(push_constant) uniform Push {
    vec2 size; uint fog_pixel,wireframe,background,mask_w,mask_h,layer;
    vec4 rect; uint fog_power,scale,out_w,out_h;
} pc;
#if defined(RASTER_FS)||defined(MASK_FS)||defined(COMPOSITE_FS)||defined(RESOLVE_CS)
layout(set=0,binding=2,r32ui) uniform coherent uimage2D Color;
layout(set=0,binding=3,r32f) uniform coherent image2D DepthBuf;
layout(set=0,binding=4,r32f) uniform coherent image2D Transported;
layout(set=0,binding=5,r32ui) uniform coherent uimage2DArray Mask;
#endif

// Gleiche Operanden wie die Bandstufe, ohne Kontraktion oder algebraische Ersatzformel (precise = NoContraction).
// Vulkan erlaubt fuer x/y 2.5 ULP; der Quotient wird ueber den exakten fma-Rest schrittweise auf den
// naechstliegenden Wert (Gleichstand: gerade Mantisse) gezogen, wie IEEE-Division der Bandstufe.
float div_rn(float x,float y){
    precise float q=x/y;
    if(isinf(q)||isnan(q)||q==0.0||isinf(y))return q;
    precise float best=q;precise float rest=abs(fma(-q,y,x));
    for(int step=0;step<3;++step){precise float from=best;
        for(int k=-1;k<=1;k+=2){
            precise float c=uintBitsToFloat(uint(int(floatBitsToUint(from))+k));
            precise float r=abs(fma(-c,y,x));
            if(r<rest||(r==rest&&(floatBitsToUint(c)&1u)==0u)){best=c;rest=r;}}
        if(best==from)break;}
    return best;}
uint ch(uint c,uint s){return (c>>s)&255u;}
float edge(vec2 a,vec2 b,float x,float y){precise float r=(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);return r;}
bool inclusive(vec2 from,vec2 to){return to.y<from.y||(to.y==from.y&&to.x>from.x);}
// Rechteck [minx,maxx+1) x [miny,maxy+1) als zwei Dreiecke; Bildzeile y = Speicherzeile y (NDC -1 = oben).
vec4 corner_position(ivec4 b,uint corner,vec2 size){
    bool right=corner==1u||corner==2u||corner==4u,bottom=corner==2u||corner==4u||corner==5u;
    float x=float(right?b.z+1:b.x),y=float(bottom?b.w+1:b.y);
    return vec4(x/size.x*2.0-1.0,y/size.y*2.0-1.0,0.0,1.0);}

#ifdef RASTER_VS
// Erster Vertex des Zeichenaufrufs = 6 * erster Setupindex (vkCmdDraw firstVertex, in gl_VertexIndex enthalten).
layout(location=0) flat out uint v_index;
void main(){
    uint index=uint(gl_VertexIndex)/6u,corner=uint(gl_VertexIndex)%6u;
    gl_Position=corner_position(setups[index].box,corner,pc.size);v_index=index;}
#endif

#ifdef RASTER_FS
layout(pixel_interlock_ordered) in;
layout(location=0) flat in uint v_index;
layout(set=1,binding=0) uniform sampler2D Tex;
layout(set=1,binding=1) uniform usampler2D TexPoint;
uint lit1(uint base,uint s,float f){precise float v=float(ch(base,s))*f+0.5;return uint(clamp(v,0.0,255.0));}
uint lit(uint base,float r,float g,float b){return (base&0xff000000u)|(lit1(base,16u,r)<<16)|(lit1(base,8u,g)<<8)|lit1(base,0u,b);}
uint modulate(uint t,uint l,uint d){
    return 0xff000000u|(min(255u,(ch(t,16u)*ch(l,16u))/d)<<16)|(min(255u,(ch(t,8u)*ch(l,8u))/d)<<8)|min(255u,(ch(t,0u)*ch(l,0u))/d);}
// chunk_blend.cpp blend_gs: (A - B) * C + D je Kanal.
uint blend_gs(uint lo,uint hi,uint source,uint destination,float source_alpha){
    uint a=lo&3u,b=(lo>>2)&3u,c=(lo>>4)&3u,d=(lo>>6)&3u;
    float alpha=clamp(source_alpha,0.0,1.0);
    precise float factor=c==0u?alpha:c==1u?div_rn(float(destination>>24),255.0):div_rn(float(hi&255u),128.0);
    precise float av=alpha*255.0+0.5;
    uint result=uint(av)<<24;
    for(uint shift=0u;shift<24u;shift+=8u){
        float ca=a==0u?float(ch(source,shift)):a==1u?float(ch(destination,shift)):0.0;
        float cb=b==0u?float(ch(source,shift)):b==1u?float(ch(destination,shift)):0.0;
        float cd=d==0u?float(ch(source,shift)):d==1u?float(ch(destination,shift)):0.0;
        precise float v=(ca-cb)*factor+cd+0.5;
        result|=uint(clamp(v,0.0,255.0))<<shift;}
    return result;}
// Flags wie gpu_raster_metal.mm: 1 gouraud, 2 vertex_alpha, 4 textured, 8 filtered, 16 punch, 32 fogged,
// 64 fog_color_set, 128 clamp_u, 256 clamp_v, 512 cutout, 1024 uniform_alpha, 2048 linear_only,
// 4096 nonopaque, 16384 general_water, 32768 premultiplied chain, 65536 depth_write.
void main(){
    Setup s=setups[v_index];
    vec4 p0=s.p0,p1=s.p1,p2=s.p2,uv0=s.uv0,uv1=s.uv1,grad=s.grad,l0=s.l0,l1=s.l1,l2=s.l2,fogv=s.fog;uvec4 info=s.info,blend=s.blend;
    float X=gl_FragCoord.x,Y=gl_FragCoord.y; // Pixelmitte x + 0.5, y + 0.5
    float area=uv1.z;
    precise float a=div_rn(edge(p1.xy,p2.xy,X,Y),area);
    precise float b=div_rn(edge(p2.xy,p0.xy,X,Y),area);
    precise float c=div_rn(edge(p0.xy,p1.xy,X,Y),area);
    if(a<0.0||b<0.0||c<0.0)discard;
    if((a==0.0&&!inclusive(p1.xy,p2.xy))||(b==0.0&&!inclusive(p2.xy,p0.xy))||(c==0.0&&!inclusive(p0.xy,p1.xy)))discard;
    precise float invz=p0.z+b*(p1.z-p0.z)+c*(p2.z-p0.z);
    uint color=info.x,flags=info.y,divisor=info.w&0xffffu;float max_alpha=float(info.w>>16);
    precise float va=a*l0.w+b*l1.w+c*l2.w;
    precise float material_alpha=div_rn(float(color>>24),255.0)*((flags&2u)!=0u?clamp(va,0.0,1.0):1.0);
    if(material_alpha<=0.0)discard; // TEST GREATER 0 / KEEP
    uint kind=blend.z;
    uint pixel=color;precise float coverage=1.0;
    if((flags&1u)!=0u){
        precise float lr=a*l0.x+b*l1.x+c*l2.x;precise float lg=a*l0.y+b*l1.y+c*l2.y;precise float lb=a*l0.z+b*l1.z+c*l2.z;
        pixel=lit(color,lr,lg,lb);}
    if((flags&4u)!=0u){
        precise float rz=div_rn(1.0,invz);
        precise float u=(a*uv0.x+b*uv0.z+c*uv1.x)*rz;
        precise float v=(a*uv0.y+b*uv0.w+c*uv1.y)*rz;
        if((flags&8u)==0u){
            if((flags&128u)!=0u)u=clamp(u,0.0,1.0);else u-=floor(u);
            if((flags&256u)!=0u)v=clamp(v,0.0,1.0);else v-=floor(v);
            ivec2 size=textureSize(TexPoint,0);uint w=uint(size.x),h=uint(size.y);
            precise float fu=u*float(w);precise float fv=v*float(h);
            uint texel=texelFetch(TexPoint,ivec2(int(min(w-1u,uint(fu))),int(min(h-1u,uint(fv)))),0).r;
            if((texel>>24)==0u)discard;
            pixel=modulate(texel,pixel,divisor);
            if(kind!=0u)coverage=div_rn(float(texel>>24),255.0);
        }else{
            // Ableitungen von (u,v) je Bildpixel wie die Bandstufe: d(U/W) = (dU - u*dW)/W.
            float Wx=uv1.w,Wy=fogv.w;
            precise vec2 gx=vec2((grad.x-u*Wx)*rz,(grad.z-v*Wx)*rz);precise vec2 gy=vec2((grad.y-u*Wy)*rz,(grad.w-v*Wy)*rz);
            if((flags&128u)==0u)u-=floor(u);
            if((flags&256u)==0u)v-=floor(v);
            vec4 t=(flags&2048u)!=0u?textureLod(Tex,vec2(u,v),0.0):textureGrad(Tex,vec2(u,v),gx,gy);
            if(t.a<=0.0)discard;
            if(kind!=0u)coverage=min(1.0,(flags&1024u)!=0u?div_rn(max_alpha,255.0):t.a);
            else if((flags&512u)!=0u&&(flags&16u)==0u){coverage=min(1.0,div_rn(t.a*255.0,max_alpha));if(coverage<div_rn(1.0,32.0))discard;}
            vec3 rgb=(flags&32768u)!=0u?vec3(div_rn(t.r,t.a),div_rn(t.g,t.a),div_rn(t.b,t.a)):t.rgb;
            precise vec3 q=rgb*255.0+0.5;
            uint texel=0xff000000u|(uint(clamp(q.r,0.0,255.0))<<16)|(uint(clamp(q.g,0.0,255.0))<<8)|uint(clamp(q.b,0.0,255.0));
            pixel=modulate(texel,pixel,divisor);
        }
    }
    if((flags&32u)!=0u){ // GS: Cv = (Cs*F + Cf*(255-F)) >> 8
        precise float fv=a*fogv.x+b*fogv.y+c*fogv.z;
        uint Fc=uint(clamp(fv,0.0,255.0));uint fog=(flags&64u)!=0u?info.z:pc.fog_pixel;
        pixel=(pixel&0xff000000u)|(((ch(pixel,16u)*Fc+ch(fog,16u)*(255u-Fc))>>8)<<16)|(((ch(pixel,8u)*Fc+ch(fog,8u)*(255u-Fc))>>8)<<8)|((ch(pixel,0u)*Fc+ch(fog,0u)*(255u-Fc))>>8);
    }
    bool depth_write=(flags&65536u)!=0u;
    precise float tz=a*p0.w+b*p1.w+c*p2.w;
    ivec2 at=ivec2(gl_FragCoord.xy);
    beginInvocationInterlockARB();
    float below_depth=imageLoad(DepthBuf,at).r;
    if(!(invz<below_depth)){ // GEQUAL
        uint below=imageLoad(Color,at).r;
        if(kind==1u){ // additiv, ohne Tiefe
            precise float alpha=coverage*material_alpha;uint mixed=0xff000000u;
            for(uint shift=0u;shift<24u;shift+=8u){precise float m=float(ch(pixel,shift))*alpha+0.5;mixed|=min(255u,ch(below,shift)+uint(m))<<shift;}
            imageStore(Color,at,uvec4(mixed));
        }else if(kind==2u){ // GS ALPHA; Tiefe ohne transportierte Tiefe (Ps2ZbuffOff alpha >= 128 / direktes Wasser)
            precise float alpha=coverage*material_alpha;
            imageStore(Color,at,uvec4(blend_gs(blend.x,blend.y,pixel,below,alpha)));
            if(depth_write&&((flags&16384u)!=0u?alpha>0.0:alpha>=0.5))imageStore(DepthBuf,at,vec4(invz));
        }else{
            bool partial=false;
            if(coverage<0.999){
                uint mixed=0xff000000u;
                for(uint shift=0u;shift<24u;shift+=8u){precise float m=float(ch(below,shift))*(1.0-coverage)+float(ch(pixel,shift))*coverage+0.5;mixed|=uint(m)<<shift;}
                pixel=mixed;partial=coverage<0.5;
            }
            if(partial)imageStore(Color,at,uvec4(pixel));
            else{
                if(depth_write){imageStore(DepthBuf,at,vec4(invz));imageStore(Transported,at,vec4(tz));}
                imageStore(Color,at,uvec4((pc.wireframe!=0u&&min(min(a,b),c)>0.025)?pc.background:pixel));
            }
        }
    }
    endInvocationInterlockARB();
}
#endif

#ifdef MASK_VS
// Maskenrasterung von bhDrawMdfEffect (shadow_raster.h) im 640x480-Bild.
layout(location=0) flat out uint v_index;
void main(){
    uint index=uint(gl_VertexIndex)/6u,corner=uint(gl_VertexIndex)%6u;
    gl_Position=corner_position(tris[index].box,corner,vec2(640.0,480.0));v_index=index;}
#endif

#ifdef MASK_FS
layout(pixel_interlock_ordered) in;
layout(location=0) flat in uint v_index;
void main(){
    ShadowTri t=tris[v_index];
    vec2 p0=t.v01.xy,p1=t.v01.zw,p2=t.v2a.xy;float X=gl_FragCoord.x,Y=gl_FragCoord.y;
    precise float a=edge(p1,p2,X,Y);precise float b=edge(p2,p0,X,Y);precise float c=edge(p0,p1,X,Y);
    if(a<0.0||b<0.0||c<0.0)discard;
    if((a==0.0&&!inclusive(p1,p2))||(b==0.0&&!inclusive(p2,p0))||(c==0.0&&!inclusive(p0,p1)))discard;
    precise float sum=a*float(t.depth.x)+b*float(t.depth.y)+c*float(t.depth.z);
    precise float z=div_rn(sum,t.v2a.z);
    precise float fx=div_rn(X*float(pc.mask_w),640.0);precise float fy=div_rn(Y*float(pc.mask_h),480.0);
    uint sx=min(pc.mask_w-1u,uint(fx)),sy=min(pc.mask_h-1u,uint(fy));
    // Transportierte Tiefe stammt aus dem abgeschlossenen Farbrasterpass (Barriere davor), anderes Pixel.
    if(uint(z)<uint(imageLoad(Transported,ivec2(sx,sy)).r))discard; // GEQUAL
    ivec3 cell=ivec3(ivec2(gl_FragCoord.xy),int(t.depth.w>>1u));
    beginInvocationInterlockARB();
    int value=int(imageLoad(Mask,cell).r)+((t.depth.w&1u)!=0u?-32:32); // shadow_mask_fragment
    imageStore(Mask,cell,uvec4(uint(clamp(value,0,255))&0xf8u));
    endInvocationInterlockARB();
}
#endif

#ifdef FULL_VS
void main(){vec2 p=vec2(float((gl_VertexIndex<<1)&2),float(gl_VertexIndex&2));gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
#endif

#ifdef COMPOSITE_FS
layout(pixel_interlock_ordered) in;
void main(){
    ivec2 at=ivec2(gl_FragCoord.xy);
    float y=gl_FragCoord.y-0.5,x=gl_FragCoord.x-0.5;
    precise float fy=div_rn((y+0.5)*480.0,pc.size.y);int my=min(479,int(fy));
    precise float fx=div_rn((x+0.5)*640.0,pc.size.x);int mx=min(639,int(fx));
    // Maske aus dem abgeschlossenen Maskenpass (Barriere davor), Zelle des 640x480-Bildes.
    uint value=0u;
    if(!(float(my)+0.5<pc.rect.y||float(my)+0.5>=pc.rect.w)&&!(float(mx)+0.5<pc.rect.x||float(mx)+0.5>=pc.rect.z))
        value=imageLoad(Mask,ivec3(mx,my,int(pc.layer))).r;
    beginInvocationInterlockARB();
    if(value!=0u){
        uint below=imageLoad(Color,at).r;
        uint reduction=(value*((pc.fog_power*3u)/16u))/128u; // shadow_mask_composite
        uint result=below&0xff000000u;
        for(uint shift=0u;shift<24u;shift+=8u){uint v=ch(below,shift);result|=(v>reduction?v-reduction:0u)<<shift;}
        imageStore(Color,at,uvec4(result));
    }
    endInvocationInterlockARB();
}
#endif

#ifdef RESOLVE_CS
// Aufloesung in das Grundbild: S = 1 behaelt das Pixelalpha, sonst gerundeter RGB-Mittelwert, Alpha FF,
// groesstes 1/z (renderer.cpp run).
layout(local_size_x=16,local_size_y=16) in;
layout(std430,set=0,binding=6) writeonly buffer OutColor { uint out_color[]; };
layout(std430,set=0,binding=7) writeonly buffer OutDepth { float out_depth[]; };
void main(){
    uvec2 gid=gl_GlobalInvocationID.xy;
    if(gid.x>=pc.out_w||gid.y>=pc.out_h)return;
    uint S=pc.scale,at=gid.y*pc.out_w+gid.x;
    if(S==1u){out_color[at]=imageLoad(Color,ivec2(gid)).r;out_depth[at]=imageLoad(DepthBuf,ivec2(gid)).r;return;}
    uint r=0u,g=0u,b=0u;float nearest=0.0;uint n=S*S;
    for(uint sy=0u;sy<S;++sy)for(uint sx=0u;sx<S;++sx){ivec2 q=ivec2(gid*S+uvec2(sx,sy));uint cv=imageLoad(Color,q).r;
        r+=cv&255u;g+=(cv>>8)&255u;b+=(cv>>16)&255u;nearest=max(nearest,imageLoad(DepthBuf,q).r);}
    out_color[at]=0xff000000u|(((b+n/2u)/n)<<16)|(((g+n/2u)/n)<<8)|((r+n/2u)/n);out_depth[at]=nearest;
}
#endif
