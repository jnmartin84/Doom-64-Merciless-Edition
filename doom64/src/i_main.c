
#include <ultra64.h>
#include <assert.h>

#include "i_main.h"
#include "doomdef.h"
#include "st_main.h"

int	SCREEN_HT = 480;
int	SCREEN_WD = 640;


/*
 * Symbol genererated by "makerom" to indicate the end of the code segment
 * in virtual (and physical) memory
 */
//extern char _codeSegmentEnd[];

/*
 * Stacks for the threads as well as message queues for synchronization
 * This stack is ridiculously large, and could also be reclaimed once
 * the main thread is started.
 */
u64	bootStack[STACKSIZE/sizeof(u64)];

extern u16 cfb[2][640*480];

extern int globallump; // r_local.h
extern int globalcm;   // r_local.h

#define SYS_THREAD_ID_IDLE 1
#define SYS_THREAD_ID_MAIN 2
#define SYS_THREAD_ID_TICKER 3

#define SYS_IDLE_STACKSIZE 0x2020
OSThread	idle_thread;                        // 800A4A18
u64	idle_stack[SYS_IDLE_STACKSIZE/sizeof(u64)]; // 800979E0

#define SYS_MAIN_STACKSIZE 0xA000
OSThread	main_thread;                        // 800A4BC8
u64	main_stack[SYS_MAIN_STACKSIZE/sizeof(u64)]; // 80099A00

#define SYS_TICKER_STACKSIZE 0x1000
OSThread	sys_ticker_thread;                          // 800A4D78
u64	sys_ticker_stack[SYS_TICKER_STACKSIZE/sizeof(u64)]; // 800A3A00

#define SYS_MSGBUF_SIZE_PI 128
OSMesgQueue msgque_Pi;                  // 800A4FA0
OSMesg msgbuf_Pi[SYS_MSGBUF_SIZE_PI];   // 800A4FD0

#define	SYS_FIFO_SIZE	512

#if __GNUC__ 
// for GNU compiler
u64 fifo_buff[2][SYS_FIFO_SIZE] __attribute__((aligned (16))); // buffer for RDP DL
u64 sys_rcp_stack[SP_DRAM_STACK_SIZE64] __attribute__((aligned (16))); // used for matrix stack
#else // for SGI compiler
u64 fifo_buff[2][SYS_FIFO_SIZE]; // for RDP DL
u64 sys_rcp_stack[SP_DRAM_STACK_SIZE64]; // used for matrix stack
#endif

#define	SYS_YIELD_SIZE  OS_YIELD_DATA_SIZE
u64 gfx_yield_buff[SYS_YIELD_SIZE];

OSTask vid_rsptask[2] = // 8005A590
{
    {
        M_GFXTASK,                          // task type
        NULL,                               // task flags
        (u64*) rspbootTextStart,            // boot ucode pointer (fill in later)
        0,                                  // boot ucode size (fill in later)
        (u64*) gspF3DEX2_NoN_fifoTextStart, // task ucode pointer (fill in later)
        SP_UCODE_SIZE,                      // task ucode size
        (u64*) gspF3DEX2_NoN_fifoDataStart, // task ucode data pointer (fill in later)
        SP_UCODE_DATA_SIZE,                 // task ucode data size
        &sys_rcp_stack[0],                  // task dram stack pointer
        SP_DRAM_STACK_SIZE8,                // task dram stack size
        &fifo_buff[0][0],                   // task fifo buffer start ptr
        &fifo_buff[0][0]+SYS_FIFO_SIZE,     // task fifo buffer end ptr
        NULL,                               // task data pointer (fill in later)
        0,                                  // task data size (fill in later)
        &gfx_yield_buff[0],                 // task yield buffer ptr (not used here)
        SYS_YIELD_SIZE                      // task yield buffer size (not used here)
    },
    {
        M_GFXTASK,                          // task type
        NULL,                               // task flags
        (u64*) rspbootTextStart,            // boot ucode pointer (fill in later)
        0,                                  // boot ucode size (fill in later)
        (u64*) gspF3DEX2_NoN_fifoTextStart, // task ucode pointer (fill in later)
        SP_UCODE_SIZE,                      // task ucode size
        (u64*) gspF3DEX2_NoN_fifoDataStart, // task ucode data pointer (fill in later)
        SP_UCODE_DATA_SIZE,                 // task ucode data size
        &sys_rcp_stack[0],                  // task dram stack pointer
        SP_DRAM_STACK_SIZE8,                // task dram stack size
        &fifo_buff[1][0],                   // task fifo buffer start ptr
        &fifo_buff[1][0]+SYS_FIFO_SIZE,     // task fifo buffer end ptr
        NULL,                               // task data pointer (fill in later)
        0,                                  // task data size (fill in later)
        &gfx_yield_buff[0],                 // task yield buffer ptr (not used here)
        SYS_YIELD_SIZE                      // task yield buffer size (not used here)
    }
};

Vp vid_viewport320 = 
{
    320*2, 240*2, G_MAXZ,   0,		/* scale */
    320*2, 240*2,      0,   0,		/* translate */
};

Vp vid_viewport640 = 
{
    640*2, 480*2, G_MAXZ,   0,		/* scale */
    640*2, 480*2,      0,   0,		/* translate */
};

