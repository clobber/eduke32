//
// Common non-engine code/data for EDuke32 and Mapster32
//

#include "compat.h"
#include "build.h"
#include "baselayer.h"

#include "grpscan.h"

#ifdef _WIN32
# include "winbits.h"
# include <shlwapi.h>
# include <winnt.h>
# ifndef KEY_WOW64_32KEY
#  define KEY_WOW64_32KEY 0x0200
# endif
#elif defined __APPLE__
# include "osxbits.h"
#endif

#include "common.h"
#include "common_game.h"

int32_t g_gameType = GAMEFLAG_DUKE;

int32_t g_dependencyCRC = 0;
int32_t g_usingAddon = 0;

// g_gameNamePtr can point to one of: grpfiles[].name (string literal), string
// literal, malloc'd block (XXX: possible leak)
const char *g_gameNamePtr = NULL;

// grp/con handling

static const char *defaultgamegrp[GAMECOUNT] = { "DUKE3D.GRP", "NAM.GRP", "NAPALM.GRP", "WW2GI.GRP" };
static const char *defaultdeffilename[GAMECOUNT] = { "duke3d.def", "nam.def", "napalm.def", "ww2gi.def" };
static const char *defaultconfilename = "GAME.CON";
static const char *defaultgameconfilename[GAMECOUNT] = { "EDUKE.CON", "NAM.CON", "NAPALM.CON", "WW2GI.CON" };

// g_grpNamePtr can ONLY point to a malloc'd block (length BMAX_PATH)
char *g_grpNamePtr = NULL;
// g_scriptNamePtr can ONLY point to a malloc'd block (length BMAX_PATH)
char *g_scriptNamePtr = NULL;

void clearGrpNamePtr(void)
{
    if (g_grpNamePtr != NULL)
        Bfree(g_grpNamePtr);
    // g_grpNamePtr assumed to be assigned to right after
}

void clearScriptNamePtr(void)
{
    if (g_scriptNamePtr != NULL)
        Bfree(g_scriptNamePtr);
    // g_scriptNamePtr assumed to be assigned to right after
}

const char *G_DefaultGrpFile(void)
{
    if (DUKE)
        return defaultgamegrp[GAME_DUKE];
    // order is important for the following three because GAMEFLAG_NAM overlaps all
    else if (NAPALM)
        return defaultgamegrp[GAME_NAPALM];
    else if (WW2GI)
        return defaultgamegrp[GAME_WW2GI];
    else if (NAM)
        return defaultgamegrp[GAME_NAM];

    return defaultgamegrp[0];
}
const char *G_DefaultDefFile(void)
{
    if (DUKE)
        return defaultdeffilename[GAME_DUKE];
    else if (WW2GI)
        return defaultdeffilename[GAME_WW2GI];
    else if (NAPALM)
    {
        if (!testkopen(defaultdeffilename[GAME_NAPALM],0) && testkopen(defaultdeffilename[GAME_NAM],0))
            return defaultdeffilename[GAME_NAM]; // NAM/NAPALM Sharing
        else
            return defaultdeffilename[GAME_NAPALM];
    }
    else if (NAM)
    {
        if (!testkopen(defaultdeffilename[GAME_NAM],0) && testkopen(defaultdeffilename[GAME_NAPALM],0))
            return defaultdeffilename[GAME_NAPALM]; // NAM/NAPALM Sharing
        else
            return defaultdeffilename[GAME_NAM];
    }

    return defaultdeffilename[0];
}
const char *G_DefaultConFile(void)
{
    if (DUKE && testkopen(defaultgameconfilename[GAME_DUKE],0))
        return defaultgameconfilename[GAME_DUKE];
    else if (WW2GI && testkopen(defaultgameconfilename[GAME_WW2GI],0))
        return defaultgameconfilename[GAME_WW2GI];
    else if (NAPALM)
    {
        if (!testkopen(defaultgameconfilename[GAME_NAPALM],0))
        {
            if (testkopen(defaultgameconfilename[GAME_NAM],0))
                return defaultgameconfilename[GAME_NAM]; // NAM/NAPALM Sharing
        }
        else
            return defaultgameconfilename[GAME_NAPALM];
    }
    else if (NAM)
    {
        if (!testkopen(defaultgameconfilename[GAME_NAM],0))
        {
            if (testkopen(defaultgameconfilename[GAME_NAPALM],0))
                return defaultgameconfilename[GAME_NAPALM]; // NAM/NAPALM Sharing
        }
        else
            return defaultgameconfilename[GAME_NAM];
    }

    return defaultconfilename;
}

