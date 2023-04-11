/******************************************************************************

                               Copyright (c) 2011
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/**
   \file lib_cli_core.c
   Command Line Interface - core functions
*/

#include <stdio.h>
#include <string.h>

#include "ifx_types.h"
#include "lib_cli_config.h"
#include "lib_cli_osmap.h"
#include "lib_cli_debug.h"
#include "lib_cli_core.h"

#ifdef CLI_STATIC
#	undef CLI_STATIC
#endif

#ifdef CLI_DEBUG
#	define CLI_STATIC
#else
#	define CLI_STATIC   static
#endif

/** max number of user specific groups */
#define CLI_MAX_USER_GROUPS 15

/** max number of user search keys */
#define CLI_MAX_USER_GROUP_KEYS 15


/** selection mask - device */
#define CLI_USER_MASK_MISC	0x00008000

#define CLI_GROUP_MISC_DESCRIPTION "CLI commands (misc)"
#define CLI_GROUP_BUILTIN_DESCRIPTION "CLI built-in commands"

/** detailed information (-h) */
#define CLI_INTERN_MASK_DETAILED	0x00080000
/** long form of the command */
#define CLI_INTERN_MASK_LONG		0x00040000
/** short form of the command */
#define CLI_INTERN_MASK_SHORT		0x00020000
/** short form of the command */
#define CLI_INTERN_MASK_BUILTIN		0x80000000


#define CLI_EMPTY_CMD " "
#define CLI_EMPTY_CMD_HELP "n/a"

#define CLI_DEBUG_ERR	printf
#define CLI_DEBUG_WRN	printf

#define CLI_CORE_CTX_CHECK(P_CORE_CTX) \
	do {\
		if (P_CORE_CTX) \
			{if ((P_CORE_CTX)->ctx_size != sizeof(struct cli_core_context_s)) {return IFX_ERROR;} }\
		else { return IFX_ERROR;}\
	} while (0)

/** 'less then' definition for binary tree, (a < b)*/
#define comp_lt(a,b) (clios_strcmp(a,b) < 0)
/** 'equal' definition for binary tree, (a == b)*/
#define comp_eq(a,b) (clios_strcmp(a,b) == 0)


/**
   Collects the printout functions for the available output modes
*/
union cli_cmd_user_fct_u
{
#if (CLI_SUPPORT_BUFFER_OUT == 1)
	/** printout to a buffer */
	cli_cmd_user_fct_buf_t buf_out;
#endif
#if (CLI_SUPPORT_FILE_OUT == 1)
	/** printout to a file */
	cli_cmd_user_fct_file_t file_out;
#endif
};

/** user data stored in tree */
struct cli_cmd_data_s
{
	IFX_char_t const *help;
	IFX_uint32_t mask;
	union cli_cmd_user_fct_u user_fct;
};

struct cli_tree_print_buffer_s
{
	unsigned int buf_size;
	char *p_out;
};

/** CLI cmd core - binary tree node element */
struct cli_cmd_node_s
{
	/** left child */
	struct cli_cmd_node_s *p_left;
	/** right child */
	struct cli_cmd_node_s *p_right;
	/** parent */
	struct cli_cmd_node_s *p_parent;
	/** key used for searching */
	IFX_char_t const *p_key;
	/** user cmd data */
	struct cli_cmd_data_s cmd_data;
};

const char g_cli_version_what[] = CLI_WHAT_STR;
CLI_PRN_MODULE_CREATE(LIB_CLI, CLI_PRN_LOW);

#if defined(LINUX) && defined(__KERNEL__)
long cli_strtol(const char *cp, char **endp, unsigned int base)
{
	if (cp && cp[0] != 0 && cp[1] != 0 && cp[0] == '0' && cp[1] == 'x')
		base = 16;

	return simple_strtol(cp, endp, base);
}

long long cli_strtoll(const char *cp, char **endp, unsigned int base)
{
	long long res;

	if (cp && cp[0] != 0 && cp[1] != 0 && cp[0] == '0' && cp[1] == 'x')
		base = 16;
	strict_strtoll(cp, base, &res);
	return res;
}

unsigned long cli_strtoul(const char *cp, char **endp, unsigned int base)
{
	if (cp && cp[0] != 0 && cp[1] != 0 && cp[0] == '0' && cp[1] == 'x')
		base = 16;

	return simple_strtoul(cp, endp, base);
}

unsigned long long cli_strtoull(const char *cp, char **endp, unsigned int base)
{
	if (cp && cp[0] != 0 && cp[1] != 0 && cp[0] == '0' && cp[1] == 'x')
		base = 16;

	return simple_strtoull(cp, endp, base);
}
#endif	/* #if defined(LINUX) && defined(__KERNEL__) */

CLI_STATIC unsigned int user_group_mask_get(
	struct cli_core_context_s *p_core_ctx,
	unsigned int user_mask,
	char const *p_long_name)
{
	int i, best_match = -1;
	struct cli_group_key_entry_s *p_entry;

	if ((user_mask & CLI_USER_MASK_ALL) & ~p_core_ctx->group.reg_group_mask)
	{
		p_core_ctx->group.not_reg_group_mask |= (user_mask & CLI_USER_MASK_ALL) & ~p_core_ctx->group.reg_group_mask;
		return (user_mask & CLI_USER_MASK_ALL) & ~p_core_ctx->group.reg_group_mask;
	}

	if (user_mask & CLI_USER_MASK_ALL)
		return (user_mask & CLI_USER_MASK_ALL);

	for (i = 0; i < p_core_ctx->group.n_key_entries; i++)
	{
		p_entry = &p_core_ctx->group.p_key_entries[i];
		/* quick check */
		if (*p_long_name != *p_entry->p_search_key)
			continue;
		if (clios_strlen(p_long_name) < clios_strlen(p_entry->p_search_key))
			continue;

		if (clios_strncmp(
			p_entry->p_search_key, p_long_name,
			clios_strlen(p_entry->p_search_key)) == 0)
		{
			/* match */
			if (best_match == -1)
				best_match = i;
			else if (clios_strlen(p_entry->p_search_key) >
				    clios_strlen(p_core_ctx->group.p_key_entries[best_match].p_search_key))
					best_match = i;
		}
	}

	if (best_match == -1)
		return CLI_USER_MASK_MISC;

	return (unsigned int)(0x1 << p_core_ctx->group.p_key_entries[best_match].group_num);
}

/**
   allocate node for data and insert in tree
*/
CLI_STATIC enum cli_cmd_status_e cmd_core_key_insert(
	struct cli_cmd_core_s *p_cmd_core,
	char const *p_key,
	struct cli_cmd_data_s *p_cmd_data)
{
	struct cli_cmd_node_s *x, *curr, *parent;

	if (p_key == IFX_NULL)
		return e_cli_status_key_invalid;

	/* find future parent */
	curr = p_cmd_core->p_root;
	parent = IFX_NULL;
	while (curr)
	{
		if ((!comp_eq(p_key, CLI_EMPTY_CMD)) && comp_eq(p_key, curr->p_key))
			return e_cli_status_duplicate_key;
		parent = curr;
		curr = comp_lt(p_key, curr->p_key) ? curr->p_left : curr->p_right;
	}

	/* setup new node */
	if ((p_cmd_core->p_node_array != IFX_NULL) &&
	    (p_cmd_core->n_used_nodes < p_cmd_core->n_nodes) &&
	    (p_cmd_core->e_state == e_cli_cmd_core_allocate))
	{
		x = (p_cmd_core->p_node_array + p_cmd_core->n_used_nodes);
		p_cmd_core->n_used_nodes++;
	}
	else
	{
		return e_cli_status_mem_exhausted;
	}
	x->p_parent = parent;
	x->p_left = IFX_NULL;
	x->p_right = IFX_NULL;
	x->p_key = p_key;
	clios_memcpy((void *)&x->cmd_data, (void *)p_cmd_data, sizeof(struct cli_cmd_data_s));


	/* insert x in tree */
	if (parent)
		if (comp_lt(x->p_key, parent->p_key))
			parent->p_left = x;
		else
			parent->p_right = x;
	else
		p_cmd_core->p_root = x;

	return e_cli_status_ok;
}

/**
   find node containing data
*/
CLI_STATIC enum cli_cmd_status_e cmd_core_key_find(
	struct cli_cmd_core_s *p_cmd_core,
	char const *p_key,
	struct cli_cmd_data_s *p_cmd_data)
{
	struct cli_cmd_node_s *curr = p_cmd_core->p_root;

	while (curr != IFX_NULL)
	{
		if (comp_eq(p_key, curr->p_key))
		{
			clios_memcpy((void *)p_cmd_data, (void *)&curr->cmd_data,
			       sizeof(struct cli_cmd_data_s));
			return e_cli_status_ok;
		}
		else
		{
			curr = comp_lt(p_key, curr->p_key) ? curr->p_left : curr->p_right;
		}
	}
	return e_cli_status_key_not_found;
}

