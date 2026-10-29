# ---- Project ----
# WarpQuake2: id's GPL Quake II (3.19) on an RTG screen.  quake2/ is the
# engine, src/ the AmigaOS platform layer.  The game code and the software
# renderer are linked in, not loaded (GAME_HARD_LINKED, REF_HARD_LINKED).
TARGET      := WarpQuake2
BUILD_DIR   := build
# 'make' also copies the binary to TARGET_DIR (an emulator's shared drive, say)
# if that directory exists; 'make ftp' uploads to FTP_DIR on the Amiga.
TARGET_DIR  ?= ~/Documents/Amiga/amiga_uae_disk/
FTP_DIR     ?= WORK:Games/WarpQuake2

# ---- CPU ----
# FPU build only: the renderer and the game are float throughout.
CPU         ?= 68060
MATHLIB     := $(if $(filter 68040,$(CPU)),-lm040,-lm060)
# -O1, not -O2: vbcc 0.9h miscompiles some loops at -O2 (WarpQuake found it:
# a loop index stepped in the body together with a break).
OPT         ?= 1
# ASM=1: the 68060 routines in quake2/ref_soft/*.s replace their C versions
# (Q2_ASM in the C).  ASM=0 is the all-C reference, for CRC comparisons.
ASM         ?= 1
OBJ_DIR     := $(BUILD_DIR)/obj-$(CPU)-O$(OPT)$(if $(filter 0,$(ASM)),-C)
SUFFIX      := $(if $(filter 68040,$(CPU)),_040)$(if $(filter-out 1,$(OPT)),_O$(OPT))$(if $(filter 0,$(ASM)),_C)
BIN         := $(BUILD_DIR)/$(TARGET)$(SUFFIX)

# ---- SDK / Includes ----
# Override any of these on the command line or in the environment, e.g.
#   make SDK_DIR=/opt/amiga P96_SDK=~/sdk/Picasso96Develop/Include
SDK_DIR     ?= /opt/amiga_sdk
NDK_INC_C   ?= $(SDK_DIR)/NDK3.2R4/Include_H
VBCC        ?= $(SDK_DIR)/vbcc
# The Picasso96 developer headers (libraries/Picasso96.h; the Picasso96Develop
# archive), after the NDK: their proto/ and pragmas/ are for other compilers,
# vbcc has its own inline/ stubs.  The default is where they sit when this
# repo is checked out next to them.
P96_SDK     ?= ../csWarpAmigaSoftware/p96_gfx_driver/Picasso96Develop/Include
# AHI's developer headers (devices/ahi.h; the ahidev archive on Aminet), for
# src/amiga_ahi.c; vbcc has the proto/inline stubs.  The default is where
# they sit when this repo is checked out next to csWarpAmigaSoftware.
AHI_SDK     ?= ../csWarpAmigaSoftware/warpAHIDriver/ahidev_4.18/AHI/Developer/include/C
# Roadshow's network headers (bsdsocket.library), for src/amiga_net.c.
NET_INC     ?= $(SDK_DIR)/NDK3.2R4/SANA+RoadshowTCP-IP/netinclude
# AMIGA: the few engine changes that are AmigaOS-specific (the base directory).
DEF_FLAGS   := -DAMIGA -DGAME_HARD_LINKED -DREF_HARD_LINKED -DQ2_ASM=$(ASM) $(addprefix -D,$(DEFINES))

# ---- Toolchain ----
CC      := vc
AS      := vasmm68k_mot
RM      := rm -rf
MKDIR   := mkdir -p

# ---- Flags ----
OPTFLAG := -O$(OPT)
CFLAGS  = +aos68k_std -cpu=$(CPU) -fpu=$(CPU) -c99 $(OPTFLAG) -dontwarn=153 -dontwarn=214 -dontwarn=208 -dontwarn=81
LIBS    := -lamiga -ldebug $(MATHLIB)
LDFLAGS := $(LIBS)
ASFLAGS := -quiet -Fhunk -m68060 -Iquake2/ref_soft

