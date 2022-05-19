DYNINST_INCL = $(DYNINST_INSTALL)/include
DYNINST_LIB = $(DYNINST_INSTALL)/lib

COMMON_LIBS = -lparseAPI -linstructionAPI -lsymtabAPI -lcommon

PROG = call_analyzer
SRC = call_analyzer.cpp

GCC_FLAGS = -O0 -g3
GCC_FLAGS +=-Wall -W
ifdef DYNINST_INSTALL
GCC_FLAGS += -I $(DYNINST_INCL) -L $(DYNINST_LIB)
GCC_FLAGS += -Wl,-rpath=$(DYNINST_LIB)
endif

GCC = g++ $(GCC_FLAGS)

all: $(PROG)

$(PROG): jsonWriter.h

$(PROG): $(SRC)
	$(GCC) -o $@ $< $(COMMON_LIBS)

clean:
	$(RM) $(PROG)
