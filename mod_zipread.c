/*
 *  mod_zipread.c -- Apache zipread module
 *
 * Apache2 API compliant module, version 0.1 (2003)
 *
 * Copyright (c) 2003 Benoit Artuso, All rights reserved.
 * Distributed under GNU GENERAL PUBLIC LICENCE see copying for more information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *
 * This module permits to browse ZIP archive and to read contents of a file
 *
 * This version use ZZIPLIB
 *
 */

#include <zzip/file.h>
#include <zzip/zzip.h>

#include "httpd.h"
#include "http_request.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"
#include "ap_config.h"
#include "apr_strings.h"

#include "util_script.h"

module AP_MODULE_DECLARE_DATA zipread_module;

typedef struct zipread_config_struct {
	     apr_array_header_t *index_names;
} zipread_config_rec;

static const char *zipread_addindex(cmd_parms *cmd, void *dummy, const char *arg)
{
	zipread_config_rec *d = dummy;

	if (!d->index_names) {
		d->index_names = apr_array_make(cmd->pool, 2, sizeof(char *));
	}
	*(const char **)apr_array_push(d->index_names) = arg;
	return NULL;
}

static const command_rec zipread_cmds[] =
{
	AP_INIT_ITERATE("ZipReadDirIndex", zipread_addindex, NULL, OR_INDEXES, "a list of file names to handle as indexes"),
	{NULL}
};

static void *create_zipread_config(apr_pool_t *p, char *dummy)
{
	zipread_config_rec *new = apr_pcalloc(p, sizeof(zipread_config_rec));
	new->index_names = NULL;
	return((void *) new);
}

static void *merge_zipread_configs(apr_pool_t *p, void *basev, void *addv)
{
	zipread_config_rec *new = apr_pcalloc(p, sizeof(zipread_config_rec));
	zipread_config_rec *base = (zipread_config_rec *) basev;
	zipread_config_rec *add = (zipread_config_rec *) addv;
	new->index_names = add->index_names ? add->index_names : base->index_names;
	return((void *) new);
}

// zipread_getcontenttype : given a file name, try to find the content-type associated
// Note : if ap_sub_req_lookup_file is given a filename without '/', it will NOT try to locate it on disk

char * zipread_getcontenttype(request_rec * r, char *pi)
{
	char *p;
	request_rec *newRequest;

	if ((p = ap_strrchr (pi, '/')))
		p++;
	else
		p=pi;

	newRequest = ap_sub_req_lookup_file (p, r, NULL);

	if (newRequest->content_type)
		return((char *)newRequest->content_type);
	return("text/plain");
}

// zipread_showfile : send a file (with its correct content-type) to the browser

static int zipread_showfile(request_rec * r, char *fname)
{
	char *zipfile,*name;
	ZZIP_DIR *dir;

	if (!r->path_info) return(HTTP_NOT_FOUND);
	zipfile = r->filename;
	if ((!fname) || (!*fname))
	{
		name = apr_pstrdup(r->pool, r->path_info);
	}
	else
	{
		name = apr_pstrcat(r->pool, r->path_info, fname, NULL);
		//ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Trying to read : %s %s %s", name, r->path_info, fname);
	}

	r->content_type = zipread_getcontenttype(r, name);
	name ++;

	dir = zzip_dir_open (zipfile, 0);
	if (dir)
	{
		ZZIP_FILE *fp = zzip_file_open (dir, name, ZZIP_CASELESS);
		if (fp)
		{
			int len;
			char buf[ZZIP_32K];
			while ((len = zzip_file_read (fp, buf, ZZIP_32K)))
			{
				ap_rwrite (buf, len, r);
			}
			zzip_file_close (fp);
		}
		else return(HTTP_NOT_FOUND);
		zzip_dir_close (dir);
	}
	else return(HTTP_NOT_FOUND);
	return(OK);
}

// zipread_cmp : helper function for use with qsort

int zipread_cmp(const void *c1, const void *c2)
{
	return(strcmp(*(char **)c1,*(char **)c2));
}

// zipread_showheader : send the first line of the directory index to the browser

void zipread_showheader(request_rec * r, char *fn)
{
	ap_rputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n<html>\n<head>\n",r);
	ap_rprintf(r,"<TITLE>Index of %s</TITLE>\n", fn);
	ap_rputs("</head>\n<body bgcolor=\"#ffffff\" text=\"#000000\">\n\n<table><tr><td bgcolor=\"#ffffff\" class=\"title\">\n<font size=\"+3\" face=\"Helvetica,Arial,sans-serif\">\n",r);
	ap_rprintf(r,"<b>Index of %s</b></font>\n", fn);
	ap_rputs("</td></tr></table><pre>",r);
}

// zipread_showentry : show one line in the directory listing

void zipread_showentry(request_rec * r, char *list, int dir_found, int pi_start)
{
	char *p1;
	char *dir;

	if ((p1 = ap_strrchr(list, '/')))
		p1 ++;
	else
		p1 = list;

	if (dir_found)
	{
		dir = "/";
		ap_rputs("<img src=\"/icons/folder.gif\" alt=\"[DIR]\" />",r);
	}
	else
	{
		dir = "";
		ap_rputs("<img src=\"/icons/unknown.gif\" alt=\"[   ]\" />",r);
	}
	ap_rprintf (r, "<A HREF=\"%s/%s%s\">%s%s</A>\n",apr_pstrndup(r->pool, r->uri, pi_start), list,dir,p1,dir);
}

