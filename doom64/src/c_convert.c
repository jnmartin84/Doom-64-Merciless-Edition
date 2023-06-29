// c_convert.c -- Color Converter RGB2HSV & HSV2RGB

#include "doomdef.h"

// LightGetHSV
// Return HSV values based on given RGB
// Integer approximation.
// Modified to return hsv packed in a single int instead of through outvalue pointers.
int LightGetHSV(int r, int g, int b)
{
    // see https://stackoverflow.com/a/14733008
    u8 min, max;
    u8 h, s, v;

    min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    max = r > g ? (r > b ? r : b) : (g > b ? g : b);

    v = max;
    if (v == 0)
    {
        return 0;
    }

    s = (255 * (long)(max - min)) / v;
    if (s == 0)
    {
        return v;
    }

    if (max == r)
    {
        h = 0 + 43 * (g - b) / (max - min);
    }
    else if (max == g)
    {
        h = 85 + 43 * (b - r) / (max - min);
    }
    else
    {
        h = 171 + 43 * (r - g) / (max - min);
    }

    return ((h << 16) | (s << 8) | v);
}

// LightGetRGB
// Return RGB values based on given HSV
// Integer approximation.
// Modified to return hsv packed in a single int instead of through outvalue pointers.
int LightGetRGB(int h, int s, int v) // 8000248C
{
    // see https://stackoverflow.com/a/14733008
    u8 r, g, b;
    u8 region, remainder, p, q, t;

    if (s == 0)
    {
        return (v << 16) | (v << 8) | v;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
        {
            r = v;
            g = t;
            b = p;
            break;
        }
        case 1:
        {
            r = q;
            g = v;
            b = p;
            break;
        }
        case 2:
        {
            r = p;
            g = v;
            b = t;
            break;
        }
        case 3:
        {
            r = p;
            g = q;
            b = v;
            break;
        }
        case 4:
        {
            r = t;
            g = p;
            b = v;
            break;
        }
        default:
        {
            r = v;
            g = p;
            b = q;
            break;
        }
    }

    return ((r << 16) | (g << 8) | b);
}
