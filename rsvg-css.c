/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 ts=4 expandtab: */
/* 
   rsvg-css.c: Parse CSS basic data types.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
  
   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Authors: Dom Lachowicz <cinamod@hotmail.com> 
   Raph Levien <raph@artofcode.com>
*/

#include "config.h"
#include "rsvg-css.h"
#include "rsvg-private.h"
#include "rsvg-styles.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <errno.h>
#include <math.h>

#define POINTS_PER_INCH (72.0)
#define CM_PER_INCH     (2.54)
#define MM_PER_INCH     (25.4)
#define PICA_PER_INCH   (6.0)

#define SETINHERIT() G_STMT_START {if (inherit != NULL) *inherit = TRUE;} G_STMT_END
#define UNSETINHERIT() G_STMT_START {if (inherit != NULL) *inherit = FALSE;} G_STMT_END

/**
 * rsvg_css_parse_vbox
 * @vbox: The CSS viewBox
 * @x : The X output
 * @y: The Y output
 * @w: The Width output
 * @h: The Height output
 *
 * Returns: 
 */
RsvgViewBox
rsvg_css_parse_vbox (const char *vbox)
{
    RsvgViewBox vb;
    gdouble *list;
    guint list_len;
    vb.active = FALSE;

    vb.x = vb.y = 0;
    vb.w = vb.h = 0;

    list = rsvg_css_parse_number_list (vbox, &list_len);

    if (!(list && list_len))
        return vb;
    else if (list_len != 4) {
        g_free (list);
        return vb;
    } else {
        vb.x = list[0];
        vb.y = list[1];
        vb.w = list[2];
        vb.h = list[3];
        vb.active = TRUE;

        g_free (list);
        return vb;
    }
}

typedef enum _RelativeSize {
    RELATIVE_SIZE_NORMAL,
    RELATIVE_SIZE_SMALLER,
    RELATIVE_SIZE_LARGER
} RelativeSize;

static double
rsvg_css_parse_raw_length (const char *str, gboolean * in,
                           gboolean * percent, gboolean * em, gboolean * ex, RelativeSize * relative_size)
{
    double length = 0.0;
    char *p = NULL;

    /* 
     *  The supported CSS length unit specifiers are: 
     *  em, ex, px, pt, pc, cm, mm, in, and %
     */
    *percent = FALSE;
    *em = FALSE;
    *ex = FALSE;
    *relative_size = RELATIVE_SIZE_NORMAL;

    length = g_ascii_strtod (str, &p);

    if ((length == -HUGE_VAL || length == HUGE_VAL) && (ERANGE == errno)) {
        /* todo: error condition - figure out how to best represent it */
        return 0.0;
    }

    /* test for either pixels or no unit, which is assumed to be pixels */
    if (p && *p && (strcmp (p, "px") != 0)) {
        if (!strcmp (p, "pt")) {
            length /= POINTS_PER_INCH;
            *in = TRUE;
        } else if (!strcmp (p, "in"))
            *in = TRUE;
        else if (!strcmp (p, "cm")) {
            length /= CM_PER_INCH;
            *in = TRUE;
        } else if (!strcmp (p, "mm")) {
            length /= MM_PER_INCH;
            *in = TRUE;
        } else if (!strcmp (p, "pc")) {
            length /= PICA_PER_INCH;
            *in = TRUE;
        } else if (!strcmp (p, "em"))
            *em = TRUE;
        else if (!strcmp (p, "ex"))
            *ex = TRUE;
        else if (!strcmp (p, "%")) {
            *percent = TRUE;
            length *= 0.01;
        } else {
            double pow_factor = 0.0;

            if (!g_ascii_strcasecmp (p, "larger")) {
                *relative_size = RELATIVE_SIZE_LARGER;
                return 0.0;
            } else if (!g_ascii_strcasecmp (p, "smaller")) {
                *relative_size = RELATIVE_SIZE_SMALLER;
                return 0.0;
            } else if (!g_ascii_strcasecmp (p, "xx-small")) {
                pow_factor = -3.0;
            } else if (!g_ascii_strcasecmp (p, "x-small")) {
                pow_factor = -2.0;
            } else if (!g_ascii_strcasecmp (p, "small")) {
                pow_factor = -1.0;
            } else if (!g_ascii_strcasecmp (p, "medium")) {
                pow_factor = 0.0;
            } else if (!g_ascii_strcasecmp (p, "large")) {
                pow_factor = 1.0;
            } else if (!g_ascii_strcasecmp (p, "x-large")) {
                pow_factor = 2.0;
            } else if (!g_ascii_strcasecmp (p, "xx-large")) {
                pow_factor = 3.0;
            } else {
                return 0.0;
            }

            length = 12.0 * pow (1.2, pow_factor) / POINTS_PER_INCH;
            *in = TRUE;
        }
    }

    return length;
}

RsvgLength
_rsvg_css_parse_length (const char *str)
{
    RsvgLength out;
    gboolean percent, em, ex, in;
    RelativeSize relative_size = RELATIVE_SIZE_NORMAL;
    percent = em = ex = in = FALSE;

    out.length = rsvg_css_parse_raw_length (str, &in, &percent, &em, &ex, &relative_size);
    if (percent)
        out.factor = 'p';
    else if (em)
        out.factor = 'm';
    else if (ex)
        out.factor = 'x';
    else if (in)
        out.factor = 'i';
    else if (relative_size == RELATIVE_SIZE_LARGER)
        out.factor = 'l';
    else if (relative_size == RELATIVE_SIZE_SMALLER)
        out.factor = 's';
    else
        out.factor = '\0';
    return out;
}