const char *G_GrpFile(void)
{
    if (g_grpNamePtr == NULL)
        return G_DefaultGrpFile();
    else
        return g_grpNamePtr;
}

const char *G_DefFile(void)
{
    if (g_defNamePtr == NULL)
        return G_DefaultDefFile();
    else
        return g_defNamePtr;
}

const char *G_ConFile(void)
{
    if (g_scriptNamePtr == NULL)
        return G_DefaultConFile();
    else
        return g_scriptNamePtr;
}

//////////

#define NUMPSKYMULTIS 5
EDUKE32_STATIC_ASSERT(NUMPSKYMULTIS <= MAXPSKYMULTIS);
EDUKE32_STATIC_ASSERT(PSKYOFF_MAX <= MAXPSKYTILES);

// Set up new-style multi-psky handling.
void G_InitMultiPsky(int32_t CLOUDYOCEAN__DYN, int32_t MOONSKY1__DYN, int32_t BIGORBIT1__DYN, int32_t LA__DYN)
{
    int32_t i;

    psky_t *defaultsky = &multipsky[0];
    psky_t *oceansky = &multipsky[1];
    psky_t *moonsky = &multipsky[2];
    psky_t *spacesky = &multipsky[3];
    psky_t *citysky = &multipsky[4];

    static int32_t inited;
    if (inited)
        return;
    inited = 1;

    multipskytile[0] = -1;
    multipskytile[1] = CLOUDYOCEAN__DYN;
    multipskytile[2] = MOONSKY1__DYN;
    multipskytile[3] = BIGORBIT1__DYN;
    multipskytile[4] = LA__DYN;

    pskynummultis = NUMPSKYMULTIS;

    // When adding other multi-skies, take care that the tileofs[] values are
    // <= PSKYOFF_MAX. (It can be increased up to MAXPSKYTILES, but should be
    // set as tight as possible.)

    // The default sky properties (all others are implicitly zero):
    defaultsky->lognumtiles = 3;
    defaultsky->horizfrac = 32768;

    // CLOUDYOCEAN
    // Aligns with the drawn scene horizon because it has one itself.
    oceansky->lognumtiles = 3;
    oceansky->horizfrac = 65536;

    // MOONSKY1
    //        earth          mountain   mountain         sun
    moonsky->lognumtiles = 3;
    moonsky->horizfrac = 32768;
    moonsky->tileofs[6] = 1;
    moonsky->tileofs[1] = 2;
    moonsky->tileofs[4] = 2;
    moonsky->tileofs[2] = 3;

    // BIGORBIT1   // orbit
    //       earth1         2           3           moon/sun
    spacesky->lognumtiles = 3;
    spacesky->horizfrac = 32768;
    spacesky->tileofs[5] = 1;
    spacesky->tileofs[6] = 2;
    spacesky->tileofs[7] = 3;
    spacesky->tileofs[2] = 4;

    // LA // la city
    //       earth1         2           3           moon/sun
    citysky->lognumtiles = 3;
    citysky->horizfrac = 16384+1024;
    citysky->tileofs[0] = 1;
    citysky->tileofs[1] = 2;
    citysky->tileofs[2] = 1;
    citysky->tileofs[3] = 3;
    citysky->tileofs[4] = 4;
    citysky->tileofs[5] = 0;
    citysky->tileofs[6] = 2;
    citysky->tileofs[7] = 3;

    for (i=0; i<pskynummultis; ++i)
    {
        int32_t j;
        for (j=0; j<(1<<multipsky[i].lognumtiles); ++j)
            Bassert(multipsky[i].tileofs[j] <= PSKYOFF_MAX);
    }
}

void G_SetupGlobalPsky(void)
{
    int32_t i, mskyidx=0;

    // NOTE: Loop must be running backwards for the same behavior as the game
    // (greatest sector index with matching parallaxed sky takes precedence).
    for (i=numsectors-1; i>=0; i--)
    {
        if (sector[i].ceilingstat & 1)
        {
            mskyidx = getpskyidx(sector[i].ceilingpicnum);
            if (mskyidx > 0)
                break;
        }
    }

    g_pskyidx = mskyidx;
}