OSMesgQueue romcopy_msgque; // 800A4F70
OSMesg		romcopy_msgbuf; // 800A51D0

OSMesgQueue sys_msgque_joy; // 800A4F88
OSMesg		sys_msg_joy;    // 800A51D4

#define SYS_MSGBUF_SIZE_VID 16
OSMesgQueue sys_msgque_vbi; // 800A4FB8
OSMesg		sys_msgbuf_vbi[SYS_MSGBUF_SIZE_VID]; // 800A51E0

#define SYS_MSGBUF_SIZE_VID2 2
OSMesgQueue sys_msgque_vbi2; // 800A4F28
OSMesg		sys_msgbuf_vbi2[SYS_MSGBUF_SIZE_VID2]; // 800A51D8

OSMesgQueue sys_msgque_vbi3; // 800A4F40
OSMesg		sys_msgbuf_vbi3[SYS_MSGBUF_SIZE_VID2]; // 800A5220

OSMesgQueue sys_msgque_vbi4; // 800A4F58
OSMesg		sys_msgbuf_vbi4[SYS_MSGBUF_SIZE_VID2]; // 800A5228

OSContStatus gamepad_status[MAXCONTROLLERS]; // 800a5230
OSContPad   *gamepad_data;    // 800A5240

OSTask *vid_task;   // 800A5244
u32 vid_side;       // 800A5248

u32 video_hStart;   // 800A524c
u32 video_vStart1;  // 800A5250
u32 video_vStart2;  // 800A5254

u32 GfxIndex;       // 800A5258
u32 VtxIndex;       // 800A525C

u8 gamepad_bit_pattern; // 800A5260 // one bit for each controller

// Controller Pak
OSPfs ControllerPak;        // 800A5270
OSPfsState FileState[16];   // 800A52D8
s32 File_Num;   // 800A54D8
s32 Pak_Size;   // 800A54DC
u8 *Pak_Data;   // 800A54E0
s32 Pak_Memory; // 800A54E4

char Pak_Table[256] = // 8005A620
{
      '\0',// 0
      ' ', ' ', ' ', ' ', ' ',// 1
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 6
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',// 16
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',// 26
      'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',// 36
      'u', 'v', 'w', 'x', 'y', 'z', '!', '"', '#','\'',// 46
      '*', '+', ',', '-', '.', '/', ':', '=', '?', '@',// 56
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 66
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 76
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 86
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 96
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 106
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 116
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 126
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 136
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 146
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 156
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 166
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 176
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 186
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 196
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 206
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 216
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 226
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',// 236
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' // 246
};

