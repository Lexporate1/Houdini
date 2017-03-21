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
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once

#include "HoudiniGeoPartObject.h"
#include "HoudiniAssetParameter.h"
#if WITH_EDITOR
    #include "ContentBrowserDelegates.h"
#endif
#include "HoudiniAssetInstanceInput.generated.h"


class AActor;
class UStaticMesh;
class UMaterialInterface;
class UInstancedStaticMeshComponent;
class UHoudiniAssetInstanceInputField;

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetInstanceInput : public UHoudiniAssetParameter
{
    friend class UHoudiniAssetInstanceInputField;

    GENERATED_UCLASS_BODY()

    public:

        /** Destructor. **/
        virtual ~UHoudiniAssetInstanceInput();

    public:

        /** Create instance of this class. **/
        static UHoudiniAssetInstanceInput * Create(
            UHoudiniAssetComponent * InPrimaryObject,
            const FHoudiniGeoPartObject & HoudiniGeoPartObject );

        /** Create instance from another input. **/
        static UHoudiniAssetInstanceInput * Create(
            UHoudiniAssetComponent * InPrimaryObject,
            const UHoudiniAssetInstanceInput * OtherInstanceInput );

    public:

        /** Create this instance input. **/
        bool CreateInstanceInput();

        /** Recreates render states for used instanced static mesh components. **/
        void RecreateRenderStates();

        /** Recreates physics states for used instanced static mesh components. **/
        void RecreatePhysicsStates();

        /** Retrieve all instanced mesh components used by this input. **/
        bool CollectAllInstancedStaticMeshComponents(
            TArray< UInstancedStaticMeshComponent * > & Components,
            const UStaticMesh * StaticMesh );

        /** Get material replacement meshes for a given input. **/
        bool GetMaterialReplacementMeshes(
            UMaterialInterface * Material,
            TMap< UStaticMesh *, int32 > & MaterialReplacementsMap );

        FORCEINLINE const TArray< UHoudiniAssetInstanceInputField * >& GetInstanceInputFields() const { return InstanceInputFields; }

        FORCEINLINE const FHoudiniGeoPartObject& GetGeoPartObject() const { return HoudiniGeoPartObject; }
        
        /** Refresh state based on the given geo part object */
        void SetGeoPartObject( const FHoudiniGeoPartObject& InGeoPartObject );

    /** UHoudiniAssetParameter methods. **/
    public:

        /** Create this parameter from HAPI information - this implementation does nothing as this is not   **/
        /** a true parameter.                                                                               **/
        virtual bool CreateParameter(
            UObject * InPrimaryObject,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo );

        /** Set component for this parameter. **/
        virtual void SetHoudiniAssetComponent( UHoudiniAssetComponent * InComponent ) override;

#if WITH_EDITOR

        /** Create widget for this parameter and add it to a given category. **/
        virtual void CreateWidget( IDetailCategoryBuilder & DetailCategoryBuilder );

#endif

        /** Upload parameter value to HAPI. **/
        virtual bool UploadParameterValue();

    /** UObject methods. **/
    public:

        virtual void BeginDestroy() override;
        virtual void Serialize( FArchive & Ar ) override;
        static void AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector );

    public:

        /** Return true if this is an attribute instancer. **/
        bool IsAttributeInstancer() const;

        /** Return true if this is an object instancer. Attribute instancer and object instancers are   **/
        /** not mutually exclusive.                                                                     **/
        bool IsObjectInstancer() const;

        /** Return true if this is an attribute instancer with overrides. **/
        bool IsAttributeInstancerOverride() const;

#if WITH_EDITOR

        /** Clone all used instance static mesh components and and attach them to provided actor. **/
        void CloneComponentsAndAttachToActor( AActor * Actor );

#endif

    protected:

        /** Retrieve all transforms for a given path. Used by attribute instancer. **/
        void GetPathInstaceTransforms(
            const FString & ObjectInstancePath, const TArray< FString > & PointInstanceValues,
            const TArray< FTransform > & Transforms, TArray< FTransform > & OutTransforms );

    protected:

        /** Locate field which matches given criteria. Return null if not found. **/
        UHoudiniAssetInstanceInputField * LocateInputField(
            const FHoudiniGeoPartObject & GeoPartObject );

        /** Locate or create (if it does not exist) an input field. **/
        void CreateInstanceInputField(
            const FHoudiniGeoPartObject & HoudiniGeoPartObject,
            const TArray< FTransform > & ObjectTransforms, 
            const TArray< UHoudiniAssetInstanceInputField * > & OldInstanceInputFields,
            TArray< UHoudiniAssetInstanceInputField * > & NewInstanceInputFields );

        /** Locate or create (if it does not exist) an input field. This version is used with override attribute. **/
        void CreateInstanceInputField(
            UObject * InstancedObject, const TArray< FTransform > & ObjectTransforms,
            const TArray< UHoudiniAssetInstanceInputField * > & OldInstanceInputFields,
            TArray< UHoudiniAssetInstanceInputField * > & NewInstanceInputFields );

        /** Clean unused input fields. **/
        void CleanInstanceInputFields( TArray< UHoudiniAssetInstanceInputField * > & InInstanceInputFields );

        /** Returns primary object cast to HoudiniAssetComponent  */
        class UHoudiniAssetComponent* GetHoudiniAssetComponent();
        const class UHoudiniAssetComponent* GetHoudiniAssetComponent() const;

        /** Get the node id of the asset we are associated with */
        HAPI_NodeId GetAssetId() const;

    protected:

#if WITH_EDITOR

        /** Delegate used when static mesh has been drag and dropped. **/
        void OnStaticMeshDropped(
            UObject * InObject,
            UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
            int32 Idx, int32 VariationIdx );

        /** Gets the border brush to show around thumbnails, changes when the user hovers on it. **/
        const FSlateBrush * GetStaticMeshThumbnailBorder(
            UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
            int32 Idx, int32 VariationIdx ) const;

        /** Handler for when static mesh thumbnail is double clicked. We open editor in this case. **/
        FReply OnThumbnailDoubleClick(
            const FGeometry & InMyGeometry, const FPointerEvent & InMouseEvent, UObject * Object );

        /** Delegate for handling selection in content browser. **/
        void OnStaticMeshSelected(
            const FAssetData& AssetData, 
            UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
            int32 Idx, int32 VariationIdx );

        /** Closes the combo button. **/
        void CloseStaticMeshComboButton(
            UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 Idx,
            int32 VariationIdx );

        /** Triggered when combo button is opened or closed. **/
        void ChangedStaticMeshComboButton(
            bool bOpened, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
            int32 Idx, int32 VariationIdx );

        /** Browse to static mesh. **/
        void OnInstancedObjectBrowse( UObject* InstancedObject );

        /** Handler for reset static mesh button. **/
        FReply OnResetStaticMeshClicked(
            UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 Idx,
            int32 VariationIdx );

        /** Handler for adding instance variation **/
        void OnAddInstanceVariation( UHoudiniAssetInstanceInputField * InstanceInputField, int32 Index );

        /** Handler for removing instance variation **/
        void OnRemoveInstanceVariation( UHoudiniAssetInstanceInputField * InstanceInputField, int32 Index );

        /** Helper for field label */
        FString GetFieldLabel( int32 FieldIdx, int32 VariationIdx ) const;

        /** Get rotation components for given index. **/
        TOptional< float > GetRotationRoll(
            UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
            int32 VariationIdx ) const;
        TOptional< float > GetRotationPitch(
            UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
            int32 VariationIdx ) const;
        TOptional< float > GetRotationYaw(
            UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
            int32 VariationIdx ) const;

        /** Set rotation components for given index. **/
        void SetRotationRoll(
            float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx );
        void SetRotationPitch(
            float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx );
        void SetRotationYaw(
            float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx );

        /** Get scale components for a given index. **/
        TOptional< float > GetScaleX(
            UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 VariationIdx ) const;
        TOptional< float > GetScaleY(
            UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 VariationIdx ) const;
        TOptional< float > GetScaleZ(
            UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 VariationIdx ) const;

        /** Set scale components for a given index. **/
        void SetScaleX( float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx );
        void SetScaleY( float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx );
        void SetScaleZ( float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx );

        /** Return true if given index must scale linearly. **/
        ECheckBoxState IsChecked(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 VariationIdx) const;
        const FSlateBrush* GetPreserveScaleRatioImage( UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx ) const;
        /** Set option for whether scale should be linear. **/
        void CheckStateChanged(
            ECheckBoxState NewState, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
            int32 VariationIdx );

#endif

    protected:

        /** List of fields created by this instance input. **/
        TArray< UHoudiniAssetInstanceInputField * > InstanceInputFields;

#if WITH_EDITOR

        /** Delegate for filtering static meshes. **/
        FOnShouldFilterAsset OnShouldFilterStaticMesh;

#endif

        /** Corresponding geo part object. **/
        FHoudiniGeoPartObject HoudiniGeoPartObject;

        /** Id of an object to instance. **/
        HAPI_NodeId ObjectToInstanceId;

        /** Flags used by this input. **/
        union
        {
            struct
            {
                /** Set to true if this is an attribute instancer. **/
                uint32 bIsAttributeInstancer : 1;

                /** Set to true if this attribute instancer uses overrides. **/
                uint32 bAttributeInstancerOverride : 1;

                /** Set to true if this is a packed primitive instancer **/
                uint32 bIsPackedPrimitiveInstancer : 1;
            };

            uint32 HoudiniAssetInstanceInputFlagsPacked;
        };
};
