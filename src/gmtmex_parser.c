/*
 *	$Id$
 *
 *	Copyright (c) 1991-2013 by P. Wessel, W. H. F. Smith, R. Scharroo, J. Luis and F. Wobbe
 *      See LICENSE.TXT file for copying and redistribution conditions.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU Lesser General Public License as published by
 *      the Free Software Foundation; version 3 or any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU Lesser General Public License for more details.
 *
 *      Contact info: www.soest.hawaii.edu/pwessel
 *--------------------------------------------------------------------*/
/* GMT convenience functions used by MATLAB/OCTAVE mex functions
 */

#include "gmt.h"
#include <string.h>

#define GMT_VIA_MEX	0
#define GMT_IS_PS	99

#define TESTING
#ifdef TESTING
unsigned int unique_ID = 0;
#endif

/* New parser for all GMT mex modules based on design discussed by PW and JL on Mon, 2/21/11 */
/* Wherever we say "Matlab" we mean "Matlab of Octave" */

/* For the Mex interface we will wish to pass either filenames or matrices via GMT command options.
 * We select a Matlab matrix by suppying $ as the file name.  The parser will then find these $
 * arguments and replace them with references to the arrays via the GMT API mechanisms.
 * This requires us to know which options in a module may accept a file name.  As an example,
 * consider surface whose -L option may take a grid.  To pass a Matlab/Octave grid already in memory
 * we would use -L$ and give the grid as an argument to the module, e.g.,
 * Z = surface ('-R0/50/0/50 -I1 -V xyzfile -L$', lowmatrix);
 * For each option that may take a file we need to know what kind of file and if this is input or output.
 * We encode this in a 3-character word, where the first char is the option, the second is the data type,
 * and the third is I(n) or O(out).  E.g., the surface example would have the word LGI.  The data types
 * are P(olygons), L(ines), D(point data), G(rid), C(PT file), T(ext table). [We originally only had
 * D for datasets but perhaps the geometry needs to be passed too (if known); hence the P|L|D char]
 * In addition, the only common option that might take a file is -R which may take a grid as input.
 * We check for that in addition to the module-specific info passed via the key variable.
 *
 * The actual reading/writing will occur in gmt_api.c where we will add a case MEX: for each type.
 * and here we will use mx* for allocation for stuff that is sent to Matlab, and GMT_memory for things
 * that are read and reformatted from Matlab.  This includes packing up GMT grids into Matlab grid structs
 * and vice versa.
 */

#define GMT_MEX_NONE		-3
#define GMT_MEX_EXPLICIT	-2
#define GMT_MEX_IMPLICIT	-1

int gmtmex_find_option (char option, char *key[], int n_keys) {
	/* gmtmex_find_option determines if the given option is among the special options that might take $ as filename */
	int pos = -1, k;
	for (k = 0; pos == -1 && k < n_keys; k++) if (key[k][0] == option) pos = k;
	return (pos);	/* -1 if not found, otherwise the position in the key array */
}

int gmtmex_get_arg_pos (char *arg)
{	/* Look for a $ in the arg; if found return position, else return -1. Skips $ inside quoted texts */
	int pos, k;
	unsigned int mute = 0;
	for (k = 0, pos = -1; pos == -1 && k < strlen (arg); k++) {
		if (arg[k] == '\"' || arg[k] == '\'') mute = !mute;	/* Do not consider $ inside quotes */
		if (!mute && arg[k] == '$') pos = k;	/* Found a $ sign */
	}
	return (pos);	/* Either -1 (not found) or in the 0-(strlen(arg)-1) range [position of $] */
}

