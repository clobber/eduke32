//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

#include "duke3d.h"
#include "common_game.h"
#include "osd.h"
#include "player.h"
#include "demo.h"
#include "enet/enet.h"

int32_t lastvisinc;
int32_t g_currentweapon;
int32_t g_gun_pos;
int32_t g_looking_arc;
int32_t g_weapon_offset;
int32_t g_gs;
int32_t g_kb;
int32_t g_looking_angSR1;
int32_t g_weapon_xoffset;
static int32_t g_snum;

extern int32_t g_levelTextTime, ticrandomseed;

int32_t g_numObituaries = 0;
int32_t g_numSelfObituaries = 0;

void P_PalFrom(DukePlayer_t *p, uint8_t f, uint8_t r, uint8_t g, uint8_t b)
{
    p->pals.f = f;
    p->pals.r = r;
    p->pals.g = g;
    p->pals.b = b;
}

void P_UpdateScreenPal(DukePlayer_t *p)
{
    int32_t intowater = 0;
    const int32_t sect = p->cursectnum;

    if (p->heat_on) p->palette = SLIMEPAL;
    else if (sect < 0) p->palette = BASEPAL;
    else if (sector[sect].ceilingpicnum >= FLOORSLIME && sector[sect].ceilingpicnum <= FLOORSLIME+2)
    {
        p->palette = SLIMEPAL;
        intowater = 1;
    }
    else
    {
        if (sector[p->cursectnum].lotag == ST_2_UNDERWATER) p->palette = WATERPAL;
        else p->palette = BASEPAL;
        intowater = 1;
    }

    g_restorePalette = 1+intowater;
}

static void P_IncurDamage(DukePlayer_t *p)
{
    int32_t damage;

    if (VM_OnEvent(EVENT_INCURDAMAGE, p->i, sprite[p->i].yvel, -1, 0) != 0)
        return;

    sprite[p->i].extra -= p->extra_extra8>>8;

    damage = sprite[p->i].extra - p->last_extra;

    if (damage >= 0)
        return;

    p->extra_extra8 = 0;

    if (p->inv_amount[GET_SHIELD] > 0)
    {
        int32_t shield_damage =  damage * (20 + (krand()%30)) / 100;
        damage -= shield_damage;

        p->inv_amount[GET_SHIELD] += shield_damage;

        if (p->inv_amount[GET_SHIELD] < 0)
        {
            damage += p->inv_amount[GET_SHIELD];
            p->inv_amount[GET_SHIELD] = 0;
        }
    }

    sprite[p->i].extra = p->last_extra + damage;
}

void P_QuickKill(DukePlayer_t *p)
{
    P_PalFrom(p, 48, 48,48,48);

    sprite[p->i].extra = 0;
    sprite[p->i].cstat |= 32768;

    if (ud.god == 0)
        A_DoGuts(p->i,JIBS6,8);
}

static void A_DoWaterTracers(int32_t x1,int32_t y1,int32_t z1,int32_t x2,int32_t y2,int32_t z2,int32_t n)
{
    int32_t i, xv, yv, zv;
    int16_t sect = -1;

    i = n+1;
    xv = (x2-x1)/i;
    yv = (y2-y1)/i;
    zv = (z2-z1)/i;

    if ((klabs(x1-x2)+klabs(y1-y2)) < 3084)
        return;

    for (i=n; i>0; i--)
    {
        x1 += xv;
        y1 += yv;
        z1 += zv;
        updatesector(x1,y1,&sect);
        if (sect < 0)
            break;

        if (sector[sect].lotag == ST_2_UNDERWATER)
            A_InsertSprite(sect,x1,y1,z1,WATERBUBBLE,-32,4+(krand()&3),4+(krand()&3),krand()&2047,0,0,g_player[0].ps->i,5);
        else
            A_InsertSprite(sect,x1,y1,z1,SMALLSMOKE,-32,14,14,0,0,0,g_player[0].ps->i,5);
    }
}

static void A_HitscanProjTrail(const vec3_t *sv, const vec3_t *dv, int32_t ang, int32_t atwith)
{
    int32_t n, j, i;
    int16_t sect = -1;
    vec3_t srcvect;
    vec3_t destvect;

    const projectile_t *const proj = &ProjectileData[atwith];

    Bmemcpy(&destvect, dv, sizeof(vec3_t));

    srcvect.x = sv->x + (sintable[(348+ang+512)&2047]/proj->offset);
    srcvect.y = sv->y + (sintable[(ang+348)&2047]/proj->offset);
    srcvect.z = sv->z + 1024+(proj->toffset<<8);

    n = ((FindDistance2D(srcvect.x-destvect.x,srcvect.y-destvect.y))>>8)+1;

    destvect.x = ((destvect.x-srcvect.x)/n);
    destvect.y = ((destvect.y-srcvect.y)/n);
    destvect.z = ((destvect.z-srcvect.z)/n);

    srcvect.x += destvect.x>>2;
    srcvect.y += destvect.y>>2;
    srcvect.z += (destvect.z>>2);

    for (i=proj->tnum; i>0; i--)
    {
        srcvect.x += destvect.x;
        srcvect.y += destvect.y;
        srcvect.z += destvect.z;
        updatesector(srcvect.x,srcvect.y,&sect);
        if (sect < 0)
            break;
        getzsofslope(sect,srcvect.x,srcvect.y,&n,&j);
        if (srcvect.z > j || srcvect.z < n)
            break;
        j = A_InsertSprite(sect,srcvect.x,srcvect.y,srcvect.z,proj->trail,-32,
                           proj->txrepeat,proj->tyrepeat,ang,0,0,g_player[0].ps->i,0);
        changespritestat(j, STAT_ACTOR);
    }
}

int32_t A_GetHitscanRange(int32_t i)
{
    int32_t zoff = (PN == APLAYER) ? PHEIGHT : 0;
    hitdata_t hit;

    SZ -= zoff;
    hitscan((const vec3_t *)&sprite[i],SECT,
            sintable[(SA+512)&2047],
            sintable[SA&2047],
            0,&hit,CLIPMASK1);
    SZ += zoff;

    return (FindDistance2D(hit.pos.x-SX,hit.pos.y-SY));
}

static int32_t A_FindTargetSprite(spritetype *s,int32_t aang,int32_t atwith)
{
    int32_t gotshrinker,gotfreezer;
    int32_t i, j, a, k, cans;
    static int32_t aimstats[] = { 10, 13, 1, 2 };
    int32_t dx1, dy1, dx2, dy2, dx3, dy3, smax, sdist;
    int32_t xv, yv;

    if (s->picnum == APLAYER)
    {
        if (!g_player[s->yvel].ps->auto_aim)
            return -1;

        if (g_player[s->yvel].ps->auto_aim == 2)
        {
            if (A_CheckSpriteTileFlags(atwith,SPRITE_PROJECTILE) && (ProjectileData[atwith].workslike & PROJECTILE_RPG))
                return -1;
            else switch (DYNAMICTILEMAP(atwith))
                {
                case TONGUE__STATIC:
                case FREEZEBLAST__STATIC:
                case SHRINKSPARK__STATIC:
                case SHRINKER__STATIC:
                case RPG__STATIC:
                case FIRELASER__STATIC:
                case SPIT__STATIC:
                case COOLEXPLOSION1__STATIC:
                    return -1;
                default:
                    break;
                }
        }
    }

    a = s->ang;

    j = -1;

    gotshrinker = (s->picnum == APLAYER && PWEAPON(0, g_player[s->yvel].ps->curr_weapon, WorksLike) == SHRINKER_WEAPON);
    gotfreezer = (s->picnum == APLAYER && PWEAPON(0, g_player[s->yvel].ps->curr_weapon, WorksLike) == FREEZE_WEAPON);

    smax = INT32_MAX;

    dx1 = sintable[(a+512-aang)&2047];
    dy1 = sintable[(a-aang)&2047];
    dx2 = sintable[(a+512+aang)&2047];
    dy2 = sintable[(a+aang)&2047];

    dx3 = sintable[(a+512)&2047];
    dy3 = sintable[a&2047];

    for (k=0; k<4; k++)
    {
        if (j >= 0)
            break;
        for (i=headspritestat[aimstats[k]]; i >= 0; i=nextspritestat[i])
            if (sprite[i].xrepeat > 0 && sprite[i].extra >= 0 && (sprite[i].cstat&(257+32768)) == 257)
                if (A_CheckEnemySprite(&sprite[i]) || k < 2)
                {
                    if (A_CheckEnemySprite(&sprite[i]) || PN == APLAYER || PN == SHARK)
                    {
                        if (PN == APLAYER &&
                                //                        ud.ffire == 0 &&
                                (GTFLAGS(GAMETYPE_PLAYERSFRIENDLY) || (GTFLAGS(GAMETYPE_TDM) &&
                                        g_player[sprite[i].yvel].ps->team == g_player[s->yvel].ps->team)) &&
                                s->picnum == APLAYER &&
                                s != &sprite[i])
                            continue;

                        if (gotshrinker && sprite[i].xrepeat < 30)
                        {
                            if (PN == SHARK)
                            {
                                if (sprite[i].xrepeat < 20) continue;
                                continue;
                            }
                            else if (!(PN >= GREENSLIME && PN <= GREENSLIME+7))
                                continue;
                        }
                        if (gotfreezer && sprite[i].pal == 1) continue;
                    }

                    xv = (SX-s->x);
                    yv = (SY-s->y);

                    if ((dy1*xv <= dx1*yv) && (dy2*xv >= dx2*yv))
                    {
                        sdist = mulscale(dx3,xv,14) + mulscale(dy3,yv,14);

                        if (sdist > 512 && sdist < smax)
                        {
                            if (s->picnum == APLAYER)
                                a = (klabs(scale(SZ-s->z,10,sdist)-(g_player[s->yvel].ps->horiz+g_player[s->yvel].ps->horizoff-100)) < 100);
                            else a = 1;

                            if (PN == ORGANTIC || PN == ROTATEGUN)
                                cans = cansee(SX,SY,SZ,SECT,s->x,s->y,s->z-(32<<8),s->sectnum);
                            else cans = cansee(SX,SY,SZ-(32<<8),SECT,s->x,s->y,s->z-(32<<8),s->sectnum);

                            if (a && cans)
                            {
                                smax = sdist;
                                j = i;
                            }
                        }
                    }
                }
    }

    return j;
}

static void A_SetHitData(int32_t i, const hitdata_t *hit)
{
    actor[i].t_data[6] = hit->wall;
    actor[i].t_data[7] = hit->sect;
    actor[i].t_data[8] = hit->sprite;
}

static int32_t CheckShootSwitchTile(int32_t pn)
{
    return pn == DIPSWITCH || pn == DIPSWITCH+1 ||
        pn == DIPSWITCH2 || pn == DIPSWITCH2+1 ||
        pn == DIPSWITCH3 || pn == DIPSWITCH3+1 ||
        pn == HANDSWITCH || pn == HANDSWITCH+1;
}

// flags:
//  1: do sprite center adjustment (cen-=(8<<8)) for GREENSLIME or ROTATEGUN
//  2: do auto getangle only if not RECON (if clear, do unconditionally)
static int32_t GetAutoAimAngle(int32_t i, int32_t p, int32_t atwith,
                               int32_t cen_add, int32_t flags,
                               const vec3_t *srcvect, int32_t vel,
                               int32_t *zvel, int16_t *sa)
{
    int32_t j = -1;

    Bassert((unsigned)p < MAXPLAYERS);

    Gv_SetVar(g_iAimAngleVarID, AUTO_AIM_ANGLE, i, p);

    if (G_HaveEvent(EVENT_GETAUTOAIMANGLE))
        VM_OnEvent(EVENT_GETAUTOAIMANGLE, i, p, -1, 0);

    {
        int32_t aimang = Gv_GetVar(g_iAimAngleVarID, i, p);
        if (aimang > 0)
            j = A_FindTargetSprite(&sprite[i], aimang, atwith);
    }

    if (j >= 0)
    {
        const spritetype *const spr = &sprite[j];
        int32_t cen = 2*(spr->yrepeat*tilesizy[spr->picnum]) + cen_add;
        int32_t dst;

        if (flags)
        {
            int32_t pn = spr->picnum;
            if ((pn >= GREENSLIME && pn <= GREENSLIME+7) || spr->picnum==ROTATEGUN)
            {
                cen -= (8<<8);
            }
        }

        dst = ldist(&sprite[g_player[p].ps->i], &sprite[j]);
        if (dst == 0)
            dst++;

        *zvel = ((spr->z - srcvect->z - cen)*vel) / dst;

        if (!(flags&2) || sprite[j].picnum != RECON)
            *sa = getangle(spr->x-srcvect->x, spr->y-srcvect->y);
    }

    return j;
}

static void Proj_MaybeSpawn(int32_t k, int32_t atwith, const hitdata_t *hit)
{
    // atwith < 0 is for hard-coded projectiles
    int32_t spawntile = atwith < 0 ? -atwith : ProjectileData[atwith].spawns;

    if (spawntile >= 0)
    {
        int32_t wh = A_Spawn(k, spawntile);

        if (atwith >= 0)
        {
            if (ProjectileData[atwith].sxrepeat > 4)
                sprite[wh].xrepeat = ProjectileData[atwith].sxrepeat;
            if (ProjectileData[atwith].syrepeat > 4)
                sprite[wh].yrepeat = ProjectileData[atwith].syrepeat;
        }

        A_SetHitData(wh, hit);
    }
}

// <extra>: damage that this shotspark does
static int32_t Proj_InsertShotspark(const hitdata_t *hit, int32_t i, int32_t atwith,
                                    int32_t xyrepeat, int32_t ang, int32_t extra)
{
    int32_t k = A_InsertSprite(hit->sect, hit->pos.x, hit->pos.y, hit->pos.z,
                               SHOTSPARK1,-15, xyrepeat,xyrepeat, ang,0,0,i,4);
    sprite[k].extra = extra;
    // This is a hack to allow you to detect which weapon spawned a SHOTSPARK1:
    sprite[k].yvel = atwith;
    A_SetHitData(k, hit);

    return k;
}

static int32_t Proj_GetExtra(int32_t atwith)
{
    int32_t extra = ProjectileData[atwith].extra;
    if (ProjectileData[atwith].extra_rand > 0)
        extra += (krand()%ProjectileData[atwith].extra_rand);
    return extra;
}

static void Proj_MaybeAddSpread(int32_t not_accurate_p, int32_t *zvel, int16_t *sa,
                                int32_t zRange, int32_t angRange)
{
    if (not_accurate_p)
    {
        // NOTE: if {z,ang}Range is 0, there is a huge spread
        *zvel += (zRange/2)-(krand()&(zRange-1));
        *sa += (angRange/2)-(krand()&(angRange-1));
    }
}

// Prepare hitscan weapon fired from player p.
static void P_PreFireHitscan(int32_t i, int32_t p, int32_t atwith,
                             vec3_t *srcvect, int32_t *zvel, int16_t *sa,
                             int32_t accurate_autoaim_p,
                             int32_t not_accurate_p)
{
    int32_t angRange=32;
    int32_t zRange=256;

    int32_t j = GetAutoAimAngle(i, p, atwith, 5<<8, 0+1, srcvect, 256, zvel, sa);
    const DukePlayer_t *const ps = g_player[p].ps;

    Gv_SetVar(g_iAngRangeVarID,angRange, i,p);
    Gv_SetVar(g_iZRangeVarID,zRange,i,p);

    if (G_HaveEvent(EVENT_GETSHOTRANGE))
        VM_OnEvent(EVENT_GETSHOTRANGE, i,p, -1, 0);
#if !defined LUNATIC_ONLY
    // TODO
    angRange=Gv_GetVar(g_iAngRangeVarID,i,p);
    zRange=Gv_GetVar(g_iZRangeVarID,i,p);
#endif
    if (accurate_autoaim_p)
    {
        if (!ps->auto_aim)
        {
            hitdata_t hit;

            *zvel = (100-ps->horiz-ps->horizoff)<<5;
            if (actor[i].shootzvel)
                *zvel = actor[i].shootzvel;

            hitscan(srcvect, sprite[i].sectnum, sintable[(*sa+512)&2047], sintable[*sa&2047],
                    *zvel<<6,&hit,CLIPMASK1);

            if (hit.sprite != -1)
            {
                const int32_t hitstatnumsbitmap =
                    ((1<<STAT_ACTOR) | (1<<STAT_ZOMBIEACTOR) | (1<<STAT_PLAYER) | (1<<STAT_DUMMYPLAYER));
                const int32_t st = sprite[hit.sprite].statnum;

                if (st>=0 && st<=30 && (hitstatnumsbitmap&(1<<st)))
                    j = hit.sprite;
            }
        }

        if (j == -1)
        {
            *zvel = (100-ps->horiz-ps->horizoff)<<5;
            Proj_MaybeAddSpread(not_accurate_p, zvel, sa, zRange, angRange);
        }
    }
    else
    {
        if (j == -1)  // no target
            *zvel = (100-ps->horiz-ps->horizoff)<<5;
        Proj_MaybeAddSpread(not_accurate_p, zvel, sa, zRange, angRange);
    }

    srcvect->z -= (2<<8);
}

// Hitscan weapon fired from actor (sprite s);
static void A_PreFireHitscan(const spritetype *s, vec3_t *srcvect, int32_t *zvel, int16_t *sa,
                             int32_t not_accurate_p)
{
    int32_t dummydist;
    const int32_t j = A_FindPlayer(s, &dummydist);
    const DukePlayer_t *targetps = g_player[j].ps;

    int32_t d = ldist(&sprite[targetps->i], s);

    if (d == 0)
        d++;
    *zvel = ((targetps->pos.z-srcvect->z)<<8) / d;

    srcvect->z -= (4<<8);

    if (s->picnum != BOSS1)
    {
        Proj_MaybeAddSpread(not_accurate_p, zvel, sa, 256, 64);
    }
    else
    {
        *sa = getangle(targetps->pos.x-srcvect->x, targetps->pos.y-srcvect->y);

        Proj_MaybeAddSpread(not_accurate_p, zvel, sa, 256, 128);
    }
}

static int32_t Proj_DoHitscan(int32_t i, int32_t cstatmask,
                              const vec3_t *srcvect, int32_t zvel, int16_t sa,
                              hitdata_t *hit)
{
    spritetype *const s = &sprite[i];

    s->cstat &= ~cstatmask;

    if (actor[i].shootzvel)
        zvel = actor[i].shootzvel;

    hitscan(srcvect, s->sectnum,
            sintable[(sa+512)&2047],
            sintable[sa&2047],
            zvel<<6, hit, CLIPMASK1);

    s->cstat |= cstatmask;

    return (hit->sect < 0);
}

// Finish shooting hitscan weapon from player <p>. <k> is the inserted SHOTSPARK1.
// * <spawnatimpacttile> is passed to Proj_MaybeSpawn()
// * <decaltile> and <damagewalltile> are for wall impact
// * <damagewalltile> is passed to A_DamageWall()
// * <flags> is for decals upon wall impact:
//    1: handle random decal size (tile <atwith>)
//    2: set cstat to wall-aligned + random x/y flip
//
// TODO: maybe split into 3 cases (hit neither wall nor sprite, hit sprite, hit wall)?
static int32_t P_PostFireHitscan(int32_t p, int32_t k, hitdata_t *hit, int32_t i, int32_t atwith, int32_t zvel,
                                 int32_t spawnatimpacttile, int32_t decaltile, int32_t damagewalltile,
                                 int32_t flags)
{
    if (hit->wall == -1 && hit->sprite == -1)
    {
        if (zvel < 0)
        {
            if (sector[hit->sect].ceilingstat&1)
            {
                sprite[k].xrepeat = 0;
                sprite[k].yrepeat = 0;
                return -1;
            }
            else
                Sect_DamageCeiling(hit->sect);
        }

        Proj_MaybeSpawn(k, spawnatimpacttile, hit);
    }
    else if (hit->sprite >= 0)
    {
        A_DamageObject(hit->sprite, k);

        if (sprite[hit->sprite].picnum == APLAYER &&
            (ud.ffire == 1 || (!GTFLAGS(GAMETYPE_PLAYERSFRIENDLY) && GTFLAGS(GAMETYPE_TDM) &&
                               g_player[sprite[hit->sprite].yvel].ps->team != g_player[sprite[i].yvel].ps->team)))
        {
            int32_t l = A_Spawn(k, JIBS6);
            sprite[k].xrepeat = sprite[k].yrepeat = 0;
            sprite[l].z += (4<<8);
            sprite[l].xvel = 16;
            sprite[l].xrepeat = sprite[l].yrepeat = 24;
            sprite[l].ang += 64-(krand()&127);
        }
        else
        {
            Proj_MaybeSpawn(k, spawnatimpacttile, hit);
        }

        if (p >= 0 && CheckShootSwitchTile(sprite[hit->sprite].picnum))
        {
            P_ActivateSwitch(p, hit->sprite, 1);
            return -1;
        }
    }
    else if (hit->wall >= 0)
    {
        const walltype *const hitwal = &wall[hit->wall];

        Proj_MaybeSpawn(k, spawnatimpacttile, hit);

        if (CheckDoorTile(hitwal->picnum) == 1)
            goto SKIPBULLETHOLE;

        if (p >= 0 && CheckShootSwitchTile(hitwal->picnum))
        {
            P_ActivateSwitch(p, hit->wall, 0);
            return -1;
        }

        if (hitwal->hitag != 0 || (hitwal->nextwall >= 0 && wall[hitwal->nextwall].hitag != 0))
            goto SKIPBULLETHOLE;

        if (hit->sect >= 0 && sector[hit->sect].lotag == 0)
            if (hitwal->overpicnum != BIGFORCE && (hitwal->cstat&16) == 0)
                if ((hitwal->nextsector >= 0 && sector[hitwal->nextsector].lotag == 0) ||
                    (hitwal->nextsector == -1 && sector[hit->sect].lotag == 0))
                {
                    int32_t l;

                    if (hitwal->nextsector >= 0)
                        for (SPRITES_OF_SECT(hitwal->nextsector, l))
                            if (sprite[l].statnum == STAT_EFFECTOR && sprite[l].lotag == SE_13_EXPLOSIVE)
                                goto SKIPBULLETHOLE;

                    for (SPRITES_OF(STAT_MISC, l))
                        if (sprite[l].picnum == decaltile)
                            if (dist(&sprite[l],&sprite[k]) < (12+(krand()&7)))
                                goto SKIPBULLETHOLE;

                    if (decaltile >= 0)
                    {
                        l = A_Spawn(k, decaltile);

                        if (!A_CheckSpriteFlags(l , SPRITE_DECAL))
                            actor[l].flags |= SPRITE_DECAL;

                        sprite[l].xvel = -1;
                        sprite[l].ang = getangle(hitwal->x-wall[hitwal->point2].x,
                                                 hitwal->y-wall[hitwal->point2].y)+512;

                        if (flags&1)
                        {
                            if (ProjectileData[atwith].workslike & PROJECTILE_RANDDECALSIZE)
                            {
                                int32_t wh = (krand()&ProjectileData[atwith].xrepeat);
                                if (wh < ProjectileData[atwith].yrepeat)
                                    wh = ProjectileData[atwith].yrepeat;
                                sprite[l].xrepeat = wh;
                                sprite[l].yrepeat = wh;
                            }
                            else
                            {
                                sprite[l].xrepeat = ProjectileData[atwith].xrepeat;
                                sprite[l].yrepeat = ProjectileData[atwith].yrepeat;
                            }
                        }

                        if (flags&2)
                            sprite[l].cstat = 16+(krand()&(8+4));

                        sprite[l].x -= sintable[(sprite[l].ang+2560)&2047]>>13;
                        sprite[l].y -= sintable[(sprite[l].ang+2048)&2047]>>13;

                        A_SetSprite(l, CLIPMASK0);

                        // BULLETHOLE already adds itself to the deletion queue in
                        // A_Spawn(). However, some other tiles do as well.
                        if (decaltile != BULLETHOLE)
                            A_AddToDeleteQueue(l);
                    }
                }

SKIPBULLETHOLE:
        // Handle bottom-swapped walls.
        if ((hitwal->cstat&2) && hitwal->nextsector >= 0)
            if (hit->pos.z >= sector[hitwal->nextsector].floorz)
                hit->wall = hitwal->nextwall;

        A_DamageWall(k, hit->wall, &hit->pos, damagewalltile);
    }

    return 0;
}

