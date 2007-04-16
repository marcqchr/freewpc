/*
 * Copyright 2006, 2007 by Brian Dominy <brian@oddchange.com>
 *
 * This file is part of FreeWPC.
 *
 * FreeWPC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * FreeWPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with FreeWPC; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <freewpc.h>

/**
 * \file
 * \brief Routines for writing text to the DMD in a particular font.
 *
 * The font functions take strings, which can be true C constants or
 * a pointer to a buffer that is already formatted, and renders the
 * string at a particular location in a particular font.
 *
 * Coordinate values are given in pixels.
 *
 * Three variants exist, one for each justification: left, right, or
 * center.
 */


/* Space characters are not embedded in each font, because it would
 * just be a bunch of zero characters.  All fonts share the same
 * space glyph data, given here */
static U8 font_space[16] = { 0, };


/* Fonts define their own inter-character space width, but
this is not currently used... we always use a single pixel */
#if 1
#define GET_FONT_SPACING(font)	1
#else
#define GET_FONT_SPACING(font)	(font->spacing)
#endif


fontargs_t font_args;

/** The width of the current character being rendered in bits */
__fastram__ U8 font_width;

/** The width of the current character being rendered in bytes */
__fastram__ U8 font_byte_width;

/** The height of the current character being rendered */
__fastram__ U8 font_height;

/** The total width of the current string being rendered */
U8 font_string_width;

/** The overall height of the current string being rendered, which
 * is the maximum height of all its characters */
U8 font_string_height;


/* Returns a pointer to the glyph data for a character 'c'
 * in the font 'font'.  This points directly to the raw bytes
 * that can be ORed into the display.
 *
 * This function also sets some global variables related to
 * the characteristics of this character:
 * font_width : number of bits wide, including spacing
 * font_byte_width: number of bytes wide, excluding spacing
 * font_height: number of bits high.
 */
U8 *font_lookup (const font_t *font, char c)
{
	if (c == ' ')
	{
		/* TODO : if every font had an entry for the 'space' character,
		this test could be removed */
		char *data = font->glyphs[(U8)'I'];
		font_width = *data++;
		font_byte_width = (font_width + 7) >> 3;
		font_width += GET_FONT_SPACING (font);
		font_height = *data++;
		return (font_space);
	}
	else
	{
		char *data = font->glyphs[(U8)c];
		font_width = *data++;
		font_byte_width = (font_width + 7) >> 3;
		font_width += GET_FONT_SPACING (font);
		font_height = *data++;
		return ((U8 *)data);
	}
}


/** The current x-coordinate where a character is being written */
U8 blit_xpos;

U8 *blit_dmd;

U8 *blit_data;


/** Write an 8-bit value to an arbitrary location on the DMD.  
All inputs to this function are kept in global variables that must be 
initialized prior to entry. */
static void font_blit (void)
{
	switch (blit_xpos % 8)
	{
		default:
		case 0:
			blit_dmd[0] ^= *blit_data;
			break;
		case 1:
			blit_dmd[0] ^= *blit_data << 1;
			blit_dmd[1] = (*blit_data >> 7) ^ blit_dmd[1];
			break;
		case 2:
			blit_dmd[0] ^= *blit_data << 2;
			blit_dmd[1] = (*blit_data >> 6) ^ blit_dmd[1];
			break;
		case 3:
			blit_dmd[0] ^= *blit_data << 3;
			blit_dmd[1] = (*blit_data >> 5) ^ blit_dmd[1];
			break;
		case 4:
			blit_dmd[0] ^= *blit_data << 4;
			blit_dmd[1] = (*blit_data >> 4) ^ blit_dmd[1];
			break;
		case 5:
			blit_dmd[0] ^= *blit_data << 5;
			blit_dmd[1] = (*blit_data >> 3) ^ blit_dmd[1];
			break;
		case 6:
			blit_dmd[0] ^= *blit_data << 6;
			blit_dmd[1] = (*blit_data >> 2) ^ blit_dmd[1];
			break;
		case 7:
			blit_dmd[0] ^= *blit_data << 7;
			blit_dmd[1] = (*blit_data >> 1) ^ blit_dmd[1];
			break;
	}
	blit_data++;
}


