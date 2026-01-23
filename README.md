# LeanTeX
![alt text](https://github.com/lmesh91/LeanTeX/raw/main/img/LeanTeX.png "LeanTeX")

LeanTeX is a tool that converts Lean 4 programs into LaTeX documents.

> ## Warning
> LeanTeX is currently in early stages of development. Currently it is only supported for term mode proofs that use solely propositional logic and do not have any explicit input variables. Expect many bugs and issues!

# Usage
```
Usage:
  leantex <file> [options]

Options:
  -j, --jixia        Specify a path to the Jixia binary.
  -l, --latex        Specify a path to the LaTeX compiler.
  -w, --working-dir  Specify the working directory.
  -h, --help         Display this help message.
  -i, --ini          Specify an INI configuration file.
  -v, --verbose      Set verbose logging.
  -q, --quiet        Set quiet logging.
  --language         Specify language used for translation.
```
Note that the input file must be in a Lean project that has already been built using Lean v4.24.0. For more details about the INI configuration file and the command options, see the [Documentation](https://github.com/lmesh91/LeanTeX/blob/main/docs.md).
# Installation
1. Download the latest release from the [Releases](https://github.com/lmesh91/LeanTeX/releases) page, or build the source code from scratch.
2. Install [Lean 4](https://lean-lang.org/install/).
3. Install and build [this fork of Jixia](https://github.com/lmesh91/jixia). You will have to modify the INI file or the command line to refer to the path of the built executable.
4. Install a LaTeX compiler. On Linux, pdftex is the supported option, while on Windows, miktex-pdftex is the supported option. You will have to modify the INI file or the command line to refer to the path of the program.