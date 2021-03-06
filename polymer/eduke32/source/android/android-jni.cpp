/*
 *
 * Designed by Emile Belanger
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>

#include "SDL_scancode.h"
#include "SDL_main.h"

#include "TouchControlsContainer.h"
#include "JNITouchControlsUtils.h"

#ifdef GP_LIC
#include "s-setup/s-setup.h"
#endif

extern "C"
{

#define DEFAULT_FADE_FRAMES 10

extern void SDL_Android_Init(JNIEnv* env, jclass cls);
//This is a new function I put into SDL2, file SDL_androidgl.c
extern void SDL_SetSwapBufferCallBack(void (*pt2Func)(void));

#include "in_android.h"
#include "../function.h"

#include "../GL/gl.h"
#include "../GL/nano_gl.h"

//Copied from build.h, which didnt include here
enum rendmode_t {
    REND_CLASSIC,
    REND_POLYMOST = 3,
    REND_POLYMER
};

#ifndef LOGI
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"DUKE", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "DUKE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR,"DUKE", __VA_ARGS__))
#endif

#define REND_SOFT  0
#define REND_GL    1

droidsysinfo_t droidinfo;

static int curRenderer;

float gameControlsAlpha = 0.5;

static bool invertLook = false;
static bool precisionShoot = false;
static bool showSticks = true;
static bool hideTouchControls = true;
char toggleCrouch = true;
static bool selectLastWeap = true;

static bool shooting = false;

static int weaponWheelVisible = false;


static int controlsCreated = 0;
touchcontrols::TouchControlsContainer controlsContainer;

touchcontrols::TouchControls *tcBlankTap=0;
touchcontrols::TouchControls *tcYesNo=0;
touchcontrols::TouchControls *tcMenuMain=0;
touchcontrols::TouchControls *tcGameMain=0;
touchcontrols::TouchControls *tcGameWeapons=0;
//touchcontrols::TouchControls *tcInventory=0;
touchcontrols::TouchControls *tcAutomap=0;


touchcontrols::TouchJoy *touchJoyLeft;
touchcontrols::WheelSelect *weaponWheel;

extern JNIEnv* env_;
JavaVM* jvm_;

void openGLStart()
{
    if (curRenderer == REND_GL)
    {
        glPushMatrix();

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrthof (0, droidinfo.screen_width, droidinfo.screen_height, 0, 0, 1);

        //glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
        //glClear(GL_COLOR_BUFFER_BIT);
        //LOGI("openGLStart");
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_FOG);
        glEnable(GL_TEXTURE_2D);
        glEnable (GL_BLEND);

        glColor4f(1,1,1,1);

        glDisableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY );

        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);

        nanoPushState();
    }
    else //software mode
    {
        glDisable(GL_ALPHA_TEST);
        glDisableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY );
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glEnable (GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
        glMatrixMode(GL_MODELVIEW);
    }
}

void openGLEnd()
{
    if (curRenderer == REND_GL)
    {
        //glPopMatrix();
        nanoPopState();
        glPopMatrix();
    }
    else// Software mode
    {
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glMatrixMode(GL_MODELVIEW);
    }
}

void gameSettingsButton(int state)
{
    if (state == 1)
    {
        showTouchSettings();
    }
}

//Because there is no Frame(), we need to check back to java each frame to see if the app hase paused

static jclass NativeLibClass = 0;
static jmethodID checkPauseMethod = 0;

void swapBuffers()
{
    if (NativeLibClass == 0)
    {
        NativeLibClass = env_->FindClass("com/beloko/duke/engine/NativeLib");
        checkPauseMethod = env_->GetStaticMethodID(NativeLibClass, "swapBuffers", "()V");
    }
    env_->CallStaticVoidMethod(NativeLibClass, checkPauseMethod);
}

//This is a bit of a hack, if the weapon wheel was selected, then an inv chosen instead, we need to cancel the weapon selection
//NOT needed actually, weapon wheel disabled before is get finger up anyway
//static bool invWasSelected = false;

void showWeaponsInventory(bool show)
{
    if (show)
    {
        for (int n=0; n<10; n++)
        {
            weaponWheel->setSegmentEnabled(n, (PortableRead(READ_WEAPONS) >> n) & 0x1);
        }

        //Show inventory buttons
        tcGameWeapons->setAllButtonsEnable(true);

        tcGameWeapons->fade(touchcontrols::FADE_IN, 5); //fade in
        weaponWheelVisible = true;
    }
    else //hide
    {
        tcGameWeapons->setAllButtonsEnable(false);
        weaponWheel->setTapMode(false);
        weaponWheelVisible = false;
    }
}

void gameButton(int state, int code)
{
    switch (code)
    {
    case gamefunc_Fire:
        shooting = state;
        PortableAction(state, code);
        break;

    case KEY_SHOW_KBRD:
        if (state)
            toggleKeyboard();

        PortableKeyEvent(state, SDL_SCANCODE_GRAVE, 0);
        break;

    case KEY_QUICK_CMD:
        if (state == 1)
            showCustomCommands();
        break;

    case KEY_SHOW_INVEN:
        if (state)
        {
            if (!weaponWheelVisible)
            {
                weaponWheel->setTapMode(true);
                showWeaponsInventory(true);
            }
            else
                showWeaponsInventory(false);
        }
        break;

    case KEY_QUICK_SAVE:
        LOGI("QUICK SAVE");
        PortableKeyEvent(state, SDL_SCANCODE_F6, 0);
        break;

    case KEY_QUICK_LOAD:
        LOGI("QUICK LOAD");
        PortableKeyEvent(state, SDL_SCANCODE_F9, 0);
        break;

    default:
        PortableAction(state, code);
        break;
    }
}

void automapButton(int state,int code)
{
    PortableAction(state,code);
}

void inventoryButton(int state,int code)
{
    PortableAction(state,code);
    if (state == 0)
    {
        //Inventory chosen, hide them and wheel
        showWeaponsInventory(false);
    }
}

void menuButton(int state,int code)
{
    PortableKeyEvent(state, code,code);
}

void blankTapButton(int state,int code)
{
    if (PortableRead(READ_IS_DEAD)) //if the player is dead we need to send a 'open' button
    {
        if (state)
        {
            PortableAction(1, gamefunc_Open);
            PortableAction(0, gamefunc_Open);
        }
    }
    else //everything else send a return key
    {
        PortableKeyEvent(state, SDL_SCANCODE_RETURN,0);
    }

}

void weaponWheelSelected(int enabled)
{
    if (enabled)
    {
        showWeaponsInventory(true);
    }
    else
    {
        showWeaponsInventory(false);
    }
}

void weaponWheelChosen(int segment)
{

    //LOGI("weaponWheel %d",segment);
    if (segment == -1) //Nothing was selected
    {
        if (selectLastWeap)
        {
            int32_t lw = PortableRead(READ_LASTWEAPON);

            if (lw != -1)
                PortableAction(2, gamefunc_Weapon_1 + lw);
        }
    }
    else
    {
        PortableAction(2, gamefunc_Weapon_1 + segment);
    }
}

void left_double_tap(int state)
{
    //LOGI("L double %d, droidinput.left_double_action = %d",state,droidinput.left_double_action);
    if (droidinput.left_double_action != -1)
        PortableAction(state, droidinput.left_double_action);
}

void right_double_tap(int state)
{
    //LOGTOUCH("R double %d",state);
    if (droidinput.right_double_action != -1)
        PortableAction(state, droidinput.right_double_action);
}

void mouseMove(int action, float x, float y, float dx, float dy)
{
    //LOGI(" mouse dx = %f, dy = %f",dx,dy);
    if (weaponWheelVisible)
        return;

    double scale = (shooting && precisionShoot) ? PRECISIONSHOOTFACTOR : 1.f;

    PortableLook(dx * droidinput.yaw_sens * scale,
            -dy * droidinput.pitch_sens * scale * (invertLook ? -1.f : 1.f));
}

void automap_multitouch_mouse_move(int action,float x, float y,float dx, float dy)
{

    if (action == MULTITOUCHMOUSE_MOVE)
    {
        PortableAutomapControl(0,dx,dy);
    }
    else if (action == MULTITOUCHMOUSE_ZOOM)
    {
        PortableAutomapControl(x,0,0);
    }
}

void left_stick(float joy_x, float joy_y, float mouse_x, float mouse_y)
{
    //LOGI("left_stick joy_x = %f, joy_y = %f",joy_x,joy_y);
    joy_x *=10;
    float strafe = joy_x*joy_x;
    //float strafe = joy_x;
    if (joy_x < 0)
        strafe *= -1;

    PortableMove(joy_y * 15 * droidinput.forward_sens, -strafe * droidinput.strafe_sens);
}

void right_stick(float joy_x, float joy_y, float mouse_x, float mouse_y)
{
    mouseMove(0, joy_x, joy_y, mouse_x, mouse_y);
}

void setHideSticks(bool v)
{
    if (touchJoyLeft) touchJoyLeft->setHideGraphics(v);
}

void touchSettingsButton(int state)
{
    /*
    int32_t paused = PortableRead(READ_PAUSED);

    //We wanna pause the game when doing settings
    if (state && !paused || !state && paused)
    {
        PortableKeyEvent(2, SDL_SCANCODE_PAUSE, 0);
    }
     */
}

