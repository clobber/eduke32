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

/*
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
*/

#include "duke3d.h"
#include "scriplib.h"
#include "osd.h"

#include "baselayer.h"

// we load this in to get default button and key assignments
// as well as setting up function mappings

#define __SETUP__   // JBF 20031211
#include "_functio.h"

//
// Sound variables
//
int32 FXDevice;
int32 MusicDevice;
int32 FXVolume;
int32 MusicVolume;
int32 SoundToggle;
int32 MusicToggle;
int32 VoiceToggle;
int32 AmbienceToggle;
//fx_blaster_config BlasterConfig;
int32 NumVoices;
int32 NumChannels;
int32 NumBits;
int32 MixRate;
//int32 MidiPort;
int32 ReverseStereo;

int32 UseJoystick = 0, UseMouse = 1;
int32 RunMode;
int32 AutoAim;  // JBF 20031125
int32 ShowOpponentWeapons;
int32 MouseFilter,MouseBias;
int32 SmoothInput;

// JBF 20031211: Store the input settings because
// (currently) jmact can't regurgitate them
byte KeyboardKeys[NUMGAMEFUNCTIONS][2];
int32 MouseFunctions[MAXMOUSEBUTTONS][2];
int32 MouseDigitalFunctions[MAXMOUSEAXES][2];
int32 MouseAnalogueAxes[MAXMOUSEAXES];
int32 MouseAnalogueScale[MAXMOUSEAXES];
int32 JoystickFunctions[MAXJOYBUTTONS][2];
int32 JoystickDigitalFunctions[MAXJOYAXES][2];
int32 JoystickAnalogueAxes[MAXJOYAXES];
int32 JoystickAnalogueScale[MAXJOYAXES];
int32 JoystickAnalogueDead[MAXJOYAXES];
int32 JoystickAnalogueSaturate[MAXJOYAXES];

//
// Screen variables
//

int32 ScreenMode = 1;

#if defined(POLYMOST) && defined(USE_OPENGL)
int32 ScreenWidth = 1024;
int32 ScreenHeight = 768;
int32 ScreenBPP = 32;
#else
int32 ScreenWidth = 800;
int32 ScreenHeight = 600;
int32 ScreenBPP = 8;
#endif
int32 ForceSetup = 1;

static char setupfilename[256]={SETUPFILENAME};
int32 scripthandle = -1;
static int32 setupread=0;

/*
===================
=
= CONFIG_FunctionNameToNum
=
===================
*/

int32 CONFIG_FunctionNameToNum( char * func )
{
    int32 i;

    for (i=0;i<NUMGAMEFUNCTIONS;i++)
    {
        if (!Bstrcasecmp(func,gamefunctions[i]))
        {
            return i;
        }
    }
    return -1;
}

/*
===================
=
= CONFIG_FunctionNumToName
=
===================
*/

char * CONFIG_FunctionNumToName( int32 func )
{
    if ((unsigned)func >= (unsigned)NUMGAMEFUNCTIONS)
    {
        return NULL;
    }
    else
    {
        return gamefunctions[func];
    }
}

/*
===================
=
= CONFIG_AnalogNameToNum
=
===================
*/


int32 CONFIG_AnalogNameToNum( char * func )
{

    if (!Bstrcasecmp(func,"analog_turning"))
    {
        return analog_turning;
    }
    if (!Bstrcasecmp(func,"analog_strafing"))
    {
        return analog_strafing;
    }
    if (!Bstrcasecmp(func,"analog_moving"))
    {
        return analog_moving;
    }
    if (!Bstrcasecmp(func,"analog_lookingupanddown"))
    {
        return analog_lookingupanddown;
    }

    return -1;
}


char * CONFIG_AnalogNumToName( int32 func )
{
    switch (func) {
    case analog_turning:
        return "analog_turning";
    case analog_strafing:
        return "analog_strafing";
    case analog_moving:
        return "analog_moving";
    case analog_lookingupanddown:
        return "analog_lookingupanddown";
    }

    return NULL;
}


/*
===================
=
= CONFIG_SetDefaults
=
===================
*/