CLI_STATIC int cmd_core_key_add(
	struct cli_cmd_core_s *p_cmd_core,
	unsigned int group_mask,
	char const *p_short_name,
	char const *p_long_name,
	cli_cmd_user_fct_buf_t cli_cmd_user_fct_buf,
	cli_cmd_user_fct_file_t cli_cmd_user_fct_file)
{
	struct cli_cmd_data_s cmd_data;

	if (p_cmd_core->e_state == e_cli_cmd_core_count)
	{
		p_cmd_core->n_nodes += 2;
		return IFX_SUCCESS;
	}

	if (p_cmd_core->e_state != e_cli_cmd_core_allocate)
		return IFX_ERROR;

	cmd_data.mask = group_mask;

	if (p_short_name == IFX_NULL)
	{
		CLI_DEBUG_ERR("p_short_name pointer is invalid");
		return IFX_ERROR;
	}

	if (p_long_name == IFX_NULL)
	{
		CLI_DEBUG_ERR("p_long_name pointer is invalid");
		return IFX_ERROR;
	}

	switch(p_cmd_core->out_mode)
	{
	case cli_cmd_core_out_mode_buffer:
#		if (CLI_SUPPORT_BUFFER_OUT == 1)
		if (!cli_cmd_user_fct_buf)
		{
			CLI_DEBUG_ERR("ERROR CLI key add, missing fct ptr (buffer out)");
			return IFX_ERROR;
		}
		cmd_data.user_fct.buf_out = cli_cmd_user_fct_buf;
		break;
#		else
		/* not supported */
		CLI_DEBUG_ERR("ERROR - CLI key add, buffer out mode not supported");
		return IFX_ERROR;
#		endif
	case cli_cmd_core_out_mode_file:
#		if (CLI_SUPPORT_FILE_OUT == 1)
		if (!cli_cmd_user_fct_file)
		{
			CLI_DEBUG_ERR("ERROR CLI key add, missing fct ptr (file out)");
			return IFX_ERROR;
		}
		cmd_data.user_fct.file_out = cli_cmd_user_fct_file;
		break;
#		else
		/* not supported */
		CLI_DEBUG_ERR("ERROR - CLI key add, file out mode not supported");
		return IFX_ERROR;
#		endif
	default:
		return IFX_ERROR;
	}
	cmd_data.help = p_long_name;

	switch (cmd_core_key_insert(p_cmd_core, p_short_name, &cmd_data))
	{
	case e_cli_status_key_invalid:
		CLI_DEBUG_ERR("invalid key %s for %s", p_short_name, cmd_data.help);
		return IFX_ERROR;
	case e_cli_status_duplicate_key:
		if (!comp_eq(p_short_name, CLI_EMPTY_CMD))
			CLI_DEBUG_WRN("duplicate key %s for %s", p_short_name, cmd_data.help);
		/* this is non fatal error ;-)
		   return IFX_ERROR; */
		break;
	case e_cli_status_mem_exhausted:
		CLI_DEBUG_ERR("memory allocation error");
		return IFX_ERROR;
	case e_cli_status_ok:
		break;
	default:
		CLI_DEBUG_ERR("insert key aborted");
		return IFX_ERROR;
	}

	cmd_data.mask |= CLI_INTERN_MASK_LONG;
	cmd_data.help = p_short_name;

	switch (cmd_core_key_insert(p_cmd_core, p_long_name, &cmd_data))
	{
	case e_cli_status_key_invalid:
		CLI_DEBUG_ERR("invalid key %s for %s", p_long_name, cmd_data.help);
		return IFX_ERROR;
	case e_cli_status_duplicate_key:
		CLI_DEBUG_ERR("duplicate key %s for %s", p_long_name, cmd_data.help);
		return IFX_ERROR;
	case e_cli_status_mem_exhausted:
		CLI_DEBUG_ERR("memory allocation error");
		return IFX_ERROR;
	case e_cli_status_ok:
		break;
	default:
		CLI_DEBUG_ERR("insert key aborted");
		return IFX_ERROR;
	}

	return IFX_SUCCESS;
}

/* Detect spaces. */
CLI_STATIC int cli_isspace(char c)
{
	if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
		return 1;
	return IFX_SUCCESS;
}

/**
   Remove leading, tailing and double whitespaces
   Arguments has to have only one space in between to get sure that the CLI
   comments are working correctly.
*/
CLI_STATIC char *remove_whitespaces(char *str)
{
	char *buf;
	char *rd, *wr;
	int i = 0;

	/* remove leading whitespaces */
	for (buf = str; buf && *buf && cli_isspace(*buf); ++buf) {
		;
	}

	/* remove double spaces in between and at the end */
	rd = buf;
	wr = buf;
	for (i = 0; wr && rd && *rd != '\0'; ++rd) {
		if (cli_isspace(*rd)) {
			if ((i == 0) && (*(rd + 1) != '\0')) {
				*wr = *rd;
				wr++;
				i++;
			}
		} else {
			i = 0;
			*wr = *rd;
			wr++;
		}
	}

	/* Write string termination */
	if (wr && (wr != rd))
		*wr = '\0';

	return buf;
}

/**
   Remove leading, tailing and double whitespaces
   Arguments has to have only one space in between to get sure that the CLI
   comments are working correctly.
*/
CLI_STATIC char *remove_prefix(char *str, char *prefix, int length)
{
	char *buf;
	char *rd, *wr;
	int i = 0, l = 0;

	buf = str;

	/* remove prefix at front */
	for (rd = buf, wr = buf; wr && rd && *rd != '\0';
	     ++rd) {
		if (l < length) {
			/* prefix check */
			if (*rd == prefix[l]) {
				l++;
			} else {
				for (i = 0; i < l; i++) {
					*wr = prefix[i];
					wr++;
				}
				l = length;

				*wr = *rd;
				wr++;
			}
		} else {
			*wr = *rd;
			wr++;
		}
	}

	/* Write string termination */
	if (wr && (wr != rd))
		*wr = '\0';

	return buf;
}

CLI_STATIC int cmd_check_help(
	const char *p_cmd)
{
	if ( p_cmd &&
	    (clios_strstr(p_cmd, "-h") || clios_strstr(p_cmd, "--help") ||
	     clios_strstr(p_cmd, "/h") || clios_strstr(p_cmd, "-?")))
	{
		return 1;
	}
	else
	{
		return IFX_SUCCESS;
	}
}

CLI_STATIC int core_out_mode_check(
	enum cli_cmd_core_out_mode_e out_mode)
{
	switch(out_mode)
	{
	case cli_cmd_core_out_mode_buffer:
#		if (CLI_SUPPORT_BUFFER_OUT == 1)
		break;
#		else
		/* not supported */
		return IFX_ERROR;
#		endif
	case cli_cmd_core_out_mode_file:
#		if (CLI_SUPPORT_FILE_OUT == 1)
		break;
#		else
		/* not supported */
		return IFX_ERROR;
#		endif
	default:
		return IFX_ERROR;
	}
	return IFX_SUCCESS;
}

CLI_STATIC int cmd_parse_execute(
	struct cli_core_context_s *p_core_ctx,
	char *p_cmd_arg_buf,
	const unsigned int buffer_size,
	clios_file_t *p_file)
{
	struct cli_cmd_data_s cmd_data = {IFX_NULL, 0, {IFX_NULL}};
	char *p_cmd = IFX_NULL;
	char *p_arg = IFX_NULL;

	if (p_cmd_arg_buf == IFX_NULL)
		return IFX_ERROR;

	p_cmd_arg_buf = remove_whitespaces(p_cmd_arg_buf);
	p_cmd_arg_buf = remove_prefix(p_cmd_arg_buf, "CLI_", 3);

	p_cmd = p_cmd_arg_buf;
	p_arg = p_cmd_arg_buf;

	/* strip the command word */
	while (*p_arg) {
		if (cli_isspace(*p_arg)) {
			*p_arg = 0;
			p_arg++;
			break;
		}
		p_arg++;
	}

	switch (cmd_core_key_find(&p_core_ctx->cmd_core, p_cmd, &cmd_data))
	{
	case e_cli_status_ok:
		switch(p_core_ctx->cmd_core.out_mode)
		{
		case cli_cmd_core_out_mode_buffer:
#			if (CLI_SUPPORT_BUFFER_OUT == 1)
			if (cmd_data.user_fct.buf_out != IFX_NULL)
				return cmd_data.user_fct.buf_out(
					(cmd_data.mask & CLI_INTERN_MASK_BUILTIN) ? p_core_ctx : p_core_ctx->p_user_fct_data,
					p_arg, buffer_size, p_cmd_arg_buf);
			else
				return clios_sprintf(p_cmd_arg_buf,
					"errorcode=-1 (internal error, no function pointer)");
#			else
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: parse & exec - buffer printout not supported" CLI_CRLF));
			return IFX_ERROR;
#			endif
		case cli_cmd_core_out_mode_file:
#			if (CLI_SUPPORT_FILE_OUT == 1)
			if (cmd_data.user_fct.file_out != IFX_NULL)
				return cmd_data.user_fct.file_out(
					(cmd_data.mask & CLI_INTERN_MASK_BUILTIN) ? p_core_ctx : p_core_ctx->p_user_fct_data,
					p_arg, p_file);
			else
				return clios_fprintf(p_file,
					"errorcode=-1 (internal error, no function pointer)");
#			else
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: parse & exec - file printout not supported" CLI_CRLF));
			return IFX_ERROR;
#			endif
		default:
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: parse & exec - printout mode unknown" CLI_CRLF));
			return IFX_ERROR;
		}
	default:
		switch(p_core_ctx->cmd_core.out_mode)
		{
		case cli_cmd_core_out_mode_buffer:
#			if (CLI_SUPPORT_BUFFER_OUT == 1)
			return clios_sprintf(p_cmd_arg_buf, "errorcode=-1 (unknown command)");
#			else
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: parse & exec - unknown command, buffer printout not supported" CLI_CRLF));
			return IFX_ERROR;
#			endif
		case cli_cmd_core_out_mode_file:
#			if (CLI_SUPPORT_FILE_OUT == 1)
			return clios_sprintf(p_cmd_arg_buf, "errorcode=-1 (unknown command)");
#			else
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: parse & exec - unknown command, file printout not supported" CLI_CRLF));
			return IFX_ERROR;

#			endif
		default:
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: parse & exec - unknown command, unknown printout mode" CLI_CRLF));
			return IFX_ERROR;
		}
	}

	return IFX_SUCCESS;
}

