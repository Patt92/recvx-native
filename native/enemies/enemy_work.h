#ifndef RECVX_NATIVE_ENEMY_WORK_H
#define RECVX_NATIVE_ENEMY_WORK_H
#include <stdint.h>

/* ===========================================================================
   T1 Stufe A — gemeinsamer Enemy-Work-/Dienstvertrag
   ---------------------------------------------------------------------------
   Einziger Gegnerbesitzer bleibt die vorhandene native WorldEntity/RoomWorld
   (native/world.h). Diese Datei legt KEINE zweite Workwelt an: sie beschreibt
   genau die originalen BH_PWORK-Felder (include/ps2/veronica/prog/types.h:238,
   Gesamtgroesse 0x580) als EINEN transparenten Feldsatz, der als EIN Member in
   der bestehenden WorldEntity liegt. Es gibt dadurch:
     - keine eigene Allokation und keine eigene Lebensdauer,
     - keine Tabelle, Liste oder Arena auf dieser Seite,
     - kein sys/plp/rom/ene/eff und keine BH_PWORK-Spiegeltabelle,
     - keinen Dispatcher, der Originalkörper aufruft.
   Der Unterschied zum verbotenen Muster in native/en01_damage.h ist genau das:
   dort ist `RECVX_EN01` ein 16-KiB-OPAKER, getrennt per shared_ptr allokierter
   Block, der Pose/HP/Modi parallel zur WorldEntity traegt — zwei Besitzer.
   Hier ist der Feldsatz transparent, liegt IN der WorldEntity, und jedes Feld
   hat genau eine Autoritaet (Tabelle unten).

   Verbindliche Fundstellen (nur Semantikreferenz, keine Koerperuebernahme):
     eneset.c:51   bhJumpEnemy[100]        Dispatchtabelle
     eneset.c:170  bhSetEnemy              Allokation/Initialisierung
     eneset.c:325  bhCheckEneWorkNum       sys->ewk_n
     eneset.c:344  bhControlEnemy          Reihenfolge/Filter je Pass
     eneset.c:797  bhDrawEnemy             Zeichenfilter
     eneset.c:866  bhDrawEneObject         en_obj-Nachlauf
     eneset.c:906  bhEne_CallocWork        EXP-Block-Allokator
     zonzon1.c:806 bhEne_InitDamage        Abschluss je Gegnerpass
     pwksub.c:85   bhSetFloorNum           Etagenwahl aus rom->grand
     pwksub.c:445  bhSearchNearEnemy2      Zielbarkeitsfilter
     event.c:2715  bhEneSetCk              setzt NUR stflg |= 0x1000000
     en01.c:2712   bhEne01_Init            flg |= 0x178, flg &= ~2, Bit 0 bleibt
   =========================================================================== */

/* ---------------------------------------------------------------------------
   FELD-AUTORITAET (verbindlich fuer alle Teilaufgaben)

   A) Felder, die dieser Satz besitzt (Original hat sie, WorldEntity nicht):
      flg, flg2, stflg, mdflg, param, flr_nob, mode0..3, ct0..3,
      pxb/pyb/pzb, axb/ayb/azb, gpx/gpy/gpz, lox/loy/loz, aox/aoy/aoz,
      ofx/ofy/ofz, spd, xn/yn/zn, sx..szb, axp/ayp/azp,
      ar/aw/ah/ad/car/cah, cpcl, clp_jno[8], lok_jno,
      mtn_md, mtn_attr, frm_mode, mdl_no, hokan_rate/count/ctbak, mtn_tp,
      mtn_chk, dam[64], djnt_no, cpcl_no, dax/day, dpx..dvz,
      wpnr_no/wpnl_no, at_flg, comb_flg/comb_wep/comb_pnt/comb_timeout,
      total_dam, wax..wazp, watr, flr_snd, ko_num, footeff, src_no,
      psh_ct/psh_idx, lkono, lkwkp, idx_ct, exp0..exp3.

   B) Felder, die das Original ebenfalls in BH_PWORK haelt und die hier
      mitgefuehrt werden, weil die Mechanismen sie staendig lesen/schreiben;
      Autoritaet ist WAEHREND DES GEGNERPASSES dieser Satz, danach
      veroeffentlicht recvx_enemy_publish() sie EINMAL in die vorhandenen
      WorldEntity-Member. Richtung ist einseitig (Work -> Entity), genau dort,
      wo das Original bhCalcModel ruft. Das ist dieselbe Regel, die AGENTS.md
      fuer Werk gegen Objektbaum schon festgelegt hat.

        Work-Feld        WorldEntity-Member (native/world.h)
        -------------    -----------------------------------
        px,py,pz         transform.position
        ay               transform.yaw  und  angles[1]
        ax, az           angles[0], angles[2]
        flr_no           object_floor
        id               identity        (nur Init, danach konstant)
        type             type
        mdlver           (Modellwahl beim Laden, danach konstant)
        hp               hp
        mtn_no           motion          (room_motion = 0 fuer Bibliothek)
        frm_no           frame           (Host: 16.16 in int64)
        mtn_add          step            (Host: 16.16 in int64)
        flg Bit 1 (0x2)  dead            (sterbend/tot)
        stflg 0x1000000  inactive        (bhEneSetCk)
        mdflg 0x200      Zeichen-Gate    (bhEneRenderSet)
        mdflg 0x1        hidden          (nicht gezeichnet)

   C) Entity -> Work geschieht NUR an zwei Stellen:
      1. recvx_enemy_set() beim Raumladen aus dem ETTY_WORK (bhSetEnemy),
      2. recvx_enemy_adopt() nach Skriptbefehlen, die bereits die
         WorldEntity geschrieben haben (Common/Sub_controll/bhEneMotionSet,
         bhEneSetCk, bhEneRenderSet, Snapshot-Wiederherstellung).
      Jede weitere Schreibrichtung ist ein zweiter Besitzer und verboten.
   --------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { float x, y, z; } RECVX_ENEMY_VEC;

/* CPCL (types.h): Kapselpunkt-Tabelleneintrag, -1 terminiert nicht; die
   Laenge steht beim Verbraucher (weapon.c bhCheckCapCol2Capsule). */
