@echo off
del "../data/shaders/*.spv"
for %%f in (../data/shaders/*.vert) do glslangValidator -V -o ../data/shaders/%%f.spv ../data/shaders/%%f
for %%f in (../data/shaders/*.frag) do glslangValidator -V -o ../data/shaders/%%f.spv ../data/shaders/%%f