#if (CLI_SUPPORT_BUFFER_OUT == 1)
/*
   print binary tree
*/
CLI_STATIC int cmd_core_tree_print__buffer(
	struct cli_cmd_node_s *p_node,
	unsigned int mask,
	struct cli_tree_print_buffer_s *p_buffer)
{
	int j = 0, ret;
	int nFillChar = 0;
	int nHelpClm = 10;

	if (p_node == IFX_NULL)
		return IFX_SUCCESS;

	cmd_core_tree_print__buffer(p_node->p_left, mask, p_buffer);

	if ((p_buffer->buf_size + 64) >= p_buffer->buf_size)
		return IFX_SUCCESS;

	if ((p_node->cmd_data.mask & mask)
	    && ((p_node->cmd_data.mask & CLI_INTERN_MASK_LONG) == (mask & CLI_INTERN_MASK_LONG))) {
		if ((mask & CLI_INTERN_MASK_DETAILED) == CLI_INTERN_MASK_DETAILED) {
			if (p_node->cmd_data.user_fct.buf_out) {
				ret = p_node->cmd_data.user_fct.buf_out(
					IFX_NULL, "-h", p_buffer->buf_size, p_buffer->p_out);
				if (ret < 0)
					ret = 0;
				p_buffer->buf_size -= ret;
				p_buffer->p_out += ret;
			}
		} else {
			ret = clios_sprintf(p_buffer->p_out, "%s", p_node->cmd_data.help);
			if (ret < 0)
				ret = 0;
			p_buffer->buf_size -= ret;
			p_buffer->p_out += ret;
			nFillChar = nHelpClm - ret;
			if (nFillChar > 0) {
				for (j = 0; j < nFillChar; j++) {
					ret = clios_sprintf(p_buffer->p_out, " ");
					if (ret < 0)
						ret = 0;
					p_buffer->buf_size -= ret;
					p_buffer->p_out += ret;
				}
			}
			if (clios_strcmp(p_node->p_key, CLI_EMPTY_CMD) == 0) {
				ret = clios_sprintf(p_buffer->p_out, CLI_EMPTY_CMD_HELP CLI_CRLF);
			} else {
				ret = clios_sprintf(p_buffer->p_out, "%s" CLI_CRLF, p_node->p_key);
			}
			if (ret < 0)
				ret = 0;
			p_buffer->buf_size -= ret;
			p_buffer->p_out += ret;
		}
	}

	return cmd_core_tree_print__buffer(p_node->p_right, mask, p_buffer);
}
#endif /* #if (CLI_SUPPORT_BUFFER_OUT == 1) */

#if (CLI_SUPPORT_FILE_OUT == 1)
/*
   print binary tree
*/
CLI_STATIC void cmd_core_tree_print__file(
	struct cli_cmd_node_s *p_node,
	unsigned int mask,
	unsigned int *p_out_size,
	clios_file_t *p_file)
{
	int ret = 0;
	int j = 0;
	int ch = 0, fill_char = 0;
	int help_clm = 18;

	if (p_node == IFX_NULL)
		return;

	cmd_core_tree_print__file(p_node->p_left, mask, p_out_size, p_file);

	if (p_node->cmd_data.mask & mask) {
		if ((mask & CLI_INTERN_MASK_DETAILED) == CLI_INTERN_MASK_DETAILED) {
			/* don't print twice */
			if ((p_node->cmd_data.mask & CLI_INTERN_MASK_LONG) == 0)
			{
				if (p_node->cmd_data.user_fct.file_out)
					ret = p_node->cmd_data.user_fct.file_out(IFX_NULL, "-h", p_file);
					if (ret < 0)
						ret = 0;
					*p_out_size += (unsigned int)ret;
			}
		} else {
#ifdef CLI_INTERN_MASK_LONG
			if (p_node->cmd_data.mask & CLI_INTERN_MASK_LONG)
				help_clm *= 3;

			if ((p_node->cmd_data.mask & CLI_INTERN_MASK_LONG) == (mask & CLI_INTERN_MASK_LONG))
#endif
			{
				if (clios_strcmp(p_node->p_key, CLI_EMPTY_CMD) == 0) {
					ch = clios_fprintf(p_file, "   %s,", CLI_EMPTY_CMD_HELP);
				} else {
					ch = clios_fprintf(p_file, "   %s,", p_node->p_key);
				}
				if (ch < 0)
					ch = 0;
				*p_out_size += (unsigned int)ch;
				fill_char = help_clm - ch;
				if (fill_char > 0)
				{
					for (j = 0; j < fill_char; j++)
						(void)clios_fprintf(p_file, " ");
					*p_out_size += (unsigned int)fill_char;
				}
				ret = clios_fprintf(p_file,
					"%s\n", p_node->cmd_data.help);
				if (ret > 0)
					*p_out_size += (unsigned int)ret;
			}
		}
	}

	cmd_core_tree_print__file(p_node->p_right, mask, p_out_size, p_file);
}
#endif /* #if (CLI_SUPPORT_FILE_OUT == 1) */

#if (CLI_SUPPORT_BUFFER_OUT == 1)
/** Build in command - help */
CLI_STATIC int cli_core_cmd_help__buffer(
	void *p_ctx,
	const char *p_cmd,
	const unsigned int bufsize_max,
	char *p_out)
{
	int ret = 0;
	struct cli_core_context_s *p_core_ctx = p_ctx;
	unsigned int mask = CLI_USER_MASK_MISC | CLI_INTERN_MASK_LONG;
	struct cli_tree_print_buffer_s buffer;

#ifndef CLI_DEBUG_DISABLE
	static const char USAGE[] =
		"Long Form: Help" CLI_CRLF "Short Form: help" CLI_CRLF
		CLI_CRLF
		"Input Parameter" CLI_CRLF
		CLI_CRLF
		"Output Parameter" CLI_CRLF
		"- int errorcode" CLI_CRLF CLI_CRLF;
#else
#undef USAGE
#define USAGE ""
#endif
	if ((ret = cli_check_help__buffer(p_cmd, USAGE, bufsize_max, p_out)) >= 0)
		return ret;

	if (clios_strlen(p_cmd))
	{
		if (clios_strcmp(p_cmd, "detailed") == 0)
			mask |= CLI_INTERN_MASK_DETAILED;
		else
			return clios_sprintf(p_out,
				"errorcode=-1 (unknown sub command)" CLI_CRLF);
	}

	buffer.buf_size = bufsize_max;
	buffer.p_out = p_out;

	cmd_core_tree_print__buffer(p_core_ctx->cmd_core.p_root, mask, &buffer);

	return (int)buffer.buf_size;
}
#endif /* #if (CLI_SUPPORT_BUFFER_OUT == 1) */

