#!/bin/bash
build_dir=build/build_linux
install_dir=install_x86_64_linux
config=Release
os=linux

while [ -n "$1" ]
do  
if [ "$1" = "--debug" ]; then
    config=Debug
elif [ "$1" = "--os" ]; then
    shift
    if [ "$1" = "macos" ]; then
        build_dir=build/build_mac
        install_dir=install_arm64_mac
        os=macos
    else
        echo "$1" is a not a supported OS
        exit
    fi
else
    echo "$1" is not a supported argument
    exit
fi
shift
done

rm -rf ../build/conan

export OFIQLIB_CONAN_DIR=../conan
export CONAN_FILE=conanfile.txt

echo using conan file ${CONAN_FILE}

if [ "$config" = "Release" ]
then
	conan install ${OFIQLIB_CONAN_DIR}/${CONAN_FILE} \
            --build missing \
            --profile:build ${OFIQLIB_CONAN_DIR}/conan_profile_release_${os}.txt \
            --profile:host ${OFIQLIB_CONAN_DIR}/conan_profile_release_${os}.txt \
            --output-folder=../build/conan \
            -g CMakeDeps \
            -g CMakeToolchain
else
        conan install ${OFIQLIB_CONAN_DIR}/${CONAN_FILE} \
        --build missing \
        --profile:build ${OFIQLIB_CONAN_DIR}/conan_profile_debug_${os}.txt \
        --profile:host ${OFIQLIB_CONAN_DIR}/conan_profile_debug_${os}.txt \
        --output-folder=../build/conan \
        -g CMakeDeps \
        -g CMakeToolchain
fi

cd ../
echo "Removing $build_dir"
rm -r $build_dir

echo "Generating build files"
cmake -S ./ -B $build_dir -DCMAKE_INSTALL_PREFIX=$install_dir -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
     -DOS=$os -DCMAKE_BUILD_TYPE=$config
cmake --build $build_dir --target install -j 8

echo "Building finished"
