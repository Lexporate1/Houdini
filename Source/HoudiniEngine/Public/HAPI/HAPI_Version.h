/*
* Copyright (c) <2022> Side Effects Software Inc.*
* Permission is hereby granted, free of charge, to any person obtaining a copy* of this software and associated documentation files (the "Software"), to deal* in the Software without restriction, including without limitation the rights* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell* copies of the Software, and to permit persons to whom the Software is* furnished to do so, subject to the following conditions:** The above copyright notice and this permission notice shall be included in all* copies or substantial portions of the Software.** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE* SOFTWARE. *
 * Produced by:
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 * COMMENTS:
 *      Generated version information to be used when linking for
 *      sanity checks.
 */

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// WARNING! This file is GENERATED by UNREAL_Version.py script.
// DO NOT modify manually or commit to the repository!
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#ifndef __HAPI_VERSION_h__
#define __HAPI_VERSION_h__

// The three components of the Houdini version that HAPI is
// expecting to compile against.
#define HAPI_VERSION_HOUDINI_MAJOR 19
#define HAPI_VERSION_HOUDINI_MINOR 5
#define HAPI_VERSION_HOUDINI_BUILD 234
#define HAPI_VERSION_HOUDINI_PATCH 0

// The two components of the Houdini Engine (marketed) version.
#define HAPI_VERSION_HOUDINI_ENGINE_MAJOR 5
#define HAPI_VERSION_HOUDINI_ENGINE_MINOR 0

// This is a monotonously increasing API version number that can be used
// to lock against a certain API for compatibility purposes. Basically,
// when this number changes code compiled against the HAPI.h methods
// might no longer compile. Semantic changes to the methods will also
// cause this version to increase. This number will be reset to 0
// every time the Houdini Engine version is bumped.
#define HAPI_VERSION_HOUDINI_ENGINE_API 0

#endif // __HAPI_VERSION_h__
