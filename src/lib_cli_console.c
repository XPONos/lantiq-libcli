/******************************************************************************

                              Copyright (c) 2011
                            Lantiq Deutschland GmbH
                     Am Campeon 3; 85579 Neubiberg, Germany

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/

/* ifxos_file_access.h for linux seems to be broken (ifxos 1.5.10);
   thus define IFXOS_HAVE_FILE_ACCESS separately */
#if defined(WIN32) || defined(LINUX)
#define IFXOS_HAVE_FILE_ACCESS 1
#endif

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


#if (CLI_SUPPORT_CONSOLE == 1)

/** \addtogroup LIB_CLI_CONSOLE
   @{
*/

#define CLI_MAX_OLD_COMMAND		(16 / 4)
#define CLI_MAX_COMMAND_LINE_LENGTH	(2560 / 4)
#define CLI_MAX_FUNCTION_NAME_LENGTH	64

#define KEY_INPUT_NONE	0
#define KEY_INPUT_ENTER	1
#define KEY_INPUT_BS	2
#define KEY_INPUT_UP	3
#define KEY_INPUT_DOWN	4
#define KEY_INPUT_CHAR	5

#ifndef WIN32
#define BOLD ""
#define NORMAL ""
#define CLREOL "\033[K"
#else
#define BOLD   ""
#define NORMAL ""
#define CLREOL ""
#endif

struct cli_console_context_s {
	/** CLI core */
	struct cli_core_context_s *p_cli_core_ctx;
	/** user data for callback handling */
	struct cli_user_context_s *p_user_ctx;
	char old_command[CLI_MAX_OLD_COMMAND][CLI_MAX_COMMAND_LINE_LENGTH];
	char arg[CLI_MAX_COMMAND_LINE_LENGTH];
	char cmd[CLI_MAX_FUNCTION_NAME_LENGTH];
	int idx;
	int old_idx;
	int prev_idx;
	clios_file_t *file_in;
	clios_file_t *file_out;
	volatile IFX_boolean_t b_run;
};



