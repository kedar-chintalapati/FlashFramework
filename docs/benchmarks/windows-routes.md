# Windows generated route measurements

These measurements describe commit
`48a414fb7a92280230722aac9c039f644807f957`.

## Environment and method

The machine and compiler match the initial Windows report. Each result used a
fresh Release build directory, GCC 16.2.0, native optimization, and at most two
build jobs. Conan dependencies were already present. The timer covers the build
of one generated target after CMake configuration. Compiler working set was
sampled every 200 milliseconds. Each completed executable matched its last route
before the result was accepted.

## Results

| Routes | Outcome | Build seconds | Peak compiler bytes | Executable bytes | Text bytes |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1 | Passed | 1.708 | 93,327,360 | 114,666 | 19,804 |
| 10 | Passed | 2.102 | 116,928,512 | 201,961 | 105,248 |
| 100 | Passed | 30.177 | 573,796,352 | 1,089,727 | 957,036 |
| 1,000 | Memory limit | 65.093 | 2,150,912,000 | Not produced | Not produced |

The 100 route case stayed below the proposed 2 GiB compiler memory budget. The
1,000 route case was run with a 2 GiB safety limit and was stopped when the next
sample crossed it. No compiler or build process remained after the stop.

Text grew by about 9.47 KiB for each added route between the one and 100 route
targets. This is above the proposed 2 KiB investigation threshold. The current
matcher emits endpoint consideration for every route. A generated trie or a
shared table based matcher is required before claiming practical support for
1,000 routes.

The source generator creates targets for 1, 10, 100, and 1,000 routes. Run one
measurement with `benchmarks/measure-synthetic.ps1`. Results remain below
`build` and the script writes a JSON report for both successful builds and memory
limit stops.
