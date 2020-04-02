ifdef COMSPEC
DOTEXE:=.exe
else
DOTEXE:=
endif
EXENAME:=nsfid$(DOTEXE)

.PHONY: all
all: $(EXENAME)

$(EXENAME): nsfid.c
	gcc $^ -Wall -O3 -s -o $@