//////////

static char g_rootDir[BMAX_PATH];
char g_modDir[BMAX_PATH] = "/";

int32_t kopen4loadfrommod(const char *filename, char searchfirst)
{
    int32_t r=-1;

    if (g_modDir[0]!='/' || g_modDir[1]!=0)
    {
        static char fn[BMAX_PATH];

        Bsnprintf(fn, sizeof(fn), "%s/%s",g_modDir,filename);
        r = kopen4load(fn,searchfirst);
    }

    if (r < 0)
        r = kopen4load(filename,searchfirst);

    return r;
}

int32_t usecwd;
static void G_LoadAddon(void);
int32_t g_groupFileHandle;

void G_ExtPreInit(int32_t argc,const char **argv)
{
    usecwd = G_CheckCmdSwitch(argc, argv, "-usecwd");

#ifdef _WIN32
    GetModuleFileName(NULL,g_rootDir,BMAX_PATH);
    Bcorrectfilename(g_rootDir,1);
    //chdir(g_rootDir);
#else
    getcwd(g_rootDir,BMAX_PATH);
    strcat(g_rootDir,"/");
#endif
}

void G_ExtInit(void)
{
    char cwd[BMAX_PATH];

#ifdef __APPLE__
    char *appdir = Bgetappdir();
    addsearchpath(appdir);
    Bfree(appdir);
#endif

    if (getcwd(cwd,BMAX_PATH) && Bstrcmp(cwd,"/") != 0)
        addsearchpath(cwd);

    if (CommandPaths)
    {
        int32_t i;
        struct strllist *s;
        while (CommandPaths)
        {
            s = CommandPaths->next;
            i = addsearchpath(CommandPaths->str);
            if (i < 0)
            {
                initprintf("Failed adding %s for game data: %s\n", CommandPaths->str,
                           i==-1 ? "not a directory" : "no such directory");
            }

            Bfree(CommandPaths->str);
            Bfree(CommandPaths);
            CommandPaths = s;
        }
    }

#if defined(_WIN32)
    if (!access("user_profiles_enabled", F_OK))
#else
    if (usecwd == 0 && access("user_profiles_disabled", F_OK))
#endif
    {
        char *homedir;
        int32_t asperr;

        if ((homedir = Bgethomedir()))
        {
            Bsnprintf(cwd,sizeof(cwd),"%s/"
#if defined(_WIN32)
                      "EDuke32 Settings"
#elif defined(GEKKO)
                      "apps/eduke32"
#else
                      ".eduke32"
#endif
                      ,homedir);
            asperr = addsearchpath(cwd);
            if (asperr == -2)
            {
                if (Bmkdir(cwd,S_IRWXU) == 0) asperr = addsearchpath(cwd);
                else asperr = -1;
            }
            if (asperr == 0)
                Bchdir(cwd);
            Bfree(homedir);
        }
    }

    // JBF 20031220: Because it's annoying renaming GRP files whenever I want to test different game data
    if (g_grpNamePtr == NULL)
    {
        const char *cp = getenv("DUKE3DGRP");
        if (cp)
        {
            clearGrpNamePtr();
            g_grpNamePtr = dup_filename(cp);
            initprintf("Using \"%s\" as main GRP file\n", g_grpNamePtr);
        }
    }
}

void G_ExtPreStartupWindow(void)
{
    ScanGroups();
    {
        // try and identify the 'defaultgamegrp' in the set of GRPs.
        // if it is found, set up the environment accordingly for the game it represents.
        // if it is not found, choose the first GRP from the list
        struct grpfile *fg, *first = NULL;

        for (fg = foundgrps; fg; fg=fg->next)
        {
            struct grpfile *grp;
            for (grp = listgrps; grp; grp=grp->next)
                if (fg->crcval == grp->crcval) break;

            if (grp == NULL)
                continue;

            fg->game = grp->game;
            if (!first) first = fg;
            if (!Bstrcasecmp(fg->name, G_DefaultGrpFile()))
            {
                g_gameType = grp->game;
                g_gameNamePtr = grp->name;
                break;
            }
        }
        if (!fg && first)
        {
            if (g_grpNamePtr == NULL)
            {
                clearGrpNamePtr();
                g_grpNamePtr = dup_filename(first->name);
            }
            g_gameType = first->game;
            g_gameNamePtr = listgrps->name;
        }
        else if (!fg) g_gameNamePtr = NULL;
    }
}

