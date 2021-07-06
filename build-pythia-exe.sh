#!/bin/bash

## =============================================================================
## Step 3: Build generator exe 
##         TODO: need to configurability to the generator exe 


export LOCAL_PREFIX=/home/vagrant/development/common_bench

echo "Compiling   pythia_dis.cxx ..."
g++ pythia_dis.cxx -o pythia_dis  \
   -I/usr/local/include  -I${LOCAL_PREFIX}/include \
   -O2 -std=c++11 -pedantic -W -Wall -Wshadow -fPIC  \
   -L/usr/local/lib -Wl,-rpath,/usr/local/lib -lpythia8 -ldl \
   -L/usr/local/lib -Wl,-rpath,/usr/local/lib -lHepMC3

