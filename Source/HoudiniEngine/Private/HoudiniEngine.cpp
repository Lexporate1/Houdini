/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEnginePrivatePCH.h"


const FName FHoudiniEngine::HoudiniEngineAppIdentifier = FName(TEXT("HoudiniEngineApp"));


IMPLEMENT_MODULE(FHoudiniEngine, HoudiniEngine);
DEFINE_LOG_CATEGORY(LogHoudiniEngine);


FHoudiniEngine*
FHoudiniEngine::HoudiniEngineInstance = nullptr;


TSharedPtr<FSlateDynamicImageBrush>
FHoudiniEngine::GetHoudiniLogoBrush() const
{
	return HoudiniLogoBrush;
}


UStaticMesh*
FHoudiniEngine::GetHoudiniLogoStaticMesh() const
{
	return HoudiniLogoStaticMesh;
}


bool
FHoudiniEngine::CheckHapiVersionMismatch() const
{
	return bHAPIVersionMismatch;
}


FHoudiniEngine&
FHoudiniEngine::Get()
{
	check(FHoudiniEngine::HoudiniEngineInstance);
	return *FHoudiniEngine::HoudiniEngineInstance;
}


bool
FHoudiniEngine::IsInitialized()
{
	return (FHoudiniEngine::HoudiniEngineInstance != nullptr && FHoudiniEngineUtils::IsInitialized());
}


void
FHoudiniEngine::RegisterComponentVisualizers()
{
	if(GUnrealEd && !SplineComponentVisualizer.IsValid())
	{
		SplineComponentVisualizer = MakeShareable(new FHoudiniSplineComponentVisualizer);
		GUnrealEd->RegisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName(), SplineComponentVisualizer);
		SplineComponentVisualizer->OnRegister();
	}
}


void
FHoudiniEngine::UnregisterComponentVisualizers()
{
	if(GUnrealEd && SplineComponentVisualizer.IsValid())
	{
		GUnrealEd->UnregisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName());
	}
}


void
FHoudiniEngine::StartupModule()
{
	bHAPIVersionMismatch = false;
	HOUDINI_LOG_MESSAGE(TEXT("Starting the Houdini Engine module."));

	// Before starting the module, we need to locate and load HAPI library.
	{
		void* HAPILibraryHandle = nullptr;

		// See if we have HFS defined, if so attempt to load HAPI from given HFS.
		FString HFSPath = TEXT(HOUDINI_ENGINE_HFS_PATH);
		if(!HFSPath.IsEmpty())
		{
			HFSPath += TEXT("/bin");
			FPlatformProcess::PushDllDirectory(*HFSPath);
			HAPILibraryHandle = FPlatformProcess::GetDllHandle(TEXT("libHAPI.dll"));
			FPlatformProcess::PopDllDirectory(*HFSPath);

			if(HAPILibraryHandle)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Loaded libHAPI.dll from HFS path: %s"), *HFSPath);
			}
		}

#if PLATFORM_WINDOWS
		if(!HAPILibraryHandle)
		{
			// Otherwise, attempt to look up location in the registry.
			FString HoudiniRegistryLocation = FString::Printf(TEXT("Software\\Side Effects Software\\Houdini %s"), TEXT(HOUDINI_ENGINE_HOUDINI_VERSION));
			FString HoudiniInstallationPath;

			if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *HoudiniRegistryLocation, TEXT("InstallPath"), HoudiniInstallationPath))
			{
				HoudiniInstallationPath += TEXT("/bin");
				FPlatformProcess::PushDllDirectory(*HoudiniInstallationPath);
				HAPILibraryHandle = FPlatformProcess::GetDllHandle(TEXT("libHAPI.dll"));
				FPlatformProcess::PopDllDirectory(*HoudiniInstallationPath);

				if(HAPILibraryHandle)
				{
					HOUDINI_LOG_MESSAGE(TEXT("Loaded libHAPI.dll from Registry path: %s"), *HoudiniInstallationPath);
				}
			}
		}
