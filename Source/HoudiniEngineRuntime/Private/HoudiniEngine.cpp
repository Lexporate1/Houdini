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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"
#include "HoudiniEngineScheduler.h"
#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"


const FName FHoudiniEngine::HoudiniEngineAppIdentifier = FName(TEXT("HoudiniEngineApp"));


IMPLEMENT_MODULE(FHoudiniEngine, HoudiniEngine);
DEFINE_LOG_CATEGORY(LogHoudiniEngine);


FHoudiniEngine*
FHoudiniEngine::HoudiniEngineInstance = nullptr;


#if WITH_EDITOR

TSharedPtr<FSlateDynamicImageBrush>
FHoudiniEngine::GetHoudiniLogoBrush() const
{
	return HoudiniLogoBrush;
}

#endif


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


const FString&
FHoudiniEngine::GetLibHAPILocation() const
{
	return LibHAPILocation;
}


HAPI_Result
FHoudiniEngine::GetHapiState() const
{
	return HAPIState;
}


void
FHoudiniEngine::SetHapiState(HAPI_Result Result)
{
	HAPIState = Result;
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
FHoudiniEngine::StartupModule()
{
	bHAPIVersionMismatch = false;
	HAPIState = HAPI_RESULT_NOT_INITIALIZED;

	HOUDINI_LOG_MESSAGE(TEXT("Starting the Houdini Engine module."));

	// Before starting the module, we need to locate and load HAPI library.
	{
		void* HAPILibraryHandle = FHoudiniEngineUtils::LoadLibHAPI(LibHAPILocation);

		if(HAPILibraryHandle)
		{
			FHoudiniApi::InitializeHAPI(nullptr, HAPILibraryHandle);
		}
		else
		{
			// Get platform specific name of libHAPI.
			FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();

			HOUDINI_LOG_MESSAGE(TEXT("Failed locating or loading %s"), *LibHAPIName);
		}
	}

	// Register settings.
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if(SettingsModule)
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "HoudiniEngine",
				LOCTEXT("RuntimeSettingsName", "Houdini Engine"),
				LOCTEXT("RuntimeSettingsDescription", "Configure the HoudiniEngine plugin"),
				GetMutableDefault<UHoudiniRuntimeSettings>()
			);
	}

	// Create static mesh Houdini logo.
	HoudiniLogoStaticMesh = FHoudiniEngineUtils::CreateStaticMeshHoudiniLogo();
	HoudiniLogoStaticMesh->AddToRoot();

#if WITH_EDITOR

	if(!IsRunningCommandlet())
	{
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
						static const int32 ProgressIconSize = 32;
						HoudiniLogoBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName,
							FVector2D(ProgressIconSize, ProgressIconSize)));
					}
				}

				break;
			}
		}
	}

#endif

	// Build and running versions match, we can perform HAPI initialization.
	if(FHoudiniApi::IsHAPIInitialized())
	{
		// We need to make sure HAPI version is correct.
		int32 RunningEngineMajor = 0;
		int32 RunningEngineMinor = 0;
		int32 RunningEngineApi = 0;

		// Retrieve version numbers for running Houdini Engine.
		FHoudiniApi::GetEnvInt(nullptr, HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor);
		FHoudiniApi::GetEnvInt(nullptr, HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor);
		FHoudiniApi::GetEnvInt(nullptr, HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi);

		// Compare defined and running versions.
		if(RunningEngineMajor == HAPI_VERSION_HOUDINI_ENGINE_MAJOR &&
		   RunningEngineMinor == HAPI_VERSION_HOUDINI_ENGINE_MINOR &&
		   RunningEngineApi == HAPI_VERSION_HOUDINI_ENGINE_API)
		{
			HAPI_CookOptions CookOptions;
			CookOptions.curveRefineLOD = 8.0f;
			CookOptions.clearErrorsAndWarnings = false;
			CookOptions.maxVerticesPerPrimitive = 3;
			CookOptions.splitGeosByGroup = false;
			CookOptions.refineCurveToLinear = true;

			HAPI_Result Result = FHoudiniApi::Initialize(nullptr, "", "", &CookOptions, true, -1);
			if(HAPI_RESULT_SUCCESS == Result)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Successfully intialized the Houdini Engine API module."));
			}
			else
			{
				HOUDINI_LOG_MESSAGE(TEXT("Starting up the Houdini Engine API module failed: %s"),
					*FHoudiniEngineUtils::GetErrorDescription(Result));
			}
		}
		else
		{
			bHAPIVersionMismatch = true;

			HOUDINI_LOG_MESSAGE(TEXT("Starting up the Houdini Engine API module failed: build and running versions do not match."));
			HOUDINI_LOG_MESSAGE(TEXT("Defined version: %d.%d.api:%d vs Running version: %d.%d.api:%d"), HAPI_VERSION_HOUDINI_ENGINE_MAJOR,
								HAPI_VERSION_HOUDINI_ENGINE_MINOR, HAPI_VERSION_HOUDINI_ENGINE_API, RunningEngineMajor,
								RunningEngineMinor, RunningEngineApi);
		}
	}

	// Create HAPI scheduler and processing thread.
	HoudiniEngineScheduler = new FHoudiniEngineScheduler();
	HoudiniEngineSchedulerThread = FRunnableThread::Create(HoudiniEngineScheduler, TEXT("HoudiniTaskCookAsset"), 0,
		TPri_Normal);

	// Store the instance.
	FHoudiniEngine::HoudiniEngineInstance = this;
}


void
FHoudiniEngine::ShutdownModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine module."));

	// We no longer need Houdini logo static mesh.
	HoudiniLogoStaticMesh->RemoveFromRoot();

	// Unregister settings.
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if(SettingsModule)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "HoudiniEngine");
	}

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
		FHoudiniApi::Cleanup(nullptr);
	}

	FHoudiniApi::FinalizeHAPI();
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