# ---- Sources ----
# id's engine, less what another platform layer or a DLL build would bring:
# x86.c (i386 asm glue), sv_null.c (a server-less build), and of null/ only
# the CD stub (src/ is the rest of the platform layer, sound included).
ENGINE_DIRS := client qcommon server ref_soft game
ENGINE_SRC  := $(filter-out quake2/client/x86.c quake2/server/sv_null.c,$(foreach d,$(ENGINE_DIRS),$(wildcard quake2/$(d)/*.c))) \
               quake2/null/cd_null.c
ENGINE_OBJS := $(patsubst quake2/%.c,$(OBJ_DIR)/quake2/%.o,$(ENGINE_SRC))
ENGINE_ASM  := $(if $(filter 1,$(ASM)),$(wildcard quake2/ref_soft/*.s))
ENGINE_OBJS += $(patsubst quake2/%.s,$(OBJ_DIR)/quake2/%.o,$(ENGINE_ASM))
APP_SRC     := $(wildcard src/*.c)
APP_OBJS    := $(patsubst src/%.c,$(OBJ_DIR)/src/%.o,$(APP_SRC))
APP_OBJS    += $(patsubst src/%.s,$(OBJ_DIR)/src/%.o,$(wildcard src/*.s))
OBJS        := $(APP_OBJS) $(ENGINE_OBJS)

ENGINE_HDRS := $(foreach d,$(ENGINE_DIRS),$(wildcard quake2/$(d)/*.h))

.PHONY: all app prof clean distclean install ftp

# 'all' does not clean first; 'make -j8' is worth it.
all: app install

app: $(BIN)

$(BIN): $(OBJS)
	@echo '------  LINK $@ ------'
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

# The platform layer sees the OS headers; the engine never does.
$(OBJ_DIR)/src/%.o: src/%.c $(wildcard src/*.h) $(ENGINE_HDRS)
	@$(MKDIR) $(@D)
	$(CC) $(CFLAGS) -Isrc -I$(NDK_INC_C) -I$(P96_SDK) -I$(AHI_SDK) $(DEF_FLAGS) -c $< -o $@

# The TCP/IP stack's headers (Roadshow's, in the NDK) only for the socket code.
$(OBJ_DIR)/src/amiga_net.o: src/amiga_net.c src/amiga_q2.h
	@$(MKDIR) $(@D)
	$(CC) $(CFLAGS) -Isrc -I$(NDK_INC_C) -I$(NET_INC) $(DEF_FLAGS) -c $< -o $@

# Engine objects quietly, the log printed only on failure.
$(OBJ_DIR)/quake2/%.o: quake2/%.c $(ENGINE_HDRS) $(wildcard src/q2prof.h src/amiga_q2.h)
	@$(MKDIR) $(@D)
	@echo "  CC $<"
	@$(CC) $(CFLAGS) $(DEF_FLAGS) -c $< -o $@ > $@.log 2>&1 || { cat $@.log; exit 1; }

$(OBJ_DIR)/quake2/%.o: quake2/%.s
	@$(MKDIR) $(@D)
	@echo "  AS $<"
	@$(AS) $(ASFLAGS) -o $@ $< > $@.log 2>&1 || { cat $@.log; exit 1; }

$(OBJ_DIR)/src/%.o: src/%.s
	@$(MKDIR) $(@D)
	$(AS) $(ASFLAGS) -o $@ $<

# The 16 bpp builds of the asm drawers: X060rgb.s sets PIX16 and includes
# X060.s, so it is rebuilt when that changes.
$(foreach f,$(wildcard quake2/ref_soft/*060rgb.s),$(eval $(OBJ_DIR)/quake2/ref_soft/$(notdir $(f:.s=.o)): $(f:rgb.s=.s)))

# For profiling: the same objects linked without stripping, plus a map.
VBCC_LIB := $(VBCC)/targets/m68k-amigaos/lib
prof: $(OBJS)
	vlink -bamigahunk -Bstatic -Cvbcc -nostdlib -mrel $(VBCC_LIB)/startup.o $(OBJS) \
		-L$(VBCC_LIB) -lamiga -ldebug $(MATHLIB) -lvc -M$(BUILD_DIR)/$(TARGET)_sym.map -o $(BUILD_DIR)/$(TARGET)_sym

clean:
	@echo '------  CLEAN  ------'
	$(RM) $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(TARGET)_* $(BUILD_DIR)/obj-*/src

distclean:
	$(RM) $(BUILD_DIR)

install: $(BIN)
	@if [ -d $(TARGET_DIR) ]; then echo '------ INSTALL ------'; cp $(BIN) $(TARGET_DIR); fi

# Uploads: tools/amiput.py, with the Amiga's FTP login from the AMIGA_FTP_*
# environment variables or tools/ftp_config.local (git-ignored; see
# tools/ftp_config.local.example).
FTP_PUT ?= python3 tools/amiput.py

ftp: app
	$(FTP_PUT) $(BIN) -d $(FTP_DIR)
