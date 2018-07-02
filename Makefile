
CCX=g++
DFLAGS=-ggdb -g -fno-omit-frame-pointer
#CXXFLAGS=-shared -fPIC -std=gnu++14 -O2 -Wall $(DFLAGS)
CXXFLAGS=-shared -fPIC -std=gnu++14 -O0 -Wall $(DFLAGS) \
	-fno-builtin-malloc -fno-builtin-free -fno-builtin-realloc \
	-fno-builtin-calloc -fno-builtin-cfree -fno-builtin-memalign \
	-fno-builtin-posix_memalign -fno-builtin-valloc -fno-builtin-pvalloc \
	-fno-builtin -fsized-deallocation

LDFLAGS=-ldl -pthread -latomic

OBJFILES=cmalloc.o pages.o pagemap.o thread_hooks.o lfbstree.o

default: cmalloc.so cmalloc.a

%.o : %.cpp
	$(CCX) $(CXXFLAGS) -c -o $@ $< $(LDFLAGS)

cmalloc.so: $(OBJFILES)
	$(CCX) $(CXXFLAGS) -o cmalloc.so $(OBJFILES) $(LDFLAGS)

cmalloc.a: $(OBJFILES)
	ar rcs cmalloc.a $(OBJFILES)

clean:
	rm -f *.so *.o *.a