double
_rsvg_css_normalize_font_size (RsvgState * state, RsvgDrawingCtx * ctx)
{
    RsvgState *parent;

    switch (state->font_size.factor) {
    case 'p':
    case 'm':
    case 'x':
        parent= rsvg_state_parent (state);
        if (parent) {
            double parent_size;
            parent_size = _rsvg_css_normalize_font_size (parent, ctx);
            return state->font_size.length * parent_size;
        }
        break;
    default:
        return _rsvg_css_normalize_length (&state->font_size, ctx, 'v');
        break;
    }

    return 12.;
}

double
_rsvg_css_normalize_length (const RsvgLength * in, RsvgDrawingCtx * ctx, char dir)
{
    if (in->factor == '\0')
        return in->length;
    else if (in->factor == 'p') {
        if (dir == 'h')
            return in->length * ctx->vb.w;
        if (dir == 'v')
            return in->length * ctx->vb.h;
        if (dir == 'o')
            return in->length * rsvg_viewport_percentage (ctx->vb.w, ctx->vb.h);
    } else if (in->factor == 'm' || in->factor == 'x') {
        double font = _rsvg_css_normalize_font_size (rsvg_current_state (ctx), ctx);
        if (in->factor == 'm')
            return in->length * font;
        else
            return in->length * font / 2.;
    } else if (in->factor == 'i') {
        if (dir == 'h')
            return in->length * ctx->dpi_x;
        if (dir == 'v')
            return in->length * ctx->dpi_y;
        if (dir == 'o')
            return in->length * rsvg_viewport_percentage (ctx->dpi_x, ctx->dpi_y);
    } else if (in->factor == 'l') {
        /* todo: "larger" */
    } else if (in->factor == 's') {
        /* todo: "smaller" */
    }

    return 0;
}

double
_rsvg_css_hand_normalize_length (const RsvgLength * in, gdouble pixels_per_inch,
                                 gdouble width_or_height, gdouble font_size)
{
    if (in->factor == '\0')
        return in->length;
    else if (in->factor == 'p')
        return in->length * width_or_height;
    else if (in->factor == 'm')
        return in->length * font_size;
    else if (in->factor == 'x')
        return in->length * font_size / 2.;
    else if (in->factor == 'i')
        return in->length * pixels_per_inch;

    return 0;
}

static gint
rsvg_css_clip_rgb_percent (gdouble in_percent)
{
    /* spec says to clip these values */
    if (in_percent > 100.)
        return 255;
    else if (in_percent <= 0.)
        return 0;
    return (gint) floor (255. * in_percent / 100. + 0.5);
}

static gint
rsvg_css_clip_rgb (gint rgb)
{
    /* spec says to clip these values */
    if (rgb > 255)
        return 255;
    else if (rgb < 0)
        return 0;
    return rgb;
}

typedef struct {
    const char *const name;
    guint rgb;
} ColorPair;

/* compare function callback for bsearch */
static int
rsvg_css_color_compare (const void *a, const void *b)
{
    const char *needle = (const char *) a;
    const ColorPair *haystack = (const ColorPair *) b;

    return g_ascii_strcasecmp (needle, haystack->name);
}

/* pack 3 [0,255] ints into one 32 bit one */
#define PACK_RGB(r,g,b) (((r) << 16) | ((g) << 8) | (b))

/**
 * Parse a CSS2 color specifier, return RGB value
 */
