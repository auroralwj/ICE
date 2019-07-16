#!/bin/bash

CURRDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd && cd .. )"
DIR="$(dirname "$CURRDIR")"

mkdir -p build

LDIR="$DIR/lib"
IDIR="$DIR/include"
EDIR="$DIR/3rdparty/Eigen"
BDIR="$CURRDIR/build"

#-------------------------------------------------------------------------------------------------
# Build example files
#--------------------------------------------------------------------------------------------------

# g++ test_gnss_bce.cpp -std=c++11 -I"$EDIR" -L"$LDIR" -Wl,-rpath="$LDIR" -I"$IDIR" -lboost_system -lboost_program_options -ltbb -lcluster -Wno-deprecated-declarations -lgtsam -fopenmp -o "$BDIR/test_gnss_bce"

g++ test_gnss_ice.cpp -std=c++11 -I"$EDIR" -L"$LDIR" -Wl,-rpath="$LDIR" -I"$IDIR" -lboost_system -lboost_program_options -ltbb -lgpstk -lcluster -Wno-deprecated-declarations -lgtsam -fopenmp -o "$BDIR/test_gnss_ice"

g++ test_gnss_l2.cpp -std=c++11 -I"$EDIR" -L"$LDIR" -Wl,-rpath="$LDIR" -I"$IDIR" -lboost_system -lboost_program_options -ltbb -lgpstk -lgtsam -lcluster -Wno-deprecated-declarations -fopenmp -o "$BDIR/test_gnss_l2"

# g++ rnx_2_gtsam.cpp -std=c++11 -L"$LDIR" -Wl,-rpath="$LDIR" -I"$IDIR" -lboost_system -lboost_program_options -ltbb -Wno-deprecated-declarations -lgpstk -fopenmp -o "$BDIR/rnx_2_gtsam"
