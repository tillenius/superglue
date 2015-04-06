mkdir build
pushd build

cl /nologo /EHsc /I ..\include ..\test\main.cpp

for %%F in (..\test\modular\*.cpp) do (
  cl /nologo /EHsc /I ..\include %%F
)

for %%F in (..\examples\src\*.cpp) do (
  cl /nologo /EHsc /I ..\include %%F
)

for %%F in (*.exe) do (
  %%F
)

popd