// Finish shooting hitscan weapon from actor (sprite <i>).
static int32_t A_PostFireHitscan(const hitdata_t *hit, int32_t i, int32_t atwith, int32_t sa, int32_t extra,
                                 int32_t spawnatimpacttile, int32_t damagewalltile)
{
    int32_t k = Proj_InsertShotspark(hit, i, atwith, 24, sa, extra);

    if (hit->sprite >= 0)
    {
        A_DamageObject(hit->sprite, k);

        if (sprite[hit->sprite].picnum != APLAYER)
            Proj_MaybeSpawn(k, spawnatimpacttile, hit);
        else
            sprite[k].xrepeat = sprite[k].yrepeat = 0;
    }
    else if (hit->wall >= 0)
        A_DamageWall(k, hit->wall, &hit->pos, damagewalltile);

    return k;
}

int32_t A_Shoot(int32_t i, int32_t atwith)
{
    int16_t l, sa, j, k=-1;
    int32_t vel, zvel = 0, x, oldzvel;
    hitdata_t hit;
    vec3_t srcvect;
    char sizx,sizy;
    spritetype *const s = &sprite[i];
    const int16_t sect = s->sectnum;

    const int32_t p = (s->picnum == APLAYER) ? s->yvel : -1;
    DukePlayer_t *const ps = p >= 0 ? g_player[p].ps : NULL;

    if (s->picnum == APLAYER)
    {
        Bmemcpy(&srcvect,ps,sizeof(vec3_t));
        srcvect.z += ps->pyoff+(4<<8);
        sa = ps->ang;

        ps->crack_time = 777;
    }
    else
    {
        sa = s->ang;
        Bmemcpy(&srcvect,s,sizeof(vec3_t));
        srcvect.z -= (((s->yrepeat*tilesizy[s->picnum])<<1)-(4<<8));

        if (s->picnum != ROTATEGUN)
        {
            srcvect.z -= (7<<8);

            if (A_CheckEnemySprite(s) && PN != COMMANDER)
            {
                srcvect.x += (sintable[(sa+1024+96)&2047]>>7);
                srcvect.y += (sintable[(sa+512+96)&2047]>>7);
            }
        }

#ifdef POLYMER
        if (atwith >= 0)
            switch (DYNAMICTILEMAP(atwith))
            {
            case FIRELASER__STATIC:
            case SHOTGUN__STATIC:
            case SHOTSPARK1__STATIC:
            case CHAINGUN__STATIC:
            case RPG__STATIC:
            case MORTER__STATIC:
            {
                int32_t x = ((sintable[(s->ang+512)&2047])>>7), y = ((sintable[(s->ang)&2047])>>7);
                s-> x += x;
                s-> y += y;
                G_AddGameLight(0, i, PHEIGHT, 8192, 255+(95<<8),PR_LIGHT_PRIO_MAX_GAME);
                actor[i].lightcount = 2;
                s-> x -= x;
                s-> y -= y;
            }

            break;
            }
#endif // POLYMER
    }

    if (A_CheckSpriteTileFlags(atwith, SPRITE_PROJECTILE))
    {
        /* Custom projectiles */
        projectile_t *const proj = (Bassert(atwith >= 0), &ProjectileData[atwith]);

#ifdef POLYMER
        if (proj->flashcolor)
        {
            int32_t x = ((sintable[(s->ang+512)&2047])>>7), y = ((sintable[(s->ang)&2047])>>7);

            s-> x += x;
            s-> y += y;
            G_AddGameLight(0, i, PHEIGHT, 8192, proj->flashcolor,PR_LIGHT_PRIO_MAX_GAME);
            actor[i].lightcount = 2;
            s-> x -= x;
            s-> y -= y;
        }
#endif // POLYMER

        if (proj->offset == 0)
            proj->offset = 1;

        if (proj->workslike & PROJECTILE_BLOOD || proj->workslike & PROJECTILE_KNEE)
        {

            if (proj->workslike & PROJECTILE_BLOOD)
            {
                sa += 64 - (krand()&127);
                if (p < 0) sa += 1024;
                zvel = 1024-(krand()&2047);
            }

            if (proj->workslike & PROJECTILE_KNEE)
            {
                if (p >= 0)
                {
                    zvel = (100-ps->horiz-ps->horizoff)<<5;
                    srcvect.z += (6<<8);
                    sa += 15;
                }
                else if (!(proj->workslike & PROJECTILE_NOAIM))
                {
                    j = g_player[A_FindPlayer(s,&x)].ps->i;
                    zvel = ((sprite[j].z-srcvect.z)<<8) / (x+1);
                    sa = getangle(sprite[j].x-srcvect.x,sprite[j].y-srcvect.y);
                }
            }

            if (actor[i].shootzvel) zvel = actor[i].shootzvel;
            hitscan((const vec3_t *)&srcvect,sect,
                    sintable[(sa+512)&2047],
                    sintable[sa&2047],zvel<<6,
                    &hit,CLIPMASK1);

            if (proj->workslike & PROJECTILE_BLOOD)
            {
                const walltype *const hitwal = &wall[hit.wall];

                if (proj->range == 0)
                    proj->range = 1024;

                if (FindDistance2D(srcvect.x-hit.pos.x,srcvect.y-hit.pos.y) < proj->range)
                    if (FindDistance2D(hitwal->x-wall[hitwal->point2].x,hitwal->y-wall[hitwal->point2].y) >
                            (mulscale(proj->xrepeat+8,tilesizx[proj->decal],3)))
                        if (hit.wall >= 0 && hitwal->overpicnum != BIGFORCE && (hitwal->cstat&16) == 0)
                            if ((hitwal->nextsector >= 0 && hit.sect >= 0 &&
                                    sector[hitwal->nextsector].lotag == 0 &&
                                    sector[hit.sect].lotag == 0 &&
                                    sector[hitwal->nextsector].lotag == 0 &&
                                    (sector[hit.sect].floorz-sector[hitwal->nextsector].floorz) >
                                    (mulscale(proj->yrepeat,tilesizy[proj->decal],3)<<8)) ||
                                    (hitwal->nextsector == -1 && sector[hit.sect].lotag == 0))
                            {
                                if (hitwal->nextsector >= 0)
                                {
                                    k = headspritesect[hitwal->nextsector];
                                    while (k >= 0)
                                    {
                                        if (sprite[k].statnum == STAT_EFFECTOR && sprite[k].lotag == SE_13_EXPLOSIVE)
                                            return -1;
                                        k = nextspritesect[k];
                                    }
                                }

                                if (hitwal->nextwall >= 0 &&
                                    wall[hitwal->nextwall].hitag != 0)
                                    return -1;

                                if (hitwal->hitag == 0)
                                {
                                    if (proj->decal >= 0)
                                    {
                                        k = A_Spawn(i,proj->decal);

                                        if (!A_CheckSpriteFlags(k , SPRITE_DECAL))
                                            actor[k].flags |= SPRITE_DECAL;

                                        sprite[k].xvel = -1;
                                        sprite[k].ang = getangle(hitwal->x-wall[hitwal->point2].x,
                                                                 hitwal->y-wall[hitwal->point2].y)+512;
                                        Bmemcpy(&sprite[k],&hit.pos,sizeof(vec3_t));

                                        if (proj->workslike & PROJECTILE_RANDDECALSIZE)
                                        {
                                            int32_t wh = (krand()&proj->xrepeat);
                                            if (wh < proj->yrepeat)
                                                wh = proj->yrepeat;
                                            sprite[k].xrepeat = wh;
                                            sprite[k].yrepeat = wh;
                                        }
                                        else
                                        {
                                            sprite[k].xrepeat = proj->xrepeat;
                                            sprite[k].yrepeat = proj->yrepeat;
                                        }
                                        sprite[k].z += sprite[k].yrepeat<<8;
                                        //                                        sprite[k].cstat = 16+(krand()&12);
                                        sprite[k].cstat = 16;

                                        {
                                            int32_t wh = (krand()&1);
                                            if (wh == 1)
                                                sprite[k].cstat |= 4;

                                            wh = (krand()&1);
                                            if (wh == 1)
                                                sprite[k].cstat |= 8;

                                            wh = sprite[k].sectnum;
                                            sprite[k].shade = sector[wh].floorshade;
                                        }
                                        sprite[k].x -= mulscale13(1,sintable[(sprite[k].ang+2560)&2047]);
                                        sprite[k].y -= mulscale13(1,sintable[(sprite[k].ang+2048)&2047]);

                                        A_SetSprite(k,CLIPMASK0);
                                        A_AddToDeleteQueue(k);
                                        changespritestat(k,5);

                                    }
                                    //                                    if( PN == OOZFILTER || PN == NEWBEAST )
                                    //                                        sprite[k].pal = 6;
                                }
                            }

                return -1;
            }

            if (hit.sect < 0) return -1;

            if ((proj->range == 0) && (proj->workslike & PROJECTILE_KNEE))
                proj->range = 1024;

            if (proj->range > 0 && (klabs(srcvect.x-hit.pos.x)+klabs(srcvect.y-hit.pos.y)) > proj->range)
                return -1;
            else
            {
                if (hit.wall >= 0 || hit.sprite >= 0)
                {
                    int32_t picnum;

                    j = A_InsertSprite(hit.sect,hit.pos.x,hit.pos.y,hit.pos.z,atwith,-15,0,0,sa,32,0,i,4);
                    picnum = sprite[j].picnum;
                    SpriteProjectile[j].workslike = ProjectileData[picnum].workslike;
                    sprite[j].extra = proj->extra;

                    if (proj->extra_rand > 0)
                        sprite[j].extra += (krand()&proj->extra_rand);

                    if (p >= 0)
                    {
                        if (proj->spawns >= 0)
                        {
                            k = A_Spawn(j,proj->spawns);
                            sprite[k].z -= (8<<8);
                            A_SetHitData(k, &hit);
                        }
                        if (proj->sound >= 0) A_PlaySound(proj->sound,j);
                    }

                    if (p >= 0 && ps->inv_amount[GET_STEROIDS] > 0 && ps->inv_amount[GET_STEROIDS] < 400)
                        sprite[j].extra += (ps->max_player_health>>2);

                    if (hit.sprite >= 0 && sprite[hit.sprite].picnum != ACCESSSWITCH && sprite[hit.sprite].picnum != ACCESSSWITCH2)
                    {
                        A_DamageObject(hit.sprite,j);
                        if (p >= 0) P_ActivateSwitch(p,hit.sprite,1);
                    }

                    else if (hit.wall >= 0)
                    {
                        if (wall[hit.wall].cstat&2)
                            if (wall[hit.wall].nextsector >= 0)
                                if (hit.pos.z >= (sector[wall[hit.wall].nextsector].floorz))
                                    hit.wall = wall[hit.wall].nextwall;

                        if (hit.wall >= 0 && wall[hit.wall].picnum != ACCESSSWITCH && wall[hit.wall].picnum != ACCESSSWITCH2)
                        {
                            A_DamageWall(j,hit.wall,&hit.pos,atwith);
                            if (p >= 0) P_ActivateSwitch(p,hit.wall,0);
                        }
                    }
                }
                else if (p >= 0 && zvel > 0 && sector[hit.sect].lotag == ST_1_ABOVE_WATER)
                {
                    j = A_Spawn(g_player[p].ps->i,WATERSPLASH2);
                    sprite[j].x = hit.pos.x;
                    sprite[j].y = hit.pos.y;
                    sprite[j].ang = g_player[p].ps->ang; // Total tweek
                    sprite[j].xvel = 32;
                    A_SetSprite(i,CLIPMASK0);
                    sprite[j].xvel = 0;

                }
            }
            return -1;
        }

        if (proj->workslike & PROJECTILE_HITSCAN)
        {
            if (s->extra >= 0) s->shade = proj->shade;

            if (p >= 0)
                P_PreFireHitscan(i, p, atwith, &srcvect, &zvel, &sa,
                              proj->workslike & PROJECTILE_ACCURATE_AUTOAIM,
                              !(proj->workslike & PROJECTILE_ACCURATE));
            else
                A_PreFireHitscan(s, &srcvect, &zvel, &sa,
                                 !(proj->workslike & PROJECTILE_ACCURATE));

            if (Proj_DoHitscan(i, (proj->cstat >= 0) ? proj->cstat : 256+1,
                               &srcvect, zvel, sa, &hit))
                return -1;

            if (proj->range > 0 &&
                    klabs(srcvect.x-hit.pos.x)+klabs(srcvect.y-hit.pos.y) > proj->range)
                return -1;

            if (proj->trail >= 0)
                A_HitscanProjTrail(&srcvect,&hit.pos,sa,atwith);

            if (proj->workslike & PROJECTILE_WATERBUBBLES)
            {
                if ((krand()&15) == 0 && sector[hit.sect].lotag == ST_2_UNDERWATER)
                    A_DoWaterTracers(hit.pos.x,hit.pos.y,hit.pos.z,
                                     srcvect.x,srcvect.y,srcvect.z,8-(ud.multimode>>1));
            }

            if (p >= 0)
            {
                k = Proj_InsertShotspark(&hit, i, atwith, 10, sa, Proj_GetExtra(atwith));

                if (P_PostFireHitscan(p, k, &hit, i, atwith, zvel,
                                      atwith, proj->decal, atwith, 1+2) < 0)
                    return -1;
            }
            else
            {
                k = A_PostFireHitscan(&hit, i, atwith, sa, Proj_GetExtra(atwith),
                                      atwith, atwith);
            }

            if ((krand()&255) < 4 && proj->isound >= 0)
                S_PlaySound3D(proj->isound, k, &hit.pos);

            return -1;
        }

        if (proj->workslike & PROJECTILE_RPG)
        {

            /*            if(tile[atwith].proj.workslike & PROJECTILE_FREEZEBLAST)
            sz += (3<<8);*/

            if (s->extra >= 0) s->shade = proj->shade;

            vel = proj->vel;

            j = -1;

            if (p >= 0)
            {
                j = GetAutoAimAngle(i, p, atwith, 8<<8, 0+2, &srcvect, vel, &zvel, &sa);

                if (j < 0)
                    zvel = (100-ps->horiz-ps->horizoff)*(proj->vel/8);
                //                zvel = (100-ps->horiz-ps->horizoff)*81;

                if (proj->sound >= 0)
                    A_PlaySound(proj->sound,i);
            }
            else
            {
                if (!(proj->workslike & PROJECTILE_NOAIM))
                {
                    j = A_FindPlayer(s,&x);
                    sa = getangle(g_player[j].ps->opos.x-srcvect.x,g_player[j].ps->opos.y-srcvect.y);

                    l = ldist(&sprite[g_player[j].ps->i],s);
                    if (l == 0)
                        l++;
                    zvel = ((g_player[j].ps->opos.z-srcvect.z)*vel) / l;

                    if (A_CheckEnemySprite(s) && (s->hitag&face_player_smart))
                        sa = s->ang+(krand()&31)-16;
                }
            }



            if (p >= 0 && j >= 0)
                l = j;
            else l = -1;

            if (numplayers > 1 && g_netClient) return -1;

            /*                        j = A_InsertSprite(sect,
            sx+(sintable[(348+sa+512)&2047]/448),
            sy+(sintable[(sa+348)&2047]/448),
            sz-(1<<8),atwith,0,14,14,sa,vel,zvel,i,4);*/
            if (actor[i].shootzvel) zvel = actor[i].shootzvel;
            j = A_InsertSprite(sect,
                               srcvect.x+(sintable[(348+sa+512)&2047]/proj->offset),
                               srcvect.y+(sintable[(sa+348)&2047]/proj->offset),
                               srcvect.z-(1<<8),atwith,0,14,14,sa,vel,zvel,i,4);

            sprite[j].xrepeat=proj->xrepeat;
            sprite[j].yrepeat=proj->yrepeat;


            if (proj->extra_rand > 0)
                sprite[j].extra += (krand()&proj->extra_rand);
            if (!(proj->workslike & PROJECTILE_BOUNCESOFFWALLS))
                sprite[j].yvel = l;
            else
            {
                if (proj->bounces >= 1) sprite[j].yvel = proj->bounces;
                else sprite[j].yvel = g_numFreezeBounces;
                //                sprite[j].xrepeat >>= 1;
                //                sprite[j].yrepeat >>= 1;
                sprite[j].zvel -= (2<<4);
            }
            /*
            if(p == -1)
            {
            if(!(tile[atwith].proj.workslike & PROJECTILE_BOUNCESOFFWALLS))
            {
            sprite[j].xrepeat = tile[atwith].proj.xrepeat; // 30
            sprite[j].yrepeat = tile[atwith].proj.yrepeat;
            sprite[j].extra >>= 2;
            }
            }
            */
            if (proj->cstat >= 0) sprite[j].cstat = proj->cstat;
            else sprite[j].cstat = 128;
            if (proj->clipdist != 255) sprite[j].clipdist = proj->clipdist;
            else sprite[j].clipdist = 40;

            {
                int32_t picnum = sprite[j].picnum;
                Bmemcpy(&SpriteProjectile[j], &ProjectileData[picnum], sizeof(projectile_t));
            }

            //            sa = s->ang+32-(krand()&63);
            //            zvel = oldzvel+512-(krand()&1023);

            return j;
        }

    }
    else if (atwith >= 0)
    {
        switch (DYNAMICTILEMAP(atwith))
        {
        case BLOODSPLAT1__STATIC:
        case BLOODSPLAT2__STATIC:
        case BLOODSPLAT3__STATIC:
        case BLOODSPLAT4__STATIC:
            sa += 64 - (krand()&127);
            if (p < 0) sa += 1024;
            zvel = 1024-(krand()&2047);
        case KNEE__STATIC:
            if (atwith == KNEE)
            {
                if (p >= 0)
                {
                    zvel = (100-ps->horiz-ps->horizoff)<<5;
                    srcvect.z += (6<<8);
                    sa += 15;
                }
                else
                {
                    j = g_player[A_FindPlayer(s,&x)].ps->i;
                    zvel = ((sprite[j].z-srcvect.z)<<8) / (x+1);
                    sa = getangle(sprite[j].x-srcvect.x,sprite[j].y-srcvect.y);
                }
            }

            if (actor[i].shootzvel) zvel = actor[i].shootzvel;
            hitscan((const vec3_t *)&srcvect,sect,
                    sintable[(sa+512)&2047],
                    sintable[sa&2047],zvel<<6,
                    &hit,CLIPMASK1);

            if (atwith >= BLOODSPLAT1 && atwith <= BLOODSPLAT4)
            {
                const walltype *const hitwal = &wall[hit.wall];

                if (FindDistance2D(srcvect.x-hit.pos.x,srcvect.y-hit.pos.y) < 1024)
                    if (hit.wall >= 0 && hitwal->overpicnum != BIGFORCE && (hitwal->cstat&16)==0)
                        if ((hitwal->nextsector >= 0 && hit.sect >= 0 &&
                                sector[hitwal->nextsector].lotag == 0 &&
                                sector[hit.sect].lotag == 0 &&
                                sector[hitwal->nextsector].lotag == 0 &&
                                (sector[hit.sect].floorz-sector[hitwal->nextsector].floorz) > (16<<8)) ||
                                (hitwal->nextsector == -1 && sector[hit.sect].lotag == 0))
                        {
                            if (hitwal->nextsector >= 0)
                            {
                                k = headspritesect[hitwal->nextsector];
                                while (k >= 0)
                                {
                                    if (sprite[k].statnum == STAT_EFFECTOR && sprite[k].lotag == SE_13_EXPLOSIVE)
                                        return -1;
                                    k = nextspritesect[k];
                                }
                            }

                            if (hitwal->nextwall >= 0 &&
                                wall[hitwal->nextwall].hitag != 0)
                                return -1;

                            if (hitwal->hitag == 0)
                            {
                                k = A_Spawn(i,atwith);
                                sprite[k].xvel = -12;
                                sprite[k].ang = getangle(hitwal->x-wall[hitwal->point2].x,
                                                         hitwal->y-wall[hitwal->point2].y)+512;
                                sprite[k].x = hit.pos.x;
                                sprite[k].y = hit.pos.y;
                                sprite[k].z = hit.pos.z;
                                sprite[k].cstat |= (krand()&4);
                                A_SetSprite(k,CLIPMASK0);
                                setsprite(k,(vec3_t *)&sprite[k]);
                                if (PN == OOZFILTER || PN == NEWBEAST)
                                    sprite[k].pal = 6;
                            }
                        }

                return -1;
            }

            if (hit.sect < 0) break;

            if ((klabs(srcvect.x-hit.pos.x)+klabs(srcvect.y-hit.pos.y)) < 1024)
            {
                if (hit.wall >= 0 || hit.sprite >= 0)
                {
                    j = A_InsertSprite(hit.sect,hit.pos.x,hit.pos.y,hit.pos.z,KNEE,-15,0,0,sa,32,0,i,4);
                    sprite[j].extra += (krand()&7);
                    if (p >= 0)
                    {
                        k = A_Spawn(j,SMALLSMOKE);
                        sprite[k].z -= (8<<8);
                        A_PlaySound(KICK_HIT,j);
                        A_SetHitData(k, &hit);
                    }

                    if (p >= 0 && ps->inv_amount[GET_STEROIDS] > 0 && ps->inv_amount[GET_STEROIDS] < 400)
                        sprite[j].extra += (ps->max_player_health>>2);

                    if (hit.sprite >= 0 && sprite[hit.sprite].picnum != ACCESSSWITCH && sprite[hit.sprite].picnum != ACCESSSWITCH2)
                    {
                        A_DamageObject(hit.sprite,j);
                        if (p >= 0) P_ActivateSwitch(p,hit.sprite,1);
                    }

                    else if (hit.wall >= 0)
                    {
                        if (wall[hit.wall].cstat&2)
                            if (wall[hit.wall].nextsector >= 0)
                                if (hit.pos.z >= (sector[wall[hit.wall].nextsector].floorz))
                                    hit.wall = wall[hit.wall].nextwall;

                        if (hit.wall >= 0 && wall[hit.wall].picnum != ACCESSSWITCH && wall[hit.wall].picnum != ACCESSSWITCH2)
                        {
                            A_DamageWall(j,hit.wall,&hit.pos,atwith);
                            if (p >= 0) P_ActivateSwitch(p,hit.wall,0);
                        }
                    }
                }
                else if (p >= 0 && zvel > 0 && sector[hit.sect].lotag == ST_1_ABOVE_WATER)
                {
                    j = A_Spawn(g_player[p].ps->i,WATERSPLASH2);
                    sprite[j].x = hit.pos.x;
                    sprite[j].y = hit.pos.y;
                    sprite[j].ang = g_player[p].ps->ang; // Total tweek
                    sprite[j].xvel = 32;
                    A_SetSprite(i,CLIPMASK0);
                    sprite[j].xvel = 0;

                }
            }
            break;

        case SHOTSPARK1__STATIC:
        case SHOTGUN__STATIC:
        case CHAINGUN__STATIC:

            if (s->extra >= 0) s->shade = -96;

            if (p >= 0)
                P_PreFireHitscan(i, p, atwith, &srcvect, &zvel, &sa, 1, 1);
            else
                A_PreFireHitscan(s, &srcvect, &zvel, &sa, 1);

            if (Proj_DoHitscan(i, 256+1, &srcvect, zvel, sa, &hit))
                return -1;

            if ((krand()&15) == 0 && sector[hit.sect].lotag == ST_2_UNDERWATER)
                A_DoWaterTracers(hit.pos.x,hit.pos.y,hit.pos.z,
                                 srcvect.x,srcvect.y,srcvect.z,8-(ud.multimode>>1));

            if (p >= 0)
            {
                k = Proj_InsertShotspark(&hit, i, atwith, 10, sa,
                                         G_InitialActorStrength(atwith) + (krand()%6));

                if (P_PostFireHitscan(p, k, &hit, i, atwith, zvel,
                                      -SMALLSMOKE, BULLETHOLE, SHOTSPARK1, 0) < 0)
                    return -1;
            }
            else
            {
                k = A_PostFireHitscan(&hit, i, atwith, sa, G_InitialActorStrength(atwith),
                                      -SMALLSMOKE, SHOTSPARK1);
            }

            if ((krand()&255) < 4)
                S_PlaySound3D(PISTOL_RICOCHET, k, &hit.pos);

            return -1;

        case FIRELASER__STATIC:
        case SPIT__STATIC:
        case COOLEXPLOSION1__STATIC:

            if (s->extra >= 0) s->shade = -96;

            if (atwith == SPIT) vel = 292;
            else
            {
                if (atwith == COOLEXPLOSION1)
                {
                    if (s->picnum == BOSS2) vel = 644;
                    else vel = 348;
                    srcvect.z -= (4<<7);
                }
                else
                {
                    vel = 840;
                    srcvect.z -= (4<<7);
                }
            }

            if (p >= 0)
            {
                j = GetAutoAimAngle(i, p, atwith, -(12<<8), 0, &srcvect, vel, &zvel, &sa);

                if (j < 0)
                    zvel = (100-ps->horiz-ps->horizoff)*98;
            }
            else
            {
                j = A_FindPlayer(s,&x);
                //                sa = getangle(g_player[j].ps->opos.x-sx,g_player[j].ps->opos.y-sy);
                sa += 16-(krand()&31);
                hit.pos.x = ldist(&sprite[g_player[j].ps->i],s);
                if (hit.pos.x == 0) hit.pos.x++;
                zvel = ((g_player[j].ps->opos.z - srcvect.z + (3<<8))*vel) / hit.pos.x;
            }
            if (actor[i].shootzvel) zvel = actor[i].shootzvel;
            oldzvel = zvel;

            if (atwith == SPIT)
            {
                sizx = sizy = 18;
                srcvect.z -= (10<<8);
            }
            else if (p >= 0)
                sizx = sizy = 7;
            else
            {
                if (atwith == FIRELASER)
                {
                    if (p >= 0)
                        sizx = sizy = 34;
                    else
                        sizx = sizy = 18;
                }
                else
                    sizx = sizy = 18;
            }

            j = A_InsertSprite(sect,srcvect.x,srcvect.y,srcvect.z,
                               atwith,-127,sizx,sizy,sa,vel,zvel,i,4);
            sprite[j].extra += (krand()&7);

            if (atwith == COOLEXPLOSION1)
            {
                sprite[j].shade = 0;
                if (PN == BOSS2)
                {
                    l = sprite[j].xvel;
                    sprite[j].xvel = 1024;
                    A_SetSprite(j,CLIPMASK0);
                    sprite[j].xvel = l;
                    sprite[j].ang += 128-(krand()&255);
                }
            }

            sprite[j].cstat = 128;
            sprite[j].clipdist = 4;

            sa = s->ang+32-(krand()&63);
            zvel = oldzvel+512-(krand()&1023);

            return j;

        case FREEZEBLAST__STATIC:
            srcvect.z += (3<<8);
        case RPG__STATIC:

            if (s->extra >= 0) s->shade = -96;

            vel = 644;

            j = -1;

            if (p >= 0)
            {
                j = GetAutoAimAngle(i, p, atwith, 8<<8, 0+2, &srcvect, vel, &zvel, &sa);

                if (j < 0)
                    zvel = (100-ps->horiz-ps->horizoff)*81;

                if (atwith == RPG)
                    A_PlaySound(RPG_SHOOT,i);
            }
            else
            {
                j = A_FindPlayer(s,&x);
                sa = getangle(g_player[j].ps->opos.x-srcvect.x,g_player[j].ps->opos.y-srcvect.y);
                if (PN == BOSS3)
                    srcvect.z -= (32<<8);
                else if (PN == BOSS2)
                {
                    vel += 128;
                    srcvect.z += 24<<8;
                }

                l = ldist(&sprite[g_player[j].ps->i],s);
                if (l == 0)
                    l++;
                zvel = ((g_player[j].ps->opos.z-srcvect.z)*vel) / l;

                if (A_CheckEnemySprite(s) && (s->hitag&face_player_smart))
                    sa = s->ang+(krand()&31)-16;
            }

            if (p >= 0 && j >= 0)
                l = j;
            else l = -1;

            if (numplayers > 1 && g_netClient) return -1;

            if (actor[i].shootzvel) zvel = actor[i].shootzvel;
            j = A_InsertSprite(sect,
                               srcvect.x+(sintable[(348+sa+512)&2047]/448),
                               srcvect.y+(sintable[(sa+348)&2047]/448),
                               srcvect.z-(1<<8),atwith,0,14,14,sa,vel,zvel,i,4);

            sprite[j].extra += (krand()&7);
            if (atwith != FREEZEBLAST)
                sprite[j].yvel = l;
            else
            {
                sprite[j].yvel = g_numFreezeBounces;
                sprite[j].xrepeat >>= 1;
                sprite[j].yrepeat >>= 1;
                sprite[j].zvel -= (2<<4);
            }

            if (p == -1)
            {
                if (PN == BOSS3)
                {
                    if (krand()&1)
                    {
                        sprite[j].x -= sintable[sa&2047]>>6;
                        sprite[j].y -= sintable[(sa+1024+512)&2047]>>6;
                        sprite[j].ang -= 8;
                    }
                    else
                    {
                        sprite[j].x += sintable[sa&2047]>>6;
                        sprite[j].y += sintable[(sa+1024+512)&2047]>>6;
                        sprite[j].ang += 4;
                    }
                    sprite[j].xrepeat = 42;
                    sprite[j].yrepeat = 42;
                }
                else if (PN == BOSS2)
                {
                    sprite[j].x -= sintable[sa&2047]/56;
                    sprite[j].y -= sintable[(sa+1024+512)&2047]/56;
                    sprite[j].ang -= 8+(krand()&255)-128;
                    sprite[j].xrepeat = 24;
                    sprite[j].yrepeat = 24;
                }
                else if (atwith != FREEZEBLAST)
                {
                    sprite[j].xrepeat = 30;
                    sprite[j].yrepeat = 30;
                    sprite[j].extra >>= 2;
                }
            }

            else if (PWEAPON(0, g_player[p].ps->curr_weapon, WorksLike) == DEVISTATOR_WEAPON)
            {
                sprite[j].extra >>= 2;
                sprite[j].ang += 16-(krand()&31);
                sprite[j].zvel += 256-(krand()&511);

                if (g_player[p].ps->hbomb_hold_delay)
                {
                    sprite[j].x -= sintable[sa&2047]/644;
                    sprite[j].y -= sintable[(sa+1024+512)&2047]/644;
                }
                else
                {
                    sprite[j].x += sintable[sa&2047]>>8;
                    sprite[j].y += sintable[(sa+1024+512)&2047]>>8;
                }
                sprite[j].xrepeat >>= 1;
                sprite[j].yrepeat >>= 1;
            }

            sprite[j].cstat = 128;
            if (atwith == RPG)
                sprite[j].clipdist = 4;
            else
                sprite[j].clipdist = 40;

            return j;

        case HANDHOLDINGLASER__STATIC:
        {
            const int32_t zoff = (p>=0) ? g_player[p].ps->pyoff : 0;
            if (p >= 0)
                zvel = (100-ps->horiz-ps->horizoff)*32;
            else zvel = 0;
            if (actor[i].shootzvel) zvel = actor[i].shootzvel;

            srcvect.z -= zoff;
            hitscan((const vec3_t *)&srcvect,sect,
                    sintable[(sa+512)&2047],
                    sintable[sa&2047],
                    zvel<<6,&hit,CLIPMASK1);
            srcvect.z += zoff;

            j = 0;
            if (hit.sprite >= 0) break;

            if (hit.wall >= 0 && hit.sect >= 0)
                if (((hit.pos.x-srcvect.x)*(hit.pos.x-srcvect.x)+(hit.pos.y-srcvect.y)*(hit.pos.y-srcvect.y)) < (290*290))
                {
                    // ST_2_UNDERWATER
                    if (wall[hit.wall].nextsector >= 0)
                    {
                        if (sector[wall[hit.wall].nextsector].lotag <= 2 && sector[hit.sect].lotag <= 2)
                            j = 1;
                    }
                    else if (sector[hit.sect].lotag <= 2)
                        j = 1;
                }

            if (j == 1)
            {
                int32_t lTripBombControl = (p < 0) ? 0 :
                    Gv_GetVarByLabel("TRIPBOMB_CONTROL", TRIPBOMB_TRIPWIRE, g_player[p].ps->i, p);
                k = A_InsertSprite(hit.sect,hit.pos.x,hit.pos.y,hit.pos.z,TRIPBOMB,-16,4,5,sa,0,0,i,6);
                if (lTripBombControl & TRIPBOMB_TIMER)
                {
                    int32_t lLifetime=Gv_GetVarByLabel("STICKYBOMB_LIFETIME", NAM_GRENADE_LIFETIME, g_player[p].ps->i, p);
                    int32_t lLifetimeVar=Gv_GetVarByLabel("STICKYBOMB_LIFETIME_VAR", NAM_GRENADE_LIFETIME_VAR, g_player[p].ps->i, p);
                    // set timer.  blows up when at zero....
                    actor[k].t_data[7]=lLifetime
                                       + mulscale(krand(),lLifetimeVar, 14)
                                       - lLifetimeVar;
                    actor[k].t_data[6]=1;
                }
                else
                    sprite[k].hitag = k;

                A_PlaySound(LASERTRIP_ONWALL,k);
                sprite[k].xvel = -20;
                A_SetSprite(k,CLIPMASK0);
                sprite[k].cstat = 16;

                {
                    int32_t p2 = wall[hit.wall].point2;
                    int32_t a = getangle(wall[hit.wall].x-wall[p2].x, wall[hit.wall].y-wall[p2].y)-512;
                    actor[k].t_data[5] = sprite[k].ang = a;
                }
            }
            return j?k:-1;
        }
        case BOUNCEMINE__STATIC:
        case MORTER__STATIC:

            if (s->extra >= 0) s->shade = -96;

            j = g_player[A_FindPlayer(s,&x)].ps->i;
            x = ldist(&sprite[j],s);

            zvel = -x>>1;

            if (zvel < -4096)
                zvel = -2048;
            vel = x>>4;
            if (actor[i].shootzvel) zvel = actor[i].shootzvel;
            A_InsertSprite(sect,
                           srcvect.x+(sintable[(512+sa+512)&2047]>>8),
                           srcvect.y+(sintable[(sa+512)&2047]>>8),
                           srcvect.z+(6<<8),atwith,-64,32,32,sa,vel,zvel,i,1);
            break;

        case GROWSPARK__STATIC:

            if (p >= 0)
            {
                j = GetAutoAimAngle(i, p, atwith, 5<<8, 0+1, &srcvect, 256, &zvel, &sa);

                if (j < 0)
                {
                    sa += 16-(krand()&31);
                    zvel = (100-ps->horiz-ps->horizoff)<<5;
                    zvel += 128-(krand()&255);
                }

                srcvect.z -= (2<<8);
            }
            else
            {
                j = A_FindPlayer(s,&x);
                srcvect.z -= (4<<8);
                hit.pos.x = ldist(&sprite[g_player[j].ps->i], s);
                if (hit.pos.x == 0)
                    hit.pos.x++;
                zvel = ((g_player[j].ps->pos.z-srcvect.z) <<8) / hit.pos.x;
                zvel += 128-(krand()&255);
                sa += 32-(krand()&63);
            }

            k = 0;

            //            RESHOOTGROW:
            if (sect < 0) break;

            s->cstat &= ~257;
            if (actor[i].shootzvel) zvel = actor[i].shootzvel;
            hitscan((const vec3_t *)&srcvect,sect,
                    sintable[(sa+512)&2047],
                    sintable[sa&2047],
                    zvel<<6,&hit,CLIPMASK1);

            s->cstat |= 257;

            j = A_InsertSprite(sect,hit.pos.x,hit.pos.y,hit.pos.z,GROWSPARK,-16,28,28,sa,0,0,i,1);

            sprite[j].pal = 2;
            sprite[j].cstat |= 130;
            sprite[j].xrepeat = sprite[j].yrepeat = 1;

            if (hit.wall == -1 && hit.sprite == -1 && hit.sect >= 0)
            {
                if (zvel < 0 && (sector[hit.sect].ceilingstat&1) == 0)
                    Sect_DamageCeiling(hit.sect);
            }
            else if (hit.sprite >= 0) A_DamageObject(hit.sprite,j);
            else if (hit.wall >= 0 && wall[hit.wall].picnum != ACCESSSWITCH && wall[hit.wall].picnum != ACCESSSWITCH2)
            {
                /*    if(wall[hit.wall].overpicnum == MIRROR && k == 0)
                {
                l = getangle(
                wall[wall[hit.wall].point2].x-wall[hit.wall].x,
                wall[wall[hit.wall].point2].y-wall[hit.wall].y);

                sx = hit.pos.x;
                sy = hit.pos.y;
                srcvect.z = hit.pos.z;
                sect = hit.sect;
                sa = ((l<<1) - sa)&2047;
                sx += sintable[(sa+512)&2047]>>12;
                sy += sintable[sa&2047]>>12;

                k++;
                goto RESHOOTGROW;
                }
                else */
                A_DamageWall(j,hit.wall,&hit.pos,atwith);
            }

            break;

        case SHRINKER__STATIC:
            if (s->extra >= 0) s->shade = -96;
            if (p >= 0)
            {
                j = GetAutoAimAngle(i, p, atwith, 4<<8, 0, &srcvect, 768, &zvel, &sa);

                if (j < 0)
                    zvel = (100-ps->horiz-ps->horizoff)*98;
            }
            else if (s->statnum != STAT_EFFECTOR)
            {
                j = A_FindPlayer(s,&x);
                l = ldist(&sprite[g_player[j].ps->i],s);
                if (l == 0)
                    l++;
                zvel = ((g_player[j].ps->opos.z-srcvect.z)*512) / l ;
            }
            else zvel = 0;
            if (actor[i].shootzvel) zvel = actor[i].shootzvel;
            j = A_InsertSprite(sect,
                               srcvect.x+(sintable[(512+sa+512)&2047]>>12),
                               srcvect.y+(sintable[(sa+512)&2047]>>12),
                               srcvect.z+(2<<8),SHRINKSPARK,-16,28,28,sa,768,zvel,i,4);

            sprite[j].cstat = 128;
            sprite[j].clipdist = 32;


            return j;
        }
    }
    return -1;
}