void G_ExtPostStartupWindow(int32_t autoload)
{
    if (g_modDir[0] != '/')
    {
        char cwd[BMAX_PATH];

        Bstrcat(g_rootDir,g_modDir);
        addsearchpath(g_rootDir);
//        addsearchpath(mod_dir);

        if (getcwd(cwd,BMAX_PATH))
        {
            Bsprintf(cwd,"%s/%s",cwd,g_modDir);
            if (!Bstrcmp(g_rootDir, cwd))
            {
                if (addsearchpath(cwd) == -2)
                    if (Bmkdir(cwd,S_IRWXU) == 0) addsearchpath(cwd);
            }
        }

#ifdef USE_OPENGL
        Bsprintf(cwd,"%s/%s",g_modDir,TEXCACHEFILE);
        Bstrcpy(TEXCACHEFILE,cwd);
#endif
    }

    if (g_usingAddon)
        G_LoadAddon();

    {
        int32_t i;
        const char *grpfile = G_GrpFile();

        if (g_dependencyCRC)
        {
            struct grpfile * grp = FindGroup(g_dependencyCRC);
            if (grp)
            {
                if ((i = initgroupfile(grp->name)) == -1)
                    initprintf("Warning: could not find main data file \"%s\"!\n",grp->name);
                else
                    initprintf("Using \"%s\" as main game data file.\n", grp->name);
            }
        }

        if ((i = initgroupfile(grpfile)) == -1)
            initprintf("Warning: could not find main data file \"%s\"!\n",grpfile);
        else
            initprintf("Using \"%s\" as main game data file.\n", grpfile);

        if (autoload)
        {
            G_LoadGroupsInDir("autoload");

            if (i != -1)
                G_DoAutoload(grpfile);
        }
    }

    if (g_modDir[0] != '/')
        G_LoadGroupsInDir(g_modDir);

    if (g_defNamePtr == NULL)
    {
        const char *tmpptr = getenv("DUKE3DDEF");
        if (tmpptr)
        {
            clearDefNamePtr();
            g_defNamePtr = dup_filename(tmpptr);
            initprintf("Using \"%s\" as definitions file\n", g_defNamePtr);
        }
    }

    loaddefinitions_game(G_DefFile(), TRUE);

    {
        struct strllist *s;

        pathsearchmode = 1;
        while (CommandGrps)
        {
            int32_t j;

            s = CommandGrps->next;

            if ((j = initgroupfile(CommandGrps->str)) == -1)
                initprintf("Could not find file \"%s\".\n",CommandGrps->str);
            else
            {
                g_groupFileHandle = j;
                initprintf("Using file \"%s\" as game data.\n",CommandGrps->str);
                if (autoload)
                    G_DoAutoload(CommandGrps->str);
            }

            Bfree(CommandGrps->str);
            Bfree(CommandGrps);
            CommandGrps = s;
        }
        pathsearchmode = 0;
    }
}

#ifdef _WIN32
const char * G_GetInstallPath(int32_t insttype)
{
    static char spath[NUMINSTPATHS][BMAX_PATH];
    static int32_t success[NUMINSTPATHS] = { -1, -1, -1, -1, -1 };
    int32_t siz = BMAX_PATH;

    if (success[insttype] == -1)
    {
        HKEY HKLM32;
        LONG keygood = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NULL, 0, KEY_READ | KEY_WOW64_32KEY, &HKLM32);
        // KEY_WOW64_32KEY gets us around Wow6432Node on 64-bit builds

        if (keygood == ERROR_SUCCESS)
        {
            switch (insttype)
            {
            case INSTPATH_STEAM_DUKE3D:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 225140", "InstallLocation", NULL, spath[insttype], (LPDWORD)&siz);
                break;
            case INSTPATH_GOG_DUKE3D:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\GOG.com\\GOGDUKE3D", "PATH", NULL, spath[insttype], (LPDWORD)&siz);
                break;
            case INSTPATH_3DR_DUKE3D:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\3DRealms\\Duke Nukem 3D", NULL, NULL, spath[insttype], (LPDWORD)&siz);
                break;
            case INSTPATH_3DR_ANTH:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\3DRealms\\Anthology", NULL, NULL, spath[insttype], (LPDWORD)&siz);
                break;
            case INSTPATH_STEAM_NAM:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 329650", "InstallLocation", NULL, spath[insttype], (LPDWORD)&siz);
                break;
            }

            RegCloseKey(HKLM32);
        }
    }

    if (success[insttype] == ERROR_SUCCESS)
        return spath[insttype];

    return NULL;
}
#endif

