.PHONY: all clean sockets

GPP = g++
OUTDIR = ./Release
OUTFILE = $(OUTDIR)/sockets.so

COMPILE_FLAGS = -c -fPIC -m32 -std=c++11 -O3 -w -D LINUX -I ./amx/

all: sockets

clean:
	-rm *.o
	-rm -r $(OUTDIR)

$(OUTDIR):
	mkdir -p $(OUTDIR)

sockets: clean $(OUTDIR)
	$(GPP) $(COMPILE_FLAGS) *.cpp
	$(GPP) -O2 -fPIC -m32 -std=c++11 -fshort-wchar -shared -o $(OUTFILE) *.o
	-rm *.o
