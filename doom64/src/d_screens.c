#include "i_main.h"
#include "doomdef.h"
#include "r_local.h"
#include "st_main.h"

extern int DefaultConfiguration[5][13];

#define COLOR_BLACK (GPACK_RGBA5551(0, 0, 0, 0) << 16 | GPACK_RGBA5551(0, 0, 0, 0))
#define COLOR_WHITE 0xffffffff
#define COLOR_WARNING 0xc00000ff

int D_RunDemo(char *name, skill_t skill, int map)
{
    int lump;
    int exit;

    demo_p = Z_Alloc(16000, PU_STATIC, NULL);

    lump = W_GetNumForName(name);
    W_ReadLump(lump, demo_p, dec_d64);

    exit = G_PlayDemoPtr(skill, map);

    Z_Free(demo_p);

    return exit;
}

int D_TitleMap(void)
{
    int exit;

    D_OpenControllerPak();

    demo_p = Z_Alloc(16000, PU_STATIC, NULL);

    D_memset(demo_p, 0, 16000);
    D_memcpy(demo_p, DefaultConfiguration[0], 13 * sizeof(int));

    // [Immorpher] Randomize the intro fun a bit!
    exit = G_PlayDemoPtr(2 * (I_Random() % 3), 33);

    Z_Free(demo_p);

    return exit;
}

int D_WarningTicker(void)
{
    if ((gamevbls < gametic) && !(gametic & 7))
    {
        MenuAnimationTic = (MenuAnimationTic + 1) & 7;
    }

    return 0;
}

void D_DrawWarning(void)
{
    I_ClearFrame();

    gDPPipeSync(GFX1++);
    gDPSetCycleType(GFX1++, G_CYC_FILL);
    gDPSetRenderMode(GFX1++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WD, OS_K0_TO_PHYSICAL(cfb[vid_side]));
    gDPSetFillColor(GFX1++, COLOR_BLACK);
    gDPFillRectangle(GFX1++, 0, 0, SCREEN_WD - 1, SCREEN_HT - 1);

    if (MenuAnimationTic & 1)
        ST_DrawString(-1, 30, "WARNING!", COLOR_WARNING);

    ST_DrawString(-1, 60, "nintendo 64 controller", COLOR_WHITE);
    ST_DrawString(-1, 80, "is not connected.", COLOR_WHITE);
    ST_DrawString(-1, 120, "please turn off your", COLOR_WHITE);
    ST_DrawString(-1, 140, "nintendo 64 system.", COLOR_WHITE);
    ST_DrawString(-1, 180, "plug in your nintendo 64", COLOR_WHITE);
    ST_DrawString(-1, 200, "controller and turn it on.", COLOR_WHITE);

    I_DrawFrame();
}

int D_LegalTicker(void)
{
    if ((ticon - last_ticon) >= (5 * TICRATE))
    {
        text_alpha -= 8;
        if (text_alpha < 0)
        {
            text_alpha = 0;
            return 8;
        }
    }
    return 0;
}

void D_DrawLegal(void)
{
    I_ClearFrame();

    gDPPipeSync(GFX1++);
    gDPSetCycleType(GFX1++, G_CYC_FILL);
    gDPSetRenderMode(GFX1++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WD, OS_K0_TO_PHYSICAL(cfb[vid_side]));
    gDPSetFillColor(GFX1++, COLOR_BLACK);
    gDPFillRectangle(GFX1++, 0, 0, SCREEN_WD - 1, SCREEN_HT - 1);

    M_DrawBackground(27, 74, text_alpha, "USLEGAL");

    if (FilesUsed > -1)
    {
        ST_DrawString(-1, 200, "hold \x8d to manage pak", text_alpha | (COLOR_WHITE & ~0xff));
    }

    I_DrawFrame();
}

int D_NoPakTicker(void)
{
    if ((ticon - last_ticon) >= (8 * TICRATE))
        return 8;

    return 0;
}

void D_DrawNoPak(void)
{
    I_ClearFrame();

    gDPPipeSync(GFX1++);
    gDPSetCycleType(GFX1++, G_CYC_FILL);
    gDPSetRenderMode(GFX1++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WD, OS_K0_TO_PHYSICAL(cfb[vid_side]));
    gDPSetFillColor(GFX1++, COLOR_BLACK);
    gDPFillRectangle(GFX1++, 0, 0, SCREEN_WD - 1, SCREEN_HT - 1);

    ST_DrawString(-1, 40, "no controller pak.", COLOR_WHITE);
    ST_DrawString(-1, 60, "your game cannot", COLOR_WHITE);
    ST_DrawString(-1, 80, "be saved.", COLOR_WHITE);
    ST_DrawString(-1, 120, "please turn off your", COLOR_WHITE);
    ST_DrawString(-1, 140, "nintendo 64 system", COLOR_WHITE);
    ST_DrawString(-1, 160, "before inserting a", COLOR_WHITE);
    ST_DrawString(-1, 180, "controller pak.", COLOR_WHITE);

    I_DrawFrame();
}

