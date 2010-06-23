/*
 * Copyright 2007, 2008, 2009 by Brian Dominy <brian@oddchange.com>
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


/* FIF is the FreeWPC Image Format, and fiftool is the main utility for
reading/writing FIF files.  FIF is a compressed image format.  This is
a replacement for the older, xbmprog construct.

Common scenarios:

- you have a PGM (at most 2 bitplanes) generated through external means.
You want to convert it to FIF.  Use fiftool -o image.fif -c image.pgm.

- Or maybe it's in XBM format.  That works too: 
fiftool -o image.fif -c image.xbm

- Before writing a frame out, maybe you want to apply some transformation on
it.  Use fiftool's command-line options to accept common pgmlib.c functions
without having to write a full-fledged program.

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pgmlib.h>

#define MAX_INFILES 128

#define error(format, rest...) \
do { \
	fprintf (stderr, "fiftool: "); \
	fprintf (stderr, format, ## rest); \
	fprintf (stderr, "\n"); \
	exit (1); \
} while (0)

#define output_write(ofp, b) fprintf (ofp, "0x%02X, ", b)


#define OPT_NEGATE 0x1
#define OPT_DITHER 0x2
#define OPT_DELTA 0x4


enum image_format { 
	FORMAT_BAD, FORMAT_XBM, FORMAT_PGM, FORMAT_FIF
}; 


unsigned int n_infiles = 0;

const char *infile[MAX_INFILES];

unsigned long infile_options[MAX_INFILES];

const char *outfile;

enum image_format informat;

enum image_format outformat;


/** Return the format of a file, based on its extension. */
enum image_format get_file_format (const char *filename)
{
	char *sep = strrchr (filename, '.');
	if (!sep)
		return FORMAT_BAD;

	sep++;
	if (!strcmp (sep, "xbm"))
		return FORMAT_XBM;
	else if (!strcmp (sep, "pgm"))
		return FORMAT_PGM;
	else if (!strcmp (sep, "fif"))
		return FORMAT_FIF;
	else
		return FORMAT_BAD;
}


/** Open a new output file. */
FILE *output_file_open (void)
{
	FILE *fp;
	char symbol_name[64], *sym;
	char *sep;

	fp = fopen (outfile, "w");
	if (!fp)
		error ("could not open %s for writing\n", outfile);

	fprintf (fp, "#include <freewpc.h>\n");
	fprintf (fp, "#include <xbmprog.h>\n\n");
	fprintf (fp, "/* vi: set filetype=c: Automatically generated by fiftool */\n\n");

	sprintf (symbol_name, "%s", outfile);

	sym = symbol_name;
	while ((sep = strchr (sym, '/')) != NULL)
		sym = sep+1;

	sep = strrchr (sym, '.');
	if (!sep)
		error ("invalid output file name");
	*sep++ = '\0';

	fprintf (fp, "const U8 %s_%s[] = {\n", sep, sym);
	return fp;
}


/** Close an output file. */
void output_file_close (FILE *fp)
{
	fprintf (fp, "};\n");
	fclose (fp);
}


/** Write a FIF formatted file */
void write_fif (void)
{
	FILE *ofp;
	int n;
	int plane;
	PGM *pgm;
	XBM *xbm;
	XBMSET *xbmset;
	XBMPROG *xbmprog;
	enum image_format format;
	int n_planes = 2;

	ofp = output_file_open ();
	for (n=0 ; n < n_infiles; n++)
	{
		format = get_file_format (infile[n]);

		/* Output the format (XBM or PGM).  Equivalently, this
		is the number of bitplanes in the image. */
		fprintf (ofp, "   ");
		output_write (ofp, format);
		fprintf (ofp, "/* format */\n");

		switch (format)
		{
			case FORMAT_XBM:
				n_planes = 1;
				/* FALLTHROUGH */

			case FORMAT_PGM:	
				/* Convert XBM/PGM to FIF. */
				pgm = pgm_read (infile[n]);
				if (!pgm)
					error ("cannot open %s for reading\n", infile[n]);

				/* Apply any options to the input file before proceeding. */
				if (infile_options[n] & OPT_DITHER)
					pgm_dither (pgm, (1 << n_planes) - 1);
				else
					pgm_change_maxval (pgm, (1 << n_planes) - 1);

				if (infile_options[n] & OPT_NEGATE)
					pgm_invert (pgm);

				/* Convert into the internal xbmset format.
				This divides the PGM into 2 XBMs. */
				xbmset = pgm_make_xbmset (pgm);
	
				/* Now convert each XBM plane into FIF format. */
				for (plane = 0; plane < n_planes; plane++)
				{
					fprintf (ofp, "   ");
					xbm = xbmset_plane (xbmset, plane);

					/* Use RLE encoding to save space, or RLE_DELTA
					if requested */
					xbmprog = xbm_make_prog (xbm);

					if (infile_options[n] & OPT_DELTA)
						fprintf (ofp, "XBMPROG_METHOD_RLE_DELTA, ");
					else
						fprintf (ofp, "XBMPROG_METHOD_RLE, ");

					xbmprog_write (ofp, NULL, 0, xbmprog);
					xbmprog_free (xbmprog);

					fprintf (ofp, "\n");
				}
				xbmset_free (xbmset);
				pgm_free (pgm);
				break;

			default:
				error ("invalid input file format");
		}
	}
	output_file_close (ofp);
}


int main (int argc, char *argv[])
{
	int argn;
	const char *arg;
	enum image_format format;

	for (argn = 1; argn < argc; argn++)
	{
		arg = argv[argn];
		if (*arg == '-')
		{
			switch (arg[1])
			{
				case 'c':
					break;

				case 'n':
					infile_options[n_infiles-1] |= OPT_NEGATE;
					break;

				case 'd':
					infile_options[n_infiles-1] |= OPT_DELTA;
					break;

				case 'o':
					outfile = argv[++argn];
					break;
			}
		}
		else
		{
			/* Add to input file list */
			infile[n_infiles] = arg;
			infile_options[n_infiles] = 0;
			n_infiles++;
		}
	}

	if (!outfile)
		error ("no output file given");

	switch (format = get_file_format (outfile))
	{
		case FORMAT_FIF:
			write_fif ();
			break;
		default:
			error ("invalid output file format (%d) for %s", format, outfile);
	}
	return 0;
}