void initControls(int width, int height, const char * graphics_path, const char *settings_file)
{
    touchcontrols::GLScaleWidth = (float)width;
    touchcontrols::GLScaleHeight = (float)height;

    LOGI("initControls %d x %d,x path = %s, settings = %s",width,height,graphics_path,settings_file);

    if (!controlsCreated)
    {
        LOGI("creating controls");

        touchcontrols::setGraphicsBasePath(graphics_path);

        controlsContainer.openGL_start.connect( sigc::ptr_fun(&openGLStart));
        controlsContainer.openGL_end.connect( sigc::ptr_fun(&openGLEnd));
        controlsContainer.signal_settings.connect( sigc::ptr_fun(&touchSettingsButton));

        tcBlankTap    = new touchcontrols::TouchControls("blank_tap", false, false);
        tcYesNo       = new touchcontrols::TouchControls("yes_no",    false, false);
        tcMenuMain    = new touchcontrols::TouchControls("menu",      false, false);
        tcGameMain    = new touchcontrols::TouchControls("game",      false,true,1,true);
        tcGameWeapons = new touchcontrols::TouchControls("weapons",   false,true,1,false);
        tcAutomap     = new touchcontrols::TouchControls("automap",   false,false);
        // tcInventory   = new touchcontrols::TouchControls("inventory", false,true,1,false);

        ///////////////////////// BLANK TAP SCREEN //////////////////////

        //One button on whole screen with no graphic, send a return key
        tcBlankTap->addControl(new touchcontrols::Button("whole_screen",  touchcontrols::RectF(0,0,26,16), std::string("test"),   SDL_SCANCODE_RETURN));
        tcBlankTap->signal_button.connect(  sigc::ptr_fun(&blankTapButton) ); //Just reuse the menuButton function



        ///////////////////////// YES NO SCREEN /////////////////////

        tcYesNo->addControl(new touchcontrols::Button("enter", touchcontrols::RectF(8,10,11,13),    "yes",        SDL_SCANCODE_RETURN));
        tcYesNo->addControl(new touchcontrols::Button("esc",   touchcontrols::RectF(14,10,17,13),    "no",          SDL_SCANCODE_ESCAPE));
        tcYesNo->signal_button.connect(  sigc::ptr_fun(&menuButton) ); //Just reuse the menuButton function



        ///////////////////////// MAIN MENU SCREEN /////////////////////

        //Menu
        /* 3x3
        tcMenuMain->addControl(new touchcontrols::Button("down_arrow",  touchcontrols::RectF(20,13,23,16),  "arrow_down",   SDL_SCANCODE_DOWN,  true)); //Repeating buttons
        tcMenuMain->addControl(new touchcontrols::Button("up_arrow",    touchcontrols::RectF(20,10,23,13),  "arrow_up",     SDL_SCANCODE_UP,    true));
        tcMenuMain->addControl(new touchcontrols::Button("left_arrow",  touchcontrols::RectF(17,13,20,16),  "arrow_left",   SDL_SCANCODE_LEFT,  true));
        tcMenuMain->addControl(new touchcontrols::Button("right_arrow", touchcontrols::RectF(23,13,26,16),  "arrow_right",  SDL_SCANCODE_RIGHT, true));
        tcMenuMain->addControl(new touchcontrols::Button("enter",       touchcontrols::RectF(0,13,3,16),    "enter",        SDL_SCANCODE_RETURN));
        tcMenuMain->addControl(new touchcontrols::Button("esc",         touchcontrols::RectF(0,10,3,13),    "esc",          SDL_SCANCODE_ESCAPE));
         */

        tcMenuMain->addControl(new touchcontrols::Button("down_arrow",  touchcontrols::RectF(22,14,24,16),  "arrow_down",   SDL_SCANCODE_DOWN,  true)); //Repeating buttons
        tcMenuMain->addControl(new touchcontrols::Button("up_arrow",    touchcontrols::RectF(22,12,24,14),  "arrow_up",     SDL_SCANCODE_UP,    true));
        tcMenuMain->addControl(new touchcontrols::Button("left_arrow",  touchcontrols::RectF(20,14,22,16),  "arrow_left",   SDL_SCANCODE_LEFT,  true));
        tcMenuMain->addControl(new touchcontrols::Button("right_arrow", touchcontrols::RectF(24,14,26,16),  "arrow_right",  SDL_SCANCODE_RIGHT, true));
        tcMenuMain->addControl(new touchcontrols::Button("enter",       touchcontrols::RectF(0,14,2,16),    "enter",        SDL_SCANCODE_RETURN));
        tcMenuMain->addControl(new touchcontrols::Button("esc",         touchcontrols::RectF(0,12,2,14),    "esc",          SDL_SCANCODE_ESCAPE));



        tcMenuMain->signal_button.connect(  sigc::ptr_fun(&menuButton) );
        tcMenuMain->setAlpha(1);



        //////////////////////////// GAME SCREEN /////////////////////
        tcGameMain->setAlpha(gameControlsAlpha);
        controlsContainer.editButtonAlpha = gameControlsAlpha;
        tcGameMain->addControl(new touchcontrols::Button("use",                           touchcontrols::RectF(23,3,26,6),    "use",      gamefunc_Open));
        tcGameMain->addControl(new touchcontrols::Button("attack",                        touchcontrols::RectF(20,7,23,10),   "fire2",    gamefunc_Fire));
        tcGameMain->addControl(new touchcontrols::Button("jump",                          touchcontrols::RectF(23,6,26,9),   "jump",     gamefunc_Jump));
        tcGameMain->addControl(new touchcontrols::Button("crouch",                        touchcontrols::RectF(24,12,26,14),  "crouch",   gamefunc_Crouch));
        tcGameMain->addControl(new touchcontrols::Button("kick","Mighty Foot",            touchcontrols::RectF(20,4,23,7),    "boot",     gamefunc_Quick_Kick,false,true));

        tcGameMain->addControl(new touchcontrols::Button("quick_save","Quick Save",       touchcontrols::RectF(22,0,24,2),    "save",     KEY_QUICK_SAVE,false,true));
        tcGameMain->addControl(new touchcontrols::Button("quick_load","Quick Load",       touchcontrols::RectF(20,0,22,2),    "load",     KEY_QUICK_LOAD,false,true));
        touchcontrols::Button *map_button = new touchcontrols::Button("map","Autotmap",                touchcontrols::RectF(6,0,8,2),      "map",      gamefunc_Map,       false,true);
        tcGameMain->addControl(map_button);
        tcGameMain->addControl(new touchcontrols::Button("keyboard","Show Console",       touchcontrols::RectF(8,0,10,2),     "keyboard", KEY_SHOW_KBRD,      false,true));

        tcGameMain->addControl(new touchcontrols::Button("show_inventory","Show Inventory",touchcontrols::RectF(24,0,26,2),    "inv",      KEY_SHOW_INVEN));

        tcGameMain->addControl(new touchcontrols::Button("next_weapon","Next Weapon",     touchcontrols::RectF(0,3,3,5),      "next_weap",gamefunc_Next_Weapon,false,true));
        tcGameMain->addControl(new touchcontrols::Button("prev_weapon","Previous Weapon", touchcontrols::RectF(0,5,3,7),      "prev_weap",gamefunc_Previous_Weapon,false,true));
        /*
            	//quick actions binds
            	tcGameMain->addControl(new touchcontrols::Button("quick_key_1",touchcontrols::RectF(4,3,6,5),"quick_key_1",KEY_QUICK_KEY1,false,true));
            	tcGameMain->addControl(new touchcontrols::Button("quick_key_2",touchcontrols::RectF(6,3,8,5),"quick_key_2",KEY_QUICK_KEY2,false,true));
            	tcGameMain->addControl(new touchcontrols::Button("quick_key_3",touchcontrols::RectF(8,3,10,5),"quick_key_3",KEY_QUICK_KEY3,false,true));
            	tcGameMain->addControl(new touchcontrols::Button("quick_key_4",touchcontrols::RectF(10,3,12,5),"quick_key_4",KEY_QUICK_KEY4,false,true));
         */
        //Left stick
        touchJoyLeft = new touchcontrols::TouchJoy("stick",touchcontrols::RectF(0,7,8,16),"strafe_arrow");
        tcGameMain->addControl(touchJoyLeft);
        touchJoyLeft->signal_move.connect(sigc::ptr_fun(&left_stick) );
        touchJoyLeft->signal_double_tap.connect(sigc::ptr_fun(&left_double_tap) );

        //Right stick (not used)
        //touchJoyRight = new touchcontrols::TouchJoy("touch",touchcontrols::RectF(17,7,26,16),"look_arrow");
        //tcGameMain->addControl(touchJoyRight);
        //touchJoyRight->signal_move.connect(sigc::ptr_fun(&right_stick) );
        //touchJoyRight->signal_double_tap.connect(sigc::ptr_fun(&right_double_tap) );
        //touchJoyRight->setEnabled(false);


        //Mouse look for whole screen
        touchcontrols::Mouse *mouse = new touchcontrols::Mouse("mouse",touchcontrols::RectF(3,0,26,16),"");
        mouse->signal_action.connect(sigc::ptr_fun(&mouseMove));
        mouse->signal_double_tap.connect(sigc::ptr_fun(&right_double_tap) );

        mouse->setHideGraphics(true);
        tcGameMain->addControl(mouse);

        tcGameMain->signal_button.connect(  sigc::ptr_fun(&gameButton) );
        tcGameMain->signal_settingsButton.connect(  sigc::ptr_fun(&gameSettingsButton) );

        ///////////////////////// AUTO MAP SCREEN ///////////////////////


        //Automap
        touchcontrols::MultitouchMouse *multimouse = new touchcontrols::MultitouchMouse("gamemouse",touchcontrols::RectF(0,0,26,16),"");
        multimouse->setHideGraphics(true);
        tcAutomap->addControl(multimouse);
        multimouse->signal_action.connect(sigc::ptr_fun(&automap_multitouch_mouse_move) );
        tcAutomap->addControl(map_button);
        tcAutomap->signal_button.connect(  sigc::ptr_fun(&gameButton) );
        tcAutomap->setAlpha(0.5);





        //Now inventory in the weapons control group!
        tcGameWeapons->addControl(new touchcontrols::Button("jetpack", touchcontrols::RectF(0,3,2,5),"jetpack",gamefunc_Jetpack,false,false,true));
        tcGameWeapons->addControl(new touchcontrols::Button("medkit",  touchcontrols::RectF(0,5,2,7),"medkit",gamefunc_MedKit,false,false,true));
        tcGameWeapons->addControl(new touchcontrols::Button("nightv",  touchcontrols::RectF(0,7,2,9),"nightvision",gamefunc_NightVision,false,false,true));
        tcGameWeapons->addControl(new touchcontrols::Button("holoduke",touchcontrols::RectF(0,9,2,11),"holoduke",gamefunc_Holo_Duke,false,false,true));
        tcGameWeapons->addControl(new touchcontrols::Button("steroids",touchcontrols::RectF(0,11,2,13),"steroids",gamefunc_Steroids,false,false,true));
        //Inventory are the only buttons so safe to do this
        tcGameWeapons->signal_button.connect(  sigc::ptr_fun(&inventoryButton) );


        //Weapons
        weaponWheel = new touchcontrols::WheelSelect("weapon_wheel",touchcontrols::RectF(7,2,19,14),"weapon_wheel_orange_blank",10);
        weaponWheel->signal_selected.connect(sigc::ptr_fun(&weaponWheelChosen) );
        weaponWheel->signal_enabled.connect(sigc::ptr_fun(&weaponWheelSelected));
        tcGameWeapons->addControl(weaponWheel);
        tcGameWeapons->setAlpha(0.9);

        /*
        tcInventory->addControl(new touchcontrols::Button("jetpack", touchcontrols::RectF(0,3,2,5),"jetpack",gamefunc_Jetpack));
        tcInventory->addControl(new touchcontrols::Button("medkit",  touchcontrols::RectF(0,5,2,7),"medkit",gamefunc_MedKit));
        tcInventory->addControl(new touchcontrols::Button("nightv",  touchcontrols::RectF(0,7,2,9),"nightvision",gamefunc_NightVision));
        tcInventory->addControl(new touchcontrols::Button("holoduke",touchcontrols::RectF(0,9,2,11),"holoduke",gamefunc_Holo_Duke));
        tcInventory->addControl(new touchcontrols::Button("steroids",touchcontrols::RectF(0,11,2,13),"steroids",gamefunc_Steroids));
        tcInventory->setAlpha(1);

        tcInventory->signal_button.connect(  sigc::ptr_fun(&inventoryButton));
         */


        /////////////////////////////////////////////////////////////


        controlsContainer.addControlGroup(tcMenuMain);
        controlsContainer.addControlGroup(tcGameMain);
        //        controlsContainer.addControlGroup(tcInventory);//Need to be above tcGameMain incase buttons over stick
        controlsContainer.addControlGroup(tcGameWeapons);
        controlsContainer.addControlGroup(tcAutomap);
        controlsContainer.addControlGroup(tcYesNo);
        controlsContainer.addControlGroup(tcBlankTap);
        controlsCreated = 1;

        tcGameMain->setAlpha(gameControlsAlpha);
        controlsContainer.editButtonAlpha = gameControlsAlpha;
        tcGameWeapons->setAlpha(gameControlsAlpha);
        tcMenuMain->setAlpha(gameControlsAlpha);


        tcGameMain->setXMLFile((std::string)graphics_path +  "/game.xml");
        tcGameWeapons->setXMLFile((std::string)graphics_path +  "/weapons.xml");
        tcAutomap->setXMLFile((std::string)graphics_path +  "/automap.xml");
        // tcInventory->setXMLFile((std::string)graphics_path +  "/inventory.xml");

        setControlsContainer(&controlsContainer);
    }
    else
        LOGI("NOT creating controls");

    //controlsContainer.initGL();
}

