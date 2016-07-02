/*--------------------------------------------------------------------
 *	$Id$
 *
 *	Copyright (c) 1991-2016 by P. Wessel and J. Luis
 *	See LICENSE.TXT file for copying and redistribution conditions.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU Lesser General Public License as published by
 *	the Free Software Foundation; version 3 or any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU Lesser General Public License for more details.
 *
 *	Contact info: gmt.soest.hawaii.edu
 *--------------------------------------------------------------------*/
/*
 * This is the MATLAB/Octave(mex) GMT application, which can do the following:
 * 1) Create a new session and optionally return the API pointer. We provide for
 *    storing the pointer as a global variable (persistent) between calls.
 * 2) Destroy a GMT session, either given the API pointer or by fetching it from
 *    the global (persistent) variable.
 * 3) Call any of the GMT modules while passing data in and out of GMT.
 *
 * First argument to the gmt function is the API pointer, but it is optional once created.
 * Next argument is the module name
 * Thrid argument is the option string
 * Finally, there are optional comma-separated MATLAB array entities required by the command.
 * Information about the options of each program is provided via GMT_Encode_Options.
 *
 * Version:	5.3.x
 * Created:	20-JUN-2016
 *
 */

#include "gmtmex.h"

extern int GMT_get_V (char arg);	/* Temporary here to allow full debug messaging */

/* Being declared external we can access it between MEX calls */
static uintptr_t *pPersistent;    /* To store API address back and forth within a single MATLAB session */

/* Here is the exit function, which gets run when the MEX-file is
   cleared and when the user exits MATLAB. The mexAtExit function
   should always be declared as static. */
static void force_Destroy_Session (void) {
	void *API = (void *)pPersistent[0];	/* Get the GMT API pointer */
	if (API != NULL) {		/* Otherwise just silently ignore this call */
		if (GMT_Destroy_Session (API)) mexErrMsgTxt ("Failure to destroy GMT session\n");
		*pPersistent = 0;	/* Wipe the persistent memory */
	}
}

static void usage (int nlhs, int nrhs) {
	/* Basic usage message */
	if (nrhs == 0) {	/* No arguments at all results in the GMT banner message */
		mexPrintf("\nGMT - The Generic Mapping Tools, Version 5.%d %s API\n", GMT_MINOR_VERSION, MEX_PROG);
		mexPrintf("Copyright 1991-2016 Paul Wessel, Walter H. F. Smith, R. Scharroo, J. Luis, and F. Wobbe\n\n");
		mexPrintf("This program comes with NO WARRANTY, to the extent permitted by law.\n");
		mexPrintf("You may redistribute copies of this program under the terms of the\n");
		mexPrintf("GNU Lesser General Public License.\n");
		mexPrintf("For more information about these matters, see the file named LICENSE.TXT.\n");
		mexPrintf("For a brief description of GMT modules, type gmt ('help')\n\n");
	}
	else {
		mexPrintf("Usage is:\n\tgmt ('module_name', 'options'[, <matlab arrays>]); %% Run a GMT module\n");
		if (nlhs != 0)
			mexErrMsgTxt ("But meanwhile you already made an error by asking help and an output.\n");
	}
}

static void *Initiate_Session (unsigned int verbose) {
	/* Initialize the GMT Session and store the API pointer in a persistent variable */
	void *API = NULL;
	/* Initializing new GMT session with a MATLAB-acceptable replacement for the printf function */
	/* For debugging with verbose we pass the specified verbose shifted by 10 bits - this is decoded in API */
	if ((API = GMT_Create_Session (MEX_PROG, 2U, (verbose << 10) + GMT_SESSION_NOEXIT + GMT_SESSION_EXTERNAL +
	                               GMT_SESSION_COLMAJOR, GMTMEX_print_func)) == NULL)
		mexErrMsgTxt ("GMT: Failure to create new GMT session\n");

	if (!pPersistent) pPersistent = mxMalloc(sizeof(uintptr_t));
	pPersistent[0] = (uintptr_t)(API);
	mexMakeMemoryPersistent (pPersistent);

	return (API);
}