typedef struct { int16_t x, y, z; } RECVX_ENEMY_CPCL;

/* NJS_CAPSULE-Teil, den die Gegner schreiben (watr). */
typedef struct { RECVX_ENEMY_VEC c1, c2; float r; } RECVX_ENEMY_CAPSULE;

/* ATR_WORK-Felder, die ein Gegner in sys->mwalp schreibt (bhEne00,
   eneset.c:429-451). Vollstaendiger 36-Byte-Satz des Originals, damit der
   Besitzer eine echte Flaeche und nicht nur einen Header erhaelt. */
typedef struct {
    uint32_t flg, type, attr;
    int32_t  flr_no;
    float    px, py, pz;
    float    w, h, d;
    uint8_t  prm0, prm1, prm2, prm3;
    /* ATR_WORK.id (types.h:498 Offset 0x02). Waehlt in en03.c:5638 die hohe
       Kante der Schraege und in en03.c:5820/5879 die Lage des rechten
       Winkels; ohne dieses Feld ist diese Geometrie nicht bestimmbar. */
    uint8_t  id;
} RECVX_ENEMY_ATR;

/* O_WRK-Felder, die ein Gegner an einem Raumobjekt schreibt (bhEne56 am
   UFO-Objekt sys->obwp[sys->ufo_oidx]). Transparente Sicht statt Einzel-
   setter, weil das Original Felder und Bits im selben Pass mischt. */
typedef struct {
    uint32_t flg;              /* Bit 0x80: haengt an lkwkp               */
    int32_t  lkono;
    float    lox, loy, loz;
    float    px, py, pz;
    float    yn;
    uint32_t mode1;
    RECVX_ENEMY_VEC root;      /* mlwP->owP[0].mtx[12..14], nur lesend    */
} RECVX_ENEMY_OBJECT;

/* Die BH_PWORK-Felder des SPIELERS, die Gegnermechanismen lesen und (im
   Griff) schreiben. Transparente Sicht mit get/set, implementiert vom
   vorhandenen nativen Spielerbesitzer. Kein Gegner haelt ein eigenes plp:
   ein zweiter Spielerwork ist genau der Fehler, den native/en01_damage.h
   eingebaut hatte. `player_get` liefert 0, wenn kein Spielerbesitzer
   gebunden ist — dann melden die Mechanismen eine Luecke und erfinden
   weder HP noch Pose. */
typedef struct {
    RECVX_ENEMY_VEC pos;
    int32_t  ax, ay, az;
    int32_t  flr_no;
    int32_t  hp;
    uint32_t flg, flg2, stflg, at_flg;
    uint8_t  mode0, mode1, mode2, mode3;
    int32_t  ct0, ct1;
    int32_t  ayp;               /* PlyDG00/01: persistenter Drehschritt     */
    uint32_t mtn_no, mtn_md, hokan_count;
    int32_t  hokan_rate, frm_no, mtn_add;
    /* plp->mnwP: 1 = die Motionquelle des Gegners ist eingehaengt
       (bhEne07_PlayerControl), 0 = plp->mnwPb, die eigene Bibliothek. */
    uint8_t  mtn_enemy;
    uint32_t mtn_enemy_owner;   /* Gegnerindex, dessen Bibliothek gilt      */
    /* plp->mnwP = epw->mnwP (en01.c:7584, en04.c:3405): das greifende Work
       selbst; gleiche Gegner-IDs koennen verschiedene mnwP tragen
       (bhSetEneMtn-Raumform), deshalb reicht der Index nicht. */
    const struct RECVX_ENEMY_WORK* mtn_enemy_work;
    int32_t  wpnr_no;
    /* plp->car/plp->cah: PlyInfo[sys->ply_id][0/1] - 1.0f (player.c:199/708).
       bhCheckPlayer (hitchk.c:5712) und bhCheckEnemies (hitchk.c:5788) rechnen
       damit; plp->aox/aoy/aoz sind im Original nur in player.c:713 genullt und
       werden nirgends geschrieben, also 0. */
    float    car, cah;
    uint32_t ply_id;            /* sys->ply_id, nur lesend                  */
    uint32_t exp1_flags;        /* plp->exp1 EXP1_I(0)                      */
    /* Read-only host debug policy, not an original player flag.
       player_set never writes this value back to the player owner. */
    uint8_t debug_attack_disabled;
} RECVX_ENEMY_PLAYER;

#define RECVX_ENEMY_SLOTS      128u  /* eneset.c:315 Tabellengrenze       */
#define RECVX_ENEMY_DAM        64u   /* types.h:354 dam[64]               */
#define RECVX_ENEMY_OBJECTS    16u   /* types.h:743 sys->en_obj[16]       */

struct RECVX_ENEMY_WORK;
struct RECVX_ENEMY_SERVICES;

