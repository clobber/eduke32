#include "sdl_inc.h"
#include "baselayer.h"
#include "keys.h"
#include "duke3d.h"
#include "common_game.h"
#include "osd.h"
#include "player.h"
#include "game.h"
#include "build.h"

#include "jmact/keyboard.h"
#include "jmact/control.h"

#include "../src/video/android/SDL_androidkeyboard.h" // FIXME: include header locally if necessary

#include "in_android.h"

#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"DUKE", __VA_ARGS__))

extern int32_t main(int32_t argc, char *argv []);

extern int SDL_SendKeyboardKey(Uint8 state, SDL_Scancode scancode);
extern int SDL_SendKeyboardText(const char *text);

static char sdl_text[2];

static droidinput_t droidinput;

int PortableKeyEvent(int state, int code,int unicode)
{
    LOGI("PortableKeyEvent %d %d %d",state,code,unicode);

    SDL_SendKeyboardKey(state ? SDL_PRESSED : SDL_RELEASED, code);
    SDL_EventState(SDL_TEXTINPUT, SDL_ENABLE);

    if (code == 42)
        unicode = 42;

    if (state)
    {
        //if (unicode < 128)
        {
            sdl_text[0] = unicode;
            sdl_text[1] = 0;

            int posted = SDL_SendKeyboardText((const char*)sdl_text);
            LOGI("posted = %d",posted);
        }

        if (state == 2)
            PortableKeyEvent(0, code, unicode);
    }

    return 0;

}

void changeActionState(int state, int action)
{
    if (state)
    {
        //BUTTONSET(action,1);
        droidinput.functionSticky  |= ((uint64_t)1<<((uint64_t)(action)));
        droidinput.functionHeld    |= ((uint64_t)1<<((uint64_t)(action)));

        return;
    }

    //BUTTONCLEAR(action);
    droidinput.functionHeld  &= ~((uint64_t) 1<<((uint64_t) (action)));
}

void PortableAction(int state, int action)
{
    LOGI("PortableAction action = %d, state = %d", action, state);

    //Action is for the menu
    if (action >= MENU_UP)
    {
        if (PortableRead(READ_MENU))
        {
            int sdl_code = 0;
            switch (action)
            {
            case MENU_UP:
                sdl_code = SDL_SCANCODE_UP;
                break;
            case MENU_DOWN:
                sdl_code = SDL_SCANCODE_DOWN;
                break;
            case MENU_LEFT:
                sdl_code = SDL_SCANCODE_LEFT;
                break;
            case MENU_RIGHT:
                sdl_code = SDL_SCANCODE_RIGHT;
                break;
            case MENU_SELECT:
                sdl_code = SDL_SCANCODE_RETURN;
                break;
            case MENU_BACK:
                sdl_code = SDL_SCANCODE_ESCAPE;
                break;
            }

            PortableKeyEvent(state, sdl_code, 0);

            return;
        }
    }
    else
    {
        if (PortableRead(READ_MENU)) //If in the menu, dont do any game actions
            return;

        //Special toggle for crouch, NOT when using jetpack or in water
        if (!g_player[myconnectindex].ps->jetpack_on &&
                g_player[myconnectindex].ps->on_ground &&
                (sector[g_player[myconnectindex].ps->cursectnum].lotag != ST_2_UNDERWATER))
        {
            if (toggleCrouch)
            {
                if (action == gamefunc_Crouch)
                {
                    if (state)
                        droidinput.crouchToggleState = !droidinput.crouchToggleState;

                    state = droidinput.crouchToggleState;
                }
            }
        }

        //Check if jumping while crouched
        if (action == gamefunc_Jump)
        {
            crouchToggleState = 0;
            changeActionState(0, gamefunc_Crouch);
        }

        changeActionState(state, action);

        LOGI("PortableAction state = 0x%016llX", CONTROL_ButtonState);
    }
}
// =================== FORWARD and SIDE MOVMENT ==============

void PortableMove(float fwd, float strafe)
{
    droidinput.forwardmove = fclamp2(fwd    + droidinput.forwardmove, -1.f, 1.f);
    droidinput.sidemove    = fclamp2(strafe + droidinput.sidemove,    -1.f, 1.f);
}

//======================================================================

void PortableLook(double yaw, double pitch)
{
    //LOGI("PortableLookPitch %d %f",mode, pitch);
    droidinput.pitch += pitch;
    droidinput.yaw   += yaw;
}

void PortableCommand(const char * cmd)
{
    OSD_Dispatch(cmd);
}

void PortableInit(int argc,const char ** argv)
{
    main(argc, argv);
}

int32_t PortableRead(portableread_t r)
{
    switch (r)
    {
    case READ_MENU:
        return (g_player[myconnectindex].ps->gm & MODE_MENU) == MODE_MENU || (g_player[myconnectindex].ps->gm & MODE_GAME) != MODE_GAME;
    case READ_WEAPONS:
        return g_player[myconnectindex].ps->gotweapon;
    case READ_AUTOMAP:
        return ud.overhead_on != 0;
    case READ_KEYBOARD:
        return 0;
    case READ_RENDERER:
        return getrendermode();
    case READ_LASTWEAPON:
        return droidinput.lastWeapon;
    case READ_PAUSED:
        return ud.pause_on != 0;
    default:
        return 0;
    }
}

///This stuff is called from the game/engine

void CONTROL_Android_SetLastWeapon(int w)
{
    LOGI("setLastWeapon %d",w);
    droidinput.lastWeapon = w;
}

void CONTROL_Android_ClearButton(int32_t whichbutton)
{
    BUTTONCLEAR(whichbutton);
    droidinput.functionHeld  &= ~((uint64_t)1<<((uint64_t)(whichbutton)));
}

void  CONTROL_Android_PollDevices(ControlInfo *info)
{
    //LOGI("CONTROL_Android_PollDevices %f %f",forwardmove,sidemove);

    info->dz     = -droidinput.forwardmove * 5000;
    info->dx     = droidinput.sidemove     * 200;
    info->dpitch = droidinput.pitch        * 100000;
    info->dyaw   = -droidinput.yaw         * 80000;

    droidinput.forwardmove = droidinput.sidemove = 0.f;
    droidinput.pitch = droidinput.yaw = 0.f;

    CONTROL_ButtonState = 0;
    CONTROL_ButtonState |= droidinput.functionSticky;
    CONTROL_ButtonState |= droidinput.functionHeld;

    droidinput.functionSticky = 0;

    //LOGI("poll state = 0x%016llX",CONTROL_ButtonState);
}