#if (CLI_SUPPORT_FILE_OUT == 1)
CLI_STATIC int cli_core_cmd_help__file(
	void *p_ctx,
	const char *p_cmd,
	clios_file_t *p_file)
{
	int ret = 0, i;
	struct cli_core_context_s *p_core_ctx = p_ctx;
	unsigned int out_size = 0, mask = CLI_INTERN_MASK_SHORT;

#ifndef CLI_DEBUG_DISABLE
	static const char USAGE[] =
		"Long Form: Help" CLI_CRLF "Short Form: help" CLI_CRLF
		CLI_CRLF
		"Input Parameter" CLI_CRLF
		CLI_CRLF
		"Output Parameter" CLI_CRLF
		"- int errorcode" CLI_CRLF CLI_CRLF;
#else
#undef USAGE
#define USAGE ""
#endif
	if ((ret = cli_check_help__file(p_cmd, USAGE, p_file)) > 0)
		return ret;

	if (clios_strlen(p_cmd))
	{
		if (clios_strcmp(p_cmd, "detailed") == 0)
			mask |= CLI_INTERN_MASK_DETAILED;
		else
			return clios_fprintf(p_file,
				"errorcode=-1 (unknown sub command)" CLI_CRLF);
	}

	if (p_core_ctx->group.n_reg_groups > 0)
	{
		for (i = 0; i < p_core_ctx->group.n_reg_groups; i++)
		{
			out_size += clios_fprintf(p_file,
					"== Group: <%s> ===" CLI_CRLF, p_core_ctx->group.pp_group_list[i]);
			cmd_core_tree_print__file(p_core_ctx->cmd_core.p_root, mask | (0x1 << i), &out_size, p_file);
		}
	}

	if(p_core_ctx->group.not_reg_group_mask != 0)
	{
		unsigned int reg_mask = p_core_ctx->group.not_reg_group_mask;
		for (i = 0; i < 16; i++)
		{
			if (reg_mask == 0)
				break;

			if (!(reg_mask & (0x1 << i)))
				continue;

			out_size += clios_fprintf(p_file,
					"== Group: <not registered cmds> ===" CLI_CRLF);
			cmd_core_tree_print__file(p_core_ctx->cmd_core.p_root, mask | (0x1 << i), &out_size, p_file);
			reg_mask &= ~(0x1 << i);
		}
	}

	out_size += clios_fprintf(p_file,
			"== Group: <%s> ===" CLI_CRLF, p_core_ctx->group.p_misc_group);
	cmd_core_tree_print__file(p_core_ctx->cmd_core.p_root, mask | CLI_USER_MASK_MISC, &out_size, p_file);


	out_size += clios_fprintf(p_file,
			"== Group: <%s> ===" CLI_CRLF, p_core_ctx->group.p_builtin_group);
	cmd_core_tree_print__file(p_core_ctx->cmd_core.p_root, mask | CLI_INTERN_MASK_BUILTIN, &out_size, p_file);


	return (int)out_size;
}
#endif	/* #if (CLI_SUPPORT_FILE_OUT == 1) */


#if (CLI_SUPPORT_BUFFER_OUT == 1)
/** Build in command - what */
CLI_STATIC int cli_core_cmd_what__buffer(
	void *p_ctx,
	const char *p_cmd,
	const unsigned int bufsize_max,
	char *p_out)
{
	int ret = 0;

#ifndef CLI_DEBUG_DISABLE
	static const char USAGE[] =
		"Long Form: what_string" CLI_CRLF "Short Form: what" CLI_CRLF
		CLI_CRLF
		"Input Parameter" CLI_CRLF
		CLI_CRLF
		"Output Parameter" CLI_CRLF
		"- int errorcode" CLI_CRLF CLI_CRLF;
#else
#undef USAGE
#define USAGE ""
#endif
	if ((ret = cli_check_help__buffer(p_cmd, USAGE, bufsize_max, p_out)) >= 0)
		return ret;

	ret = clios_snprintf(p_out, bufsize_max, "%s" CLI_CRLF, &g_cli_version_what[4]);

	return ret;
}
#endif /* #if (CLI_SUPPORT_BUFFER_OUT == 1) */

#if (CLI_SUPPORT_FILE_OUT == 1)
CLI_STATIC int cli_core_cmd_what__file(
	void *p_ctx,
	const char *p_cmd,
	clios_file_t *p_file)
{
	int ret = 0;

#ifndef CLI_DEBUG_DISABLE
	static const char USAGE[] =
		"Long Form: what_string" CLI_CRLF "Short Form: what" CLI_CRLF
		CLI_CRLF
		"Input Parameter" CLI_CRLF
		CLI_CRLF
		"Output Parameter" CLI_CRLF
		"- int errorcode" CLI_CRLF CLI_CRLF;
#else
#undef USAGE
#define USAGE ""
#endif
	if ((ret = cli_check_help__file(p_cmd, USAGE, p_file)) > 0)
		return ret;

	ret = clios_fprintf(p_file, "%s" CLI_CRLF, &g_cli_version_what[4]);

	return ret;
}
#endif	/* #if (CLI_SUPPORT_FILE_OUT == 1) */


CLI_STATIC void all_user_if_quit(
	struct cli_core_context_s *p_core_ctx)
{
	struct cli_user_context_s *p_user_ctx = p_core_ctx->p_user_head;

	while (p_user_ctx)
	{
		if (p_user_ctx->exit_cb_fct)
			p_user_ctx->exit_cb_fct(p_user_ctx->p_user_data);

		p_user_ctx = p_user_ctx->p_next;
	}
	return;
}

#if (CLI_SUPPORT_BUFFER_OUT == 1)
/** Build in command - help */
CLI_STATIC int cli_core_cmd_quit__buffer(
	void *p_ctx,
	const char *p_cmd,
	const unsigned int bufsize_max,
	char *p_out)
{
	int ret = 0;
	struct cli_core_context_s *p_core_ctx = p_ctx;

#ifndef CLI_DEBUG_DISABLE
	static const char USAGE[] =
		"Long Form: Quit" CLI_CRLF "Short Form: quit" CLI_CRLF
		CLI_CRLF
		"Input Parameter" CLI_CRLF
		CLI_CRLF
		"Output Parameter" CLI_CRLF
		"- int errorcode" CLI_CRLF CLI_CRLF;
#else
#undef USAGE
#define USAGE ""
#endif
	if ((ret = cli_check_help__buffer(p_cmd, USAGE, bufsize_max, p_out)) >= 0)
		return ret;

	all_user_if_quit(p_core_ctx);

	return /* (int)buffer.buf_size */ ret;
}
#endif /* #if (CLI_SUPPORT_BUFFER_OUT == 1) */

#if (CLI_SUPPORT_FILE_OUT == 1)
CLI_STATIC int cli_core_cmd_quit__file(
	void *p_ctx,
	const char *p_cmd,
	clios_file_t *p_file)
{
	int ret = 0;
	struct cli_core_context_s *p_core_ctx = p_ctx;

#ifndef CLI_DEBUG_DISABLE
	static const char USAGE[] =
		"Long Form: Quit" CLI_CRLF "Short Form: quit" CLI_CRLF
		CLI_CRLF
		"Input Parameter" CLI_CRLF
		CLI_CRLF
		"Output Parameter" CLI_CRLF
		"- int errorcode" CLI_CRLF CLI_CRLF;
#else
#undef USAGE
#define USAGE ""
#endif
	if ((ret = cli_check_help__file(p_cmd, USAGE, p_file)) > 0)
		return ret;

	all_user_if_quit(p_core_ctx);

	return ret;
}
#endif	/* #if (CLI_SUPPORT_FILE_OUT == 1) */