#endif

		if(HAPILibraryHandle)
		{
			FHoudiniApi::InitializeHAPI(HAPILibraryHandle);
		}
		else
		{
			HOUDINI_LOG_MESSAGE(TEXT("Failed locating or loading libHAPI.dll"));
		}
	}

	// Create and register asset type actions for Houdini asset.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FHoudiniAssetTypeActions()));

	// Create and register broker for Houdini asset.
	HoudiniAssetBroker = MakeShareable(new FHoudiniAssetBroker());
	FComponentAssetBrokerage::RegisterBroker(HoudiniAssetBroker, UHoudiniAssetComponent::StaticClass(), true, true);

	// Register thumbnail renderer for Houdini asset.
	UThumbnailManager::Get().RegisterCustomRenderer(UHoudiniAsset::StaticClass(), UHoudiniAssetThumbnailRenderer::StaticClass());

	// Register details presenter for our component type.
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(TEXT("HoudiniAssetComponent"), FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniAssetComponentDetails::MakeInstance));

	// Create Houdini logo brush.
	const TArray<FPluginStatus> Plugins = IPluginManager::Get().QueryStatusForAllPlugins();
	for(auto PluginIt(Plugins.CreateConstIterator()); PluginIt; ++PluginIt)
	{
		const FPluginStatus& PluginStatus = *PluginIt;
		if(PluginStatus.Name == TEXT("HoudiniEngine"))
		{
			if(FPlatformFileManager::Get().GetPlatformFile().FileExists(*PluginStatus.Icon128FilePath))
			{
				const FName BrushName(*PluginStatus.Icon128FilePath);
				const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);

				if(Size.X > 0 && Size.Y > 0)
				{
					static const int ProgressIconSize = 32;
					HoudiniLogoBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
				}
			}

			break;
		}
	}

	// Create static mesh Houdini logo.
	HoudiniLogoStaticMesh = FHoudiniEngineUtils::CreateStaticMeshHoudiniLogo();
	HoudiniLogoStaticMesh->AddToRoot();

	// Extend main menu, we will add Houdini section to 'Window' menu tab.
	{
		MainMenuExtender = MakeShareable(new FExtender);
		MainMenuExtender->AddMenuExtension("WindowLocalTabSpawners", EExtensionHook::After, NULL, FMenuExtensionDelegate::CreateRaw(this, &FHoudiniEngine::AddHoudiniMenuExtension));
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);
	}

	// Build and running versions match, we can perform HAPI initialization.
	if(FHoudiniApi::IsHAPIInitialized())
	{
		// We need to make sure HAPI version is correct.
		int RunningEngineMajor = 0;
		int RunningEngineMinor = 0;
		int RunningEngineApi = 0;

		// Retrieve version numbers for running Houdini Engine.
		FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor);
		FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor);
		FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi);

		// Compare defined and running versions.
		if(RunningEngineMajor == HOUDINI_ENGINE_HOUDINI_ENGINE_MAJOR && 
		   RunningEngineMinor == HOUDINI_ENGINE_HOUDINI_ENGINE_MINOR &&
		   RunningEngineApi == HOUDINI_ENGINE_HOUDINI_ENGINE_API)
		{
			HAPI_CookOptions CookOptions;
			CookOptions.curveRefineLOD = 8.0f;
			CookOptions.clearErrorsAndWarnings = false;
			CookOptions.maxVerticesPerPrimitive = 3;
			CookOptions.splitGeosByGroup = false;
			CookOptions.refineCurveToLinear = true;

			HAPI_Result Result = FHoudiniApi::Initialize("", "", &CookOptions, true, -1);
			if(HAPI_RESULT_SUCCESS == Result)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Successfully intialized the Houdini Engine API module."));
			}
			else
			{
				HOUDINI_LOG_MESSAGE(TEXT("Starting up the Houdini Engine API module failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(Result));
			}
		}
		else
		{
			bHAPIVersionMismatch = true;

			HOUDINI_LOG_MESSAGE(TEXT("Starting up the Houdini Engine API module failed: build and running versions do not match."));
			HOUDINI_LOG_MESSAGE(TEXT("Defined version: %d.%d.api:%d vs Running version: %d.%d.api:%d"), HOUDINI_ENGINE_HOUDINI_ENGINE_MAJOR,
								HOUDINI_ENGINE_HOUDINI_ENGINE_MINOR, HOUDINI_ENGINE_HOUDINI_ENGINE_API, RunningEngineMajor, 
								RunningEngineMinor, RunningEngineApi);
		}
	}

	// Create HAPI scheduler and processing thread.
	HoudiniEngineScheduler = new FHoudiniEngineScheduler();
	HoudiniEngineSchedulerThread = FRunnableThread::Create(HoudiniEngineScheduler, TEXT("HoudiniTaskCookAsset"), 0, TPri_Normal);

	// Store the instance.
	FHoudiniEngine::HoudiniEngineInstance = this;
}