guint32
rsvg_css_parse_color (const char *str, gboolean * inherit)
{
    gint val = 0;

    SETINHERIT ();

    if (str[0] == '#') {
        int i;
        for (i = 1; str[i]; i++) {
            int hexval;
            if (str[i] >= '0' && str[i] <= '9')
                hexval = str[i] - '0';
            else if (str[i] >= 'A' && str[i] <= 'F')
                hexval = str[i] - 'A' + 10;
            else if (str[i] >= 'a' && str[i] <= 'f')
                hexval = str[i] - 'a' + 10;
            else
                break;
            val = (val << 4) + hexval;
        }
        /* handle #rgb case */
        if (i == 4) {
            val = ((val & 0xf00) << 8) | ((val & 0x0f0) << 4) | (val & 0x00f);
            val |= val << 4;
        }
    }
    /* i want to use g_str_has_prefix but it isn't in my gstrfuncs.h?? */
    else if (strstr (str, "rgb") != NULL) {
        gint r, g, b;
        r = g = b = 0;

        if (strstr (str, "%") != 0) {
            guint i, nb_toks;
            char **toks;

            /* assume rgb (9%, 100%, 23%) */
            for (i = 0; str[i] != '('; i++);

            i++;

            toks = rsvg_css_parse_list (str + i, &nb_toks);

            if (toks) {
                if (nb_toks == 3) {
                    r = rsvg_css_clip_rgb_percent (g_ascii_strtod (toks[0], NULL));
                    g = rsvg_css_clip_rgb_percent (g_ascii_strtod (toks[1], NULL));
                    b = rsvg_css_clip_rgb_percent (g_ascii_strtod (toks[2], NULL));
                }

                g_strfreev (toks);
            }
        } else {
            /* assume "rgb (r, g, b)" */
            if (3 == sscanf (str, " rgb ( %d , %d , %d ) ", &r, &g, &b)) {
                r = rsvg_css_clip_rgb (r);
                g = rsvg_css_clip_rgb (g);
                b = rsvg_css_clip_rgb (b);
            } else
                r = g = b = 0;
        }

        val = PACK_RGB (r, g, b);
    } else if (!strcmp (str, "inherit"))
        UNSETINHERIT ();
    else {
        static const ColorPair color_list[] = {
            {"aliceblue", PACK_RGB (240, 248, 255)},
            {"antiquewhite", PACK_RGB (250, 235, 215)},
            {"aqua", PACK_RGB (0, 255, 255)},
            {"aquamarine", PACK_RGB (127, 255, 212)},
            {"azure", PACK_RGB (240, 255, 255)},
            {"beige", PACK_RGB (245, 245, 220)},
            {"bisque", PACK_RGB (255, 228, 196)},
            {"black", PACK_RGB (0, 0, 0)},
            {"blanchedalmond", PACK_RGB (255, 235, 205)},
            {"blue", PACK_RGB (0, 0, 255)},
            {"blueviolet", PACK_RGB (138, 43, 226)},
            {"brown", PACK_RGB (165, 42, 42)},
            {"burlywood", PACK_RGB (222, 184, 135)},
            {"cadetblue", PACK_RGB (95, 158, 160)},
            {"chartreuse", PACK_RGB (127, 255, 0)},
            {"chocolate", PACK_RGB (210, 105, 30)},
            {"coral", PACK_RGB (255, 127, 80)},
            {"cornflowerblue", PACK_RGB (100, 149, 237)},
            {"cornsilk", PACK_RGB (255, 248, 220)},
            {"crimson", PACK_RGB (220, 20, 60)},
            {"cyan", PACK_RGB (0, 255, 255)},
            {"darkblue", PACK_RGB (0, 0, 139)},
            {"darkcyan", PACK_RGB (0, 139, 139)},
            {"darkgoldenrod", PACK_RGB (184, 132, 11)},
            {"darkgray", PACK_RGB (169, 169, 169)},
            {"darkgreen", PACK_RGB (0, 100, 0)},
            {"darkgrey", PACK_RGB (169, 169, 169)},
            {"darkkhaki", PACK_RGB (189, 183, 107)},
            {"darkmagenta", PACK_RGB (139, 0, 139)},
            {"darkolivegreen", PACK_RGB (85, 107, 47)},
            {"darkorange", PACK_RGB (255, 140, 0)},
            {"darkorchid", PACK_RGB (153, 50, 204)},
            {"darkred", PACK_RGB (139, 0, 0)},
            {"darksalmon", PACK_RGB (233, 150, 122)},
            {"darkseagreen", PACK_RGB (143, 188, 143)},
            {"darkslateblue", PACK_RGB (72, 61, 139)},
            {"darkslategray", PACK_RGB (47, 79, 79)},
            {"darkslategrey", PACK_RGB (47, 79, 79)},
            {"darkturquoise", PACK_RGB (0, 206, 209)},
            {"darkviolet", PACK_RGB (148, 0, 211)},
            {"deeppink", PACK_RGB (255, 20, 147)},
            {"deepskyblue", PACK_RGB (0, 191, 255)},
            {"dimgray", PACK_RGB (105, 105, 105)},
            {"dimgrey", PACK_RGB (105, 105, 105)},
            {"dodgerblue", PACK_RGB (30, 144, 255)},
            {"firebrick", PACK_RGB (178, 34, 34)},
            {"floralwhite", PACK_RGB (255, 255, 240)},
            {"forestgreen", PACK_RGB (34, 139, 34)},
            {"fuchsia", PACK_RGB (255, 0, 255)},
            {"gainsboro", PACK_RGB (220, 220, 220)},
            {"ghostwhite", PACK_RGB (248, 248, 255)},
            {"gold", PACK_RGB (255, 215, 0)},
            {"goldenrod", PACK_RGB (218, 165, 32)},
            {"gray", PACK_RGB (128, 128, 128)},
            {"green", PACK_RGB (0, 128, 0)},
            {"greenyellow", PACK_RGB (173, 255, 47)},
            {"grey", PACK_RGB (128, 128, 128)},
            {"honeydew", PACK_RGB (240, 255, 240)},
            {"hotpink", PACK_RGB (255, 105, 180)},
            {"indianred", PACK_RGB (205, 92, 92)},
            {"indigo", PACK_RGB (75, 0, 130)},
            {"ivory", PACK_RGB (255, 255, 240)},
            {"khaki", PACK_RGB (240, 230, 140)},
            {"lavender", PACK_RGB (230, 230, 250)},
            {"lavenderblush", PACK_RGB (255, 240, 245)},
            {"lawngreen", PACK_RGB (124, 252, 0)},
            {"lemonchiffon", PACK_RGB (255, 250, 205)},
            {"lightblue", PACK_RGB (173, 216, 230)},
            {"lightcoral", PACK_RGB (240, 128, 128)},
            {"lightcyan", PACK_RGB (224, 255, 255)},
            {"lightgoldenrodyellow", PACK_RGB (250, 250, 210)},
            {"lightgray", PACK_RGB (211, 211, 211)},
            {"lightgreen", PACK_RGB (144, 238, 144)},
            {"lightgrey", PACK_RGB (211, 211, 211)},
            {"lightpink", PACK_RGB (255, 182, 193)},
            {"lightsalmon", PACK_RGB (255, 160, 122)},
            {"lightseagreen", PACK_RGB (32, 178, 170)},
            {"lightskyblue", PACK_RGB (135, 206, 250)},
            {"lightslategray", PACK_RGB (119, 136, 153)},
            {"lightslategrey", PACK_RGB (119, 136, 153)},
            {"lightsteelblue", PACK_RGB (176, 196, 222)},
            {"lightyellow", PACK_RGB (255, 255, 224)},
            {"lime", PACK_RGB (0, 255, 0)},
            {"limegreen", PACK_RGB (50, 205, 50)},
            {"linen", PACK_RGB (250, 240, 230)},
            {"magenta", PACK_RGB (255, 0, 255)},
            {"maroon", PACK_RGB (128, 0, 0)},
            {"mediumaquamarine", PACK_RGB (102, 205, 170)},
            {"mediumblue", PACK_RGB (0, 0, 205)},
            {"mediumorchid", PACK_RGB (186, 85, 211)},
            {"mediumpurple", PACK_RGB (147, 112, 219)},
            {"mediumseagreen", PACK_RGB (60, 179, 113)},
            {"mediumslateblue", PACK_RGB (123, 104, 238)},
            {"mediumspringgreen", PACK_RGB (0, 250, 154)},
            {"mediumturquoise", PACK_RGB (72, 209, 204)},
            {"mediumvioletred", PACK_RGB (199, 21, 133)},
            {"midnightblue", PACK_RGB (25, 25, 112)},
            {"mintcream", PACK_RGB (245, 255, 250)},
            {"mistyrose", PACK_RGB (255, 228, 225)},
            {"moccasin", PACK_RGB (255, 228, 181)},
            {"navajowhite", PACK_RGB (255, 222, 173)},
            {"navy", PACK_RGB (0, 0, 128)},
            {"oldlace", PACK_RGB (253, 245, 230)},
            {"olive", PACK_RGB (128, 128, 0)},
            {"olivedrab", PACK_RGB (107, 142, 35)},
            {"orange", PACK_RGB (255, 165, 0)},
            {"orangered", PACK_RGB (255, 69, 0)},
            {"orchid", PACK_RGB (218, 112, 214)},
            {"palegoldenrod", PACK_RGB (238, 232, 170)},
            {"palegreen", PACK_RGB (152, 251, 152)},
            {"paleturquoise", PACK_RGB (175, 238, 238)},
            {"palevioletred", PACK_RGB (219, 112, 147)},
            {"papayawhip", PACK_RGB (255, 239, 213)},
            {"peachpuff", PACK_RGB (255, 218, 185)},
            {"peru", PACK_RGB (205, 133, 63)},
            {"pink", PACK_RGB (255, 192, 203)},
            {"plum", PACK_RGB (221, 160, 203)},
            {"powderblue", PACK_RGB (176, 224, 230)},
            {"purple", PACK_RGB (128, 0, 128)},
            {"red", PACK_RGB (255, 0, 0)},
            {"rosybrown", PACK_RGB (188, 143, 143)},
            {"royalblue", PACK_RGB (65, 105, 225)},
            {"saddlebrown", PACK_RGB (139, 69, 19)},
            {"salmon", PACK_RGB (250, 128, 114)},
            {"sandybrown", PACK_RGB (244, 164, 96)},
            {"seagreen", PACK_RGB (46, 139, 87)},
            {"seashell", PACK_RGB (255, 245, 238)},
            {"sienna", PACK_RGB (160, 82, 45)},
            {"silver", PACK_RGB (192, 192, 192)},
            {"skyblue", PACK_RGB (135, 206, 235)},
            {"slateblue", PACK_RGB (106, 90, 205)},
            {"slategray", PACK_RGB (119, 128, 144)},
            {"slategrey", PACK_RGB (119, 128, 144)},
            {"snow", PACK_RGB (255, 255, 250)},
            {"springgreen", PACK_RGB (0, 255, 127)},
            {"steelblue", PACK_RGB (70, 130, 180)},
            {"tan", PACK_RGB (210, 180, 140)},
            {"teal", PACK_RGB (0, 128, 128)},
            {"thistle", PACK_RGB (216, 191, 216)},
            {"tomato", PACK_RGB (255, 99, 71)},
            {"turquoise", PACK_RGB (64, 224, 208)},
            {"violet", PACK_RGB (238, 130, 238)},
            {"wheat", PACK_RGB (245, 222, 179)},
            {"white", PACK_RGB (255, 255, 255)},
            {"whitesmoke", PACK_RGB (245, 245, 245)},
            {"yellow", PACK_RGB (255, 255, 0)},
            {"yellowgreen", PACK_RGB (154, 205, 50)}
        };

        ColorPair *result = bsearch (str, color_list,
                                     sizeof (color_list) / sizeof (color_list[0]),
                                     sizeof (ColorPair),
                                     rsvg_css_color_compare);

        /* default to black on failed lookup */
        if (result == NULL) {
            UNSETINHERIT ();
            val = 0;
        } else
            val = result->rgb;
    }

    return val;
}