//////////////////// HUD WEAPON / MISC. DISPLAY CODE ////////////////////

static void P_DisplaySpit(int32_t snum)
{
    int32_t i, a, x, y, z;
    DukePlayer_t *const ps = g_player[snum].ps;

    if (ps->loogcnt == 0)
        return;

    y = (ps->loogcnt<<2);

    for (i=0; i<ps->numloogs; i++)
    {
        a = klabs(sintable[((ps->loogcnt+i)<<5)&2047])>>5;
        z = 4096+((ps->loogcnt+i)<<9);
        x = (-g_player[snum].sync->avel)+(sintable[((ps->loogcnt+i)<<6)&2047]>>10);

        rotatesprite_fs(
            (ps->loogiex[i]+x)<<16,(200+ps->loogiey[i]-y)<<16,z-(i<<8),256-a,
            LOOGIE,0,0,2);
    }
}

static int32_t P_GetHudPal(const DukePlayer_t *p)
{
    if (sprite[p->i].pal == 1)
        return 1;

    if (p->cursectnum >= 0)
    {
        int32_t dapal = sector[p->cursectnum].floorpal;
        if (!g_noFloorPal[dapal])
            return dapal;
    }

    return 0;
}

static int32_t P_DisplayFist(int32_t gs,int32_t snum)
{
    int32_t looking_arc,fisti,fistpal;
    int32_t fistzoom, fistz;

    int32_t wx[2] = { windowx1, windowx2 };

    const DukePlayer_t *const ps = g_player[snum].ps;

    fisti = ps->fist_incs;
    if (fisti > 32) fisti = 32;
    if (fisti <= 0) return 0;

    looking_arc = klabs(ps->look_ang)/9;

    fistzoom = 65536 - (sintable[(512+(fisti<<6))&2047]<<2);
    fistzoom = clamp(fistzoom, 40920, 90612);

    fistz = 194 + (sintable[((6+fisti)<<7)&2047]>>9);

    fistpal = P_GetHudPal(ps);

    // XXX: this is outdated, doesn't handle above/below split.
    if (g_fakeMultiMode==2)
        wx[(g_snum==0)] = (wx[0]+wx[1])/2+1;

    rotatesprite(
        (-fisti+222+(g_player[snum].sync->avel>>4))<<16,
        (looking_arc+fistz)<<16,
        fistzoom,0,FIST,gs,fistpal,2,
        wx[0],windowy1,wx[1],windowy2);

    return 1;
}


#define DRAWEAP_CENTER 262144

static inline int32_t weapsc(int32_t sc)
{
    return scale(sc, ud.weaponscale, 100);
}

static void G_DrawTileScaled(int32_t x, int32_t y, int32_t tilenum, int32_t shade, int32_t orientation, int32_t p)
{
    int32_t ang = 0;
    int32_t xoff = 192;

    int32_t wx[2] = { windowx1, windowx2 };
    int32_t wy[2] = { windowy1, windowy2 };
    int32_t yofs = 0;

    switch (g_currentweapon)
    {
    case DEVISTATOR_WEAPON:
    case TRIPBOMB_WEAPON:
        xoff = 160;
        break;
    default:
        if (orientation & DRAWEAP_CENTER)
        {
            xoff = 160;
            orientation &= ~DRAWEAP_CENTER;
        }
        break;
    }

    // bit 4 means "flip x" for G_DrawTileScaled
    if (orientation&4)
        ang = 1024;

    if (g_fakeMultiMode==2)
    {
        const int32_t sidebyside = (ud.screen_size!=0);

        // splitscreen HACK
        orientation &= ~(1024|512|256);
        if (sidebyside)
        {
            orientation &= ~8;
            wx[(g_snum==0)] = (wx[0]+wx[1])/2 + 2;
        }
        else
        {
            orientation |= 8;
            if (g_snum==0)
                yofs = -(100<<16);
            wy[(g_snum==0)] = (wy[0]+wy[1])/2 + 2;
        }
    }

#ifdef USE_OPENGL
    if (getrendermode() >= REND_POLYMOST && usemodels && md_tilehasmodel(tilenum,p) >= 0)
        y += (224-weapsc(224));
#endif
    rotatesprite(weapsc(x<<16) + ((xoff-weapsc(xoff))<<16),
                 weapsc(y<<16) + ((200-weapsc(200))<<16) + yofs,
                 weapsc(65536L),ang,tilenum,shade,p,(2|orientation),
                 wx[0],wy[0], wx[1],wy[1]);
}

static void G_DrawWeaponTile(int32_t x, int32_t y, int32_t tilenum, int32_t shade,
                             int32_t orientation, int32_t p, uint8_t slot)
{
    static int32_t shadef[2] = {0, 0}, palf[2] = {0, 0};

    // sanity checking the slot value
    if (slot > 1)
        slot = 1;

    // basic fading between player weapon shades
    if (shadef[slot] != shade && (!p || palf[slot] == p))
    {
        shadef[slot] += (shade-shadef[slot])>>2;

        if (!((shade-shadef[slot])>>2))
        {
            shadef[slot] += (shade-shadef[slot])>>1;
            if (!((shade-shadef[slot])>>1))
                shadef[slot] = shade;
        }
    }
    else
        shadef[slot] = shade;

    palf[slot] = p;

    switch (ud.drawweapon)
    {
    default:
        return;
    case 1:
        G_DrawTileScaled(x,y,tilenum,shadef[slot],orientation,p);
        return;
    case 2:
    {
        const DukePlayer_t *const ps = g_player[screenpeek].ps;
        const int32_t sc = scale(65536,ud.statusbarscale,100);

        switch (g_currentweapon)
        {
        case PISTOL_WEAPON:
        case CHAINGUN_WEAPON:
        case RPG_WEAPON:
        case FREEZE_WEAPON:
        case SHRINKER_WEAPON:
        case GROW_WEAPON:
        case DEVISTATOR_WEAPON:
        case TRIPBOMB_WEAPON:
        case HANDREMOTE_WEAPON:
        case HANDBOMB_WEAPON:
        case SHOTGUN_WEAPON:
            rotatesprite_win(160<<16,(180+(ps->weapon_pos*ps->weapon_pos))<<16,
                             sc,0,g_currentweapon==GROW_WEAPON?GROWSPRITEICON:WeaponPickupSprites[g_currentweapon],
                             0,0,2);
            return;
        default:
            return;
        }
    }
    }
}

static int32_t P_DisplayKnee(int32_t gs,int32_t snum)
{
    static const int8_t knee_y[] = {0,-8,-16,-32,-64,-84,-108,-108,-108,-72,-32,-8};
    int32_t looking_arc, pal;

    const DukePlayer_t *const ps = g_player[snum].ps;

    if (ps->knee_incs > 11 || ps->knee_incs == 0 || sprite[ps->i].extra <= 0) return 0;

    looking_arc = knee_y[ps->knee_incs] + klabs(ps->look_ang)/9;

    looking_arc -= (ps->hard_landing<<3);

    pal = P_GetHudPal(ps);
    if (pal == 0)
        pal = ps->palookup;

    G_DrawTileScaled(105+(g_player[snum].sync->avel>>4)-(ps->look_ang>>1)+(knee_y[ps->knee_incs]>>2),
                     looking_arc+280-((ps->horiz-ps->horizoff)>>4),KNEE,gs,4+DRAWEAP_CENTER,pal);

    return 1;
}

static int32_t P_DisplayKnuckles(int32_t gs,int32_t snum)
{
    static const int8_t knuckle_frames[] = {0,1,2,2,3,3,3,2,2,1,0};
    int32_t looking_arc, pal;

    const DukePlayer_t *const ps = g_player[snum].ps;

    if (ps->knuckle_incs == 0 || sprite[ps->i].extra <= 0) return 0;

    looking_arc = klabs(ps->look_ang)/9;

    looking_arc -= (ps->hard_landing<<3);

    pal = P_GetHudPal(ps);

    G_DrawTileScaled(160+(g_player[snum].sync->avel>>4)-(ps->look_ang>>1),
                     looking_arc+180-((ps->horiz-ps->horizoff)>>4),
                     CRACKKNUCKLES+knuckle_frames[ps->knuckle_incs>>1],gs,4+DRAWEAP_CENTER,pal);

    return 1;
}

static void P_FireWeapon(DukePlayer_t *p)
{
    int32_t i, snum = sprite[p->i].yvel;

    if (VM_OnEvent(EVENT_DOFIRE, p->i, snum, -1, 0) == 0)
    {
        if (p->weapon_pos != 0) return;

        if (PWEAPON(snum, p->curr_weapon, WorksLike) != KNEE_WEAPON)
            p->ammo_amount[p->curr_weapon]--;

        if (PWEAPON(snum, p->curr_weapon, FireSound) > 0)
            A_PlaySound(PWEAPON(snum, p->curr_weapon, FireSound),p->i);

        Gv_SetVar(g_iWeaponVarID,p->curr_weapon,p->i,snum);
        Gv_SetVar(g_iWorksLikeVarID,PWEAPON(snum, p->curr_weapon, WorksLike), p->i, snum);
//        OSD_Printf("doing %d %d %d\n",PWEAPON(snum, p->curr_weapon, Shoots),p->curr_weapon,snum);
        A_Shoot(p->i,PWEAPON(snum, p->curr_weapon, Shoots));

        for (i=PWEAPON(snum, p->curr_weapon, ShotsPerBurst)-1; i > 0; i--)
        {
            if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_FIREEVERYOTHER)
            {
                // this makes the projectiles fire on a delay from player code
                actor[p->i].t_data[7] = (PWEAPON(snum, p->curr_weapon, ShotsPerBurst))<<1;
            }
            else
            {
                if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_AMMOPERSHOT &&
                        PWEAPON(snum, p->curr_weapon, WorksLike) != KNEE_WEAPON)
                {
                    if (p->ammo_amount[p->curr_weapon] > 0)
                        p->ammo_amount[p->curr_weapon]--;
                    else break;
                }
                A_Shoot(p->i,PWEAPON(snum, p->curr_weapon, Shoots));
            }
        }

        if (!(PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_NOVISIBLE))
        {
#ifdef POLYMER
            spritetype *s = &sprite[p->i];
            int32_t x = ((sintable[(s->ang+512)&2047])>>7), y = ((sintable[(s->ang)&2047])>>7);

            s->x += x;
            s->y += y;
            G_AddGameLight(0, p->i, PHEIGHT, 8192, PWEAPON(snum, p->curr_weapon, FlashColor),PR_LIGHT_PRIO_MAX_GAME);
            actor[p->i].lightcount = 2;
            s->x -= x;
            s->y -= y;
#endif // POLYMER
            lastvisinc = totalclock+32;
            p->visibility = 0;
        }
    }
}

