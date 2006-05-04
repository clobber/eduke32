//-------------------------------------------------------------------------
/*
Copyright (C) 2005 - EDuke32 team

This file is part of EDuke32

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

int loadpheader(char spot,struct savehead *saveh)
{
    char fn[13];
    long fil;
    long bv;

    strcpy(fn, "egam0.sav");
    fn[4] = spot+'0';

    if ((fil = kopen4load(fn,0)) == -1) return(-1);

    walock[TILE_LOADSHOT] = 255;

    if(kdfread(&bv,sizeof(bv),1,fil) != 1) goto corrupt;
    if(kdfread(g_szBuf,bv,1,fil) != 1) goto corrupt;
    g_szBuf[bv]=0;
    AddLog(g_szBuf);

    if (kdfread(&bv,4,1,fil) != 1) goto corrupt;
    if(bv != BYTEVERSION) {
        FTA(114,&ps[myconnectindex]);
        kclose(fil);
        return 1;
    }

    if (kdfread(&saveh->numplr,sizeof(int32),1,fil) != 1) goto corrupt;

    if (kdfread(saveh->name,19,1,fil) != 1) goto corrupt;
    if (kdfread(&saveh->volnum,sizeof(int32),1,fil) != 1) goto corrupt;
    if (kdfread(&saveh->levnum,sizeof(int32),1,fil) != 1) goto corrupt;
    if (kdfread(&saveh->plrskl,sizeof(int32),1,fil) != 1) goto corrupt;
    if (kdfread(saveh->boardfn,BMAX_PATH,1,fil) != 1) goto corrupt;

    if (waloff[TILE_LOADSHOT] == 0) allocache(&waloff[TILE_LOADSHOT],320*200,&walock[TILE_LOADSHOT]);
    tilesizx[TILE_LOADSHOT] = 200; tilesizy[TILE_LOADSHOT] = 320;
    if (kdfread((char *)waloff[TILE_LOADSHOT],320,200,fil) != 200) goto corrupt;
    invalidatetile(TILE_LOADSHOT,0,255);

    kclose(fil);

    return(0);
corrupt:
    kclose(fil);
    return 1;
}

int loadplayer(signed char spot)
{
    short k;
    char fn[13];
    char mpfn[13];
    char *fnptr, scriptptrs[MAXSCRIPTSIZE];
    long fil, bv, i, j, x;
    int32 nump;

    strcpy(fn, "egam0.sav");
    strcpy(mpfn, "egamA_00.sav");

    if(spot < 0)
    {
        multiflag = 1;
        multiwhat = 0;
        multipos = -spot-1;
        return -1;
    }

    if( multiflag == 2 && multiwho != myconnectindex )
    {
        fnptr = mpfn;
        mpfn[4] = spot + 'A';

        if(ud.multimode > 9)
        {
            mpfn[6] = (multiwho/10) + '0';
            mpfn[7] = (multiwho%10) + '0';
        }
        else mpfn[7] = multiwho + '0';
    }
    else
    {
        fnptr = fn;
        fn[4] = spot + '0';
    }

    if ((fil = kopen4load(fnptr,0)) == -1) return(-1);

    ready2send = 0;

    if(kdfread(&bv,sizeof(bv),1,fil) != 1) goto corrupt;
    if(kdfread(g_szBuf,bv,1,fil) != 1) goto corrupt;
    g_szBuf[bv]=0;
    AddLog(g_szBuf);

    if (kdfread(&bv,4,1,fil) != 1) return -1;
    if(bv != BYTEVERSION)
    {
        FTA(114,&ps[myconnectindex]);
        kclose(fil);
        ototalclock = totalclock;
        ready2send = 1;
        return 1;
    }

    if (kdfread(&nump,sizeof(nump),1,fil) != 1) return -1;
    if(nump != numplayers)
    {
        kclose(fil);
        ototalclock = totalclock;
        ready2send = 1;
        FTA(124,&ps[myconnectindex]);
        return 1;
    }

    if(numplayers > 1)
    {
        pub = NUMPAGES;
        pus = NUMPAGES;
        vscrn();
        drawbackground();
        menutext(160,100,0,0,"LOADING...");
        nextpage();
    }

    waitforeverybody();

    FX_StopAllSounds();
    clearsoundlocks();
    MUSIC_StopSong();

    if(numplayers > 1) {
        if (kdfread(&buf,19,1,fil) != 1) goto corrupt;
    } else {
        if (kdfread(&ud.savegame[spot][0],19,1,fil) != 1) goto corrupt;
    }


    if (kdfread(&ud.volume_number,sizeof(ud.volume_number),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.level_number,sizeof(ud.level_number),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.player_skill,sizeof(ud.player_skill),1,fil) != 1) goto corrupt;
    if (kdfread(&boardfilename[0],BMAX_PATH,1,fil) != 1) goto corrupt;

    ud.m_level_number = ud.level_number;
    ud.m_volume_number = ud.volume_number;
    ud.m_player_skill = ud.player_skill;

    //Fake read because lseek won't work with compression
    walock[TILE_LOADSHOT] = 1;
    if (waloff[TILE_LOADSHOT] == 0) allocache(&waloff[TILE_LOADSHOT],320*200,&walock[TILE_LOADSHOT]);
    tilesizx[TILE_LOADSHOT] = 200; tilesizy[TILE_LOADSHOT] = 320;
    if (kdfread((char *)waloff[TILE_LOADSHOT],320,200,fil) != 200) goto corrupt;
    invalidatetile(TILE_LOADSHOT,0,255);

    if (kdfread(&numwalls,2,1,fil) != 1) goto corrupt;
    if (kdfread(&wall[0],sizeof(walltype),MAXWALLS,fil) != MAXWALLS) goto corrupt;
    if (kdfread(&numsectors,2,1,fil) != 1) goto corrupt;
    if (kdfread(&sector[0],sizeof(sectortype),MAXSECTORS,fil) != MAXSECTORS) goto corrupt;
    if (kdfread(&sprite[0],sizeof(spritetype),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&spriteext[0],sizeof(spriteexttype),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&headspritesect[0],2,MAXSECTORS+1,fil) != MAXSECTORS+1) goto corrupt;
    if (kdfread(&prevspritesect[0],2,MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&nextspritesect[0],2,MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&headspritestat[0],2,MAXSTATUS+1,fil) != MAXSTATUS+1) goto corrupt;
    if (kdfread(&prevspritestat[0],2,MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&nextspritestat[0],2,MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&numcyclers,sizeof(numcyclers),1,fil) != 1) goto corrupt;
    if (kdfread(&cyclers[0][0],12,MAXCYCLERS,fil) != MAXCYCLERS) goto corrupt;
    if (kdfread(ps,sizeof(ps),1,fil) != 1) goto corrupt;
    if (kdfread(po,sizeof(po),1,fil) != 1) goto corrupt;
    if (kdfread(&numanimwalls,sizeof(numanimwalls),1,fil) != 1) goto corrupt;
    if (kdfread(&animwall,sizeof(animwall),1,fil) != 1) goto corrupt;
    if (kdfread(&msx[0],sizeof(long),sizeof(msx)/sizeof(long),fil) != sizeof(msx)/sizeof(long)) goto corrupt;
    if (kdfread(&msy[0],sizeof(long),sizeof(msy)/sizeof(long),fil) != sizeof(msy)/sizeof(long)) goto corrupt;
    if (kdfread((short *)&spriteqloc,sizeof(short),1,fil) != 1) goto corrupt;
    if (kdfread((short *)&spriteqamount,sizeof(short),1,fil) != 1) goto corrupt;
    if (kdfread((short *)&spriteq[0],sizeof(short),spriteqamount,fil) != spriteqamount) goto corrupt;
    if (kdfread(&mirrorcnt,sizeof(short),1,fil) != 1) goto corrupt;
    if (kdfread(&mirrorwall[0],sizeof(short),64,fil) != 64) goto corrupt;
    if (kdfread(&mirrorsector[0],sizeof(short),64,fil) != 64) goto corrupt;
    if (kdfread(&show2dsector[0],sizeof(char),MAXSECTORS>>3,fil) != (MAXSECTORS>>3)) goto corrupt;
    if (kdfread(&actortype[0],sizeof(char),MAXTILES,fil) != MAXTILES) goto corrupt;

    if (kdfread(&numclouds,sizeof(numclouds),1,fil) != 1) goto corrupt;
    if (kdfread(&clouds[0],sizeof(short)<<7,1,fil) != 1) goto corrupt;
    if (kdfread(&cloudx[0],sizeof(short)<<7,1,fil) != 1) goto corrupt;
    if (kdfread(&cloudy[0],sizeof(short)<<7,1,fil) != 1) goto corrupt;

    if (kdfread(&scriptptrs[0],1,MAXSCRIPTSIZE,fil) != MAXSCRIPTSIZE) goto corrupt;
    if (kdfread(&script[0],4,MAXSCRIPTSIZE,fil) != MAXSCRIPTSIZE) goto corrupt;
    for(i=0;i<MAXSCRIPTSIZE;i++)
        if( scriptptrs[i] )
        {
            j = (long)script[i]+(long)&script[0];
            script[i] = j;
        }

    if (kdfread(&actorscrptr[0],4,MAXTILES,fil) != MAXTILES) goto corrupt;
    for(i=0;i<MAXTILES;i++)
        if(actorscrptr[i])
        {
            j = (long)actorscrptr[i]+(long)&script[0];
            actorscrptr[i] = (long *)j;
        }
    if (kdfread(&actorLoadEventScrptr[0],4,MAXTILES,fil) != MAXTILES) goto corrupt;
    for(i=0;i<MAXTILES;i++)
        if(actorLoadEventScrptr[i])
        {
            j = (long)actorLoadEventScrptr[i]+(long)&script[0];
            actorLoadEventScrptr[i] = (long *)j;
        }

    if (kdfread(&scriptptrs[0],1,MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&hittype[0],sizeof(struct weaponhit),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;

    for(i=0;i<MAXSPRITES;i++)
    {
        j = (long)(&script[0]);
        if( scriptptrs[i]&1 ) T2 += j;
        if( scriptptrs[i]&2 ) T5 += j;
        if( scriptptrs[i]&4 ) T6 += j;
    }

    if (kdfread(&lockclock,sizeof(lockclock),1,fil) != 1) goto corrupt;
    if (kdfread(&pskybits,sizeof(pskybits),1,fil) != 1) goto corrupt;
    if (kdfread(&pskyoff[0],sizeof(pskyoff[0]),MAXPSKYTILES,fil) != MAXPSKYTILES) goto corrupt;

    if (kdfread(&animatecnt,sizeof(animatecnt),1,fil) != 1) goto corrupt;
    if (kdfread(&animatesect[0],2,MAXANIMATES,fil) != MAXANIMATES) goto corrupt;
    if (kdfread(&animateptr[0],4,MAXANIMATES,fil) != MAXANIMATES) goto corrupt;
    for(i = animatecnt-1;i>=0;i--) animateptr[i] = (long *)((long)animateptr[i]+(long)(&sector[0]));
    if (kdfread(&animategoal[0],4,MAXANIMATES,fil) != MAXANIMATES) goto corrupt;
    if (kdfread(&animatevel[0],4,MAXANIMATES,fil) != MAXANIMATES) goto corrupt;

    if (kdfread(&earthquaketime,sizeof(earthquaketime),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.from_bonus,sizeof(ud.from_bonus),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.secretlevel,sizeof(ud.secretlevel),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.respawn_monsters,sizeof(ud.respawn_monsters),1,fil) != 1) goto corrupt;
    ud.m_respawn_monsters = ud.respawn_monsters;
    if (kdfread(&ud.respawn_items,sizeof(ud.respawn_items),1,fil) != 1) goto corrupt;
    ud.m_respawn_items = ud.respawn_items;
    if (kdfread(&ud.respawn_inventory,sizeof(ud.respawn_inventory),1,fil) != 1) goto corrupt;
    ud.m_respawn_inventory = ud.respawn_inventory;

    if (kdfread(&ud.god,sizeof(ud.god),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.auto_run,sizeof(ud.auto_run),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.crosshair,sizeof(ud.crosshair),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.monsters_off,sizeof(ud.monsters_off),1,fil) != 1) goto corrupt;
    ud.m_monsters_off = ud.monsters_off;
    if (kdfread(&ud.last_level,sizeof(ud.last_level),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.eog,sizeof(ud.eog),1,fil) != 1) goto corrupt;

    if (kdfread(&ud.coop,sizeof(ud.coop),1,fil) != 1) goto corrupt;
    ud.m_coop = ud.coop;
    if (kdfread(&ud.marker,sizeof(ud.marker),1,fil) != 1) goto corrupt;
    ud.m_marker = ud.marker;
    if (kdfread(&ud.ffire,sizeof(ud.ffire),1,fil) != 1) goto corrupt;
    ud.m_ffire = ud.ffire;

    if (kdfread(&camsprite,sizeof(camsprite),1,fil) != 1) goto corrupt;
    if (kdfread(&connecthead,sizeof(connecthead),1,fil) != 1) goto corrupt;
    if (kdfread(connectpoint2,sizeof(connectpoint2),1,fil) != 1) goto corrupt;
    if (kdfread(&numplayersprites,sizeof(numplayersprites),1,fil) != 1) goto corrupt;
    if (kdfread((short *)&frags[0][0],sizeof(frags),1,fil) != 1) goto corrupt;

    if (kdfread(&randomseed,sizeof(randomseed),1,fil) != 1) goto corrupt;
    if (kdfread(&global_random,sizeof(global_random),1,fil) != 1) goto corrupt;
    if (kdfread(&parallaxyscale,sizeof(parallaxyscale),1,fil) != 1) goto corrupt;

    if (kdfread(&projectile[0],sizeof(proj_struct),MAXTILES,fil) != MAXTILES) goto corrupt;
    if (kdfread(&defaultprojectile[0],sizeof(proj_struct),MAXTILES,fil) != MAXTILES) goto corrupt;
    if (kdfread(&thisprojectile[0],sizeof(proj_struct),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;

    if (kdfread(&spriteflags[0],sizeof(spriteflags[0]),MAXTILES,fil) != MAXTILES) goto corrupt;
    if (kdfread(&actorspriteflags[0],sizeof(actorspriteflags[0]),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;

    if (kdfread(&spritecache[0],sizeof(spritecache[0]),MAXTILES,fil) != MAXTILES) goto corrupt;

    if (kdfread(&i,sizeof(long),1,fil) != 1) goto corrupt;

    while(i != MAXQUOTES)
    {
        if(fta_quotes[i] != NULL)
            Bfree(fta_quotes[i]);

        fta_quotes[i] = Bcalloc(MAXQUOTELEN,sizeof(char));

        if(kdfread((char *)fta_quotes[i],MAXQUOTELEN,1,fil) != 1) goto corrupt;
        if(kdfread(&i,sizeof(long),1,fil) != 1) goto corrupt;
    }

    if (kdfread(&redefined_quote_count,sizeof(redefined_quote_count),1,fil) != 1) goto corrupt;

    for(i=0;i<redefined_quote_count;i++)
    {
        if(redefined_quotes[i] != NULL)
            Bfree(redefined_quotes[i]);

        redefined_quotes[i] = Bcalloc(MAXQUOTELEN,sizeof(char));

        if(kdfread((char *)redefined_quotes[i],MAXQUOTELEN,1,fil) != 1) goto corrupt;
    }

    if (kdfread(&dynamictostatic[0],sizeof(dynamictostatic[0]),MAXTILES,fil) != MAXTILES) goto corrupt;

    if (kdfread(&ud.noexits,sizeof(ud.noexits),1,fil) != 1) goto corrupt;
    ud.m_noexits = ud.noexits;


    if(ReadGameVars(fil)) goto corrupt;

    kclose(fil);

    if(ps[myconnectindex].over_shoulder_on != 0)
    {
        cameradist = 0;
        cameraclock = 0;
        ps[myconnectindex].over_shoulder_on = 1;
    }

    screenpeek = myconnectindex;

    clearbufbyte(gotpic,sizeof(gotpic),0L);
    clearsoundlocks();
    cacheit();

    music_select = (ud.volume_number*11) + ud.level_number;
    playmusic(&music_fn[0][music_select][0]);

    ps[myconnectindex].gm = MODE_GAME;
    ud.recstat = 0;

    if(ps[myconnectindex].jetpack_on)
        spritesound(DUKE_JETPACK_IDLE,ps[myconnectindex].i);

    restorepalette = 1;
    setpal(&ps[myconnectindex]);
    vscrn();

    FX_SetReverb(0);

    if(ud.lockout == 0)
    {
        for(x=0;x<numanimwalls;x++)
            if( wall[animwall[x].wallnum].extra >= 0 )
                wall[animwall[x].wallnum].picnum = wall[animwall[x].wallnum].extra;
    }
    else
    {
        for(x=0;x<numanimwalls;x++)
            switch(dynamictostatic[wall[animwall[x].wallnum].picnum])
            {
            case FEMPIC1__STATIC:
                wall[animwall[x].wallnum].picnum = BLANKSCREEN;
                break;
            case FEMPIC2__STATIC:
            case FEMPIC3__STATIC:
                wall[animwall[x].wallnum].picnum = SCREENBREAK6;
                break;
            }
    }

    numinterpolations = 0;
    startofdynamicinterpolations = 0;

    k = headspritestat[3];
    while(k >= 0)
    {
        switch(sprite[k].lotag)
        {
        case 31:
            setinterpolation(&sector[sprite[k].sectnum].floorz);
            break;
        case 32:
            setinterpolation(&sector[sprite[k].sectnum].ceilingz);
            break;
        case 25:
            setinterpolation(&sector[sprite[k].sectnum].floorz);
            setinterpolation(&sector[sprite[k].sectnum].ceilingz);
            break;
        case 17:
            setinterpolation(&sector[sprite[k].sectnum].floorz);
            setinterpolation(&sector[sprite[k].sectnum].ceilingz);
            break;
        case 0:
        case 5:
        case 6:
        case 11:
        case 14:
        case 15:
        case 16:
        case 26:
        case 30:
            setsectinterpolate(k);
            break;
        }

        k = nextspritestat[k];
    }

    for(i=numinterpolations-1;i>=0;i--) bakipos[i] = *curipos[i];
    for(i = animatecnt-1;i>=0;i--)
        setinterpolation(animateptr[i]);

    show_shareware = 0;
    everyothertime = 0;

    clearbufbyte(playerquitflag,MAXPLAYERS,0x01010101);

    resetmys();

    ready2send = 1;

    flushpackets();
    clearfifo();
    waitforeverybody();

    resettimevars();

    return(0);
corrupt:
    Bsprintf(tempbuf,"Save game file \"%s\" is corrupt.",fnptr);
    gameexit(tempbuf);
    return -1;
}

int saveplayer(signed char spot)
{
    long i, j;
    char fn[13];
    char mpfn[13];
    char *fnptr,scriptptrs[MAXSCRIPTSIZE];
    FILE *fil;
    long bv = BYTEVERSION;

    strcpy(fn, "egam0.sav");
    strcpy(mpfn, "egamA_00.sav");

    if(spot < 0)
    {
        multiflag = 1;
        multiwhat = 1;
        multipos = -spot-1;
        return -1;
    }

    waitforeverybody();

    if( multiflag == 2 && multiwho != myconnectindex )
    {
        fnptr = mpfn;
        mpfn[4] = spot + 'A';

        if(ud.multimode > 9)
        {
            mpfn[6] = (multiwho/10) + '0';
            mpfn[7] = multiwho + '0';
        }
        else mpfn[7] = multiwho + '0';
    }
    else
    {
        fnptr = fn;
        fn[4] = spot + '0';
    }

    if ((fil = fopen(fnptr,"wb")) == 0) return(-1);

    ready2send = 0;

    Bsprintf(g_szBuf,"EDuke32");
    i=strlen(g_szBuf);
    dfwrite(&i,sizeof(i),1,fil);
    dfwrite(g_szBuf,i,1,fil);

    dfwrite(&bv,4,1,fil);
    dfwrite(&ud.multimode,sizeof(ud.multimode),1,fil);

    dfwrite(&ud.savegame[spot][0],19,1,fil);
    dfwrite(&ud.volume_number,sizeof(ud.volume_number),1,fil);
    dfwrite(&ud.level_number,sizeof(ud.level_number),1,fil);
    dfwrite(&ud.player_skill,sizeof(ud.player_skill),1,fil);
    dfwrite(&boardfilename[0],BMAX_PATH,1,fil);
    if (!waloff[TILE_SAVESHOT]) {
        walock[TILE_SAVESHOT] = 254;
        allocache((long *)&waloff[TILE_SAVESHOT],200*320,&walock[TILE_SAVESHOT]);
        clearbuf((void*)waloff[TILE_SAVESHOT],(200*320)/4,0);
        walock[TILE_SAVESHOT] = 1;
    }
    dfwrite((char *)waloff[TILE_SAVESHOT],320,200,fil);

    dfwrite(&numwalls,2,1,fil);
    dfwrite(&wall[0],sizeof(walltype),MAXWALLS,fil);
    dfwrite(&numsectors,2,1,fil);
    dfwrite(&sector[0],sizeof(sectortype),MAXSECTORS,fil);
    dfwrite(&sprite[0],sizeof(spritetype),MAXSPRITES,fil);
    dfwrite(&spriteext[0],sizeof(spriteexttype),MAXSPRITES,fil);
    dfwrite(&headspritesect[0],2,MAXSECTORS+1,fil);
    dfwrite(&prevspritesect[0],2,MAXSPRITES,fil);
    dfwrite(&nextspritesect[0],2,MAXSPRITES,fil);
    dfwrite(&headspritestat[0],2,MAXSTATUS+1,fil);
    dfwrite(&prevspritestat[0],2,MAXSPRITES,fil);
    dfwrite(&nextspritestat[0],2,MAXSPRITES,fil);
    dfwrite(&numcyclers,sizeof(numcyclers),1,fil);
    dfwrite(&cyclers[0][0],12,MAXCYCLERS,fil);
    dfwrite(ps,sizeof(ps),1,fil);
    dfwrite(po,sizeof(po),1,fil);
    dfwrite(&numanimwalls,sizeof(numanimwalls),1,fil);
    dfwrite(&animwall,sizeof(animwall),1,fil);
    dfwrite(&msx[0],sizeof(long),sizeof(msx)/sizeof(long),fil);
    dfwrite(&msy[0],sizeof(long),sizeof(msy)/sizeof(long),fil);
    dfwrite(&spriteqloc,sizeof(short),1,fil);
    dfwrite(&spriteqamount,sizeof(short),1,fil);
    dfwrite(&spriteq[0],sizeof(short),spriteqamount,fil);
    dfwrite(&mirrorcnt,sizeof(short),1,fil);
    dfwrite(&mirrorwall[0],sizeof(short),64,fil);
    dfwrite(&mirrorsector[0],sizeof(short),64,fil);
    dfwrite(&show2dsector[0],sizeof(char),MAXSECTORS>>3,fil);
    dfwrite(&actortype[0],sizeof(char),MAXTILES,fil);

    dfwrite(&numclouds,sizeof(numclouds),1,fil);
    dfwrite(&clouds[0],sizeof(short)<<7,1,fil);
    dfwrite(&cloudx[0],sizeof(short)<<7,1,fil);
    dfwrite(&cloudy[0],sizeof(short)<<7,1,fil);

    for(i=0;i<MAXSCRIPTSIZE;i++)
    {
        if( (long)script[i] >= (long)(&script[0]) && (long)script[i] < (long)(&script[MAXSCRIPTSIZE]) )
        {
            scriptptrs[i] = 1;
            j = (long)script[i] - (long)&script[0];
            script[i] = j;
        }
        else scriptptrs[i] = 0;
    }

    dfwrite(&scriptptrs[0],1,MAXSCRIPTSIZE,fil);
    dfwrite(&script[0],4,MAXSCRIPTSIZE,fil);

    for(i=0;i<MAXSCRIPTSIZE;i++)
        if( scriptptrs[i] )
        {
            j = script[i]+(long)&script[0];
            script[i] = j;
        }

    for(i=0;i<MAXTILES;i++)
        if(actorscrptr[i])
        {
            j = (long)actorscrptr[i]-(long)&script[0];
            actorscrptr[i] = (long *)j;
        }
    dfwrite(&actorscrptr[0],4,MAXTILES,fil);
    for(i=0;i<MAXTILES;i++)
        if(actorscrptr[i])
        {
            j = (long)actorscrptr[i]+(long)&script[0];
            actorscrptr[i] = (long *)j;
        }

    for(i=0;i<MAXTILES;i++)
        if(actorLoadEventScrptr[i])
        {
            j = (long)actorLoadEventScrptr[i]-(long)&script[0];
            actorLoadEventScrptr[i] = (long *)j;
        }
    dfwrite(&actorLoadEventScrptr[0],4,MAXTILES,fil);
    for(i=0;i<MAXTILES;i++)
        if(actorLoadEventScrptr[i])
        {
            j = (long)actorLoadEventScrptr[i]+(long)&script[0];
            actorLoadEventScrptr[i] = (long *)j;
        }


    for(i=0;i<MAXSPRITES;i++)
    {
        scriptptrs[i] = 0;

        if(actorscrptr[PN] == 0) continue;

        j = (long)&script[0];

        if(T2 >= j && T2 < (long)(&script[MAXSCRIPTSIZE]) )
        {
            scriptptrs[i] |= 1;
            T2 -= j;
        }
        if(T5 >= j && T5 < (long)(&script[MAXSCRIPTSIZE]) )
        {
            scriptptrs[i] |= 2;
            T5 -= j;
        }
        if(T6 >= j && T6 < (long)(&script[MAXSCRIPTSIZE]) )
        {
            scriptptrs[i] |= 4;
            T6 -= j;
        }
    }

    dfwrite(&scriptptrs[0],1,MAXSPRITES,fil);
    dfwrite(&hittype[0],sizeof(struct weaponhit),MAXSPRITES,fil);

    for(i=0;i<MAXSPRITES;i++)
    {
        if(actorscrptr[PN] == 0) continue;
        j = (long)&script[0];

        if(scriptptrs[i]&1)
            T2 += j;
        if(scriptptrs[i]&2)
            T5 += j;
        if(scriptptrs[i]&4)
            T6 += j;
    }

    dfwrite(&lockclock,sizeof(lockclock),1,fil);
    dfwrite(&pskybits,sizeof(pskybits),1,fil);
    dfwrite(&pskyoff[0],sizeof(pskyoff[0]),MAXPSKYTILES,fil);
    dfwrite(&animatecnt,sizeof(animatecnt),1,fil);
    dfwrite(&animatesect[0],2,MAXANIMATES,fil);
    for(i = animatecnt-1;i>=0;i--) animateptr[i] = (long *)((long)animateptr[i]-(long)(&sector[0]));
    dfwrite(&animateptr[0],4,MAXANIMATES,fil);
    for(i = animatecnt-1;i>=0;i--) animateptr[i] = (long *)((long)animateptr[i]+(long)(&sector[0]));
    dfwrite(&animategoal[0],4,MAXANIMATES,fil);
    dfwrite(&animatevel[0],4,MAXANIMATES,fil);

    dfwrite(&earthquaketime,sizeof(earthquaketime),1,fil);
    dfwrite(&ud.from_bonus,sizeof(ud.from_bonus),1,fil);
    dfwrite(&ud.secretlevel,sizeof(ud.secretlevel),1,fil);
    dfwrite(&ud.respawn_monsters,sizeof(ud.respawn_monsters),1,fil);
    dfwrite(&ud.respawn_items,sizeof(ud.respawn_items),1,fil);
    dfwrite(&ud.respawn_inventory,sizeof(ud.respawn_inventory),1,fil);
    dfwrite(&ud.god,sizeof(ud.god),1,fil);
    dfwrite(&ud.auto_run,sizeof(ud.auto_run),1,fil);
    dfwrite(&ud.crosshair,sizeof(ud.crosshair),1,fil);
    dfwrite(&ud.monsters_off,sizeof(ud.monsters_off),1,fil);
    dfwrite(&ud.last_level,sizeof(ud.last_level),1,fil);
    dfwrite(&ud.eog,sizeof(ud.eog),1,fil);
    dfwrite(&ud.coop,sizeof(ud.coop),1,fil);
    dfwrite(&ud.marker,sizeof(ud.marker),1,fil);
    dfwrite(&ud.ffire,sizeof(ud.ffire),1,fil);
    dfwrite(&camsprite,sizeof(camsprite),1,fil);
    dfwrite(&connecthead,sizeof(connecthead),1,fil);
    dfwrite(connectpoint2,sizeof(connectpoint2),1,fil);
    dfwrite(&numplayersprites,sizeof(numplayersprites),1,fil);
    dfwrite((short *)&frags[0][0],sizeof(frags),1,fil);

    dfwrite(&randomseed,sizeof(randomseed),1,fil);
    dfwrite(&global_random,sizeof(global_random),1,fil);
    dfwrite(&parallaxyscale,sizeof(parallaxyscale),1,fil);

    dfwrite(&projectile[0],sizeof(proj_struct),MAXTILES,fil);
    dfwrite(&defaultprojectile[0],sizeof(proj_struct),MAXTILES,fil);
    dfwrite(&thisprojectile[0],sizeof(proj_struct),MAXSPRITES,fil);

    dfwrite(&spriteflags[0],sizeof(spriteflags[0]),MAXTILES,fil);
    dfwrite(&actorspriteflags[0],sizeof(actorspriteflags[0]),MAXSPRITES,fil);

    dfwrite(&spritecache[0],sizeof(spritecache[0]),MAXTILES,fil);

    for(i=0;i<MAXQUOTES;i++)
    {
        if(fta_quotes[i] != NULL)
        {
            dfwrite(&i,sizeof(long),1,fil);
            dfwrite(fta_quotes[i],MAXQUOTELEN, 1, fil);
        }
    }
    dfwrite(&i,sizeof(long),1,fil);

    dfwrite(&redefined_quote_count,sizeof(redefined_quote_count),1,fil);
    for(i=0;i<redefined_quote_count;i++)
    {
        if(redefined_quotes[i] != NULL)
            dfwrite(redefined_quotes[i],MAXQUOTELEN, 1, fil);
    }

    dfwrite(&dynamictostatic[0],sizeof(dynamictostatic[0]),MAXTILES,fil);

    dfwrite(&ud.noexits,sizeof(ud.noexits),1,fil);

    SaveGameVars(fil);

    fclose(fil);

    if(ud.multimode < 2)
    {
        strcpy(fta_quotes[122],"GAME SAVED");
        FTA(122,&ps[myconnectindex]);
    }

    ready2send = 1;

    waitforeverybody();

    ototalclock = totalclock;

    return(0);
}