#undef PACK_RGB

guint
rsvg_css_parse_opacity (const char *str)
{
    char *end_ptr;
    double opacity;

    opacity = g_ascii_strtod (str, &end_ptr);

    if (end_ptr && end_ptr[0] == '%')
        opacity *= 0.01;

    return (guint) floor (opacity * 255. + 0.5);
}

/*
  <angle>: An angle value is a <number>  optionally followed immediately with 
  an angle unit identifier. Angle unit identifiers are:

    * deg: degrees
    * grad: grads
    * rad: radians

    For properties defined in [CSS2], an angle unit identifier must be provided.
    For SVG-specific attributes and properties, the angle unit identifier is 
    optional. If not provided, the angle value is assumed to be in degrees.
*/
double
rsvg_css_parse_angle (const char *str)
{
    double degrees;
    char *end_ptr;

    degrees = g_ascii_strtod (str, &end_ptr);

    /* todo: error condition - figure out how to best represent it */
    if ((degrees == -HUGE_VAL || degrees == HUGE_VAL) && (ERANGE == errno))
        return 0.0;

    if (end_ptr) {
        if (!strcmp (end_ptr, "rad"))
            return degrees * 180. / G_PI;
        else if (!strcmp (end_ptr, "grad"))
            return degrees * 360. / 400.;
    }

    return degrees;
}