void P_DoWeaponSpawn(DukePlayer_t *p)
{
    int32_t j, snum = sprite[p->i].yvel;

    if (PWEAPON(snum, p->curr_weapon, Spawn) <= 0)  // <=0 : AMC TC beta/RC2 has WEAPONx_SPAWN -1
        return;

    j = A_Spawn(p->i, PWEAPON(snum, p->curr_weapon, Spawn));

    if ((PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_SPAWNTYPE3))
    {
        // like chaingun shells
        sprite[j].ang += 1024;
        sprite[j].ang &= 2047;
        sprite[j].xvel += 32;
        sprite[j].z += (3<<8);
    }

    A_SetSprite(j,CLIPMASK0);

}

void P_DisplayScuba(int32_t snum)
{
    if (g_player[snum].ps->scuba_on)
    {
        int32_t p = P_GetHudPal(g_player[snum].ps);

        g_snum = snum;
        G_DrawTileScaled(43, (200-tilesizy[SCUBAMASK]), SCUBAMASK, 0, 2+16+DRAWEAP_CENTER, p);
        G_DrawTileScaled(320-43, (200-tilesizy[SCUBAMASK]), SCUBAMASK, 0, 2+4+16+DRAWEAP_CENTER, p);
    }
}

static int32_t P_DisplayTip(int32_t gs,int32_t snum)
{
    int32_t p,looking_arc, i, tipy;

    static const int16_t tip_y[] = {
        0,-8,-16,-32,-64,
        -84,-108,-108,-108,-108,
        -108,-108,-108,-108,-108,
        -108,-96,-72,-64,-32,
        -16
    };

    const DukePlayer_t *const ps = g_player[snum].ps;

    if (ps->tipincs == 0) return 0;

    looking_arc = klabs(ps->look_ang)/9;
    looking_arc -= (ps->hard_landing<<3);

    p = P_GetHudPal(ps);

    /*    if(ps->access_spritenum >= 0)
            p = sprite[ps->access_spritenum].pal;
        else
            p = wall[ps->access_wallnum].pal;
      */

    // FIXME?
    // OOB access of tip_y[] happens in 'Spider Den' of WGR2 SVN r72
    i = ps->tipincs;
    tipy = ((unsigned)i < sizeof(tip_y)/sizeof(tip_y[0])) ? (tip_y[i]>>1) : 0;

    G_DrawTileScaled(170+(g_player[snum].sync->avel>>4)-(ps->look_ang>>1),
                     tipy+looking_arc+240-((ps->horiz-ps->horizoff)>>4),
                     TIP+((26-ps->tipincs)>>4),gs,DRAWEAP_CENTER,p);

    return 1;
}

static int32_t P_DisplayAccess(int32_t gs,int32_t snum)
{
    static const int16_t access_y[] = {
        0,-8,-16,-32,-64,
        -84,-108,-108,-108,-108,
        -108,-108,-108,-108,-108,
        -108,-96,-72,-64,-32,
        -16
    };

    int32_t looking_arc, p = 0;
    const DukePlayer_t *const ps = g_player[snum].ps;

    if (ps->access_incs == 0 || sprite[ps->i].extra <= 0) return 0;

    looking_arc = access_y[ps->access_incs] + klabs(ps->look_ang)/9 -
                  (ps->hard_landing<<3);

    if (ps->access_spritenum >= 0)
        p = sprite[ps->access_spritenum].pal;

    //    else
    //        p = wall[ps->access_wallnum].pal;

    if ((ps->access_incs-3) > 0 && (ps->access_incs-3)>>3)
    {
        guniqhudid = 200;
        G_DrawTileScaled(170+(g_player[snum].sync->avel>>4)-(ps->look_ang>>1)+(access_y[ps->access_incs]>>2),
                         looking_arc+266-((ps->horiz-ps->horizoff)>>4),HANDHOLDINGLASER+(ps->access_incs>>3),
                         gs,DRAWEAP_CENTER,p);
        guniqhudid = 0;
    }
    else
    {
        guniqhudid = 201;
        G_DrawTileScaled(170+(g_player[snum].sync->avel>>4)-(ps->look_ang>>1)+(access_y[ps->access_incs]>>2),
                         looking_arc+266-((ps->horiz-ps->horizoff)>>4),HANDHOLDINGACCESS,gs,4+DRAWEAP_CENTER,p);
        guniqhudid = 0;
    }

    return 1;
}


static int32_t fistsign;

void P_DisplayWeapon(int32_t snum)
{
    int32_t gun_pos, looking_arc, cw;
    int32_t weapon_xoffset, i, j;
    int32_t o = 0,pal = 0;
    DukePlayer_t *const p = g_player[snum].ps;
    const uint8_t *const kb = &p->kickback_pic;
    int32_t gs;

    g_snum = snum;

    looking_arc = klabs(p->look_ang)/9;

    gs = sprite[p->i].shade;
    if (gs > 24) gs = 24;

    if (p->newowner >= 0 || ud.camerasprite >= 0 || p->over_shoulder_on > 0 || (sprite[p->i].pal != 1 && sprite[p->i].extra <= 0) ||
            P_DisplayFist(gs,snum) || P_DisplayKnuckles(gs,snum) || P_DisplayTip(gs,snum) || P_DisplayAccess(gs,snum))
        return;

    P_DisplayKnee(gs,snum);

    gun_pos = 80-(p->weapon_pos*p->weapon_pos);

    weapon_xoffset = (160)-90;

    if (ud.weaponsway)
    {
        weapon_xoffset -= (sintable[((p->weapon_sway>>1)+512)&2047]/(1024+512));

        if (sprite[p->i].xrepeat < 32)
            gun_pos -= klabs(sintable[(p->weapon_sway<<2)&2047]>>9);
        else gun_pos -= klabs(sintable[(p->weapon_sway>>1)&2047]>>10);
    }
    else gun_pos -= 16;

    weapon_xoffset -= 58 + p->weapon_ang;
    gun_pos -= (p->hard_landing<<3);

    if (p->last_weapon >= 0)
        cw = PWEAPON(snum, p->last_weapon, WorksLike);
    else
        cw = PWEAPON(snum, p->curr_weapon, WorksLike);

    g_gun_pos=gun_pos;
    g_looking_arc=looking_arc;
    g_currentweapon=cw;
    g_weapon_xoffset=weapon_xoffset;
    g_gs=gs;
    g_kb=*kb;
    g_looking_angSR1=p->look_ang>>1;

    if (VM_OnEvent(EVENT_DISPLAYWEAPON, p->i, screenpeek, -1, 0) == 0)
    {
        j = 14-p->quick_kick;
        if (j != 14 || p->last_quick_kick)
        {
            pal = P_GetHudPal(p);
            if (pal == 0)
                pal = p->palookup;

            guniqhudid = 100;
            if (j < 6 || j > 12)
                G_DrawTileScaled(weapon_xoffset+80-(p->look_ang>>1),
                                 looking_arc+250-gun_pos,KNEE,gs,o|4|DRAWEAP_CENTER,pal);
            else G_DrawTileScaled(weapon_xoffset+160-16-(p->look_ang>>1),
                                  looking_arc+214-gun_pos,KNEE+1,gs,o|4|DRAWEAP_CENTER,pal);
            guniqhudid = 0;
        }

        if (sprite[p->i].xrepeat < 40)
        {
            pal = P_GetHudPal(p);

            if (p->jetpack_on == 0)
            {
                i = sprite[p->i].xvel;
                looking_arc += 32-(i>>1);
                fistsign += i>>1;
            }

            cw = weapon_xoffset;
            weapon_xoffset += sintable[(fistsign)&2047]>>10;
            G_DrawTileScaled(weapon_xoffset+250-(p->look_ang>>1),
                             looking_arc+258-(klabs(sintable[(fistsign)&2047]>>8)),
                             FIST,gs,o, pal);
            weapon_xoffset = cw - (sintable[(fistsign)&2047]>>10);
            G_DrawTileScaled(weapon_xoffset+40-(p->look_ang>>1),
                             looking_arc+200+(klabs(sintable[(fistsign)&2047]>>8)),
                             FIST,gs,o|4, pal);
        }
        else
        {
            pal = P_GetHudPal(p);

            switch (cw)
            {
            case KNEE_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    if ((*kb) > 0)
                    {
                        if (pal == 0)
                            pal = p->palookup;

                        guniqhudid = cw;
                        if ((*kb) < 5 || (*kb) > 9)
                            G_DrawTileScaled(weapon_xoffset+220-(p->look_ang>>1),
                            looking_arc+250-gun_pos,KNEE,gs,o,pal);
                        else
                            G_DrawTileScaled(weapon_xoffset+160-(p->look_ang>>1),
                            looking_arc+214-gun_pos,KNEE+1,gs,o,pal);
                        guniqhudid = 0;
                    }
                }
                break;

            case TRIPBOMB_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    weapon_xoffset += 8;
                    gun_pos -= 10;

                    if ((*kb) > 6)
                        looking_arc += ((*kb)<<3);
                    else if ((*kb) < 4)
                    {
                        guniqhudid = cw<<2;
                        G_DrawWeaponTile(weapon_xoffset+142-(p->look_ang>>1),
                            looking_arc+234-gun_pos,HANDHOLDINGLASER+3,gs,o,pal,0);
                    }

                    guniqhudid = cw;
                    G_DrawWeaponTile(weapon_xoffset+130-(p->look_ang>>1),
                        looking_arc+249-gun_pos,
                        HANDHOLDINGLASER+((*kb)>>2),gs,o,pal,0);

                    guniqhudid = cw<<1;
                    G_DrawWeaponTile(weapon_xoffset+152-(p->look_ang>>1),
                        looking_arc+249-gun_pos,
                        HANDHOLDINGLASER+((*kb)>>2),gs,o|4,pal,0);
                    guniqhudid = 0;
                }
                break;

            case RPG_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    weapon_xoffset -= sintable[(768+((*kb)<<7))&2047]>>11;
                    gun_pos += sintable[(768+((*kb)<<7))&2047]>>11;

                    if (*kb > 0 && *kb < 8)
                    {
                        G_DrawWeaponTile(weapon_xoffset+164,(looking_arc<<1)+176-gun_pos,
                            RPGGUN+((*kb)>>1),gs,o|512,pal,0);
                    }

                    G_DrawWeaponTile(weapon_xoffset+164,(looking_arc<<1)+176-gun_pos,
                        RPGGUN,gs,o|512,pal,0);
                }
                break;

            case SHOTGUN_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    weapon_xoffset -= 8;

                    switch (*kb)
                    {
                    case 1:
                    case 2:
                        guniqhudid = cw<<1;
                        G_DrawWeaponTile(weapon_xoffset+168-(p->look_ang>>1),looking_arc+201-gun_pos,
                            SHOTGUN+2,-128,o,pal,0);
                    case 0:
                    case 6:
                    case 7:
                    case 8:
                        guniqhudid = cw;
                        G_DrawWeaponTile(weapon_xoffset+146-(p->look_ang>>1),looking_arc+202-gun_pos,
                            SHOTGUN,gs,o,pal,0);
                        guniqhudid = 0;
                        break;
                    case 3:
                    case 4:
                    case 5:
                    case 9:
                    case 10:
                    case 11:
                    case 12:
                        if (*kb > 1 && *kb < 5)
                        {
                            gun_pos -= 40;
                            weapon_xoffset += 20;

                            guniqhudid = cw<<1;
                            G_DrawWeaponTile(weapon_xoffset+178-(p->look_ang>>1),looking_arc+194-gun_pos,
                                SHOTGUN+1+((*(kb)-1)>>1),-128,o,pal,0);
                        }
                        guniqhudid = cw;
                        G_DrawWeaponTile(weapon_xoffset+158-(p->look_ang>>1),looking_arc+220-gun_pos,
                            SHOTGUN+3,gs,o,pal,0);
                        guniqhudid = 0;
                        break;
                    case 13:
                    case 14:
                    case 15:
                        guniqhudid = cw;
                        G_DrawWeaponTile(32+weapon_xoffset+166-(p->look_ang>>1),looking_arc+210-gun_pos,
                            SHOTGUN+4,gs,o,pal,0);
                        guniqhudid = 0;
                        break;
                    case 16:
                    case 17:
                    case 18:
                    case 19:
                        guniqhudid = cw;
                        G_DrawWeaponTile(64+weapon_xoffset+170-(p->look_ang>>1),looking_arc+196-gun_pos,
                            SHOTGUN+5,gs,o,pal,0);
                        guniqhudid = 0;
                        break;
                    case 20:
                    case 21:
                    case 22:
                    case 23:
                        guniqhudid = cw;
                        G_DrawWeaponTile(64+weapon_xoffset+176-(p->look_ang>>1),looking_arc+196-gun_pos,
                            SHOTGUN+6,gs,o,pal,0);
                        guniqhudid = 0;
                        break;
                    case 24:
                    case 25:
                    case 26:
                    case 27:
                        guniqhudid = cw;
                        G_DrawWeaponTile(64+weapon_xoffset+170-(p->look_ang>>1),looking_arc+196-gun_pos,
                            SHOTGUN+5,gs,o,pal,0);
                        guniqhudid = 0;
                        break;
                    case 28:
                    case 29:
                    case 30:
                        guniqhudid = cw;
                        G_DrawWeaponTile(32+weapon_xoffset+156-(p->look_ang>>1),looking_arc+206-gun_pos,
                            SHOTGUN+4,gs,o,pal,0);
                        guniqhudid = 0;
                        break;
                    }
                }
                break;

            case CHAINGUN_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    if (*kb > 0)
                    {
                        gun_pos -= sintable[(*kb)<<7]>>12;

                        if (sprite[p->i].pal != 1)
                            weapon_xoffset += 1-(rand()&3);
                    }

                    G_DrawWeaponTile(weapon_xoffset+168-(p->look_ang>>1),looking_arc+260-gun_pos,
                        CHAINGUN,gs,o,pal,0);

                    switch (*kb)
                    {
                    case 0:
                        G_DrawWeaponTile(weapon_xoffset+178-(p->look_ang>>1),looking_arc+233-gun_pos,
                            CHAINGUN+1,gs,o,pal,0);
                        break;

                    default:
                        if (*kb > PWEAPON(0, CHAINGUN_WEAPON, FireDelay) && *kb < PWEAPON(0, CHAINGUN_WEAPON, TotalTime))
                        {
                            i = 0;
                            if (sprite[p->i].pal != 1) i = rand()&7;
                            G_DrawWeaponTile(i+weapon_xoffset-4+140-(p->look_ang>>1),i+looking_arc-((*kb)>>1)+208-gun_pos,
                                CHAINGUN+5+((*kb-4)/5),gs,o,pal,0);
                            if (sprite[p->i].pal != 1) i = rand()&7;
                            G_DrawWeaponTile(i+weapon_xoffset-4+184-(p->look_ang>>1),i+looking_arc-((*kb)>>1)+208-gun_pos,
                                CHAINGUN+5+((*kb-4)/5),gs,o,pal,0);
                        }

                        if (*kb < PWEAPON(0, CHAINGUN_WEAPON, TotalTime)-4)
                        {
                            i = rand()&7;
                            G_DrawWeaponTile(i+weapon_xoffset-4+162-(p->look_ang>>1),i+looking_arc-((*kb)>>1)+208-gun_pos,
                                CHAINGUN+5+((*kb-2)/5),gs,o,pal,0);
                            G_DrawWeaponTile(weapon_xoffset+178-(p->look_ang>>1),looking_arc+233-gun_pos,
                                CHAINGUN+1+((*kb)>>1),gs,o,pal,0);
                        }
                        else G_DrawWeaponTile(weapon_xoffset+178-(p->look_ang>>1),looking_arc+233-gun_pos,
                            CHAINGUN+1,gs,o,pal,0);

                        break;
                    }
                }
                break;

            case PISTOL_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    if ((*kb) < PWEAPON(0, PISTOL_WEAPON, TotalTime)+1)
                    {
                        static uint8_t kb_frames[] = { 0, 1, 2 };
                        int32_t l = 195-12+weapon_xoffset;

                        if ((*kb) == PWEAPON(0, PISTOL_WEAPON, FireDelay))
                            l -= 3;

                        guniqhudid = cw;
                        G_DrawWeaponTile((l-(p->look_ang>>1)),(looking_arc+244-gun_pos),FIRSTGUN+kb_frames[*kb>2?0:*kb],gs,2,pal,0);
                        guniqhudid = 0;
                    }
                    else
                    {

                        if ((*kb) < PWEAPON(0, PISTOL_WEAPON, Reload)-17)
                        {
                            guniqhudid = cw;
                            G_DrawWeaponTile(194-(p->look_ang>>1),looking_arc+230-gun_pos,FIRSTGUN+4,gs,o|512,pal,0);
                            guniqhudid = 0;
                        }
                        else if ((*kb) < PWEAPON(0, PISTOL_WEAPON, Reload)-12)
                        {
                            G_DrawWeaponTile(244-((*kb)<<3)-(p->look_ang>>1),looking_arc+130-gun_pos+((*kb)<<4),FIRSTGUN+6,gs,o|512,pal,0);
                            guniqhudid = cw;
                            G_DrawWeaponTile(224-(p->look_ang>>1),looking_arc+220-gun_pos,FIRSTGUN+5,gs,o|512,pal,0);
                            guniqhudid = 0;
                        }
                        else if ((*kb) < PWEAPON(0, PISTOL_WEAPON, Reload)-7)
                        {
                            G_DrawWeaponTile(124+((*kb)<<1)-(p->look_ang>>1),looking_arc+430-gun_pos-((*kb)<<3),FIRSTGUN+6,gs,o|512,pal,0);
                            guniqhudid = cw;
                            G_DrawWeaponTile(224-(p->look_ang>>1),looking_arc+220-gun_pos,FIRSTGUN+5,gs,o|512,pal,0);
                            guniqhudid = 0;
                        }

                        else if ((*kb) < PWEAPON(0, PISTOL_WEAPON, Reload)-4)
                        {
                            G_DrawWeaponTile(184-(p->look_ang>>1),looking_arc+235-gun_pos,FIRSTGUN+8,gs,o|512,pal,0);
                            guniqhudid = cw;
                            G_DrawWeaponTile(224-(p->look_ang>>1),looking_arc+210-gun_pos,FIRSTGUN+5,gs,o|512,pal,0);
                            guniqhudid = 0;
                        }
                        else if ((*kb) < PWEAPON(0, PISTOL_WEAPON, Reload)-2)
                        {
                            G_DrawWeaponTile(164-(p->look_ang>>1),looking_arc+245-gun_pos,FIRSTGUN+8,gs,o|512,pal,0);
                            guniqhudid = cw;
                            G_DrawWeaponTile(224-(p->look_ang>>1),looking_arc+220-gun_pos,FIRSTGUN+5,gs,o|512,pal,0);
                            guniqhudid = 0;
                        }
                        else if ((*kb) < PWEAPON(0, PISTOL_WEAPON, Reload))
                        {
                            guniqhudid = cw;
                            G_DrawWeaponTile(194-(p->look_ang>>1),looking_arc+235-gun_pos,FIRSTGUN+5,gs,o|512,pal,0);
                            guniqhudid = 0;
                        }

                    }
                }
                break;

            case HANDBOMB_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    guniqhudid = cw;
                    if ((*kb))
                    {
                        if ((*kb) < (PWEAPON(0, p->curr_weapon, TotalTime)))
                        {

                            static uint8_t throw_frames[] = {0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2};

                            if ((*kb) < 7)
                                gun_pos -= 10*(*kb);        //D
                            else if ((*kb) < 12)
                                gun_pos += 20*((*kb)-10); //U
                            else if ((*kb) < 20)
                                gun_pos -= 9*((*kb)-14);  //D

                            G_DrawWeaponTile(weapon_xoffset+190-(p->look_ang>>1),looking_arc+250-gun_pos,HANDTHROW+throw_frames[(*kb)],gs,o,pal,0);
                        }
                    }
                    else
                        G_DrawWeaponTile(weapon_xoffset+190-(p->look_ang>>1),looking_arc+260-gun_pos,HANDTHROW,gs,o,pal,0);
                    guniqhudid = 0;
                }
                break;

            case HANDREMOTE_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    static uint8_t remote_frames[] = {0,1,1,2,1,1,0,0,0,0,0};

                    weapon_xoffset = -48;
                    guniqhudid = cw;
                    //                    if ((*kb))
                    G_DrawWeaponTile(weapon_xoffset+150-(p->look_ang>>1),looking_arc+258-gun_pos,HANDREMOTE+remote_frames[(*kb)],gs,o,pal,0);
                    //                    else
                    //                        G_DrawWeaponTile(weapon_xoffset+150-(p->look_ang>>1),looking_arc+258-gun_pos,HANDREMOTE,gs,o,pal,0);
                    guniqhudid = 0;
                }
                break;

            case DEVISTATOR_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    if ((*kb) < (PWEAPON(0, DEVISTATOR_WEAPON, TotalTime)+1) && (*kb) > 0)
                    {
                        static uint8_t cycloidy[] = {0,4,12,24,12,4,0};

                        i = ksgn((*kb)>>2);

                        if (p->hbomb_hold_delay)
                        {
                            guniqhudid = cw;
                            G_DrawWeaponTile((cycloidy[*kb]>>1)+weapon_xoffset+268-(p->look_ang>>1),cycloidy[*kb]+looking_arc+238-gun_pos,DEVISTATOR+i,-32,o,pal,0);
                            guniqhudid = cw<<1;
                            G_DrawWeaponTile(weapon_xoffset+30-(p->look_ang>>1),looking_arc+240-gun_pos,DEVISTATOR,gs,o|4,pal,0);
                            guniqhudid = 0;
                        }
                        else
                        {
                            guniqhudid = cw<<1;
                            G_DrawWeaponTile(-(cycloidy[*kb]>>1)+weapon_xoffset+30-(p->look_ang>>1),cycloidy[*kb]+looking_arc+240-gun_pos,DEVISTATOR+i,-32,o|4,pal,0);
                            guniqhudid = cw;
                            G_DrawWeaponTile(weapon_xoffset+268-(p->look_ang>>1),looking_arc+238-gun_pos,DEVISTATOR,gs,o,pal,0);
                            guniqhudid = 0;
                        }
                    }
                    else
                    {
                        guniqhudid = cw;
                        G_DrawWeaponTile(weapon_xoffset+268-(p->look_ang>>1),looking_arc+238-gun_pos,DEVISTATOR,gs,o,pal,0);
                        guniqhudid = cw<<1;
                        G_DrawWeaponTile(weapon_xoffset+30-(p->look_ang>>1),looking_arc+240-gun_pos,DEVISTATOR,gs,o|4,pal,0);
                        guniqhudid = 0;
                    }
                }
                break;

            case FREEZE_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    if ((*kb) < (PWEAPON(snum, p->curr_weapon, TotalTime)+1) && (*kb) > 0)
                    {
                        static uint8_t cat_frames[] = { 0,0,1,1,2,2 };

                        if (sprite[p->i].pal != 1)
                        {
                            weapon_xoffset += rand()&3;
                            looking_arc += rand()&3;
                        }
                        gun_pos -= 16;
                        guniqhudid = 0;
                        G_DrawWeaponTile(weapon_xoffset+210-(p->look_ang>>1),looking_arc+261-gun_pos,FREEZE+2,-32,o|512,pal,0);
                        guniqhudid = cw;
                        G_DrawWeaponTile(weapon_xoffset+210-(p->look_ang>>1),looking_arc+235-gun_pos,FREEZE+3+cat_frames[*kb%6],-32,o|512,pal,0);
                        guniqhudid = 0;
                    }
                    else
                    {
                        guniqhudid = cw;
                        G_DrawWeaponTile(weapon_xoffset+210-(p->look_ang>>1),looking_arc+261-gun_pos,FREEZE,gs,o|512,pal,0);
                        guniqhudid = 0;
                    }
                }
                break;

            case GROW_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    weapon_xoffset += 28;
                    looking_arc += 18;

                    if ((*kb) < PWEAPON(snum, p->curr_weapon, TotalTime) && (*kb) > 0)
                    {
                        if (sprite[p->i].pal != 1)
                        {
                            weapon_xoffset += rand()&3;
                            gun_pos += (rand()&3);
                        }

                        guniqhudid = cw<<1;
                        G_DrawWeaponTile(weapon_xoffset+184-(p->look_ang>>1),
                                         looking_arc+240-gun_pos,SHRINKER+3+((*kb)&3),-32,
                                         o,2,1);

                        guniqhudid = cw;
                        G_DrawWeaponTile(weapon_xoffset+188-(p->look_ang>>1),
                                         looking_arc+240-gun_pos,SHRINKER-1,gs,o,pal,0);
                        guniqhudid = 0;
                    }
                    else
                    {
                        guniqhudid = cw<<1;
                        G_DrawWeaponTile(weapon_xoffset+184-(p->look_ang>>1),
                                         looking_arc+240-gun_pos,SHRINKER+2,
                                         16-(sintable[p->random_club_frame&2047]>>10),
                                         o,2,1);

                        guniqhudid = cw;
                        G_DrawWeaponTile(weapon_xoffset+188-(p->look_ang>>1),
                                         looking_arc+240-gun_pos,SHRINKER-2,gs,o,pal,0);
                        guniqhudid = 0;
                    }
                }
                break;

            case SHRINKER_WEAPON:
                if (VM_OnEvent(EVENT_DRAWWEAPON,g_player[screenpeek].ps->i,screenpeek, -1, 0) == 0)
                {
                    weapon_xoffset += 28;
                    looking_arc += 18;

                    if (((*kb) > 0) && ((*kb) < PWEAPON(snum, p->curr_weapon, TotalTime)))
                    {
                        if (sprite[p->i].pal != 1)
                        {
                            weapon_xoffset += rand()&3;
                            gun_pos += (rand()&3);
                        }
                        guniqhudid = cw<<1;
                        G_DrawWeaponTile(weapon_xoffset+184-(p->look_ang>>1),
                            looking_arc+240-gun_pos,SHRINKER+3+((*kb)&3),-32,
                            o,0,1);
                        guniqhudid = cw;
                        G_DrawWeaponTile(weapon_xoffset+188-(p->look_ang>>1),
                            looking_arc+240-gun_pos,SHRINKER+1,gs,o,pal,0);
                        guniqhudid = 0;

                    }
                    else
                    {
                        guniqhudid = cw<<1;
                        G_DrawWeaponTile(weapon_xoffset+184-(p->look_ang>>1),
                            looking_arc+240-gun_pos,SHRINKER+2,
                            16-(sintable[p->random_club_frame&2047]>>10),
                            o,0,1);
                        guniqhudid = cw;
                        G_DrawWeaponTile(weapon_xoffset+188-(p->look_ang>>1),
                            looking_arc+240-gun_pos,SHRINKER,gs,o,pal,0);
                        guniqhudid = 0;
                    }
                }
                break;

            }
        }
    }

    P_DisplaySpit(snum);
}