void updateTouchScreenMode(touchscreemode_t mode)
{
    // LOGI("updateTouchScreenModeA %d",mode);

    static  touchscreemode_t lastMode = TOUCH_SCREEN_BLANK;


    if (mode != lastMode){

        //first disable the last screen and fade out is necessary
        switch(lastMode){
        case TOUCH_SCREEN_BLANK:
            //Does not exist yet
            break;
        case TOUCH_SCREEN_BLANK_TAP:
            tcBlankTap->resetOutput();
            tcBlankTap->setEnabled(false); //Dont fade out as no graphics
            break;
        case TOUCH_SCREEN_YES_NO:
            tcYesNo->resetOutput();
            tcYesNo->fade(touchcontrols::FADE_OUT,DEFAULT_FADE_FRAMES);
            break;
        case TOUCH_SCREEN_MENU:
            tcMenuMain->resetOutput();
            tcMenuMain->fade(touchcontrols::FADE_OUT,DEFAULT_FADE_FRAMES);
            break;
        case TOUCH_SCREEN_GAME:
            tcGameMain->resetOutput();

            tcGameMain->fade(touchcontrols::FADE_OUT,DEFAULT_FADE_FRAMES);
            tcGameWeapons->setEnabled(false);

            break;
        case TOUCH_SCREEN_AUTOMAP:
            tcAutomap->resetOutput();
            tcAutomap->fade(touchcontrols::FADE_OUT,DEFAULT_FADE_FRAMES);
            break;
        case TOUCH_SCREEN_CONSOLE:
            break;
        }

        //Enable the current new screen
        switch(mode){
        case TOUCH_SCREEN_BLANK:
            //Does not exist yet
            break;
        case TOUCH_SCREEN_BLANK_TAP:
            tcBlankTap->setEnabled(true);
            break;
        case TOUCH_SCREEN_YES_NO:
            tcYesNo->setEnabled(true);
            tcYesNo->fade(touchcontrols::FADE_IN,DEFAULT_FADE_FRAMES);
            break;
        case TOUCH_SCREEN_MENU:
            tcMenuMain->setEnabled(true);
            tcMenuMain->fade(touchcontrols::FADE_IN,DEFAULT_FADE_FRAMES);

            //This is a bit of a hack, we need to enable the inventory buttons so they can be edited, they will not be seen anyway
            showWeaponsInventory(true);
            break;
        case TOUCH_SCREEN_GAME:
            tcGameMain->setEnabled(true);
            tcGameMain->fade(touchcontrols::FADE_IN,DEFAULT_FADE_FRAMES);
            tcGameWeapons->setEnabled(true);
            showWeaponsInventory(false);
            break;
        case TOUCH_SCREEN_AUTOMAP:
            tcAutomap->setEnabled(true);
            tcAutomap->fade(touchcontrols::FADE_IN,DEFAULT_FADE_FRAMES);

            break;
        case TOUCH_SCREEN_CONSOLE:
            break;
        }

        lastMode = mode;
    }

}


