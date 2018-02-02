/*
 * Copyright (c) <2017> Side Effects Software Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Produced by:
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 * COMMENTS:
 *      This file is generated. Do not modify directly.
 */

/*

    Houdini Version: 17.0.120
    Houdini Engine Version: 3.2.7
    Unreal Version: 4.18.0

*/

using UnrealBuildTool;
using System.IO;

public class HoudiniEngineEditor : ModuleRules
{
    public HoudiniEngineEditor( ReadOnlyTargetRules Target ) : base( Target )
    {
        PCHUsage = PCHUsageMode.UseSharedPCHs;
        bool bIsRelease = true;
        string HFSPath = "";
        string HoudiniVersion = "17.0.120";

        // Check if we are compiling on unsupported platforms.
        if( Target.Platform != UnrealTargetPlatform.Win64 &&
            Target.Platform != UnrealTargetPlatform.Mac &&
            Target.Platform != UnrealTargetPlatform.Linux )
        {
            string Err = string.Format( "Houdini Engine Runtime: Compiling on unsupported platform." );
            System.Console.WriteLine( Err );
            throw new BuildException( Err );
        }

        if( bIsRelease )
        {
            if( Target.Platform == UnrealTargetPlatform.Win64 )
            {
                // We first check if Houdini Engine is installed.
                string HPath = "C:/Program Files/Side Effects Software/Houdini Engine " + HoudiniVersion;
                if( !Directory.Exists( HPath ) )
                {
                    // If Houdini Engine is not installed, we check for Houdini installation.
                    HPath = "C:/Program Files/Side Effects Software/Houdini " + HoudiniVersion;
                    if( !Directory.Exists( HPath ) )
                    {
                        if ( !Directory.Exists( HFSPath ) )
                        {
                            string Err = string.Format( "Houdini Engine : Please install Houdini or Houdini Engine {0}", HoudiniVersion );
                            System.Console.WriteLine( Err );
                        }
                    }
                    else
                    {
                        HFSPath = HPath;
                    }
                }
                else
                {
                    HFSPath = HPath;
                }
            }
            else if( Target.Platform == UnrealTargetPlatform.Mac )
            {
                string HPath = "/Applications/Houdini/Houdini" + HoudiniVersion + "/Frameworks/Houdini.framework/Versions/Current/Resources";
                if( !Directory.Exists( HPath ) )
                {
                    if ( !Directory.Exists( HFSPath ) )
                    {
                        string Err = string.Format( "Houdini Engine : Please install Houdini {0}", HoudiniVersion );
                        System.Console.WriteLine( Err );
                    }
                }
                else
                {
                    HFSPath = HPath;
                }
            }
            else
            {
                HFSPath = System.Environment.GetEnvironmentVariable( "HFS" );
                System.Console.WriteLine( "Linux - found HFS:" + HFSPath );
            }
        }

        string HAPIIncludePath = "";

        if( HFSPath != "" )
        {
            HAPIIncludePath = HFSPath + "/toolkit/include/HAPI";

            if( Target.Platform == UnrealTargetPlatform.Win64 )
            {
                Definitions.Add( "HOUDINI_ENGINE_HFS_PATH_DEFINE=" + HFSPath );
            }
        }

        if( HAPIIncludePath != "" )
        {
            PublicIncludePaths.Add( HAPIIncludePath );

            // Add the custom include path as well in case the toolkit path doesn't exist yet.
            PublicIncludePaths.Add( HFSPath + "/custom/houdini/include/HAPI" );
        }

        PublicIncludePaths.AddRange(
            new string[] {
                "HoudiniEngineRuntime/Public/HAPI",
                "HoudiniEngineRuntime/Public",
                "HoudiniEngineEditor/Public"
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                "HoudiniEngineEditor/Private",
                "HoudiniEngineRuntime/Private"
            }
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "PlacementMode"
            }
        );

        // Add common dependencies.
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "HoudiniEngineRuntime",
                "Slate",
                "SlateCore",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AppFramework",
                "AssetTools",
                "ContentBrowser",
                "DesktopWidgets",
                "EditorStyle",
                "EditorWidgets",
                "Engine",
                "InputCore",
                "LevelEditor",
                "MainFrame",
                "Projects",
                "PropertyEditor",
                "RHI",
                "RawMesh",
                "RenderCore",
                "ShaderCore",
                "TargetPlatform",
                "UnrealEd",
                "ApplicationCore",
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                "PlacementMode",
            }
        );
    }
}
