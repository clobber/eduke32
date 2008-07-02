// On-screen display (ie. console)
// for the Build Engine
// by Jonathon Fowler (jonof@edgenetwk.com)

#ifndef __osd_h__
#define __osd_h__


typedef struct {
	int numparms;
	const char *name;
	const char **parms;
	const char *raw;
} osdfuncparm_t;

typedef struct _symbol
{
    const char *name;
    struct _symbol *next;

    const char *help;
    int (*func)(const osdfuncparm_t *);
} symbol_t;

symbol_t *symbols;
const char *stripcolorcodes(const char *t);

#define OSD_ALIAS 1337
#define OSD_UNALIASED 1338

#define OSDCMD_OK	0
#define OSDCMD_SHOWHELP 1

extern int osdexecscript;
extern int osdkey;

int OSD_Exec(const char *szScript);

// initializes things
void OSD_Init(void);

// sets the file to echo output to
void OSD_SetLogFile(char *fn);

// sets the functions the OSD will call to interrogate the environment
void OSD_SetFunctions(
		void (*drawchar)(int,int,char,int,int),
		void (*drawstr)(int,int,char*,int,int,int),
		void (*drawcursor)(int,int,int,int),
		int (*colwidth)(int),
		int (*rowheight)(int),
		void (*clearbg)(int,int),
		int (*gettime)(void),
		void (*onshow)(int)
	);

// sets the parameters for presenting the text
void OSD_SetParameters(
		int promptshade, int promptpal,
		int editshade, int editpal,
		int textshade, int textpal
	);

// sets the scancode for the key which activates the onscreen display
void OSD_CaptureKey(int sc);

// handles keyboard input when capturing input. returns 0 if key was handled
// or the scancode if it should be handled by the game.
int  OSD_HandleScanCode(int sc, int press);
int  OSD_HandleChars(void);

// handles the readjustment when screen resolution changes
void OSD_ResizeDisplay(int w,int h);

// captures and frees osd input
void OSD_CaptureInput(int cap);

// sets the console version string
void OSD_SetVersionString(const char *version, int shade, int pal);

// shows or hides the onscreen display
void OSD_ShowDisplay(int onf);

// draw the osd to the screen
void OSD_Draw(void);

// just like printf
void OSD_Printf(const char *fmt, ...);
#define printOSD OSD_Printf

// executes buffered commands
void OSD_DispatchQueued(void);

// executes a string
int OSD_Dispatch(const char *cmd);

// registers a function
//   name = name of the function
//   help = a short help string
//   func = the entry point to the function
int OSD_RegisterFunction(const char *name, const char *help, int (*func)(const osdfuncparm_t*));

#endif // __osd_h__

