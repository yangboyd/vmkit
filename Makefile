#===- ./Makefile -------------------------------------------*- Makefile -*--===#
# 
#                     The vmkit project
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
#===------------------------------------------------------------------------===#

LEVEL := .

include $(LEVEL)/Makefile.config

# Top-Level vmkit Build Stages:
#
DIRS := lib tools

ifeq ($(WITH_LLVM_GCC), 1)
  DIRS += mmtk
endif

EXTRA_DIST=include

include $(LEVEL)/Makefile.common

#------------------------------------------------------------------------
# Make sure the generated headers are up-to-date. This must be kept in
# sync with the AC_CONFIG_HEADER invocations in autoconf/configure.ac
#------------------------------------------------------------------------
FilesToConfig := \
  include/llvm/Config/config.h \
  lib/JnJVM/Classpath/Classpath.h \
FilesToConfigPATH  := $(addprefix $(LLVM_OBJ_ROOT)/,$(FilesToConfig))