typedef struct RECVX_ENEMY_WORK {
    /* --- Identitaet / Vorlage (eneset.c:183-241) ------------------------- */
    uint32_t flg;            /* Bit 0 kommt aus etp->flg der EGG_WORK-Vorlage */
    uint32_t flg2, stflg, mdflg;
    uint16_t id, type;
    uint8_t  mdlver, param;
    int8_t   flr_no, flr_nob;

    /* --- Modi und Zaehler ----------------------------------------------- */
    uint8_t  mode0, mode1, mode2, mode3;
    int32_t  ct0, ct1, ct2, ct3;

    /* --- Lage ------------------------------------------------------------ */
    float    px, py, pz;
    int32_t  ax, ay, az;
    float    pxb, pyb, pzb;          /* bhControlEnemy sichert sie je Pass  */
    int32_t  axb, ayb, azb;
    float    gpx, gpy, gpz;
    float    lox, loy, loz;
    float    aox, aoy, aoz;
    float    ofx, ofy, ofz;
    float    spd, xn, yn, zn;
    float    sx, sy, sz, sxb, syb, szb;
    int32_t  axp, ayp, azp;

    /* --- Koerper und Kollision ------------------------------------------- */
    float    ar, aw, ah, ad, car, cah;
    const RECVX_ENEMY_CPCL* cpcl;
    int8_t   clp_jno[8];
    int32_t  lok_jno;
    RECVX_ENEMY_CAPSULE watr;

    /* --- Motion ----------------------------------------------------------- */
    uint32_t mtn_no, mtn_md, mtn_attr, frm_mode, mdl_no;
    int32_t  frm_no, mtn_add;        /* 16.16, wie im Original              */
    int32_t  hokan_rate;
    uint32_t hokan_count, hokan_ctbak;
    const void* mtn_tp;
    int32_t  mtn_chk;
    /* mnwP: Auswahl der Motionquelle. 0 = eingebaute Gegnerbibliothek
       (emtp), 1 = Raum-MTN-Liste (sys->rmthp, bhEneMotionSet/bhEne56). */
    uint8_t  mtn_room;

    /* --- Schaden ---------------------------------------------------------- */
    int32_t  hp;
    int32_t  dam[RECVX_ENEMY_DAM];
    int32_t  djnt_no, cpcl_no, dax, day;
    float    dpx, dpy, dpz, dvx, dvy, dvz;
    int32_t  wpnr_no, wpnl_no;
    uint32_t at_flg;
    uint32_t comb_flg, comb_wep;
    int32_t  comb_pnt, comb_timeout, total_dam;
    int16_t  wax, way, waz, waxp, wayp, wazp;

    /* --- Boden/Sound ------------------------------------------------------ */
    int32_t  flr_snd, ko_num, footeff, src_no;
    int16_t  psh_ct, psh_idx;

    /* --- Verknuepfung (Child-/Link-Work) ---------------------------------- */
    /* flg 0x80: dieser Work haengt an lkwkp. bhControlEnemy und bhDrawEnemy
       dereferenzieren lkwkp OHNE NULL-Pruefung, sobald flg 0x80 steht
       (eneset.c:362 / :807). bhSetEnemy setzt lkwkp auf NULL, also muss der
       Erzeuger ihn im selben Pass setzen. Der Vertrag haelt das fest und
       behandelt lkwkp == NULL bei flg 0x80 als Portierungsluecke des
       Erzeugers, nicht als stillen Normalfall. */
    int32_t  lkono;
    struct RECVX_ENEMY_WORK* lkwkp;
    int32_t  idx_ct;                 /* eneset.c:268 Slotindex             */

    /* --- Morph ------------------------------------------------------------ */
    float    shp_ct;

    /* --- Wurzelmatrix (types.h:139/140 mtxbuf[64]/mtx, eneset.c:283
       `epp->mtx = (float(*)[16])epp->mtxbuf`) ------------------------------
       bhCalcModel (MdlPut.c:240) baut die Wurzelmatrix NUR dann aus
       px/py/pz und ax/ay/az, wenn weder flg 0x80 (Link an lkwkp) noch
       flg 0x1000 gesetzt ist. Bei flg 0x1000 ohne flg 0x80 benutzt es die
       hier liegende Matrix UNVERAENDERT weiter (bhCalcTree(ewP->mtx, ...)).
       Genau das ist die angedockte Haltung von bhEne07_NG01 (en07.c:969-976).
       Vertrag an den calc_model-Anbieter: bei (flg & 0x1000) && !(flg & 0x80)
       ist root_mtx die Wurzelmatrix dieses Passes; Lage und Winkel des Works
       werden dann nicht erneut eingerechnet. */
    float    root_mtx[16];

    /* --- EXP-Erweiterungen (bhEne_CallocWork, eneset.c:906) --------------- */
    /* Original: rohe Byteblöcke mit festen Offsets (EXP0_I/EXP0_F-Makros).
       Nativ: je Gegner ein benannter typisierter Block. Das ist eine
       Datenrepraesentations-Anpassung, keine andere Entscheidung: Inhalt,
       Nullinitialisierung und Lebensdauer bleiben gleich. */
    void*    exp0;
    void*    exp1;
    void*    exp2;
    void*    exp3;

    /* --- Rueckverweis auf den einzigen Besitzer --------------------------- */
    /* Kein zweiter Work: das ist die Adresse der WorldEntity, in der dieser
       Feldsatz als Member liegt, plus ihr (kind,index). kind ist immer 1
       (Gegner); index ist der Raumindex des ETTY_WORK. */
    void*    owner;
    uint32_t owner_index;

    /* --- Dienste --------------------------------------------------------- */
    const struct RECVX_ENEMY_SERVICES* sv;
} RECVX_ENEMY_WORK;