int cli_sscanf(const char *buf, char const *fmt, ...)
{
#ifndef _lint
	va_list marker;
#endif
	char const *p;
	char const *a;
	char *r = IFX_NULL;
	const char *s = buf;
	int ret = 0, mode = 32, base = 10, array_cnt = 0, i;
	int *v32 = IFX_NULL;
	short *v16 = IFX_NULL;
	char *v8 = IFX_NULL;
	unsigned int *vu32 = IFX_NULL;
	unsigned short *vu16 = IFX_NULL;
	unsigned char *vu8 = IFX_NULL;
#ifndef __KERNEL__
	float *f = NULL;
#endif

#ifndef _lint
	va_start(marker, fmt);
#endif

	if (s == IFX_NULL) {
		/*lint -save -e(801) */
		goto SF_END;
		/*lint -restore */
	}

	for (p = fmt; *p; p++) {
		if (*p != '%')
			continue;

		if (s == IFX_NULL) {
			/*lint -save -e(801) */
			goto SF_END;
			/*lint -restore */
		}

		/* skip spaces and tabs */
		while ((*s == ' ') || (*s == '\t'))
			s++;

		if (*s == 0) {
			/*lint -save -e(801) */
			goto SF_END;
			/*lint -restore */
		}

		switch (*++p) {
		case 0:
			/* ret = 0; */
			/*lint -save -e(801) */
			goto SF_END;
			/*lint -restore */

			/* 8 bit */
		case 'b':
			mode = 8;
			p++;
			break;

			/* 16 bit */
		case 'h':
			mode = 16;
			p++;
			break;

			/* 32 bit */
		default:
			mode = 32;
			break;
		}

		switch (*p) {
		case 'd':
		case 'i':
		case 'u':
			base = 10;
			break;

		case 'x':
			base = 16;
			break;

		default:
			break;
		}

		a = p + 1;
		i = 0;

		array_cnt = 1;

		switch (*a) {
		case '[':
			a++;
			if (*a) {
#if defined(LINUX) && defined(__KERNEL__)
				array_cnt = (char)simple_strtol(a, IFX_NULL, 10);
#else
				array_cnt = (char)strtol(a, IFX_NULL, 10);
#endif
				if (array_cnt > 256) {
					array_cnt = 0;
				}
				do {
					a++;
					if (*a == ']') {
						a++;
						break;
					}
				} while (*a);
			}
			break;

		default:
			break;
		}

		switch (*p) {
		case 0:
			/* ret = 0; */
			/*lint -save -e(801) */
			goto SF_END;
			/*lint -restore */

#if 1
			/* string */
		case 's':
			{
#ifndef _lint
				r = va_arg(marker, char *);
#endif
				if (r != NULL) {
					const char *q = s;
					do {
						if ((*q == ' ') || (*q == '\t'))
							q++;
						else
							break;
					} while (*q);
					if (*q) {
						do {
							if ((*q != ' ')
							    && (*q != '\t')
							    && (*q != '\n')
							    && (*q != '\r'))
								*r++ = *q++;
							else
								break;
						} while (*q);
						s = q;
						*r = 0;
						ret++;
					}
				}
				break;
			}

#else
			/* string */
		case 's':
			{
#ifndef _lint
				r = va_arg(marker, char *);
#endif
				if (r != IFX_NULL) {
					const char *q = s;

					do {
						if ((*q == ' ') || (*q == '\t'))
							q++;
					} while (*q);

					if (*q) {
						strcpy(r, q);
						ret++;
					}
				}
				break;
			}
#endif

#ifndef __KERNEL__
		case 'f':
			{
				switch(mode) {
				case 32:
#ifndef _lint
					f = va_arg(marker, float *);
#endif
					if (f != NULL) {
						for (i = 0; i < array_cnt; i++)
						{
							char *ptr = NULL;

							f[i] = (float)
							       strtod(s, &ptr);
							s = ptr;
						}
						ret++;
					}
					break;
				default:
					break;
				}
				break;
			}
#endif

			/* signed */
		case 'd':
		case 'i':
			{
				switch (mode) {
				case 8:
#ifndef _lint
					v8 = va_arg(marker, char *);
#endif
					if (v8 != IFX_NULL) {
						for (i = 0; i < array_cnt; i++) {
							char *ptr = IFX_NULL;

#if defined(LINUX) && defined(__KERNEL__)
							v8[i] = (char)
							    simple_strtol(s,
									  &ptr,
									  base);
#else
							v8[i] =
							    (char)strtol(s,
									 &ptr,
									 base);
#endif
							s = ptr;
						}
						ret++;
					}
					break;

				case 16:
#ifndef _lint
					v16 = va_arg(marker, short *);
#endif
					if (v16 != IFX_NULL) {
						for (i = 0; i < array_cnt; i++) {
							char *ptr = IFX_NULL;

#if defined(LINUX) && defined(__KERNEL__)
							v16[i] = (short)
							    simple_strtol(s,
									  &ptr,
									  base);
#else
							v16[i] =
							    (short)strtol(s,
									  &ptr,
									  base);
#endif
							s = ptr;
						}
						ret++;
					}
					break;

				case 32:
#ifndef _lint
					v32 = va_arg(marker, int *);
#endif
					if (v32 != IFX_NULL) {
						for (i = 0; i < array_cnt; i++) {
							char *ptr = IFX_NULL;

#if defined(LINUX) && defined(__KERNEL__)
							v32[i] = (int)
							    simple_strtol(s,
									  &ptr,
									  base);
#else
							v32[i] =
							    (int)strtol(s, &ptr,
									base);
#endif
							s = ptr;
						}
						ret++;
					}
					break;

				default:
					break;
				}
				break;
			}

			/* unsigned */
		case 'u':
			/* hexadecimal */
		case 'x':
			{
				switch (mode) {
				case 8:
#ifndef _lint
					vu8 = va_arg(marker, unsigned char *);
#endif
					if (vu8 != IFX_NULL) {
						for (i = 0; i < array_cnt; i++) {
							char *ptr = IFX_NULL;

#if defined(LINUX) && defined(__KERNEL__)
							vu8[i] = (unsigned char)
							    simple_strtoul(s,
									   &ptr,
									   base);
#else
							vu8[i] = (unsigned char)
							    strtoul(s, &ptr,
								    base);
#endif
							s = ptr;
						}
						ret++;
					}
					break;

				case 16:
#ifndef _lint
					vu16 = va_arg(marker, unsigned short *);
#endif
					if (vu16 != IFX_NULL) {
						for (i = 0; i < array_cnt; i++) {
							char *ptr = IFX_NULL;

#if defined(LINUX) && defined(__KERNEL__)
							vu16[i] =
							    (unsigned short)
							    simple_strtoul(s,
									   &ptr,
									   base);
#else
							vu16[i] =
							    (unsigned short)
							    strtoul(s, &ptr,
								    base);
#endif
							s = ptr;
						}
						ret++;
					}
					break;

				case 32:
#ifndef _lint
					vu32 = va_arg(marker, unsigned int *);
#endif
					if (vu32 != IFX_NULL) {
						for (i = 0; i < array_cnt; i++) {
							char *ptr = IFX_NULL;

#if defined(LINUX) && defined(__KERNEL__)
							vu32[i] = (unsigned int)
							    simple_strtoul(s,
									   &ptr,
									   base);
#else
							vu32[i] = (unsigned int)
							    strtoul(s, &ptr,
								    base);
#endif
							s = ptr;
						}
						ret++;
					}
					break;

				default:
					break;
				}
				break;

			}
		default:
			break;
		}

		if (array_cnt > 1) {
			p = a;
		}
	}

      SF_END:

#ifndef _lint
	va_end(marker);
#endif

	return ret;
}

int cli_core_group_release(
	struct cli_core_context_s *p_core_ctx)
{
	CLI_CORE_CTX_CHECK(p_core_ctx);

	if (p_core_ctx->group.pp_group_list != IFX_NULL)
		clios_memfree((void *)p_core_ctx->group.pp_group_list);

	if (p_core_ctx->group.p_key_entries != IFX_NULL)
		clios_memfree(p_core_ctx->group.p_key_entries);

	clios_memset(&p_core_ctx->group, 0x00, sizeof(struct cli_group_s));

	return IFX_SUCCESS;
}

int cli_core_cfg_cmd_shutdown(
	struct cli_core_context_s *p_core_ctx,
	enum cli_cmd_core_out_mode_e out_mode)
{
	CLI_CORE_CTX_CHECK(p_core_ctx);

	if (core_out_mode_check(out_mode))
	{
		/* invalid outmode */
		return IFX_ERROR;
	}

	(void)clios_lockget(&p_core_ctx->lock_ctx);
	p_core_ctx->cmd_core.e_state = e_cli_cmd_core_delete;
	p_core_ctx->cmd_core.p_root = IFX_NULL;
	if (p_core_ctx->cmd_core.p_node_array)
		clios_memfree(p_core_ctx->cmd_core.p_node_array);

	clios_memset(&p_core_ctx->cmd_core, 0x0, sizeof(struct cli_cmd_core_s));
	p_core_ctx->cmd_core.out_mode = out_mode;
	p_core_ctx->cmd_core.e_state = e_cli_cmd_core_count;
	(void)clios_lockrelease(&p_core_ctx->lock_ctx);

	return IFX_SUCCESS;
}

int cli_core_cfg_mode_cmd_reg(
	struct cli_core_context_s *p_core_ctx)
{
	CLI_CORE_CTX_CHECK(p_core_ctx);

	(void)clios_lockget(&p_core_ctx->lock_ctx);

	if (p_core_ctx->cmd_core.e_state != e_cli_cmd_core_count)
	{
		(void)clios_lockrelease(&p_core_ctx->lock_ctx);
		/* invalid core mode */
		return IFX_ERROR;
	}

	if (p_core_ctx->cmd_core.n_nodes == 0)
	{
		(void)clios_lockrelease(&p_core_ctx->lock_ctx);
		/* no registered commands */
		return IFX_ERROR;
	}

	p_core_ctx->cmd_core.p_node_array =
		clios_memalloc(p_core_ctx->cmd_core.n_nodes * sizeof(struct cli_cmd_node_s));

	if (!p_core_ctx->cmd_core.p_node_array)
	{
		(void)clios_lockrelease(&p_core_ctx->lock_ctx);
		/* mem alloc */
		return IFX_ERROR;
	}
	clios_memset(
		p_core_ctx->cmd_core.p_node_array, 0x0,
		p_core_ctx->cmd_core.n_nodes * sizeof(struct cli_cmd_node_s));

	p_core_ctx->cmd_core.e_state = e_cli_cmd_core_allocate;
	(void)clios_lockrelease(&p_core_ctx->lock_ctx);

	return IFX_SUCCESS;
}

int cli_core_cfg_mode_active(
	struct cli_core_context_s *p_core_ctx)
{
	CLI_CORE_CTX_CHECK(p_core_ctx);

	(void)clios_lockget(&p_core_ctx->lock_ctx);

	if (p_core_ctx->cmd_core.e_state != e_cli_cmd_core_allocate)
	{
		(void)clios_lockrelease(&p_core_ctx->lock_ctx);
		/* invalid core mode */
		return IFX_ERROR;
	}

	if (!p_core_ctx->cmd_core.p_node_array || !p_core_ctx->cmd_core.p_root)
	{
		(void)clios_lockrelease(&p_core_ctx->lock_ctx);
		/* no registered commands */
		return IFX_ERROR;
	}

	p_core_ctx->cmd_core.e_state = e_cli_cmd_core_active;
	(void)clios_lockrelease(&p_core_ctx->lock_ctx);

