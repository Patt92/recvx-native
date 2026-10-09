// Native port semantics for the EN07 SFX / effect / player-interaction core
// (zonzon1.c / en07.c).  The bodies are the original C transcribed verbatim;
// only the platform leaf services (audio mixer + particle emitter) are mocked
// through the host binding so this test can observe the exact argument
// forwarding and state writes without the runtime mixer.
#include "types.h"
extern "C" {
#include "main.h"
#include "core_enemy_interaction.h"
}
#include <cstdint>
#include <cstdio>
#include <cstring>

// Mock host binding: records every leaf call and its arguments.
static int recv_enemy = -1;
static int recv_se = -1;
static int recv_flag = -1;
static int recv_fade = -1;
static NJS_POINT3 recv_pos = { 0.0f, 0.0f, 0.0f };
static int recv_voice = -1;
static int particle_calls = 0;
static unsigned int particle_src = 0;
static unsigned int particle_dst = 0;
static int particle_typ = -1;
static NJS_POINT3 particle_pos = { 0.0f, 0.0f, 0.0f };
static NJS_POINT3 particle_dir = { 0.0f, 0.0f, 0.0f };

static void mock_request_enemy_se_basic(int enemy, const NJS_POINT3* pos, int se,
                                        int flag, int fade)
{
    recv_enemy = enemy;
    recv_se = se;
    recv_flag = flag;
    recv_fade = fade;
    recv_pos = *pos;
}

static int mock_check_play_enemy_se(int enemy, int se)
{
    recv_enemy = enemy;
    recv_se = se;
    return (se == 0x101) ? 1 : 0;
}

static void mock_call_player_voice(int no)
{
    recv_voice = no;
}

static O_WRK* mock_set_eff_particle_mk2(BH_PWORK* /*ewP*/, int /*lnk_no*/,
                                        NJS_POINT3* offP, NJS_POINT3* dirP,
                                        unsigned int src_col, unsigned int dst_col,
                                        int typ_no)
{
    particle_calls++;
    particle_src = src_col;
    particle_dst = dst_col;
    particle_typ = typ_no;
    particle_pos = *offP;
    particle_dir = *dirP;
    return NULL;
}

static int fail(const char* m)
{
    std::fprintf(stderr, "%s\n", m);
    return 1;
}