unsigned int gmtmex_get_key_pos (char *key[], unsigned int n_keys, struct GMT_OPTION *head, int def[])
{	/* Must determine if default input and output have been set via program options or if they should be added explicitly.
 	 * As an example, consider the GMT command grdfilter in.nc -Fg200k -Gfilt.nc.  In Matlab this might be
	 * filt = GMT_grdfilter ('$ -Fg200k -G$', in);
	 * However, it is more natural not to specify the lame -G$, i.e.
	 * filt = GMT_grdfilter ('$ -Fg200k', in);
	 * In that case we need to know that -G is the default way to specify the output grid and if -G is not given we
	 * must associate -G with the first left-hand-side item (here filt).
	 */
	int pos, PS = 0;
	struct GMT_OPTION *opt = NULL;
	def[GMT_IN] = def[GMT_OUT] = GMT_MEX_IMPLICIT;	/* Initialize to setting the i/o implicitly */
	
	for (opt = head; opt; opt = opt->next) {	/* Loop over the module options to see if inputs and outputs are set explicitly or implicitly */
		pos = gmtmex_find_option (opt->option, key, n_keys);	/* First see if this option is one that might take $ */
		if (pos == -1) continue;		/* No, it was some other harmless option, e.g., -J, -O ,etc. */
		/* Here, the current option is one that might take an input or output file. See if it matches
		 * the UPPERCASE I or O [default source/dest] rather than the standard i|o (optional input/output) */
		if (key[pos][2] == 'I') def[GMT_IN]  = GMT_MEX_EXPLICIT;	/* Default input  is actually set explicitly via option setting now indicated by key[pos] */
		if (key[pos][2] == 'O') def[GMT_OUT] = GMT_MEX_EXPLICIT;	/* Default output is actually set explicitly via option setting now indicated by key[pos] */
	}
	/* Here, if def[] == GMT_MEX_IMPLICIT (the default in/out option was NOT given), then we want to return the corresponding entry in key */
	for (pos = 0; pos < n_keys; pos++) {	/* For all module options that might take a file */
		if ((key[pos][2] == 'I' || key[pos][2] == 'i') && key[pos][0] == '-') def[GMT_IN]  = GMT_MEX_NONE;	/* This program takes no input (e.g., psbasemap) */
		else if (key[pos][2] == 'I' && def[GMT_IN]  == GMT_MEX_IMPLICIT) def[GMT_IN]  = pos;	/* Must add implicit input; use def to determine option,type */
		if ((key[pos][2] == 'O' || key[pos][2] == 'o') && key[pos][0] == '-') def[GMT_OUT] = GMT_MEX_NONE;	/* This program produces no output */
		else if (key[pos][2] == 'O' && def[GMT_OUT] == GMT_MEX_IMPLICIT) def[GMT_OUT] = pos;	/* Must add implicit output; use def to determine option,type */
		if ((key[pos][2] == 'O' || key[pos][2] == 'o') && key[pos][1] == 'X' && key[pos][0] == '-') PS = 1;	/* This program produces PostScript */
	}
	return (PS);
}

int gmtmex_get_arg_dir (char option, char *key[], int n_keys, int *data_type, int *geometry)
{
	int item;
	
	/* 1. First determine if this option is one of the choices in key */
	
	item = gmtmex_find_option (option, key, n_keys);
	if (item == -1) fprintf (stderr, "GMTMEX_parser: This option does not allow $ arguments\n");	/* This means a coding error we must fix */
	
	/* 2. Assign direction, data_type, and geometry */
	
	switch (key[item][1]) {	/* 2nd char contains the data type code */
		case 'G':
			*data_type = GMT_IS_GRID;
			*geometry = GMT_IS_SURFACE;
			break;
		case 'P':
			*data_type = GMT_IS_DATASET;
			*geometry = GMT_IS_POLY;
			break;
		case 'L':
			*data_type = GMT_IS_DATASET;
			*geometry = GMT_IS_LINE;
			break;
		case 'D':
			*data_type = GMT_IS_DATASET;
			*geometry = GMT_IS_POINT;
			break;
		case 'C':
			*data_type = GMT_IS_CPT;
			*geometry = GMT_IS_NONE;
			break;
		case 'T':
			*data_type = GMT_IS_TEXTSET;
			*geometry = GMT_IS_NONE;
			break;
		case 'I':
			*data_type = GMT_IS_IMAGE;
			*geometry = GMT_IS_SURFACE;
			break;
		case 'X':
			*data_type = GMT_IS_PS;
			*geometry = GMT_IS_NONE;
			break;
		default:
			fprintf (stderr, "GMTMEX_parser: Bad data_type character in 3-char module code!\n");
			break;
	}
	/* Third key character contains the in/out code */
	if (key[item][2] == 'I') key[item][2] = 'i';	/* This was the default input option set explicitly; no need to add later */
	if (key[item][2] == 'O') key[item][2] = 'o';	/* This was the default output option set explicitly; no need to add later */
	return ((key[item][2] == 'i') ? GMT_IN : GMT_OUT);
}

char ** make_char_array (char *string, unsigned int *n_items)
{
	unsigned int len, k, n;
	char **s = NULL;
	char *next, *tmp;
	
	if (!string) return NULL;
	len = strlen (string);
	if (len == 0) return NULL;
	tmp = strdup (string);
	for (k = n = 0; k < len; k++) if (tmp[k] == ',') n++;
	n++;
	s = (char **) calloc (n, sizeof (char *));
	k = 0;
	while ((next = strsep (&tmp, ",")) != NULL) {
		s[k++] = strdup (next);
	}
	*n_items = n;
	free ((void *)tmp);
	return s;
}