#define TURBOTURNTIME (TICRATE/8) // 7
#define NORMALTURN   15
#define PREAMBLETURN 5
#define NORMALKEYMOVE 40
#define MAXVEL       ((NORMALKEYMOVE*2)+10)
#define MAXSVEL      ((NORMALKEYMOVE*2)+10)
#define MAXANGVEL    127
#define MAXHORIZ     127

int32_t g_myAimMode = 0, g_myAimStat = 0, g_oldAimStat = 0;
int32_t mouseyaxismode = -1;
int32_t g_emuJumpTics = 0;

void getinput(int32_t snum)
{
    int32_t j, daang;
    static ControlInfo info[2];
    static int32_t turnheldtime; //MED
    static int32_t lastcontroltime; //MED

    int32_t tics, running;
    int32_t turnamount;
    int32_t keymove;
    int32_t momx = 0,momy = 0;
    DukePlayer_t *p = g_player[snum].ps;

    if ((p->gm & (MODE_MENU|MODE_TYPE)) || (ud.pause_on && !KB_KeyPressed(sc_Pause)))
    {
        if (!(p->gm&MODE_MENU))
            CONTROL_GetInput(&info[0]);

        Bmemset(&info[1], 0, sizeof(input_t));
        Bmemset(&loc, 0, sizeof(input_t));
        loc.bits = (((int32_t)g_gameQuit)<<SK_GAMEQUIT);
        loc.extbits = (g_player[snum].pteam != g_player[snum].ps->team)<<6;
        loc.extbits |= (1<<7);

        return;
    }

    if (ud.mouseaiming)
        g_myAimMode = BUTTON(gamefunc_Mouse_Aiming);
    else
    {
        g_oldAimStat = g_myAimStat;
        g_myAimStat = BUTTON(gamefunc_Mouse_Aiming);
        if (g_myAimStat > g_oldAimStat)
        {
            g_myAimMode ^= 1;
            P_DoQuote(QUOTE_MOUSE_AIMING_OFF+g_myAimMode,p);
        }
    }

    if (g_myAimMode) j = analog_lookingupanddown;
    else j = ud.config.MouseAnalogueAxes[1];

    if (j != mouseyaxismode)
    {
        CONTROL_MapAnalogAxis(1, j, controldevice_mouse);
        mouseyaxismode = j;
    }

    CONTROL_GetInput(&info[0]);

    if (ud.config.MouseDeadZone)
    {
        if (info[0].dpitch > 0)
        {
            if (info[0].dpitch > ud.config.MouseDeadZone)
                info[0].dpitch -= ud.config.MouseDeadZone;
            else info[0].dpitch = 0;
        }
        else if (info[0].dpitch < 0)
        {
            if (info[0].dpitch < -ud.config.MouseDeadZone)
                info[0].dpitch += ud.config.MouseDeadZone;
            else info[0].dpitch = 0;
        }
        if (info[0].dyaw > 0)
        {
            if (info[0].dyaw > ud.config.MouseDeadZone)
                info[0].dyaw -= ud.config.MouseDeadZone;
            else info[0].dyaw = 0;
        }
        else if (info[0].dyaw < 0)
        {
            if (info[0].dyaw < -ud.config.MouseDeadZone)
                info[0].dyaw += ud.config.MouseDeadZone;
            else info[0].dyaw = 0;
        }
    }

    if (ud.config.MouseBias)
    {
        if (klabs(info[0].dyaw) > klabs(info[0].dpitch))
            info[0].dpitch /= ud.config.MouseBias;
        else info[0].dyaw /= ud.config.MouseBias;
    }

    tics = totalclock-lastcontroltime;
    lastcontroltime = totalclock;

    //    running = BUTTON(gamefunc_Run)|ud.auto_run;
    // JBF: Run key behaviour is selectable
    if (ud.runkey_mode)
        running = BUTTON(gamefunc_Run)|ud.auto_run; // classic
    else
        running = ud.auto_run^BUTTON(gamefunc_Run); // modern

    svel = vel = angvel = horiz = 0;

    if (BUTTON(gamefunc_Strafe))
    {
        svel = -(info[0].dyaw+info[1].dyaw)/8;
        info[1].dyaw = (info[1].dyaw+info[0].dyaw) % 8;
    }
    else
    {
        angvel = (info[0].dyaw+info[1].dyaw)/64;
        info[1].dyaw = (info[1].dyaw+info[0].dyaw) % 64;
    }

    if (ud.mouseflip)
        horiz = -(info[0].dpitch+info[1].dpitch)/(314-128);
    else horiz = (info[0].dpitch+info[1].dpitch)/(314-128);

    info[1].dpitch = (info[1].dpitch+info[0].dpitch) % (314-128);

    svel -= info[0].dx;
    info[1].dz = info[0].dz % (1<<6);
    vel = -info[0].dz>>6;

//     OSD_Printf("running: %d\n", running);
    if (running)
    {
        turnamount = NORMALTURN<<1;
        keymove = NORMALKEYMOVE<<1;
    }
    else
    {
        turnamount = NORMALTURN;
        keymove = NORMALKEYMOVE;
    }

    if (BUTTON(gamefunc_Strafe))
    {
        if (BUTTON(gamefunc_Turn_Left) && !(g_player[snum].ps->movement_lock&4))
            svel -= -keymove;
        if (BUTTON(gamefunc_Turn_Right) && !(g_player[snum].ps->movement_lock&8))
            svel -= keymove;
    }
    else
    {
        if (BUTTON(gamefunc_Turn_Left))
        {
            turnheldtime += tics;
            if (turnheldtime>=TURBOTURNTIME)
                angvel -= turnamount;
            else
                angvel -= PREAMBLETURN;
        }
        else if (BUTTON(gamefunc_Turn_Right))
        {
            turnheldtime += tics;
            if (turnheldtime>=TURBOTURNTIME)
                angvel += turnamount;
            else
                angvel += PREAMBLETURN;
        }
        else
            turnheldtime=0;
    }

    if (BUTTON(gamefunc_Strafe_Left) && !(g_player[snum].ps->movement_lock&4))
        svel += keymove;
    if (BUTTON(gamefunc_Strafe_Right) && !(g_player[snum].ps->movement_lock&8))
        svel += -keymove;
    if (BUTTON(gamefunc_Move_Forward) && !(g_player[snum].ps->movement_lock&1))
        vel += keymove;
    if (BUTTON(gamefunc_Move_Backward) && !(g_player[snum].ps->movement_lock&2))
        vel += -keymove;

    if (vel < -MAXVEL) vel = -MAXVEL;
    if (vel > MAXVEL) vel = MAXVEL;
    if (svel < -MAXSVEL) svel = -MAXSVEL;
    if (svel > MAXSVEL) svel = MAXSVEL;
    if (angvel < -MAXANGVEL) angvel = -MAXANGVEL;
    if (angvel > MAXANGVEL) angvel = MAXANGVEL;
    if (horiz < -MAXHORIZ) horiz = -MAXHORIZ;
    if (horiz > MAXHORIZ) horiz = MAXHORIZ;

    j=0;

    if (BUTTON(gamefunc_Weapon_1))
        j = 1;
    if (BUTTON(gamefunc_Weapon_2))
        j = 2;
    if (BUTTON(gamefunc_Weapon_3))
        j = 3;
    if (BUTTON(gamefunc_Weapon_4))
        j = 4;
    if (BUTTON(gamefunc_Weapon_5))
        j = 5;
    if (BUTTON(gamefunc_Weapon_6))
        j = 6;
    if (BUTTON(gamefunc_Weapon_7))
        j = 7;
    if (BUTTON(gamefunc_Weapon_8))
        j = 8;
    if (BUTTON(gamefunc_Weapon_9))
        j = 9;
    if (BUTTON(gamefunc_Weapon_10))
        j = 10;
    if (BUTTON(gamefunc_Previous_Weapon) || (BUTTON(gamefunc_Dpad_Select) && vel < 0))
        j = 11;
    if (BUTTON(gamefunc_Next_Weapon) || (BUTTON(gamefunc_Dpad_Select) && vel > 0))
        j = 12;

    if (BUTTON(gamefunc_Jump) && p->on_ground)
        g_emuJumpTics = 4;

    loc.bits = (g_emuJumpTics > 0 || BUTTON(gamefunc_Jump))<<SK_JUMP;

    if (g_emuJumpTics > 0)
        g_emuJumpTics--;

    loc.bits |=   BUTTON(gamefunc_Crouch)<<SK_CROUCH;
    loc.bits |=   BUTTON(gamefunc_Fire)<<SK_FIRE;
    loc.bits |= (BUTTON(gamefunc_Aim_Up) || (BUTTON(gamefunc_Dpad_Aiming) && vel > 0))<<SK_AIM_UP;
    loc.bits |= (BUTTON(gamefunc_Aim_Down) || (BUTTON(gamefunc_Dpad_Aiming) && vel < 0))<<SK_AIM_DOWN;
    if (ud.runkey_mode) loc.bits |= (ud.auto_run | BUTTON(gamefunc_Run))<<SK_RUN;
    else loc.bits |= (BUTTON(gamefunc_Run) ^ ud.auto_run)<<SK_RUN;
    loc.bits |=   BUTTON(gamefunc_Look_Left)<<SK_LOOK_LEFT;
    loc.bits |=   BUTTON(gamefunc_Look_Right)<<SK_LOOK_RIGHT;
    loc.bits |=   j<<SK_WEAPON_BITS;
    loc.bits |=   BUTTON(gamefunc_Steroids)<<SK_STEROIDS;
    loc.bits |=   BUTTON(gamefunc_Look_Up)<<SK_LOOK_UP;
    loc.bits |=   BUTTON(gamefunc_Look_Down)<<SK_LOOK_DOWN;
    loc.bits |=   BUTTON(gamefunc_NightVision)<<SK_NIGHTVISION;
    loc.bits |=   BUTTON(gamefunc_MedKit)<<SK_MEDKIT;
    loc.bits |=   BUTTON(gamefunc_Center_View)<<SK_CENTER_VIEW;
    loc.bits |=   BUTTON(gamefunc_Holster_Weapon)<<SK_HOLSTER;
    loc.bits |= (BUTTON(gamefunc_Inventory_Left) || (BUTTON(gamefunc_Dpad_Select) && (svel > 0 || angvel < 0))) <<SK_INV_LEFT;
    loc.bits |=   KB_KeyPressed(sc_Pause)<<SK_PAUSE;
    loc.bits |=   BUTTON(gamefunc_Quick_Kick)<<SK_QUICK_KICK;
    loc.bits |=   g_myAimMode<<SK_AIMMODE;
    loc.bits |=   BUTTON(gamefunc_Holo_Duke)<<SK_HOLODUKE;
    loc.bits |=   BUTTON(gamefunc_Jetpack)<<SK_JETPACK;
    loc.bits |= (((int32_t)g_gameQuit)<<SK_GAMEQUIT);
    loc.bits |= (BUTTON(gamefunc_Inventory_Right) || (BUTTON(gamefunc_Dpad_Select) && (svel < 0 || angvel > 0))) <<SK_INV_RIGHT;
    loc.bits |=   BUTTON(gamefunc_TurnAround)<<SK_TURNAROUND;
    loc.bits |=   BUTTON(gamefunc_Open)<<SK_OPEN;
    loc.bits |=   BUTTON(gamefunc_Inventory)<<SK_INVENTORY;
    loc.bits |=   KB_KeyPressed(sc_Escape)<<SK_ESCAPE;

    if (BUTTON(gamefunc_Dpad_Select))
        vel = svel = angvel = 0;

    if (BUTTON(gamefunc_Dpad_Aiming))
        vel = 0;

    if (PWEAPON(snum, g_player[snum].ps->curr_weapon, Flags) & WEAPON_SEMIAUTO && BUTTON(gamefunc_Fire))
        CONTROL_ClearButton(gamefunc_Fire);

    loc.extbits = 0;
    loc.extbits |= (BUTTON(gamefunc_Move_Forward) || (vel > 0));
    loc.extbits |= (BUTTON(gamefunc_Move_Backward) || (vel < 0))<<1;
    loc.extbits |= (BUTTON(gamefunc_Strafe_Left) || (svel > 0))<<2;
    loc.extbits |= (BUTTON(gamefunc_Strafe_Right) || (svel < 0))<<3;

    if (G_HaveEvent(EVENT_PROCESSINPUT) || G_HaveEvent(EVENT_TURNLEFT))
        loc.extbits |= BUTTON(gamefunc_Turn_Left)<<4;

    if (G_HaveEvent(EVENT_PROCESSINPUT) || G_HaveEvent(EVENT_TURNRIGHT))
        loc.extbits |= BUTTON(gamefunc_Turn_Right)<<5;

    // used for changing team
    loc.extbits |= (g_player[snum].pteam != g_player[snum].ps->team)<<6;

    if (ud.scrollmode && ud.overhead_on)
    {
        ud.folfvel = vel;
        ud.folavel = angvel;
        loc.fvel = loc.svel = loc.avel = loc.horz = 0;
        return;
    }

    daang = p->ang;

    momx = mulscale9(vel,sintable[(daang+2560)&2047]);
    momy = mulscale9(vel,sintable[(daang+2048)&2047]);

    momx += mulscale9(svel,sintable[(daang+2048)&2047]);
    momy += mulscale9(svel,sintable[(daang+1536)&2047]);

    momx += fricxv;
    momy += fricyv;

    loc.fvel = momx;
    loc.svel = momy;

    loc.avel = angvel;
    loc.horz = horiz;
}

static int32_t P_DoCounters(DukePlayer_t *p)
{
    int32_t snum = sprite[p->i].yvel;

//        j = g_player[snum].sync->avel;
//        p->weapon_ang = -(j/5);

    if (snum < 0) return 1;

    if (p->invdisptime > 0)
        p->invdisptime--;

    if (p->tipincs > 0) p->tipincs--;

    if (p->last_pissed_time > 0)
    {
        switch (--p->last_pissed_time)
        {
        case GAMETICSPERSEC*219:
            {
                A_PlaySound(FLUSH_TOILET,p->i);
                if (snum == screenpeek || GTFLAGS(GAMETYPE_COOPSOUND))
                    A_PlaySound(DUKE_PISSRELIEF,p->i);
            }
            break;
        case GAMETICSPERSEC*218:
            {
                p->holster_weapon = 0;
                p->weapon_pos = 10;
            }
            break;
        }
    }

    if (p->crack_time > 0)
    {
        if (--p->crack_time == 0)
        {
            p->knuckle_incs = 1;
            p->crack_time = 777;
        }
    }

    if (p->inv_amount[GET_STEROIDS] > 0 && p->inv_amount[GET_STEROIDS] < 400)
    {
        if (--p->inv_amount[GET_STEROIDS] == 0)
            P_SelectNextInvItem(p);

        if (!(p->inv_amount[GET_STEROIDS]&7))
            if (snum == screenpeek || GTFLAGS(GAMETYPE_COOPSOUND))
                A_PlaySound(DUKE_HARTBEAT,p->i);
    }

    if (p->heat_on && p->inv_amount[GET_HEATS] > 0)
    {
        if (--p->inv_amount[GET_HEATS] == 0)
        {
            p->heat_on = 0;
            P_SelectNextInvItem(p);
            A_PlaySound(NITEVISION_ONOFF,p->i);
            P_UpdateScreenPal(p);
        }
    }

    if (p->holoduke_on >= 0)
    {
        if (--p->inv_amount[GET_HOLODUKE] <= 0)
        {
            A_PlaySound(TELEPORTER,p->i);
            p->holoduke_on = -1;
            P_SelectNextInvItem(p);
        }
    }

    if (p->jetpack_on && p->inv_amount[GET_JETPACK] > 0)
    {
        if (--p->inv_amount[GET_JETPACK] <= 0)
        {
            p->jetpack_on = 0;
            P_SelectNextInvItem(p);
            A_PlaySound(DUKE_JETPACK_OFF,p->i);
            S_StopEnvSound(DUKE_JETPACK_IDLE,p->i);
            S_StopEnvSound(DUKE_JETPACK_ON,p->i);
        }
    }

    if (p->quick_kick > 0 && sprite[p->i].pal != 1)
    {
        p->last_quick_kick = p->quick_kick+1;

        if (--p->quick_kick == 8)
            A_Shoot(p->i,KNEE);
    }
    else if (p->last_quick_kick > 0) p->last_quick_kick--;

    if (p->access_incs && sprite[p->i].pal != 1)
    {
        p->access_incs++;
        if (sprite[p->i].extra <= 0)
            p->access_incs = 12;

        if (p->access_incs == 12)
        {
            if (p->access_spritenum >= 0)
            {
                P_ActivateSwitch(snum,p->access_spritenum,1);
                switch (sprite[p->access_spritenum].pal)
                {
                case 0:
                    p->got_access &= (0xffff-0x1);
                    break;
                case 21:
                    p->got_access &= (0xffff-0x2);
                    break;
                case 23:
                    p->got_access &= (0xffff-0x4);
                    break;
                }
                p->access_spritenum = -1;
            }
            else
            {
                P_ActivateSwitch(snum,p->access_wallnum,0);
                switch (wall[p->access_wallnum].pal)
                {
                case 0:
                    p->got_access &= (0xffff-0x1);
                    break;
                case 21:
                    p->got_access &= (0xffff-0x2);
                    break;
                case 23:
                    p->got_access &= (0xffff-0x4);
                    break;
                }
            }
        }

        if (p->access_incs > 20)
        {
            p->access_incs = 0;
            p->weapon_pos = 10;
            p->kickback_pic = 0;
        }
    }

    if (p->cursectnum >= 0 && p->scuba_on == 0 && sector[p->cursectnum].lotag == ST_2_UNDERWATER)
    {
        if (p->inv_amount[GET_SCUBA] > 0)
        {
            p->scuba_on = 1;
            p->inven_icon = ICON_SCUBA;
            P_DoQuote(QUOTE_SCUBA_ON,p);
        }
        else
        {
            if (p->airleft > 0)
                p->airleft--;
            else
            {
                p->extra_extra8 += 32;
                if (p->last_extra < (p->max_player_health>>1) && (p->last_extra&3) == 0)
                    A_PlaySound(DUKE_LONGTERM_PAIN,p->i);
            }
        }
    }
    else if (p->inv_amount[GET_SCUBA] > 0 && p->scuba_on)
    {
        p->inv_amount[GET_SCUBA]--;
        if (p->inv_amount[GET_SCUBA] == 0)
        {
            p->scuba_on = 0;
            P_SelectNextInvItem(p);
        }
    }

    if (p->knuckle_incs)
    {
        if (++p->knuckle_incs == 10)
        {
            if (totalclock > 1024)
                if (snum == screenpeek || GTFLAGS(GAMETYPE_COOPSOUND))
                {

                    if (rand()&1)
                        A_PlaySound(DUKE_CRACK,p->i);
                    else A_PlaySound(DUKE_CRACK2,p->i);

                }

            A_PlaySound(DUKE_CRACK_FIRST,p->i);

        }
        else if (p->knuckle_incs == 22 || TEST_SYNC_KEY(g_player[snum].sync->bits, SK_FIRE))
            p->knuckle_incs=0;

        return 1;
    }
    return 0;
}

int16_t WeaponPickupSprites[MAX_WEAPONS] = { KNEE__STATIC, FIRSTGUNSPRITE__STATIC, SHOTGUNSPRITE__STATIC,
        CHAINGUNSPRITE__STATIC, RPGSPRITE__STATIC, HEAVYHBOMB__STATIC, SHRINKERSPRITE__STATIC, DEVISTATORSPRITE__STATIC,
        TRIPBOMBSPRITE__STATIC, FREEZESPRITE__STATIC, HEAVYHBOMB__STATIC, SHRINKERSPRITE__STATIC
                                           };
// this is used for player deaths
void P_DropWeapon(DukePlayer_t *p)
{
    int32_t snum = sprite[p->i].yvel,
            cw = PWEAPON(snum, p->curr_weapon, WorksLike);

    if ((unsigned)cw >= MAX_WEAPONS) return;
      
    if (krand()&1)
        A_Spawn(p->i, WeaponPickupSprites[cw]);
    else switch (cw)
        {
        case RPG_WEAPON:
        case HANDBOMB_WEAPON:
            A_Spawn(p->i, EXPLOSION2);
            break;
        }
}

void P_AddAmmo(int32_t weapon,DukePlayer_t *p,int32_t amount)
{
    p->ammo_amount[weapon] += amount;

    if (p->ammo_amount[weapon] > p->max_ammo_amount[weapon])
        p->ammo_amount[weapon] = p->max_ammo_amount[weapon];
}

void P_AddWeaponNoSwitch(DukePlayer_t *p, int32_t weapon)
{
    int32_t snum = sprite[p->i].yvel;

    if ((p->gotweapon & (1<<weapon)) == 0)
    {
        p->gotweapon |= (1<<weapon);

        if (weapon == SHRINKER_WEAPON)
            p->gotweapon |= (1<<GROW_WEAPON);
    }

    if (PWEAPON(snum, p->curr_weapon, SelectSound) > 0)
        S_StopEnvSound(PWEAPON(snum, p->curr_weapon, SelectSound),p->i);

    if (PWEAPON(snum, weapon, SelectSound) > 0)
        A_PlaySound(PWEAPON(snum, weapon, SelectSound),p->i);
}

