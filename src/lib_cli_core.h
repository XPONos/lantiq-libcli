/******************************************************************************

                               Copyright (c) 2011
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef _LIB_CLI_CORE_H
#define _LIB_CLI_CORE_H

/**
   \file lib_cli_core.h
   Command Line Interface - core defines
*/

/*
   CLI command core definitons:
   - tree (binary tree) which is used for searching.
   - control stuff

\remarks
   There are 2 kind of registration modes (add command):
   - count memory usage
   - allocate, setup the search tree.

*/

#ifdef __cplusplus
extern "C" {
#endif

#include "lib_cli_config.h"

#include "ifx_types.h"
#include "ifxos_lock.h"

#if (CLI_SUPPORT_FILE_OUT == 1)
#	include "ifxos_file_access.h"
#	define clios_file_t	IFXOS_File_t
#else
#	define clios_file_t	IFXOS_void_t
#endif

/** this version supports grouping of the commands (see cli_core_group_setup__file) */
#define CLI_HAVE_GROUP_SUPPORT	1

/** empty command name */
#define CLI_EMPTY_CMD		" "
/** help for empry command */
#define CLI_EMPTY_CMD_HELP	"n/a"

/** User selection mask all - allows to group commands from user side */
#define CLI_USER_MASK_ALL	0x0000FFFF

/** Register build-in command "Help" */
#define CLI_BUILDIN_REG_HELP	0x00000001
/** Register build-in command "Quit" */
#define CLI_BUILDIN_REG_QUIT	0x00000002
/** Register build-in command "What" */
#define CLI_BUILDIN_REG_WHAT	0x00000004

/** command node element for binary tree */
struct cli_cmd_node_s;

/** implementation dependend declarations */
enum cli_cmd_status_e
{
	/** command node valid */
	e_cli_status_ok,
	/** command node memory */
	e_cli_status_mem_exhausted,
	/** command node key duplicate */
	e_cli_status_duplicate_key,
	/** command node key not found */
	e_cli_status_key_not_found,
	/** command node key invalid */
	e_cli_status_key_invalid
};

/** states of the cli command register process */
enum cli_cmd_core_state_e {
	/** CLI cmd core state - memory count mode */
	e_cli_cmd_core_count,
	/** CLI cmd core state - mem allocated, cmd register mode */
	e_cli_cmd_core_allocate,
	/** CLI cmd core state - active, ready for operation */
	e_cli_cmd_core_active,
	/** CLI cmd core state - delete, release binary tree */
	e_cli_cmd_core_delete
};

/** output mode for the CLI command user function */
enum cli_cmd_core_out_mode_e {
	/** printout to buffer */
	cli_cmd_core_out_mode_buffer,
	/** printout to file */
	cli_cmd_core_out_mode_file
};

/** CLI cmd core - control struct */
struct cli_cmd_core_s
{
	/** CLI cmd operation state */
	enum cli_cmd_core_state_e e_state;
	/** selects the printout mode - buffer or file */
	enum cli_cmd_core_out_mode_e out_mode;
	/** number of nodes */
	IFX_uint_t n_nodes;
	/** number of used nodes */
	IFX_uint_t n_used_nodes;
	/** CLI cmd tree memory */
	struct cli_cmd_node_s *p_node_array;
	/** root of the binary CLI command tree */
	struct cli_cmd_node_s *p_root;
};

/** Group key entry to assign a search key to a group */
struct cli_group_key_entry_s
{
	/** group search key used to sort the CLI commands (long name) */
	char const *p_search_key;
	/** assignes the search key to a group */
	unsigned char group_num;
};

/** basic structure to manage groups */
struct cli_group_s
{
	/** number of groups */
	int n_reg_groups;
	/** number of specified search keys */
	int n_key_entries;

	/** list of group names */
	char const **pp_group_list;
	/** user group key entries */
	struct cli_group_key_entry_s *p_key_entries;

	/** mask to keep registered groups */
	unsigned int reg_group_mask;
	/** mask to keep not registerd groups */
	unsigned int not_reg_group_mask;
	/** group descriptor for misc commands */
	char const *p_misc_group;
	/** group descriptor for builtin commands */
	char const *p_builtin_group;
};

typedef int (*cli_cmd_user_fct_buf_t)(
	void *p_user_data,
	const char *p_cmd,
	const unsigned int out_buf_size,
	char *p_out_buf);

typedef int (*cli_cmd_user_fct_file_t)(
	void *p_user_data,
	const char *p_cmd,
	clios_file_t *p_out_file);

/** Exit callback with private pointer.
   (pointer can be defined with \ref cli_register) */
typedef int (*cli_exit_callback) (void *);

/** Event callback with private pointer (from \ref cli_register)
    and text of decoded event */
typedef int (*cli_event_callback) (void *, char *);

/** Message dump callback with private pointer (from \ref cli_register)
    and text of decoded event */
typedef int (*cli_dump_callback) (void *, char *);


/** CLI context to control several CLI's */
struct cli_user_context_s
{
	/** points to the next user CLI */
	struct cli_user_context_s *p_next;
	/** points to the user data for callback handling */
	void *p_user_data;
	/** callback function - on exit */
	cli_exit_callback exit_cb_fct;
	/** callback function - on event */
	cli_event_callback event_cb_fct;
	/** callback function - on dump */
	cli_dump_callback dump_cb_fct;
};

/** CLI context to control several CLI's */
struct cli_core_context_s
{
	/** init/control indication */
	unsigned int ctx_size;

	/** context control lock */
	IFXOS_lock_t lock_ctx;

	/** command core - contains user cli commands */
	struct cli_cmd_core_s cmd_core;

	/** user data - used for CLI function calls */
	void *p_user_fct_data;

	/** callback handling - points to the list of user CLI's */
	struct cli_user_context_s *p_user_head;

	/** group configuration for grouping the CLI commands */
	struct cli_group_s group;
};

/**
   User command register function type
*/
typedef int (*cli_cmd_register__file) (struct cli_core_context_s *);


/** CLI dummy interface, used for wait on quit */
struct cli_dummy_interface_s
{
	IFX_boolean_t b_run;
};

extern const char g_cli_version_what[];

extern int cli_sscanf(const char *buf, char const *fmt, ...);

extern int cli_core_cfg_cmd_shutdown(
	struct cli_core_context_s *p_core_ctx,
	enum cli_cmd_core_out_mode_e out_mode);

extern int cli_core_cfg_mode_cmd_reg(
	struct cli_core_context_s *p_core_ctx);

extern int cli_core_cfg_mode_active(
	struct cli_core_context_s *p_core_ctx);

extern int cli_core_release(
	struct cli_core_context_s **pp_core_ctx,
	enum cli_cmd_core_out_mode_e out_mode);

extern int cli_core_init(
	struct cli_core_context_s **pp_core_ctx,
	enum cli_cmd_core_out_mode_e out_mode,
	void *p_cli_fct_data);

extern int cli_core_user_group_init(
	struct cli_core_context_s *p_core_ctx,
	char const *p_group_list[],
	struct cli_group_key_entry_s *p_key_entries);


extern int cli_core_buildin_register(
	struct cli_core_context_s *p_core_ctx,
	unsigned int select_mask);

extern int cli_check_help__buffer(
	const char *p_cmd,
	const char *p_usage,
	const unsigned int bufsize_max,
	char *p_out);

extern int cli_check_help__file(
	const char *p_cmd,
	const char *p_usage,
	clios_file_t *p_file);

extern int cli_core_key_add__buffer(
	struct cli_core_context_s *p_core_ctx,
	unsigned int group_mask,
	char const *p_short_name,
	char const *p_long_name,
	cli_cmd_user_fct_buf_t cli_cmd_user_fct_buf);

extern int cli_core_key_add__file(
	struct cli_core_context_s *p_core_ctx,
	unsigned int group_mask,
	char const *p_short_name,
	char const *p_long_name,
	cli_cmd_user_fct_file_t cli_cmd_user_fct_file);

extern int cli_core_cmd_exec__buffer(
	struct cli_core_context_s *p_core_ctx,
	char *p_cmd_arg_buf,
	const unsigned int buffer_size);

extern int cli_core_cmd_exec__file(
	struct cli_core_context_s *p_core_ctx,
	char *p_cmd_arg_buf,
	clios_file_t *p_file);

int cli_core_cmd_arg_exec__file(
	struct cli_core_context_s *p_core_ctx,
	char *p_cmd,
	char *p_arg,
	clios_file_t *p_file);

extern int cli_core_setup__file(
	struct cli_core_context_s **pp_core_ctx,
	unsigned int builtin_selmask,
	void *p_user_fct_data,
	cli_cmd_register__file cmd_reg_list[]);

extern int cli_core_group_setup__file(
	struct cli_core_context_s **pp_core_ctx,
	unsigned int builtin_selmask,
	void *p_user_fct_data,
	cli_cmd_register__file cmd_reg_list[],
	char const *p_group_list[],
	struct cli_group_key_entry_s *p_key_entries);

extern int cli_user_if_register(
	struct cli_core_context_s *p_core_ctx,
	void *p_cb_data,
	cli_exit_callback exit_cb_fct,
	cli_event_callback event_cb_fct,
	cli_dump_callback dump_cb_fct,
	struct cli_user_context_s **pp_user_ctx);

extern int cli_user_if_unregister(
	struct cli_core_context_s *p_core_ctx,
	struct cli_user_context_s **pp_user_ctx);

extern int cli_dummy_if_start(
	struct cli_core_context_s *p_core_ctx,
	unsigned int sleep_ms);

#ifdef __cplusplus
}
#endif

#endif  /* _LIB_CLI_CORE_H */