// zipread_showlist : show the directory contents. filtre is the sub-directory.

static int zipread_showlist(request_rec *r, char *filtre)
{
	char *filename = r->filename;
	char *pathinfo = r->path_info;
	int flag_filtre=0;
	ZZIP_DIR *dir = zzip_dir_open (filename, 0);

	if ((filtre) && (*filtre)) flag_filtre=1;

	if (dir)
	{
		ZZIP_DIRENT dirent;
		int pi_start;
		char *old;
		char *ppdir;
		int i;
		apr_array_header_t * arr = apr_array_make(r->pool, 0, sizeof(char *));
		char **list;

		r->content_type = "text/html";
		zipread_showheader(r,r->uri);
		ppdir = apr_pstrdup(r->pool, "");
		if ((pathinfo) && (strlen(pathinfo) >=2))
		{
			int i;
			for (i = strlen(pathinfo)-2 ; i >= 1 ; i--)
			{
				if (pathinfo[i] == '/')
				{
					ppdir = apr_pstrndup(r->pool, pathinfo, i);
					break;
				}
			}
		}
		old = "";

		// find the start of pathinfo in r->uri
		if (pathinfo)
			pi_start = ap_find_path_info(r->uri, r->path_info);
		else
			pi_start = strlen(r->uri);

		ap_rprintf(r,"<img src=\"/icons/back.gif\" alt=\"[BCK]\" /><A HREF=\"%s%s/\">%s</A>\n",apr_pstrndup(r->pool, r->uri, pi_start), ppdir,"Parent Directory");
		while (zzip_dir_read (dir, &dirent))
		{
			if ((flag_filtre == 0) || (!strncmp (dirent.d_name, filtre, strlen (filtre) )))
			{
				char **new;
				new = (char **) apr_array_push(arr);
				*new = apr_pstrdup(r->pool, dirent.d_name);

			}
		}
		list = (char **)arr->elts;

		// Sort the list of files we contruscted
		qsort((void *)list, arr->nelts, sizeof(char *), zipread_cmp);

		// Show the list of files and remove the duplicates
		for (i=0; i < arr->nelts; i++)
		{
			int dir_found=0;
			char *p1;
			int decalage = 0;
			
			if (flag_filtre) decalage = strlen(filtre);
			if ((p1 = strchr(list[i] + decalage + 1, '/')))
			{
				dir_found=1;
				*(p1+1)='\0';
			}
			if (strcmp(list[i],old))
			{
				if (list[i][strlen(list[i])-1] == '/')
				{
					dir_found = 1;
					if (filtre)
						if (!strcmp(list[i], filtre)) continue;
					list[i][strlen(list[i])-1] = '\0';
				}
				zipread_showentry(r, list[i], dir_found, pi_start);
				list[i][strlen(list[i])] = '/';
				old = apr_pstrdup(r->pool, list[i]);
			}
		}
		ap_rputs("<hr /></pre>mod_zipread on Apache 2.0 (build on "__DATE__ " " __TIME__ ")\n</body></html>", r);
		zzip_dir_close (dir);
	}
	return(OK);
}

// zipread_handler : generate a html page OR to send the file requested

static int zipread_handler (request_rec * r)
{
	char *pathinfo = r->path_info;
	char *filtre=NULL;
	int flag_filtre = 0;

	if (strcmp (r->handler, "zipread"))
		return DECLINED;

	// We must decide what to do : show the directory, show a file

	if ((pathinfo) && (pathinfo[0]) && (pathinfo[0] == '/'))
	{
		int len = strlen (pathinfo);
		if (pathinfo[len - 1] == '/')
		{
			if (len >1)
			{
			  int i;
			  zipread_config_rec *cfg = (zipread_config_rec *)ap_get_module_config(r->per_dir_config, &zipread_module);
			  filtre = apr_pstrdup(r->pool, pathinfo+1);
			  if (cfg->index_names)
			  {
				  char **list = (char **)cfg->index_names->elts;
				  for (i=0; i < cfg->index_names->nelts; i++)
					  if (zipread_showfile(r, list[i]) == OK) return(OK);
			  }
			  else
				  if ((zipread_showfile(r,"index.html") == OK) || (zipread_showfile(r,"index.htm") == OK))
					  return(OK);
			  /* zipread_showlist(r,filtre);*/
			  flag_filtre = 1;
			}
		}
		else
		{
			return(zipread_showfile(r,NULL));
		}
	}
	else
	{
		/* zipread_showlist(r,"");*/
	}
	if (r->header_only) return(OK);
	return(zipread_showlist(r,filtre));
}

static void zipread_register_hooks (apr_pool_t * p)
{
	ap_hook_handler (zipread_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA zipread_module = {
	STANDARD20_MODULE_STUFF,
	create_zipread_config,				/* create per-dir    config structures */
	merge_zipread_configs,				/* merge  per-dir    config structures */
	NULL,				/* create per-server config structures */
	NULL,				/* merge  per-server config structures */
	zipread_cmds,				/* table of config file commands       */
	zipread_register_hooks	/* register hooks                      */
};