void P_ChangeWeapon(DukePlayer_t *p,int32_t weapon)
{
    int32_t i = 0, snum = sprite[p->i].yvel;
    int8_t curr_weapon = p->curr_weapon;

    if (p->reloading) return;

    if (p->curr_weapon != weapon && G_HaveEvent(EVENT_CHANGEWEAPON))
        i = VM_OnEvent(EVENT_CHANGEWEAPON,p->i, snum, -1, weapon);

    if (i == -1)
        return;
    else if (i != -2)
        p->curr_weapon = weapon;

    p->last_weapon = curr_weapon;

    p->random_club_frame = 0;

    if (p->weapon_pos == 0)
        p->weapon_pos = -1;
    else p->weapon_pos = -9;

    if (p->holster_weapon)
    {
        p->weapon_pos = 10;
        p->holster_weapon = 0;
        p->last_weapon = -1;
    }

    p->kickback_pic = 0;

    Gv_SetVar(g_iWeaponVarID, p->curr_weapon, p->i, snum);
    Gv_SetVar(g_iWorksLikeVarID,
              (unsigned)p->curr_weapon < MAX_WEAPONS ? PWEAPON(snum, p->curr_weapon, WorksLike) : -1,
              p->i, snum);
}

void P_AddWeapon(DukePlayer_t *p,int32_t weapon)
{
    P_AddWeaponNoSwitch(p, weapon);
    P_ChangeWeapon(p, weapon);
}

void P_SelectNextInvItem(DukePlayer_t *p)
{
    if (p->inv_amount[GET_FIRSTAID] > 0)
        p->inven_icon = ICON_FIRSTAID;
    else if (p->inv_amount[GET_STEROIDS] > 0)
        p->inven_icon = ICON_STEROIDS;
    else if (p->inv_amount[GET_JETPACK] > 0)
        p->inven_icon = ICON_JETPACK;
    else if (p->inv_amount[GET_HOLODUKE] > 0)
        p->inven_icon = ICON_HOLODUKE;
    else if (p->inv_amount[GET_HEATS] > 0)
        p->inven_icon = ICON_HEATS;
    else if (p->inv_amount[GET_SCUBA] > 0)
        p->inven_icon = ICON_SCUBA;
    else if (p->inv_amount[GET_BOOTS] > 0)
        p->inven_icon = ICON_BOOTS;
    else p->inven_icon = ICON_NONE;
}

void P_CheckWeapon(DukePlayer_t *p)
{
    int32_t i, snum, weapon;

    if (p->reloading) return;

    if (p->wantweaponfire >= 0)
    {
        weapon = p->wantweaponfire;
        p->wantweaponfire = -1;

        if (weapon == p->curr_weapon) return;
        if ((p->gotweapon & (1<<weapon)) && p->ammo_amount[weapon] > 0)
        {
            P_AddWeapon(p,weapon);
            return;
        }
    }

    weapon = p->curr_weapon;

    if ((p->gotweapon & (1<<weapon)) && (p->ammo_amount[weapon] > 0 || !(p->weaponswitch & 2)))
        return;

    snum = sprite[p->i].yvel;

    for (i=0; i<10; i++)
    {
        weapon = g_player[snum].wchoice[i];
        if (VOLUMEONE && weapon > 6) continue;

        if (weapon == 0) weapon = 9;
        else weapon--;

        if (weapon == 0 || ((p->gotweapon & (1<<weapon)) && p->ammo_amount[weapon] > 0))
            break;
    }

    if (i == 10) weapon = 0;

    // Found the weapon

    P_ChangeWeapon(p, weapon);
}

void P_CheckTouchDamage(DukePlayer_t *p, int32_t obj)
{
    if ((obj = VM_OnEvent(EVENT_CHECKTOUCHDAMAGE, p->i, sprite[p->i].yvel, -1, obj)) == -1)
        return;

    if ((obj&49152) == 49152)
    {
        obj &= (MAXSPRITES-1);

        if (sprite[obj].picnum == CACTUS)
        {
            if (p->hurt_delay < 8)
            {
                sprite[p->i].extra -= 5;

                p->hurt_delay = 16;
                P_PalFrom(p, 32, 32,0,0);
                A_PlaySound(DUKE_LONGTERM_PAIN,p->i);
            }
        }
        return;
    }

    if ((obj&49152) != 32768) return;
    obj &= (MAXWALLS-1);

    if (p->hurt_delay > 0) p->hurt_delay--;
    else if (wall[obj].cstat&85)
    {
        int32_t switchpicnum = wall[obj].overpicnum;
        if (switchpicnum>W_FORCEFIELD && switchpicnum<=W_FORCEFIELD+2)
            switchpicnum=W_FORCEFIELD;

        switch (DYNAMICTILEMAP(switchpicnum))
        {
        case W_FORCEFIELD__STATIC:
            //        case W_FORCEFIELD+1:
            //        case W_FORCEFIELD+2:
            sprite[p->i].extra -= 5;

            p->hurt_delay = 16;
            P_PalFrom(p, 32, 32,0,0);

            p->vel.x = -(sintable[(p->ang+512)&2047]<<8);
            p->vel.y = -(sintable[(p->ang)&2047]<<8);
            A_PlaySound(DUKE_LONGTERM_PAIN,p->i);

            {
                vec3_t davect;

                davect.x = p->pos.x+(sintable[(p->ang+512)&2047]>>9);
                davect.y = p->pos.y+(sintable[p->ang&2047]>>9);
                davect.z = p->pos.z;
                A_DamageWall(p->i,obj,&davect,-1);
            }

            break;

        case BIGFORCE__STATIC:
            p->hurt_delay = GAMETICSPERSEC;
            {
                vec3_t davect;

                davect.x = p->pos.x+(sintable[(p->ang+512)&2047]>>9);
                davect.y = p->pos.y+(sintable[p->ang&2047]>>9);
                davect.z = p->pos.z;
                A_DamageWall(p->i,obj,&davect,-1);
            }
            break;

        }
    }
}

int32_t P_CheckFloorDamage(DukePlayer_t *p, int32_t tex)
{
    spritetype *s = &sprite[p->i];

    if ((unsigned)(tex = VM_OnEvent(EVENT_CHECKFLOORDAMAGE, p->i, sprite[p->i].yvel, -1, tex)) >= MAXTILES)
        return 0;

    switch (DYNAMICTILEMAP(tex))
    {
    case HURTRAIL__STATIC:
        if (rnd(32))
        {
            if (p->inv_amount[GET_BOOTS] > 0)
                return 1;
            else
            {
                if (!A_CheckSoundPlaying(p->i,DUKE_LONGTERM_PAIN))
                    A_PlaySound(DUKE_LONGTERM_PAIN,p->i);

                P_PalFrom(p, 32, 64,64,64);

                s->extra -= 1+(krand()&3);
                if (!A_CheckSoundPlaying(p->i,SHORT_CIRCUIT))
                    A_PlaySound(SHORT_CIRCUIT,p->i);

                return 0;
            }
        }
        break;
    case FLOORSLIME__STATIC:
        if (rnd(16))
        {
            if (p->inv_amount[GET_BOOTS] > 0)
                return 1;
            else
            {
                if (!A_CheckSoundPlaying(p->i,DUKE_LONGTERM_PAIN))
                    A_PlaySound(DUKE_LONGTERM_PAIN,p->i);

                P_PalFrom(p, 32, 0,8,0);
                s->extra -= 1+(krand()&3);

                return 0;
            }
        }
        break;
    case FLOORPLASMA__STATIC:
        if (rnd(32))
        {
            if (p->inv_amount[GET_BOOTS] > 0)
                return 1;
            else
            {
                if (!A_CheckSoundPlaying(p->i,DUKE_LONGTERM_PAIN))
                    A_PlaySound(DUKE_LONGTERM_PAIN,p->i);

                P_PalFrom(p, 32, 8,0,0);
                s->extra -= 1+(krand()&3);

                return 0;
            }
        }
        break;
    }

    return 0;
}


int32_t P_FindOtherPlayer(int32_t p,int32_t *d)
{
    int32_t j, closest_player = p;
    int32_t x, closest = INT32_MAX;

    for (TRAVERSE_CONNECT(j))
        if (p != j && sprite[g_player[j].ps->i].extra > 0)
        {
            x = klabs(g_player[j].ps->opos.x-g_player[p].ps->pos.x) +
                klabs(g_player[j].ps->opos.y-g_player[p].ps->pos.y) +
                (klabs(g_player[j].ps->opos.z-g_player[p].ps->pos.z)>>4);

            if (x < closest)
            {
                closest_player = j;
                closest = x;
            }
        }

    *d = closest;
    return closest_player;
}

void P_FragPlayer(int32_t snum)
{
    DukePlayer_t *p = g_player[snum].ps;
    spritetype *s = &sprite[p->i];

    if (g_netServer || g_netClient)
        randomseed = ticrandomseed;

    if (s->pal != 1)
    {
        P_PalFrom(p, 63, 63,0,0);

        p->pos.z -= (16<<8);
        s->z -= (16<<8);

        p->dead_flag = (512-((krand()&1)<<10)+(krand()&255)-512)&2047;
        if (p->dead_flag == 0)
            p->dead_flag++;
#ifndef NETCODE_DISABLE
        if (g_netServer)
        {
            packbuf[0] = PACKET_FRAG;
            packbuf[1] = snum;
            packbuf[2] = p->frag_ps;
            packbuf[3] = actor[p->i].picnum;
            *(int32_t *)&packbuf[4] = ticrandomseed;
            packbuf[8] = myconnectindex;

            enet_host_broadcast(g_netServer, CHAN_GAMESTATE, enet_packet_create(packbuf, 9, ENET_PACKET_FLAG_RELIABLE));
        }
#endif
    }

    p->jetpack_on = 0;
    p->holoduke_on = -1;

    S_StopEnvSound(DUKE_JETPACK_IDLE,p->i);
    if (p->scream_voice > FX_Ok)
    {
        FX_StopSound(p->scream_voice);
        S_Cleanup();
        //                S_TestSoundCallback(DUKE_SCREAM);
        p->scream_voice = -1;
    }

    if (s->pal != 1 && (s->cstat&32768) == 0) s->cstat = 0;

    if ((g_netServer || ud.multimode > 1) && (s->pal != 1 || (s->cstat&32768)))
    {
        if (p->frag_ps != snum)
        {
            if (GTFLAGS(GAMETYPE_TDM) && g_player[p->frag_ps].ps->team == g_player[snum].ps->team)
                g_player[p->frag_ps].ps->fraggedself++;
            else
            {
                g_player[p->frag_ps].ps->frag++;
                g_player[p->frag_ps].frags[snum]++;
                g_player[snum].frags[snum]++; // deaths
            }

            if (snum == screenpeek)
            {
                Bsprintf(ScriptQuotes[QUOTE_RESERVED],"Killed by %s",&g_player[p->frag_ps].user_name[0]);
                P_DoQuote(QUOTE_RESERVED,p);
            }
            else
            {
                Bsprintf(ScriptQuotes[QUOTE_RESERVED2],"Killed %s",&g_player[snum].user_name[0]);
                P_DoQuote(QUOTE_RESERVED2,g_player[p->frag_ps].ps);
            }

            if (ud.obituaries)
            {
                Bsprintf(tempbuf,ScriptQuotes[OBITQUOTEINDEX+(krand()%g_numObituaries)],
                         &g_player[p->frag_ps].user_name[0],
                         &g_player[snum].user_name[0]);
                G_AddUserQuote(tempbuf);
            }
            else krand();
        }
        else
        {
            if (actor[p->i].picnum != APLAYERTOP)
            {
                p->fraggedself++;
                if (A_CheckEnemyTile(sprite[p->wackedbyactor].picnum))
                    Bsprintf(tempbuf,ScriptQuotes[OBITQUOTEINDEX+(krand()%g_numObituaries)],"A monster",&g_player[snum].user_name[0]);
                else if (actor[p->i].picnum == NUKEBUTTON)
                    Bsprintf(tempbuf,"^02%s^02 tried to leave",&g_player[snum].user_name[0]);
                else
                {
                    // random suicide death string
                    Bsprintf(tempbuf,ScriptQuotes[SUICIDEQUOTEINDEX+(krand()%g_numSelfObituaries)],&g_player[snum].user_name[0]);
                }
            }
            else Bsprintf(tempbuf,"^02%s^02 switched to team %d",&g_player[snum].user_name[0],p->team+1);

            if (ud.obituaries)
                G_AddUserQuote(tempbuf);
        }
        p->frag_ps = snum;
        pus = NUMPAGES;
    }
}

static void P_ProcessWeapon(int32_t snum)
{
    DukePlayer_t *const p = g_player[snum].ps;
    uint8_t *const kb = &p->kickback_pic;
    const int32_t shrunk = (sprite[p->i].yrepeat < 32);
    uint32_t sb_snum = g_player[snum].sync->bits;
    int32_t i, j, k;

    switch (p->weapon_pos)
    {
    case -9:
        if (p->last_weapon >= 0)
        {
            p->weapon_pos = 10;
            p->last_weapon = -1;
        }
        else if (p->holster_weapon == 0)
            p->weapon_pos = 10;
        break;
    case 0:
        break;
    default:
        p->weapon_pos--;
        break;
    }

    if (TEST_SYNC_KEY(sb_snum, SK_FIRE))
    {
        Gv_SetVar(g_iWeaponVarID,p->curr_weapon,p->i,snum);
        Gv_SetVar(g_iWorksLikeVarID,PWEAPON(snum, p->curr_weapon, WorksLike),p->i,snum);
        
        if (VM_OnEvent(EVENT_PRESSEDFIRE, p->i, snum, -1, 0) != 0)
            sb_snum &= ~BIT(SK_FIRE);
    }

    if (TEST_SYNC_KEY(sb_snum, SK_HOLSTER))   // 'Holster Weapon
    {
        Gv_SetVar(g_iWeaponVarID,p->curr_weapon,p->i,snum);
        Gv_SetVar(g_iWorksLikeVarID,PWEAPON(snum, p->curr_weapon, WorksLike),p->i,snum);
        
        if (VM_OnEvent(EVENT_HOLSTER, p->i, snum, -1, 0) == 0)
        {
            if (PWEAPON(0, p->curr_weapon, WorksLike) != KNEE_WEAPON)
            {
                if (p->holster_weapon == 0 && p->weapon_pos == 0)
                {
                    p->holster_weapon = 1;
                    p->weapon_pos = -1;
                    P_DoQuote(QUOTE_WEAPON_LOWERED,p);
                }
                else if (p->holster_weapon == 1 && p->weapon_pos == -9)
                {
                    p->holster_weapon = 0;
                    p->weapon_pos = 10;
                    P_DoQuote(QUOTE_WEAPON_RAISED,p);
                }
            }

            if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_HOLSTER_CLEARS_CLIP)
            {
                if (p->ammo_amount[p->curr_weapon] > PWEAPON(snum, p->curr_weapon, Clip)
                        && (p->ammo_amount[p->curr_weapon] % PWEAPON(snum, p->curr_weapon, Clip)) != 0)
                {
                    p->ammo_amount[p->curr_weapon]-=
                        p->ammo_amount[p->curr_weapon] % PWEAPON(snum, p->curr_weapon, Clip) ;
                    (*kb) = PWEAPON(snum, p->curr_weapon, TotalTime);
                    sb_snum &= ~BIT(SK_FIRE); // not firing...
                }
                return;
            }
        }
    }

    if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_GLOWS)
    {
        p->random_club_frame += 64; // Glowing

        if (p->kickback_pic == 0)
        {
            spritetype *s = &sprite[p->i];
            int32_t x = ((sintable[(s->ang+512)&2047])>>7), y = ((sintable[(s->ang)&2047])>>7);
            int32_t r = 1024+(sintable[p->random_club_frame&2047]>>3);

            s->x += x;
            s->y += y;
            G_AddGameLight(0, p->i, PHEIGHT, max(r, 0), PWEAPON(snum, p->curr_weapon, FlashColor),PR_LIGHT_PRIO_HIGH_GAME);
            actor[p->i].lightcount = 2;
            s->x -= x;
            s->y -= y;
        }

    }

    // this is a hack for WEAPON_FIREEVERYOTHER
    if (actor[p->i].t_data[7])
    {
        actor[p->i].t_data[7]--;
        if (p->last_weapon == -1 && actor[p->i].t_data[7] != 0 && ((actor[p->i].t_data[7] & 1) == 0))
        {
            if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_AMMOPERSHOT)
            {
                if (p->ammo_amount[p->curr_weapon] > 0)
                    p->ammo_amount[p->curr_weapon]--;
                else
                {
                    actor[p->i].t_data[7] = 0;
                    P_CheckWeapon(p);
                }
            }

            if (actor[p->i].t_data[7] != 0)
                A_Shoot(p->i,PWEAPON(snum, p->curr_weapon, Shoots));
        }
    }

    if (p->rapid_fire_hold == 1)
    {
        if (TEST_SYNC_KEY(sb_snum, SK_FIRE)) return;
        p->rapid_fire_hold = 0;
    }

    if (shrunk || p->tipincs || p->access_incs)
        sb_snum &= ~BIT(SK_FIRE);
    else if (shrunk == 0 && (sb_snum&(1<<2)) && (*kb) == 0 && p->fist_incs == 0 &&
             p->last_weapon == -1 && (p->weapon_pos == 0 || p->holster_weapon == 1))
    {
        p->crack_time = 777;

        if (p->holster_weapon == 1)
        {
            if (p->last_pissed_time <= (GAMETICSPERSEC*218) && p->weapon_pos == -9)
            {
                p->holster_weapon = 0;
                p->weapon_pos = 10;
                P_DoQuote(QUOTE_WEAPON_RAISED,p);
            }
        }
        else
        {
            Gv_SetVar(g_iWeaponVarID,p->curr_weapon,p->i,snum);
            Gv_SetVar(g_iWorksLikeVarID,PWEAPON(snum, p->curr_weapon, WorksLike),p->i,snum);

            if (VM_OnEvent(EVENT_FIRE, p->i, snum, -1, 0) == 0)
            {
                if (G_HaveEvent(EVENT_FIREWEAPON)) // this event is deprecated
                    VM_OnEvent(EVENT_FIREWEAPON, p->i, snum, -1, 0);

                switch (PWEAPON(snum, p->curr_weapon, WorksLike))
                {
                case HANDBOMB_WEAPON:
                    p->hbomb_hold_delay = 0;
                    if (p->ammo_amount[p->curr_weapon] > 0)
                    {
                        (*kb)=1;
                        if (PWEAPON(snum, p->curr_weapon, InitialSound) > 0)
                            A_PlaySound(PWEAPON(snum, p->curr_weapon, InitialSound), p->i);
                    }
                    break;

                case HANDREMOTE_WEAPON:
                    p->hbomb_hold_delay = 0;
                    (*kb) = 1;
                    if (PWEAPON(snum, p->curr_weapon, InitialSound) > 0)
                        A_PlaySound(PWEAPON(snum, p->curr_weapon, InitialSound), p->i);
                    break;

                case SHOTGUN_WEAPON:
                    if (p->ammo_amount[p->curr_weapon] > 0 && p->random_club_frame == 0)
                    {
                        (*kb)=1;
                        if (PWEAPON(snum, p->curr_weapon, InitialSound) > 0)
                            A_PlaySound(PWEAPON(snum, p->curr_weapon, InitialSound), p->i);
                    }
                    break;

                case TRIPBOMB_WEAPON:
                    if (p->ammo_amount[p->curr_weapon] > 0)
                    {
                        hitdata_t hit;
                        hitscan((const vec3_t *)p,
                                p->cursectnum, sintable[(p->ang+512)&2047],
                                sintable[p->ang&2047], (100-p->horiz-p->horizoff)*32,
                                &hit,CLIPMASK1);

                        if (hit.sect < 0 || hit.sprite >= 0)
                            break;

                        // ST_2_UNDERWATER
                        if (hit.wall >= 0 && sector[hit.sect].lotag > 2)
                            break;

                        if (hit.wall >= 0 && wall[hit.wall].overpicnum >= 0)
                            if (wall[hit.wall].overpicnum == BIGFORCE)
                                break;

                        j = headspritesect[hit.sect];
                        while (j >= 0)
                        {
                            if (sprite[j].picnum == TRIPBOMB &&
                                    klabs(sprite[j].z-hit.pos.z) < (12<<8) &&
                                    ((sprite[j].x-hit.pos.x)*(sprite[j].x-hit.pos.x)+
                                     (sprite[j].y-hit.pos.y)*(sprite[j].y-hit.pos.y)) < (290*290))
                                break;
                            j = nextspritesect[j];
                        }

                        // ST_2_UNDERWATER
                        if (j == -1 && hit.wall >= 0 && (wall[hit.wall].cstat&16) == 0)
                            if ((wall[hit.wall].nextsector >= 0 &&
                                    sector[wall[hit.wall].nextsector].lotag <= 2) ||
                                    (wall[hit.wall].nextsector == -1 && sector[hit.sect].lotag <= 2))
                                if (((hit.pos.x-p->pos.x)*(hit.pos.x-p->pos.x) +
                                        (hit.pos.y-p->pos.y)*(hit.pos.y-p->pos.y)) < (290*290))
                                {
                                    p->pos.z = p->opos.z;
                                    p->vel.z = 0;
                                    (*kb) = 1;
                                    if (PWEAPON(snum, p->curr_weapon, InitialSound) > 0)
                                    {
                                        A_PlaySound(PWEAPON(snum, p->curr_weapon, InitialSound), p->i);
                                    }
                                }
                    }
                    break;

                case PISTOL_WEAPON:
                case CHAINGUN_WEAPON:
                case SHRINKER_WEAPON:
                case GROW_WEAPON:
                case FREEZE_WEAPON:
                case RPG_WEAPON:
                    if (p->ammo_amount[p->curr_weapon] > 0)
                    {
                        (*kb) = 1;
                        if (PWEAPON(snum, p->curr_weapon, InitialSound) > 0)
                            A_PlaySound(PWEAPON(snum, p->curr_weapon, InitialSound), p->i);
                    }
                    break;

                case DEVISTATOR_WEAPON:
                    if (p->ammo_amount[p->curr_weapon] > 0)
                    {
                        (*kb) = 1;
                        p->hbomb_hold_delay = !p->hbomb_hold_delay;
                        if (PWEAPON(snum, p->curr_weapon, InitialSound) > 0)
                            A_PlaySound(PWEAPON(snum, p->curr_weapon, InitialSound), p->i);
                    }
                    break;

                case KNEE_WEAPON:
                    if (p->quick_kick == 0)
                    {
                        (*kb) = 1;
                        if (PWEAPON(snum, p->curr_weapon, InitialSound) > 0)
                            A_PlaySound(PWEAPON(snum, p->curr_weapon, InitialSound), p->i);
                    }
                    break;
                }
            }
        }
    }
    else if (*kb)
    {
        if (PWEAPON(snum, p->curr_weapon, WorksLike) == HANDBOMB_WEAPON)
        {
            if (PWEAPON(snum, p->curr_weapon, HoldDelay) && ((*kb) == PWEAPON(snum, p->curr_weapon, FireDelay)) && TEST_SYNC_KEY(sb_snum, SK_FIRE))
            {
                p->rapid_fire_hold = 1;
                return;
            }

            if (++(*kb) == PWEAPON(snum, p->curr_weapon, HoldDelay))
            {
                int32_t lPipeBombControl;

                p->ammo_amount[p->curr_weapon]--;

                if (numplayers < 2 || g_netServer)
                {
                    if (p->on_ground && TEST_SYNC_KEY(sb_snum, SK_CROUCH))
                    {
                        k = 15;
                        i = ((p->horiz+p->horizoff-100)*20);
                    }
                    else
                    {
                        k = 140;
                        i = -512-((p->horiz+p->horizoff-100)*20);
                    }

                    j = A_InsertSprite(p->cursectnum,
                                       p->pos.x+(sintable[(p->ang+512)&2047]>>6),
                                       p->pos.y+(sintable[p->ang&2047]>>6),
                                       p->pos.z,PWEAPON(snum, p->curr_weapon, Shoots),-16,9,9,
                                       p->ang,(k+(p->hbomb_hold_delay<<5)),i,p->i,1);

                    lPipeBombControl=Gv_GetVarByLabel("PIPEBOMB_CONTROL", PIPEBOMB_REMOTE, -1, snum);

                    if (lPipeBombControl & PIPEBOMB_TIMER)
                    {
                        int32_t lv=Gv_GetVarByLabel("GRENADE_LIFETIME_VAR", NAM_GRENADE_LIFETIME_VAR, -1, snum);

                        actor[j].t_data[7]= Gv_GetVarByLabel("GRENADE_LIFETIME", NAM_GRENADE_LIFETIME, -1, snum)
                                            + mulscale(krand(),lv, 14)
                                            - lv;
                        actor[j].t_data[6]=1;
                    }
                    else actor[j].t_data[6]=2;

                    if (k == 15)
                    {
                        sprite[j].yvel = 3;
                        sprite[j].z += (8<<8);
                    }

                    if (A_GetHitscanRange(p->i) < 512)
                    {
                        sprite[j].ang += 1024;
                        sprite[j].zvel /= 3;
                        sprite[j].xvel /= 3;
                    }
                }

                p->hbomb_on = 1;
            }
            else if ((*kb) < PWEAPON(snum, p->curr_weapon, HoldDelay) && TEST_SYNC_KEY(sb_snum, SK_FIRE))
                p->hbomb_hold_delay++;
            else if ((*kb) > PWEAPON(snum, p->curr_weapon, TotalTime))
            {
                (*kb) = 0;
                p->weapon_pos = 10;

                if (Gv_GetVarByLabel("PIPEBOMB_CONTROL", PIPEBOMB_REMOTE, -1, snum) == PIPEBOMB_REMOTE)
                {
                    p->curr_weapon = HANDREMOTE_WEAPON;
                    p->last_weapon = -1;
                }
                else P_CheckWeapon(p);
            }
        }
        else if (PWEAPON(snum, p->curr_weapon, WorksLike) == HANDREMOTE_WEAPON)
        {
            if (++(*kb) == PWEAPON(snum, p->curr_weapon, FireDelay))
            {
                if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_BOMB_TRIGGER)
                    p->hbomb_on = 0;

                if (PWEAPON(snum, p->curr_weapon, Shoots) != 0)
                {
                    if (!(PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_NOVISIBLE))
                    {
                        lastvisinc = totalclock+32;
                        p->visibility = 0;
                    }
                    Gv_SetVar(g_iWeaponVarID,p->curr_weapon,p->i,snum);
                    Gv_SetVar(g_iWorksLikeVarID,PWEAPON(snum, p->curr_weapon, WorksLike), p->i, snum);
                    A_Shoot(p->i, PWEAPON(snum, p->curr_weapon, Shoots));
                }
            }

            if ((*kb) >= PWEAPON(snum, p->curr_weapon, TotalTime))
            {
                (*kb) = 0;
                if ((p->ammo_amount[HANDBOMB_WEAPON] > 0) &&
                        Gv_GetVarByLabel("PIPEBOMB_CONTROL", PIPEBOMB_REMOTE, -1, snum) == PIPEBOMB_REMOTE)
                    P_AddWeapon(p,HANDBOMB_WEAPON);
                else P_CheckWeapon(p);
            }
        }
        else
        {
            // the basic weapon...
            (*kb)++;

            if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_CHECKATRELOAD)
            {
                if (PWEAPON(snum, p->curr_weapon, WorksLike) == TRIPBOMB_WEAPON)
                {
                    if ((*kb) >= PWEAPON(snum, p->curr_weapon, TotalTime))
                    {
                        (*kb) = 0;
                        P_CheckWeapon(p);
                        p->weapon_pos = -9;
                    }
                }
                else if (*kb >= PWEAPON(snum, p->curr_weapon, Reload))
                    P_CheckWeapon(p);
            }
            else if (PWEAPON(snum, p->curr_weapon, WorksLike)!=KNEE_WEAPON && *kb >= PWEAPON(snum, p->curr_weapon, FireDelay))
                P_CheckWeapon(p);

            if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_STANDSTILL
                    && *kb < (PWEAPON(snum, p->curr_weapon, FireDelay)+1))
            {
                p->pos.z = p->opos.z;
                p->vel.z = 0;
            }

            if (*kb == PWEAPON(snum, p->curr_weapon, Sound2Time))
                if (PWEAPON(snum, p->curr_weapon, Sound2Sound) > 0)
                    A_PlaySound(PWEAPON(snum, p->curr_weapon, Sound2Sound),p->i);

            if (*kb == PWEAPON(snum, p->curr_weapon, SpawnTime))
                P_DoWeaponSpawn(p);

            if ((*kb) >= PWEAPON(snum, p->curr_weapon, TotalTime))
            {
                if (/*!(PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_CHECKATRELOAD) && */ p->reloading == 1 ||
                        (PWEAPON(snum, p->curr_weapon, Reload) > PWEAPON(snum, p->curr_weapon, TotalTime) && p->ammo_amount[p->curr_weapon] > 0
                         && (PWEAPON(snum, p->curr_weapon, Clip)) && (((p->ammo_amount[p->curr_weapon]%(PWEAPON(snum, p->curr_weapon, Clip)))==0))))
                {
                    int32_t i = PWEAPON(snum, p->curr_weapon, Reload) - PWEAPON(snum, p->curr_weapon, TotalTime);

                    p->reloading = 1;

                    if ((*kb) != (PWEAPON(snum, p->curr_weapon, TotalTime)))
                    {
                        if ((*kb) == (PWEAPON(snum, p->curr_weapon, TotalTime)+1))
                        {
                            if (PWEAPON(snum, p->curr_weapon, ReloadSound1) > 0)
                                A_PlaySound(PWEAPON(snum, p->curr_weapon, ReloadSound1),p->i);
                        }
                        else if (((*kb) == (PWEAPON(snum, p->curr_weapon, Reload) - (i/3)) &&
                                  !(PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_RELOAD_TIMING)) ||

                                 ((*kb) == (PWEAPON(snum, p->curr_weapon, Reload) - i+4) &&
                                  (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_RELOAD_TIMING)))
                        {
                            if (PWEAPON(snum, p->curr_weapon, ReloadSound2) > 0)
                                A_PlaySound(PWEAPON(snum, p->curr_weapon, ReloadSound2),p->i);
                        }
                        else if ((*kb) >= (PWEAPON(snum, p->curr_weapon, Reload)))
                        {
                            *kb=0;
                            p->reloading = 0;
                        }
                    }
                }
                else
                {
                    if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_AUTOMATIC &&
                            (PWEAPON(snum, p->curr_weapon, WorksLike)==KNEE_WEAPON?1:p->ammo_amount[p->curr_weapon] > 0))
                    {
                        if (TEST_SYNC_KEY(sb_snum, SK_FIRE))
                        {
                            if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_RANDOMRESTART)
                                *kb = 1+(krand()&3);
                            else *kb=1;
                        }
                        else *kb = 0;
                    }
                    else *kb = 0;

                    if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_RESET &&
                            ((PWEAPON(snum, p->curr_weapon, WorksLike) == KNEE_WEAPON)?1:p->ammo_amount[p->curr_weapon] > 0))
                    {
                        if (TEST_SYNC_KEY(sb_snum, SK_FIRE)) *kb = 1;
                        else *kb = 0;
                    }
                }
            }
            else if (*kb >= PWEAPON(snum, p->curr_weapon, FireDelay) && (*kb) < PWEAPON(snum, p->curr_weapon, TotalTime)
                     && ((PWEAPON(snum, p->curr_weapon, WorksLike) == KNEE_WEAPON)?1:p->ammo_amount[p->curr_weapon] > 0))
            {
                if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_AUTOMATIC)
                {
                    if (!(PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_SEMIAUTO))
                    {
                        if (TEST_SYNC_KEY(sb_snum, SK_FIRE) == 0 && PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_RESET)
                            *kb = 0;
                        if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_FIREEVERYTHIRD)
                        {
                            if (((*(kb))%3) == 0)
                            {
                                P_FireWeapon(p);
                                P_DoWeaponSpawn(p);
                            }
                        }
                        else if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_FIREEVERYOTHER)
                        {
                            P_FireWeapon(p);
                            P_DoWeaponSpawn(p);
                        }
                        else
                        {
                            if (*kb == PWEAPON(snum, p->curr_weapon, FireDelay))
                            {
                                P_FireWeapon(p);
                                //                                P_DoWeaponSpawn(p);
                            }
                        }
                        if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_RESET &&
                                (*kb) > PWEAPON(snum, p->curr_weapon, TotalTime)-PWEAPON(snum, p->curr_weapon, HoldDelay) &&
                                ((PWEAPON(snum, p->curr_weapon, WorksLike) == KNEE_WEAPON) || p->ammo_amount[p->curr_weapon] > 0))
                        {
                            if (TEST_SYNC_KEY(sb_snum, SK_FIRE)) *kb = 1;
                            else *kb = 0;
                        }
                    }
                    else
                    {
                        if (PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_FIREEVERYOTHER)
                        {
                            P_FireWeapon(p);
                            P_DoWeaponSpawn(p);
                        }
                        else
                        {
                            if (*kb == PWEAPON(snum, p->curr_weapon, FireDelay))
                            {
                                P_FireWeapon(p);
                                //                                P_DoWeaponSpawn(p);
                            }
                        }
                    }
                }
                else if (*kb == PWEAPON(snum, p->curr_weapon, FireDelay))
                    P_FireWeapon(p);
            }
        }
    }
}