#define N_MEX_FIELDNAMES_IMAGE	18
#define N_MEX_FIELDNAMES_GRID	16
#define N_MEX_FIELDNAMES_CPT	7
#define N_MEX_FIELDNAMES_PS	3
static void *alloc_default_plhs (void *API, struct GMT_RESOURCE *X) {
	/* Allocate a default plhs when it was not stated in command line. That is, mimic the Matlab behavior
	   when we do for example (i.e. no lhs):  sqrt([4 9])  
	*/
	int   k;
	char *fnames_grd[N_MEX_FIELDNAMES_GRID];	/* This array contains the names of the fields of the output grid structure. */
	char *fnames_img[N_MEX_FIELDNAMES_IMAGE];	/* This array contains the names of the fields of the output grid structure. */
	char *fnames_cpt[N_MEX_FIELDNAMES_CPT];	/* Array with the names of the fields of the output grid structure. */
	char *fnames_ps[N_MEX_FIELDNAMES_PS];	/* Array with the names of the fields of the output postscript structure. */
	void *ptr = NULL;
	mxClassID type;
	switch (X->family) {
		case GMT_IS_GRID:
			for (k = 0; k < N_MEX_FIELDNAMES_GRID; k++) fnames_grd[k] = "";
			ptr = (void *)mxCreateStructMatrix (0, 0, N_MEX_FIELDNAMES_GRID, (const char **)fnames_grd);
			break;
		case GMT_IS_IMAGE:
			for (k = 0; k < N_MEX_FIELDNAMES_IMAGE; k++) fnames_img[k] = "";
			ptr = (void *)mxCreateStructMatrix (0, 0, N_MEX_FIELDNAMES_IMAGE, (const char **)fnames_img);
			break;
		case GMT_IS_DATASET:
			type = GMTMEX_type (API);		/* Get GMT's default data type */
			ptr = (void *)mxCreateNumericMatrix(0, 0, type, mxREAL);
			break;
		case GMT_IS_TEXTSET:
			break;
		case GMT_IS_PALETTE:
			for (k = 0; k < N_MEX_FIELDNAMES_CPT; k++) fnames_cpt[k] = "";
			ptr = (void *)mxCreateStructMatrix (0, 0, N_MEX_FIELDNAMES_CPT, (const char **)fnames_cpt);
			break;
		case GMT_IS_POSTSCRIPT:
			for (k = 0; k < N_MEX_FIELDNAMES_PS; k++) fnames_ps[k] = "";
			ptr = (void *)mxCreateStructMatrix (0, 0, N_MEX_FIELDNAMES_PS, (const char **)fnames_cpt);
			break;
		default:
			break;
	}
	return ptr;
}

