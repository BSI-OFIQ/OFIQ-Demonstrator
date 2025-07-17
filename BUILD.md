# Building the Demonstrator

## Checking out the project

To check out the repository run the following command from the command prompt or use your favorite git client:

``` bash
git clone https://github.com/BSI-OFIQ/OFIQ-Demonstrator.git
```

The demonstrator requires the OFIQ library which is located in a different repository and is added to this repository as a submodule. To check the OFIQ library out run the following command:

``` bash
# use other dir if you specified it in git clone command
cd OFIQ-Demonstrator
git submodule update --init
```

After that you'll find the OFIQ library in `./extern/OFIQ-Project`.

## Building the project

To be able to build the project you'll need the following tools:

- Python
- pip
- conan
- cmake
- build tools (compiler linker etc.)

### Windows

The following has been tested on a Windows 10 (64 bit) using a Python version 3.9.18 with pip package. Furthermore, an installation
of **cmake version 3.29** has been used. As the compiler, **Microsoft's Visual Studio 2019** was used.

To install conan open the command prompt and run

``` cmd
pip install conan==2.18.1
# This may be required to switch to wxWidgets v3.2.8
conan remote update conancenter --url="https://center2.conan.io"
```



In order to successfully build the demonstrator, the OFIQ library must first be built. To do this, the following command should be executed in the directory in which the demonstrator was checked out.

``` cmd
cd extern\OFIQ-Project\scripts
build.cmd
```

Once the OFIQ library has been successfully built, the demonstrator can be built. To install all required packages and build the OFIQ demonstrator execute the following command in the directory into which the demonstrator was checked out.

``` cmd
cd scripts
build.cmd
```

Once the OFIQ demonstrator has been successfully built, the folder `.\build\build_win\Release\` should contain the following files:

``` cmd
ofiq_lib.dll
OFIQDemonstrator.exe
onnxruntime.dll
```

To start the demonstrator run `OFIQDemonstrator.exe`

### Linux

The following has been tested on Ubuntu 24.04 x64. 

First, configure conan as described [here](https://github.com/BSI-OFIQ/OFIQ-Project/blob/main/BUILD.md#ubuntu-2404-x86_64) and ensure that you are workin in an activated python environment
 
``` bash
# use other dir if python environment is at another location
source /path/to/py_ofiq_env/bin/activate
```

Compile OFIQ.

``` bash
# use other dir if OFIQ-Demonstrator is at another location
cd /path/to/OFIQ-Demonstrator/extern/OFIQ-Project/scripts/
sh build.sh
```

Now, compile the demonstrator

``` bash
# use other dir if OFIQ-Demonstrator is at another location
cd /path/to/OFIQ-Demonstrator/scripts/
sh build.sh
```

NOTE: If you encounter problems with compiling wxWidgets, upgrade to Conan v2.18.1 and upgrade
the url of conancenter as follows.
``` bash
pip install conan==2.18.1
conan remote update conancenter --url="https://center2.conan.io"
```

After successful building, the folder `./build/build_linux/Release` should contain the following files:

``` bash
OFIQDemonstrator
ofiq_lib.so
onnxruntime.so.1.17.3
```

You can start the demonstrator as follows.

``` bash
# use other dir if OFIQ-Demonstrator is at another location
cd /path/to/OFIQ-Demonstrator/build/build_linux/
./OFIQDemonstrator
```

NOTE: If attempting to start the demonstrator results in an error on loading shared libraries, check and ensure that the directory `./` is in the library path, e.g., by running
``` bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./
```
before starting the demonstrator.

### MacOS

The following has been tested on Ubuntu 24.04 x64. 

First, configure conan as described [here](https://github.com/BSI-OFIQ/OFIQ-Project/blob/main/BUILD.md#ubuntu-2404-x86_64) and ensure that you are workin in an activated python environment
 
``` bash
# use other dir if python environment is at another location
source /path/to/py_ofiq_env/bin/activate
```

Compile OFIQ.

``` bash
# use other dir if OFIQ-Demonstrator is at another location
cd /path/to/OFIQ-Demonstrator/extern/OFIQ-Project/scripts/
sh build.sh
```

Now, compile the demonstrator

``` bash
# use other dir if OFIQ-Demonstrator is at another location
cd /path/to/OFIQ-Demonstrator/scripts/
sh build.sh
```

NOTE: If you encounter problems with compiling wxWidgets, upgrade to Conan v2.18.1 and upgrade
the url of conancenter as follows.
``` bash
pip install conan==2.18.1
conan remote update conancenter --url="https://center2.conan.io"
```

After successful building, the folder `./build/build_linux/Release` should contain the following files:

``` bash
OFIQDemonstrator
ofiq_lib.so
onnxruntime.so.1.17.3
```

You can start the demonstrator as follows.

``` bash
# use other dir if OFIQ-Demonstrator is at another location
cd /path/to/OFIQ-Demonstrator/build/build_linux/
./OFIQDemonstrator
```

NOTE: If attempting to start the demonstrator results in an error on loading shared libraries, check and ensure that the directory `./` is in the library path, e.g., by running
``` bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./
```
before starting the demonstrator.