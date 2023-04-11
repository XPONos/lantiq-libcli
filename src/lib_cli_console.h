/******************************************************************************

                              Copyright (c) 2011
                            Lantiq Deutschland GmbH
                     Am Campeon 3; 85579 Neubiberg, Germany

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef _LIB_CLI_CONSOLE_H
#define _LIB_CLI_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif


#include "lib_cli_config.h"
#include "lib_cli_core.h"

/* forward declaration */
struct cli_console_context_s;


/** \addtogroup LIB_CLI
   @{
*/

/** \defgroup LIB_CLI_CONSOLE CLI Console
   @{
*/

/**
   Initialize the console.

   \param[in]  p_cli_core_ctx  CLI core context pointer.
   \param[in]  p_in   Pointer to stdin descriptor.
   \param[in]  p_out  Pointer to stdout descriptor.
   \param[out] pp_console_ctx  returns the console context.

   \return
   - IFX_ERROR On error
   - IFX_SUCCESS On success
*/
extern int cli_console_init(
	struct cli_core_context_s *p_cli_core_ctx,
	clios_file_t *p_in,
	clios_file_t *p_out,
	struct cli_console_context_s **pp_console_ctx);

/**
   Shutdown and release the console

   \param[in]  p_cli_core_ctx  CLI core context pointer.
   \param[out] pp_console_ctx  contains the console context.

   \return
   - IFX_ERROR On error
   - IFX_SUCCESS On success
*/
extern int cli_console_release(
	struct cli_core_context_s *p_cli_core_ctx,
	struct cli_console_context_s **pp_console_ctx);

/**
   This function collects single key input and handles execution of commands.


   \param[out] p_console_ctx  points to the console context.

   \return
   - IFX_SUCCESS if a command was completely entered and successful
                           executed
   - IFX_ERROR if an error occurred or the command "quit" was entered
*/
extern int cli_console_handle(
	struct cli_console_context_s *p_console_ctx);


/** Setup and run a console with the given CLI core.

   \param[in]  p_cli_core_ctx  CLI core context pointer.
   \param[in]  p_in   Pointer to stdin descriptor.
   \param[in]  p_out  Pointer to stdout descriptor.

   \return
   - IFX_ERROR On error
   - IFX_SUCCESS On success
*/
extern int cli_console_run(
	struct cli_core_context_s *p_cli_core_ctx,
	clios_file_t *p_in,
	clios_file_t *p_out);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif	/* #ifndef _LIB_CLI_CONSOLE_H */