#ifdef GP_LIC
#define GP_LIC_INC 1
#include "s-setup/gp_lic_include.h"
#endif

void frameControls()
{
    static int loadedGLImages = 0;

    LOGI("frameControls");


    //We need to do this here now because duke loads a new gl context
    if (!loadedGLImages)
    {
        controlsContainer.initGL();
        loadedGLImages = 1;
    }

    //LOGI("frameControls");
    curRenderer = (PortableRead(READ_RENDERER) != REND_CLASSIC);

    updateTouchScreenMode((touchscreemode_t)PortableRead(READ_SCREEN_MODE));


    setHideSticks(!showSticks);
    controlsContainer.draw();

#ifdef GP_LIC
#undef GP_LIC_INC
#define GP_LIC_INC 2
#include "s-setup/gp_lic_include.h"
#endif

}

void setTouchSettings(float alpha,float strafe,float fwd,float pitch,float yaw,int other)
{

    gameControlsAlpha = MINCONTROLALPHA + (alpha * (1.0f - MINCONTROLALPHA));

    if (tcGameMain)
    {
        tcGameMain->setAlpha(gameControlsAlpha);
        controlsContainer.editButtonAlpha = gameControlsAlpha;
        tcGameWeapons->setAlpha(gameControlsAlpha);
        tcMenuMain->setAlpha(gameControlsAlpha);
        // tcInventory->setAlpha(gameControlsAlpha);
    }

    // TODO: defined names for these values
    selectLastWeap  = other & 0x1 ? true : false;
    toggleCrouch	= other & 0x2 ? true : false;
    invertLook      = other & 0x4 ? true : false;
    precisionShoot  = other & 0x8 ? true : false;
    showSticks      = other & 0x1000 ? true : false;

    hideTouchControls = other & 0x80000000 ? true : false;

    int doubletap_options[6] = {0,gamefunc_Fire,gamefunc_Jump,gamefunc_Quick_Kick,gamefunc_MedKit,gamefunc_Jetpack};

    droidinput.left_double_action = doubletap_options[((other>>4) & 0xF)];
    droidinput.right_double_action = doubletap_options[((other>>8) & 0xF)];


    droidinput.strafe_sens = strafe;
    droidinput.forward_sens = fwd;
    droidinput.pitch_sens = pitch;
    droidinput.yaw_sens = yaw;

    LOGI("setTouchSettings alpha = %f, left_double_action = %d",alpha,droidinput.left_double_action);
}

