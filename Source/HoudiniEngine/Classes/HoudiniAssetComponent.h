/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * This class represents a Houdini Engine component in Unreal Engine. 
 *
 * The following section explains some of the designs and decisions made by us (regarding this class) and should be 
 * removed in the future from production code.
 *
 * One of the problems we've encountered with Unreal Engine design is that the editor UI (details panel) relies on 
 * class RTTI information. That is, the editor uses RTTI to enumerate all fields / properties of a component and 
 * constructs corresponding UI elements (sliders, input fields, and so forth) from that information.
 *
 * This RTTI information (and corresponding c++ stubs) is generated during the pre-build step by Unreal Engine. 
 * Unfortunately this means that each class assumes a fixed layout, which would not work in our case as 
 * each Houdini Digital Asset can have a variable number of parameters and even more, number of parameters can change
 * between the cooks. 
 *
 * There are multiple ways around this problem, including using arrays to store parameters, however
 * each approach has it's limitations ~ whether it's unnecessary complexity or issues with Blueprint integration. With
 * this in mind we chose a slightly different approach: after HDA is loaded, we enumerate its parameters and based on
 * that we generate new RTTI information and replace RTTI information generated by Unreal at runtime. 
 *
 * A bit more information regarding Unreal RTTI: for each UObject derived instance, class object is stored in Class 
 * member variable (UClass type). For each UPROPERTY, a UProperty instance is created and is stored in link list inside 
 * UClass object. UProperty has an internal offset, which is an offset in bytes (from the beginning of an object). 
 * Furthermore, this offset property is a signed 32 bit integer. 
 * 
 * Another problem is the space required by each component to store the data fetched from HDA parameters.
 * This information becomes available only once the asset is cooked and parameters are fetched. This problem can be 
 * solved in a few ways. First approach would be to request a larger memory block when component is created (but large
 * enough to accommodate all parameter data), use placement new to create the component at the beginning of the fetched
 * block and store parameter data past the end of the component data. This approach would work, but could potentially
 * cause problems if 3rd party user decided to store meta information in a similar way ~ effectively overwriting ours.
 * This is a pretty common use case. Second solution is to patch the Unreal engine ~ patch the offset property to be
 * 64 bit int. This way we could store properties outside the component and calculate necessary offsets from the 
 * beginning of an object. We think this is a reasonable approach, but would require a pull request to Epic. This is
 * on our TODO list. Our current temporary solution is to use a fixed component layout with 64k scratch space. We store
 * all property data (and patch corresponding offsets) inside that scratch space. Scratch space size is controlled by
 * HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE definition, which is defined in Plugin Build.cs file.
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

#pragma once
#include "HAPI.h"
#include "HoudiniEngineSerialization.h"
#include "HoudiniAssetComponent.generated.h"

class UClass;
class UProperty;
class UMaterial;
class FTransform;
class UHoudiniAsset;
class UHoudiniAssetObject;
class FPrimitiveSceneProxy;
class FHoudiniAssetObjectGeo;
class FComponentInstanceDataCache;

struct FPropertyChangedEvent;

UCLASS(ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew, meta=(BlueprintSpawnableComponent))
class HOUDINIENGINE_API UHoudiniAssetComponent : public UPrimitiveComponent
{
	friend class FHoudiniMeshSceneProxy;
	friend class AHoudiniAssetActor;

	GENERATED_UCLASS_BODY()

public:

	/** Houdini Asset associated with this component. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Transient, Category=HoudiniAsset)
	UHoudiniAsset* HoudiniAsset;

	/** List of generated Houdini textures used by this component. Changes between the cooks. **/
	UPROPERTY(VisibleInstanceOnly, EditFixedSize, NoClear, Transient, BlueprintReadOnly, Category=Textures)
	TArray<UTexture2D*> HoudiniTextures;

public:

	/** Change the Houdini Asset used by this component. **/
	virtual void SetHoudiniAsset(UHoudiniAsset* NewHoudiniAsset);

	/** Ticking function to check cooking / instatiation status. **/
	void TickHoudiniComponent();

	/** Used to differentiate native components from dynamic ones. **/
	void SetNative(bool InbIsNativeComponent);

	/** Return id of a Houdini asset. **/
	HAPI_AssetId GetAssetId() const;

	/** Set id of a Houdini asset. **/
	void SetAssetId(HAPI_AssetId InAssetId);