static void G_LoadAddon(void)
{
    struct grpfile * grp;
    int32_t crc = 0;  // compiler-happy

    switch (g_usingAddon)
    {
    case ADDON_DUKEDC:
        crc = DUKEDC_CRC;
        break;
    case ADDON_NWINTER:
        crc = DUKENW_CRC;
        break;
    case ADDON_CARIBBEAN:
        crc = DUKECB_CRC;
        break;
    }

    if (!crc) return;

    grp = FindGroup(crc);

    if (grp && FindGroup(DUKE15_CRC))
    {
        clearGrpNamePtr();
        g_grpNamePtr = dup_filename(FindGroup(DUKE15_CRC)->name);

        G_AddGroup(grp->name);

        for (grp = listgrps; grp; grp=grp->next)
            if (crc == grp->crcval) break;

        if (grp != NULL && grp->scriptname)
        {
            clearScriptNamePtr();
            g_scriptNamePtr = dup_filename(grp->scriptname);
        }

        if (grp != NULL && grp->defname)
        {
            clearDefNamePtr();
            g_defNamePtr = dup_filename(grp->defname);
        }
    }
}

#if defined __APPLE__ || defined __linux__ || defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__
static void G_AddSteamPaths(const char *basepath)
{
    char buf[BMAX_PATH];

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot", basepath);
    addsearchpath(buf);

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot/addons/dc", basepath);
    addsearchpath(buf);

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot/addons/nw", basepath);
    addsearchpath(buf);

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot/addons/vacation", basepath);
    addsearchpath(buf);

#if defined __APPLE__
    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Nam/Nam.app/Contents/Resources/Nam.boxer/C.harddisk/NAM", basepath);
#else
    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Nam/NAM", basepath);
#endif
    addsearchpath(buf);
}

