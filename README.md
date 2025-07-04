# File Systems Project

## Build
The following command will build the executable `./fsutils`.
```bash
$ make
```

The following command will clean the binaries generated.
```bash
$ make clean
```


## Commands
### Phase 1
The flag `--info` will show the metadata of the file system.
```bash
$ ./fsutils --info <file system>
```

### Example
```bash
$ ./fsutils --info libfat
```
```bash
$ ./fsutils --info lolext
```

### Phase 2
The flag `--tree` will display a hierarchical tree representation of the specified file system.
```bash
$ ./fsutils --tree <file system>
```

### Example
```bash
$ ./fsutils --tree libfat
```
```bash
$ ./fsutils --tree lolext
```

### Phase 3
The flag `--cat` will show the content of the FAT16 file.
```bash
$ ./fsutils --cat <FAT16 file system> <file>
```

### Example
```bash
$ ./fsutils --cat studentfat100MB practica.c
```




## Resources
There are some resources that have been used to test the code ubicated in the `res` folder:
- D3V1Next
- DEVINfat
- fat1.fs
- libfat
- lolext
- studentext100MB
- studentfat100MB