	return IFX_SUCCESS;
}

int cli_core_release(
	struct cli_core_context_s **pp_core_ctx,
	enum cli_cmd_core_out_mode_e out_mode)
{
	struct cli_core_context_s *p_core_ctx = *pp_core_ctx;

	if (*pp_core_ctx)
	{
		p_core_ctx = *pp_core_ctx;
		*pp_core_ctx = IFX_NULL;

		(void)cli_core_cfg_cmd_shutdown(p_core_ctx, out_mode);

		(void)clios_lockget(&p_core_ctx->lock_ctx);
		(void)cli_core_group_release(p_core_ctx);
		p_core_ctx->ctx_size = 0;
		(void)clios_lockrelease(&p_core_ctx->lock_ctx);
		(void)clios_lockdelete(&p_core_ctx->lock_ctx);
		clios_memfree(p_core_ctx);
	}

	return IFX_SUCCESS;
}

/**
   Setup the CLI core data

\param
   p_core_ctx  CLI Core context pointer
\param
   out_mode  selects the output mode, printout to buffer or file.
\param
   p_user_fct_data  user data, used for the cli user function calls

\return
   IFX_SUCCESS: success
   IFX_ERROR: error occured

\remarks
   Depending on the selected output mode the corresponding user functions must have
   one of the following form:
   \ref cli_cmd_user_fct_buf_t for printout to a buffer.
   \ref cli_cmd_user_fct_file_t for printout to a file.
*/
int cli_core_init(
	struct cli_core_context_s **pp_core_ctx,
	enum cli_cmd_core_out_mode_e out_mode,
	void *p_user_fct_data)
{
	struct cli_core_context_s *p_core_ctx = IFX_NULL;

	if (*pp_core_ctx) {
		/* expect NULL pointer */
		return IFX_ERROR;
	}

	if (core_out_mode_check(out_mode))
	{
		/* invalid outmode */
		return IFX_ERROR;
	}

	p_core_ctx = (struct cli_core_context_s *)
		clios_memalloc(sizeof(struct cli_core_context_s));
	if (!p_core_ctx)
	{
		/* error mem alloc */
		return IFX_ERROR;
	}
	clios_memset(p_core_ctx, 0x0, sizeof(struct cli_core_context_s));
	(void)clios_lockinit(&p_core_ctx->lock_ctx, "cli_core", 0);

	p_core_ctx->cmd_core.out_mode = out_mode;
	p_core_ctx->cmd_core.e_state = e_cli_cmd_core_count;
	p_core_ctx->p_user_fct_data = p_user_fct_data;
	p_core_ctx->ctx_size = sizeof(struct cli_core_context_s);

	p_core_ctx->group.p_misc_group = CLI_GROUP_MISC_DESCRIPTION;
	p_core_ctx->group.p_builtin_group = CLI_GROUP_BUILTIN_DESCRIPTION;

	*pp_core_ctx = p_core_ctx;
	return IFX_SUCCESS;
}

int cli_core_user_group_init(
	struct cli_core_context_s *p_core_ctx,
	char const *p_group_list[],
	struct cli_group_key_entry_s *p_key_entries)
{
	IFX_int_t grp_cnt = 0, key_cnt = 0;
	IFX_uint_t mask = 0;

	CLI_CORE_CTX_CHECK(p_core_ctx);

	if (p_core_ctx->cmd_core.e_state != e_cli_cmd_core_count)
	{
		/* wrong state, already in use */
		return IFX_ERROR;
	}

	(void)cli_core_group_release(p_core_ctx);
	p_core_ctx->group.p_misc_group = CLI_GROUP_MISC_DESCRIPTION;
	p_core_ctx->group.p_builtin_group = CLI_GROUP_BUILTIN_DESCRIPTION;

	grp_cnt = 0;
	while (p_group_list[grp_cnt] != IFX_NULL)
	{
		grp_cnt++;
		if (grp_cnt >= CLI_MAX_USER_GROUPS) /* max 15 user groups */
			return IFX_ERROR;
	}
	if (grp_cnt == 0)
		return IFX_SUCCESS;

	key_cnt = 0;
	if (p_key_entries)
	{
		while (p_key_entries[key_cnt].p_search_key)
		{
			if (p_key_entries[key_cnt].group_num >= (unsigned char)grp_cnt)
			{
				return IFX_ERROR;
			}

			key_cnt++;
			if (key_cnt >= CLI_MAX_USER_GROUP_KEYS)
				return IFX_ERROR;
		}
	}
	if (key_cnt == 0)
		return IFX_SUCCESS;

	p_core_ctx->group.pp_group_list = clios_memalloc(grp_cnt * sizeof(char *));
	p_core_ctx->group.p_key_entries = clios_memalloc(key_cnt * sizeof(struct cli_group_key_entry_s));
	if ((p_core_ctx->group.pp_group_list == IFX_NULL) ||
	    (p_core_ctx->group.p_key_entries == IFX_NULL) )
	{
		(void)cli_core_group_release(p_core_ctx);
		return IFX_ERROR;
	}
	p_core_ctx->group.n_reg_groups = grp_cnt;
	p_core_ctx->group.n_key_entries = key_cnt;

	mask = 0;
	grp_cnt = 0;
	while (p_group_list[grp_cnt] != IFX_NULL)
	{
		p_core_ctx->group.pp_group_list[grp_cnt] = p_group_list[grp_cnt];
		mask |= (0x1 << grp_cnt);
		grp_cnt++;
	}
	p_core_ctx->group.reg_group_mask = mask;

	key_cnt = 0;
	while (p_key_entries[key_cnt].p_search_key != IFX_NULL)
	{
		p_core_ctx->group.p_key_entries[key_cnt].group_num = p_key_entries[key_cnt].group_num;
		p_core_ctx->group.p_key_entries[key_cnt].p_search_key = p_key_entries[key_cnt].p_search_key;
		key_cnt++;
	}

	return IFX_SUCCESS;
}

int cli_core_buildin_register(
	struct cli_core_context_s *p_core_ctx,
	unsigned int select_mask)
{
	CLI_CORE_CTX_CHECK(p_core_ctx);

	switch(p_core_ctx->cmd_core.out_mode)
	{
	case cli_cmd_core_out_mode_buffer:
#		if (CLI_SUPPORT_BUFFER_OUT == 1)
		if (select_mask & CLI_BUILDIN_REG_HELP) {
			(void)cmd_core_key_add(
				&p_core_ctx->cmd_core, CLI_INTERN_MASK_BUILTIN,
				"help", "Help", cli_core_cmd_help__buffer, IFX_NULL);
		}
		if (select_mask & CLI_BUILDIN_REG_QUIT) {
			(void)cmd_core_key_add(
				&p_core_ctx->cmd_core, CLI_INTERN_MASK_BUILTIN,
				"quit", "Quit", cli_core_cmd_quit__buffer, IFX_NULL);
		}
		if (select_mask & CLI_BUILDIN_REG_WHAT) {
			(void)cmd_core_key_add(
				&p_core_ctx->cmd_core, CLI_INTERN_MASK_BUILTIN,
				"what", "what_string", cli_core_cmd_what__buffer, IFX_NULL);
		}
#		else
		CLI_USR_ERR(LIB_CLI,
			("ERR CLI Core: register build-in - buffer printout mode not supported" CLI_CRLF));
		return IFX_ERROR;
#		endif
		break;

	case cli_cmd_core_out_mode_file:
#		if (CLI_SUPPORT_FILE_OUT == 1)
		if (select_mask & CLI_BUILDIN_REG_HELP) {
			(void)cmd_core_key_add(
				&p_core_ctx->cmd_core, CLI_INTERN_MASK_BUILTIN,
				"help", "Help", IFX_NULL, cli_core_cmd_help__file);
		}
		if (select_mask & CLI_BUILDIN_REG_QUIT) {
			(void)cmd_core_key_add(
				&p_core_ctx->cmd_core, CLI_INTERN_MASK_BUILTIN,
				"quit", "Quit", IFX_NULL, cli_core_cmd_quit__file);
		}
		if (select_mask & CLI_BUILDIN_REG_WHAT) {
			(void)cmd_core_key_add(
				&p_core_ctx->cmd_core, CLI_INTERN_MASK_BUILTIN,
				"what", "what_string", IFX_NULL, cli_core_cmd_what__file);
		}
#		else
		CLI_USR_ERR(LIB_CLI,
			("ERR CLI Core: register build-in - file printout mode not supported" CLI_CRLF));
		return IFX_ERROR;
#		endif
		break;

	default:
		CLI_USR_ERR(LIB_CLI,
			("ERR CLI Core: register build-in - unknown printout mode" CLI_CRLF));
		return IFX_ERROR;
	}

	return IFX_SUCCESS;
}