void CONFIG_SetDefaults( void )
{
    // JBF 20031211
    int32 i,f;

    AmbienceToggle = 1;
    AutoAim = 1;
    FXDevice = 0;
    FXVolume = 220;
    MixRate = 44100;
    MouseBias = 0;
    MouseFilter = 0;
    MusicDevice = 0;
    MusicToggle = 1;
    MusicVolume = 200;
    myaimmode = ps[0].aim_mode = 1;
    NumBits = 16;
    NumChannels = 2;
    NumVoices = 32;
    ReverseStereo = 0;
    RunMode = ud.auto_run = 1;
    ShowOpponentWeapons = 0;
    SmoothInput = 1;
    SoundToggle = 1;
    ud.automsg = 0;
    ud.autovote = 0;
    ud.brightness = 8;
    ud.color = 0;
    ud.crosshair = 2;
    ud.democams = 1;
    ud.detail = 1;
    ud.drawweapon = 1;
    ud.idplayers = 1;
    ud.levelstats = 0;
    ud.lockout = 0;
    ud.m_ffire = 1;
    ud.m_marker = 1;
    ud.mouseaiming = 0;
    ud.mouseflip = 1;
    ud.msgdisptime = 120;
    ud.pwlockout[0] = '\0';
    ud.runkey_mode = 0;
    ud.screen_size = 4;
    ud.screen_tilting = 1;
    ud.shadows = 1;
    ud.statusbarmode = 0;
    ud.statusbarscale = 100;
    ud.weaponswitch = 3;	// new+empty
    UseJoystick = 0;
    UseMouse = 1;
    VoiceToggle = 2;

    Bstrcpy(ud.rtsname, "DUKE.RTS");
    Bstrcpy(myname, "Duke");

    Bstrcpy(ud.ridecule[0], "An inspiration for birth control.");
    Bstrcpy(ud.ridecule[1], "You're gonna die for that!");
    Bstrcpy(ud.ridecule[2], "It hurts to be you.");
    Bstrcpy(ud.ridecule[3], "Lucky Son of a Bitch.");
    Bstrcpy(ud.ridecule[4], "Hmmm....Payback time.");
    Bstrcpy(ud.ridecule[5], "You bottom dwelling scum sucker.");
    Bstrcpy(ud.ridecule[6], "Damn, you're ugly.");
    Bstrcpy(ud.ridecule[7], "Ha ha ha...Wasted!");
    Bstrcpy(ud.ridecule[8], "You suck!");
    Bstrcpy(ud.ridecule[9], "AARRRGHHHHH!!!");

    // JBF 20031211
    memset(KeyboardKeys, 0xff, sizeof(KeyboardKeys));
    for (i=0; i < (int32)(sizeof(keydefaults)/sizeof(keydefaults[0])); i+=3) {
        f = CONFIG_FunctionNameToNum( keydefaults[i+0] );
        if (f == -1) continue;
        KeyboardKeys[f][0] = KB_StringToScanCode( keydefaults[i+1] );
        KeyboardKeys[f][1] = KB_StringToScanCode( keydefaults[i+2] );

        if (f == gamefunc_Show_Console) OSD_CaptureKey(KeyboardKeys[f][0]);
        else CONTROL_MapKey( f, KeyboardKeys[f][0], KeyboardKeys[f][1] );
    }

    memset(MouseFunctions, -1, sizeof(MouseFunctions));
    for (i=0; i<MAXMOUSEBUTTONS; i++) {
        MouseFunctions[i][0] = CONFIG_FunctionNameToNum( mousedefaults[i] );
        CONTROL_MapButton( MouseFunctions[i][0], i, 0, controldevice_mouse );
        if (i>=4) continue;
        MouseFunctions[i][1] = CONFIG_FunctionNameToNum( mouseclickeddefaults[i] );
        CONTROL_MapButton( MouseFunctions[i][1], i, 1, controldevice_mouse );
    }

    memset(MouseDigitalFunctions, -1, sizeof(MouseDigitalFunctions));
    for (i=0; i<MAXMOUSEAXES; i++) {
        MouseAnalogueScale[i] = 65536;
        CONTROL_SetAnalogAxisScale( i, MouseAnalogueScale[i], controldevice_mouse );

        MouseDigitalFunctions[i][0] = CONFIG_FunctionNameToNum( mousedigitaldefaults[i*2] );
        MouseDigitalFunctions[i][1] = CONFIG_FunctionNameToNum( mousedigitaldefaults[i*2+1] );
        CONTROL_MapDigitalAxis( i, MouseDigitalFunctions[i][0], 0, controldevice_mouse );
        CONTROL_MapDigitalAxis( i, MouseDigitalFunctions[i][1], 1, controldevice_mouse );

        MouseAnalogueAxes[i] = CONFIG_AnalogNameToNum( mouseanalogdefaults[i] );
        CONTROL_MapAnalogAxis( i, MouseAnalogueAxes[i], controldevice_mouse);
    }
    CONTROL_SetMouseSensitivity(DEFAULTMOUSESENSITIVITY);

    memset(JoystickFunctions, -1, sizeof(JoystickFunctions));
    for (i=0; i<MAXJOYBUTTONS; i++) {
        JoystickFunctions[i][0] = CONFIG_FunctionNameToNum( joystickdefaults[i] );
        JoystickFunctions[i][1] = CONFIG_FunctionNameToNum( joystickclickeddefaults[i] );
        CONTROL_MapButton( JoystickFunctions[i][0], i, 0, controldevice_joystick );
        CONTROL_MapButton( JoystickFunctions[i][1], i, 1, controldevice_joystick );
    }

    memset(JoystickDigitalFunctions, -1, sizeof(JoystickDigitalFunctions));
    for (i=0; i<MAXJOYAXES; i++) {
        JoystickAnalogueScale[i] = 65536;
        JoystickAnalogueDead[i] = 1000;
        JoystickAnalogueSaturate[i] = 9500;
        CONTROL_SetAnalogAxisScale( i, JoystickAnalogueScale[i], controldevice_joystick );

        JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum( joystickdigitaldefaults[i*2] );
        JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum( joystickdigitaldefaults[i*2+1] );
        CONTROL_MapDigitalAxis( i, JoystickDigitalFunctions[i][0], 0, controldevice_joystick );
        CONTROL_MapDigitalAxis( i, JoystickDigitalFunctions[i][1], 1, controldevice_joystick );

        JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum( joystickanalogdefaults[i] );
        CONTROL_MapAnalogAxis(i, JoystickAnalogueAxes[i], controldevice_joystick);
    }
}
/*
===================
=
= CONFIG_ReadKeys
=
===================
*/