/* ---------------------------------------------------------------------------
   Allokation und Raumreihenfolge aller Enemy-Works
   ---------------------------------------------------------------------------
   bhSetEnemy (eneset.c:170) sucht linear ab Slot 0 den ersten Work mit
   !(flg & 1), nullt ihn vollstaendig und initialisiert ihn aus der Vorlage.
   Bei 128 belegten Slots gibt es NULL zurueck — und ruft auch dann
   bhCheckEneWorkNum (eneset.c:317). idx_ct ist der gefundene Slotindex.

   Raumreihenfolge: room.c:114-126 laeuft rom->enep aufsteigend und legt fuer
   jeden ETTY_WORK mit flg & 1 ueber bhSetEneMdl/bhSetEnemy einen Work an.
   Der Slotindex ist damit beim Raumladen gleich dem Raumindex i, solange
   kein Eintrag uebersprungen wird. Spaeter per bhSetEnemy erzeugte Child-
   Works (en01.c:2819, en02.c:777, en03.c:692/712/739, en05.c:1015,
   en06.c:337/351/1182, en13.c:100..173, en17.c:888, en23.c:492, en29.c:1713)
   belegen die erste freie Luecke, NICHT das Tabellenende.

   bhCheckEneWorkNum (eneset.c:325) setzt sys->ewk_n auf hoechsten belegten
   Index + 1, also einschliesslich Luecken. Jeder Pass laeuft 0..ewk_n-1 und
   ueberspringt Luecken erst ueber das flg-Bit-0-Gate.
   --------------------------------------------------------------------------- */

/* Tabellensicht ueber die vorhandenen WorldEntity-Works eines Raums. Haelt
   nur Zeiger auf fremden Speicher (die Entities) — keine Works, keine Posen,
   keine Kopien. Der Besitzer baut sie je Pass neu auf. */
typedef struct {
    RECVX_ENEMY_WORK* slot[RECVX_ENEMY_SLOTS];
    uint32_t ewk_n;          /* sys->ewk_n, bhCheckEneWorkNum              */
    uint32_t enow;           /* sys->enow, Index des laufenden Passes      */
    /* sys->en_obj/en_objn: Works mit flg 0x80000000, die bhDrawEneObject
       NACH dem normalen Gegnerzeichnen zeichnet (eneset.c:381/866). */
    RECVX_ENEMY_WORK* en_obj[RECVX_ENEMY_OBJECTS];
    uint32_t en_objn;
} RECVX_ENEMY_TABLE;

/* Original-BP_WORK (types.h:2210, 0x40 Byte) der Blutpool-/Blutspurparameter
   einer Gegnerroutine (z.B. en07.c:29 BloodParam, en04.c). Reine Daten des
   Aufrufers; der Effektdienst liest sie nur. */
typedef struct {
    RECVX_ENEMY_VEC off_pos; /* 0x00 */
    int32_t srd_dir;         /* 0x0C */
    float   srd_pos;         /* 0x10 */
    float   bld_spd;         /* 0x14 */
    float   srt_spd[5];      /* 0x18 */
    int32_t srt_dir[5];      /* 0x2C */
} RECVX_ENEMY_BP_WORK;

/* ---------------------------------------------------------------------------
   Dienstvertrag
   ---------------------------------------------------------------------------
   Jeder Eintrag nennt seinen originalen Aufruf und den nativen Anbieter, an
   den der Integrator ihn binden MUSS. Ein nicht gebundener Dienst ist NULL;
   die Mechanismen melden ihn dann namentlich ueber `gap` und tun nichts.
   Kein Eintrag darf durch einen plausiblen Ersatzwert belegt werden.
   --------------------------------------------------------------------------- */
