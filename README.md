# Yugawara - A fundamental language toolkit for SQL IR

## Requirements

* CMake `>= 3.10`
* C++ Compiler `>= C++17`
* and see *Dockerfile* section

```sh
# retrieve third party modules
git submodule update --init --recursive
```

### Dockerfile

```dockerfile
FROM ubuntu:18.04

RUN apt update -y && apt install -y git build-essential cmake ninja-build
```

optional packages:

* `doxygen`
* `graphviz`
* `clang-tidy-8`

### Install sub-modules

#### takatori

see [README](third_party/takatori/README.md).

## How to build

```sh
mkdir -p build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

available options:

* `-DCMAKE_INSTALL_PREFIX=/path/to/install-root` - change install location
* `-DBUILD_SHARED_LIBS=OFF` - create static libraries instead of shared libraries
* `-DBUILD_TESTS=OFF` - don't build test programs
* `-DBUILD_DOCUMENTS=OFF` - don't build documents by doxygen

### install

```sh
ninja install
```

### run tests

```sh
ctest
```

### generate documents

```sh
ninja doxygen
```

## License

[Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)