int32_t P_DoFist(DukePlayer_t *p)
{
    // the fist punching NUKEBUTTON

    if (++p->fist_incs == 28)
    {
        if (ud.recstat == 1) G_CloseDemoWrite();
        S_PlaySound(PIPEBOMB_EXPLODE);

        P_PalFrom(p, 48, 64,64,64);
    }

    if (p->fist_incs > 42)
    {
        int32_t i;

        for (TRAVERSE_CONNECT(i))
            g_player[i].ps->gm = MODE_EOL;

        if (p->buttonpalette && ud.from_bonus == 0)
        {
            ud.from_bonus = ud.level_number+1;
            if (ud.secretlevel > 0 && ud.secretlevel <= MAXLEVELS)
                ud.level_number = ud.secretlevel-1;
            ud.m_level_number = ud.level_number;
        }
        else
        {
            if (ud.from_bonus)
            {
                ud.m_level_number = ud.level_number = ud.from_bonus;
                ud.from_bonus = 0;
            }
            else
            {
                if (ud.level_number == ud.secretlevel && ud.from_bonus > 0)
                    ud.level_number = ud.from_bonus;
                else ud.level_number++;

                if (ud.level_number > MAXLEVELS-1)
                    ud.level_number = 0;
                ud.m_level_number = ud.level_number;
            }
        }

        p->fist_incs = 0;

        return 1;
    }

    return 0;
}

#ifdef YAX_ENABLE
static void getzsofslope_player(int16_t sectnum, int32_t dax, int32_t day, int32_t *ceilz, int32_t *florz)
{
    int32_t i, didceil=0, didflor=0;

    if ((sector[sectnum].ceilingstat&512)==0)
    {
        i = yax_getneighborsect(dax, day, sectnum, YAX_CEILING);
        if (i >= 0)
        {
            *ceilz = getceilzofslope(i, dax,day);
            didceil = 1;
        }
    }

    if ((sector[sectnum].floorstat&512)==0)
    {
        i = yax_getneighborsect(dax, day, sectnum, YAX_FLOOR);
        if (i >= 0)
        {
            *florz = getflorzofslope(i, dax,day);
            didflor = 1;
        }
    }

    if (!didceil || !didflor)
    {
        int32_t cz, fz;
        getzsofslope(sectnum, dax, day, &cz, &fz);

        if (!didceil)
            *ceilz = cz;
        if (!didflor)
            *florz = fz;
    }
}
#endif

void P_UpdatePosWhenViewingCam(DukePlayer_t *p)
{
    int32_t i = p->newowner;

    p->pos.x = SX;
    p->pos.y = SY;
    p->pos.z = SZ;
    p->ang =  SA;
    p->vel.x = p->vel.y = sprite[p->i].xvel = 0;
    p->look_ang = 0;
    p->rotscrnang = 0;
}

