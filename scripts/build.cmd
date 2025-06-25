@echo off

set build_dir=build\build_win
set architecture=x64  
set install_dir=install_x86_64
set set_compiler=""
set generator="Visual Studio 16 2019"
set config=Release

:loop 
if "%1" == "" goto done
if /i "%1" == "--arch" set "architecture=%2"
if /i "%1" == "--compiler" set "set_compiler=%2"
if /i "%1" == "--debug" set config=Debug
shift
goto :loop
:done

@REM switch architecture
IF %architecture%==x86 (
  set conan_set_arch=-s:a arch=x86
  set architecture=Win32
  set install_dir=install_x86
) else (
  set "conan_set_arch="
)
set set_architecture=-A %architecture%
@REM  switch to compiler.version 17 based on parameter provided
IF %set_compiler%==17 (
  set generator="Visual Studio 17 2022"
  set compiler=-s:a compiler.version=193
) else if %set_compiler%==gcc (
  set generator="MinGW Makefiles"
  set compiler=-s:a compiler=gcc -s:a compiler.libcxx=libstdc++11 -s:a compiler.version=9
  set "set_architecture="

  echo Building on Windows with gcc is not supported
  goto :end
) else (
  set "compiler="
)

@REM build dependencies
erase /Q ..\build\conan\*.*
if %config%==Release (
    conan install ../conan/conanfile.txt --build missing ^
    --profile:build ../conan/conan_profile_release.txt ^
    --profile:host ../conan/conan_profile_release.txt ^
      %compiler% %conan_set_arch% ^
    --output-folder=../build/conan || goto end
) else (
    conan install ../conan/conanfile.txt --build missing ^
    --profile:build ../conan/conan_profile_debug.txt ^
    --profile:host ../conan/conan_profile_debug.txt ^
      %compiler% %conan_set_arch% ^
    --output-folder=../build/conan || goto end
)
pushd %cd%

echo Attempting to build the implementation

cd ../
if exist %build_dir% (
  echo Removing %build_dir%
  rmdir /S /Q %build_dir% && mkdir %build_dir%
)

echo Generating build files
cmake -S ./ -G %generator% %set_architecture% -B %build_dir% -DCMAKE_INSTALL_PREFIX=%install_dir% ^
  -DOS=windows -DCMAKE_BUILD_TYPE=%config% -DARCHITECTURE=%architecture% || goto end

echo Building %config%
cmake --build %build_dir% --config %config% --target install -j 8 || goto end

echo Building finished

@REM pause
popd
exit /B 0

:end
echo Building failed
popd
exit /B 1
