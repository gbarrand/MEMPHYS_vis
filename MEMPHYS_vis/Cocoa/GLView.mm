// Copyright (C) 2010, Guy Barrand. All rights reserved.
// See the file MEMPHYS_vis.license for terms.

#define pi messy_pi
#define perThousand messy_perThousand

#import <Cocoa/Cocoa.h>

#undef pi
#undef perThousand
#undef pascal
#undef negativeInfinity

#include "../MEMPHYS_vis/main"

typedef MEMPHYS_vis::main app_main_t;

#import <exlib/app/Cocoa/GLView.mm>
