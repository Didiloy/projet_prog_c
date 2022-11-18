#!/bin/bash

make clean
make master
make worker
make client
./rmsempipe.sh