/**
   Read command line.

\param[in] s  Destination pointer for command line
\param[in] len  Maximum length of command line
\param[in] p_in  File for key input (e.g. IFXOS_STDIN)
\param[in] p_out File for output (e.g. IFXOS_STDOUT)
\return Length of command string
*/
CLI_STATIC int cmd_line_read(
	char *s,
	unsigned int len,
	clios_file_t *p_in,
	clios_file_t *p_out)
{
	char c;
	char echo;
	int blank_cnt = 0;
	int first = 1;
	int ret = KEY_INPUT_CHAR;

	if (clios_strlen(s))
		first = 0;

	len -= (unsigned int)clios_strlen(s);
	s += (unsigned int)clios_strlen(s);

	while (len--) {
		echo = 0;
#if defined(IFXOS_HAVE_FILE_ACCESS) && (IFXOS_HAVE_FILE_ACCESS == 1)
		if ((int)clios_fread(&c, 1, 1, p_in) == 0) {
			if (clios_feof(p_in)) {
				ret = KEY_INPUT_NONE;
				/*lint -save -e(801) */
				goto KEY_FINISH;
				/*lint -restore */
			}
		}
#else
		{
			int ret = clios_getchar();

			if (ret == EOF) {
				goto KEY_FINISH;
			}
			c = (char)ret;
		}
#endif

		if (first && (c == ' ')) {
			len++;
			continue;
		}

		first = 0;

		if (c != ' ') {
			blank_cnt = 0;
		}

		switch (c) {
		default:
			echo = c;
			break;

		case ' ':
			if (blank_cnt) {
				len++;
				s--;
			} else {
				echo = ' ';
			}
			blank_cnt++;
			break;

			/* backspace */
		case 127:	/* BS in Linux Terminal */
		case 8:	/* BS in TTY */
			len++;
			s--;
			ret = KEY_INPUT_BS;
			/*lint -save -e(801) */
			goto KEY_FINISH;
			/*lint -restore */

			/* tab */
		case 9:
			break;
		case '\n':
			(void)clios_putchar('\n', p_out);
#if defined(IFXOS_HAVE_FILE_ACCESS) && (IFXOS_HAVE_FILE_ACCESS == 1)
			(void)clios_fflush(p_out);
#endif
			ret = KEY_INPUT_ENTER;
			/*lint -save -e(801) */
			goto KEY_FINISH;
			/*lint -restore */

		case 033:
#if defined(IFXOS_HAVE_FILE_ACCESS) && (IFXOS_HAVE_FILE_ACCESS == 1)
			if ((int)clios_fread(&c, 1, 1, p_in) == 0)
				break;
#else
			{
				int ret = clios_getchar();

				if (ret == EOF) {
					break;
				}
				c = (char)ret;
			}
#endif
			switch (c) {
			case 27:
				/* escape */
				break;

			case '[':
#if defined(IFXOS_HAVE_FILE_ACCESS) && (IFXOS_HAVE_FILE_ACCESS == 1)
				if ((int)clios_fread(&c, 1, 1, p_in) == 0)
					break;
#else
				{
					int ret = clios_getchar();

					if (ret == EOF) {
						break;
					}
					c = (char)ret;
				}
#endif
				switch (c) {
				case 'D':
					/* left */
					break;
				case 'C':
					/* right */
					break;
				case 'A':
					/* up */
					ret = KEY_INPUT_UP;
					break;
				case 'B':
					/* down */
					ret = KEY_INPUT_DOWN;
					break;
				default:
					break;
				}
				break;

			default:
				break;
			}
			/*lint -save -e(801) */
			goto KEY_FINISH;
			/*lint -restore */
		}

		*s++ = c;
		if (echo)
			(void)clios_putchar(echo, p_out);
#if defined(IFXOS_HAVE_FILE_ACCESS) && (IFXOS_HAVE_FILE_ACCESS == 1)
		(void)clios_fflush(p_out);
#endif
	}

KEY_FINISH:

	*s++ = 0;

	return ret;
}

/** Exit CLI

   \param[in] user callback context pointer
*/
CLI_STATIC int console_exit(void *p_cb_ctx)
{
	struct cli_console_context_s *p_console_ctx;

	p_console_ctx = (struct cli_console_context_s *)p_cb_ctx;
	p_console_ctx->p_cli_core_ctx = IFX_NULL;
	p_console_ctx->b_run = IFX_FALSE;

	return IFX_SUCCESS;
}


/** Initialize CLI

   \param[in] p_cli_core_ctx  CLI core context pointer
   \param[in] p_in  Input FD
   \param[in] p_out  Output FD
   \param[in] pp_console_ctx  Returns the CLI Console Context pointer
*/
int cli_console_init(
	struct cli_core_context_s *p_cli_core_ctx,
	clios_file_t *p_in,
	clios_file_t *p_out,
	struct cli_console_context_s **pp_console_ctx)
{
	struct cli_console_context_s *p_console_ctx;

	if (*pp_console_ctx != IFX_NULL) {
		CLI_USR_ERR(LIB_CLI,
			("ERR CLI Console: init - invalid arg, context pointer not zero" CLI_CRLF));
		return IFX_ERROR;
	}

	p_console_ctx = (struct cli_console_context_s *)
	    clios_memalloc(sizeof(struct cli_console_context_s));
	if (p_console_ctx == IFX_NULL) {
		CLI_USR_ERR(LIB_CLI,
			("ERR CLI Console: init - no memory" CLI_CRLF));
		return IFX_ERROR;
	}

	clios_memset(p_console_ctx, 0x00, sizeof(struct cli_console_context_s));
	p_console_ctx->p_cli_core_ctx = p_cli_core_ctx;
	p_console_ctx->file_in = p_in;
	p_console_ctx->file_out = p_out;
	p_console_ctx->b_run = IFX_TRUE;

#if ( defined(IFXOS_HAVE_TERMIOS) && (IFXOS_HAVE_TERMIOS == 1) )
	if (p_out == IFXOS_STDOUT)
		OMCI_EchoOff();
	if (p_in == IFXOS_STDIN)
		OMCI_KeypressSet();
#endif

