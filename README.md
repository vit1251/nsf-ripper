# nsf-ripper

NES game sound file (.nsf) music extractor to FLAC format

# depends
    - 
    - lib64flac-devel

# build



run
1. `cmake [PATH] -DWITH_OGG=OFF -DCMAKE_BUILD_TYPE=Release`
2. `cmake --build .`

how to use
==========

```
NsfRipper [NSF file]
```

for example

```
NsfRipper "Chip 'n Dale Rescue Rangers.nsf"
```
