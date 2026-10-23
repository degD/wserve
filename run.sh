#!/bin/bash

# This script compiles the source code,
# kills existing wserve instances, and
# runs a new wserve instance.

make build
pkill wserve
./wserve
