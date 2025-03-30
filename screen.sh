#!/bin/sh
source /home/orlangur/ncs/toolchains/b77d8c1312/env.sh
cat $(nrfutil device list | grep ports | awk '{print $2}')
