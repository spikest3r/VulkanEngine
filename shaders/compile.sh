#!/bin/bash
set -e
glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv
echo "Shaders compiled OK"