typedef struct RECVX_ENEMY_SERVICES {
    /* Ungebundener Originalmechanismus; `name` ist die Originalroutine. */
    void (*gap)(const char* name);

    /* --- Zufall: native/core_random.h (EE-libc rand, ein Strom) ---------- */
    /* rand() (en04.c) und njRandom() (en07.c) teilen diesen Strom. */
    int32_t (*random_next)(void);

    /* --- Kollision: native/geometry_motion.h + native/effect_collision.h - */
    /* bhCheckWallType (hitchk.c): Praedikat, 1 = Treffer. */
    int   (*wall_type)(const RECVX_ENEMY_VEC* pos, uint32_t flg, float r, float h);
    /* bhCheckWallEx (hitchk.c): bewegt `pos` aus `prev`; 1 = Kontakt. */
    int   (*wall_move)(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* pos,
                       const RECVX_ENEMY_VEC* prev, float r, float h, uint32_t flg);
    /* hitchk.c:5391 bhCheckDansa(pp): laeuft rom->etcp und danach sys->metcp
       ab, loescht stflg 0x20000 und setzt es wieder, wenn der Fuss des Works
       in einer Typ-2-Flaeche mit prm0 != 0 auf derselben Etage liegt und
       stflg 0x30 frei ist (hitchk.c:5400-5424). bhCheckWallEx ruft dieselbe
       Routine, wenn stflg 0x20000 schon steht (hitchk.c:664-667). Der Besitzer
       stellt beide Tabellen in der Originalreihenfolge bereit; NULL -> der
       Aufrufer meldet "bhCheckDansa (hitchk.c:5391)". */
    void  (*check_dansa)(RECVX_ENEMY_WORK* w);
    /* bhCollisionCheckLine / Line2 / Line3: `to` und `normal` werden bei
       Treffer gesetzt, bei Fehltreffer unveraendert gelassen.
       `variant` 1/2/3 waehlt die Originalfassung. `rec` (optional) erhaelt
       den vollstaendigen 36-Byte-ATR-Satz des Treffers, `id` eine stabile
       Identitaet dafuer (0 = keine), damit ein Gegner dieselbe Flaeche
       wiedererkennen kann — das ist der originale EXP0_ATR-Zeigervergleich
       (bhEne07_FloorCollision gegen bhEne07_ObjEdge). */
    int   (*line)(uint32_t variant, const RECVX_ENEMY_VEC* from, RECVX_ENEMY_VEC* to,
                  RECVX_ENEMY_VEC* normal, uint32_t flg, int floor,
                  RECVX_ENEMY_ATR* rec, uint32_t* id);
    /* bhGetGroundPosition (hitchk.c); -1000 ist die originale Leermarke. */
    float (*ground)(const RECVX_ENEMY_VEC* pos);
    /* bhCheckFloorNum (pwksub.c:112). */
    int   (*floor_num)(float py);
    /* rom->grand[i] (pwksub.c:85 bhSetFloorNum liest genau diese Tabelle). */
    float (*grand)(uint32_t index);
    /* bhGetHitCollisionNormal: Normale des letzten Wandtreffers. */
    int   (*hit_normal)(RECVX_ENEMY_VEC* normal);
    /* bhCheckRoute / rutchk.c fuer die Wegewahl von en04. */
    int   (*route)(int from, int to);
    /* npCollisionCheckSC (njplus.c): Kugel gegen Kapsel. */
    int   (*sphere_capsule)(const RECVX_ENEMY_VEC* c, float r,
                            const RECVX_ENEMY_VEC* c1, const RECVX_ENEMY_VEC* c2, float cr);
    /* hitchk.c bhKaidanAtrCheck / bhSetUseKaidanFlag / bhClrUseKaidanFlag. */
    int   (*stair_area)(const RECVX_ENEMY_VEC* pos, int floor);
    void  (*stair_use)(int index, int used);
    /* en03.c:5220 bhEne03_Collision(epw): gemeinsamer Gegner-gegen-
       Raumwand-Durchlauf ueber rom->walp + sys->mwalp (letzte Trefferflaeche
       gewinnt; Trefferflags flg&1, Doppelseiten-Typen bei w->flg 0x400 aus,
       Typ 0..7 ueber die en03-eigenen Box-/Zylinder-/Dreieck-/Schraege-/
       Bodenhelfer, die die Position direkt stauchen). Rueckgabe 1 =
       mindestens eine Flaeche getroffen (Original: ATR_WORK* != NULL;
       en03.c:5166 wertet das aus, en07.c:1863 verwirft es). Aufrufer u.a.
       en03sub.c:286/389, en05sub.c, en06.c:1840, en07.c:1863, en08.c,
       en11.c:852, en23.c:3066, en24.c:512, en30.c:84 — ein Mechanismus fuer
       alle, darum ein Dienst (Anbieter: EN03-Port). */
    int   (*collision_works)(RECVX_ENEMY_WORK* w);

    /* --- Audio: native/audio.h -------------------------------------------- */
    /* sdfunc.c RequestEnemySe(sys->enow, pos, se). bhEne_CallSE (zonzon1.c:828)
       unterdrueckt den Aufruf bei flg 0x10000 — dieses Gate liegt im
       Mechanismus, nicht im Dienst. */
    void  (*enemy_se)(uint32_t enemy, const RECVX_ENEMY_VEC* pos, int se);
    void  (*enemy_se_ex)(uint32_t enemy, const RECVX_ENEMY_VEC* pos, int se, int fade);
    int   (*enemy_se_playing)(uint32_t enemy, int se);
    /* bhEne_CallPlayerVoice / CallPlayerVoice (sdfunc.c). */
    void  (*player_voice)(int se);
    /* CallSystemSe(0, se) — bhEne56 Mode 6 nutzt 0x80000211. */
    void  (*system_se)(int se);

    /* --- Assets / Motion: native/actor.h ---------------------------------- */
    /* mlwP->obj_num des aktuell gewaehlten Modells. */
    uint32_t (*obj_num)(const RECVX_ENEMY_WORK* w);
    /* mnwP[mtn].frm_num (MN_WORK) der gewaehlten Motionquelle; 0 = fehlt. */
    uint32_t (*motion_frames)(const RECVX_ENEMY_WORK* w, uint32_t mtn_no);
    /* mnwP[mtn].obj_num (MN_WORK, types.h:70) — die Knotenzahl der MOTION,
       nicht die des Modells. Beide Zahlen sind im Original getrennte Felder
       (ML_WORK types.h:53 gegen MN_WORK types.h:70) und duerfen nicht
       gegeneinander eingesetzt werden. Die Schadensumme mehrerer Gegner
       laeuft ueber genau diese Zahl (en07.c:1020 `epw->mnwP->obj_num`, also
       Motionindex 0), waehrend eneset.c:505 und zonzon1.c:811 dafuer
       mlwP->obj_num nehmen. 0 = fehlt. */
    uint32_t (*motion_obj_num)(const RECVX_ENEMY_WORK* w, uint32_t mtn_no);
    /* bhSetMotion (Motion.c): sampelt frm_no VOR dem Vorschub und stellt
       anschliessend frm_no um add weiter; `md` ist mtn_md. Rueckgabe 1,
       wenn der Clip in diesem Pass umgelaufen ist. */
    int   (*set_motion)(RECVX_ENEMY_WORK* w, int add, uint32_t md);
    /* bhCalcModel (MdlPut.c:240): veroeffentlicht den Work in den Objektbaum.
       Danach sind die Gelenkmatrizen dieses Passes gueltig. */
    void  (*calc_model)(RECVX_ENEMY_WORK* w);
    /* mlwP->owP[joint].mtx — 16 Floats, Zeilenfolge wie NJS_MATRIX. */
    int   (*joint_matrix)(const RECVX_ENEMY_WORK* w, uint32_t joint, float* out16);
    /* njCalcPoint(owP[joint].mtx, local, world) — bhEne_GetPartsPos/
       bhEne_CalcPartsPos (zonzon1.c) bauen darauf auf. */
    int   (*joint_point)(const RECVX_ENEMY_WORK* w, uint32_t joint,
                         const RECVX_ENEMY_VEC* local, RECVX_ENEMY_VEC* world);
    /* bhEne_GetTranslateMtn: Translationsschluessel der laufenden Motion. */
    int   (*motion_translation)(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* out);
    /* MN_WORK md2P->p[0] / p[1]: Positions- bzw. Winkelschluessel des
       WURZELKNOTENS einer Motion auf dem ganzzahligen Frame. Mehrere
       EN07-Zweige lesen diese Rohschluessel direkt (Sprungversatz,
       Deckenandock-Korrektur, Atari-Offset). 0 = Schluessel fehlt. */
    int   (*motion_root_position)(const RECVX_ENEMY_WORK* w, uint32_t mtn_no,
                                  uint32_t frame, RECVX_ENEMY_VEC* out);
    int   (*motion_root_angles)(const RECVX_ENEMY_WORK* w, uint32_t mtn_no,
                               uint32_t frame, int32_t* out3);
    /* mlwP->objP[0].pos — lokale Lage des Modellwurzelknotens. */
    int   (*model_root_position)(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* out);
    /* Zu bhEne_GetPartsPos (zonzon1.c:465): die Routine dreht nach jedem
       njTranslateEx noch per njRotateEx, liest am Ende aber nur
       njGetTranslation. Die Winkel des LETZTEN Kettenglieds wirken daher
       nicht auf das Ergebnis; nur Zwischenglieder brauchen sie. en07s Kette
       junction_tree[0] (en07.c:25) ist {0, -1}, hat also kein Zwischenglied —
       darum genuegt hier model_root_position. Eine Gegnerroutine mit laengerer
       Kette braucht zusaetzlich objP[n].ang; das ist dann mit ihrer eigenen
       Fundstelle nachzutragen. */
    /* mlwP->objP[node].evalflags Bit 0x8 (Knoten nicht auswerten/zeichnen). */
    void  (*model_node_hidden)(RECVX_ENEMY_WORK* w, uint32_t node, int hidden);

    /* Die ene-Liste in Original-Reihenfolge; `enemy_count` ist sys->ewk_n und
       `enemy_now` ist sys->enow. Mehrere EN07-Zweige durchsuchen ihre eigene
       Art (Schwarmabstand, Deckenplatz). Reiner Lesezugriff auf dieselbe
       Tabelle — keine zweite Liste. */
    uint32_t (*enemy_count)(void);
    RECVX_ENEMY_WORK* (*enemy_at)(uint32_t index);
    uint32_t (*enemy_now)(void);

    /* SYS_WORK-Flagbaenke des vorhandenen Besitzers. `which`:
       0 = sys->cb_flg, 1 = sys->rm_flg, 2 = sys->st_flg, 3 = sys->sp_flg. */
    uint32_t (*system_flags)(uint32_t which);
    void  (*system_flags_or)(uint32_t which, uint32_t bits);

    /* eff[slot].mdlver nach bhSetShadow (bhEne07_Init setzt 1). */
    void  (*effect_set_mdlver)(int slot, uint32_t mdlver);

    /* --- Effekte ---------------------------------------------------------- */
    /* effect.c bhSetEffect(id, &POINT, lkp, param); Rueckgabe Slotindex oder
       -1 bei vollem eff[512]-Pool (effect.c:436 waehlt !(flg & 3)). */
    int   (*set_effect)(int id, const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* off,
                        RECVX_ENEMY_WORK* link, int param);
    /* effect.c:644 bhSetShadow(jtb, lkp, lkono, sx, sy, sz) -> Slotindex oder
       -1 bei vollem eff[512]-Pool (dort nur Opp->flg & 3 geprueft). `jtb` ist
       die mit -1 abgeschlossene Gelenkkette; NULL laesst jno[0] = -1 und
       erzeugt, wie im Original, type 1 statt 0. `link` NULL laesst lkwkp leer
       (flg 0x80 nur bei != NULL). Aufrufer in meinen Dateien: en07.c:215
       (SdwTab {1,3,-1}, lkono 1, 5/2/4) und eneset.c:585 (NULL, lkono 1,
       12/10/12, bhEne56). Folgezugriff en07.c:216 `eff[sdw].mdlver = 1` —
       bei sdw == -1 schreibt das Original ungeschuetzt vor den Pool; der
       Aufrufer meldet das ueber gap, statt fremden Speicher zu beruehren. */
    int   (*set_shadow)(const int8_t* jtb, RECVX_ENEMY_WORK* link, int lkono,
                        float sx, float sy, float sz);
    /* weapon.c bhSetExplosion(&pos). */
    void  (*explode)(const RECVX_ENEMY_VEC* pos);
    /* zonzon1.c:52 bhEne_SetMinceEffect(epw, type, num) — Hackfleischwolke am
       dpx/dpy/dpz des Works; type/num wie im Original (en07.c:1041/1042). */
    void  (*mince_effect)(RECVX_ENEMY_WORK* w, int type, int num);
    /* zonzon1.c:175 bhEne_SetBloodEffect4(pos, vec, col, type1, type2) — OHNE
       Work-Bezug: Position und Geschwindigkeitsvektor kommen als Zeiger
       (en07.c:1043 mit &epw->px/&epw->dvx, en07.c:2168 mit lokalen Punkten). */
    void  (*blood_effect4)(const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* vec,
                            int col, int type1, int type2);
    /* zonzon1.c:243 bhEne_SetBloodEffectBurst(epw, type, jno, pos, flg);
       pos darf NULL sein (en07.c:1078). */
    void  (*blood_effect_burst)(RECVX_ENEMY_WORK* w, int type, int jno,
                                const RECVX_ENEMY_VEC* pos, int flg);
    /* zonzon1.c:354 bhEne_SetFireEffect(epw, jno, ofp, size, len). */
    void  (*fire_effect)(RECVX_ENEMY_WORK* w, int jno, const RECVX_ENEMY_VEC* ofp,
                         float size, int len);
    /* zonzon1.c:397 bhEne_SetAcidEffect(epw, jno, ofp, size). */
    void  (*acid_effect)(RECVX_ENEMY_WORK* w, int jno, const RECVX_ENEMY_VEC* ofp,
                         float size);
    /* zonzon1.c:391 bhEne_BloodPool(epw, pos, ang, param): vier Aufrufe je
       Sterbeanimation in en07.c (1230/1339/1480/1621/1719/1809) mit dem
       Original-BP_WORK der Gegnerroutine. */
    void  (*blood_pool)(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int ang,
                        const RECVX_ENEMY_BP_WORK* param);
    /* vibman.c StartVibrationEx / bhEne_SetVibration. */
    void  (*vibration)(int motor, int power);

    /* --- Spielerschaden und Spielerzustand -------------------------------- */
    /* Der vorhandene native Spielerbesitzer, als transparente Sicht.
       `player_get` liefert 0, wenn kein Spieler gebunden ist. */
    int   (*player_get)(RECVX_ENEMY_PLAYER* out);
    void  (*player_set)(const RECVX_ENEMY_PLAYER* in);
    /* plp->mlwP->owP[joint]: Gelenkmatrix bzw. njCalcPoint darauf. */
    int   (*player_joint_matrix)(uint32_t joint, float* out16);
    int   (*player_joint_point)(uint32_t joint, const RECVX_ENEMY_VEC* local,
                                RECVX_ENEMY_VEC* world);
    /* mnwP[mtn].frm_num der Spielerbibliothek bzw. der eingehaengten
       Gegnerbibliothek (bhEne07_PlayerControl liest sie nach dem Umhaengen). */
    uint32_t (*player_motion_frames)(uint32_t mtn_no, int from_enemy_library);
    /* hitchk.c bhCheckPlayer(ep): Gegner-gegen-Spieler-Kontakt (flg 0x40). */
    int   (*check_player)(RECVX_ENEMY_WORK* w);
    /* weapon.c bhCheckEnemies(ep): Gegner-gegen-Gegner-Kontakt (flg 0x8). */
    int   (*check_enemies)(RECVX_ENEMY_WORK* w);
    /* hitchk.c bhCheckWall(ep) fuer flg 0x10 (Gegner schiebt Wand-ATR). */
    int   (*check_wall)(RECVX_ENEMY_WORK* w);
    /* Schaden an den Spieler; `kind` ist die originale Routine
       (z.B. "bhEne07_PlayerControl"), damit ein offener Zweig sichtbar ist. */
    void  (*player_damage)(RECVX_ENEMY_WORK* w, const char* kind, int amount);
    /* zonzon1.c bhEne_LeverCheck: Hebelruetteln waehrend eines Griffs.
       Rueckgabe = Anzahl neuer Eingaben seit dem letzten Aufruf. */
    int   (*lever_check)(void);
    /* eneset.c:349-350: bhControlEnemy loescht plp->ofx und plp->ofz
       UNBEDINGT und VOR dem sp_flg-0x2-Test. Das sind Felder des
       Spielerbesitzers, nicht eines Gegnerworks — darum ein Dienst. */
    void  (*player_clear_offset)(void);

    /* subpl.c:143 bhEne_Event(epw) = subpl_mv01_tbl[ct2]: der gemeinsame
       Mode0[5]-Zweig fast aller Gegnertabellen (en07.c:73, en03.c:415,
       en01/en04/en05/...). Er setzt ct2 aus ct0, laeuft den Subplayer-
       Bewegungsapparat und schaltet bei ct2 > 2 auf Mode0 um. Der
       Mechanismus gehoert dem Subplayer-Besitzer; hier bleibt nur der
       Aufruf. NULL -> gap("bhEne_Event"). */
    void  (*event)(RECVX_ENEMY_WORK* w);
    /* zonzon1.c:591 bhEne_SetWeponAtr(epw, j1, j2, r): Waffenkapsel aus ZWEI
       Gelenkmatrizen; c1 = mlwP->owP[j1].mtx[12..14], c2 = owP[j2], r = r.
       Belegte Aufrufer mit beiden Gelenken: en02.c:738 (11,3,10.0f),
       en03.c:614/618 (35,1/6,5.0f), en06.c:265 (10,1,2.0f), en07.c:124
       (6,11,2.0f), en12.c:502 (5,21,4.0f), en14.c:206 (6,1,5.0f),
       en23.c:434/438 (22,1/12,8.0f), en30.c:92 (0,3,3.0f). */
    void  (*weapon_atr)(RECVX_ENEMY_WORK* w, int joint1, int joint2, float r);
    /* sys->mwalp / sys->mwal_n: bewegliche ATR_WORK-Flaechen, die ein Gegner
       veroeffentlicht (bhEne00, eneset.c:427-451). `alloc` liefert den
       originalen Index (sys->mwal_n++) oder -1 bei erschoepfter Tabelle; der
       Aufrufer darf dann KEINE Ersatzflaeche einsetzen. Die Flaeche folgt in
       den Folgepaessen der Gegnerposition (eneset.c:496-498). */
    /* `get` erlaubt das originale Lesen-Aendern-Schreiben auf derselben
       Flaeche (bhEne00 Mode 0 else-Zweig und Mode 1 loeschen einzelne Bits). */
    int   (*mobile_wall_alloc)(void);
    int   (*mobile_wall_get)(int index, RECVX_ENEMY_ATR* out);
    void  (*mobile_wall_set)(int index, const RECVX_ENEMY_ATR* atr);

    /* flag.c bhCkFlg(sys->ev_flg, no) — bhEne56 liest Eventflag 0x3B. */
    int   (*event_flag)(uint32_t no);
    /* sys->obwp[no]: der Objektwork, an den bhEne56 das UFO haengt. Als
       transparente Sicht mit Lesen-Aendern-Schreiben, weil das Original
       einzelne Felder und Bits mischt (eneset.c:592-602, :699-780).
       `root` ist mlwP->owP[0].mtx[12..14] und nur lesend gueltig. */
    int   (*object_get)(uint32_t object, RECVX_ENEMY_OBJECT* out);
    /* `link` != NULL setzt op->lkwkp auf diesen Gegnerwork. */
    void  (*object_set)(uint32_t object, const RECVX_ENEMY_OBJECT* in,
                        RECVX_ENEMY_WORK* link);

    /* SYS_WORK-Felder, die bhEne56 mit dem Event-/Objektbesitzer teilt:
       sys->ufo_pos, sys->ufo_md, sys->ufo_flg, sys->ufo_oidx. Sie gehoeren
       nicht dem Gegnerwork; ein eigener Satz waere ein zweiter Besitzer. */
    int   (*ufo_get)(RECVX_ENEMY_VEC* pos, int* md, int* flg, uint32_t* oidx);
    void  (*ufo_set_pos)(const RECVX_ENEMY_VEC* pos);
    void  (*ufo_set_md)(int md);
    void  (*ufo_set_oidx)(uint32_t oidx);

    /* --- EXP-Block-Allokator (eneset.c:906 bhEne_CallocWork) -------------- */
    /* Nullinitialisierter Block mit der Lebensdauer des Raums. NULL, wenn das
       originale 0xC40000-Budget erschoepft ist — der Aufrufer muss das wie im
       Original behandeln und darf keinen statischen Ersatzblock nehmen. */
    void* (*calloc_work)(uint32_t size);
} RECVX_ENEMY_SERVICES;