	(void)clios_fprintf(p_out, CLI_CRLF "CLI management interface" CLI_CRLF);
	(void)clios_fprintf(p_out, "Enter 'help' for a list of built-in commands." CLI_CRLF);
	/* (void)omci_cli_help_print(context, "-h", p_out); */

	(void)cli_user_if_register(
		p_cli_core_ctx, p_console_ctx,
		console_exit, IFX_NULL, IFX_NULL,
		&p_console_ctx->p_user_ctx);

	*pp_console_ctx = p_console_ctx;
	return IFX_SUCCESS;
}

/** Shutdown CLI

   \param[in] pp_console_ctx Contains the console context pointer
*/
int cli_console_release(
	struct cli_core_context_s *p_cli_core_ctx,
	struct cli_console_context_s **pp_console_ctx)
{
	struct cli_console_context_s *p_console_ctx;

	if (*pp_console_ctx)
	{
		p_console_ctx = *pp_console_ctx;
		*pp_console_ctx = IFX_NULL;

		if (cli_user_if_unregister(
			p_cli_core_ctx, &p_console_ctx->p_user_ctx) == IFX_SUCCESS)
		{
			CLI_USR_ERR(LIB_CLI,
				("ERR CLI Console: shutdown - failed" CLI_CRLF));
			return -1;
		}
		clios_memfree(p_console_ctx);
	}

	return IFX_SUCCESS;
}

/** Handle console input

   \param[in] p_console_ctx Console context pointer
*/
int cli_console_handle(
	struct cli_console_context_s *p_console_ctx)
{
	int result, len, i;
	struct cli_core_context_s *p_cli_core_ctx = p_console_ctx->p_cli_core_ctx;
	clios_file_t *p_in = p_console_ctx->file_in;
	clios_file_t *p_out = p_console_ctx->file_out;

	p_console_ctx->cmd[0] = 0;
	p_console_ctx->arg[0] = 0;
	clios_memset(p_console_ctx->arg, 0x00, sizeof(p_console_ctx->arg));

	(void)clios_fprintf(p_out, CLI_CRLF);

      ARROW_KEY:
#if defined(IFXOS_HAVE_FILE_ACCESS) && (IFXOS_HAVE_FILE_ACCESS == 1)
	(void)clios_fflush(p_out);
#endif
	(void)clios_fprintf(p_out, "\r" CLREOL BOLD "#>" NORMAL "%s",
			    p_console_ctx->arg);
#if defined(IFXOS_HAVE_FILE_ACCESS) && (IFXOS_HAVE_FILE_ACCESS == 1)
	(void)clios_fflush(p_out);
#endif

	i = p_console_ctx->old_idx;

      NEW_LOOP:

	/* read command line */
	switch (cmd_line_read
		(p_console_ctx->arg, CLI_MAX_COMMAND_LINE_LENGTH, p_in,
		 p_out)) {
	case KEY_INPUT_UP:
		if (p_console_ctx->old_idx)
			p_console_ctx->old_idx--;
		else
			p_console_ctx->old_idx = CLI_MAX_OLD_COMMAND - 1;

		if (clios_strlen(p_console_ctx->old_command[p_console_ctx->old_idx])) {
			clios_strcpy(p_console_ctx->arg, p_console_ctx->old_command[p_console_ctx->old_idx]);
			/*lint -save -e(801) */
			goto ARROW_KEY;
			/*lint -restore */
		} else {
			p_console_ctx->old_idx = i;
			/*lint -save -e(801) */
			goto NEW_LOOP;
			/*lint -restore */
		}

	case KEY_INPUT_DOWN:
		if (++p_console_ctx->old_idx >= CLI_MAX_OLD_COMMAND)
			p_console_ctx->old_idx = 0;

		if (clios_strlen(p_console_ctx->old_command[p_console_ctx->old_idx])) {
			if (p_console_ctx->old_idx >= CLI_MAX_OLD_COMMAND)
				p_console_ctx->old_idx = 0;

			clios_strcpy(
				p_console_ctx->arg,
				p_console_ctx->old_command[p_console_ctx->old_idx]);
			/*lint -save -e(801) */
			goto ARROW_KEY;
			/*lint -restore */
		} else {
			p_console_ctx->old_idx = i;
			/*lint -save -e(801) */
			goto NEW_LOOP;
			/*lint -restore */
		}

	case KEY_INPUT_BS:
		/* backspace */
		/*lint -save -e(801) */
		goto ARROW_KEY;
		/*lint -restore */

	case KEY_INPUT_NONE:
		if (p_console_ctx->b_run == IFX_FALSE)
			/* indicate command "quit" */
			return IFX_ERROR;
		/* clios_fprintf(IFXOS_STDOUT, "KEY_INPUT_NONE" CLI_CRLF ); */
		/* OMCI_Sleep(1); */
		/*lint -save -e(801) */
		goto NEW_LOOP;
		/*lint -restore */

	default:
		break;
	}