void CONFIG_ReadKeys( void )
{
    int32 i;
    int32 numkeyentries;
    int32 function;
    char keyname1[80];
    char keyname2[80];
    kb_scancode key1,key2;

    if (scripthandle < 0) return;

    numkeyentries = SCRIPT_NumberEntries( scripthandle,"KeyDefinitions" );

    for (i=0;i<numkeyentries;i++)
    {
        function = CONFIG_FunctionNameToNum(SCRIPT_Entry(scripthandle,"KeyDefinitions", i ));
        if (function != -1)
        {
            memset(keyname1,0,sizeof(keyname1));
            memset(keyname2,0,sizeof(keyname2));
            SCRIPT_GetDoubleString
            (
                scripthandle,
                "KeyDefinitions",
                SCRIPT_Entry( scripthandle, "KeyDefinitions", i ),
                keyname1,
                keyname2
            );
            key1 = 0xff;
            key2 = 0xff;
            if (keyname1[0])
            {
                key1 = (byte) KB_StringToScanCode( keyname1 );
            }
            if (keyname2[0])
            {
                key2 = (byte) KB_StringToScanCode( keyname2 );
            }
            KeyboardKeys[function][0] = key1;
            KeyboardKeys[function][1] = key2;
        }
    }

    for (i=0; i<NUMGAMEFUNCTIONS; i++)
    {
        if (i == gamefunc_Show_Console)
            OSD_CaptureKey(KeyboardKeys[i][0]);
        else
            CONTROL_MapKey( i, KeyboardKeys[i][0], KeyboardKeys[i][1] );
    }
}


/*
===================
=
= CONFIG_SetupMouse
=
===================
*/