// A bare-bones "parser" for Valve's KeyValues VDF format.
// There is no guarantee this will function properly with ill-formed files.
static void KeyValues_SkipWhitespace(char **vdfbuf, char * const vdfbufend)
{
    while (((*vdfbuf)[0] == ' ' || (*vdfbuf)[0] == '\n' || (*vdfbuf)[0] == '\r' || (*vdfbuf)[0] == '\t' || (*vdfbuf)[0] == '\0') && *vdfbuf < vdfbufend)
        (*vdfbuf)++;

    // comments
    if ((*vdfbuf) + 2 < vdfbufend && (*vdfbuf)[0] == '/' && (*vdfbuf)[1] == '/')
    {
        while ((*vdfbuf)[0] != '\n' && (*vdfbuf)[0] != '\r' && *vdfbuf < vdfbufend)
            (*vdfbuf)++;

        KeyValues_SkipWhitespace(vdfbuf, vdfbufend);
    }
}
static void KeyValues_SkipToEndOfQuotedToken(char **vdfbuf, char * const vdfbufend)
{
    (*vdfbuf)++;
    while ((*vdfbuf)[0] != '\"' && (*vdfbuf)[-1] != '\\' && *vdfbuf < vdfbufend)
        (*vdfbuf)++;
}
static void KeyValues_SkipToEndOfUnquotedToken(char **vdfbuf, char * const vdfbufend)
{
    while ((*vdfbuf)[0] != ' ' && (*vdfbuf)[0] != '\n' && (*vdfbuf)[0] != '\r' && (*vdfbuf)[0] != '\t' && (*vdfbuf)[0] != '\0' && *vdfbuf < vdfbufend)
        (*vdfbuf)++;
}
static void KeyValues_SkipNextWhatever(char **vdfbuf, char * const vdfbufend)
{
    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);

    if (*vdfbuf == vdfbufend)
        return;

    if ((*vdfbuf)[0] == '{')
    {
        (*vdfbuf)++;
        do
        {
            KeyValues_SkipNextWhatever(vdfbuf, vdfbufend);
        }
        while ((*vdfbuf)[0] != '}');
        (*vdfbuf)++;
    }
    else if ((*vdfbuf)[0] == '\"')
        KeyValues_SkipToEndOfQuotedToken(vdfbuf, vdfbufend);
    else if ((*vdfbuf)[0] != '}')
        KeyValues_SkipToEndOfUnquotedToken(vdfbuf, vdfbufend);

    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);
}
static char* KeyValues_NormalizeToken(char **vdfbuf, char * const vdfbufend)
{
    char *token = *vdfbuf;

    if ((*vdfbuf)[0] == '\"' && *vdfbuf < vdfbufend)
    {
        token++;

        KeyValues_SkipToEndOfQuotedToken(vdfbuf, vdfbufend);
        (*vdfbuf)[0] = '\0';

        // account for escape sequences
        char *writeseeker = token, *readseeker = token;
        while (readseeker <= *vdfbuf)
        {
            if (readseeker[0] == '\\')
                readseeker++;

            writeseeker[0] = readseeker[0];

            writeseeker++;
            readseeker++;
        }

        return token;
    }

    KeyValues_SkipToEndOfUnquotedToken(vdfbuf, vdfbufend);
    (*vdfbuf)[0] = '\0';

    return token;
}
static void KeyValues_FindKey(char **vdfbuf, char * const vdfbufend, const char *token)
{
    char *ParentKey = KeyValues_NormalizeToken(vdfbuf, vdfbufend);
    if (token != NULL) // pass in NULL to find the next key instead of a specific one
        while (Bstrcmp(ParentKey, token) != 0 && *vdfbuf < vdfbufend)
        {
            KeyValues_SkipNextWhatever(vdfbuf, vdfbufend);
            ParentKey = KeyValues_NormalizeToken(vdfbuf, vdfbufend);
        }

    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);
}
static int32_t KeyValues_FindParentKey(char **vdfbuf, char * const vdfbufend, const char *token)
{
    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);

    // end of scope
    if ((*vdfbuf)[0] == '}')
        return 0;

    KeyValues_FindKey(vdfbuf, vdfbufend, token);

    // ignore the wrong type
    while ((*vdfbuf)[0] != '{' && *vdfbuf < vdfbufend)
    {
        KeyValues_SkipNextWhatever(vdfbuf, vdfbufend);
        KeyValues_FindKey(vdfbuf, vdfbufend, token);
    }

    if (*vdfbuf == vdfbufend)
        return 0;

    return 1;
}
static char* KeyValues_FindKeyValue(char **vdfbuf, char * const vdfbufend, const char *token)
{
    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);

    // end of scope
    if ((*vdfbuf)[0] == '}')
        return NULL;

    KeyValues_FindKey(vdfbuf, vdfbufend, token);

    // ignore the wrong type
    while ((*vdfbuf)[0] == '{' && *vdfbuf < vdfbufend)
    {
        KeyValues_SkipNextWhatever(vdfbuf, vdfbufend);
        KeyValues_FindKey(vdfbuf, vdfbufend, token);
    }

    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);

    if (*vdfbuf == vdfbufend)
        return NULL;

    return KeyValues_NormalizeToken(vdfbuf, vdfbufend);
}

static void G_ParseSteamKeyValuesForPaths(const char *vdf)
{
    int32_t fd = Bopen(vdf, BO_RDONLY);
    int32_t size = Bfilelength(fd);
    char *vdfbufstart, *vdfbuf, *vdfbufend;

    if (size <= 0)
        return;

    vdfbufstart = vdfbuf = (char*)Bmalloc(size);
    size = (int32_t)Bread(fd, vdfbuf, size);
    Bclose(fd);
    vdfbufend = vdfbuf + size;

    if (KeyValues_FindParentKey(&vdfbuf, vdfbufend, "LibraryFolders"))
    {
        char *result;
        vdfbuf++;
        while ((result = KeyValues_FindKeyValue(&vdfbuf, vdfbufend, NULL)) != NULL)
            G_AddSteamPaths(result);
    }

    Bfree(vdfbufstart);
}
#endif