/*
  <frequency>: Frequency values are used with aural properties. The normative 
  definition of frequency values can be found in [CSS2-AURAL]. A frequency 
  value is a <number> immediately followed by a frequency unit identifier. 
  Frequency unit identifiers are:

    * Hz: Hertz
    * kHz: kilo Hertz

    Frequency values may not be negative.
*/
double
rsvg_css_parse_frequency (const char *str)
{
    double f_hz;
    char *end_ptr;

    f_hz = g_ascii_strtod (str, &end_ptr);

    /* todo: error condition - figure out how to best represent it */
    if ((f_hz == -HUGE_VAL || f_hz == HUGE_VAL) && (ERANGE == errno))
        return 0.0;

    if (end_ptr && !strcmp (end_ptr, "kHz"))
        return f_hz * 1000.;

    return f_hz;
}

/*
  <time>: A time value is a <number> immediately followed by a time unit 
  identifier. Time unit identifiers are:
  
  * ms: milliseconds
  * s: seconds
  
  Time values are used in CSS properties and may not be negative.
*/
double
rsvg_css_parse_time (const char *str)
{
    double ms;
    char *end_ptr;

    ms = g_ascii_strtod (str, &end_ptr);

    /* todo: error condition - figure out how to best represent it */
    if ((ms == -HUGE_VAL || ms == HUGE_VAL) && (ERANGE == errno))
        return 0.0;

    if (end_ptr && !strcmp (end_ptr, "s"))
        return ms * 1000.;

    return ms;
}

PangoStyle
rsvg_css_parse_font_style (const char *str, gboolean * inherit)
{
    SETINHERIT ();

    if (str) {
        if (!strcmp (str, "oblique"))
            return PANGO_STYLE_OBLIQUE;
        if (!strcmp (str, "italic"))
            return PANGO_STYLE_ITALIC;
        else if (!strcmp (str, "inherit")) {
            UNSETINHERIT ();
            return PANGO_STYLE_NORMAL;
        }
    }
    UNSETINHERIT ();
    return PANGO_STYLE_NORMAL;
}

PangoVariant
rsvg_css_parse_font_variant (const char *str, gboolean * inherit)
{
    SETINHERIT ();

    if (str) {
        if (!strcmp (str, "small-caps"))
            return PANGO_VARIANT_SMALL_CAPS;
        else if (!strcmp (str, "inherit")) {
            UNSETINHERIT ();
            return PANGO_VARIANT_NORMAL;
        }
    }
    UNSETINHERIT ();
    return PANGO_VARIANT_NORMAL;
}

PangoWeight
rsvg_css_parse_font_weight (const char *str, gboolean * inherit)
{
    SETINHERIT ();
    if (str) {
        if (!strcmp (str, "lighter"))
            return PANGO_WEIGHT_LIGHT;
        else if (!strcmp (str, "bold"))
            return PANGO_WEIGHT_BOLD;
        else if (!strcmp (str, "bolder"))
            return PANGO_WEIGHT_ULTRABOLD;
        else if (!strcmp (str, "100"))
            return (PangoWeight) 100;
        else if (!strcmp (str, "200"))
            return (PangoWeight) 200;
        else if (!strcmp (str, "300"))
            return (PangoWeight) 300;
        else if (!strcmp (str, "400"))
            return (PangoWeight) 400;
        else if (!strcmp (str, "500"))
            return (PangoWeight) 500;
        else if (!strcmp (str, "600"))
            return (PangoWeight) 600;
        else if (!strcmp (str, "700"))
            return (PangoWeight) 700;
        else if (!strcmp (str, "800"))
            return (PangoWeight) 800;
        else if (!strcmp (str, "900"))
            return (PangoWeight) 900;
        else if (!strcmp (str, "inherit")) {
            UNSETINHERIT ();
            return PANGO_WEIGHT_NORMAL;
        }
    }

    UNSETINHERIT ();
    return PANGO_WEIGHT_NORMAL;
}

PangoStretch
rsvg_css_parse_font_stretch (const char *str, gboolean * inherit)
{
    SETINHERIT ();

    if (str) {
        if (!strcmp (str, "ultra-condensed"))
            return PANGO_STRETCH_ULTRA_CONDENSED;
        else if (!strcmp (str, "extra-condensed"))
            return PANGO_STRETCH_EXTRA_CONDENSED;
        else if (!strcmp (str, "condensed") || !strcmp (str, "narrower"))       /* narrower not quite correct */
            return PANGO_STRETCH_CONDENSED;
        else if (!strcmp (str, "semi-condensed"))
            return PANGO_STRETCH_SEMI_CONDENSED;
        else if (!strcmp (str, "semi-expanded"))
            return PANGO_STRETCH_SEMI_EXPANDED;
        else if (!strcmp (str, "expanded") || !strcmp (str, "wider"))   /* wider not quite correct */
            return PANGO_STRETCH_EXPANDED;
        else if (!strcmp (str, "extra-expanded"))
            return PANGO_STRETCH_EXTRA_EXPANDED;
        else if (!strcmp (str, "ultra-expanded"))
            return PANGO_STRETCH_ULTRA_EXPANDED;
        else if (!strcmp (str, "inherit")) {
            UNSETINHERIT ();
            return PANGO_STRETCH_NORMAL;
        }
    }
    UNSETINHERIT ();
    return PANGO_STRETCH_NORMAL;
}

