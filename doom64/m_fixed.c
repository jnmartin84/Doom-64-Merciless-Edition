
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

fixed_t FixedDiv(fixed_t a, fixed_t b) // 80002BF8
{
    register unsigned    c;
    register unsigned    bit;
    register int    sign;

    sign = a^b;

    if (a < 0)
        a = -a;

    if (b < 0)
        b = -b;

    if ((unsigned)(a >> 14) >= b)
    {
        if (sign < 0)
            c = MININT;
        else
            c = MAXINT;
    }
    else
	{
		bit = 0x10000;
		do
		{
			b <<= 1;
			bit <<= 1;
		} while (b < a);

		c = 0;
		do
		{
			if (a >= b)
			{
				a -= b;
				c |= bit;
			}
			a <<= 1;
			bit >>= 1;
		} while (bit && a);

		if (sign < 0)
        {
			c = -c;
        }
	}

    return c;
}

fixed_t FixedDiv2(register fixed_t a, register fixed_t b)//L8003EEF0()
{
	register unsigned        c;
	register unsigned        bit;
	register int             sign;

	sign = a^b;

	if (a <= 0)
		a = -a;

	if (b <= 0)
		b = -b;

	bit = 0x10000;
	do
	{
		b <<= 1;
		bit <<= 1;
	} while (b < a);

	c = 0;
	do
	{
		if (a >= b)
		{
			a -= b;
			c |= bit;
		}
		a <<= 1;
		bit >>= 1;
	} while (bit && a);

	if (sign < 0)
		c = -c;

	return c;
}

/*
===============
=
= FixedMul
=
===============
*/

/*
see m_fixed_s.s
*/