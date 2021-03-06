# LoFreq*: A sequence-quality aware, ultra-sensitive variant caller for NGS data



## Note

Most users will want to use either the binary or the source-code
package. Both are distributed via
[LoFreq's Sourceforge site](https://sourceforge.net/projects/lofreq/files/).
The source hosted here on github is mainly for developers!



## Building the Source

### Current Build Status

[![Build Status](https://travis-ci.org/CSB5/lofreq.svg?branch=master)](https://travis-ci.org/CSB5/lofreq)

### Prerequisites

You will need:

- a C compiler (e.g. gcc or clang)
- a Python 2.7 interpreter
- zlib developer files
- a compiled version of [samtools (>=1.1)]((http://sourceforge.net/projects/samtools/files/samtools/1.1/samtools-1.1.tar.bz2/download))
- a compiled version of htslib (>= 1.1; usually part of the above)

### Compilation

- Clone the repo (or download the current master as zip package and unpack)
- Run `./bootstrap` to set up the required automake files
  - If you get an error like `required file './ltmain.sh'
    not found`, run `libtoolize` (or `glibtoolize`) first and then
    `bootstrap` again
  - Subsequent pulls won't require rerunning `./bootstrap`. This is
    only necesary when changing `configure.ac` or any of the `Makefile.am`
- Run `./configure` with the **absolute** path to samtools and htslib
  (e.g. `./configure SAMTOOLS=/path-to-samtools HTSLIB=/path-to-htslib
  [--prefix=inst-path]`)
- Run `make`
  - At this point you can already start using lofreq: `./bin/lofreq`
- Run `make install` to properly install the package
  - Default is `/usr/local/`. If `--prefix` was given to `configure`,
    the corresponding argument is used
  - Depending on the used prefix you might need to adjust your PATH (and PYTHONPATH).


## Documentation

- Simply calling `lofreq` on the command line will display a list of
subcommands
- `lofreq cmd` will then display help for `cmd`
- See [LoFreq's website](http://csb5.github.io/lofreq/) for full documentation


## License

LoFreq is licensed under the MIT License (see LICENSE).

Licenses for third party software that is part of the source:
- cdflib90 (see src/cdflib90.README)
- uthash (see src/uthash/LICENSE)
