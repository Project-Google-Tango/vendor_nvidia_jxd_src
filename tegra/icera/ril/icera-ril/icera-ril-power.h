/* Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2010, Icera Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** You may not use this file except in compliance with the License.
** You may obtain a copy of the License at:
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** imitations under the License.
*/
/*
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ICERA_RIL_POWER_H
#define ICERA_RIL_POWER_H 1

/**
 * EDP initialization
 *
 * This is where RIL registers the Icera modem as a new EDP
 * consumer
 */
void rilEDPInit(void);

/**
 * Force full power
 * This is typically called when exiting aeroplane mode, so that modem has
 * enough power until it can adjust to its real needs.
 */
int rilEDPSetFullPower(void);
/*
 * Parse %IEDPU unsolicited
 * @param s pointer to character string to parse
*/
void parseIedpu(const char *s);

#endif
