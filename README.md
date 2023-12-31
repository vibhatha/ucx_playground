# ucx_playground

## Pre-requisites

Build and install UCX locally. Refer to the [UCX Github releases](https://github.com/openucx/ucx/releases).
Pick a release that you prefer and build locally. 
Then include it to the `LD_LIBRARY_PATH`

```bash
export UCX_HOME=<your-path-to-ucx>
export LD_LIBRARY_PATH=$UCX_HOME/build/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}
export PATH=$PATH:$UCX_HOME/build/bin
```

## Build Project

```bash
cd cpp
mkdir build
cd build
cmake ../
make
```

## Run Sample App

```bash
cd cpp/build
./main
```