#define EXPORT_ME __NDK_FPABI__ __attribute__ ((visibility("default")))

JNIEnv* env_;

int argc=1;
const char * argv[32];
std::string graphicpath;
std::string doom_path;

static const char * getGamePath()
{
    return doom_path.c_str();
}


jint EXPORT_ME
Java_com_beloko_duke_engine_NativeLib_init( JNIEnv* env,
        jobject thiz,jstring graphics_dir,jint audio_rate,jint audio_buffer_size,jobjectArray argsArray,jint renderer,jstring doom_path_ )
{
    env_ = env;

#ifdef GP_LIC
    getGlobalClasses(env_);
#endif

    droidinfo.audio_sample_rate = audio_rate;
    droidinfo.audio_buffer_size = audio_buffer_size;

    curRenderer = renderer;
    //curRenderer = REND_SOFT;
    curRenderer = REND_GL;

    argv[0] = "eduke32";
    int argCount = (env)->GetArrayLength( argsArray);
    LOGI("argCount = %d",argCount);
    for (int i=0; i<argCount; i++) {
        jstring string = (jstring) (env)->GetObjectArrayElement( argsArray, i);
        argv[argc] = (char *)(env)->GetStringUTFChars( string, 0);
        LOGI("arg = %s",argv[argc]);
        argc++;
    }

    doom_path = (char *)(env)->GetStringUTFChars( doom_path_, 0);

    //Change working dir, save games etc
    // FIXME: potentially conflicts with chdirs in -game_dir support
    chdir(getGamePath());
    char timidity_env[512];

    sprintf(timidity_env,"TIMIDITY_CFG=%s/../timidity.cfg",getGamePath());
    //putenv("TIMIDITY_CFG=../timidity.cfg");
    putenv(timidity_env);

    LOGI("doom_path = %s",getGamePath());

    const char * p = env->GetStringUTFChars(graphics_dir,NULL);
    graphicpath =  std::string(p);

    initControls(droidinfo.screen_width, -droidinfo.screen_height,
            graphicpath.c_str(),(graphicpath + "/touch_controls.xml").c_str());

    /*
    if (renderer != REND_SOFT)
        SDL_SetSwapBufferCallBack(frameControls);

    if (renderer == REND_SOFT)// In soft mode SDL calls swap buffer, disable so it does not flicker
        SDL_SwapBufferPerformsSwap(false);
     */


    SDL_SetSwapBufferCallBack(frameControls);

    //Now doen in java to keep context etc
    //SDL_SwapBufferPerformsSwap(false);

    PortableInit(argc, argv);

    return 0;
}


