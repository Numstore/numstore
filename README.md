<p align="center">
  <img src="docs/logo.svg" alt="NumStore Logo" width="200"/>
</p>

# NumStore

![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)
![Version](https://img.shields.io/badge/version-0.0.1-brightgreen.svg)
![Build](https://img.shields.io/badge/build-passing-success.svg)

Numstore is a database for contiguous bytes. 

It's essentially a file system, but in database format with log N mutations in the middle.

Numstore got its idea of an "R+Tree" which is just a rope equivalent to a B+Tree that keeps track of the count of bytes each node stores. It's designed for:

- log N Insertions anywhere in the byte stream (compared to N insertions in traditional file systems) 
- log N deletions anywhere in the byte stream
- log N reads 

When I say log N, I really mean it takes log N to find the location. Once it's found the location, it holds onto the bottom layer and it's real time. Ideally as fast as just appending to the file.

It's written entirely in C with just the lemon parser as a dependency and some posix apis (with a wrapper - I haven't prioritized testing cross platform, but I designed it in a way to work cross platform. That feature will come soon).

## Quick Start

### Building from Source

```bash
# Clone the repository
git clone https://gitlab.com/lincketheo/numstore.git
cd numstore

make 

$ ./build/debug/apps/test                                               # Run tests
$ ./build/debug/apps/examples/nsfslite/example1_basic_persistence       # Run example
```

## Project Structure

```
numstore/
├── libs/              # All libraries
│   ├── nscore/        # Numstore agnostic code and utilities
│   ├── nspager/       # Pager / WAL / Cursor code
│   └── apps/          # Specific user aware libraries
├── apps/              # Applications and tools
├── docs/              # Documentation
└── cmake/             # CMake build modules
```

## Contributing

I welcome contributions. This project is early, so to whoever is reading this, please just be considerate. 
I don't have the bandwidth to tell people what to contribute to yet. Feel free to ask me questions though

## License

NumStore is licensed under the [Apache License 2.0](LICENSE).

```
Copyright 2025 Theo Lincke

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
```