void CONFIG_SetupMouse( void )
{
    int32 i;
    char str[80];
    char temp[80];
    int32 function, scale;

    if (scripthandle < 0) return;

    for (i=0;i<MAXMOUSEBUTTONS;i++)
    {
        Bsprintf(str,"MouseButton%ld",i); temp[0] = 0;
        if (!SCRIPT_GetString( scripthandle,"Controls", str,temp))
            MouseFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"MouseButtonClicked%ld",i); temp[0] = 0;
        if (!SCRIPT_GetString( scripthandle,"Controls", str,temp))
            MouseFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    // map over the axes
    for (i=0;i<MAXMOUSEAXES;i++)
    {
        Bsprintf(str,"MouseAnalogAxes%ld",i); temp[0] = 0;
        if (!SCRIPT_GetString(scripthandle, "Controls", str,temp))
            MouseAnalogueAxes[i] = CONFIG_AnalogNameToNum(temp);

        Bsprintf(str,"MouseDigitalAxes%ld_0",i); temp[0] = 0;
        if (!SCRIPT_GetString(scripthandle, "Controls", str,temp))
            MouseDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"MouseDigitalAxes%ld_1",i); temp[0] = 0;
        if (!SCRIPT_GetString(scripthandle, "Controls", str,temp))
            MouseDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"MouseAnalogScale%ld",i);
        scale = MouseAnalogueScale[i];
        SCRIPT_GetNumber(scripthandle, "Controls", str,&scale);
        MouseAnalogueScale[i] = scale;
    }

    function = DEFAULTMOUSESENSITIVITY;
    SCRIPT_GetNumber( scripthandle, "Controls","Mouse_Sensitivity",&function);
    CONTROL_SetMouseSensitivity(function);

    for (i=0; i<MAXMOUSEBUTTONS; i++)
    {
        CONTROL_MapButton( MouseFunctions[i][0], i, 0, controldevice_mouse );
        CONTROL_MapButton( MouseFunctions[i][1], i, 1,  controldevice_mouse );
    }
    for (i=0; i<MAXMOUSEAXES; i++)
    {
        CONTROL_MapAnalogAxis( i, MouseAnalogueAxes[i], controldevice_mouse);
        CONTROL_MapDigitalAxis( i, MouseDigitalFunctions[i][0], 0,controldevice_mouse );
        CONTROL_MapDigitalAxis( i, MouseDigitalFunctions[i][1], 1,controldevice_mouse );
        CONTROL_SetAnalogAxisScale( i, MouseAnalogueScale[i], controldevice_mouse );
    }
}

/*
===================
=
= CONFIG_SetupJoystick
=
===================
*/

void CONFIG_SetupJoystick( void )
{
    int32 i;
    char str[80];
    char temp[80];
    int32 scale;

    if (scripthandle < 0) return;

    for (i=0;i<MAXJOYBUTTONS;i++)
    {
        Bsprintf(str,"JoystickButton%ld",i); temp[0] = 0;
        if (!SCRIPT_GetString( scripthandle,"Controls", str,temp))
            JoystickFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"JoystickButtonClicked%ld",i); temp[0] = 0;
        if (!SCRIPT_GetString( scripthandle,"Controls", str,temp))
            JoystickFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    // map over the axes
    for (i=0;i<MAXJOYAXES;i++)
    {
        Bsprintf(str,"JoystickAnalogAxes%ld",i); temp[0] = 0;
        if (!SCRIPT_GetString(scripthandle, "Controls", str,temp))
            JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum(temp);

        Bsprintf(str,"JoystickDigitalAxes%ld_0",i); temp[0] = 0;
        if (!SCRIPT_GetString(scripthandle, "Controls", str,temp))
            JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"JoystickDigitalAxes%ld_1",i); temp[0] = 0;
        if (!SCRIPT_GetString(scripthandle, "Controls", str,temp))
            JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"JoystickAnalogScale%ld",i);
        scale = JoystickAnalogueScale[i];
        SCRIPT_GetNumber(scripthandle, "Controls", str,&scale);
        JoystickAnalogueScale[i] = scale;

        Bsprintf(str,"JoystickAnalogDead%ld",i);
        scale = JoystickAnalogueDead[i];
        SCRIPT_GetNumber(scripthandle, "Controls", str,&scale);
        JoystickAnalogueDead[i] = scale;

        Bsprintf(str,"JoystickAnalogSaturate%ld",i);
        scale = JoystickAnalogueSaturate[i];
        SCRIPT_GetNumber(scripthandle, "Controls", str,&scale);
        JoystickAnalogueSaturate[i] = scale;
    }

    for (i=0;i<MAXJOYBUTTONS;i++)
    {
        CONTROL_MapButton( JoystickFunctions[i][0], i, 0, controldevice_joystick );
        CONTROL_MapButton( JoystickFunctions[i][1], i, 1,  controldevice_joystick );
    }
    for (i=0;i<MAXJOYAXES;i++)
    {
        CONTROL_MapAnalogAxis(i, JoystickAnalogueAxes[i], controldevice_joystick);
        CONTROL_MapDigitalAxis( i, JoystickDigitalFunctions[i][0], 0, controldevice_joystick );
        CONTROL_MapDigitalAxis( i, JoystickDigitalFunctions[i][1], 1, controldevice_joystick );
        CONTROL_SetAnalogAxisScale( i, JoystickAnalogueScale[i], controldevice_joystick );
    }
}

