mkdir build
cd build
cmake.exe -DCMAKE_INSTALL_PREFIX=..\install -G "NMake Makefiles" ..
nmake install
cd ..