/** Renders a string whose characteristics have already been
 * computed.  font_args contains the font type, starting
 * coordinates (from the upper left), and pointer to the string
 * data. */
static void fontargs_render_string (void)
{
	static U8 *dmd_base;
	static const char *s;
	char c;
	const fontargs_t *args = &font_args;

	dmd_base = ((U8 *)dmd_low_buffer) + args->coord.y * DMD_BYTE_WIDTH;
	s = sprintf_buffer;
  	blit_xpos = args->coord.x;

	/* When running in native mode, there is no DMD.  However it
	is useful to know what text the program is trying to display,
	so output the text string to the console instead.  Note that
	there is no return here, so the remaining code is still executed,
	and the 'virtual' DMD buffer is indeed written to, although it
	can't actually be seen. */
#ifdef CONFIG_PLATFORM_LINUX
	linux_write_string (s);
#endif

	/* Font data is stored in a separate page of ROM; switch
	 * there to be able to read the font data */
	wpc_push_page (FONT_PAGE);

	while ((c = *s++) != '\0')
	{
		static U8 i, j;
		static U8 xb;
		static U8 top_space;

		blit_data = font_lookup (args->font, c);

		if (font_height < args->font->height)
		{
			top_space = (args->font->height - font_height);
			top_space *= DMD_BYTE_WIDTH;
			dmd_base += top_space;
		}
		else
			top_space = 0;

		xb = blit_xpos / 8;

		blit_dmd = wpc_dmd_addr_verify (dmd_base + xb);
		for (i=0; i < font_height; i++)
		{
			task_dispatching_ok = TRUE;
			for (j=0; j < font_byte_width; j++)
			{
				/* TODO : font_blit is applicable to more than just
				fonts; it could be used for arbitrary-sized bitmaps. */
				font_blit ();
				blit_dmd = wpc_dmd_addr_verify (blit_dmd + 1);

			} /* end for each byte in same row */

			blit_dmd = wpc_dmd_addr_verify (blit_dmd - font_byte_width);
			blit_dmd = wpc_dmd_addr_verify (blit_dmd + DMD_BYTE_WIDTH);
		} /* end for each row */

		/* advance by 1 char ... args->font->width */
		blit_xpos += font_width;
		if (top_space != 0)
			dmd_base -= top_space;
	} /* end for each character in the string */
	wpc_pop_page ();
}


/** Erase an arbitrary region of the DMD. */
void blit_erase (union dmd_coordinate coord, U8 width, U8 height)
{
	U8 *dmd_base;
	static U8 xr;
	static U8 partial_left[] = { 
		0x0, 0x7f, 0x3f, 0x1f, 0x0f, 0x7, 0x3, 0x1 };
	static U8 partial_right[] = { 
		0x0, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80 };
	static U16 hoffset;
	S16 hoff;

	dmd_base = ((U8 *)dmd_low_buffer) + coord.y * DMD_BYTE_WIDTH +
		coord.x / 8;
	xr = coord.x % 8;
	hoffset = height * DMD_BYTE_WIDTH;
	
	if (xr)
	{
		/* Erase partial left region */
		U8 mask = partial_left[(8 - xr)];
		width -= xr;
		for (hoff=hoffset; hoff >= 0; hoff -= DMD_BYTE_WIDTH)
			dmd_base[hoff] &= mask;
		dmd_base++;
	}

	while (width >= 16)
	{
		/* Erase middle region */
		for (hoff=hoffset; hoff >= 0; hoff -= DMD_BYTE_WIDTH)
			((U16 *)dmd_base)[hoff] = 0;
		dmd_base += 2;
		width -= 16;
	}

	while (width >= 8)
	{
		/* Erase middle region */
		for (hoff=hoffset; hoff >= 0; hoff -= DMD_BYTE_WIDTH)
			dmd_base[hoff] = 0;
		dmd_base++;
		width -= 8;
	}

	if (width)
	{
		/* Erase partial right region */
		U8 mask = partial_right[width];
		for (hoff=hoffset; hoff >= 0; hoff -= DMD_BYTE_WIDTH)
			dmd_base[hoff] &= mask;
	}
}