int main()
{
    static SYS_WORK system_work;
    static BH_PWORK enemy_work;
    static BH_PWORK player_work;
    std::memset(&system_work, 0, sizeof(system_work));
    std::memset(&enemy_work, 0, sizeof(enemy_work));
    std::memset(&player_work, 0, sizeof(player_work));

    sys = &system_work;
    plp = &player_work;

    RECVX_ENEMY_INTERACTION_HOST h;
    h.request_enemy_se_basic = mock_request_enemy_se_basic;
    h.check_play_enemy_se = mock_check_play_enemy_se;
    h.call_player_voice = mock_call_player_voice;
    h.set_eff_particle_mk2 = mock_set_eff_particle_mk2;
    core_bh_ene_interaction_host_set(&h);

    /* ------------------------------------------------------------------ */
    /* bhEne_CallSE / _EX / SetSEPan: flag gate + sys->enow routing        */
    /* ------------------------------------------------------------------ */
    sys->enow = 7;

    enemy_work.flg = 0;
    NJS_POINT3 sepos = { 1.0f, 2.0f, 3.0f };
    core_bh_ene_call_se(&enemy_work, &sepos, 0x2302);
    if (recv_enemy != 7 || recv_se != 0x2302 || recv_flag != 1 || recv_fade != 0)
        return fail("bhEne_CallSE did not forward enemy/se/flag/fade");
    if (recv_pos.x != 1.0f || recv_pos.y != 2.0f || recv_pos.z != 3.0f)
        return fail("bhEne_CallSE position not forwarded");

    // flag 0x10000 suppresses the call
    recv_se = -1;
    enemy_work.flg = 0x10000;
    core_bh_ene_call_se(&enemy_work, &sepos, 0x2302);
    if (recv_se != -1)
        return fail("bhEne_CallSE did not honour flg & 0x10000 gate");

    enemy_work.flg = 0;
    core_bh_ene_call_se_ex(&enemy_work, &sepos, 0x99, 3);
    if (recv_se != 0x99 || recv_fade != 3 || recv_flag != 1)
        return fail("bhEne_CallSE_EX fade not forwarded");

    core_bh_ene_set_se_pan(&enemy_work, &sepos, 0x77);
    if (recv_se != 0x77 || recv_flag != 0 || recv_fade != 0)
        return fail("bhEne_SetSEPan flag not 0");

    /* ------------------------------------------------------------------ */
    /* bhEne_CallEffectSE: enemy 127, flag 1                               */
    /* ------------------------------------------------------------------ */
    core_bh_ene_call_effect_se(&sepos, 0x55);
    if (recv_enemy != 127 || recv_se != 0x55 || recv_flag != 1)
        return fail("bhEne_CallEffectSE did not route enemy 127");

    /* ------------------------------------------------------------------ */
    /* bhEne_CheckPlayEffectSE                                             */
    /* ------------------------------------------------------------------ */
    if (core_bh_ene_check_play_effect_se(0x101) != 1)
        return fail("bhEne_CheckPlayEffectSE did not return mock result");
    if (recv_enemy != 127)
        return fail("bhEne_CheckPlayEffectSE did not route enemy 127");
    if (core_bh_ene_check_play_effect_se(0x202) != 0)
        return fail("bhEne_CheckPlayEffectSE nonzero for unmatched se");

    /* ------------------------------------------------------------------ */
    /* bhEne_CallPlayerVoice: no |= 0x400                                  */
    /* ------------------------------------------------------------------ */
    core_bh_ene_call_player_voice(0x12);
    if (recv_voice != 0x412)
        return fail("bhEne_CallPlayerVoice did not apply |0x400");

    /* ------------------------------------------------------------------ */
    /* bhEne_SetWeponAtr: capsule copy from joint matrices                 */
    /* ------------------------------------------------------------------ */
    static ML_WORK mlw;
    static O_WORK ow[3];
    std::memset(&mlw, 0, sizeof(mlw));
    std::memset(ow, 0, sizeof(ow));
    mlw.owP = ow;
    enemy_work.mlwP = &mlw;

    ow[0].mtx[12] = 10.0f; ow[0].mtx[13] = 11.0f; ow[0].mtx[14] = 12.0f;
    ow[1].mtx[12] = 20.0f; ow[1].mtx[13] = 21.0f; ow[1].mtx[14] = 22.0f;

    core_bh_ene_set_wepon_atr(&enemy_work, 0, 1, 5.0f);
    if (enemy_work.watr.c1.x != 10.0f || enemy_work.watr.c1.y != 11.0f ||
        enemy_work.watr.c1.z != 12.0f)
        return fail("bhEne_SetWeponAtr c1 copy wrong");
    if (enemy_work.watr.c2.x != 20.0f || enemy_work.watr.c2.y != 21.0f ||
        enemy_work.watr.c2.z != 22.0f)
        return fail("bhEne_SetWeponAtr c2 copy wrong");
    if (enemy_work.watr.r != 5.0f)
        return fail("bhEne_SetWeponAtr radius not set");

    /* ------------------------------------------------------------------ */
    /* bhEne_AttackHitCheck: nearest joint selection                       */
    /* ------------------------------------------------------------------ */
    static ML_WORK amlw;
    static O_WORK aow[3];
    std::memset(&amlw, 0, sizeof(amlw));
    std::memset(aow, 0, sizeof(aow));
    amlw.owP = aow;
    amlw.obj_num = 3;
    enemy_work.mlwP = &amlw;

    enemy_work.watr.c1.x = 0.0f; enemy_work.watr.c1.y = 0.0f; enemy_work.watr.c1.z = 0.0f;
    enemy_work.watr.c2.x = 0.0f; enemy_work.watr.c2.y = 0.0f; enemy_work.watr.c2.z = 1.0f;
    enemy_work.watr.r = 1.0f;

    aow[0].mtx[12] = 1.0f; aow[0].mtx[13] = 0.0f; aow[0].mtx[14] = 0.0f;
    aow[1].mtx[12] = 0.0f; aow[1].mtx[13] = 1.0f; aow[1].mtx[14] = 0.0f;
    aow[2].mtx[12] = 0.5f; aow[2].mtx[13] = 0.0f; aow[2].mtx[14] = 0.0f;

    NJS_POINT3 atkpos = { 0.0f, 0.0f, 0.0f };
    unsigned char hit = core_bh_ene_attack_hit_check(&enemy_work, &atkpos, 2.0f);
    if (hit != 1)
        return fail("bhEne_AttackHitCheck did not report a hit");
    if (enemy_work.djnt_no != 2)
        return fail("bhEne_AttackHitCheck did not select nearest joint");
    if (enemy_work.dpx != 0.0f || enemy_work.dpy != 0.0f || enemy_work.dpz != 0.0f)
        return fail("bhEne_AttackHitCheck dpx/dpy/dpz wrong");
    if (enemy_work.dvx != 0.5f || enemy_work.dvy != 0.0f || enemy_work.dvz != 0.0f)
        return fail("bhEne_AttackHitCheck dvx/dvy/dvz wrong");

    /* ------------------------------------------------------------------ */
    /* bhEne_SetBloodEffect4 / PlayerBloodEffect: sys->ef + particle       */
    /* ------------------------------------------------------------------ */
    particle_calls = 0;
    NJS_POINT3 bpos = { 4.0f, 5.0f, 6.0f };
    NJS_VECTOR bvec = { 1.0f, 2.0f, 3.0f };
    core_bh_ene_set_blood_effect4(&bpos, &bvec, 2, 10, 200);

    if (particle_calls != 1)
        return fail("bhEne_SetBloodEffect4 did not emit one particle");
    if (particle_src != 0xFF3F3F16 || particle_dst != 0xFF102F18)
        return fail("bhEne_SetBloodEffect4 wrong BloodCol[2]");
    if (particle_typ != 10)
        return fail("bhEne_SetBloodEffect4 wrong typ_no");
    if (particle_pos.x != 4.0f || particle_pos.y != 5.0f || particle_pos.z != 6.0f)
        return fail("bhEne_SetBloodEffect4 wrong particle position");

    if (eff[0].id != 5 || eff[0].flg != 1)
        return fail("bhEne_SetBloodEffect4 ef id/flg not committed via effect table");
    if (eff[0].type != (200 & 0xFF))
        return fail("bhEne_SetBloodEffect4 ef type mismatch");
    if (eff[0].mdlver != 2)
        return fail("bhEne_SetBloodEffect4 ef mdlver mismatch");
    if (eff[0].px != 4.0f || eff[0].py != 5.0f || eff[0].pz != 6.0f)
        return fail("bhEne_SetBloodEffect4 ef position mismatch");
    if (eff[0].sx != 2.0f || eff[0].sy != 2.0f || eff[0].sz != 2.0f)
        return fail("bhEne_SetBloodEffect4 ef scale mismatch");

    /* ------------------------------------------------------------------ */
    /* bhEne07_PlayerControl: state machine + packed mode write            */
    /* ------------------------------------------------------------------ */
    static MN_WORK pmn[40];
    std::memset(pmn, 0, sizeof(pmn));
    static MN_WORK pmn_back;
    std::memset(&pmn_back, 0, sizeof(pmn_back));

    static unsigned char eexp0[64];
    static unsigned char pexp1[64];
    std::memset(eexp0, 0, sizeof(eexp0));
    std::memset(pexp1, 0, sizeof(pexp1));

    enemy_work.mnwP = pmn;
    enemy_work.exp0 = eexp0;
    player_work.exp1 = pexp1;
    player_work.mnwPb = &pmn_back;

    sys->ply_id = 0;

    pmn[0x1F].frm_num = 10; // ply_motion[0][1]

    player_work.mode2 = 0;
    player_work.mode3 = 0;
    player_work.flg = 0;
    player_work.stflg = 0;
    core_bh_ene07_player_control(&enemy_work);

    // case 0 / mode3 0 -> 1, ct0 = frm_num-1 = 9, mode3 == 1 after ++
    if (player_work.mode3 != 1)
        return fail("bhEne07_PlayerControl mode3 did not advance");
    if ((player_work.flg & 0x40000) == 0 || (player_work.flg & 0x80000) == 0 ||
        (player_work.flg & 0x10004) != 0x10004)
        return fail("bhEne07_PlayerControl flg bits not set");
    if ((player_work.stflg & 0x10000) == 0)
        return fail("bhEne07_PlayerControl stflg 0x10000 not set");
    if (player_work.mtn_no != 0x1F)
        return fail("bhEne07_PlayerControl mtn_no wrong");
    // Init sets ct0 = frm_num-1 = 9, then the immediate fallthrough to case 1
    // performs ct0--, so the observed value after one call is 8.
    if (player_work.ct0 != 8)
        return fail("bhEne07_PlayerControl ct0 != frm_num-2 after fallthrough");
    if (player_work.hokan_count != 5 || player_work.hokan_rate != 0xCCCC ||
        player_work.mtn_add != 0x10000)
        return fail("bhEne07_PlayerControl motion params wrong");
    {
        int e1 = *(int*)((char*)player_work.exp1 + 0);
        if ((e1 & 0x1E0) != 0x1E0 || (e1 & 0x4) != 0)
            return fail("bhEne07_PlayerControl exp1 0x1E0/0x4 bits wrong");
    }

    // Run case 1 decrement to completion (ct0 9..0 -> 10 iterations).
    int spins = 0;
    while (player_work.mode2 == 0 && player_work.mode3 == 1)
    {
        if (++spins > 20)
            return fail("bhEne07_PlayerControl case-1 loop did not terminate");
        core_bh_ene07_player_control(&enemy_work);
    }
    // Completion sets mode0=1 (packed write clears mode1/2/3), frees motion.
    if (player_work.mode0 != 1 || player_work.mode1 != 0 ||
        player_work.mode2 != 0 || player_work.mode3 != 0)
        return fail("bhEne07_PlayerControl packed mode write wrong");
    if ((player_work.flg & 0x10004) != 0 || (player_work.flg & 0x8) == 0)
        return fail("bhEne07_PlayerControl completion flg wrong");
    if (player_work.at_flg != 0)
        return fail("bhEne07_PlayerControl at_flg not cleared");
    if (player_work.mnwP != player_work.mnwPb)
        return fail("bhEne07_PlayerControl mnwP not restored");
    {
        int e1 = *(int*)((char*)player_work.exp1 + 0);
        if ((e1 & 0x4) == 0)
            return fail("bhEne07_PlayerControl completion exp1 0x4 not set");
    }

    /* ------------------------------------------------------------------ */
    /* bhEne07_PlayerBloodEffect: njCalcPoint + original v-vector bug      */
    /* ------------------------------------------------------------------ */
    std::memset(eff, 0, sizeof(eff));
    static ML_WORK pmlw;
    static O_WORK pow[14];
    std::memset(&pmlw, 0, sizeof(pmlw));
    std::memset(pow, 0, sizeof(pow));
    pmlw.owP = pow;
    for (int k = 0; k < 16; k++)
        pow[13].mtx[k] = 0.0f;
    pow[13].mtx[0] = 1.0f; pow[13].mtx[5] = 1.0f;
    pow[13].mtx[10] = 1.0f; pow[13].mtx[15] = 1.0f; // AtkPos[0] == 0x0D
    player_work.mlwP = &pmlw;
    enemy_work.exp0 = eexp0;
    eexp0[0x35] = 0;
    enemy_work.px = 0.0f; enemy_work.py = 0.0f; enemy_work.pz = 0.0f;

    particle_calls = 0;
    core_bh_ene07_player_blood_effect(&enemy_work, 200);
    if (particle_calls != 1)
        return fail("bhEne07_PlayerBloodEffect did not emit one particle");
    if (particle_pos.x != -0.5f || particle_pos.y != 0.5f || particle_pos.z != 0.0f)
        return fail("bhEne07_PlayerBloodEffect pos != AtkOffset[0]");
    // Original decompiler bug: v.y and v.z both use pos.x, not pos.y/pos.z.
    if (particle_dir.x != -0.5f || particle_dir.y != -0.5f || particle_dir.z != -0.5f)
        return fail("bhEne07_PlayerBloodEffect v-vector bug not preserved");
    if (eff[0].id != 5 || eff[0].type != 200 || eff[0].mdlver != 0)
        return fail("bhEne07_PlayerBloodEffect ef state wrong");

    std::printf("EN07 SFX/effect/player-interaction core passed.\n");
    return 0;
}
