# Haiku build of Minuta (run inside Haiku; see tools/haiku-vm/build).
#   make          objects/Minuta
#   make install  copy it to ~/config/non-packaged/apps and add a `minuta`
#                 command to ~/config/non-packaged/bin
#   make clean
# Plain g++/gcc: the core and md4c are C, the shell is C++ on the BeAPI.

ROOT := ../..
CORE := $(ROOT)/src/core
MD4C := $(ROOT)/vendor/md4c/src
OBJ := objects
APP := $(OBJ)/Minuta

MD4C_DEFINES := -DMD_VERSION_MAJOR=0 -DMD_VERSION_MINOR=6 -DMD_VERSION_RELEASE=0
INCLUDES := -I$(CORE) -I$(MD4C)

CFLAGS := -std=c99 -O2 -Wall -Wextra $(MD4C_DEFINES) $(INCLUDES)
VENDOR_CFLAGS := -std=c99 -O2 -w $(MD4C_DEFINES) $(INCLUDES)
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra $(MD4C_DEFINES) $(INCLUDES)
LIBS := -lbe -ltracker

CORE_SRC := buffer.c document.c document_utf8.c encoding.c rtf.c scanner.c stats.c \
	stats_utf8.c styled.c
MD4C_SRC := md4c.c md4c-html.c entity.c
APP_SRC := $(wildcard *.cpp)

CORE_OBJ := $(addprefix $(OBJ)/,$(CORE_SRC:.c=.o))
MD4C_OBJ := $(addprefix $(OBJ)/,$(MD4C_SRC:.c=.o))
APP_OBJ := $(addprefix $(OBJ)/,$(APP_SRC:.cpp=.o))

APPS_DIR = $(shell finddir B_USER_NONPACKAGED_DIRECTORY)/apps
BIN_DIR = $(shell finddir B_USER_NONPACKAGED_BIN_DIRECTORY)

.PHONY: all install clean

all: $(APP)

$(APP): $(APP_OBJ) $(CORE_OBJ) $(MD4C_OBJ) $(OBJ)/Minuta.rsrc
	g++ -o $@ $(APP_OBJ) $(CORE_OBJ) $(MD4C_OBJ) $(LIBS)
	xres -o $@ $(OBJ)/Minuta.rsrc
	mimeset -f $@

$(OBJ)/%.o: %.cpp $(wildcard *.h) | $(OBJ)
	g++ -c $(CXXFLAGS) -o $@ $<

$(OBJ)/%.o: $(CORE)/%.c $(wildcard $(CORE)/*.h) | $(OBJ)
	gcc -c $(CFLAGS) -o $@ $<

$(OBJ)/%.o: $(MD4C)/%.c | $(OBJ)
	gcc -c $(VENDOR_CFLAGS) -o $@ $<

$(OBJ)/Minuta.rsrc: Minuta.rdef | $(OBJ)
	rc -o $@ $<

$(OBJ):
	mkdir -p $@

install: $(APP)
	mkdir -p $(APPS_DIR) $(BIN_DIR)
	cp $(APP) $(APPS_DIR)/Minuta
	mimeset -f $(APPS_DIR)/Minuta
	ln -sf $(APPS_DIR)/Minuta $(BIN_DIR)/minuta

clean:
	rm -rf $(OBJ)