	if (p_console_ctx->b_run == IFX_FALSE)
		/* indicate command "quit" */
		return IFX_ERROR;

	/* get command name */
	clios_sscanf(p_console_ctx->arg,
	       "%" _MKSTR(CLI_MAX_FUNCTION_NAME_LENGTH) "s", p_console_ctx->cmd);

	len = (int)clios_strlen(p_console_ctx->cmd);

	if (len) {
		if (clios_strncmp(
			p_console_ctx->old_command[p_console_ctx->prev_idx],
			p_console_ctx->arg, (unsigned int)len) != 0) {
			clios_strcpy(
				p_console_ctx->old_command[p_console_ctx->idx],
				p_console_ctx->arg);
			p_console_ctx->prev_idx = p_console_ctx->idx;
			if (++p_console_ctx->idx >= CLI_MAX_OLD_COMMAND)
				p_console_ctx->idx = 0;
		}

		p_console_ctx->old_idx = p_console_ctx->idx;

#if defined(IFXOS_HAVE_FILE_ACCESS) && (IFXOS_HAVE_FILE_ACCESS == 1)
		(void)clios_fflush(p_out);
#endif

		result = cli_core_cmd_arg_exec__file(
			p_cli_core_ctx, p_console_ctx->cmd, p_console_ctx->arg + len + 1, p_out);

#if defined(IFXOS_HAVE_FILE_ACCESS) && (IFXOS_HAVE_FILE_ACCESS == 1)
		(void)clios_fflush(p_out);
#endif

		if (result == 1) {
			/* indicate command "quit" */
			return IFX_ERROR;
		}
	}

	return IFX_SUCCESS;
}

/** Handle console input

\param[in] p_console_ctx Console context pointer
\param[in] p_in  File for key input (e.g. IFXOS_STDIN)
\param[in] p_out File for output (e.g. IFXOS_STDOUT)
*/
int cli_console_run(
	struct cli_core_context_s *p_cli_core_ctx,
	clios_file_t *p_in,
	clios_file_t *p_out)
{
	int retval = 0;
	struct cli_console_context_s *p_console_ctx = IFX_NULL;
	clios_file_t *p_in_used = (p_in) ?  p_in : IFXOS_STDIN;
	clios_file_t *p_out_used = (p_out) ? p_out : IFXOS_STDOUT;

	if ((retval = cli_console_init(
			p_cli_core_ctx, p_in_used, p_out_used, &p_console_ctx)) != IFX_SUCCESS) {
		return IFX_ERROR;
	}

	do {
		retval = cli_console_handle(p_console_ctx);
	} while ((retval == IFX_SUCCESS) && (p_console_ctx->b_run == IFX_TRUE));

	(void)cli_console_release(p_cli_core_ctx, &p_console_ctx);

	return 0;
}


/** @} */

#endif /* #if (CLI_SUPPORT_CONSOLE == 1) */

