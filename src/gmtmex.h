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

#ifndef GMTMEX_H
#define GMTMEX_H

#include "gmt.h"
#ifdef NO_MEX
#define mxArray void
#else
#include <mex.h>
#define mxIsScalar_(mx) \
	( (2 == mxGetNumberOfDimensions(mx)) \
		&&  (1 == mxGetM(mx))&&  (1 == mxGetN(mx)) )
#endif	/* NO_MEX */
#include <string.h>
#include <ctype.h>
#ifndef GMTMEX_LIB
#include "gmtmex_keys.h"
#endif
#ifdef GMT_MATLAB
#define MEX_PROG "Matlab"
#define MEX_COL_ORDER GMT_IS_COL_FORMAT
#else
#define MEX_PROG "Octave"
#define MEX_COL_ORDER GMT_IS_ROW_FORMAT
#endif

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

struct GMTMEX {	/* Array to hold information relating to output from GMT */
	enum GMT_enum_family type;	/* type of GMT data, i.e., GMT_IS_DATASET, GMT_IS_GRID, etc. */
	enum GMT_io_enum direction;	/* Either GMT_IN or GMT_OUT */
	int ID;				/* Registration ID returned by GMT_Register_IO */
	int lhs_index;			/* Corresponding index into plhs array */
};

EXTERN_MSC int GMTMEX_print_func (FILE *fp, const char *message);
EXTERN_MSC int GMTMEX_pre_process (void *API, const char *module, mxArray *plhs[], int nlhs, const mxArray *prhs[], int nrhs, char *keys, struct GMT_OPTION **head, struct GMTMEX **X);
EXTERN_MSC int GMTMEX_post_process (void *API, struct GMTMEX *X, int n_items, mxArray *plhs[]);
EXTERN_MSC int GMTMEX_find_module (void *API, char *module);

#endif
