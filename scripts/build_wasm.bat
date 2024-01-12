cd ..\build
mkdir ..\temp
move .gitignore ..\temp
erase *.* /s /q
move ..\temp\* .
rmdir ..\temp
cls
emcmake cmake .. -DCMAKE_BUILD_TYPE="Release" && cd ..\scripts && cmake --build ../build --config=release -j4