char Game_Name[16] = // 8005A790
{
    0x1D, 0x28, 0x28, 0x26, 0x0F, 0x16, 0x14, 0x00, // (doom 64) byte index from Pak_Table
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

boolean disabledrawing = false; // 8005A720

s32 vsync = 0;              // 8005A724
s32 drawsync2 = 0;          // 8005A728
s32 drawsync1 = 0;          // 8005A72C
u32 NextFrameIdx = 0;       // 8005A730

s32 ControllerPakStatus = 1; // 8005A738
s32 gamepad_system_busy = 0; // 8005A73C
s32 FilesUsed = -1;                 // 8005A740
u32 SystemTickerStatus = 0;  // 8005a744

Gfx Gfx_base[2][MAX_GFX];    // 800653E0
Mtx Mtx_base[2][MAX_MTX];    // 800793E0
Vtx Vtx_base[2][MAX_VTX];    // 800795E0

Gfx *GFX1;	// 800A4A00
Gfx *GFX2;	// 800A4A04

Vtx *VTX1;	// 800A4A08
Vtx *VTX2;	// 800A4A0C

Mtx *MTX1;	// 800A4A10
Mtx *MTX2;	// 800A4A14

Gfx *GfxBlocks[8] = {0,0,0,0,0,0,0,0}; // 8005A748
Vtx *VtxBlocks[8] = {0,0,0,0,0,0,0,0}; // 8005A768

extern OSTask * wess_work(void);

void I_Start(void)  // 80005620
{
    /* Re-initialize U64 operating system... */
    osInitialize();

    /* Create and start idle thread... */
    osCreateThread(&idle_thread, SYS_THREAD_ID_IDLE, I_IdleGameThread, (void *)0,
                   idle_stack + SYS_IDLE_STACKSIZE/sizeof(u64), 10);
    osStartThread(&idle_thread);
}

void I_IdleGameThread(void *arg) // 8000567C
{
    /* Create and start the Pi manager... */
    osCreatePiManager( (OSPri)OS_PRIORITY_PIMGR, &msgque_Pi, msgbuf_Pi,
                        SYS_MSGBUF_SIZE_PI );

    /* Create main thread... */
    osCreateThread(&main_thread, SYS_THREAD_ID_MAIN, I_Main, (void *)0,
                   main_stack + SYS_MAIN_STACKSIZE/sizeof(u64), 10);
    osStartThread(&main_thread);

    osSetThreadPri(&idle_thread, (OSPri)OS_PRIORITY_IDLE);

    /* Idle loop... */
    do {
        osYieldThread();
    } while(TRUE);
}

void I_Main(void *arg) // 80005710
{
    D_DoomMain();
}

void I_SystemTicker(void *arg) // 80005730
{
    int vbi_msg;
    int vidside;
    int side, ret;
    int current_fbuf, next_fbuf;
    OSTask *wess;
    OSTask *rspTask;
    OSTask *rspTaskPrev;

    //char str[64];

    vidside = 0;
    rspTask = NULL;
    rspTaskPrev = NULL;
    side = 1;

    while(true)
    {
        osRecvMesg(&sys_msgque_vbi, (OSMesg *)&vbi_msg, OS_MESG_BLOCK);

        //sprintf(str, "SystemTickerStatus %d",SystemTickerStatus);
        //printstr(WHITE, 0, 13, str);
        //sprintf(str, "vbi_msg %d",vbi_msg);
        //printstr(WHITE, 0, 14, str);
        //sprintf(str, "vidside %d",vidside);
        //printstr(WHITE, 0, 15, str);

        switch (vbi_msg)
		{
		    case VID_MSG_RSP:				// end of signal processing
		        {
		            //sprintf(str, "VID_MSG_RSP");
                    //printstr(WHITE, 0, 28, str);

                    if(rspTask->t.type == M_AUDTASK)
                    {
                        SystemTickerStatus &= ~32;

                        if (SystemTickerStatus & 4)//OS_SC_YIELDED
                        {
                            SystemTickerStatus &= ~4;
                            SystemTickerStatus |= 1;

                            rspTask = rspTaskPrev;
                            osWritebackDCacheAll();
                            osSpTaskLoad(rspTask);
                            osSpTaskStartGo(rspTask);
                        }
                        else
                        {
                            if ((SystemTickerStatus & 24) == 0)
                            {
                                ret = osRecvMesg(&sys_msgque_vbi3, (OSMesg *)&vbi_msg, OS_MESG_NOBLOCK);
                                rspTask = (OSTask*)vbi_msg;

                                if(ret != -1)
                                {
                                    if(rspTask == vid_rsptask)
                                        vidside = 0;
                                    else
                                        vidside = 1;

                                    SystemTickerStatus |= 1;

                                    osWritebackDCacheAll();
                                    osSpTaskLoad(rspTask);
                                    osSpTaskStartGo(rspTask);
                                }
                            }
                        }
                    }
                    else
                    {
                        SystemTickerStatus &= ~1;

                        if(SystemTickerStatus & 2)//OS_SC_YIELD
                        {
                            SystemTickerStatus &= ~2;

                            if (osSpTaskYielded(rspTask))
                            {
                                rspTaskPrev = rspTask;

                                SystemTickerStatus |= 4;//OS_SC_YIELDED

                                osRecvMesg(&sys_msgque_vbi4, (OSMesg *)&vbi_msg, OS_MESG_NOBLOCK);
                                rspTask = (OSTask*)vbi_msg;

                                SystemTickerStatus |= 32;

                                osWritebackDCacheAll();
                                osSpTaskLoad(rspTask);
                                osSpTaskStartGo(rspTask);
                            }
                        }
                        else
                        {
                            if ((SystemTickerStatus & 16) == 0)
                                SystemTickerStatus |= 8;

                            ret = osRecvMesg(&sys_msgque_vbi4, (OSMesg *)&vbi_msg, OS_MESG_NOBLOCK);
                            rspTask = (OSTask*)vbi_msg;

                            if(ret!= -1)
                            {
                                SystemTickerStatus |= 32;

                                osWritebackDCacheAll();
                                osSpTaskLoad(rspTask);
                                osSpTaskStartGo(rspTask);
                            }
                        }
                    }
		        }
				break;

            case VID_MSG_RDP:				// end of display processing
				{
				    //sprintf(str, "VID_MSG_RDP");
                    //printstr(WHITE, 0, 28, str);

				    SystemTickerStatus &= ~8;
				    SystemTickerStatus |= 16;

                    osViSwapBuffer(cfb[vidside]);
				}
				break;

            case VID_MSG_PRENMI:
                {
                    //sprintf(str, "VID_MSG_PRENMI");
                    //printstr(WHITE, 0, 28, str);
				    disabledrawing = true;
                    S_StopAll();
                    osViBlack(TRUE);
                    I_MoveDisplay(0,0);
                }
				break;

            case VID_MSG_VBI:
                {
                    //sprintf(str, "VID_MSG_VBI || vsync(%d) || side(%d)", vsync, side);
                    //printstr(WHITE, 0, 28, str);

                    vsync += 1;

                    if (sys_msgque_vbi4.validCount)
                    {
                        if (SystemTickerStatus & 1)
                        {
                            SystemTickerStatus |= 2; // GFX_YIELDED
                            osSpTaskYield();
                        }
                        else
                        {
                            if ((SystemTickerStatus & 32) == 0)
                            {
                                osRecvMesg(&sys_msgque_vbi4, (OSMesg *)&vbi_msg, OS_MESG_NOBLOCK);
                                rspTask = (OSTask*)vbi_msg;

                                SystemTickerStatus |= 32;

                                osWritebackDCacheAll();
                                osSpTaskLoad(rspTask);
                                osSpTaskStartGo(rspTask);
                            }
                        }
                    }

                    if (side & 1)
                    {
                        if (gamepad_system_busy)
                        {
                            osContGetReadData(gamepad_data);
                            gamepad_system_busy = 0;
                        }

                        // next audio function task
                        wess = wess_work();
                        if (wess)
                            osSendMesg(&sys_msgque_vbi4,(OSMesg) wess, OS_MESG_NOBLOCK);
                    }
                    side++;

                    if (SystemTickerStatus & 16)
                    {
                        if ((u32)(vsync - drawsync2) < 2) continue;

                        current_fbuf = (int)osViGetCurrentFramebuffer();
                        next_fbuf = (int)osViGetNextFramebuffer();

                        if (next_fbuf != current_fbuf) continue;

                        SystemTickerStatus &= ~16;

                        if (demoplayback || demorecording)
                        {
                            vsync = drawsync2 + 2;
                        }

                        //sprintf(str, "vsync %d | side %d",vsync, side);
                        //printstr(WHITE, 0, 29, str);

                        drawsync1 = vsync - drawsync2;
                        drawsync2 = vsync;

                        if ((ControllerPakStatus != 0) && (gamepad_system_busy == 0))
                        {
                            osContStartReadData(&sys_msgque_joy);
                            gamepad_system_busy = 1;
                        }

                        osSendMesg(&sys_msgque_vbi2, (OSMesg)VID_MSG_KICKSTART, OS_MESG_NOBLOCK);
                    }

                    if(SystemTickerStatus == 0)
                    {
                        ret = osRecvMesg(&sys_msgque_vbi3, (OSMesg *)&vbi_msg, OS_MESG_NOBLOCK);
                        rspTask = (OSTask*)vbi_msg;

                        //sprintf(str, "ret %d", ret);
                        //printstr(WHITE, 0, 17, str);

                        if(ret != -1)
                        {
                            //sprintf(str, "rspTask->t.type %d",rspTask->t.type);
                            //printstr(WHITE, 0, 14, str);
                            //sprintf(str, "rspTask->t.type %x",(u64*)rspTask->t.ucode);
                            //printstr(WHITE, 0, 15, str);

                            if(rspTask == vid_rsptask)
                                vidside = 0;
                            else
                                vidside = 1;

                            SystemTickerStatus |= 1;

                            osWritebackDCacheAll();
                            osSpTaskLoad(rspTask);
                            osSpTaskStartGo(rspTask);
                        }
                    }
                }
				break;
		}
    }
}

extern void S_Init(void);

void I_Init(void) // 80005C50
{
    // assume NTSC
    OSViMode *ViMode = &osViModeTable[OS_VI_NTSC_LPN1];

    vid_rsptask[0].t.ucode_boot_size = (int)rspbootTextEnd - (int)rspbootTextStart;	// set ucode size (waste but who cares)
    vid_rsptask[1].t.ucode_boot_size = (int)rspbootTextEnd - (int)rspbootTextStart;	// set ucode size (waste but who cares)

    osCreateMesgQueue( &romcopy_msgque, &romcopy_msgbuf, 1 );

    osCreateMesgQueue( &sys_msgque_vbi, sys_msgbuf_vbi, SYS_MSGBUF_SIZE_VID );

    osCreateMesgQueue(&sys_msgque_vbi2, sys_msgbuf_vbi2, SYS_MSGBUF_SIZE_VID2);//&sys_msgque_jam, sys_msgbuf_jam
    osCreateMesgQueue(&sys_msgque_vbi3, sys_msgbuf_vbi3, SYS_MSGBUF_SIZE_VID2);//&sys_msgque_ser, sys_msgbuf_ser
    osCreateMesgQueue(&sys_msgque_vbi4, sys_msgbuf_vbi4, SYS_MSGBUF_SIZE_VID2);//&sys_msgque_tmr, sys_msgbuf_tmr

    if(osTvType == OS_TV_PAL)
    {
        // diff for hi-res pr
if (SCREEN_WD == 640)
        ViMode = &osViModeTable[OS_VI_PAL_HPN1];
else
        ViMode = &osViModeTable[OS_VI_PAL_LPN1];
    }
    else if(osTvType == OS_TV_NTSC)
    {
        // diff for hi-res pr
if (SCREEN_WD == 640)
        ViMode = &osViModeTable[OS_VI_NTSC_HPN1];
else
        ViMode = &osViModeTable[OS_VI_NTSC_LPN1];
    }
    else if(osTvType == OS_TV_MPAL)
    {
        // diff for hi-res pr
if (SCREEN_WD == 640)
        ViMode = &osViModeTable[OS_VI_MPAL_HPN1];
else
        ViMode = &osViModeTable[OS_VI_MPAL_LPN1];
    }

    video_hStart = ViMode->comRegs.hStart;
    video_vStart1 = ViMode->fldRegs[0].vStart;
    video_vStart2 = ViMode->fldRegs[1].vStart;

    // Create and start the Vi manager and init the video mode...

	osCreateViManager( OS_PRIORITY_VIMGR );
    osViSetMode(ViMode);
    osViBlack(TRUE);

    osViSetSpecialFeatures(OS_VI_GAMMA_OFF|OS_VI_GAMMA_DITHER_OFF|OS_VI_DIVOT_OFF|OS_VI_DITHER_FILTER_OFF);

    osViSetXScale(1.0);
    osViSetYScale(1.0);

    D_memset(cfb, 0, ((SCREEN_WD*SCREEN_HT)*sizeof(u16))*2);
    osViSwapBuffer(cfb);

    if (osViGetCurrentFramebuffer() != cfb) {
		do {
		} while (osViGetCurrentFramebuffer() != cfb);
	}

    osViBlack(FALSE);

    osSetEventMesg( OS_EVENT_SP, &sys_msgque_vbi, (OSMesg)VID_MSG_RSP );
    osSetEventMesg( OS_EVENT_DP, &sys_msgque_vbi, (OSMesg)VID_MSG_RDP );
    osSetEventMesg( OS_EVENT_PRENMI, &sys_msgque_vbi, (OSMesg)VID_MSG_PRENMI );

    osViSetEvent( &sys_msgque_vbi, (OSMesg)VID_MSG_VBI, 1 ); // last parm: 2 indicates 30 FPS (1=60)

    vid_side = 1;

    /* Serial/Joy queue */

    osCreateMesgQueue(&sys_msgque_joy, &sys_msg_joy, 1);
    osSetEventMesg(OS_EVENT_SI, &sys_msgque_joy, &sys_msg_joy);

    osContInit(&sys_msgque_joy, &gamepad_bit_pattern, gamepad_status);

    gamepad_data = (OSContPad *)idle_stack;

    if ((gamepad_bit_pattern & 1) != 0)
    {
        osContStartReadData(&sys_msgque_joy);
        osRecvMesg(&sys_msgque_joy, NULL, OS_MESG_BLOCK);
        osContGetReadData(gamepad_data);
    }

    S_Init();

    /* Create and start ticker thread... */
    osCreateThread(&sys_ticker_thread, SYS_THREAD_ID_TICKER, I_SystemTicker, (void *)0,
                   sys_ticker_stack + SYS_TICKER_STACKSIZE/sizeof(u64), 11);
    osStartThread(&sys_ticker_thread);

    osJamMesg(&sys_msgque_vbi2, (OSMesg)VID_MSG_KICKSTART, OS_MESG_NOBLOCK);
}

#include "stdarg.h"

void I_Error(char *error, ...) // 80005F30
{
    char buffer[256];
    va_list args;
    va_start (args, error);
    D_vsprintf (buffer, error, args);
    va_end (args);

    while (true)
    {
        I_ClearFrame();

        gDPPipeSync(GFX1++);
        gDPSetCycleType(GFX1++, G_CYC_FILL);
        gDPSetRenderMode(GFX1++,G_RM_NOOP,G_RM_NOOP2);
        gDPSetColorImage(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WD, OS_K0_TO_PHYSICAL(cfb[vid_side]));
        gDPSetFillColor(GFX1++, GPACK_RGBA5551(0,0,0,0) << 16 | GPACK_RGBA5551(0,0,0,0)) ;
        gDPFillRectangle(GFX1++, 0, 0, SCREEN_WD-1, SCREEN_HT-1);

        ST_Message(err_text_x, err_text_y, buffer, 0xffffffff);
        I_DrawFrame();
    }
}

typedef struct
{
	int pad_data;
} pad_t;

int I_GetControllerData(void) // 800060D0
{
    return ((pad_t*)gamepad_data)->pad_data;
}

void I_CheckGFX(void) // 800060E8
{
	memblock_t *block;

	Gfx **Gfx_Blocks;
	Vtx **Vtx_Blocks;

	int i, index;
	int block_idx;

	index = (int)((int)GFX1 - (int)GFX2) / sizeof(Gfx);

	if (index > MAX_GFX)
		I_Error("I_CheckGFX: GFX Overflow by %d\n",index);

	if ((index < (MAX_GFX-1024)) == 0)
	{
		Gfx_Blocks = GfxBlocks;
		block_idx = -1;

		for(i = 0; i < 8; i++)
		{
			block = (memblock_t *)((byte *)*Gfx_Blocks - sizeof(memblock_t));

			if (*Gfx_Blocks)
			{
				if(((u32)block->lockframe < NextFrameIdx - 1) == 0)
                {
                    Gfx_Blocks++;
                    continue;
                }

                block->lockframe = NextFrameIdx;
                GFX2 = (Gfx *)*Gfx_Blocks;
                goto move_gfx;
			}

            block_idx = i;
		}

		if (block_idx < 0)
			I_Error("I_CheckGFX: GFX Cache overflow");

		GFX2 = (Gfx *)Z_Malloc(MAX_GFX * sizeof(Gfx), PU_CACHE, &GfxBlocks[block_idx]);

	move_gfx:
		gSPBranchList(GFX1,GFX2);
		GFX1 = GFX2;
		GfxIndex += index;
	}

	index = (int)((int)VTX1 - (int)VTX2) / sizeof(Vtx);

	if (index > MAX_VTX)
		I_Error("I_CheckVTX: VTX Overflow by %d\n",index);

	if ((index < (MAX_VTX-615)) == 0)
	{
		Vtx_Blocks = VtxBlocks;
		block_idx = -1;

		for(i = 0; i < 8; i++)
		{
		    block = (memblock_t *)((byte *)*Vtx_Blocks - sizeof(memblock_t));

			if (*Vtx_Blocks)
			{
				if(((u32)block->lockframe < NextFrameIdx - 1) == 0)
                {
                    Vtx_Blocks++;
                    continue;
                }

                block->lockframe = NextFrameIdx;
                VTX2 = (Vtx *)*Vtx_Blocks;
                goto move_vtx;
			}

            block_idx = i;
		}

		if (block_idx < 0)
			I_Error("I_CheckGFX: VTX Cache overflow");

        VTX2 = (Vtx *)Z_Malloc(MAX_VTX * sizeof(Vtx), PU_CACHE, &VtxBlocks[block_idx]);

	move_vtx:
		VTX1 = VTX2;
		VtxIndex += index;
	}
}

void I_ClearFrame(void) // 8000637C
{
    NextFrameIdx += 1;

    GFX1 = Gfx_base[vid_side];
    GFX2 = GFX1;
    GfxIndex = 0;

    VTX1 = Vtx_base[vid_side];
    VTX2 = VTX1;
    VtxIndex = 0;

    MTX1 = Mtx_base[vid_side];

    vid_task = &vid_rsptask[vid_side];

    vid_task->t.ucode = (u64 *) gspF3DEX2_NoN_fifoTextStart;
    vid_task->t.ucode_data = (u64 *) gspF3DEX2_NoN_fifoDataStart;

    gMoveWd(GFX1++, G_MW_SEGMENT, G_MWO_SEGMENT_0, 0);

    gDPSetColorImage(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WD, OS_K0_TO_PHYSICAL(cfb[vid_side]));
    gDPSetScissor(GFX1++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WD, SCREEN_HT);//32, 32, SCREEN_WD-(16*(SCREEN_WD/320)), SCREEN_HT-(16*(SCREEN_WD/320)));

	R_RenderFilter();    // [GEC and Immorpher] New filter options

if(SCREEN_WD == 640) 
{
    gSPViewport(GFX1++, &vid_viewport640);
}
else
{
    gSPViewport(GFX1++, &vid_viewport320);
}
    gSPClearGeometryMode(GFX1++, -1);
    gSPSetGeometryMode(GFX1++, G_SHADE|G_SHADING_SMOOTH|G_FOG );

    globallump = -1;
    globalcm = 0;
}

void I_DrawFrame(void)  // 80006570
{
    int index;

    gDPFullSync(GFX1++);
    gSPEndDisplayList(GFX1++);

    index = (int)((int)GFX1 - (int)GFX2) / sizeof(Gfx);
	if (index > MAX_GFX)
		I_Error("I_DrawFrame: GFX Overflow by %d\n\n",index);

    index = (int)((int)VTX1 - (int)VTX2) / sizeof(Vtx);
	if (index > MAX_VTX)
		I_Error("I_DrawFrame: VTX Overflow by %d\n",index);

    vid_task->t.data_ptr = (u64 *) Gfx_base[vid_side];
    vid_task->t.data_size = (u32)((((int)((int)GFX1 - (int)GFX2) / sizeof(Gfx)) + GfxIndex) * sizeof(Gfx));

    osSendMesg(&sys_msgque_vbi3,(OSMesg) vid_task, OS_MESG_NOBLOCK);
    osRecvMesg(&sys_msgque_vbi2, NULL, OS_MESG_BLOCK);//retraceMessageQ
    vid_side ^= 1;
}

void I_GetScreenGrab(void) // 800066C0
{
    if ((SystemTickerStatus & ~32) || (sys_msgque_vbi3.validCount != 0)) {
        osRecvMesg(&sys_msgque_vbi2, (OSMesg *)0, OS_MESG_BLOCK);
        osJamMesg(&sys_msgque_vbi2, (OSMesg)VID_MSG_KICKSTART, OS_MESG_NOBLOCK);
    }
}

long LongSwap(long dat) // 80006724
{
    return (u32)dat >> 0x18 | (dat >> 8 & 0xff00U) | (dat & 0xff00U) << 8 | dat << 0x18;
}

short LittleShort(short dat) // 80006750
{
    return ((((dat << 8) | (dat >> 8 & 0xff)) << 16) >> 16);
}

short BigShort(short dat) // 80006770
{
    return ((dat << 8) | (dat >> 8 & 0xff)) & 0xffff;
}

void I_MoveDisplay(int x,int y) // 80006790
{
  int ViMode;

  ViMode = osViGetCurrentMode();

  osViModeTable[ViMode].comRegs.hStart =
       (int)(((int)video_hStart >> 0x10 & 65535) + x) % 65535 << 0x10 |
       (int)((video_hStart & 65535) + x) % 65535;

  osViModeTable[ViMode].fldRegs[0].vStart =
       (int)(((int)video_vStart1 >> 0x10 & 65535) + y) % 65535 << 0x10 |
       (int)((video_vStart1 & 65535) + y) % 65535;

  osViModeTable[ViMode].fldRegs[1].vStart =
       (int)(((int)video_vStart2 >> 0x10 & 65535) + y) % 65535 << 0x10 |
       (int)((video_vStart2 & 65535) + y) % 65535;
}

void I_WIPE_MeltScreen(void) // 80006964
{
    u16 *fb;
    int y1;
    int tpos;
    int yscroll;
    int height;

    fb = Z_Malloc((SCREEN_WD*SCREEN_HT)*sizeof(u16), PU_STATIC, NULL);

    I_GetScreenGrab();
    D_memcpy(&cfb[vid_side][0], &cfb[vid_side ^ 1][0], (SCREEN_WD*SCREEN_HT)*sizeof(u16));

    yscroll = 1;
    while( true )
    {
        y1 = 0;
        D_memcpy(fb, &cfb[vid_side ^ 1][0], (SCREEN_WD*SCREEN_HT)*sizeof(u16));

        I_ClearFrame();

        gDPSetCycleType(GFX1++, G_CYC_1CYCLE);
        gDPSetTextureLUT(GFX1++, G_TT_NONE);
        gDPSetTexturePersp(GFX1++, G_TP_NONE);
        gDPSetAlphaCompare(GFX1++, G_AC_THRESHOLD);
        gDPSetBlendColor(GFX1++, 0, 0, 0, 0);
        gDPSetCombineMode(GFX1++, G_CC_D64COMB19, G_CC_D64COMB19);
        gDPSetRenderMode(GFX1++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetPrimColor(GFX1++, 0, 0, 15, 0, 0, 22); // 0x0f000016

        height = SCREEN_HT - (yscroll >> 2);
        tpos = 0;
        if (height > 0)
        {
            do
            {
                gDPSetTextureImage(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b , SCREEN_WD, fb);
                gDPSetTile(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                           (SCREEN_WD >> 2), 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);

                gDPLoadSync(GFX1++);
                gDPLoadTile(GFX1++, G_TX_LOADTILE,
                            (0 << 2), (tpos << 2),
                            ((SCREEN_WD-1) << 2), (((tpos+3)-1) << 2));

                gDPPipeSync(GFX1++);
                gDPSetTile(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                           (SCREEN_WD >> 2), 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);

                gDPSetTileSize(GFX1++, G_TX_RENDERTILE,
                               (0 << 2), (tpos << 2),
                               ((SCREEN_WD-1) << 2), (((tpos+3)-1) << 2));

                gSPTextureRectangle(GFX1++,
                                    (0 << 2), (y1 << 2) + yscroll,
                                    (SCREEN_WD << 2), ((y1 + 3) << 2) + yscroll,
                                    G_TX_RENDERTILE,
                                    (0 << 5), (tpos << 5),
                                    (1 << 10), (1 << 10));

                // diff for hi-res pr
                y1 += 2*(SCREEN_WD/320);
                tpos += 2*(SCREEN_WD/320);
            } while (y1 < height);
        }

        // diff for hi-res pr
        yscroll += 2*(SCREEN_WD/320);
        if (yscroll >= (SCREEN_WD>>1)) break;
        I_DrawFrame();
    }

    Z_Free(fb);
    I_WIPE_FadeOutScreen();
}

void I_WIPE_FadeOutScreen(void) // 80006D34
{
    u32 *fb;
    int y1, tpos, outcnt;

    fb = Z_Malloc((SCREEN_WD*SCREEN_HT)*sizeof(u32), PU_STATIC, NULL);

    I_GetScreenGrab();
    D_memcpy(fb, &cfb[vid_side ^ 1][0], (SCREEN_WD*SCREEN_HT)*sizeof(u16));

    outcnt = 248;
    do
    {
        I_ClearFrame();

        gDPSetCycleType(GFX1++, G_CYC_1CYCLE);
        gDPSetTextureLUT(GFX1++, G_TT_NONE);
        gDPSetTexturePersp(GFX1++, G_TP_NONE);
        gDPSetAlphaCompare(GFX1++, G_AC_NONE);
        gDPSetCombineMode(GFX1++, G_CC_D64COMB06, G_CC_D64COMB06);
        gDPSetRenderMode(GFX1++,G_RM_OPA_SURF,G_RM_OPA_SURF2);
        gDPSetPrimColor(GFX1++, 0, 0, outcnt, outcnt, outcnt, 0);

        tpos = 0;
        y1 = 0;
        do
        {
            gDPSetTextureImage(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b , SCREEN_WD, fb);
            gDPSetTile(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                       (SCREEN_WD >> 2), 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);

            gDPLoadSync(GFX1++);
            gDPLoadTile(GFX1++, G_TX_LOADTILE,
                        (0 << 2), (tpos << 2),
                        ((SCREEN_WD-1) << 2), (((tpos+3)-1) << 2));

            gDPPipeSync(GFX1++);
            gDPSetTile(GFX1++, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                       (SCREEN_WD >> 2), 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);

            gDPSetTileSize(GFX1++, G_TX_RENDERTILE,
                           (0 << 2), (tpos << 2),
                           ((SCREEN_WD-1) << 2), (((tpos+3)-1) << 2));

            gSPTextureRectangle(GFX1++,
                                (0 << 2), (y1 << 2),
                                (SCREEN_WD << 2), ((y1+3) << 2),
                                G_TX_RENDERTILE,
                                (0 << 5), (tpos << 5),
                                (1 << 10), (1 << 10));

            tpos += 3;
            y1 += 3;
        } while (y1 != SCREEN_HT);

        I_DrawFrame();
        outcnt -= 8;
    } while (outcnt >= 0);

    I_GetScreenGrab();
    Z_Free(fb);
}


int I_CheckControllerPak(void) // 800070B0
{
    int ret, file;
    OSPfsState *fState;
    s32 MaxFiles [2];
    u8 validpaks;

    ControllerPakStatus = 0;

    if (gamepad_system_busy != 0)
    {
        do {
            osYieldThread();
        } while (gamepad_system_busy != 0);
    }

    FilesUsed = -1;
    ret = PFS_ERR_NOPACK;

    osPfsIsPlug(&sys_msgque_joy, &validpaks);

    /* does the current controller have a memory pak? */
    if (validpaks & 1)
    {
        ret = osPfsInit(&sys_msgque_joy, &ControllerPak, NULL);

        if ((ret != PFS_ERR_NOPACK) &&
            (ret != PFS_ERR_ID_FATAL) &&
            (ret != PFS_ERR_DEVICE) &&
            (ret != PFS_ERR_CONTRFAIL))
        {
            ret = osPfsNumFiles(&ControllerPak, MaxFiles, &FilesUsed);

            if (ret == PFS_ERR_INCONSISTENT)
                ret = osPfsChecker(&ControllerPak);

            if (ret == 0)
            {
                Pak_Memory = 123;
                fState = FileState;
                file = 0;
                do
                {
                    ret = osPfsFileState(&ControllerPak, file, fState);
                    file += 1;

                    if (ret != 0)
                      fState->file_size = 0;

                    Pak_Memory -= (fState->file_size >> 8);
                    fState += 1;
                } while (file != 16);
                ret = 0;
            }
        }
    }

    ControllerPakStatus = 1;

    return ret;
}

int I_DeletePakFile(int filenumb) // 80007224
{
    int ret;
    OSPfsState *fState;

    ControllerPakStatus = 0;

    if (gamepad_system_busy != 0)
    {
        do {
            osYieldThread();
        } while (gamepad_system_busy != 0);
    }

    fState = &FileState[filenumb];

    if (fState->file_size == 0) {
        ret = 0;
    }
    else
    {
        ret = osPfsDeleteFile(&ControllerPak,
            FileState[filenumb].company_code,
            FileState[filenumb].game_code,
            (u8*)FileState[filenumb].game_name,
            (u8*)FileState[filenumb].ext_name);

        if (ret == PFS_ERR_INCONSISTENT)
            ret = osPfsChecker(&ControllerPak);

        if (ret == 0)
        {
            Pak_Memory += (fState->file_size >> 8);
            fState->file_size = 0;
        }
    }

    ControllerPakStatus = 1;

    return ret;
}

int I_SavePakFile(int filenumb, int flag, byte *data, int size) // 80007308
{
    int ret;

    ControllerPakStatus = 0;

    if (gamepad_system_busy != 0)
    {
        do {
        osYieldThread();
        } while (gamepad_system_busy != 0);
    }

    ret = osPfsReadWriteFile(&ControllerPak, filenumb, (u8)flag, 0, size, (u8*)data);

    if (ret == PFS_ERR_INCONSISTENT)
        ret = osPfsChecker(&ControllerPak);

    ControllerPakStatus = 1;

    return ret;
}

#define COMPANY_CODE 0x3544     // 5D
#define GAME_CODE 0x4e444d45    // NDME

int I_ReadPakFile(void) // 800073B8
{
    int ret;
    u8 *ext_name;

    ControllerPakStatus = 0;

    if (gamepad_system_busy != 0)
    {
        do {
        osYieldThread();
        } while (gamepad_system_busy != 0);
    }

    Pak_Data = NULL;
    Pak_Size = 0;
    ext_name = NULL;

    ret = osPfsFindFile(&ControllerPak, COMPANY_CODE, GAME_CODE, (u8*)Game_Name, ext_name, &File_Num);

    if (ret == 0)
    {
        Pak_Size = FileState[File_Num].file_size;
        Pak_Data = (byte *)Z_Malloc(Pak_Size, PU_STATIC, NULL);
        ret = osPfsReadWriteFile(&ControllerPak, File_Num, PFS_READ, 0, Pak_Size, Pak_Data);
    }

    ControllerPakStatus = 1;

    return ret;
}

int I_CreatePakFile(void) // 800074D4
{
    int ret;
    u8 ExtName [8];

    ControllerPakStatus = 0;

    if (gamepad_system_busy != 0)
    {
        do {
          osYieldThread();
        } while (gamepad_system_busy != 0);
    }

    if (Pak_Memory < 2)
        Pak_Size = 256;
    else
        Pak_Size = 512;

    Pak_Data = (byte *)Z_Malloc(Pak_Size, PU_STATIC, NULL);
    D_memset(Pak_Data, 0, Pak_Size);

    *(int*)ExtName = 0;

    ret = osPfsAllocateFile(&ControllerPak, COMPANY_CODE, GAME_CODE, (u8*)Game_Name, ExtName, Pak_Size, &File_Num);

    if (ret == PFS_ERR_INCONSISTENT)
        ret = osPfsChecker(&ControllerPak);

    if (ret == 0)
        ret = osPfsReadWriteFile(&ControllerPak, File_Num, PFS_WRITE, 0, Pak_Size, Pak_Data);

    ControllerPakStatus = 1;

    return ret;
}
