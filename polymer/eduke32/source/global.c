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

char MusicPtr[72000*2];

short global_random;
short neartagsector, neartagwall, neartagsprite;

long neartaghitdist,lockclock,max_player_health,max_armour_amount,max_ammo_amount[MAX_WEAPONS];
// JBF: gc modified to default to Atomic ed. default when using 1.3d CONs
long gc=176;

// long temp_data[MAXSPRITES][6];
struct weaponhit hittype[MAXSPRITES];
short spriteq[1024],spriteqloc,spriteqamount=64;
struct animwalltype animwall[MAXANIMWALLS];
short numanimwalls;
long *animateptr[MAXANIMATES], animategoal[MAXANIMATES], animatevel[MAXANIMATES], animatecnt;
// long oanimateval[MAXANIMATES];
short animatesect[MAXANIMATES];
long msx[2048],msy[2048];
short cyclers[MAXCYCLERS][6],numcyclers;

char *fta_quotes[MAXQUOTES],*redefined_quotes[MAXQUOTES];

char tempbuf[2048], packbuf[576];

char buf[1024];

short camsprite;
short mirrorwall[64], mirrorsector[64], mirrorcnt;

int current_menu;

char level_names[MAXVOLUMES*11][33],level_file_names[MAXVOLUMES*11][BMAX_PATH];
long partime[MAXVOLUMES*11],designertime[MAXVOLUMES*11];
char volume_names[MAXVOLUMES][33] = { "L.A. MELTDOWN", "LUNAR APOCALYPSE", "SHRAPNEL CITY" };
char skill_names[5][33] = { "PIECE OF CAKE", "LET'S ROCK", "COME GET SOME", "DAMN I'M GOOD" };

char gametype_names[MAXGAMETYPES][33] = { "DUKEMATCH (SPAWN)","COOPERATIVE PLAY","DUKEMATCH (NO SPAWN)"};
int gametype_flags[MAXGAMETYPES] = {4+8+16+1024+2048+16384,1+2+32+64+128+256+512+4096+8192+32768,2+4+8+16+16384};
char num_gametypes = 3;

long soundsiz[NUM_SOUNDS];

short soundps[NUM_SOUNDS],soundpe[NUM_SOUNDS],soundvo[NUM_SOUNDS];
char soundm[NUM_SOUNDS],soundpr[NUM_SOUNDS];
char sounds[NUM_SOUNDS][BMAX_PATH];

short title_zoom;

int framerate;

char num_volumes = 3;

short timer=120;
//fx_device device;

SAMPLE Sound[ NUM_SOUNDS ];
SOUNDOWNER SoundOwner[NUM_SOUNDS][4];

char numplayersprites,loadfromgrouponly=0,earthquaketime;

long fricxv,fricyv;
struct player_orig po[MAXPLAYERS];
struct player_struct ps[MAXPLAYERS];
struct user_defs ud;

char pus, pub;
char syncstat, syncval[MAXPLAYERS][MOVEFIFOSIZ];
long syncvalhead[MAXPLAYERS], syncvaltail, syncvaltottail;

input sync[MAXPLAYERS], loc;
input recsync[RECSYNCBUFSIZ];
long avgfvel, avgsvel, avgavel, avghorz, avgbits, avgbits2;


input inputfifo[MOVEFIFOSIZ][MAXPLAYERS];

long movefifosendplc;

//Multiplayer syncing variables
short screenpeek;
long movefifoend[MAXPLAYERS];


//Game recording variables

char playerreadyflag[MAXPLAYERS],ready2send;
char playerquitflag[MAXPLAYERS];
long vel, svel, angvel, horiz, ototalclock, respawnactortime=768, respawnitemtime=768, groupfile;

long *scriptptr,*insptr,*labelcode,labelcnt,defaultlabelcnt,*labeltype;
long *actorscrptr[MAXTILES],*parsing_actor;
char *label,*textptr,error,warning,killit_flag;
char *music_pointer;
char actortype[MAXTILES];
long script[MAXSCRIPTSIZE+16];

char display_mirror,typebuflen,typebuf[141];

char music_fn[MAXVOLUMES+1][11][13],music_select;
char env_music_fn[MAXVOLUMES+1][13];
char rtsplaying;


short weaponsandammosprites[15] = {
                                      RPGSPRITE__STATIC,
                                      CHAINGUNSPRITE__STATIC,
                                      DEVISTATORAMMO__STATIC,
                                      RPGAMMO__STATIC,
                                      RPGAMMO__STATIC,
                                      JETPACK__STATIC,
                                      SHIELD__STATIC,
                                      FIRSTAID__STATIC,
                                      STEROIDS__STATIC,
                                      RPGAMMO__STATIC,
                                      RPGAMMO__STATIC,
                                      RPGSPRITE__STATIC,
                                      RPGAMMO__STATIC,
                                      FREEZESPRITE__STATIC,
                                      FREEZEAMMO__STATIC
                                  };

long impact_damage;
char condebug;

//GLOBAL.C - replace the end "my's" with this
long myx, omyx, myxvel, myy, omyy, myyvel, myz, omyz, myzvel;
short myhoriz, omyhoriz, myhorizoff, omyhorizoff;
short myang, omyang, mycursectnum, myjumpingcounter,frags[MAXPLAYERS][MAXPLAYERS];

char myjumpingtoggle, myonground, myhardlanding, myreturntocenter;
signed char multiwho, multipos, multiwhat, multiflag;

long fakemovefifoplc,movefifoplc;
long myxbak[MOVEFIFOSIZ], myybak[MOVEFIFOSIZ], myzbak[MOVEFIFOSIZ];
long myhorizbak[MOVEFIFOSIZ],dukefriction = 0xcc00, show_shareware;

short myangbak[MOVEFIFOSIZ];
char myname[32],camerashitable,freezerhurtowner=0,lasermode=0;
char networkmode = 255, movesperpacket = 1,gamequit = 0,everyothertime;
long numfreezebounces=3,rpgblastradius,pipebombblastradius,tripbombblastradius,shrinkerblastradius,morterblastradius,bouncemineblastradius,seenineblastradius;
STATUSBARTYPE sbar;

long myminlag[MAXPLAYERS], mymaxlag, otherminlag, bufferjitter = 1;
short numclouds,clouds[128],cloudx[128],cloudy[128];
long cloudtotalclock = 0,totalmemory = 0;
long numinterpolations = 0, startofdynamicinterpolations = 0;
long oldipos[MAXINTERPOLATIONS];
long bakipos[MAXINTERPOLATIONS];
long *curipos[MAXINTERPOLATIONS];

int nextvoxid = 0;

int spriteflags[MAXTILES], actorspriteflags[MAXSPRITES];

proj_struct projectile[MAXTILES], thisprojectile[MAXSPRITES], defaultprojectile[MAXTILES];

char cheatkey[2] = { sc_D, sc_N };
