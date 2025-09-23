/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef IONVIDEO_EXT_H
#define IONVIDEO_EXT_H

int ionvideo_assign_map(char **receiver_name, int *inst);

int ionvideo_alloc_map(int *inst);

void ionvideo_release_map(int inst);

#endif /* IONVIDEO_EXT_H */