/* ===========================================================================
   Schnittstelle der Stufe A
   =========================================================================== */

/* eneset.c:164 bhInitEnemy — leert die ganze Tabelle. */
void recvx_enemy_table_init(RECVX_ENEMY_TABLE* t);

/* eneset.c:170 bhSetEnemy. `tmpl` ist der ETTY_WORK/EGG_WORK-Satz (36 Byte im
   Original, hier als benannte Vorlage). Rueckgabe NULL bei voller Tabelle;
   ewk_n wird in beiden Faellen nachgezogen. */
typedef struct {
    uint32_t flg;            /* Bit 0 landet in epp->flg (eneset.c:183)     */
    uint16_t id, type;
    int8_t   flr_no;
    uint8_t  mdlver;
    float    px, py, pz;
    int32_t  ay;
} RECVX_ENEMY_TEMPLATE;

RECVX_ENEMY_WORK* recvx_enemy_set(RECVX_ENEMY_TABLE* t, RECVX_ENEMY_WORK* storage,
                                  const RECVX_ENEMY_TEMPLATE* tmpl,
                                  void* owner, uint32_t owner_index,
                                  const RECVX_ENEMY_SERVICES* sv);

/* eneset.c:325 bhCheckEneWorkNum — hoechster belegter Index + 1, Luecken inkl. */
void recvx_enemy_check_work_num(RECVX_ENEMY_TABLE* t);