jint EXPORT_ME
Java_com_beloko_duke_engine_NativeLib_frame( JNIEnv* env,
        jobject thiz )
{
    LOGI("Java_com_beloko_duke_engine_NativeLib_frame");

    frameControls();
    return 0;
}

__attribute__((visibility("default"))) jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    LOGI("JNI_OnLoad");
    setTCJNIEnv(vm);
    jvm_ = vm;
    return JNI_VERSION_1_4;
}


void EXPORT_ME
Java_com_beloko_duke_engine_NativeLib_keypress(JNIEnv *env, jobject obj,
        jint down, jint keycode, jint unicode)
{
    LOGI("keypress %d",keycode);
    if (controlsContainer.isEditing())
    {
        if (down && (keycode == SDL_SCANCODE_ESCAPE ))
            controlsContainer.finishEditing();
        return;
    }

    PortableKeyEvent(down,keycode,unicode);
}


void EXPORT_ME
Java_com_beloko_duke_engine_NativeLib_touchEvent(JNIEnv *env, jobject obj,
        jint action, jint pid, jdouble x, jdouble y)
{
    //LOGI("TOUCHED");
    controlsContainer.processPointer(action,pid,x,y);
}


void EXPORT_ME Java_com_beloko_duke_engine_NativeLib_doAction(JNIEnv *env, jobject obj,
        jint state, jint action)
{
    LOGI("doAction %d %d",state,action);

    //gamepadButtonPressed();
    if (hideTouchControls && tcGameMain)
    {
        if (tcGameMain->isEnabled())
            tcGameMain->animateOut(30);

        if (tcGameWeapons->isEnabled())
            tcGameWeapons->animateOut(30);
    }

    PortableAction(state,action);
}

