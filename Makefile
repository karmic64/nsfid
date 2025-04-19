ifdef COMSPEC
DOTEXE:=.exe
else
DOTEXE:=
endif
EXENAME:=nsfid$(DOTEXE)

.PHONY: all
all: $(EXENAME)

$(EXENAME): nsfid.c
	gcc $^ -Wall -Ofast -s -o $@