	/** Return current referenced Houdini asset. **/
	UHoudiniAsset* GetHoudiniAsset() const;

	/** Return owner Houdini actor. **/
	AHoudiniAssetActor* GetHoudiniAssetActorOwner() const;

public: /** UObject methods. **/

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreSave() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected: /** UActorComponent methods. **/

	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed() override;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual FName GetComponentInstanceDataType() const override;
	virtual TSharedPtr<class FComponentInstanceDataBase> GetComponentInstanceData() const override;
	virtual void ApplyComponentInstanceData(TSharedPtr<class FComponentInstanceDataBase> ComponentInstanceData) override;

private: /** UPrimitiveComponent methods. **/

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

private: /** USceneComponent methods. **/

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private: /** FEditorDelegates delegates. **/

	void OnPreSaveWorld(uint32 SaveFlags, class UWorld* World);
	void OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess);
	void OnPIEEventBegin(const bool bIsSimulating);
	void OnPIEEventEnd(const bool bIsSimulating);

protected:

	/** Patch RTTI : patch class information for this component's class based on given Houdini Asset. **/
	void ReplaceClassInformation(const FString& ActorLabel, bool bReplace = true);

private:

	/** Patch RTTI : translate asset parameters to class properties and insert them into a given class instance. **/
	bool ReplaceClassProperties(UClass* ClassInstance);

	/** Patch RTTI: remove generated properties from class information object. **/
	void RemoveClassProperties(UClass* ClassInstance);

	/** Patch RTTI : patch class object. **/
	void ReplaceClassObject(UClass* ClassObjectNew);

	/** Patch RTTI : replace property offset data. **/
	void ReplacePropertyOffset(UProperty* Property, int Offset);

	/** Patch RTTI : Restore original class information. **/
	void RestoreOriginalClassInformation();

	/** Patch RTTI : Restore patched class information. **/
	void RestorePatchedClassInformation();

	/** Patch RTTI : Create property based on given type. **/
	UProperty* CreateProperty(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags, EHoudiniEngineProperty::Type PropertyType);

	/** Patch RTTI : Create Integer property. **/
	UProperty* CreatePropertyInt(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags);
	UProperty* CreatePropertyInt(UClass* ClassInstance, const FString& Name, int Count, const int32* Value, uint32& Offset);

	/** Patch RTTI : Create Float property. **/
	UProperty* CreatePropertyFloat(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags);
	UProperty* CreatePropertyFloat(UClass* ClassInstance, const FString& Name, int Count, const float* Value, uint32& Offset);

	/** Patch RTTI : Create Toggle property. **/
	UProperty* CreatePropertyToggle(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags);
	UProperty* CreatePropertyToggle(UClass* ClassInstance, const FString& Name, int Count, const int32* bValue, uint32& Offset);

	/** Patch RTTI : Create Color property. **/
	UProperty* CreatePropertyColor(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags);
	UProperty* CreatePropertyColor(UClass* ClassInstance, const FString& Name, int Count, const float* Value, uint32& Offset);

	/** Patch RTTI: Create String property. **/
	UProperty* CreatePropertyString(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags);
	UProperty* CreatePropertyString(UClass* ClassInstance, const FString& Name, int Count, const HAPI_StringHandle* Value, uint32& Offset);

	/** Patch RTTI: Create Enumeration property. **/
	UProperty* CreatePropertyEnum(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags);
	UProperty* CreatePropertyEnum(UClass* ClassInstance, const FString& Name, const std::vector<HAPI_ParmChoiceInfo>& Choices, int32 Value, uint32& Offset);
	UProperty* CreatePropertyEnum(UClass* ClassInstance, const FString& Name, const std::vector<HAPI_ParmChoiceInfo>& Choices, const FString& ValueString, uint32& Offset);

	/** Patch RTTI : Create an enum object. **/
	UField* CreateEnum(UClass* ClassInstance, const FString& Name, const std::vector<HAPI_ParmChoiceInfo>& Choices);

	/** Patch RTTI : Remove all meta information from given enum object. **/
	void RemoveMetaDataFromEnum(UEnum* EnumObject);

	/** Subscribe to Editor events. **/
	void SubscribeEditorDelegates();

	/** Unsubscribe from Editor events. **/
	void UnsubscribeEditorDelegates();

	/** Set parameter values which have changed. **/
	void SetChangedParameterValues();

	/** Helper function to compute proper alignment boundary at a given offset for a specified type. **/
	template <typename TType> TType* ComputeOffsetAlignmentBoundary(uint32 Offset) const;

	/** Return property type for a given property. **/
	EHoudiniEngineProperty::Type GetPropertyType(UProperty* Property) const;

	/** Update rendering information. **/
	void UpdateRenderingInformation();

	/** Refresh editor's detail panel and update properties. **/
	void UpdateEditorProperties();

	/** Start ticking. **/
	void StartHoudiniTicking();

	/** Stop ticking. **/
	void StopHoudiniTicking();

	/** Assign actor label based on asset instance name. **/
	void AssignUniqueActorLabel();

	/** Release materials for this component. **/
	void ReleaseComponentMaterials();

	/** Clear all existing geos (and their parts). This is called during geometry recreation. **/
	void ClearGeos();

	/** Create necessary rendering resources for each geo. **/
	void CreateRenderingResources();

	/** Release rendering resources used by each geo. **/
	void ReleaseRenderingResources();

	/** Return true if this component contains geometry. **/
	bool ContainsGeos() const;

	/** Collect textures from geometry. **/
	void CollectTextures();

	/** Compute bounding volume for all geometry of this component. **/
	void ComputeComponentBoundingVolume();

