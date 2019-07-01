## Introduction

`coa` is a lock-free coalescing-capable mechanism that can manage page-sized
blocks, and is suitable to serve as the lowest layer of any memory allocator.

It is an implementation of the mechanism described in
[A Lock-Free Coalescing-Capable Mechanism for Memory Management](
https://dl.acm.org/citation.cfm?id=3329982)

## Building

To compile, download this repository and then run:
```console
make
```

## Usage

You can directly use `coa` by including `coa.h` in your application.
"Documentation" is available in `coa.h`. To use `coa.h`, you must also link
with the object files (`pages.o`, `pagemap.o`, ...) produced by the Makefile.

You can also use `coa` as a standalone allocator, as it implements the
`malloc()` interface. You can thus link `coa` with your application at
compile time with:
```console
-lcmalloc
```
or you can dynamically link it with your application by using LD_PRELOAD (if
your application was not already statically linked with another memory
allocator).
```console
LD_PRELOAD=cmalloc.so ./your_application
```

## TODOs and gotchas

`coa` internally uses a lock-free binary search tree to handle ownership of
blocks, which uses internal nodes that must be dynamically allocated.
Currently, the memory used for those internal nodes is never reclaimed nor
re-used, so naturally a continually increasing memory usage in long-living
programs is expected.

## Copyright

License: MIT

Read file [COPYING](COPYING).