void G_AddSearchPaths(void)
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    char buf[BMAX_PATH];
    char *homepath = Bgethomedir();

    Bsnprintf(buf, sizeof(buf), "%s/.steam/steam", homepath);
    G_AddSteamPaths(buf);

    Bsnprintf(buf, sizeof(buf), "%s/.steam/steam/steamapps/libraryfolders.vdf", homepath);
    G_ParseSteamKeyValuesForPaths(buf);

    Bfree(homepath);

    addsearchpath("/usr/share/games/jfduke3d");
    addsearchpath("/usr/local/share/games/jfduke3d");
    addsearchpath("/usr/share/games/eduke32");
    addsearchpath("/usr/local/share/games/eduke32");
#elif defined(__APPLE__)
    char buf[BMAX_PATH];
    int32_t i;
    char *applications[] = { osx_getapplicationsdir(0), osx_getapplicationsdir(1) };
    char *support[] = { osx_getsupportdir(0), osx_getsupportdir(1) };

    for (i = 0; i < 2; i++)
    {
        Bsnprintf(buf, sizeof(buf), "%s/Steam", support[i]);
        G_AddSteamPaths(buf);

        Bsnprintf(buf, sizeof(buf), "%s/Steam/steamapps/libraryfolders.vdf", support[i]);
        G_ParseSteamKeyValuesForPaths(buf);

        Bsnprintf(buf, sizeof(buf), "%s/Duke Nukem 3D.app/Contents/Resources/Duke Nukem 3D.boxer/C.harddisk", applications[i]);
        addsearchpath(buf);
    }

    for (i = 0; i < 2; i++)
    {
        Bsnprintf(buf, sizeof(buf), "%s/JFDuke3D", support[i]);
        addsearchpath(buf);
        Bsnprintf(buf, sizeof(buf), "%s/EDuke32", support[i]);
        addsearchpath(buf);
    }

    for (i = 0; i < 2; i++)
    {
        Bfree(applications[i]);
        Bfree(support[i]);
    }
