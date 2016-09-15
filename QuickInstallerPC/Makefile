NAME := QuickInstallerPC

TK_COMMA:= ,
TK_EMPTY:=
TK_SPACE:= $(TK_EMPTY) $(TK_EMPTY)

SHELL := /bin/bash
CSC := mcs
SRC_FILES := $(wildcard QuickInstallerPC/*.cs)
LIB_FILES := $(wildcard Libs/*.dll)
LIB_FILES += System System.Core
LIB_FILES_LINK := $(subst $(TK_SPACE),$(TK_COMMA),$(LIB_FILES))

all: $(SRC_FILES)
	$(CSC) -out:$(NAME).exe -sdk:4.5 -warn:4 -r:$(LIB_FILES_LINK) $(SRC_FILES)

.PHONY: clean

clean:
	rm -f $(NAME).exe
