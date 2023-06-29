// doomlib.c

#include "doomdef.h"

#define WORDMASK 7

void D_memmove(void *dest, void *src)
{
    byte p;
    byte *p1;
    byte *p2;

    p = *(byte *)src;
    p1 = (byte *)src;
    p2 = (byte *)dest;

    *p2++ = *p1++;

    while (p != '\0')
    {
        p = *p1;
        *p2++ = *p1++;
    }
}

#if 0
void D_memset(void *dest, int val, int count)
{
    byte *p;
    int *lp;
    int v;

    // round up to nearest word
    p = dest;
    while ((int)p & WORDMASK)
    {
        if (--count < 0)
        {
            return;
        }
        *p++ = (char)val;
    }

    // write 8 bytes at a time
    lp = (int *)p;
    v = (int)(val << 24) | (val << 16) | (val << 8) | val;
    while (count >= 8)
    {
        lp[0] = lp[1] = v;
        lp += 2;
        count -= 8;
    }

    // finish up
    p = (byte *)lp;
    while (count--)
    {
        *p++ = (char)val;
    }
}

void D_memcpy(void *dest, void *src, int count)
{
    byte *d, *s;
    int *ld, *ls;
    int stopcnt;

    ld = (int *)dest;
    ls = (int *)src;

    if ((((int)ld | (int)ls | count) & 7))
    {
        d = (byte *)dest;
        s = (byte *)src;
        while (count--)
        {
            *d++ = *s++;
        }
    }
    else
    {
        if (count == 0)
        {
            return;
        }
        if (-(count & 31))
        {
            stopcnt = -(count & 31) + count;
            while (stopcnt != count)
            {
                ld[0] = ls[0];
                ld[1] = ls[1];
                ld += 2;
                ls += 2;
                count -= 8;
            }

            if (count == 0)
            {
                return;
            }
        }

        while (count)
        {
            ld[0] = ls[0];
            ld[1] = ls[1];
            ld[2] = ls[2];
            ld[3] = ls[3];
            ld[4] = ls[4];
            ld[5] = ls[5];
            ld[6] = ls[6];
            ld[7] = ls[7];
            ld += 8;
            ls += 8;
            count -= 32;
        }
    }
}
#endif

void D_strncpy(char *dest, char *src, int maxcount)
{
    byte *p1, *p2;
    p1 = (byte *)dest;
    p2 = (byte *)src;
    while (maxcount--)
    {
        if (!(*p1++ = *p2++))
        {
            return;
        }
    }
}

int D_strncasecmp(char *s1, char *s2, int len)
{
    while (*s1 && *s2)
    {
        if (*s1 != *s2)
        {
            return 1;
        }
        s1++;
        s2++;
        if (!--len)
        {
            return 0;
        }
    }
    if (*s1 != *s2)
    {
        return 1;
    }
    return 0;
}

void D_strupr(char *s)
{
    char c;

    while ((c = *s) != 0)
    {
        if (c >= 'a' && c <= 'z')
        {
            c -= 'a' - 'A';
        }
        *s++ = c;
    }
}

int D_strlen(char *s)
{
    int len = 0;

    while (*(s++))
    {
        len++;
    }
    return len;
}