const char *
rsvg_css_parse_font_family (const char *str, gboolean * inherit)
{
    SETINHERIT ();

    if (!str)
        return NULL;
    else if (!strcmp (str, "inherit")) {
        UNSETINHERIT ();
        return NULL;
    } else
        return str;
}

#if !defined(HAVE_STRTOK_R)

static char *
strtok_r (char *s, const char *delim, char **last)
{
    char *p;

    if (s == NULL)
        s = *last;

    if (s == NULL)
        return NULL;

    while (*s && strchr (delim, *s))
        s++;

    if (*s == '\0') {
        *last = NULL;
        return NULL;
    }

    p = s;
    while (*p && !strchr (delim, *p))
        p++;

    if (*p == '\0')
        *last = NULL;
    else {
        *p = '\0';
        p++;
        *last = p;
    }

    return s;
}

#endif                          /* !HAVE_STRTOK_R */

gchar **
rsvg_css_parse_list (const char *in_str, guint * out_list_len)
{
    char *ptr, *tok;
    char *str;

    guint n = 0;
    GSList *string_list = NULL;
    gchar **string_array = NULL;

    str = g_strdup (in_str);
    tok = strtok_r (str, ", \t", &ptr);
    if (tok != NULL) {
        if (strcmp (tok, " ") != 0) {
            string_list = g_slist_prepend (string_list, g_strdup (tok));
            n++;
        }

        while ((tok = strtok_r (NULL, ", \t", &ptr)) != NULL) {
            if (strcmp (tok, " ") != 0) {
                string_list = g_slist_prepend (string_list, g_strdup (tok));
                n++;
            }
        }
    }
    g_free (str);

    if (out_list_len)
        *out_list_len = n;

    if (string_list) {
        GSList *slist;

        string_array = g_new (gchar *, n + 1);

        string_array[n--] = NULL;
        for (slist = string_list; slist; slist = slist->next)
            string_array[n--] = (gchar *) slist->data;

        g_slist_free (string_list);
    }

    return string_array;
}

gdouble *
rsvg_css_parse_number_list (const char *in_str, guint * out_list_len)
{
    gchar **string_array;
    gdouble *output;
    guint len, i;

    if (out_list_len)
        *out_list_len = 0;

    string_array = rsvg_css_parse_list (in_str, &len);

    if (!(string_array && len))
        return NULL;

    output = g_new (gdouble, len);

    /* TODO: some error checking */
    for (i = 0; i < len; i++)
        output[i] = g_ascii_strtod (string_array[i], NULL);

    g_strfreev (string_array);

    if (out_list_len != NULL)
        *out_list_len = len;

    return output;
}

void
rsvg_css_parse_number_optional_number (const char *str, double *x, double *y)
{
    char *endptr;

    /* TODO: some error checking */

    *x = g_ascii_strtod (str, &endptr);

    if (endptr && *endptr != '\0')
        while (g_ascii_isspace (*endptr) && *endptr)
            endptr++;

    if (endptr && *endptr)
        *y = g_ascii_strtod (endptr, NULL);
    else
        *y = *x;
}

int
rsvg_css_parse_aspect_ratio (const char *str)
{
    char **elems;
    guint nb_elems;

    int ratio = RSVG_ASPECT_RATIO_NONE;

    elems = rsvg_css_parse_list (str, &nb_elems);

    if (elems && nb_elems) {
        guint i;

        for (i = 0; i < nb_elems; i++) {
            if (!strcmp (elems[i], "xMinYMin"))
                ratio = RSVG_ASPECT_RATIO_XMIN_YMIN;
            else if (!strcmp (elems[i], "xMidYMin"))
                ratio = RSVG_ASPECT_RATIO_XMID_YMIN;
            else if (!strcmp (elems[i], "xMaxYMin"))
                ratio = RSVG_ASPECT_RATIO_XMAX_YMIN;
            else if (!strcmp (elems[i], "xMinYMid"))
                ratio = RSVG_ASPECT_RATIO_XMIN_YMID;
            else if (!strcmp (elems[i], "xMidYMid"))
                ratio = RSVG_ASPECT_RATIO_XMID_YMID;
            else if (!strcmp (elems[i], "xMaxYMid"))
                ratio = RSVG_ASPECT_RATIO_XMAX_YMID;
            else if (!strcmp (elems[i], "xMinYMax"))
                ratio = RSVG_ASPECT_RATIO_XMIN_YMAX;
            else if (!strcmp (elems[i], "xMidYMax"))
                ratio = RSVG_ASPECT_RATIO_XMID_YMAX;
            else if (!strcmp (elems[i], "xMaxYMax"))
                ratio = RSVG_ASPECT_RATIO_XMAX_YMAX;
            else if (!strcmp (elems[i], "slice"))
                ratio |= RSVG_ASPECT_RATIO_SLICE;
        }

        g_strfreev (elems);
    }

    return ratio;
}

gboolean
rsvg_css_parse_overflow (const char *str, gboolean * inherit)
{
    SETINHERIT ();
    if (!strcmp (str, "visible") || !strcmp (str, "auto"))
        return 1;
    if (!strcmp (str, "hidden") || !strcmp (str, "scroll"))
        return 0;
    UNSETINHERIT ();
    return 0;
}

/***********************************************************************/
/***********************************************************************/