public:

	/** Some RTTI classes which are used during property construction. **/
	static UScriptStruct* ScriptStructColor;

private:

	/** Patch class counter, we need this to generate unique ids. **/
	static uint32 ComponentPatchedClassCounter;

protected:

	/** Array of asset objects geos. **/
	TArray<FHoudiniAssetObjectGeo*> HoudiniAssetObjectGeos;

	/** Set of properties that have changed. Will force object recook. Cleared after each recook. **/
	TSet<UProperty*> ChangedProperties;

	/** Array of properties we have created. We keep these for serialization purposes. **/
	TArray<UProperty*> CreatedProperties;

	/** Array of data containing serialized properties. Used during loading. **/
	TArray<FHoudiniEngineSerializedProperty> SerializedProperties;

	/** Notification used by this component. **/
	TWeakPtr<SNotificationItem> NotificationPtr;

	/** Bounding volume information for current geometry. **/
	FBoxSphereBounds BoundingVolume;

	/** A fence which is used to keep track of the rendering thread releasing rendering resources. **/
	FRenderCommandFence ReleaseResourcesFence;

	/** GUID used to track asynchronous cooking requests. **/
	FGuid HapiGUID;

	/** Timer delegate, we use it for ticking during cooking or instantiation. **/
	FTimerDelegate TimerDelegate;

	/** Patched class information. We store this here because we need sometimes to unroll back to original class information. **/
	UClass* PatchedClass;

	/** Id of corresponding Houdini asset. **/
	HAPI_AssetId AssetId;

	/** Is set to true when this component is native and false is when it is dynamic. **/
	bool bIsNativeComponent;

	/** Is set to true when this component belongs to a preview actor. **/
	bool bIsPreviewComponent;

	/** Is set to true when asynchronous rendering resources release has been started. Kept for debugging purposes. **/
	bool bAsyncResourceReleaseHasBeenStarted;

	/** Is set to true when PreSave has been triggered. **/
	bool bPreSaveTriggered;

	/** Is set to true if this component has been loaded. **/
	bool bLoadedComponent;

	/** Is set to true when component is loaded and no instantiation / cooking is necessary. **/
	bool bLoadedComponentRequiresInstantiation;

	/** Is set to true when blueprint component is being destroyed outside the regular		**/
	/** blueprint create/destroy cycle.														**/
	mutable bool bIsRealDestroy;

	/** Is set to true when PIE mode is on (either play or simulate.) **/
	bool bIsPlayModeActive;

private:

	/** Marker ~ beginning of scratch space. **/
	uint64 ScratchSpaceMarker;

	/** Scratch space buffer ~ used to store data for each property. **/
	char ScratchSpaceBuffer[HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE];
};


template <typename TType>
TType*
UHoudiniAssetComponent::ComputeOffsetAlignmentBoundary(uint32 Offset) const
{
	return Align<TType*>((TType*)(((char*) this) + Offset), ALIGNOF(TType));
}
