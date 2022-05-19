# call\_analyzer

## Options

```
Usage: ./call_analyzer [options] infile [outfile]
  --compact-json   minify json output
  --all-calls      include all calls to non-external functions
  --help           print this message and exit
  --version        print version and exit
```

## Building

To build type `make` and the `call_analyzer` program will be created.  `make
clean` will remove the program.

If dyninst is not installed in a standard OS location, set the
`DYNINST_INSTALL` environment variable to the installation directory using
`export DYNINST_INSTALL=<PATH_DYNINST_INSTALL>`.

## Sample Output

Shows the relevant parts of JSON output file for the `main` function of the program (listing is below).  The
`main` function calls `printf` which the compiler optimizes to `puts` and then
calls `exit`.

```
{
  "functions": [                        # one entry per function defined
    {
      "funcName": "main",                 # name of function
      "funcAddr": 1408,                   # offset of function
      "sectionName": ".text",             # ELF section containing function
      "isInPlt": false,                   # true if function is to an external DSO
      "calls": [                          # one entry per called function
        {
          "callInstructionAddr": 1685,      # offset of call instruction
          "calledAddr": 1360,               # offset of called function
          "callToPlt": true,                # true if called function is to an external DSO
          "liveRegisters": [                # set of live registers at call
            "rdi"
          ],
          "funcNames": [                    # name(s) of called function (generally there is 1)
            "puts"
          ]
        },
        {
          "callInstructionAddr": 1695,
          "calledAddr": 1376,
          "callToPlt": true,
          "liveRegisters": [
            "rdi"
          ],
          "funcNames": [
            "exit"
          ]
        }
      ]
    },
    ...
  ]
}
```

## Example Program Listing

Source code the program that produced the JSON file above.

```
#include <stdio.h>
#include <stdlib.h>

int main()
{
    printf("hello world\n");
    exit(0);
}
```