int GMTMEX_parser (void *API, void *plhs[], int nlhs, void *prhs[], int nrhs, char *keys, struct GMT_OPTION *head)
{
	/* API controls all things within GMT.
	 * plhs (and nlhs) are the outputs specified on the left side of the equal sign in Matlab.
	 * prhs (and nrhs) are the inputs specified after the option string in the GMT-mex function.
	 * keys is comma-separated string with 3-char codes for current module i/o.
	 * opt is the linked list of GMT options passed in.
	 */
	
	int lr_pos[2] = {0, 0};	/* These position keeps track where we are in the L and R pointer arrays */
	int direction;		/* Either GMT_IN or GMT_OUT */
	int data_type;		/* Either GMT_IS_DATASET, GMT_IS_TEXTSET, GMT_IS_GRID, GMT_IS_CPT, GMT_IS_IMAGE */
	int geometry;		/* Either GMT_IS_NONE, GMT_IS_POINT, GMT_IS_LINE, GMT_IS_POLY, or GMT_IS_SURFACE */
	int def[2];		/* Either GMT_MEX_EXPLICIT or the item number in the keys array */
	int ID, error;
	unsigned int k, n_keys = 0, pos, PS;
	char name[GMTAPI_STRLEN];	/* Used to hold the GMT API embedded file name, e.g., @GMTAPI@-###### */
	char **key = NULL;
	struct GMT_OPTION *opt, *new_ptr;	/* Pointer to a GMT option structure */
#ifndef TESTING
	void *ptr = NULL;		/* Void pointer used to point to either L or R side pointer argument */
#endif
	
	key = make_char_array (keys, &n_keys);
	
	PS = gmtmex_get_key_pos (key, n_keys, head, def);	/* Determine if we must add the primary in and out arguments to the option list */
	for (direction = GMT_IN; direction <= GMT_OUT; direction++) {
		if (def[direction] == GMT_MEX_NONE) continue;	/* No source or destination required */
		if (def[direction] == GMT_MEX_EXPLICIT) continue;	/* Source or destination was set explicitly; skip */
		/* Must add the primary input or output from prhs[0] or plhs[0] */
		(void)gmtmex_get_arg_dir (key[def[direction]][0], key, n_keys, &data_type, &geometry);		/* Get info about the data set */
#ifdef TESTING
		ID = unique_ID++;
#else
		ptr = (direction == GMT_IN) ? (void *)prhs[lr_pos[direction]] : (void *)plhs[lr_pos[direction]];	/* Pick the next left or right side pointer */
		/* Register a Matlab/Octave entity as a source or destination */
		if ((ID = GMT_Register_IO (API, data_type, GMT_IS_REFERENCE + GMT_VIA_MEX, geometry, direction, NULL, ptr)) == GMTAPI_NOTSET) {
			fprintf (stderr, "GMTMEX_parser: Failure to register GMT source or destination\n");
		}
#endif
		lr_pos[direction]++;		/* Advance uint64_t for next time */
		if (GMT_Encode_ID (API, name, ID) != GMT_NOERROR) {	/* Make filename with embedded object ID */
			fprintf (stderr, "GMTMEX_parser: Failure to encode string\n");
		}
		new_ptr = GMT_Make_Option (API, key[def[direction]][0], name);	/* Create the missing (implicit) GMT option */
		GMT_Append_Option (API, new_ptr, head);				/* Append it to the option list */
	}
		
	for (opt = head; opt; opt = opt->next) {	/* Loop over the module options given */
		if (PS && opt->option == GMTAPI_OPT_OUTFILE) PS++;
		/* Determine if this option as a $ in its argument and if so return its position in pos; return -1 otherwise */
		if ((pos = gmtmex_get_arg_pos (opt->arg)) == -1) continue;	/* No $ argument found or it is part of a text string */
		
		/* Determine several things about this option, such as direction, data type, method, and geometry */
		direction = gmtmex_get_arg_dir (opt->option, key, n_keys, &data_type, &geometry);
#ifdef TESTING
		ID = unique_ID++;
#else
		ptr = (direction == GMT_IN) ? (void *)prhs[lr_pos[direction]] : (void *)plhs[lr_pos[direction]];	/* Pick the next left or right side pointer */
		/* Register a Matlab/Octave entity as a source or destination */
		if ((ID = GMT_Register_IO (API, data_type, GMT_IS_REFERENCE + GMT_VIA_MEX, geometry, direction, NULL, ptr)) == GMTAPI_NOTSET) {
			fprintf (stderr, "GMTMEX_parser: Failure to register GMT source or destination\n");
		}
#endif
		if (GMT_Encode_ID (API, name, ID) != GMT_NOERROR) {	/* Make filename with embedded object ID */
			fprintf (stderr, "GMTMEX_parser: Failure to encode string\n");
		}
		lr_pos[direction]++;		/* Advance uint64_t for next time */
		
		/* Replace the option argument with the embedded file */
		if (GMT_Update_Option (API, opt->option, name, head)) {
			fprintf (stderr, "GMTMEX_parser: Failure to update option argument\n");
		}
	}
	
	if (PS == 1)	/* No redirection of PS to a file */
		error = 1;
	else if (PS > 2)	/* Too many output files for PS */
		error = 2;
	else
		error = GMT_NOERROR;
	for (k = 0; k < n_keys; k++) free ((void *)key[k]);
	free ((void *)key);
	
	/* Here, a command line '-F200k -G$ $ -L$ -P' has been changed to '-F200k -G@GMTAPI@-000001 @GMTAPI@-000002 -L@GMTAPI@-000003 -P'
	 * where the @GMTAPI@-00000x are encodings to registered resources or destinations */
	return (error);
}
