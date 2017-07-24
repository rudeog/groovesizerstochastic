The core directory contains the collected files from arduino core, plus in the subdirectories of core, there are files
that were collected from needed libraries.
The makefile should look under core for everything that will be built into core.a, and look in src for everything else 
that will be linked with core.a