/* 
   Code largely based on xmltok_impl.c from Expat

   Copyright (c) 1998, 1999, 2000 Thai Open Source Software Center Ltd
   and Clark Cooper
   Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006 Expat maintainers.
   
   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:
   
   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *valuePtr;
    const char *valueEnd;
    char normalized;
} ATTRIBUTE;

#define PTRCALL
#define PTRFASTCALL
#define PREFIX(x) x
typedef void ENCODING;

#define BYTE_TO_ASCII(enc, p) (*(p))

/* minimum bytes per character */
#define MINBPC(enc) 1

#define BYTE_TYPE(enc, p) utf8_byte_type_table[(int)(*(p))]

#define ASCII_SPACE 0x20

enum {
    BT_NONXML,
    BT_MALFORM,
    BT_LT,
    BT_AMP,
    BT_RSQB,
    BT_LEAD2,
    BT_LEAD3,
    BT_LEAD4,
    BT_TRAIL,
    BT_CR,
    BT_LF,
    BT_GT,
    BT_QUOT,
    BT_APOS,
    BT_EQUALS,
    BT_QUEST,
    BT_EXCL,
    BT_SOL,
    BT_SEMI,
    BT_NUM,
    BT_LSQB,
    BT_S,
    BT_NMSTRT,
    BT_COLON,
    BT_HEX,
    BT_DIGIT,
    BT_NAME,
    BT_MINUS,
    BT_OTHER,                   /* known not to be a name or name start character */
    BT_NONASCII,                /* might be a name or name start character */
    BT_PERCNT,
    BT_LPAR,
    BT_RPAR,
    BT_AST,
    BT_PLUS,
    BT_COMMA,
    BT_VERBAR
};

static const char utf8_byte_type_table[] = {
    /* 0x00 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
    /* 0x04 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
    /* 0x08 */ BT_NONXML, BT_S, BT_LF, BT_NONXML,
    /* 0x0C */ BT_NONXML, BT_CR, BT_NONXML, BT_NONXML,
    /* 0x10 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
    /* 0x14 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
    /* 0x18 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
    /* 0x1C */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
    /* 0x20 */ BT_S, BT_EXCL, BT_QUOT, BT_NUM,
    /* 0x24 */ BT_OTHER, BT_PERCNT, BT_AMP, BT_APOS,
    /* 0x28 */ BT_LPAR, BT_RPAR, BT_AST, BT_PLUS,
    /* 0x2C */ BT_COMMA, BT_MINUS, BT_NAME, BT_SOL,
    /* 0x30 */ BT_DIGIT, BT_DIGIT, BT_DIGIT, BT_DIGIT,
    /* 0x34 */ BT_DIGIT, BT_DIGIT, BT_DIGIT, BT_DIGIT,
    /* 0x38 */ BT_DIGIT, BT_DIGIT, BT_COLON, BT_SEMI,
    /* 0x3C */ BT_LT, BT_EQUALS, BT_GT, BT_QUEST,
    /* 0x40 */ BT_OTHER, BT_HEX, BT_HEX, BT_HEX,
    /* 0x44 */ BT_HEX, BT_HEX, BT_HEX, BT_NMSTRT,
    /* 0x48 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
    /* 0x4C */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
    /* 0x50 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
    /* 0x54 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
    /* 0x58 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_LSQB,
    /* 0x5C */ BT_OTHER, BT_RSQB, BT_OTHER, BT_NMSTRT,
    /* 0x60 */ BT_OTHER, BT_HEX, BT_HEX, BT_HEX,
    /* 0x64 */ BT_HEX, BT_HEX, BT_HEX, BT_NMSTRT,
    /* 0x68 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
    /* 0x6C */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
    /* 0x70 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
    /* 0x74 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
    /* 0x78 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_OTHER,
    /* 0x7C */ BT_VERBAR, BT_OTHER, BT_OTHER, BT_OTHER,
    /* 0x80 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0x84 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0x88 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0x8C */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0x90 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0x94 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0x98 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0x9C */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0xA0 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0xA4 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0xA8 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0xAC */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0xB0 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0xB4 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0xB8 */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0xBC */ BT_TRAIL, BT_TRAIL, BT_TRAIL, BT_TRAIL,
    /* 0xC0 */ BT_LEAD2, BT_LEAD2, BT_LEAD2, BT_LEAD2,
    /* 0xC4 */ BT_LEAD2, BT_LEAD2, BT_LEAD2, BT_LEAD2,
    /* 0xC8 */ BT_LEAD2, BT_LEAD2, BT_LEAD2, BT_LEAD2,
    /* 0xCC */ BT_LEAD2, BT_LEAD2, BT_LEAD2, BT_LEAD2,
    /* 0xD0 */ BT_LEAD2, BT_LEAD2, BT_LEAD2, BT_LEAD2,
    /* 0xD4 */ BT_LEAD2, BT_LEAD2, BT_LEAD2, BT_LEAD2,
    /* 0xD8 */ BT_LEAD2, BT_LEAD2, BT_LEAD2, BT_LEAD2,
    /* 0xDC */ BT_LEAD2, BT_LEAD2, BT_LEAD2, BT_LEAD2,
    /* 0xE0 */ BT_LEAD3, BT_LEAD3, BT_LEAD3, BT_LEAD3,
    /* 0xE4 */ BT_LEAD3, BT_LEAD3, BT_LEAD3, BT_LEAD3,
    /* 0xE8 */ BT_LEAD3, BT_LEAD3, BT_LEAD3, BT_LEAD3,
    /* 0xEC */ BT_LEAD3, BT_LEAD3, BT_LEAD3, BT_LEAD3,
    /* 0xF0 */ BT_LEAD4, BT_LEAD4, BT_LEAD4, BT_LEAD4,
    /* 0xF4 */ BT_LEAD4, BT_NONXML, BT_NONXML, BT_NONXML,
    /* 0xF8 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
    /* 0xFC */ BT_NONXML, BT_NONXML, BT_MALFORM, BT_MALFORM
};

