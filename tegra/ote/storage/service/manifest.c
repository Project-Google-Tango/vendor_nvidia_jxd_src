/*
 * Copyright (C) 2012-2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stddef.h>
#include <stdio.h>

#include <service/ote_manifest.h>

OTE_MANIFEST OTE_MANIFEST_ATTRS ote_manifest =
{
	/* UUID : {33456890-8572-4a6f-a1f1-03aa9b05f9ff} */
	{ 0x33456890, 0x8572, 0x4a6f,
		{ 0xa1, 0xf1, 0x03, 0xaa, 0x9b, 0x05, 0xf9, 0xff } },

	/* optional configuration options here */
	{
		/* no direct access for non-secure clients */
		OTE_CONFIG_RESTRICT_ACCESS(OTE_RESTRICT_NON_SECURE_APPS),
	},
};
