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
*/

#include "HoudiniMeshSplitInstancerComponent.h"

#include "HoudiniApi.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "Components/StaticMeshComponent.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#include "MeshPaintHelpers.h"
#endif

#include "Internationalization/Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniMeshSplitInstancerComponent::UHoudiniMeshSplitInstancerComponent( const FObjectInitializer& ObjectInitializer )
: Super( ObjectInitializer )
, InstancedMesh( nullptr )
{
}

void
UHoudiniMeshSplitInstancerComponent::OnComponentDestroyed( bool bDestroyingHierarchy )
{
    ClearInstances(0);
    Super::OnComponentDestroyed( bDestroyingHierarchy );
}

void
UHoudiniMeshSplitInstancerComponent::Serialize( FArchive & Ar )
{
    Super::Serialize( Ar );
    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    Ar << InstancedMesh;
    Ar << OverrideMaterial;
    Ar << Instances;
}

void 
UHoudiniMeshSplitInstancerComponent::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniMeshSplitInstancerComponent * ThisMSIC = Cast< UHoudiniMeshSplitInstancerComponent >(InThis);
    if ( ThisMSIC && !ThisMSIC->IsPendingKill() )
    {
        Collector.AddReferencedObject(ThisMSIC->InstancedMesh, ThisMSIC);
        Collector.AddReferencedObject(ThisMSIC->OverrideMaterial, ThisMSIC);
        Collector.AddReferencedObjects(ThisMSIC->Instances, ThisMSIC);
    }
}

void 
UHoudiniMeshSplitInstancerComponent::SetInstances( 
    const TArray<FTransform>& InstanceTransforms,
    const TArray<FLinearColor> & InstancedColors)
{
#if WITH_EDITOR
    if ( Instances.Num() || InstanceTransforms.Num() )
    {
        if (!GetOwner() || GetOwner()->IsPendingKill())
            return;

        const FScopedTransaction Transaction( LOCTEXT( "UpdateInstances", "Update Instances" ) );
        GetOwner()->Modify();

        // Destroy previous instances while keeping some of the one that we'll be able to reuse
        ClearInstances(InstanceTransforms.Num());

        if( !InstancedMesh || InstancedMesh->IsPendingKill() )
        {
            HOUDINI_LOG_ERROR(TEXT("%s: Null InstancedMesh for split instanced mesh override"), *GetOwner()->GetName());
            return;
        }

        TArray<FColor> InstanceColorOverride;
        InstanceColorOverride.SetNumUninitialized(InstancedColors.Num());
        for( int32 ix = 0; ix < InstancedColors.Num(); ++ix )
        {
            InstanceColorOverride[ix] = InstancedColors[ix].GetClamped().ToFColor(false);
        }

        // Only create new SMC for newly added instances
        for (int32 iAdd = Instances.Num(); iAdd < InstanceTransforms.Num(); ++iAdd)
        {
            const FTransform& InstanceTransform = InstanceTransforms[iAdd];

            UStaticMeshComponent* SMC = NewObject< UStaticMeshComponent >(
                GetOwner(), UStaticMeshComponent::StaticClass(),
                NAME_None, RF_Transactional);

            SMC->SetRelativeTransform(InstanceTransform);

            Instances.Add(SMC);
        }

        ensure(InstanceTransforms.Num() == Instances.Num());
        if (InstanceTransforms.Num() == Instances.Num())
        {
            for (int32 iIns = 0; iIns < Instances.Num(); ++iIns)
            {
                UStaticMeshComponent* SMC = Instances[iIns];
                const FTransform& InstanceTransform = InstanceTransforms[iIns];

                if (!SMC || SMC->IsPendingKill())
                    continue;

                SMC->SetRelativeTransform(InstanceTransform);

                // Attach created static mesh component to this thing
                SMC->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);

                SMC->SetStaticMesh(InstancedMesh);
                SMC->SetVisibility(IsVisible());
                SMC->SetMobility(Mobility);
                if (OverrideMaterial && !OverrideMaterial->IsPendingKill())
                {
                    int32 MeshMaterialCount = InstancedMesh->StaticMaterials.Num();
                    for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
                        SMC->SetMaterial(Idx, OverrideMaterial);
                }

                // If we have override colors, apply them
                int32 InstIndex = Instances.Num();
                if (InstanceColorOverride.IsValidIndex(InstIndex))
                {
                    MeshPaintHelpers::FillStaticMeshVertexColors(SMC, -1, InstanceColorOverride[InstIndex], FColor::White);
                    //FIXME: How to get rid of the warning about fixup vertex colors on load?
                    //SMC->FixupOverrideColorsIfNecessary();
                }

                SMC->RegisterComponent();

                // Adding to the array has been done above
                // Instances.Add(SMC);

                // Properties not being propagated to newly created UStaticMeshComponents
                if (UHoudiniAssetComponent * pHoudiniAsset = Cast<UHoudiniAssetComponent>(GetAttachParent()))
                {
                    pHoudiniAsset->CopyComponentPropertiesTo(SMC);
                }
            }
        }
    }
#endif
}

void 
UHoudiniMeshSplitInstancerComponent::ClearInstances(int32 NumToKeep)
{
    if (NumToKeep <= 0)
    {
        for (auto&& Instance : Instances)
        {
            if (Instance)
            {
                Instance->ConditionalBeginDestroy();
            }
        }
        Instances.Empty();
    }
    else if (NumToKeep > 0 && NumToKeep < Instances.Num())
    {
        for (int32 i = NumToKeep; i < Instances.Num(); ++i)
        {
            UStaticMeshComponent * Instance = Instances[i];
            if (Instance)
            {
                Instance->ConditionalBeginDestroy();
            }
        }
        Instances.SetNum(NumToKeep);
    }
}

#undef LOCTEXT_NAMESPACE