void P_ProcessInput(int32_t snum)
{
    DukePlayer_t *const p = g_player[snum].ps;
    spritetype *const s = &sprite[p->i];

    uint32_t sb_snum = g_player[snum].sync->bits;

    int32_t j, i, k, doubvel = TICSPERFRAME, shrunk;
    int32_t fz, cz, hz, lz, truefdist, x, y, psectlotag;
    const uint8_t *const kb = &p->kickback_pic;
    int16_t tempsect;

    if (g_player[snum].playerquitflag == 0)
        return;

    p->player_par++;

    VM_OnEvent(EVENT_PROCESSINPUT, p->i, snum, -1, 0);

    if (p->cheat_phase > 0) sb_snum = 0;

    if (p->cursectnum == -1)
    {
        if (s->extra > 0 && ud.noclip == 0)
        {
            P_QuickKill(p);
            A_PlaySound(SQUISHED,p->i);
        }
        p->cursectnum = 0;
    }

    psectlotag = sector[p->cursectnum].lotag;
    p->spritebridge = p->sbs = 0;

    shrunk = (s->yrepeat < 32);
    getzrange((vec3_t *)p,p->cursectnum,&cz,&hz,&fz,&lz,163L,CLIPMASK0);

#ifdef YAX_ENABLE
    getzsofslope_player(p->cursectnum,p->pos.x,p->pos.y,&p->truecz,&p->truefz);
#else
    getzsofslope(p->cursectnum,p->pos.x,p->pos.y,&p->truecz,&p->truefz);
#endif
    j = p->truefz;

    truefdist = klabs(p->pos.z-j);

    if ((lz&49152) == 16384 && psectlotag == 1 && truefdist > PHEIGHT+(16<<8))
        psectlotag = 0;

    actor[p->i].floorz = fz;
    actor[p->i].ceilingz = cz;

    p->ohoriz = p->horiz;
    p->ohorizoff = p->horizoff;

    // calculates automatic view angle for playing without a mouse
    if (p->aim_mode == 0 && p->on_ground && psectlotag != ST_2_UNDERWATER && (sector[p->cursectnum].floorstat&2))
    {
        x = p->pos.x+(sintable[(p->ang+512)&2047]>>5);
        y = p->pos.y+(sintable[p->ang&2047]>>5);
        tempsect = p->cursectnum;
        updatesector(x,y,&tempsect);
        if (tempsect >= 0)
        {
            k = getflorzofslope(p->cursectnum,x,y);
            if (p->cursectnum == tempsect)
                p->horizoff += mulscale16(j-k,160);
            else if (klabs(getflorzofslope(tempsect,x,y)-k) <= (4<<8))
                p->horizoff += mulscale16(j-k,160);
        }
    }

    if (p->horizoff > 0) p->horizoff -= ((p->horizoff>>3)+1);
    else if (p->horizoff < 0) p->horizoff += (((-p->horizoff)>>3)+1);

    if (hz >= 0 && (hz&49152) == 49152)
    {
        hz &= (MAXSPRITES-1);

        if (sprite[hz].statnum == STAT_ACTOR && sprite[hz].extra >= 0)
        {
            hz = 0;
            cz = p->truecz;
        }
    }

    if (lz >= 0 && (lz&49152) == 49152)
    {
        j = lz&(MAXSPRITES-1);

        if ((sprite[j].cstat&33) == 33 || (sprite[j].cstat&17) == 17)
        {
            psectlotag = 0;
            p->footprintcount = 0;
            p->spritebridge = 1;
            p->sbs = j;
        }
        else if (A_CheckEnemySprite(&sprite[j]) && sprite[j].xrepeat > 24 && klabs(s->z-sprite[j].z) < (84<<8))
        {
            // I think this is what makes the player slide off enemies... might be a good sprite flag to add later
            j = getangle(sprite[j].x-p->pos.x,sprite[j].y-p->pos.y);
            p->vel.x -= sintable[(j+512)&2047]<<4;
            p->vel.y -= sintable[j&2047]<<4;
        }
    }

    if (s->extra > 0)
        P_IncurDamage(p);
    else
    {
        s->extra = 0;
        p->inv_amount[GET_SHIELD] = 0;
    }

    p->last_extra = s->extra;

    if (p->loogcnt > 0) p->loogcnt--;
    else p->loogcnt = 0;

    if (p->fist_incs && P_DoFist(p)) return;

    if (p->timebeforeexit > 1 && p->last_extra > 0)
    {
        if (--p->timebeforeexit == GAMETICSPERSEC*5)
        {
            FX_StopAllSounds();
            S_ClearSoundLocks();

            if (p->customexitsound >= 0)
            {
                S_PlaySound(p->customexitsound);
                P_DoQuote(QUOTE_WEREGONNAFRYYOURASS,p);
            }
        }
        else if (p->timebeforeexit == 1)
        {
            for (TRAVERSE_CONNECT(i))
                g_player[i].ps->gm = MODE_EOL;

            ud.m_level_number = ud.level_number++;

            if (ud.from_bonus)
            {
                ud.m_level_number = ud.level_number = ud.from_bonus;
                ud.from_bonus = 0;
            }
            return;
        }
    }

    if (p->pals.f > 0)
        p->pals.f--;

    if (p->fta > 0 && --p->fta == 0)
    {
        pub = pus = NUMPAGES;
        p->ftq = 0;
    }

    if (g_levelTextTime > 0)
        g_levelTextTime--;

    if (s->extra <= 0)
    {
        if (ud.recstat == 1 && (!g_netServer && ud.multimode < 2))
            G_CloseDemoWrite();

        if ((numplayers < 2 || g_netServer) && p->dead_flag == 0)
            P_FragPlayer(snum);

        if (psectlotag == ST_2_UNDERWATER)
        {
            if (p->on_warping_sector == 0)
            {
                if (klabs(p->pos.z-fz) > (PHEIGHT>>1))
                    p->pos.z += 348;
            }
            else
            {
                s->z -= 512;
                s->zvel = -348;
            }

            clipmove((vec3_t *)p,&p->cursectnum,
                     0,0,164L,(4L<<8),(4L<<8),CLIPMASK0);
            //                        p->bobcounter += 32;
        }

        Bmemcpy(&p->opos.x, &p->pos.x, sizeof(vec3_t));
        p->oang = p->ang;
        p->opyoff = p->pyoff;

        p->horiz = 100;
        p->horizoff = 0;

        updatesector(p->pos.x,p->pos.y,&p->cursectnum);

        pushmove((vec3_t *)p,&p->cursectnum,128L,(4L<<8),(20L<<8),CLIPMASK0);

        if (fz > cz+(16<<8) && s->pal != 1)
            p->rotscrnang = (p->dead_flag + ((fz+p->pos.z)>>7))&2047;

        p->on_warping_sector = 0;

        return;
    }

    if (p->transporter_hold > 0)
    {
        p->transporter_hold--;
        if (p->transporter_hold == 0 && p->on_warping_sector)
            p->transporter_hold = 2;
    }
    else if (p->transporter_hold < 0)
        p->transporter_hold++;

    if (p->newowner >= 0)
    {
        P_UpdatePosWhenViewingCam(p);
        P_DoCounters(p);

        if (PWEAPON(0, p->curr_weapon, WorksLike) == HANDREMOTE_WEAPON)
            P_ProcessWeapon(snum);

        return;
    }

    p->rotscrnang -= (p->rotscrnang>>1);

    if (p->rotscrnang && !(p->rotscrnang>>1))
        p->rotscrnang -= ksgn(p->rotscrnang);

    p->look_ang -= (p->look_ang>>2);

    if (p->look_ang && !(p->look_ang>>2))
        p->look_ang -= ksgn(p->look_ang);

    if (TEST_SYNC_KEY(sb_snum, SK_LOOK_LEFT))
    {
        // look_left
        if (VM_OnEvent(EVENT_LOOKLEFT,p->i,snum, -1, 0) == 0)
        {
            p->look_ang -= 152;
            p->rotscrnang += 24;
        }
    }

    if (TEST_SYNC_KEY(sb_snum, SK_LOOK_RIGHT))
    {
        // look_right
        if (VM_OnEvent(EVENT_LOOKRIGHT,p->i,snum, -1, 0) == 0)
        {
            p->look_ang += 152;
            p->rotscrnang -= 24;
        }
    }

    if (p->on_crane >= 0)
        goto HORIZONLY;

    j = ksgn(g_player[snum].sync->avel);

    if (s->xvel < 32 || p->on_ground == 0 || p->bobcounter == 1024)
    {
        if ((p->weapon_sway&2047) > (1024+96))
            p->weapon_sway -= 96;
        else if ((p->weapon_sway&2047) < (1024-96))
            p->weapon_sway += 96;
        else p->weapon_sway = 1024;
    }
    else p->weapon_sway = p->bobcounter;

    // NOTE: This silently wraps if the difference is too great, e.g. used to do
    // that when teleported by silent SE7s.
    s->xvel = ksqrt(uhypsq(p->pos.x-p->bobposx, p->pos.y-p->bobposy));

    if (p->on_ground)
        p->bobcounter += sprite[p->i].xvel>>1;

    if (ud.noclip == 0 && ((uint16_t)p->cursectnum >= MAXSECTORS || sector[p->cursectnum].floorpicnum == MIRROR))
    {
        p->pos.x = p->opos.x;
        p->pos.y = p->opos.y;
    }
    else
    {
        p->opos.x = p->pos.x;
        p->opos.y = p->pos.y;
    }

    p->bobposx = p->pos.x;
    p->bobposy = p->pos.y;

    p->opos.z = p->pos.z;
    p->opyoff = p->pyoff;
    p->oang = p->ang;

    if (p->one_eighty_count < 0)
    {
        p->one_eighty_count += 128;
        p->ang += 128;
    }

    // Shrinking code

    i = 40;

    if (psectlotag == ST_2_UNDERWATER)
    {
        // under water
        p->jumping_counter = 0;

        p->pycount += 32;
        p->pycount &= 2047;
        p->pyoff = sintable[p->pycount]>>7;

        if (!A_CheckSoundPlaying(p->i,DUKE_UNDERWATER))
            A_PlaySound(DUKE_UNDERWATER,p->i);

        if (TEST_SYNC_KEY(sb_snum, SK_JUMP))
        {
            if (VM_OnEvent(EVENT_SWIMUP,p->i,snum, -1, 0) == 0)
            {
                // jump
                if (p->vel.z > 0) p->vel.z = 0;
                p->vel.z -= 348;
                if (p->vel.z < -(256*6)) p->vel.z = -(256*6);
            }
        }
        else if (TEST_SYNC_KEY(sb_snum, SK_CROUCH))
        {
            if (VM_OnEvent(EVENT_SWIMDOWN,p->i,snum, -1, 0) == 0)
            {
                // crouch
                if (p->vel.z < 0) p->vel.z = 0;
                p->vel.z += 348;
                if (p->vel.z > (256*6)) p->vel.z = (256*6);
            }
        }
        else
        {
            // normal view
            if (p->vel.z < 0)
            {
                p->vel.z += 256;
                if (p->vel.z > 0)
                    p->vel.z = 0;
            }
            if (p->vel.z > 0)
            {
                p->vel.z -= 256;
                if (p->vel.z < 0)
                    p->vel.z = 0;
            }
        }

        if (p->vel.z > 2048)
            p->vel.z >>= 1;

        p->pos.z += p->vel.z;

        if (p->pos.z > (fz-(15<<8)))
            p->pos.z += ((fz-(15<<8))-p->pos.z)>>1;

        if (p->pos.z < cz)
        {
            p->pos.z = cz;
            p->vel.z = 0;
        }

        if (p->scuba_on && (krand()&255) < 8)
        {
            j = A_Spawn(p->i,WATERBUBBLE);
            sprite[j].x +=
                sintable[(p->ang+512+64-(g_globalRandom&128))&2047]>>6;
            sprite[j].y +=
                sintable[(p->ang+64-(g_globalRandom&128))&2047]>>6;
            sprite[j].xrepeat = 3;
            sprite[j].yrepeat = 2;
            sprite[j].z = p->pos.z+(8<<8);
        }
    }
    else if (p->jetpack_on)
    {
        p->on_ground = 0;
        p->jumping_counter = 0;
        p->hard_landing = 0;
        p->falling_counter = 0;

        p->pycount += 32;
        p->pycount &= 2047;
        p->pyoff = sintable[p->pycount]>>7;

        if (p->jetpack_on < 11)
        {
            p->jetpack_on++;
            p->pos.z -= (p->jetpack_on<<7); //Goin up
        }
        else if (p->jetpack_on == 11 && !A_CheckSoundPlaying(p->i,DUKE_JETPACK_IDLE))
            A_PlaySound(DUKE_JETPACK_IDLE,p->i);

        if (shrunk) j = 512;
        else j = 2048;

        if (TEST_SYNC_KEY(sb_snum, SK_JUMP))         //A (soar high)
        {
            // jump
            if (VM_OnEvent(EVENT_SOARUP,p->i,snum, -1, 0) == 0)
            {
                p->pos.z -= j;
                p->crack_time = 777;
            }
        }

        if (TEST_SYNC_KEY(sb_snum, SK_CROUCH))   //Z (soar low)
        {
            // crouch
            if (VM_OnEvent(EVENT_SOARDOWN,p->i,snum, -1, 0) == 0)
            {
                p->pos.z += j;
                p->crack_time = 777;
            }
        }

        if (shrunk == 0 && (psectlotag == 0 || psectlotag == ST_2_UNDERWATER)) k = 32;
        else k = 16;

        if (psectlotag != ST_2_UNDERWATER && p->scuba_on == 1)
            p->scuba_on = 0;

        if (p->pos.z > (fz-(k<<8)))
            p->pos.z += ((fz-(k<<8))-p->pos.z)>>1;
        if (p->pos.z < (actor[p->i].ceilingz+(18<<8)))
            p->pos.z = actor[p->i].ceilingz+(18<<8);
    }
    else if (psectlotag != ST_2_UNDERWATER)
    {
        p->airleft = 15 * GAMETICSPERSEC; // 13 seconds

        if (p->scuba_on == 1)
            p->scuba_on = 0;

        if (psectlotag == ST_1_ABOVE_WATER && p->spritebridge == 0)
        {
            if (shrunk == 0)
            {
                i = 34;
                p->pycount += 32;
                p->pycount &= 2047;
                p->pyoff = sintable[p->pycount]>>6;
            }
            else i = 12;

            if (shrunk == 0 && truefdist <= PHEIGHT)
            {
                if (p->on_ground == 1)
                {
                    if (p->dummyplayersprite == -1)
                        p->dummyplayersprite =
                            A_Spawn(p->i,PLAYERONWATER);
                    sprite[p->dummyplayersprite].pal = sprite[p->i].pal;
                    sprite[p->dummyplayersprite].cstat |= 32768;

                    p->footprintcount = 6;
                    if (sector[p->cursectnum].floorpicnum == FLOORSLIME)
                        p->footprintpal = 8;
                    else p->footprintpal = 0;
                    p->footprintshade = 0;
                }
            }
        }
        else
        {
            if (p->footprintcount > 0 && p->on_ground)
                if (p->cursectnum >= 0 && (sector[p->cursectnum].floorstat&2) != 2)
                {
                    for (j=headspritesect[p->cursectnum]; j>=0; j=nextspritesect[j])
                        if (sprite[j].picnum == FOOTPRINTS || sprite[j].picnum == FOOTPRINTS2 ||
                                sprite[j].picnum == FOOTPRINTS3 || sprite[j].picnum == FOOTPRINTS4)
                            if (klabs(sprite[j].x-p->pos.x) < 384 && klabs(sprite[j].y-p->pos.y) < 384)
                                break;

                    if (j < 0)
                    {
                        if (p->cursectnum >= 0 && sector[p->cursectnum].lotag == 0 && sector[p->cursectnum].hitag == 0)
#ifdef YAX_ENABLE
                            if (yax_getbunch(p->cursectnum, YAX_FLOOR) < 0 || (sector[p->cursectnum].floorstat&512))
#endif
                        {
                            switch (krand()&3)
                            {
                            case 0:
                                j = A_Spawn(p->i,FOOTPRINTS);
                                break;
                            case 1:
                                j = A_Spawn(p->i,FOOTPRINTS2);
                                break;
                            case 2:
                                j = A_Spawn(p->i,FOOTPRINTS3);
                                break;
                            default:
                                j = A_Spawn(p->i,FOOTPRINTS4);
                                break;
                            }
                            sprite[j].pal = p->footprintpal;
                            sprite[j].shade = p->footprintshade;
                            p->footprintcount--;
                        }
                    }
                }
        }

        if (p->pos.z < (fz-(i<<8)))  //falling
        {
            // not jumping or crouching

            if (!TEST_SYNC_KEY(sb_snum, SK_JUMP) && !TEST_SYNC_KEY(sb_snum, SK_CROUCH) &&
                    p->on_ground && (sector[p->cursectnum].floorstat&2) && p->pos.z >= (fz-(i<<8)-(16<<8)))
                p->pos.z = fz-(i<<8);
            else
            {
                p->on_ground = 0;
                p->vel.z += (g_spriteGravity+80); // (TICSPERFRAME<<6);
                if (p->vel.z >= (4096+2048)) p->vel.z = (4096+2048);
                if (p->vel.z > 2400 && p->falling_counter < 255)
                {
                    p->falling_counter++;
                    if (p->falling_counter >= 38 && p->scream_voice <= FX_Ok)
                        p->scream_voice = A_PlaySound(DUKE_SCREAM,p->i);
                }

                if ((p->pos.z+p->vel.z) >= (fz-(i<<8)) && p->cursectnum >= 0)   // hit the ground
                    if (sector[p->cursectnum].lotag != ST_1_ABOVE_WATER)
                    {
                        if (p->falling_counter > 62)
                            P_QuickKill(p);
                        else if (p->falling_counter > 9)
                        {
                            s->extra -= p->falling_counter-(krand()&3);
                            if (s->extra <= 0)
                            {
                                A_PlaySound(SQUISHED,p->i);

//                                P_PalFrom(p, 63, 63,0,0);
                            }
                            else
                            {
                                A_PlaySound(DUKE_LAND,p->i);
                                A_PlaySound(DUKE_LAND_HURT,p->i);
                            }

                            P_PalFrom(p, 32, 16,0,0);
                        }
                        else if (p->vel.z > 2048)
                            A_PlaySound(DUKE_LAND,p->i);
                    }
            }
        }
        else
        {
            p->falling_counter = 0;

            if (p->scream_voice > FX_Ok)
            {
                FX_StopSound(p->scream_voice);
                S_Cleanup();
                p->scream_voice = -1;
            }

            if (psectlotag != ST_1_ABOVE_WATER && psectlotag != ST_2_UNDERWATER && p->on_ground == 0 && p->vel.z > (6144>>1))
                p->hard_landing = p->vel.z>>10;

            p->on_ground = 1;

            if (i==40)
            {
                //Smooth on the ground

                k = ((fz-(i<<8))-p->pos.z)>>1;
                if (klabs(k) < 256) k = 0;
                p->pos.z += k;
                p->vel.z -= 768;
                if (p->vel.z < 0) p->vel.z = 0;
            }
            else if (p->jumping_counter == 0)
            {
                p->pos.z += ((fz-(i<<7))-p->pos.z)>>1; //Smooth on the water
                if (p->on_warping_sector == 0 && p->pos.z > fz-(16<<8))
                {
                    p->pos.z = fz-(16<<8);
                    p->vel.z >>= 1;
                }
            }

            p->on_warping_sector = 0;

            if (TEST_SYNC_KEY(sb_snum, SK_CROUCH))
            {
                // crouching
                if (VM_OnEvent(EVENT_CROUCH,p->i,snum, -1, 0) == 0)
                {
                    p->pos.z += (2048+768);
                    p->crack_time = 777;
                }
            }

            // jumping
            if (!TEST_SYNC_KEY(sb_snum, SK_JUMP) && p->jumping_toggle == 1)
                p->jumping_toggle = 0;
            else if (TEST_SYNC_KEY(sb_snum, SK_JUMP) && p->jumping_toggle == 0)
            {
                if (p->jumping_counter == 0)
                    if ((fz-cz) > (56<<8))
                    {
                        if (VM_OnEvent(EVENT_JUMP,p->i,snum, -1, 0) == 0)
                        {
                            p->jumping_counter = 1;
                            p->jumping_toggle = 1;
                        }
                    }
            }

            if (p->jumping_counter && !TEST_SYNC_KEY(sb_snum, SK_JUMP))
                p->jumping_toggle = 0;
        }

        if (p->jumping_counter)
        {
            if (!TEST_SYNC_KEY(sb_snum, SK_JUMP) && p->jumping_toggle == 1)
                p->jumping_toggle = 0;

            if (p->jumping_counter < (1024+256))
            {
                if (psectlotag == ST_1_ABOVE_WATER && p->jumping_counter > 768)
                {
                    p->jumping_counter = 0;
                    p->vel.z = -512;
                }
                else
                {
                    p->vel.z -= (sintable[(2048-128+p->jumping_counter)&2047])/12;
                    p->jumping_counter += 180;
                    p->on_ground = 0;
                }
            }
            else
            {
                p->jumping_counter = 0;
                p->vel.z = 0;
            }
        }

        p->pos.z += p->vel.z;

        if ((psectlotag != ST_2_UNDERWATER || cz != sector[p->cursectnum].ceilingz) && p->pos.z < (cz+(4<<8)))
        {
            p->jumping_counter = 0;
            if (p->vel.z < 0)
                p->vel.x = p->vel.y = 0;
            p->vel.z = 128;
            p->pos.z = cz+(4<<8);
        }
    }

    if (p->fist_incs || p->transporter_hold > 2 || p->hard_landing || p->access_incs > 0 || p->knee_incs > 0 ||
            (PWEAPON(0, p->curr_weapon, WorksLike) == TRIPBOMB_WEAPON &&
             *kb > 1 && *kb < PWEAPON(0, p->curr_weapon, FireDelay)))
    {
        doubvel = 0;
        p->vel.x = 0;
        p->vel.y = 0;
    }
    else if (g_player[snum].sync->avel)            //p->ang += syncangvel * constant
    {
        int32_t tempang = g_player[snum].sync->avel<<1;

        if (psectlotag == ST_2_UNDERWATER) p->angvel =(tempang-(tempang>>3))*ksgn(doubvel);
        else p->angvel = tempang*ksgn(doubvel);

        p->ang += p->angvel;
        p->ang &= 2047;
        p->crack_time = 777;
    }

    if (p->spritebridge == 0)
    {
        j = sector[s->sectnum].floorpicnum;

        if (j == PURPLELAVA || sector[s->sectnum].ceilingpicnum == PURPLELAVA)
        {
            if (p->inv_amount[GET_BOOTS] > 0)
            {
                p->inv_amount[GET_BOOTS]--;
                p->inven_icon = ICON_BOOTS;
                if (p->inv_amount[GET_BOOTS] <= 0)
                    P_SelectNextInvItem(p);
            }
            else
            {
                if (!A_CheckSoundPlaying(p->i,DUKE_LONGTERM_PAIN))
                    A_PlaySound(DUKE_LONGTERM_PAIN,p->i);

                P_PalFrom(p, 32, 0,8,0);
                s->extra--;
            }
        }

        if (p->on_ground && truefdist <= PHEIGHT+(16<<8) && P_CheckFloorDamage(p, j))
        {
            P_DoQuote(QUOTE_BOOTS_ON, p);
            p->inv_amount[GET_BOOTS] -= 2;
            if (p->inv_amount[GET_BOOTS] <= 0)
            {
                p->inv_amount[GET_BOOTS] = 0;
                P_SelectNextInvItem(p);
            }
        }
    }

    if (g_player[snum].sync->extbits&(1))
        VM_OnEvent(EVENT_MOVEFORWARD,p->i,snum, -1, 0);

    if (g_player[snum].sync->extbits&(1<<1))
        VM_OnEvent(EVENT_MOVEBACKWARD,p->i,snum, -1, 0);

    if (g_player[snum].sync->extbits&(1<<2))
        VM_OnEvent(EVENT_STRAFELEFT,p->i,snum, -1, 0);

    if (g_player[snum].sync->extbits&(1<<3))
        VM_OnEvent(EVENT_STRAFERIGHT,p->i,snum, -1, 0);

    if (g_player[snum].sync->extbits&(1<<4) || g_player[snum].sync->avel < 0)
        VM_OnEvent(EVENT_TURNLEFT,p->i,snum, -1, 0);

    if (g_player[snum].sync->extbits&(1<<5) || g_player[snum].sync->avel > 0)
        VM_OnEvent(EVENT_TURNRIGHT,p->i,snum, -1, 0);

    if (p->vel.x || p->vel.y || g_player[snum].sync->fvel || g_player[snum].sync->svel)
    {
        p->crack_time = 777;

        k = sintable[p->bobcounter&2047]>>12;

        if ((truefdist < PHEIGHT+(8<<8)) && (k == 1 || k == 3))
        {
            if (p->walking_snd_toggle == 0 && p->on_ground)
            {
                switch (psectlotag)
                {
                case 0:
                    if (lz >= 0 && (lz&49152) == 49152)
                        j = sprite[lz&(MAXSPRITES-1)].picnum;
                    else j = sector[p->cursectnum].floorpicnum;

                    switch (DYNAMICTILEMAP(j))
                    {
                    case PANNEL1__STATIC:
                    case PANNEL2__STATIC:
                        A_PlaySound(DUKE_WALKINDUCTS,p->i);
                        p->walking_snd_toggle = 1;
                        break;
                    }
                    break;

                case ST_1_ABOVE_WATER:
                    if (!p->spritebridge)
                    {
                        if ((krand()&1) == 0)
                            A_PlaySound(DUKE_ONWATER,p->i);
                        p->walking_snd_toggle = 1;
                    }
                    break;
                }
            }
        }
        else if (p->walking_snd_toggle > 0)
            p->walking_snd_toggle--;

        if (p->jetpack_on == 0 && p->inv_amount[GET_STEROIDS] > 0 && p->inv_amount[GET_STEROIDS] < 400)
            doubvel <<= 1;

        p->vel.x += ((g_player[snum].sync->fvel*doubvel)<<6);
        p->vel.y += ((g_player[snum].sync->svel*doubvel)<<6);

        j = 0;

        if (psectlotag == ST_2_UNDERWATER)
            j = 0x1400;
        else if (p->on_ground && (TEST_SYNC_KEY(sb_snum, SK_CROUCH) || (*kb > 10 && PWEAPON(snum, p->curr_weapon, WorksLike) == KNEE_WEAPON)))
            j = 0x2000;

        p->vel.x = mulscale16(p->vel.x,p->runspeed-j);
        p->vel.y = mulscale16(p->vel.y,p->runspeed-j);

        if (klabs(p->vel.x) < 2048 && klabs(p->vel.y) < 2048)
            p->vel.x = p->vel.y = 0;

        if (shrunk)
        {
            p->vel.x = mulscale16(p->vel.x,p->runspeed-(p->runspeed>>1)+(p->runspeed>>2));
            p->vel.y = mulscale16(p->vel.y,p->runspeed-(p->runspeed>>1)+(p->runspeed>>2));
        }
    }

HORIZONLY:
    if (psectlotag == ST_1_ABOVE_WATER || p->spritebridge == 1) i = p->autostep_sbw;
    else i = p->autostep;

    if (p->cursectnum >= 0 && sector[p->cursectnum].lotag == ST_2_UNDERWATER) k = 0;
    else k = 1;

    if (ud.noclip)
    {
        p->pos.x += p->vel.x>>14;
        p->pos.y += p->vel.y>>14;
        updatesector(p->pos.x,p->pos.y,&p->cursectnum);
        changespritesect(p->i,p->cursectnum);
    }
    else
    {
#ifdef YAX_ENABLE
        int32_t sect = p->cursectnum;
        int16_t cb, fb;

        if (sect >= 0)
            yax_getbunches(sect, &cb, &fb);

        // this updatesectorz conflicts with Duke3d's way of teleporting through water,
        // so make it a bit conditional... OTOH, this way we have an ugly z jump when
        // changing from above water to underwater
        if (sect >= 0 && !(sector[sect].lotag==ST_1_ABOVE_WATER && p->on_ground && fb>=0))
        {
            if ((fb>=0 && !(sector[sect].floorstat&512)) || (cb>=0 && !(sector[sect].ceilingstat&512)))
            {
                p->cursectnum += MAXSECTORS;  // skip initial z check, restored by updatesectorz
                updatesectorz(p->pos.x,p->pos.y,p->pos.z,&p->cursectnum);
            }
        }
#endif
        if ((j = clipmove((vec3_t *)p,&p->cursectnum, p->vel.x,p->vel.y,164L,(4L<<8),i,CLIPMASK0)))
            P_CheckTouchDamage(p, j);
    }

    // This makes the player view lower when shrunk.  NOTE that it can get the
    // view below the sector floor (and does, when on the ground).
    if (p->jetpack_on == 0 && psectlotag != ST_2_UNDERWATER && psectlotag != ST_1_ABOVE_WATER && shrunk)
        p->pos.z += 32<<8;

    if (p->jetpack_on == 0)
    {
        if (s->xvel > 16)
        {
            if (psectlotag != ST_1_ABOVE_WATER && psectlotag != ST_2_UNDERWATER && p->on_ground)
            {
                p->pycount += 52;
                p->pycount &= 2047;
                p->pyoff =
                    klabs(s->xvel*sintable[p->pycount])/1596;
            }
        }
        else if (psectlotag != ST_2_UNDERWATER && psectlotag != ST_1_ABOVE_WATER)
            p->pyoff = 0;
    }

    // RBG***

    p->pos.z += PHEIGHT;
    setsprite(p->i,(vec3_t *)&p->pos.x);
    p->pos.z -= PHEIGHT;

    // ST_2_UNDERWATER
    if (p->cursectnum >= 0 && psectlotag < 3)
    {
//        p->cursectnum = s->sectnum;

        if (!ud.noclip && sector[p->cursectnum].lotag == ST_31_TWO_WAY_TRAIN)
        {
            if (sprite[sector[p->cursectnum].hitag].xvel && actor[sector[p->cursectnum].hitag].t_data[0] == 0)
            {
                P_QuickKill(p);
                return;
            }
        }
    }

    if (p->cursectnum >= 0 && truefdist < PHEIGHT && p->on_ground &&
            psectlotag != ST_1_ABOVE_WATER && shrunk == 0 && sector[p->cursectnum].lotag == ST_1_ABOVE_WATER)
        if (!A_CheckSoundPlaying(p->i,DUKE_ONWATER))
            A_PlaySound(DUKE_ONWATER,p->i);

    if (p->cursectnum >=0 && p->cursectnum != s->sectnum)
        changespritesect(p->i, p->cursectnum);

    if (p->cursectnum >= 0 && ud.noclip == 0)
    {
        j = (pushmove((vec3_t *)p,&p->cursectnum,164L,(4L<<8),(4L<<8),CLIPMASK0) < 0 && A_GetFurthestAngle(p->i,8) < 512);

        if (klabs(actor[p->i].floorz-actor[p->i].ceilingz) < (48<<8) || j)
        {
            if (!(sector[s->sectnum].lotag&0x8000) && (isanunderoperator(sector[s->sectnum].lotag) ||
                    isanearoperator(sector[s->sectnum].lotag)))
                G_ActivateBySector(s->sectnum,p->i);
            if (j)
            {
                P_QuickKill(p);
                return;
            }
        }
        else if (klabs(fz-cz) < (32<<8) && isanunderoperator(sector[p->cursectnum].lotag))
            G_ActivateBySector(p->cursectnum,p->i);
    }

    i = 0;
    if (TEST_SYNC_KEY(sb_snum, SK_CENTER_VIEW) || p->hard_landing)
        if (VM_OnEvent(EVENT_RETURNTOCENTER,p->i,snum, -1, 0) == 0)
            p->return_to_center = 9;

    if (TEST_SYNC_KEY(sb_snum, SK_LOOK_UP))
    {
        if (VM_OnEvent(EVENT_LOOKUP,p->i,snum, -1, 0) == 0)
        {
            p->return_to_center = 9;
            if (TEST_SYNC_KEY(sb_snum, SK_RUN)) p->horiz += 12;
            p->horiz += 12;
            i++;
        }
    }

    if (TEST_SYNC_KEY(sb_snum, SK_LOOK_DOWN))
    {
        if (VM_OnEvent(EVENT_LOOKDOWN,p->i,snum, -1, 0) == 0)
        {
            p->return_to_center = 9;
            if (TEST_SYNC_KEY(sb_snum, SK_RUN)) p->horiz -= 12;
            p->horiz -= 12;
            i++;
        }
    }

    if (TEST_SYNC_KEY(sb_snum, SK_AIM_UP))
    {
        if (VM_OnEvent(EVENT_AIMUP,p->i,snum, -1, 0) == 0)
        {
            if (TEST_SYNC_KEY(sb_snum, SK_RUN)) p->horiz += 6;
            p->horiz += 6;
            i++;
        }
    }

    if (TEST_SYNC_KEY(sb_snum, SK_AIM_DOWN))
    {
        if (VM_OnEvent(EVENT_AIMDOWN,p->i,snum, -1, 0) == 0)
        {
            if (TEST_SYNC_KEY(sb_snum, SK_RUN)) p->horiz -= 6;
            p->horiz -= 6;
            i++;
        }
    }

    if (p->return_to_center > 0 && !TEST_SYNC_KEY(sb_snum, SK_LOOK_UP) && !TEST_SYNC_KEY(sb_snum, SK_LOOK_DOWN))
    {
        p->return_to_center--;
        p->horiz += 33-(p->horiz/3);
        i++;
    }

    if (p->hard_landing > 0)
    {
        p->hard_landing--;
        p->horiz -= (p->hard_landing<<4);
    }

    if (i)
    {
        if (p->horiz > 95 && p->horiz < 105) p->horiz = 100;
        if (p->horizoff > -5 && p->horizoff < 5) p->horizoff = 0;
    }

    p->horiz += g_player[snum].sync->horz;

    if (p->horiz > HORIZ_MAX) p->horiz = HORIZ_MAX;
    else if (p->horiz < HORIZ_MIN) p->horiz = HORIZ_MIN;

    //Shooting code/changes

    if (p->show_empty_weapon > 0)
    {
        p->show_empty_weapon--;
        if (p->show_empty_weapon == 0 && (p->weaponswitch & 2) && p->ammo_amount[p->curr_weapon] <= 0)
        {
            if (p->last_full_weapon == GROW_WEAPON)
                p->subweapon |= (1<<GROW_WEAPON);
            else if (p->last_full_weapon == SHRINKER_WEAPON)
                p->subweapon &= ~(1<<GROW_WEAPON);
            P_AddWeapon(p, p->last_full_weapon);
            return;
        }
    }

    if (p->knee_incs > 0)
    {
        p->horiz -= 48;
        p->return_to_center = 9;

        if (++p->knee_incs > 15)
        {
            p->knee_incs = 0;
            p->holster_weapon = 0;
            p->weapon_pos = klabs(p->weapon_pos);

            if (p->actorsqu >= 0 && sprite[p->actorsqu].statnum != MAXSTATUS && dist(&sprite[p->i],&sprite[p->actorsqu]) < 1400)
            {
                A_DoGuts(p->actorsqu,JIBS6,7);
                A_Spawn(p->actorsqu,BLOODPOOL);
                A_PlaySound(SQUISHED,p->actorsqu);

                switch (DYNAMICTILEMAP(sprite[p->actorsqu].picnum))
                {
                case FEM1__STATIC:
                case FEM2__STATIC:
                case FEM3__STATIC:
                case FEM4__STATIC:
                case FEM5__STATIC:
                case FEM6__STATIC:
                case FEM7__STATIC:
                case FEM8__STATIC:
                case FEM9__STATIC:
                case FEM10__STATIC:
                case PODFEM1__STATIC:
                case NAKED1__STATIC:
                case STATUE__STATIC:
                    if (sprite[p->actorsqu].yvel)
                        G_OperateRespawns(sprite[p->actorsqu].yvel);
                    A_DeleteSprite(p->actorsqu);
                    break;
                case APLAYER__STATIC:
                    P_QuickKill(g_player[sprite[p->actorsqu].yvel].ps);
                    g_player[sprite[p->actorsqu].yvel].ps->frag_ps = snum;
                    break;
                default:
                    if (A_CheckEnemySprite(&sprite[p->actorsqu]))
                        p->actors_killed++;
                    A_DeleteSprite(p->actorsqu);
                    break;
                }
            }
            p->actorsqu = -1;
        }
        else if (p->actorsqu >= 0)
            p->ang += G_GetAngleDelta(p->ang,getangle(sprite[p->actorsqu].x-p->pos.x,sprite[p->actorsqu].y-p->pos.y))>>2;
    }

    if (P_DoCounters(p)) return;

    P_ProcessWeapon(snum);
}
