# Minuta build.
#   make win   MINUTA.EXE (Win32 GUI) and MD2HTMLW.EXE (Win32 console)
#   make dos   MD2HTMLD.EXE (DOS/32A, self-contained)
#   make test  core unit tests, built natively
# Open Watcom 2.0 lives in ~/vms/retro/toolchain by default; override WATCOM.
# Its host binaries: armo64 on an Apple Silicon Mac, binl64 on x86-64 Linux.

WATCOM ?= $(HOME)/vms/retro/toolchain/ow2
OW_HOST ?= $(if $(filter Linux,$(shell uname -s)),binl64,armo64)
export WATCOM
export PATH := $(WATCOM)/$(OW_HOST):$(PATH):$(WATCOM)/binw
export INCLUDE := $(WATCOM)/h

CC := clang
OWCC := $(WATCOM)/$(OW_HOST)/owcc
WRC := $(WATCOM)/$(OW_HOST)/wrc

BUILD := build
OUT := $(BUILD)/bin

MD4C_DEFINES := -DMD_VERSION_MAJOR=0 -DMD_VERSION_MINOR=6 -DMD_VERSION_RELEASE=0
INCLUDES := -Ivendor/md4c/src -Ivendor/md4c/md2html -Isrc/core

MD4C_SRC := vendor/md4c/src/md4c.c vendor/md4c/src/md4c-html.c vendor/md4c/src/entity.c
CORE_SRC := $(wildcard src/core/*.c)
CLI_SRC := src/cli/md2html.c vendor/md4c/md2html/cmdline.c
CLI_CORE_SRC := src/core/encoding.c
WIN_SRC := $(wildcard src/win/*.c)
WIN_RC := src/win/minuta.rc
TEST_SRC := $(wildcard tests/unit/*.c)

vpath %.c src/core src/cli src/win vendor/md4c/src vendor/md4c/md2html tests/unit

objects = $(addprefix $(1)/,$(notdir $(2:.c=.$(3))))

# Our code builds at warning level 4 with warnings as errors; vendored md4c
# keeps its upstream code, so it only gets the default level.
OW_COMMON := -std=c99 -O2 -s $(MD4C_DEFINES) $(INCLUDES)
OW_STRICT := -Wall -Werror
OW_VENDOR := -Wlevel=1 -Wc,-wcd=107

NT_DIR := $(BUILD)/nt
DOS_DIR := $(BUILD)/dos
HOST_DIR := $(BUILD)/host

NT_MD4C := $(call objects,$(NT_DIR),$(MD4C_SRC),obj)
NT_CORE := $(call objects,$(NT_DIR),$(CORE_SRC),obj)
NT_CLI := $(call objects,$(NT_DIR),$(CLI_SRC),obj)
NT_CLI_CORE := $(call objects,$(NT_DIR),$(CLI_CORE_SRC),obj)
NT_WIN := $(call objects,$(NT_DIR),$(WIN_SRC),obj)
DOS_MD4C := $(call objects,$(DOS_DIR),$(MD4C_SRC),obj)
DOS_CORE := $(call objects,$(DOS_DIR),$(CLI_CORE_SRC),obj)
DOS_CLI := $(call objects,$(DOS_DIR),$(CLI_SRC),obj)

OW_WARN := $(OW_STRICT)
VENDOR_OBJS := $(NT_MD4C) $(DOS_MD4C) $(NT_DIR)/cmdline.obj $(DOS_DIR)/cmdline.obj
$(VENDOR_OBJS): OW_WARN := $(OW_VENDOR)

VERSION := 0.1.1
DIST := $(BUILD)/dist

.PHONY: all win dos test dist clean

all: win dos

win: $(OUT)/MD2HTMLW.EXE $(if $(WIN_SRC),$(OUT)/MINUTA.EXE)
dos: $(OUT)/MD2HTMLD.EXE

$(NT_DIR)/%.obj: %.c | $(NT_DIR)
	$(OWCC) -bnt -c $(OW_COMMON) $(OW_WARN) -I$(WATCOM)/h/nt -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -o $@ $<

$(DOS_DIR)/%.obj: %.c | $(DOS_DIR)
	$(OWCC) -bdos32a -c $(OW_COMMON) $(OW_WARN) -o $@ $<

$(NT_DIR)/minuta.res: $(WIN_RC) $(wildcard src/win/*.h) $(wildcard res/*.ICO) | $(NT_DIR)
	$(WRC) -q -r -bt=nt -i=$(WATCOM)/h/nt -i=src/win -i=res -fo=$@ $<

$(OUT)/MINUTA.EXE: $(NT_WIN) $(NT_CORE) $(NT_MD4C) $(NT_DIR)/minuta.res | $(OUT)
	$(OWCC) -bnt_win -s -o $@ $(NT_WIN) $(NT_CORE) $(NT_MD4C) -Wl,"option resource=$(NT_DIR)/minuta.res" \
		-Wl,"library comctl32.lib,comdlg32.lib"

$(OUT)/MD2HTMLW.EXE: $(NT_CLI) $(NT_CLI_CORE) $(NT_MD4C) | $(OUT)
	$(OWCC) -bnt -s -o $@ $^

$(OUT)/MD2HTMLD.EXE: $(DOS_CLI) $(DOS_CORE) $(DOS_MD4C) | $(OUT)
	$(OWCC) -bdos32a -s -o $@ $^

# Native build for the unit tests and for comparing md2html output on the Mac.
HOST_CFLAGS := -std=c99 -O1 -g -Wall -Wextra -Wpedantic -Werror $(MD4C_DEFINES) $(INCLUDES)
HOST_VENDOR_CFLAGS := -std=c99 -O1 -g -w $(MD4C_DEFINES) $(INCLUDES)
HOST_MD4C := $(call objects,$(HOST_DIR),$(MD4C_SRC),o)
HOST_CORE := $(call objects,$(HOST_DIR),$(CORE_SRC),o)
HOST_TESTS := $(call objects,$(HOST_DIR),$(TEST_SRC),o)
HOST_CLI := $(call objects,$(HOST_DIR),$(CLI_SRC),o)

$(HOST_MD4C) $(HOST_DIR)/cmdline.o: HOST_WARN := vendor

$(HOST_DIR)/%.o: %.c | $(HOST_DIR)
	$(CC) -c -MMD -MP $(if $(HOST_WARN),$(HOST_VENDOR_CFLAGS),$(HOST_CFLAGS)) -Itests/unit -o $@ $<

$(HOST_DIR)/unit-tests: $(HOST_TESTS) $(HOST_CORE) $(HOST_MD4C)
	$(CC) -o $@ $^

$(HOST_DIR)/md2html: $(HOST_CLI) $(call objects,$(HOST_DIR),$(CLI_CORE_SRC),o) $(HOST_MD4C)
	$(CC) -o $@ $^

test: $(HOST_DIR)/unit-tests $(HOST_DIR)/md2html
	$(HOST_DIR)/unit-tests
	tests/cli/run.sh $(HOST_DIR)/md2html

-include $(wildcard $(HOST_DIR)/*.d)

# Watcom has no dependency output; rebuild its objects when any header
# changes. (Explicit targets: a pattern rule without a recipe does nothing.)
ALL_HEADERS := $(wildcard src/*/*.h vendor/md4c/src/*.h vendor/md4c/md2html/*.h)
$(sort $(NT_MD4C) $(NT_CORE) $(NT_CLI) $(NT_WIN) $(DOS_MD4C) $(DOS_CORE) $(DOS_CLI)): $(ALL_HEADERS)

$(NT_DIR) $(DOS_DIR) $(HOST_DIR) $(OUT):
	mkdir -p $@

# Release ZIPs and installer (makensis 3, ANSI build); text files in CRLF,
# README.TXT in Windows-1252.
dist: win dos
	rm -rf $(DIST)
	mkdir -p $(DIST)/win98 $(DIST)/dos
	cp $(OUT)/MINUTA.EXE $(OUT)/MD2HTMLW.EXE $(DIST)/win98/
	iconv -f UTF-8 -t CP1252 dist/README.TXT | LC_ALL=C sed 's/$$/\r/' > $(DIST)/win98/README.TXT
	LC_ALL=C sed 's/$$/\r/' LICENSE > $(DIST)/win98/LICENSE.TXT
	cp $(OUT)/MD2HTMLD.EXE $(DIST)/win98/LICENSE.TXT $(DIST)/dos/
	cd $(DIST)/win98 && zip -X -q ../minuta-$(VERSION)-win98.zip MINUTA.EXE MD2HTMLW.EXE README.TXT LICENSE.TXT
	cd $(DIST)/dos && zip -X -q ../minuta-$(VERSION)-dos.zip MD2HTMLD.EXE LICENSE.TXT
	cd dist && makensis -V2 -DVERSION=$(VERSION) minuta.nsi
	ls -l $(DIST)/*.zip $(DIST)/*.exe

clean:
	rm -rf $(BUILD)
