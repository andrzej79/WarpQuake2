/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_rgb.c -- WarpQuake2: the colour tables of the 16 bpp renderer
//
// See r_local.h for the scheme.  Two tables turn palette indexes into
// RGB565 pixels:
//
// - vid.colormap16, the lighting: VID_GRADES rows of 256, row l the palette
//   at brightness 2 - 2l/63.  That is the formula id's colormap.pcx was made
//   with (91.6% of its entries are the nearest palette colour to it; the
//   rest differ where the brightened colour clips).  The 8-bit table rounds
//   every lit colour to the nearest of the 256, which is where the banding
//   and the hue shifts of 8-bit Quake II come from; this one keeps it.  The
//   colours colormap.pcx leaves the same in every row stay so here.
// - d_8to16table, the palette itself, for what is not lit: sky, water,
//   sprites, particles, beams and the 2D layer.
//
// Both are built from sw_state.currentpalette, which has the gamma in it.
// The palette blend (damage, bonus, under water, powerups: r_newrefdef.blend)
// is not built in, as the 8-bit screen gets it for free from its CLUT
// every frame; the platform blends the frame as it shows it.

#include "r_local.h"

int				r_pixbytes = 1;
qboolean		d_rgbtest;
unsigned short	d_8to16table[256];
unsigned		r_rgbkey = 255;
int				d_rgbgeneration = 1;

static unsigned short	colormap16[VID_GRADES*256];
static byte				builtpal[768];
static qboolean			built;

void SWimp_SetBlend (int r, int g, int b, int alpha);	// alpha 0..256


/*
================
Pack

An RGB colour (0..255 each) as an RGB565 pixel, each field rounded.
================
*/
static unsigned short Pack (int r, int g, int b)
{
	return (unsigned short)((((r * 31 + 127) / 255) << 11) | (((g * 63 + 127) / 255) << 5) | ((b * 31 + 127) / 255));
}


/*
================
R_RGBMix

a * wa/32 + b * (32 - wa)/32, per field: both spread over a longword so
that each field has 5 free bits above it (green in the upper word), one
multiply each, and folded back.
================
*/
unsigned short R_RGBMix (unsigned a, unsigned b, unsigned wa)
{
	unsigned	x;

	a = (a | (a << 16)) & 0x07E0F81F;
	b = (b | (b << 16)) & 0x07E0F81F;
	x = ((a * wa + b * (32 - wa)) >> 5) & 0x07E0F81F;
	return (unsigned short)(x | (x >> 16));
}


/*
================
PickKey

A pixel no table entry holds, for the transparent texels of 16-bit
sprites: the lit colours come from colormap16 only, so no surface-cache
pixel can be mistaken for it either.  Near magenta, the usual key.
================
*/
static void PickKey (void)
{
	static byte	used[65536 / 8];
	unsigned	k;
	int			i;

	memset (used, 0, sizeof(used));
	for (i = 0 ; i < 256 ; i++)
		used[d_8to16table[i] >> 3] |= 1 << (d_8to16table[i] & 7);
	for (i = 0 ; i < VID_GRADES*256 ; i++)
		used[colormap16[i] >> 3] |= 1 << (colormap16[i] & 7);
	for (k = 0xF81F ; k < 0x10000 + 0xF81F ; k++)
		if (!(used[(k & 0xFFFF) >> 3] & (1 << (k & 7))))
			break;
	r_rgbkey = k & 0xFFFF;
}


