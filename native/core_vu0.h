/* VU0 der PS2: Registersatz, Makromodus-Transfers und die Mikroprogramme.
 *
 * Das Original rechnet die Vertexpipeline der Chunk-Modelle auf VU0: der
 * EE-Code in ps2_NinjaCnk.c laedt Operanden per COP2-Makromodus und ruft mit
 * `vcallms` ein Mikroprogramm aus ps2_vu0.vsm. Beides liegt hier Befehl fuer
 * Befehl portiert. Mikroprogrammadressen sind Byteadressen im VU0-Mikrospeicher
 * und entsprechen `.vsm`-Offset minus 0x18 (die ersten drei Quadwords des
 * Pakets sind DMA-/VIF-Tags); die Werte stehen als Immediate im ELF.
 *
 * Makromodus und Mikromodus teilen denselben Registersatz, genau wie auf der
 * Hardware: `_Make_ClipMatrix` laedt vf24..vf27 im Makromodus, und
 * VU0_CALCPOINT_INIT sichert sie als Clipmatrix in den VU-Speicher. */
#ifndef RECVX_CORE_VU0_H
#define RECVX_CORE_VU0_H
#ifdef __cplusplus
extern "C" {
#endif

/* Mikroprogrammadressen (ps2_vu0.vsm). Portiert sind die drei der
 * Vertexpipeline; die uebrigen sind als Luecke gemeldet, nicht gestubbt. */
enum {
    RECVX_VU0_MINMAX            = 0x000, /* .vsm 0x0018, offen */
    RECVX_VU0_LOOP_POLY         = 0x020, /* .vsm 0x0038, offen */
    RECVX_VU0_CALCPOINT_INIT    = 0x038, /* .vsm 0x0050 */
    RECVX_VU0_CALCPOINT_END     = 0x150, /* .vsm 0x0168 */
    RECVX_VU0_CALCPOINT         = 0x188  /* .vsm 0x01A0 */
};

/* Registersatz. vf/ACC als Float-Vierer, vi 16 Bit wie auf der Hardware. */
typedef struct RECVX_VU0 {
    float vf[32][4];
    float acc[4];
    float q, i, p;
    unsigned short vi[16];
    unsigned int clip;            /* 24-Bit-Schieberegister, 6 Bit je CLIPw */
    unsigned int mac;             /* aktuell sichtbare MAC-Flags */
    unsigned int mac_pipe[4];     /* Verzoegerung: FMAND sieht die Flags von vor 4 Zyklen */
    float mem[256][4];            /* VU0-Datenspeicher, 4 KB */
    unsigned int unsupported;     /* Zahl der Aufrufe nicht portierter Mikroprogramme */
    unsigned int last_unsupported;/* zuletzt angeforderte, nicht portierte Adresse */
} RECVX_VU0;

RECVX_VU0* recvx_vu0(void);

/* COP2-Makromodus: Transferbefehle. `field` ist 0..3 fuer x..w. */
void recvx_vu0_lqc2(int vf, const void* addr);        /* lqc2 vfN, addr */
void recvx_vu0_sqc2(int vf, void* addr);              /* sqc2 vfN, addr */
void recvx_vu0_qmfc2(int vf, void* addr);             /* qmfc2 rt, vfN + sq */
void recvx_vu0_qmtc2(int vf, const void* addr);       /* lq + qmtc2 rt, vfN */
float recvx_vu0_getf(int vf, int field);              /* qmfc2 + mtc1 */
void recvx_vu0_setf(int vf, int field, float value);  /* mfc1 + qmtc2 */
unsigned int recvx_vu0_cfc2(int vi);                  /* cfc2 rt, viN (vi18 = CLIP) */
void recvx_vu0_ctc2(int vi, unsigned int value);      /* ctc2 rt, viN */
void recvx_vu0_vcallms(unsigned int address);         /* vcallms <Mikroadresse> */

/* Makromodus-Bloecke der Decomp, je als Befehlsfolge portiert (siehe core_vu0.c). */
void recvx_vu0_cnk_load_lights(const float v[11][4]); /* ps2_NinjaCnk.c:1351 */
int  recvx_vu0_cvv(const float* v0);                  /* ps2_NinjaCnk.c:1832 _CVV */

#ifdef __cplusplus
}
#endif
#endif