void D_SplashScreen(void)
{
    // Check if the n64 control is connected
    // if not connected, it will show the Warning screen
    if ((gamepad_bit_pattern & 1) == 0)
    {
        MiniLoop(NULL, NULL, D_WarningTicker, D_DrawWarning);
    }

    // Check if the n64 controller Pak is connected
    I_CheckControllerPak();

    // if not connected, it will show the NoPak screen
    if (FilesUsed < 0)
    {
        last_ticon = 0;
        MiniLoop(NULL, NULL, D_NoPakTicker, D_DrawNoPak);
    }

    // show the legals screen
    text_alpha = 0xff;
    last_ticon = 0;
    MiniLoop(NULL, NULL, D_LegalTicker, D_DrawLegal);
}

static int cred_step;
static int cred1_alpha; 
static int cred2_alpha; 
static int cred_next;   

int D_Credits(void)
{
    int exit;

    cred_next = 0;
    cred1_alpha = 0;
    cred2_alpha = 0;
    cred_step = 0;
    exit = MiniLoop(NULL, NULL, D_CreditTicker, D_CreditDrawer);

    return exit;
}

int D_CreditTicker(void)
{
    if (((u32)ticbuttons[0] >> 16) != 0)
{        return ga_exit;
}

    if ((cred_next == 0) || (cred_next == 1))
    {
        if (cred_step == 0)
        {
            cred1_alpha += 8;
            if (cred1_alpha >= 255)
            {
                cred1_alpha = 0xff;
                cred_step = 1;
            }
        }
        else if (cred_step == 1)
        {
            cred2_alpha += 8;
            if (cred2_alpha >= 255)
            {
                cred2_alpha = 0xff;
                last_ticon = ticon;
                cred_step = 2;
            }
        }
        else if (cred_step == 2)
        {
            if ((ticon - last_ticon) >= (6 * TICRATE))
{                cred_step = 3;
}        }
        else
        {
            cred1_alpha -= 8;
            cred2_alpha -= 8;
            if (cred1_alpha < 0)
            {
                cred_next += 1;
                cred1_alpha = 0;
                cred2_alpha = 0;
                cred_step = 0;
            }
        }
    }
    else if (cred_next == 2)
{        return ga_exitdemo;
}
    return ga_nothing;
}

// Background Color (Dark Blue)
#define COLOR_CREDIT_BG1(color) (GPACK_RGBA5551(0, 0, (color), 255) << 16 | GPACK_RGBA5551(0, 0, (color), 255))
// Background Color (Dark Grey)
#define COLOR_CREDIT_BG2(color) (GPACK_RGBA5551((color), (color), (color), 255) << 16 | GPACK_RGBA5551((color), (color), (color), 255))

void D_CreditDrawer(void)
{
    int color;

    I_ClearFrame();

    gDPPipeSync(GFX1++);
    gDPSetCycleType(GFX1++, G_CYC_FILL);
    gDPSetRenderMode(GFX1++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WD, OS_K0_TO_PHYSICAL(cfb[vid_side]));

    if (cred_next == 0)
    {
        color = (cred1_alpha * 16) / 255;
        gDPSetFillColor(GFX1++, COLOR_CREDIT_BG1(color));
        gDPFillRectangle(GFX1++, 0, 0, SCREEN_WD - 1, SCREEN_HT - 1);

        M_DrawBackground(68, 21, cred1_alpha, "IDCRED1");
        M_DrawBackground(32, 41, cred2_alpha, "IDCRED2");
    }
    else
    {
        if ((cred_next == 1) || (cred_next == 2))
        {
            color = (cred1_alpha * 30) / 255;
            gDPSetFillColor(GFX1++, COLOR_CREDIT_BG2(color));
            gDPFillRectangle(GFX1++, 0, 0, SCREEN_WD - 1, SCREEN_HT - 1);

            M_DrawBackground(22, 82, cred1_alpha, "WMSCRED1");
            M_DrawBackground(29, 28, cred2_alpha, "WMSCRED2");
        }
    }

    I_DrawFrame();
}

void D_OpenControllerPak(void)
{
    unsigned int oldbuttons;

    oldbuttons = I_GetControllerData();

    if (((oldbuttons & 0xffff0000) == PAD_START) && (I_CheckControllerPak() == 0))
    {
        MenuCall = M_ControllerPakDrawer;
        linepos = 0;
        cursorpos = 0;

        MiniLoop(M_FadeInStart, M_MenuClearCall, M_ScreenTicker, M_MenuGameDrawer);
        I_WIPE_FadeOutScreen();
    }

    return;
}