void readsavenames(void)
{
    long dummy,j;
    short i;
    char fn[13];
    BFILE *fil;

    Bstrcpy(fn,"egam_.sav");

    for (i=0;i<10;i++)
    {
        fn[4] = i+'0';
        if ((fil = Bfopen(fn,"rb")) == NULL ) continue;
    if(dfread(&j,sizeof(long),1,fil) != 1) { Bfclose(fil); continue; }
        if(dfread(g_szBuf,j,1,fil) != 1) { Bfclose(fil); continue; }
        if (dfread(&dummy,4,1,fil) != 1) { Bfclose(fil); continue; }
        if(dummy != BYTEVERSION) return;
    if (dfread(&dummy,4,1,fil) != 1) { Bfclose(fil); continue; }
        if (dfread(&ud.savegame[i][0],19,1,fil) != 1) { ud.savegame[i][0] = 0; }
        Bfclose(fil);
    }
}

/*
===================
=
= CONFIG_ReadSetup
=
===================
*/

int32 CONFIG_ReadSetup( void )
{
    int32 dummy, i;
    char commmacro[] = "CommbatMacro# ";
    extern int32 CommandWeaponChoice;

    CONTROL_ClearAssignments();
    CONFIG_SetDefaults();

    setupread = 1;

    if (SafeFileExists(setupfilename))  // JBF 20031211
        scripthandle = SCRIPT_Load( setupfilename );

    if (scripthandle < 0) return -1;

    if (scripthandle >= 0)
    {
        for(dummy = 0;dummy < 10;dummy++)
        {
            commmacro[13] = dummy+'0';
            SCRIPT_GetString( scripthandle, "Comm Setup",commmacro,&ud.ridecule[dummy][0]);
        }

        SCRIPT_GetString( scripthandle, "Comm Setup","PlayerName",&tempbuf[0]);

        while(Bstrlen(strip_color_codes(tempbuf)) > 10)
            tempbuf[Bstrlen(tempbuf)-1] = '\0';

        Bstrncpy(myname,tempbuf,sizeof(myname)-1);
        myname[sizeof(myname)] = '\0';

        SCRIPT_GetString( scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

        SCRIPT_GetNumber( scripthandle, "Screen Setup", "Shadows",&ud.shadows);
        SCRIPT_GetString( scripthandle, "Screen Setup","Password",&ud.pwlockout[0]);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "Detail",&ud.detail);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "Tilt",&ud.screen_tilting);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "Messages",&ud.fta_on);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "ScreenWidth",&ScreenWidth);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "ScreenHeight",&ScreenHeight);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "ScreenMode",&ScreenMode);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "ScreenGamma",&ud.brightness);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "ScreenSize",&ud.screen_size);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "Out",&ud.lockout);

#if defined(POLYMOST) && defined(USE_OPENGL)
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "ScreenBPP", &ScreenBPP);
        if (ScreenBPP < 8) ScreenBPP = 32;
#endif

#ifdef RENDERTYPEWIN
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "MaxRefreshFreq", (int32*)&maxrefreshfreq);
#endif
#if defined(POLYMOST) && defined(USE_OPENGL)
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "GLTextureMode", &gltexfiltermode);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "GLAnisotropy", &glanisotropy);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "GLProjectionFix", &glprojectionhacks);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "GLUseTextureCompr", &glusetexcompr);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "GLWidescreen", &glwidescreen);

        glusetexcache = glusetexcachecompression = -1;
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "GLUseCompressedTextureCache", &glusetexcache);
        SCRIPT_GetNumber( scripthandle, "Screen Setup", "GLUseTextureCacheCompression", &glusetexcachecompression);
        if(glusetexcache == -1 || glusetexcachecompression == -1)
        {
            i=wm_ynbox("Texture caching",
                       "Would you like to enable the on-disk texture cache? "
                       "This feature may use up to 200 megabytes of disk "
                       "space if you have a great deal of high resolution "
                       "textures and skins, but textures will load exponentially "
                       "faster after the first time they are loaded.");
            if (i) i = 'y';
            if(i == 'y' || i == 'Y' )
                useprecache = glusetexcompr = glusetexcache = glusetexcachecompression = 1;
            else glusetexcache = glusetexcachecompression = 0;
        }