#elif defined (_WIN32)
    char buf[BMAX_PATH];
    const char* instpath;

    if ((instpath = G_GetInstallPath(INSTPATH_STEAM_DUKE3D)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/gameroot", instpath);
        addsearchpath(buf);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/addons/dc", instpath);
        addsearchpath(buf);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/addons/nw", instpath);
        addsearchpath(buf);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/addons/vacation", instpath);
        addsearchpath(buf);
    }

    if ((instpath = G_GetInstallPath(INSTPATH_GOG_DUKE3D)))
        addsearchpath(instpath);

    if ((instpath = G_GetInstallPath(INSTPATH_3DR_DUKE3D)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/Duke Nukem 3D", instpath);
        addsearchpath(buf);
    }

    if ((instpath = G_GetInstallPath(INSTPATH_3DR_ANTH)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/Duke Nukem 3D", instpath);
        addsearchpath(buf);
    }

    if ((instpath = G_GetInstallPath(INSTPATH_STEAM_NAM)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/NAM", instpath);
        addsearchpath(buf);
    }
#endif
}

void G_CleanupSearchPaths(void)
{
#ifdef _WIN32
    char buf[BMAX_PATH];
    const char* instpath;

    if ((instpath = G_GetInstallPath(INSTPATH_STEAM_DUKE3D)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/gameroot", instpath);
        removesearchpath(buf);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/addons/dc", instpath);
        removesearchpath(buf);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/addons/nw", instpath);
        removesearchpath(buf);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/addons/vacation", instpath);
        removesearchpath(buf);
    }

    if ((instpath = G_GetInstallPath(INSTPATH_GOG_DUKE3D)))
        removesearchpath(instpath);

    if ((instpath = G_GetInstallPath(INSTPATH_3DR_DUKE3D)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/Duke Nukem 3D", instpath);
        removesearchpath(buf);
    }

    if ((instpath = G_GetInstallPath(INSTPATH_3DR_ANTH)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/Duke Nukem 3D", instpath);
        removesearchpath(buf);
    }

    if (g_gameType != GAMEFLAG_NAM && (instpath = G_GetInstallPath(INSTPATH_STEAM_NAM)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/NAM", instpath);
        removesearchpath(buf);
    }
#endif
}

//////////

struct strllist *CommandPaths, *CommandGrps;

char **g_scriptModules = NULL;
int32_t g_scriptModulesNum = 0;

void G_AddGroup(const char *buffer)
{
    char buf[BMAX_PATH];

    struct strllist *s = (struct strllist *)Xcalloc(1,sizeof(struct strllist));

    Bstrcpy(buf, buffer);

    if (Bstrchr(buf,'.') == 0)
        Bstrcat(buf,".grp");

    s->str = Xstrdup(buf);

    if (CommandGrps)
    {
        struct strllist *t;
        for (t = CommandGrps; t->next; t=t->next) ;
        t->next = s;
        return;
    }
    CommandGrps = s;
}

void G_AddPath(const char *buffer)
{
    struct strllist *s = (struct strllist *)Xcalloc(1,sizeof(struct strllist));
    s->str = Xstrdup(buffer);

    if (CommandPaths)
    {
        struct strllist *t;
        for (t = CommandPaths; t->next; t=t->next) ;
        t->next = s;
        return;
    }
    CommandPaths = s;
}

void G_AddCon(const char *buffer)
{
    clearScriptNamePtr();
    g_scriptNamePtr = dup_filename(buffer);
    initprintf("Using CON file \"%s\".\n",g_scriptNamePtr);
}

void G_AddConModule(const char *buffer)
{
    g_scriptModules = (char **) Xrealloc (g_scriptModules, (g_scriptModulesNum+1) * sizeof(char *));
    g_scriptModules[g_scriptModulesNum] = Xstrdup(buffer);
    ++g_scriptModulesNum;
}

//////////

// loads all group (grp, zip, pk3/4) files in the given directory
void G_LoadGroupsInDir(const char *dirname)
{
    static const char *extensions[4] = { "*.grp", "*.zip", "*.pk3", "*.pk4" };

    char buf[BMAX_PATH];
    int32_t i;

    fnlist_t fnlist = FNLIST_INITIALIZER;

    for (i=0; i<4; i++)
    {
        CACHE1D_FIND_REC *rec;

        fnlist_getnames(&fnlist, dirname, extensions[i], -1, 0);

        for (rec=fnlist.findfiles; rec; rec=rec->next)
        {
            Bsnprintf(buf, sizeof(buf), "%s/%s", dirname, rec->name);
            initprintf("Using group file \"%s\".\n", buf);
            initgroupfile(buf);
        }

        fnlist_clearnames(&fnlist);
    }
}

void G_DoAutoload(const char *dirname)
{
    char buf[BMAX_PATH];

    Bsnprintf(buf, sizeof(buf), "autoload/%s", dirname);
    G_LoadGroupsInDir(buf);
}

//////////

static uint8_t water_pal[768], slime_pal[768], title_pal[768], dre_alms[768], ending_pal[768];

uint8_t *basepaltable[BASEPALCOUNT] = {
    palette, water_pal, slime_pal,
    dre_alms, title_pal, ending_pal,
    NULL /*anim_pal*/
};

int32_t G_LoadLookups(void)
{
    int32_t fp, j;

    if ((fp=kopen4loadfrommod("lookup.dat",0)) == -1)
    {
        if ((fp=kopen4loadfrommod("lookup.dat",1)) == -1)
        {
            initprintf("ERROR: File \"lookup.dat\" not found.\n");
            return 1;
        }
    }

    j = loadlookups(fp);

    if (j < 0)
    {
        if (j == -1)
            initprintf("ERROR loading \"lookup.dat\": failed reading enough data.\n");
        return 1;
    }

    for (j=1; j<=5; j++)
    {
        // Account for TITLE and REALMS swap between basepal number and on-disk order.
        // XXX: this reordering is better off as an argument to us.
        int32_t basepalnum = (j == 3 || j == 4) ? 4+3-j : j;

        if (kread(fp, basepaltable[basepalnum], 768) != 768)
            return -1;
    }

    kclose(fp);

    generatefogpals();

    fillemptylookups();

    return 0;
}
