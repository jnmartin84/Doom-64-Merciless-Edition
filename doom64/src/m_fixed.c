
/* m_fixed.c -- fixed point implementation */

#include "i_main.h"
#include "doomdef.h"
#include "p_spec.h"
#include "r_local.h"

/*
===============
=
= FixedDiv
=
===============
*/
#if 0
// TODO figure out why line of sight and intercept things break with this inlined
fixed_t FixedDiv(fixed_t a, fixed_t b) // 80002BF8
{
    fixed_t     aa, bb;
    unsigned    c;
    int         sign;

    sign = a^b;

    if (a < 0)
        aa = -a;
    else
        aa = a;

    if (b < 0)
        bb = -b;
    else
        bb = b;

    if ((unsigned)(aa >> 14) >= bb)
    {
        if (sign < 0)
            c = MININT;
        else
            c = MAXINT;
    }
    else
    {
//        c = (fixed_t) FixedDiv2(a, b);
    c = (fixed_t)((s64)((s64) a << 16) / (s64)b);
    }

    return c;
}
#endif


extern fixed_t finesinet[10240];
//extern boolean demoplayback;
extern u32 last_fs_count;
fixed_t fine_sincos(int x)
{
    fixed_t rv;
    int xs,xc;

    xs = x << 19;          // shift to full s32 range (Q13->Q30)

	// test for quadrant 1 or 2
    if ((xs ^ (xs << 1)) < 0)     
	{
        xs = (1<<31) - xs;
	}
    xs = xs >> 19;

    rv = (xs * (3072 - (xs*xs >> 12)) >> 7)<<16;

    xc = (x + 2048) << 19;

	// test for quadrant 1 or 2
    if ((xc ^ (xc << 1)) < 0)     
	{
        xc = (1<<31) - xc;
	}
    xc = xc >> 19;
    rv = rv | ((xc * (3072 - (xc*xc >> 12)) >> 7)&0xffff);

    return rv;

#if 0
return finesinet[x];
#endif
}

fixed_t finesine(int x)
{
#if 0
return finesinet[x+2048];
#endif
#if 1
    x = x << 19;          // shift to full s32 range (Q13->Q30)

	// test for quadrant 1 or 2
    if ((x ^ (x << 1)) < 0)     
	{
        x = (1<<31) - x;
	}
    x = x >> 19;

    return (x * (3072 - (x*x >> 12)) >> 6);
#endif
}	

fixed_t finecosine(int x)
{
#if 0
return finesinet[x+2048];
#endif
#if 1
    x = (x + 2048) << 19;          // shift to full s32 range (Q13->Q30)

	// test for quadrant 1 or 2
    if ((x ^ (x << 1)) < 0)     
	{
        x = (1<<31) - x;
	}
    x = x >> 19;

    return (x * (3072 - (x*x >> 12)) >> 6);
#endif
}	
extern angle_t tantoanglet[2049];
angle_t tantoangle(int x) {
#if 1
	if (x == 2048) return 0;

    return ((angle_t)((-47*((x)*(x))) + (359628*(x)) - 3150270));
#endif
//    return tantoanglet[x];
}