int cli_core_key_add__buffer(
	struct cli_core_context_s *p_core_ctx,
	unsigned int group_mask,
	char const *p_short_name,
	char const *p_long_name,
	cli_cmd_user_fct_buf_t cli_cmd_user_fct_buf)
{
#if (CLI_SUPPORT_BUFFER_OUT == 1)
	CLI_CORE_CTX_CHECK(p_core_ctx);

	group_mask = user_group_mask_get(p_core_ctx, group_mask, p_long_name);
	if (p_core_ctx->cmd_core.out_mode == cli_cmd_core_out_mode_buffer)
		return cmd_core_key_add(
			&p_core_ctx->cmd_core, group_mask,
			p_short_name, p_long_name,
			cli_cmd_user_fct_buf, IFX_NULL);
	else
		return IFX_ERROR;
#else
	return IFX_ERROR;
#endif
}

int cli_core_key_add__file(
	struct cli_core_context_s *p_core_ctx,
	unsigned int group_mask,
	char const *p_short_name,
	char const *p_long_name,
	cli_cmd_user_fct_file_t cli_cmd_user_fct_file)
{
#if (CLI_SUPPORT_FILE_OUT == 1)
	CLI_CORE_CTX_CHECK(p_core_ctx);

	group_mask = user_group_mask_get(p_core_ctx, group_mask, p_long_name);
	if (p_core_ctx->cmd_core.out_mode == cli_cmd_core_out_mode_file)
		return cmd_core_key_add(
			&p_core_ctx->cmd_core, (group_mask & CLI_USER_MASK_ALL),
			p_short_name, p_long_name,
			IFX_NULL, cli_cmd_user_fct_file);
	else
		return IFX_ERROR;
#else
	return IFX_ERROR;
#endif
}

int cli_core_cmd_exec__buffer(
	struct cli_core_context_s *p_core_ctx,
	char *p_cmd_arg_buf,
	const unsigned int buffer_size)
{
#if (CLI_SUPPORT_BUFFER_OUT == 1)
	CLI_CORE_CTX_CHECK(p_core_ctx);

	if (p_core_ctx->cmd_core.out_mode == cli_cmd_core_out_mode_buffer)
		return cmd_parse_execute(
		p_core_ctx, p_cmd_arg_buf, buffer_size, IFX_NULL);
	else
		return IFX_ERROR;
#else
	return IFX_ERROR;
#endif
}

int cli_core_cmd_exec__file(
	struct cli_core_context_s *p_core_ctx,
	char *p_cmd_arg_buf,
	clios_file_t *p_file)
{
#if (CLI_SUPPORT_FILE_OUT == 1)
	CLI_CORE_CTX_CHECK(p_core_ctx);

	if (p_core_ctx->cmd_core.out_mode == cli_cmd_core_out_mode_file)
		return cmd_parse_execute(
			p_core_ctx, p_cmd_arg_buf, 0, p_file);
	else
		return IFX_ERROR;
#else
	return IFX_ERROR;
#endif
}

int cli_core_cmd_arg_exec__file(
	struct cli_core_context_s *p_core_ctx,
	char *p_cmd,
	char *p_arg,
	clios_file_t *p_file)
{
#if (CLI_SUPPORT_FILE_OUT == 1)
	struct cli_cmd_data_s cmd_data = {IFX_NULL, 0, {IFX_NULL}};
	char dummy_arg[10] = "";
	/* time_t start_time = 0, stop_time = 0;
	bool start_time_measured = false; */

	CLI_CORE_CTX_CHECK(p_core_ctx);

	if (p_cmd == IFX_NULL || clios_strlen(p_cmd) == 0) {
		return cli_core_cmd_arg_exec__file(p_core_ctx, "help", IFX_NULL, p_file);
		/* return IFX_ERROR; */
	}

	p_cmd = remove_whitespaces(p_cmd);
	if (p_arg != IFX_NULL) {
		p_arg = remove_whitespaces(p_arg);
	} else {
		p_arg = dummy_arg;
	}

	switch (cmd_core_key_find(&p_core_ctx->cmd_core, p_cmd, &cmd_data))
	{
	case e_cli_status_ok:
		switch(p_core_ctx->cmd_core.out_mode)
		{
		case cli_cmd_core_out_mode_buffer:
#			if (CLI_SUPPORT_BUFFER_OUT == 1) && 0
			if (cmd_data.user_fct.buf_out != IFX_NULL)
				return cmd_data.user_fct.buf_out(p_user_data, p_arg, cmd_buffer_size, p_cmd_buf);
			else
				return clios_sprintf(p_cmd_buf,
					"errorcode=-1 (internal error, no function pointer)");
#			else
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: exec - buffer printout not supported" CLI_CRLF));
			return IFX_ERROR;
#			endif
		case cli_cmd_core_out_mode_file:
#			if (CLI_SUPPORT_FILE_OUT == 1)
			if (cmd_data.user_fct.file_out != IFX_NULL)
				return cmd_data.user_fct.file_out(
					(cmd_data.mask & CLI_INTERN_MASK_BUILTIN) ? p_core_ctx : p_core_ctx->p_user_fct_data,
					p_arg, p_file);
			else
				return clios_fprintf(p_file,
					"errorcode=-1 (internal error, no function pointer)");
#			else
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: exec - file printout not supported" CLI_CRLF));
			return IFX_ERROR;
#			endif
		default:
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: exec - printout mode unknown" CLI_CRLF));
			return IFX_ERROR;
		}
	default:
		switch(p_core_ctx->cmd_core.out_mode)
		{
		case cli_cmd_core_out_mode_buffer:
#			if (CLI_SUPPORT_BUFFER_OUT == 1) && 0
			return clios_sprintf(p_cmd_buf, "errorcode=-1 (unknown command)");
#			else
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: exec - unknown command, printout mode not supported" CLI_CRLF));
			return IFX_ERROR;
#			endif
		case cli_cmd_core_out_mode_file:
#			if (CLI_SUPPORT_FILE_OUT == 1)
			return clios_fprintf(p_file, "errorcode=-1 (unknown command)");
#			else
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: exec - unknown command, printout mode not supported" CLI_CRLF));
			return IFX_ERROR;
#			endif
		default:
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Core: exec - unknown command, unknown printout mode" CLI_CRLF));
			return IFX_ERROR;
		}
	}

	return IFX_SUCCESS;
#else
	return IFX_ERROR;
#endif
}

int cli_check_help__buffer(
	const char *p_cmd,
	const char *p_usage,
	const unsigned int bufsize_max,
	char *p_out)
{
#if (CLI_SUPPORT_BUFFER_OUT == 1)
	if (cmd_check_help(p_cmd))
	{
		if (clios_strlen(p_usage) < bufsize_max)
			return clios_sprintf(p_out, "%s", p_usage);
		else
			return IFX_SUCCESS;
	}
	return IFX_ERROR;
#else
	return IFX_ERROR;
#endif
}

int cli_check_help__file(
	const char *p_cmd,
	const char *p_usage,
	clios_file_t *p_file)
{
#if (CLI_SUPPORT_FILE_OUT == 1)
	if (cmd_check_help(p_cmd))
	{
		if (clios_strlen(p_usage))
			return clios_fprintf(p_file, "%s", p_usage);
		else
			return IFX_SUCCESS;
	}
	return IFX_SUCCESS;
#else
	return IFX_ERROR;
#endif
}

/** Setup a CLI interface, therefore the follwoing steps are necessary:
   - Create the CLI core (cli_core_init)
   - Count all commands (memory usage), counting mode:
     --> register built-in commands
     --> register user commands
   - Switch to registration mode (allocates memory)
   - Register all commands (setup binary tree), registration mode:
     --> register built-in commands
     --> register user commands
   - Activate CLI Core
*/
int cli_core_setup__file(
	struct cli_core_context_s **pp_core_ctx,
	unsigned int builtin_selmask,
	void *p_user_fct_data,
	cli_cmd_register__file cmd_reg_list[])
{
#if (CLI_SUPPORT_FILE_OUT == 1)
	int retval = 0, i = 0;
	struct cli_core_context_s *p_core_ctx = IFX_NULL;

	if (!pp_core_ctx)
	{
		/* missing args */
		return IFX_ERROR;
	}

	if ((cmd_reg_list[0] == IFX_NULL) && (builtin_selmask == 0))
	{
		/* no commands */
		return IFX_ERROR;
	}

	/* create CLI core */
	if ((retval = cli_core_init(
			&p_core_ctx, cli_cmd_core_out_mode_file,
			p_user_fct_data)) != IFX_SUCCESS)
		return retval;

	/* count built-in commands (help, quit) */
	if (builtin_selmask != 0) {
		if ((retval = cli_core_buildin_register(
				p_core_ctx, builtin_selmask)) != IFX_SUCCESS)
			goto CLI_CORE_SETUP__FILE_ERR;
	}
	/* count user commands */
	i = 0;
	while (cmd_reg_list[i])  {
		if ((retval = cmd_reg_list[i](p_core_ctx)) != IFX_SUCCESS)
			break;
		i++;
	}
	if (retval != IFX_SUCCESS)
		goto CLI_CORE_SETUP__FILE_ERR;

	/* switch to registration mode */
	if ((retval = cli_core_cfg_mode_cmd_reg(p_core_ctx)) != IFX_SUCCESS)
		goto CLI_CORE_SETUP__FILE_ERR;

	/* register built-in commands (help, quit) */
	if (builtin_selmask != 0) {
		if ((retval = cli_core_buildin_register(
				p_core_ctx, builtin_selmask)) != IFX_SUCCESS)
			goto CLI_CORE_SETUP__FILE_ERR;
	}
	/* count user commands */
	i = 0;
	while (cmd_reg_list[i]) {
		if ((retval = cmd_reg_list[i](p_core_ctx)) != IFX_SUCCESS)
			break;
		i++;
	}
	if (retval != IFX_SUCCESS)
		goto CLI_CORE_SETUP__FILE_ERR;

	/* Activate CLI core */
	if ((retval = cli_core_cfg_mode_active(p_core_ctx)) != IFX_SUCCESS)
		goto CLI_CORE_SETUP__FILE_ERR;

	*pp_core_ctx = p_core_ctx;
	return retval;

CLI_CORE_SETUP__FILE_ERR:
	(void)cli_core_release(&p_core_ctx, cli_cmd_core_out_mode_file);
	return retval;
#else
	return IFX_ERROR;
#endif
}