/* This must only be called for a well-formed start-tag or empty
   element tag.  Returns the number of attributes.  Pointers to the
   first attsMax attributes are stored in atts.
*/

static int PTRCALL
PREFIX (getAtts) (const ENCODING * enc, const char *ptr, int attsMax, ATTRIBUTE * atts) {
    enum { other, inName, inValue } state = inName;
    int nAtts = 0;
    int open = 0;               /* defined when state == inValue;
                                   initialization just to shut up compilers */

    for (ptr += MINBPC (enc);; ptr += MINBPC (enc)) {
        switch (BYTE_TYPE (enc, ptr)) {

#define START_NAME \
      if (state == other) { \
        if (nAtts < attsMax) { \
          atts[nAtts].name = ptr; \
          atts[nAtts].normalized = 1; \
        } \
        state = inName; \
      }

#define LEAD_CASE(n) \
    case BT_LEAD ## n: START_NAME ptr += (n - MINBPC(enc)); break;
            LEAD_CASE (2) LEAD_CASE (3) LEAD_CASE (4)
#undef LEAD_CASE
        case BT_NONASCII:
        case BT_NMSTRT:
        case BT_HEX:
            START_NAME break;
#undef START_NAME
        case BT_QUOT:
            if (state != inValue) {
                if (nAtts < attsMax)
                    atts[nAtts].valuePtr = ptr + MINBPC (enc);
                state = inValue;
                open = BT_QUOT;
            } else if (open == BT_QUOT) {
                state = other;
                if (nAtts < attsMax)
                    atts[nAtts].valueEnd = ptr;
                nAtts++;
            }
            break;
        case BT_APOS:
            if (state != inValue) {
                if (nAtts < attsMax)
                    atts[nAtts].valuePtr = ptr + MINBPC (enc);
                state = inValue;
                open = BT_APOS;
            } else if (open == BT_APOS) {
                state = other;
                if (nAtts < attsMax)
                    atts[nAtts].valueEnd = ptr;
                nAtts++;
            }
            break;
        case BT_AMP:
            if (nAtts < attsMax)
                atts[nAtts].normalized = 0;
            break;
        case BT_S:
            if (state == inName)
                state = other;
            else if (state == inValue
                     && nAtts < attsMax
                     && atts[nAtts].normalized
                     && (ptr == atts[nAtts].valuePtr
                         || BYTE_TO_ASCII (enc, ptr) != ASCII_SPACE
                         || BYTE_TO_ASCII (enc, ptr + MINBPC (enc)) == ASCII_SPACE
                         || BYTE_TYPE (enc, ptr + MINBPC (enc)) == open))
                atts[nAtts].normalized = 0;
            break;
        case BT_CR:
        case BT_LF:
            /* This case ensures that the first attribute name is counted
               Apart from that we could just change state on the quote. */
            if (state == inName)
                state = other;
            else if (state == inValue && nAtts < attsMax)
                atts[nAtts].normalized = 0;
            break;
        case BT_GT:
        case BT_SOL:
            if (state != inValue)
                return nAtts;
            break;
        default:
            break;
        }
    }
    /* not reached */
}

static int PTRFASTCALL PREFIX (nameLength) (const ENCODING * enc, const char *ptr) {
    const char *start = ptr;
    for (;;) {
        switch (BYTE_TYPE (enc, ptr)) {
#define LEAD_CASE(n) \
    case BT_LEAD ## n: ptr += n; break;
            LEAD_CASE (2) LEAD_CASE (3) LEAD_CASE (4)
#undef LEAD_CASE
        case BT_NONASCII:
        case BT_NMSTRT:
#ifdef XML_NS
        case BT_COLON:
#endif
        case BT_HEX:
        case BT_DIGIT:
        case BT_NAME:
        case BT_MINUS:
            ptr += MINBPC (enc);
            break;
        default:
            return (int) (ptr - start);
        }
    }
}

char **
rsvg_css_parse_xml_attribute_string (const char *attribute_string)
{
    int i, nb_atts;
    ATTRIBUTE *attributes;
    int attrSize = 16;
    ENCODING *enc = NULL;
    char **atts;
    char *_attribute_string;

    _attribute_string = g_strdup_printf ("<tag %s />\n", attribute_string);
    attributes = g_new (ATTRIBUTE, attrSize);

    nb_atts = getAtts (enc, _attribute_string, attrSize, attributes);
    if (nb_atts > attrSize) {
        attrSize = nb_atts;
        g_free (attributes);

        attributes = g_new (ATTRIBUTE, attrSize);
        nb_atts = getAtts (enc, _attribute_string, attrSize, attributes);
    }

    atts = g_new0 (char *, ((2 * nb_atts) + 1));
    for (i = 0; i < nb_atts; i++) {
        atts[(2 * i)] = g_strdup (attributes[i].name);
        atts[(2 * i)][nameLength (enc, attributes[i].name)] = '\0';
        atts[(2 * i) + 1] = g_strdup (attributes[i].valuePtr);
        atts[(2 * i) + 1][attributes[i].valueEnd - attributes[i].valuePtr] = '\0';
    }

    g_free (attributes);
    g_free (_attribute_string);

    return atts;
}