/* This is the function that is called when we type gmt in MATLAB/Octave */
void mexFunction (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
	int status = 0;                 /* Status code from GMT API */
	int n_in_objects = 0;           /* Number of input objects passed to module */
	unsigned int first = 0;         /* Array ID of first command argument (not 0 when API-ID is first) */
	unsigned int verbose = 0;       /* Default verbose setting */
	unsigned int n_items = 0, pos = 0; /* Number of MATLAB arguments (left and right) */
	size_t str_length = 0, k = 0;   /* Misc. counters */
	void *API = NULL;               /* GMT API control structure */
	struct GMT_OPTION *options = NULL; /* Linked list of module options */
	struct GMT_OPTION *opt = NULL;  /* Another LL used to hold only one option */
	struct GMT_RESOURCE *X = NULL;  /* Array of information about MATLAB args */
	char *cmd = NULL;               /* Pointer used to get the user's MATLAB command */
	char *gtxt = NULL;              /* For debug printing of revised command */
	char *opt_args = NULL;          /* Pointer to the user's module options */
	char module[MODULE_LEN] = {""}; /* Name of GMT module to call */
	char name[GMT_STR16];           /* Name of GMT module to call */
	void *ptr = NULL;
	uintptr_t *pti = NULL;          /* To locally store the API address */

	/* 0. No arguments at all results in the GMT banner message */
	if (nrhs == 0) {
		usage (nlhs, nrhs);
		return;
	}

	/* 1. Check for the special commands create and help */
	
	if (nrhs == 1) {	/* This may be create or help */
		cmd = mxArrayToString (prhs[0]);
		if (!cmd) mexErrMsgTxt("GMT: First input argument must be a string. Maybe a composition of a string and a cell array?\n");
		if (!strncmp (cmd, "help", 4U) || !strncmp (cmd, "--help", 6U)) {
			usage (nlhs, 1);
			return;
		}
		if (!strncmp (cmd, "create", 6U)) {	/* Asked to create a new GMT session */
			if (nlhs > 1)	/* Asked for too much output, only 1 or 0 is allowed */
				mexErrMsgTxt ("GMT: Usage: gmt ('create') or API = gmt ('create');\n");
			if (pPersistent)                        /* See if have a GMT API pointer */
				API = (void *)pPersistent[0];
			if (API != NULL) {                      /* If another session still exists */
				GMT_Report (API, GMT_MSG_VERBOSE,
				            "GMT: A previous GMT session is still active. Ignoring your 'create' request.\n");
				if (nlhs) /* Return nothing */
					plhs[0] = mxCreateNumericMatrix (1, 0, mxUINT64_CLASS, mxREAL);
				return;
			}
			if ((gtxt = strstr (cmd, "-V")) != NULL) verbose = GMT_get_V (gtxt[2]);
			API = Initiate_Session (verbose);	/* Initializing a new GMT session */

			if (nlhs) {	/* Return the API adress as an integer (nlhs == 1 here) )*/
				plhs[0] = mxCreateNumericMatrix (1, 1, mxUINT64_CLASS, mxREAL);
				pti = mxGetData(plhs[0]);
				*pti = *pPersistent;
			}

			mexAtExit(force_Destroy_Session);	/* Register an exit function. */
			return;
		}

		/* OK, neither create nor help, must be a single command with no arguments nor the API. So get it: */
		if (!pPersistent || (API = (void *)pPersistent[0]) == NULL) {	/* No session yet, create one under the hood */
			API = Initiate_Session(verbose);    /* Initializing a new GMT session */
			mexAtExit(force_Destroy_Session);   /* Register an exit function. */
		}
		else
			API = (void *)pPersistent[0];       /* Get the GMT API pointer */
		if (API == NULL) mexErrMsgTxt ("GMT: This GMT5 session has is currupted. Better to start from scratch.\n"); 
	}
	else if (mxIsScalar_(prhs[0]) && mxIsUint64(prhs[0])) {
		/* Here, nrhs > 1 . If first arg is a scalar int, we assume it is the API memory address */
		pti = (uintptr_t *)mxGetData(prhs[0]);
		API = (void *)pti[0];	/* Get the GMT API pointer */
		first = 1;		/* Commandline args start at prhs[1] since prhs[0] had the API id argument */
	}
	else {		/* We still don't have the API, so we must get it from the past or initiate a new session */
		if (!pPersistent || (API = (void *)pPersistent[0]) == NULL)
			API = Initiate_Session (verbose);	/* Initializing new GMT session */
			mexAtExit(force_Destroy_Session);	/* Register an exit function. */
	}

	if (!cmd) {	/* First argument is the command string, e.g., 'blockmean -R0/5/0/5 -I1' or just 'destroy' */
		cmd = mxArrayToString(prhs[first]);
		if (!cmd) mexErrMsgTxt("GMT: First input argument must be a string but is probably a cell array of strings.\n");
	}

	if (!strncmp (cmd, "destroy", 7U)) {	/* Destroy the session */
		if (nlhs != 0)
			mexErrMsgTxt ("GMT: Usage is gmt ('destroy');\n");

		if (GMT_Destroy_Options (API, &options)) mexErrMsgTxt ("GMT: Failure to destroy GMT5 options\n");
		if (GMT_Destroy_Session (API)) mexErrMsgTxt ("GMT: Failure to destroy GMT5 session\n");
		*pPersistent = 0;	/* Wipe the persistent memory */
		return;
	}

	/* 2. Get module name and separate out args */
	
	/* Here we have a GMT module call. The documented use is to give the module name separately from
	 * the module options, but users may forget and combine the two.  So we check both cases. */
	
	n_in_objects = nrhs - 1;
	str_length = strlen (cmd);				/* Length of module (or command) argument */
	for (k = 0; k < str_length && cmd[k] != ' '; k++);	/* Determine first space in command */
	
	if (k == str_length) {	/* Case 2a): No spaces found: User gave 'module' separately from 'options' */
		strcpy (module, cmd);				/* Isolate the module name in this string */
		if (nrhs > 1 && mxIsChar (prhs[first+1])) {	/* Got option string */
			first++;	/* Since we have a 2nd string to skip now */
			opt_args = mxArrayToString (prhs[first]);
			n_in_objects--;
		}
		/* Else we got no options, just input objects */
	}
	else {	/* Case b2. Get mex arguments, if any, and extract the GMT module name */
		if (k >= MODULE_LEN)
			mexErrMsgTxt ("GMT: Module name in command is too long\n");
		strncpy (module, cmd, k);	/* Isolate the module name in this string */

		while (cmd[k] == ' ') k++;	/* Skip any spaces between module name and start of options */
		if (cmd[k]) opt_args = &cmd[k];
	}


	/* See if info about instalation is required */
	if (!strcmp(module, "gmt")) {
		char t[256] = {""};
		if (!strcmp(opt_args, "--show-bindir")) 	/* Show the directory that contains the 'gmt' executable */
			GMT_Get_Default (API, "BINDIR", t);
		else if (!strcmp(opt_args, "--show-sharedir"))	/* Show share directory */
			GMT_Get_Default (API, "SHAREDIR", t);
		else if (!strcmp(opt_args, "--show-datadir"))	/* Show the data directory */
			GMT_Get_Default (API, "DATADIR", t);
		else if (!strcmp(opt_args, "--show-plugindir"))	/* Show the plugin directory */
			GMT_Get_Default (API, "PLUGINDIR", t);
		else if (!strcmp(opt_args, "--show-cores"))	/* Show number of cores */
			GMT_Get_Default (API, "CORES", t);

		if (t[0] != '\0') {
			if (nlhs)
				plhs[0] = mxCreateString (t);
			else
				mexPrintf ("%s\n", t);
		}
		else
			mexPrintf ("Warning: calling the 'gmt' program by itself does nothing here.\n");
		return;
	}

	/* Make sure this is a valid module */
	if ((status = GMT_Call_Module (API, module, GMT_MODULE_EXIST, NULL)) != 0) {	/* No, not found */
		char gmt_module[MODULE_LEN] = "gmt";	/* Alternate module name that starts with "gmt" */
		/* module does not contain a valid module name; try prepending gmt: */
		strncat (gmt_module, module, MODULE_LEN-4U);
		status = GMT_Call_Module (API, gmt_module, GMT_MODULE_EXIST, NULL); /* either GMT_NOERROR or GMT_NOT_A_VALID_MODULE */
		if (status)
			mexErrMsgTxt ("GMT: No module by that name was found.\n"); 
		strcpy (module, gmt_module);	/* Use the prepended module name since that one worked */
	}
	
	/* 2+ Add -F to psconvert if user requested a return image but did not explicitly give -F */
	if (!strncmp (module, "psconvert", 9U) && nlhs == 1 && (!opt_args || !strstr ("-F", opt_args))) {	/* OK, add -F */
		if (opt_args)
			strcat (opt_args, " -F");
		else
			opt_args = "-F";
	}
	/* 2+ If gmtwrite then add -T? with correct object type */
	
	if (!strncmp (module, "gmtwrite", 8U) && opt_args && n_in_objects == 1) {	/* Add type for writing to disk */
		char targ[5] = {" -T?"};
		gmtmex_objecttype (targ, prhs[nrhs-1]);
		strcat (opt_args, targ);
	}

	/* 3. Convert mex command line arguments to a linked GMT option list */
	if (opt_args && (options = GMT_Create_Options (API, 0, opt_args)) == NULL)
		mexErrMsgTxt ("GMT: Failure to parse GMT5 command options\n");

	/* 3+ Add -T to gmtwrite if user did not explicitly give -T. Also check that first arg is not mem API address */
	if (!strncmp(module, "gmtwrite", 8U) && !strstr(opt_args, "-T") && !mxIsScalar_(prhs[0]) && nrhs > (int)first+1) {
		mxArray *mx_ptr = NULL;
		 
		if ((mx_ptr = mxGetField (prhs[first+1], 0, "image")) != NULL)
			opt = GMT_Create_Options (API, 0, "-Ti");
		else if ((mx_ptr = mxGetField (prhs[first+1], 0, "z")) != NULL)
			opt = GMT_Create_Options (API, 0, "-Tg");
		else if ((mx_ptr = mxGetField (prhs[first+1], 0, "postscript")) != NULL)
			opt = GMT_Create_Options (API, 0, "-Tp");
		else if ((mx_ptr = mxGetField (prhs[first+1], 0, "bfn")) != NULL)
			opt = GMT_Create_Options (API, 0, "-Tc");
		else if (mxIsNumeric(prhs[first+1]))
			opt = GMT_Create_Options (API, 0, "-Td");
		else if (mxIsChar(prhs[first+1]))
			opt = GMT_Create_Options (API, 0, "-Tt");

		if (opt) GMT_Append_Option (API, opt, options);
	}

	if (!options && nlhs == 0 && nrhs == 1) {	/* Just requesting usage message, so add -? to options */
		options = GMT_Create_Options (API, 0, "-?");
	}
	
	/* 4. Preprocess to update GMT option lists and return info array X */
	if ((X = GMT_Encode_Options (API, module, n_in_objects, &options, &n_items)) == NULL) {
		if (n_items == UINT_MAX)	/* Just got usage/synopsis option */
			n_items = 0;
		else
			mexErrMsgTxt ("GMT: Failure to encode mex command options\n");
	}
	
	if (options) {	/* Only for debugging - remove this section when stable */
		gtxt = GMT_Create_Cmd (API, options);
		GMT_Report (API, GMT_MSG_DEBUG, "GMT_Encode_Options: Revised command after memory-substitution: %s\n", gtxt);
		GMT_Destroy_Cmd (API, &gtxt);	/* Only needed it for the above verbose */
	}
	
	/* 5. Assign input sources (from MATLAB to GMT) and output destinations (from GMT to MATLAB) */
	
	for (k = 0; k < n_items; k++) {	/* Number of GMT containers involved in this module call */
		if (X[k].direction == GMT_IN) {
			if ((X[k].pos+first+1) < (unsigned int)nrhs)
				ptr = (void *)prhs[X[k].pos+first+1];
			else
				mexErrMsgTxt ("GMT: Attempting to address a prhs entry that does not exist\n");
		}
		else {
			if ((X[k].pos) < nlhs)
				ptr = (void *)plhs[X[k].pos];
			else {
				//mexErrMsgTxt ("GMT: Attempting to address a plhs entry that does not exist\n");
				ptr = alloc_default_plhs (API, &X[k]);
			}
		}
		X[k].object = GMTMEX_Register_IO (API, &X[k], ptr);	/* Get object pointer */
		if (X[k].object == NULL || X[k].object_ID == GMT_NOTSET)
			mexErrMsgTxt("GMT: Failure to register the resource\n");
		if (GMT_Encode_ID (API, name, X[k].object_ID) != GMT_NOERROR) 	/* Make filename with embedded object ID */
			mexErrMsgTxt ("GMT: Failure to encode string\n");
		if (GMT_Expand_Option (API, X[k].option, name) != GMT_NOERROR)	/* Replace ? in argument with name */
			mexErrMsgTxt ("GMT: Failure to expand filename marker (?)\n");
	}
	
	/* 6. Run GMT module; give usage message if errors arise during parsing */
	status = GMT_Call_Module (API, module, GMT_MODULE_OPT, options);
	if (status != GMT_NOERROR) {
		if (status != GMT_SYNOPSIS) {
			mexPrintf("GMT: Module return with failure while executing the command\n%s\n", cmd);
			mexErrMsgTxt("GMT: exiting\n");
		}
		return;
	}

	/* 7. Hook up any GMT outputs to MATLAB plhs array */
	
	for (k = 0; k < n_items; k++) {	/* Get results from GMT into MATLAB arrays */
		if (X[k].direction == GMT_IN) continue;	/* Only looking for stuff coming OUT of GMT here */
		
		if ((X[k].object = GMT_Retrieve_Data (API, X[k].object_ID)) == NULL)
			mexErrMsgTxt ("GMT: Error retrieving object from GMT\n");
		pos = X[k].pos;	/* Short-hand for index into the plhs[] array being returned to MATLAB */
		switch (X[k].family) {	/* Determine what container we got */
			case GMT_IS_GRID:	/* A GMT grid; make it the pos'th output item */
				plhs[pos] = GMTMEX_Get_Grid (API, X[k].object);
				break;
			case GMT_IS_DATASET:	/* A GMT table; make it a matrix and the pos'th output item */
				plhs[pos] = GMTMEX_Get_Dataset (API, X[k].object);
				break;
			case GMT_IS_TEXTSET:	/* A GMT textset; make it a cell and the pos'th output item */
				plhs[pos] = GMTMEX_Get_Textset (API, X[k].object);
				break;
			case GMT_IS_PALETTE:	/* A GMT CPT; make it a colormap and the pos'th output item  */
				plhs[pos] = GMTMEX_Get_CPT (API, X[k].object);
				break;
			case GMT_IS_IMAGE:	/* A GMT Image; make it the pos'th output item  */
				plhs[pos] = GMTMEX_Get_Image (API, X[k].object);
				break;
#if GMT_MINOR_VERSION > 2
			case GMT_IS_POSTSCRIPT:		/* A GMT PostScript string; make it the pos'th output item  */
				plhs[pos] = GMTMEX_Get_POSTSCRIPT (API, X[k].object);
#if 0
				{
				char cmd[32] = {""};
				strcpy(cmd, name);		strcat(cmd, " -A -Tf");
				GMT_Call_Module(API, "psconvert", GMT_MODULE_CMD, cmd);
				}
#endif
				break;
#endif
			default:
				mexErrMsgTxt ("GMT: Internal Error - unsupported data type\n");
				break;
		}
	}
	
	/* 8. Free all GMT containers involved in this module call */
	
	for (k = 0; k < n_items; k++) {
		void *ppp = X[k].object;
		if (GMT_Destroy_Data (API, &X[k].object) != GMT_NOERROR)
			mexErrMsgTxt ("GMT: Failed to destroy object used in the interface between GMT and MATLAB\n");
		else {	/* Success, now make sure we dont destroy the same pointer more than once */
			for (size_t kk = k+1; kk < n_items; kk++)
				if (X[kk].object == ppp) X[kk].object = NULL;
		}
	}

	/* 9. Destroy linked option list */
	
	if (GMT_Destroy_Options (API, &options)) mexErrMsgTxt ("GMT: Failure to destroy GMT5 options\n");
	return;
}
