## Process this file with automake to produce Makefile.in

if BUILD_APPL_EXAMPLE
bin_PROGRAMS = cli_example
endif

if ENABLE_LINUX
lib_LTLIBRARIES = libcli.la
endif ENABLE_LINUX

cli_interface_header_sources = \
	lib_cli_config.h \
	lib_cli_core.h\
	lib_cli_console.h\
	lib_cli_pipe.h

cli_extra_header_sources = \
	lib_cli_osmap.h\
	lib_cli_debug.h

cli_core_sources = \
	lib_cli_core.c

cli_console_sources = \
	lib_cli_console.c

cli_pipe_sources = \
	lib_cli_pipe.c

cli_example_sources = \
	cli_example.c


AM_CPPFLAGS = \
	-I@srcdir@\
	-I@KERNEL_INCL_PATH@ \
	-I@KERNEL_BUILD_PATH@/include \
	-I@KERNEL_BUILD_PATH@/include2 \
	$(IFXOS_INCLUDE_PATH)

if HAVE_GCC
AM_CFLAGS = \
	-Wall -Wimplicit -Wunused -Wundef -Wswitch -Wcomment \
	-Wuninitialized -Wparentheses -Wreturn-type

additional_flags =

AM_LDFLAGS = \
	-L@top_builddir@/src \
	@IFXOS_LIBRARY_PATH@ \
	-Bstatic -dn -static

else !HAVE_GCC

AM_CFLAGS =
additional_flags =

AM_LDFLAGS = \
	-dn -Bstatic

endif !HAVE_GCC

if ENABLE_DEBUG
optimization_flags = -O1 -g
else
optimization_flags = -O1
endif

if ENABLE_LINUX
additional_flags +=\
	-DLINUX \
	-DRW_MULTI_THREAD \
	-D_REENTRANT \
	-D_THREAD_SAFE \
	-D_POSIX_SOURCE \
	-D_GNU_SOURCE \
	-Wstrict-aliasing
endif

if ENABLE_GENERIC
additional_flags += \
	-DGENERIC_OS \
	-DRW_MULTI_THREAD \
	-D_REENTRANT \
	-D_THREAD_SAFE \
	-D_POSIX_SOURCE -D_GNU_SOURCE
endif

if ENABLE_LINT
lint_file=std_generic.lnt
endif

libcli_cflags = \
	${LIB_CLI_FEATURE_OPTIONS} \
	$(LIB_CLI_TARGET_OPTIONS)

if HAVE_IFXOS_SYSOBJ_SUPPORT
libcli_cflags += -DHAVE_IFXOS_SYSOBJ_SUPPORT=1
endif

if ENABLE_LINUX
libcli_ladir = /include/cli
libcli_la_HEADERS = $(cli_interface_header_sources)
libcli_la_LDFLAGS = -version-info 1:0:0


libcli_la_SOURCES = \
	$(cli_interface_header_sources) \
	$(cli_extra_header_sources) \
	$(cli_core_sources)

if ENABLE_CLI_CONSOLE
libcli_la_SOURCES += $(cli_console_sources)
endif

if ENABLE_CLI_PIPE
libcli_la_SOURCES += $(cli_pipe_sources)
endif

libcli_la_CFLAGS = \
	$(AM_CFLAGS)\
	$(additional_flags)\
	$(optimization_flags)\
	$(libcli_cflags)

endif ENABLE_LINUX


if BUILD_APPL_EXAMPLE
cli_example_SOURCES = \
	$(cli_interface_header_sources) \
	$(cli_extra_header_sources) \
	$(cli_example_sources)

cli_example_CFLAGS = \
	$(AM_CFLAGS)\
	$(additional_flags)\
	$(optimization_flags)\
	$(libcli_cflags)

cli_example_LDADD = \
	-lcli \
	-lifxos \
	-lpthread \
	-lrt

endif BUILD_APPL_EXAMPLE

if ENABLE_LINUX
if WITH_KERNEL_MODULE
bin_PROGRAMS = mod_cli.ko

mod_cli_ko_cflags = \
	$(LIB_CLI_TARGET_OPTIONS)\
	-DCLI_SUPPORT_FILE_OUT=0

mod_cli_sources = \
	$(cli_interface_header_sources) \
	$(cli_extra_header_sources) \
	$(cli_core_sources) \
	mod_cli_linux.c

mod_cli_ko_CFLAGS = \
	$(AM_CFLAGS)\
	$(additional_flags)\
	$(optimization_flags)\
	$(mod_cli_ko_cflags)

# linux 2.6 kernel object
# linux 2.6 kernel object - dummy to force dependencies
mod_cli_ko_SOURCES = ../ChangeLog

mod_cli_ko_OBJS = "$(subst .c,.o,$(filter %.c,$(mod_cli_sources)))"
mod_cli.ko: $(mod_cli_sources)
	@echo -e "mod_cli: Making Linux 2.6.x kernel object"
	if test ! -e lib_cli_core.c ; then \
		echo "copy source files (as links only!)"; \
		for f in $(filter %.c,$(mod_cli_sources)); do \
			mkdir -p $(PWD)/`dirname $$f`/ ; \
			cp -sf $(addprefix @abs_srcdir@/,$$f) $(PWD)/`dirname $$f`/ ; \
		done \
	fi
	@echo -e "# mod_cli: Generated to build Linux 2.6.x kernel object" > $(PWD)/Kbuild
	@echo -e "obj-m := $(subst .ko,.o,$@)"			>> $(PWD)/Kbuild
	@echo -e "$(subst .ko,,$@)-y := $(mod_cli_ko_OBJS)"	>> $(PWD)/Kbuild
	@echo -e "EXTRA_CFLAGS := -DHAVE_CONFIG_H -I@abs_srcdir@ -I@abs_srcdir@/include $(IFXOS_INCLUDE_PATH)" >> $(PWD)/Kbuild
	$(MAKE) CONFIG_DEBUG_SECTION_MISMATCH=y ARCH=@KERNEL_ARCH@ -C @KERNEL_BUILD_PATH@ O=@KERNEL_BUILD_PATH@ M=$(PWD) modules


clean-generic:
	@echo "Cleanup Linux 2.6.x kernel object build"
	@- find . -name ".*.cmd" | xargs rm -f
	@- find . -name "*.o" | xargs rm -f
	@- rm -f Module.symvers Kbuild
	@- rm -rf .tmp_versions *.mod.c *.order


endif WITH_KERNEL_MODULE
endif ENABLE_LINUX


# all sources for the current library distribution
EXTRA_DIST = \
	$(cli_interface_header_sources) \
	$(cli_extra_header_sources) \
	$(cli_core_sources) \
	$(cli_console_sources) \
	$(cli_pipe_sources) \
	$(cli_example_sources)

lint:
	flint +b -vm \
	-i$(shell dirname `$(CC) -print-file-name=include`)/include \
	$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(libcli_cflags) \
	-i@top_srcdir@ -i@srcdir@ $(lint_file) \
	$(cli_core_sources) \
	$(cli_console_sources) \
	$(cli_pipe_sources) \
	$(cli_example_sources)