/* pwksub.c:85 bhSetFloorNum. */
void recvx_enemy_set_floor_num(RECVX_ENEMY_WORK* w);

/* zonzon1.c:806 bhEne_InitDamage. */
void recvx_enemy_init_damage(RECVX_ENEMY_WORK* w);

/* eneset.c:362 Filter eines Gegnerpasses (ohne Seiteneffekt). */
int recvx_enemy_pass_enabled(const RECVX_ENEMY_WORK* w);
/* eneset.c:807 Zeichenfilter; `st_flg` ist sys->st_flg. */
int recvx_enemy_draw_enabled(const RECVX_ENEMY_WORK* w, uint32_t st_flg);
/* pwksub.c:445 Zielbarkeitsfilter von bhSearchNearEnemy2. */
int recvx_enemy_targetable(const RECVX_ENEMY_WORK* w);

/* Einseitige Veroeffentlichung Work -> WorldEntity (Abschnitt B oben). Der
   Besitzer ruft sie genau einmal je Pass, dort wo das Original bhCalcModel
   ruft. Reine Werteabgabe; kein Zugriff auf Host-Typen in dieser Datei. */
typedef struct {
    float    px, py, pz;
    int32_t  ax, ay, az;
    int8_t   flr_no;
    uint16_t type;
    int32_t  hp;
    uint32_t mtn_no;
    uint8_t  mtn_room;
    int32_t  frm_no, mtn_add;
    uint32_t flg, flg2, stflg, mdflg;
    uint8_t  mode0, mode1, mode2, mode3;
} RECVX_ENEMY_PUBLISH;
void recvx_enemy_publish(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_PUBLISH* out);

/* Abschnitt C: Skript-/Ladeschreibrichtung Entity -> Work. Nur diese eine
   Funktion darf das; sie uebernimmt genau die Felder, die der Aufrufer
   ausdruecklich als gueltig markiert (`mask`). */
#define RECVX_ENEMY_ADOPT_POSE    0x001u  /* px/py/pz, ax/ay/az, flr_no     */
#define RECVX_ENEMY_ADOPT_MOTION  0x002u  /* mtn_no/mtn_room/frm_no/mtn_add */
#define RECVX_ENEMY_ADOPT_MODES   0x004u  /* mode0..3                       */
#define RECVX_ENEMY_ADOPT_HP      0x008u  /* hp                             */
#define RECVX_ENEMY_ADOPT_FLAGS   0x010u  /* flg/flg2/stflg/mdflg           */
void recvx_enemy_adopt(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PUBLISH* in, uint32_t mask);

#ifdef __cplusplus
}
#endif
#endif
