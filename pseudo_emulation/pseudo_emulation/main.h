#pragma once
/*
This pseudo emulation uses some files that are compiled on AVR so
it's exercising the actual code. Anything coming from the proj\src
dir is also compiled on AVR. Anything in this dir is windows only


*/

#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "sequencer.h"
#include "protos.h"

void doSequencer();