/*
================
R_RGB_Build

Both tables, from a palette of R, G, B, x entries.
================
*/
static void R_RGB_Build (const byte *pal)
{
	int				l, c, ch, v;
	int				lit[3];
	unsigned		scale;
	unsigned short	*dest;

	if (d_rgbtest)
	{
		for (c = 0 ; c < 256 ; c++)
			d_8to16table[c] = c;
		for (c = 0 ; c < VID_GRADES*256 ; c++)
			colormap16[c] = vid.colormap[c];
		r_rgbkey = 255;
		return;
	}

	for (c = 0 ; c < 256 ; c++)
		d_8to16table[c] = Pack (pal[c*4], pal[c*4+1], pal[c*4+2]);

	dest = colormap16;
	for (l = 0 ; l < VID_GRADES ; l++)
	{
		// brightness 2 - 2l/63 as 16.16, applied with rounding
		scale = ((2 * (VID_GRADES - 1 - l)) << 16) / (VID_GRADES - 1);
		for (c = 0 ; c < 256 ; c++, dest++)
		{
			// what colormap.pcx does not light (fullbright)
			if (vid.colormap[c] == c && vid.colormap[(VID_GRADES-1)*256 + c] == c)
			{
				*dest = d_8to16table[c];
				continue;
			}
			for (ch = 0 ; ch < 3 ; ch++)
			{
				v = (pal[c*4+ch] * scale + 0x8000) >> 16;
				lit[ch] = v > 255 ? 255 : v;
			}
			*dest = Pack (lit[0], lit[1], lit[2]);
		}
	}
	PickKey ();
}


/*
================
R_RGB_ModeChanged

After SWimp_SetMode: r_pixbytes is the new mode's.  The tables are built
on the next R_RGB_SetPalette.
================
*/
void R_RGB_ModeChanged (void)
{
	extern int	COM_CheckParm (char *parm);	// qcommon: the renderer is linked in

	vid.colormap16 = colormap16;
	d_rgbtest = (r_pixbytes == 2 && COM_CheckParm ("-rgbtest"));
	built = false;
	d_rgbgeneration++;
	SWimp_SetBlend (0, 0, 0, 0);
}


/*
================
R_RGB_SetPalette

The palette R_GammaCorrectAndSetPalette made, on a 16-bit screen.  The
tables are rebuilt only when it changed (the gamma, a cinematic); then
the surface cache and the 16-bit textures are in the old colours.
================
*/
void R_RGB_SetPalette (const byte *palette)
{
	int		i;
	byte	pal[768];

	for (i = 0 ; i < 256 ; i++)
	{
		pal[i*3+0] = palette[i*4+0];
		pal[i*3+1] = palette[i*4+1];
		pal[i*3+2] = palette[i*4+2];
	}
	if (built && !memcmp (pal, builtpal, sizeof(pal)))
		return;
	memcpy (builtpal, pal, sizeof(pal));
	R_RGB_Build (palette);
	built = true;
	d_rgbgeneration++;
	D_FlushCaches ();
}


/*
================
R_RGB_SetBlend

R_CalcPalette's work on a 16-bit screen: the palette is blended toward
the colour by alpha, after the gamma (the 8-bit palette gets the gamma
after the blend; the same at the default gamma of 1).
================
*/
void R_RGB_SetBlend (const float *blend)
{
	float	alpha = blend[3];
	int		c[3], i;

	if (alpha <= 0 || d_rgbtest)
	{
		SWimp_SetBlend (0, 0, 0, 0);
		return;
	}
	if (alpha > 1)
		alpha = 1;
	for (i = 0 ; i < 3 ; i++)
	{
		c[i] = (int)(blend[i] * 255);
		c[i] = sw_state.gammatable[c[i] < 0 ? 0 : (c[i] > 255 ? 255 : c[i])];
	}
	SWimp_SetBlend (c[0], c[1], c[2], (int)(alpha * 256 + 0.5f));
}


/*
================
R_Pixels16

An image's mip 0 as pixels, made when first asked for after the tables
change.  A sprite's texel 255 becomes r_rgbkey, its transparent pixel;
in the others (sky, warping water) it is a colour.
================
*/
unsigned short *R_Pixels16 (image_t *image)
{
	int		i, n;
	byte	*src;

	if (image->pixels16 && image->pixels16gen == d_rgbgeneration)
		return image->pixels16;
	n = image->width * image->height;
	if (!image->pixels16)
	{
		image->pixels16 = malloc (n * 2);
		if (!image->pixels16)
			ri.Sys_Error (ERR_FATAL, "R_Pixels16: out of memory");
	}
	src = image->pixels[0];
	for (i = 0 ; i < n ; i++)
		image->pixels16[i] = d_8to16table[src[i]];
	if (image->type == it_sprite)
		for (i = 0 ; i < n ; i++)
			if (src[i] == 255)
				image->pixels16[i] = r_rgbkey;
	image->pixels16gen = d_rgbgeneration;
	return image->pixels16;
}
