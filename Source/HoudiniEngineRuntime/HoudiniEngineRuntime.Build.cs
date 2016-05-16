/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
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

    Houdini Version: 16.0.249
    Houdini Engine Version: 3.0.12
    Unreal Version: 4.11.1

*/

using UnrealBuildTool;
using System.IO;

public class HoudiniEngineRuntime : ModuleRules
{
	public HoudiniEngineRuntime( TargetInfo Target )
	{
		bool bIsRelease = true;
		string HFSPath = "";
		string HoudiniVersion = "16.0.249";

		// Check if we are compiling on unsupported platforms.
		if( Target.Platform != UnrealTargetPlatform.Win64 &&
			Target.Platform != UnrealTargetPlatform.Mac )
		{
			string Err = string.Format( "Houdini Engine : Compiling on unsupported platform." );
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
				string HPath = "/Applications/Houdini " + HoudiniVersion + "/Frameworks/Houdini.framework/Versions/Current/Resources";
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
		}

		PublicIncludePaths.AddRange(
			new string[] {
				"HoudiniEngineRuntime/Public/HAPI",
				"HoudiniEngineRuntime/Public"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"HoudiniEngineRuntime/Private"
			}
		);

		// Add common dependencies.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"ShaderCore",
				"InputCore",
				"RHI",
				"Settings",
				"Foliage",
				"Landscape"
			}
		);

		if (UEBuildConfiguration.bBuildEditor == true)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"UnrealEd",
					"Slate",
					"SlateCore",
					"Projects",
					"PropertyEditor",
					"ContentBrowser",
					"LevelEditor",
					"MainFrame",
					"EditorStyle",
					"EditorWidgets",
					"AppFramework",
					"TargetPlatform",
					"RawMesh",
					"DesktopWidgets"
				}
			);
		}

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
