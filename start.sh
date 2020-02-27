#!/bin/sh
make mrproper;make rd88f6281a_config NBOOT=1 NAND=1 LE=1;make -s;