/** Calculate font_string_width and font_string_height
 * in advance for a particular string to be rendered in a
 * particular font.  This is needed when doing centered or
 * right-justified writing, in order to calculate how to translate
 * the input coordinates to the real starting coordinates. */
void font_get_string_area (const font_t *font, const char *s)
{
	U8 c;

	/* Copy the string to be rendered into the RAM buffer,
	 * if it is not already there.  This allows us to be
	 * more efficient later on, as page flipping between
	 * static strings and the font data is not required.
	 */
	if (s != sprintf_buffer)
	{
		char *ram = sprintf_buffer;
		while (*s != '\0')
			*ram++ = *s++;
		*ram = '\0';
		s = sprintf_buffer;
	}

	wpc_push_page (FONT_PAGE);

	font_string_width = 0;
	font_string_height = 0;

	while ((c = *s++) != '\0')
	{
		/* The character data is irrelevant; we just call this
		to set font_width and font_height. */
		(void)font_lookup (font, c);

		/* Update the total width */
		font_string_width += font_width;

		/* Update the total height */
		if (font_height > font_string_height)
			font_string_height = font_height;
	}

	/* Don't count the space at the end of the string */
	font_string_width -= GET_FONT_SPACING (font);

	wpc_pop_page ();

	/* This can take a while for long strings; don't let the
	software watchdog expire */
	task_dispatching_ok = TRUE;
}


static void fontargs_prep_left (const fontargs_t *args)
{
	font_get_string_area (args->font, args->s);
	if (args != &font_args)
	{
		font_args.coord = args->coord;
		font_args.font = args->font;
		font_args.s = args->s;
	}
}

static void fontargs_prep_center (const fontargs_t *args)
{
	font_get_string_area (args->font, args->s);
	font_args.coord.x = args->coord.x - (font_string_width / 2);
	font_args.coord.y = args->coord.y - (font_string_height / 2);
	if (args != &font_args)
	{
		font_args.font = args->font;
		font_args.s = args->s;
	}
}

static void fontargs_prep_right (const fontargs_t *args)
{
	font_get_string_area (args->font, args->s);
	font_args.coord.x = args->coord.x - font_string_width;
	if (args != &font_args)
	{
		font_args.coord.y = args->coord.y;
		font_args.font = args->font;
		font_args.s = args->s;
	}
}


void fontargs_render_string_left (const fontargs_t *args)
{
	fontargs_prep_left (args);
	fontargs_render_string ();
}


void fontargs_render_string_center (const fontargs_t *args)
{
	fontargs_prep_center (args);
	fontargs_render_string ();
}


void fontargs_render_string_right (const fontargs_t *args)
{
	fontargs_prep_right (args);
	fontargs_render_string ();
}


void fontargs_render_string_left2 (const fontargs_t *args)
{
	fontargs_prep_left (args);
	fontargs_render_string ();
	dmd_flip_low_high ();
	fontargs_render_string ();
	dmd_flip_low_high ();
}


void fontargs_render_string_center2 (const fontargs_t *args)
{
	fontargs_prep_center (args);
	fontargs_render_string ();
	dmd_flip_low_high ();
	fontargs_render_string ();
	dmd_flip_low_high ();
}


void fontargs_render_string_right2 (const fontargs_t *args)
{
	fontargs_prep_right (args);
	fontargs_render_string ();
	dmd_flip_low_high ();
	fontargs_render_string ();
	dmd_flip_low_high ();
}


void bitmap_draw (union dmd_coordinate coord, U8 c)
{
	extern const font_t font_bitmap_common;

	sprintf_buffer[0] = c;
	sprintf_buffer[1] = '\0';

	font_args.font = &font_bitmap_common;
	font_args.coord = coord;
	font_args.s = sprintf_buffer;

	fontargs_render_string ();
}