void EXPORT_ME Java_com_beloko_duke_engine_NativeLib_analogFwd(JNIEnv *env, jobject obj,
        jfloat v)
{
    PortableMove(v, NAN);
}

void EXPORT_ME Java_com_beloko_duke_engine_NativeLib_analogSide(JNIEnv *env, jobject obj,
        jfloat v)
{
    PortableMove(NAN, v);
}

void EXPORT_ME Java_com_beloko_duke_engine_NativeLib_analogPitch(JNIEnv *env, jobject obj,
        jint mode,jfloat v)
{
    PortableLookJoystick(NAN, v);
}

void EXPORT_ME Java_com_beloko_duke_engine_NativeLib_analogYaw(JNIEnv *env, jobject obj,
        jint mode,jfloat v)
{
    PortableLookJoystick(v, NAN);
}

void EXPORT_ME Java_com_beloko_duke_engine_NativeLib_setTouchSettings(JNIEnv *env, jobject obj,
        jfloat alpha,jfloat strafe,jfloat fwd,jfloat pitch,jfloat yaw,int other)
{
    setTouchSettings(alpha,strafe,fwd,pitch,yaw,other);
}

void EXPORT_ME Java_com_beloko_duke_engine_NativeLib_resetTouchSettings(JNIEnv *env, jobject obj)
{
    controlsContainer.resetDefaults();
}

std::string quickCommandString;
jint EXPORT_ME
Java_com_beloko_duke_engine_NativeLib_quickCommand(JNIEnv *env, jobject obj,
        jstring command)
{
    const char * p = env->GetStringUTFChars(command,NULL);
    quickCommandString =  std::string(p) + "\n";
    env->ReleaseStringUTFChars(command, p);
    PortableCommand(quickCommandString.c_str());
}

void EXPORT_ME
Java_com_beloko_duke_engine_NativeLib_setScreenSize( JNIEnv* env,
        jobject thiz, jint width, jint height)
{
    droidinfo.screen_width = width;
    droidinfo.screen_height = height;
}

void EXPORT_ME  Java_org_libsdl_app_SDLActivity_nativeInit(JNIEnv* env, jclass cls)
{
    /* This interface could expand with ABI negotiation, calbacks, etc. */
    SDL_Android_Init(env, cls);
    SDL_SetMainReady();
    // SDL_EventState(SDL_TEXTINPUT,SDL_ENABLE);
}

#ifdef GP_LIC
#undef GP_LIC_INC
#define GP_LIC_INC 3
#include "s-setup/gp_lic_include.h"
#endif
}