#endif
        dummy = usemodels; SCRIPT_GetNumber( scripthandle, "Screen Setup", "UseModels",&dummy); usemodels = dummy != 0;
        dummy = usehightile; SCRIPT_GetNumber( scripthandle, "Screen Setup", "UseHightile",&dummy); usehightile = dummy != 0;

        SCRIPT_GetNumber( scripthandle, "Misc", "Executions",&ud.executions);
        ud.executions++;
        SCRIPT_GetNumber( scripthandle, "Setup", "ForceSetup",&ForceSetup);
        SCRIPT_GetNumber( scripthandle, "Misc", "RunMode",&RunMode);
        SCRIPT_GetNumber( scripthandle, "Misc", "Crosshairs",&ud.crosshair);
        SCRIPT_GetNumber( scripthandle, "Misc", "StatusBarScale",&ud.statusbarscale);
        SCRIPT_GetNumber( scripthandle, "Misc", "ShowLevelStats",&ud.levelstats);
        SCRIPT_GetNumber( scripthandle, "Misc", "ShowOpponentWeapons",&ShowOpponentWeapons);
        ud.showweapons = ShowOpponentWeapons;
        SCRIPT_GetNumber( scripthandle, "Misc", "ShowViewWeapon",&ud.drawweapon);
        SCRIPT_GetNumber( scripthandle, "Misc", "DemoCams",&ud.democams);
        SCRIPT_GetNumber( scripthandle, "Misc", "ShowFPS",&ud.tickrate);
        SCRIPT_GetNumber( scripthandle, "Misc", "Color",&ud.color);
        ps[0].palookup = ud.pcolor[0] = ud.color;
        SCRIPT_GetNumber( scripthandle, "Misc", "MPMessageDisplayTime",&ud.msgdisptime);
        SCRIPT_GetNumber( scripthandle, "Misc", "StatusBarMode",&ud.statusbarmode);
        SCRIPT_GetNumber( scripthandle, "Misc", "AutoVote",&ud.autovote);
        SCRIPT_GetNumber( scripthandle, "Misc", "AutoMsg",&ud.automsg);
        SCRIPT_GetNumber( scripthandle, "Misc", "IDPlayers",&ud.automsg);

        dummy = useprecache; SCRIPT_GetNumber( scripthandle, "Misc", "UsePrecache",&dummy); useprecache = dummy != 0;

        // weapon choices are defaulted in checkcommandline, which may override them
        if (!CommandWeaponChoice)
            for(i=0;i<10;i++)
            {
                Bsprintf(buf,"WeaponChoice%ld",i);
                dummy = -1;
                SCRIPT_GetNumber( scripthandle, "Misc", buf, &dummy);
                if (dummy >= 0) ud.wchoice[0][i] = dummy;
            }

        SCRIPT_GetNumber( scripthandle, "Sound Setup", "FXDevice",&FXDevice);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "MusicDevice",&MusicDevice);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "FXVolume",&FXVolume);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "MusicVolume",&MusicVolume);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "SoundToggle",&SoundToggle);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "MusicToggle",&MusicToggle);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "VoiceToggle",&VoiceToggle);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "AmbienceToggle",&AmbienceToggle);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "NumVoices",&NumVoices);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "NumChannels",&NumChannels);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "NumBits",&NumBits);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "MixRate",&MixRate);
        SCRIPT_GetNumber( scripthandle, "Sound Setup", "ReverseStereo",&ReverseStereo);

        SCRIPT_GetNumber( scripthandle, "Controls","MouseAimingFlipped",&ud.mouseflip); // mouse aiming inverted
        SCRIPT_GetNumber( scripthandle, "Controls","MouseAiming",&ud.mouseaiming);		// 1=momentary/0=toggle
        ps[0].aim_mode = ud.mouseaiming;
        SCRIPT_GetNumber( scripthandle, "Controls","MouseBias",&MouseBias);
        SCRIPT_GetNumber( scripthandle, "Controls","MouseFilter",&MouseFilter);
        SCRIPT_GetNumber( scripthandle, "Controls","SmoothInput",&SmoothInput);
        SCRIPT_GetNumber( scripthandle, "Controls","UseJoystick",&UseJoystick);
        SCRIPT_GetNumber( scripthandle, "Controls","UseMouse",&UseMouse);
        SCRIPT_GetNumber( scripthandle, "Controls","AimingFlag",(int32 *)&myaimmode);   // (if toggle mode) gives state
        SCRIPT_GetNumber( scripthandle, "Controls","RunKeyBehaviour",&ud.runkey_mode);  // JBF 20031125
        SCRIPT_GetNumber( scripthandle, "Controls","AutoAim",&AutoAim);         // JBF 20031125
        ps[0].auto_aim = AutoAim;
        SCRIPT_GetNumber( scripthandle, "Controls","WeaponSwitchMode",&ud.weaponswitch);
        ps[0].weaponswitch = ud.weaponswitch;
    }

    CONFIG_ReadKeys();

    //CONFIG_SetupMouse(scripthandle);
    //CONFIG_SetupJoystick(scripthandle);
    setupread = 1;
    return 0;
}

/*
===================
=
= CONFIG_WriteSetup
=
===================
*/

