# Documentation
## Command Line/INI options
All options can be used in either an INI file or as command line flags, unless otherwise specified. Note that command line flags take priority over options in the INI file.


| CLI Flag              | INI Option   | Description                                                                                                     |
| --------------------- | ------------ | --------------------------------------------------------------------------------------------------------------- |
| `-j`, `--jixia`       | `Jixia`      | Specifies a path to the Jixia binary.                                                                           |
| `-l`, `--latex`       | `LaTeX`      | Specifies a path (or command name) for the LaTeX compiler.                                                      |
| `-w`, `--working-dir` | `WorkingDir` | Specifies the working directory, used for temporary files as well as the output. Default: `.leantex`            |
| `--language`          | `Language`   | Specifies a language to use for the outputted proofs. Default: `en_us`                                          |
| (default)             | `CodePath`   | Specifies a path to the Lean program to be processed. Note that all programs must be within a compiled project. |
| `-i`, `--ini`         |              | Specifies a specific INI file to use. Default: `leantex.ini`                                                    |
| `-h`, `--help`        |              | Displays a help message.                                                                                        |
| `-v`, `--verbose`     |              | Sets verbose logging (shows debug messages)                                                                     |
| `-q, --quiet`         |              | Sets quiet logging (hides info messages)                                                                        |
## Localization JSON specifications
The translations for individual constants in Lean are stored as JSON files in the `data` folder. The keys at the root level refer to names of constants. Constants that are scoped are stored as children (for instance the information for `Classical.em` is stored in `Classical.children.em` in the JSON).

Each constant can have any of these translation modes:
- `text` (required): Used within a written sentence.
- `math`: Used inside a math block. Falls back to `text` if not included.
- `apply`: Used during partial function application. Falls back to `text` if not included.
- `intro`: Used when introducing variables. Falls back to `math` if not included.

Parameters of a function can be referred to inside angled brackets. The `.out` parameter refers to the output of the function. By default, the type of a parameter is used inside a translation; use the `@` symbol in the beginning to refer to its value. Additionally, to force a specific translation mode, add `:<mode>` at the end of the variable name, where `<mode>` is the first letter of the desired translation mode. For example, `<@h:m>` will translate the value of `h` into math mode.

The `priority` value is used in math mode to add parentheses. When specified, the parenthesis are placed such that operations of lower priority are automatically applied first, from left to right.