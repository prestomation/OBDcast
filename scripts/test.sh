#!/bin/bash
set -e
pip install platformio
pio test -e native