/** Setup a CLI interface, therefore the follwoing steps are necessary:
   - Create the CLI core (cli_core_init)
   - Count all commands (memory usage), counting mode:
     --> register built-in commands
     --> register user commands
   - Switch to registration mode (allocates memory)
   - Register all commands (setup binary tree), registration mode:
     --> register built-in commands
     --> register user commands
   - Activate CLI Core
*/
int cli_core_group_setup__file(
	struct cli_core_context_s **pp_core_ctx,
	unsigned int builtin_selmask,
	void *p_user_fct_data,
	cli_cmd_register__file cmd_reg_list[],
	char const *p_group_list[],
	struct cli_group_key_entry_s *p_key_entries)
{
#if (CLI_SUPPORT_FILE_OUT == 1)
	int retval = 0, i = 0;
	struct cli_core_context_s *p_core_ctx = IFX_NULL;

	if (!pp_core_ctx)
	{
		/* missing args */
		return IFX_ERROR;
	}

	if ((cmd_reg_list[0] == IFX_NULL) && (builtin_selmask == 0))
	{
		/* no commands */
		return IFX_ERROR;
	}

	/* create CLI core */
	if ((retval = cli_core_init(
			&p_core_ctx, cli_cmd_core_out_mode_file,
			p_user_fct_data)) != IFX_SUCCESS)
		return retval;

	if ((retval = cli_core_user_group_init(p_core_ctx, p_group_list, p_key_entries)) != IFX_SUCCESS)
		return retval;

	/* count built-in commands (help, quit) */
	if (builtin_selmask != 0) {
		if ((retval = cli_core_buildin_register(
				p_core_ctx, builtin_selmask)) != IFX_SUCCESS)
			goto CLI_CORE_SETUP__FILE_ERR;
	}
	/* count user commands */
	i = 0;
	while (cmd_reg_list[i])  {
		if ((retval = cmd_reg_list[i](p_core_ctx)) != IFX_SUCCESS)
			break;
		i++;
	}
	if (retval != IFX_SUCCESS)
		goto CLI_CORE_SETUP__FILE_ERR;

	/* switch to registration mode */
	if ((retval = cli_core_cfg_mode_cmd_reg(p_core_ctx)) != IFX_SUCCESS)
		goto CLI_CORE_SETUP__FILE_ERR;

	/* register built-in commands (help, quit) */
	if (builtin_selmask != 0) {
		if ((retval = cli_core_buildin_register(
				p_core_ctx, builtin_selmask)) != IFX_SUCCESS)
			goto CLI_CORE_SETUP__FILE_ERR;
	}
	/* count user commands */
	i = 0;
	while (cmd_reg_list[i]) {
		if ((retval = cmd_reg_list[i](p_core_ctx)) != IFX_SUCCESS)
			break;
		i++;
	}
	if (retval != IFX_SUCCESS)
		goto CLI_CORE_SETUP__FILE_ERR;

	/* Activate CLI core */
	if ((retval = cli_core_cfg_mode_active(p_core_ctx)) != IFX_SUCCESS)
		goto CLI_CORE_SETUP__FILE_ERR;

	*pp_core_ctx = p_core_ctx;
	return retval;

CLI_CORE_SETUP__FILE_ERR:
	(void)cli_core_release(&p_core_ctx, cli_cmd_core_out_mode_file);
	return retval;
#else
	return IFX_ERROR;
#endif
}


int cli_user_if_register(
	struct cli_core_context_s *p_core_ctx,
	void *p_cb_data,
	cli_exit_callback exit_cb_fct,
	cli_event_callback event_cb_fct,
	cli_dump_callback dump_cb_fct,
	struct cli_user_context_s **pp_user_ctx)
{
	IFX_int32_t tout = 0;
	struct cli_user_context_s *p_user_ctx = IFX_NULL;

	CLI_CORE_CTX_CHECK(p_core_ctx);
	if (*pp_user_ctx != IFX_NULL)
	{
		/* expect NULL pointer */
		return IFX_ERROR;
	}

	if ((clios_lockget_timout(
		&p_core_ctx->lock_ctx, (IFX_uint32_t)-1, &tout) == IFX_SUCCESS)
	    && (tout == 0))
	{
		p_user_ctx = (struct cli_user_context_s *)
			clios_memalloc(sizeof(struct cli_user_context_s));
		if (!p_user_ctx)
		{
			(void)clios_lockrelease(&p_core_ctx->lock_ctx);
			/* error mem alloc */
			return IFX_ERROR;
		}
		clios_memset(p_user_ctx, 0x0, sizeof(struct cli_user_context_s));
		p_user_ctx->p_user_data = p_cb_data;
		p_user_ctx->exit_cb_fct = exit_cb_fct;
		p_user_ctx->event_cb_fct = event_cb_fct;
		p_user_ctx->dump_cb_fct = dump_cb_fct;

		if (p_core_ctx->p_user_head == IFX_NULL) {
			p_core_ctx->p_user_head = p_user_ctx;
		} else {
			struct cli_user_context_s *p_user_list = p_core_ctx->p_user_head;
			while (p_user_list->p_next != IFX_NULL) {
				p_user_list = p_user_list->p_next;
			}
			p_user_list->p_next = p_user_ctx;
		}

		(void)clios_lockrelease(&p_core_ctx->lock_ctx);
	}

	*pp_user_ctx = p_user_ctx;

	return IFX_SUCCESS;
}

int cli_user_if_unregister(
	struct cli_core_context_s *p_core_ctx,
	struct cli_user_context_s **pp_user_ctx)
{
	IFX_int32_t tout = 0;
	struct cli_user_context_s *p_user_ctx = IFX_NULL, *p_user_list = IFX_NULL;

	CLI_CORE_CTX_CHECK(p_core_ctx);
	if (*pp_user_ctx == IFX_NULL)
	{
		return IFX_SUCCESS;
	}

	if ((clios_lockget_timout(
		&p_core_ctx->lock_ctx, (IFX_uint32_t)-1, &tout) == IFX_SUCCESS)
	    && (tout == 0))
	{
		if (p_core_ctx->p_user_head == *pp_user_ctx)
		{
			p_user_ctx = *pp_user_ctx;
			*pp_user_ctx = IFX_NULL;
			p_core_ctx->p_user_head = p_user_ctx->p_next;
		}
		else
		{
			p_user_list = p_core_ctx->p_user_head;
			while (p_user_list)
			{
				if (p_user_list->p_next == *pp_user_ctx)
				{
					p_user_ctx = *pp_user_ctx;
					*pp_user_ctx = IFX_NULL;
					p_user_list->p_next = p_user_ctx->p_next;
					break;
				}
				p_user_list = p_user_list->p_next;
			}
		}
		(void)clios_lockrelease(&p_core_ctx->lock_ctx);

		if (p_user_ctx)
		{
			clios_memfree(p_user_ctx);
		}

	}

	return IFX_ERROR;
}

static int cli_dummy_if_exit(void *p_ctx)
{
	struct cli_dummy_interface_s *p_dummy_console = (struct cli_dummy_interface_s *) p_ctx;

	p_dummy_console->b_run = IFX_FALSE;

	return IFX_SUCCESS;
}

int cli_dummy_if_start(
	struct cli_core_context_s *p_core_ctx,
	unsigned int sleep_ms)
{
	int retval = IFX_SUCCESS;
	struct cli_dummy_interface_s dummy_if;
	struct cli_user_context_s *p_user_ctx = IFX_NULL;

	/* ToDo: check if quit cmd is registered */

	dummy_if.b_run = IFX_TRUE;
	retval = cli_user_if_register(
		p_core_ctx, (void *)&dummy_if,
		cli_dummy_if_exit, IFX_NULL, IFX_NULL,
		&p_user_ctx);

	if (retval != IFX_SUCCESS) {
		return IFX_ERROR;
	}

	while (dummy_if.b_run == IFX_TRUE)
	{
		clios_sleep_msec(sleep_ms);
	}

	retval = cli_user_if_unregister(
		p_core_ctx, &p_user_ctx);

	return retval;
}