void
FHoudiniEngine::ShutdownModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine module."));

	if(UObjectInitialized())
	{
		// Unregister broker.
		FComponentAssetBrokerage::UnregisterBroker(HoudiniAssetBroker);

		// Unregister thumbnail renderer.
		UThumbnailManager::Get().UnregisterCustomRenderer(UHoudiniAsset::StaticClass());
	}

	// Unregister asset type actions we have previously registered.
	if(FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		for(int32 Index = 0; Index < AssetTypeActions.Num(); ++Index)
		{
			AssetTools.UnregisterAssetTypeActions(AssetTypeActions[Index].ToSharedRef());
		}

		AssetTypeActions.Empty();
	}

	// Unregister details presentation.
	if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(TEXT("HoudiniAssetComponent"));
	}

	// Unregister our component visualizers.
	UnregisterComponentVisualizers();

	// We no longer need Houdini logo static mesh.
	HoudiniLogoStaticMesh->RemoveFromRoot();

	// Do scheduler and thread clean up.
	if(HoudiniEngineScheduler)
	{
		HoudiniEngineScheduler->Stop();
	}

	if(HoudiniEngineSchedulerThread)
	{
		//HoudiniEngineSchedulerThread->Kill(true);
		HoudiniEngineSchedulerThread->WaitForCompletion();

		delete HoudiniEngineSchedulerThread;
		HoudiniEngineSchedulerThread = nullptr;
	}

	if(HoudiniEngineScheduler)
	{
		delete HoudiniEngineScheduler;
		HoudiniEngineScheduler = nullptr;
	}

	// Perform HAPI finalization.
	if(FHoudiniApi::IsHAPIInitialized())
	{
		FHoudiniApi::Cleanup();
	}

	FHoudiniApi::FinalizeHAPI();
}


void
FHoudiniEngine::AddHoudiniMenuExtension(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Houdini", LOCTEXT("HoudiniLabel", "Houdini Engine"));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("HoudiniMenuEntryTitle", "Save .hip file"),
			LOCTEXT("HoudiniMenuEntryToolTip", "Saves a .hip file of the current Houdini scene."),
			//FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tutorials"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FHoudiniEngine::SaveHIPFile)));
	MenuBuilder.EndSection();
}


void
FHoudiniEngine::SaveHIPFile()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if(DesktopPlatform && FHoudiniEngineUtils::IsInitialized())
	{
		TArray<FString> SaveFilenames;
		bool bSaved = false;
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if(MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		bSaved = DesktopPlatform->SaveFileDialog(ParentWindowWindowHandle,
			NSLOCTEXT("SaveHIPFile", "SaveHIPFile", "Saves a .hip file of the current Houdini scene.").ToString(), 
			*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)), 
			TEXT(""), 
			TEXT("Houdini HIP file|*.hip"),
			EFileDialogFlags::None, 
			SaveFilenames);

		if(bSaved && SaveFilenames.Num())
		{
			// Get first path.
			std::wstring HIPPath(*SaveFilenames[0]);
			std::string HIPPathConverted(HIPPath.begin(), HIPPath.end());

			// Save HIP file through Engine.
			FHoudiniApi::SaveHIPFile(HIPPathConverted.c_str());
		}
	}
}


void
FHoudiniEngine::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	AssetTypeActions.Add(Action);
}


void
FHoudiniEngine::AddTask(const FHoudiniEngineTask& Task)
{
	HoudiniEngineScheduler->AddTask(Task);

	FScopeLock ScopeLock(&CriticalSection);
	FHoudiniEngineTaskInfo TaskInfo;
	TaskInfos.Add(Task.HapiGUID, TaskInfo);
}


void
FHoudiniEngine::AddTaskInfo(const FGuid HapIGUID, const FHoudiniEngineTaskInfo& TaskInfo)
{
	FScopeLock ScopeLock(&CriticalSection);
	TaskInfos.Add(HapIGUID, TaskInfo);
}


void
FHoudiniEngine::RemoveTaskInfo(const FGuid HapIGUID)
{
	FScopeLock ScopeLock(&CriticalSection);
	TaskInfos.Remove(HapIGUID);
}


bool
FHoudiniEngine::RetrieveTaskInfo(const FGuid HapIGUID, FHoudiniEngineTaskInfo& TaskInfo)
{
	FScopeLock ScopeLock(&CriticalSection);
	
	if(TaskInfos.Contains(HapIGUID))
	{
		TaskInfo = TaskInfos[HapIGUID];
		return true;
	}

	return false;
}
