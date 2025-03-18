#!/bin/bash

cd $HOME/ecmcast

cd ecmio
make clean
make all

cd ../ecmsocket
make all