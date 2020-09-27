
CXX = g++
OPTS1 = -O0 -Wall -Wno-switch -Wno-int-in-bool-context -Wunused-variable -Wtautological-compare
OPTS2 = -shared -fPIC -std=c++11
CXXFLAGS = $(OPTS1) $(OPTS2) -lboost_regex -lrexx -lrexxapi -lmagic

# MODULES = PBRO01A PCMD0A PCMD0B PDLGTST PDPANLA PEDIT01 PEDRXM1 PFLST0A PLRFLST1 PMAIN0A POREXX1 PPSP01A PSHELL0 PTEST01 PTUTORA

# OBJS = PBRO01A.o PCMD0A.o PCMD0B.o PDLGTST.o PDPANLA.o PEDIT01.o PEDRXM1.o PFLST0A.o PLRFLST1.o PMAIN0A.o POREXX1.o PPSP01A.o PSHELL0.o PTEST01.o PTUTORA.o

DYLIBS = libPBRO01A.dylib libPCMD0A.dylib libPCMD0B.dylib libPDLGTST.dylib libPDPANLA.dylib libPEDIT01.dylib libPEDRXM1.dylib libPFLST0A.dylib libPLRFLST1.dylib libPMAIN0A.dylib libPOREXX1.dylib libPPSP01A.dylib libPSHELL0.dylib libPTEST01.dylib libPTUTORA.dylib

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

lib%.dylib: %.o
	g++ -dynamiclib -undefined suppress -flat_namespace $< -o $@

all: $(DYLIBS)
.PHONY: all