void CONFIG_WriteSetup( void )
{
    int32 dummy;

    if (!setupread) return;

    if (scripthandle < 0)
        scripthandle = SCRIPT_Init(setupfilename);

    SCRIPT_PutNumber( scripthandle, "Controls","AimingFlag",(long) myaimmode,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","AutoAim",AutoAim,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","MouseAimingFlipped",ud.mouseflip,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","MouseAiming",ud.mouseaiming,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","MouseBias",MouseBias,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","MouseFilter",MouseFilter,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","SmoothInput",SmoothInput,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","RunKeyBehaviour",ud.runkey_mode,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","UseJoystick",UseJoystick,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","UseMouse",UseMouse,false,false);
    SCRIPT_PutNumber( scripthandle, "Controls","WeaponSwitchMode",ud.weaponswitch,false,false);

    SCRIPT_PutNumber( scripthandle, "Misc", "AutoMsg",ud.automsg,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "AutoVote",ud.autovote,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "Color",ud.color,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "Crosshairs",ud.crosshair,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "DemoCams",ud.democams,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "Executions",ud.executions,false,false);
    SCRIPT_PutNumber( scripthandle, "Setup", "ForceSetup",ForceSetup,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "IDPlayers",ud.idplayers,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "MPMessageDisplayTime",ud.msgdisptime,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "RunMode",RunMode,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "ShowFPS",ud.tickrate,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "ShowLevelStats",ud.levelstats,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "ShowOpponentWeapons",ShowOpponentWeapons,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "ShowViewWeapon",ud.drawweapon,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "StatusBarMode",ud.statusbarmode,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "StatusBarScale",ud.statusbarscale,false,false);
    SCRIPT_PutNumber( scripthandle, "Misc", "UsePrecache",useprecache,false,false);

    SCRIPT_PutNumber( scripthandle, "Screen Setup", "Detail",ud.detail,false,false);
#if defined(POLYMOST) && defined(USE_OPENGL)
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "GLAnisotropy",glanisotropy,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "GLProjectionFix",glprojectionhacks,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "GLTextureMode",gltexfiltermode,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "GLUseCompressedTextureCache", glusetexcache,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "GLUseTextureCacheCompression", glusetexcachecompression,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "GLUseTextureCompr",glusetexcompr,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "GLWidescreen",glwidescreen,false,false);
#endif
#ifdef RENDERTYPEWIN
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "MaxRefreshFreq",maxrefreshfreq,false,false);
#endif
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "Messages",ud.fta_on,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "Out",ud.lockout,false,false);
    SCRIPT_PutString( scripthandle, "Screen Setup", "Password",ud.pwlockout);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "ScreenBPP",ScreenBPP,false,false); // JBF 20040523
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "ScreenGamma",ud.brightness,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "ScreenHeight",ScreenHeight,false,false);   // JBF 20031206
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "ScreenMode",ScreenMode,false,false);   // JBF 20031206
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "ScreenSize",ud.screen_size,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "ScreenWidth",ScreenWidth,false,false); // JBF 20031206
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "Shadows",ud.shadows,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "Tilt",ud.screen_tilting,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "UseHightile",usehightile,false,false);
    SCRIPT_PutNumber( scripthandle, "Screen Setup", "UseModels",usemodels,false,false);

    SCRIPT_PutNumber( scripthandle, "Sound Setup", "AmbienceToggle",AmbienceToggle,false,false);
    SCRIPT_PutNumber( scripthandle, "Sound Setup", "FXVolume",FXVolume,false,false);
    SCRIPT_PutNumber( scripthandle, "Sound Setup", "MusicToggle",MusicToggle,false,false);
    SCRIPT_PutNumber( scripthandle, "Sound Setup", "MusicVolume",MusicVolume,false,false);
    SCRIPT_PutNumber( scripthandle, "Sound Setup", "ReverseStereo",ReverseStereo,false,false);
    SCRIPT_PutNumber( scripthandle, "Sound Setup", "SoundToggle",SoundToggle,false,false);
    SCRIPT_PutNumber( scripthandle, "Sound Setup", "VoiceToggle",VoiceToggle,false,false);

    // JBF 20031211
    for(dummy=0;dummy<NUMGAMEFUNCTIONS;dummy++) {
        SCRIPT_PutDoubleString( scripthandle, "KeyDefinitions", CONFIG_FunctionNumToName(dummy),
                                KB_ScanCodeToString(KeyboardKeys[dummy][0]), KB_ScanCodeToString(KeyboardKeys[dummy][1]));
    }

    for(dummy=0;dummy<10;dummy++)
    {
        Bsprintf(buf,"WeaponChoice%ld",dummy);
        SCRIPT_PutNumber( scripthandle, "Misc",buf,ud.wchoice[myconnectindex][dummy],false,false);
    }

    for (dummy=0;dummy<MAXMOUSEBUTTONS;dummy++) {
        Bsprintf(buf,"MouseButton%ld",dummy);
        SCRIPT_PutString( scripthandle,"Controls", buf, CONFIG_FunctionNumToName( MouseFunctions[dummy][0] ));

        if (dummy >= (MAXMOUSEBUTTONS-2)) continue;

        Bsprintf(buf,"MouseButtonClicked%ld",dummy);
        SCRIPT_PutString( scripthandle,"Controls", buf, CONFIG_FunctionNumToName( MouseFunctions[dummy][1] ));
    }
    for (dummy=0;dummy<MAXMOUSEAXES;dummy++) {
        Bsprintf(buf,"MouseAnalogAxes%ld",dummy);
        SCRIPT_PutString(scripthandle, "Controls", buf, CONFIG_AnalogNumToName( MouseAnalogueAxes[dummy] ));

        Bsprintf(buf,"MouseDigitalAxes%ld_0",dummy);
        SCRIPT_PutString(scripthandle, "Controls", buf, CONFIG_FunctionNumToName(MouseDigitalFunctions[dummy][0]));

        Bsprintf(buf,"MouseDigitalAxes%ld_1",dummy);
        SCRIPT_PutString(scripthandle, "Controls", buf, CONFIG_FunctionNumToName(MouseDigitalFunctions[dummy][1]));

        Bsprintf(buf,"MouseAnalogScale%ld",dummy);
        SCRIPT_PutNumber(scripthandle, "Controls", buf, MouseAnalogueScale[dummy], false, false);
    }
    dummy = CONTROL_GetMouseSensitivity();
    SCRIPT_PutNumber( scripthandle, "Controls","Mouse_Sensitivity",dummy,false,false);

    for (dummy=0;dummy<MAXJOYBUTTONS;dummy++) {
        Bsprintf(buf,"JoystickButton%ld",dummy);
        SCRIPT_PutString( scripthandle,"Controls", buf, CONFIG_FunctionNumToName( JoystickFunctions[dummy][0] ));

        Bsprintf(buf,"JoystickButtonClicked%ld",dummy);
        SCRIPT_PutString( scripthandle,"Controls", buf, CONFIG_FunctionNumToName( JoystickFunctions[dummy][1] ));
    }
    for (dummy=0;dummy<MAXJOYAXES;dummy++) {
        Bsprintf(buf,"JoystickAnalogAxes%ld",dummy);
        SCRIPT_PutString(scripthandle, "Controls", buf, CONFIG_AnalogNumToName( JoystickAnalogueAxes[dummy] ));

        Bsprintf(buf,"JoystickDigitalAxes%ld_0",dummy);
        SCRIPT_PutString(scripthandle, "Controls", buf, CONFIG_FunctionNumToName(JoystickDigitalFunctions[dummy][0]));

        Bsprintf(buf,"JoystickDigitalAxes%ld_1",dummy);
        SCRIPT_PutString(scripthandle, "Controls", buf, CONFIG_FunctionNumToName(JoystickDigitalFunctions[dummy][1]));

        Bsprintf(buf,"JoystickAnalogScale%ld",dummy);
        SCRIPT_PutNumber(scripthandle, "Controls", buf, JoystickAnalogueScale[dummy], false, false);

        Bsprintf(buf,"JoystickAnalogDead%ld",dummy);
        SCRIPT_PutNumber(scripthandle, "Controls", buf, JoystickAnalogueDead[dummy], false, false);

        Bsprintf(buf,"JoystickAnalogSaturate%ld",dummy);
        SCRIPT_PutNumber(scripthandle, "Controls", buf, JoystickAnalogueSaturate[dummy], false, false);
    }

    SCRIPT_PutString( scripthandle, "Comm Setup","PlayerName",&myname[0]);
    SCRIPT_PutString( scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

    {
        char commmacro[] = "CommbatMacro# ";

        for(dummy = 0;dummy < 10;dummy++)
        {
            commmacro[13] = dummy+'0';
            SCRIPT_PutString( scripthandle, "Comm Setup",commmacro,&ud.ridecule[dummy][0]);
        }
    }

    SCRIPT_Save (scripthandle, setupfilename);
    SCRIPT_Free (scripthandle);
}


/*
 * vim:ts=4:sw=4:
 */

