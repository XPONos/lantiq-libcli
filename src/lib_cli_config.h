#ifndef _LIB_CLI_CONFIG_H
#define _LIB_CLI_CONFIG_H
/****************************************************************************

                       Copyright (c) 2011
                     Lantiq Deutschland GmbH
                Am Campeon 3; 85579 Neubiberg, Germany

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

*****************************************************************************/

/** \file
	Mapping of the OS functions used by the Command Line Interface.
*/

#ifdef __cplusplus
extern "C"
{
#endif


/** CLI version defines */
#define CLI_VER_REVISION	2
#define CLI_VER_FEATURE		1
#define CLI_VER_MINOR		0
#ifndef CLI_VER_BUILD_STEP
#	define CLI_VER_BUILD_STEP	0
#endif

/** CLI version (hex) */
#define CLI_VERSION \
		(  (CLI_VER_REVISION << 24) \
		 ||(CLI_VER_FEATURE  << 16) \
		 ||(CLI_VER_MINOR  << 8) \
		 ||(CLI_VER_BUILD_STEP) )


/** CLI version (string) */
#if (CLI_VER_BUILD_STEP == 0)
#define CLI_VERSION_STR \
	_MKSTR(CLI_VER_REVISION) "." \
	_MKSTR(CLI_VER_FEATURE) "." \
	_MKSTR(CLI_VER_MINOR) ".\0"
#else
#define CLI_VERSION_STR \
	_MKSTR(CLI_VER_REVISION) "." \
	_MKSTR(CLI_VER_FEATURE) "." \
	_MKSTR(CLI_VER_MINOR) "." \
	_MKSTR(CLI_VER_BUILD_STEP) ".\0"
#endif

/** CLI version, base name */
#define CLI_BASE_NAME  "CLI Library"

/** CLI version, what string */
#define CLI_WHAT_STR  "@(#)" CLI_BASE_NAME ", Version " CLI_VERSION_STR

#ifndef CLI_DEBUG_PRINTOUT
	/** enable debug printout for the CLI lib */
#	define CLI_DEBUG_PRINTOUT 1
#endif

#ifndef CLI_ERROR_PRINTOUT
	/** enable error printout for the CLI lib */
#	define CLI_ERROR_PRINTOUT 1
#endif

#ifndef CLI_SUPPORT_BUFFER_OUT
	/** enable user function with printout via a buffer */
#	define CLI_SUPPORT_BUFFER_OUT	1
#endif

#ifndef CLI_SUPPORT_FILE_OUT
	/** enable user function with printout via a file descriptor */
#	if defined(LINUX) && defined(__KERNEL__)
#		define CLI_SUPPORT_FILE_OUT	0
#	else
#		define CLI_SUPPORT_FILE_OUT	1
#	endif
#endif

#if (CLI_SUPPORT_BUFFER_OUT == 0) && (CLI_SUPPORT_FILE_OUT == 0)
#	error "you have to specify at least one output type (printout via buffer or file)"
#endif

#if defined(LINUX) && defined(__KERNEL__) && (CLI_SUPPORT_FILE_OUT == 1)
#	error "sorry - printout via file not supported for the kernel space"
#endif


#ifndef CLI_SUPPORT_CONSOLE
	/** enable CLI console */
#	define CLI_SUPPORT_CONSOLE	1
#endif

#if defined(LINUX) && !defined(__KERNEL__)
#	ifndef CLI_SUPPORT_PIPE
		/** enable CLI pipe interface */
#		define CLI_SUPPORT_PIPE	1
#	endif
#else
#	ifdef CLI_SUPPORT_PIPE
#		undef CLI_SUPPORT_PIPE
#	endif
#	define CLI_SUPPORT_PIPE	0
#endif

#ifndef MAX_CLI_PIPES
#	define MAX_CLI_PIPES	3
#endif


#ifndef CLI_MOD_SUPPORT_PROCFS
	/** Linux Kernel Module - enable proc fs */
#	define CLI_MOD_SUPPORT_PROCFS	1
#endif

#ifdef __cplusplus
/* extern "C" */
}
#endif

#endif   /* #define _LIB_CLI_CONFIG_H */

