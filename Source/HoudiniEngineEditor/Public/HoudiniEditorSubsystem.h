// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniInput.h"

#include "HoudiniEditorSubsystem.generated.h"


class USkeletalMesh;

USTRUCT()
struct HOUDINIENGINEEDITOR_API FHoudiniNodeSyncOptions
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FString FetchNodePath = "/obj/UnrealContent";

	UPROPERTY()
	FString SendNodePath = "/obj/UnrealContent";

	UPROPERTY()
	FString UnrealAssetName = "TestAsset";

	UPROPERTY()
	FString UnrealPathName = "/Game/000";

	//UPROPERTY()
	//FDirectoryPath DirectoryPath;

	UPROPERTY()
	bool UseOutputNodes = true;

	UPROPERTY()
	bool OverwriteSkeleton = false;

	UPROPERTY()
	FString SkeletonAssetPath = "";
	
	UPROPERTY()
	UHoudiniInput* NodeSyncInput;
};


/**
 * Editor Susbsystem that creates a "Managed" Session HDA used to transfer assets between Houdini and Unreal
 */
UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void SendToHoudini(const TArray<UObject*>& SelectedAssets);

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void FetchFromHoudini(const FString& InPackageName, const FString& InPackageFolder, const int32& MaxInfluences = 1, const bool& ImportNormals=false);
	
	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void Fetch();

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void SendWorldSelection();

	bool CreateSessionIfNeeded();

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void DumpSessionInfo();

	FHoudiniNodeSyncOptions NodeSyncOptions;

	UHoudiniInput* GetNodeSyncInput();

	//virtual void RegisterLayoutExtensions(FLayoutExtender& Extender) override;

private:

	bool InitNodeSyncInputIfNeeded();

	bool SendStaticMeshToHoudini(const HAPI_NodeId& InMeshNodeId, UStaticMesh* InMesh);
	bool SendSkeletalMeshToHoudini(const HAPI_NodeId& InMeshNodeId, USkeletalMesh* InSkelMesh);
	bool FetchSkeletalMeshFromHoudini(const HAPI_NodeId& InNodeId, const FString& InPackageName, const FString& InPackageFolder, const int32& MaxInfluences, const bool& ImportNormals);
	bool FetchStaticMeshFromHoudini(const HAPI_NodeId& InNodeId, const FString& InPackageName, const FString& InPackageFolder);



	HAPI_NodeId object_node_id = -1;
	//HAPI_NodeId network_node_id = -1;
};
