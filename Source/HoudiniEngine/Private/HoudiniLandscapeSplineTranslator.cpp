﻿/*
* Copyright (c) <2023> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniLandscapeSplineTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOutput.h"
#include "HoudiniPackageParams.h"
#include "HoudiniSplineTranslator.h"

#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplinesComponent.h"
#include "Materials/Material.h"
#include "WorldPartition/WorldPartition.h"


/** Per mesh attribute data for a landscape spline segment. */
 struct FLandscapeSplineSegmentMeshAttributes
{
	/** Mesh ref */
	bool bHasMeshRefAttribute = false;
	TArray<FString> MeshRef;
	
	/** Mesh material override, the outer index is material 0, 1, 2 ... */
	TArray<TArray<FString>> MeshMaterialOverrideRefs;

	/** Mesh scale. */
	bool bHasMeshScaleAttribute = false;
	TArray<float> MeshScale;
};


/** Attribute data extracted from curve points and prims. */
struct FLandscapeSplineCurveAttributes
{
	//
	// Point Attributes
	//
	
	/** Resampled point positions. */
	TArray<float> PointPositions;
	
	/** Point rotations. */
	bool bHasPointRotationAttribute = false;
	TArray<float> PointRotations;
	
	/** Point paint layer names */
	bool bHasPointPaintLayerNameAttribute = false;
	TArray<FString> PointPaintLayerNames;
	
	/** Point bRaiseTerrain */
	bool bHasPointRaiseTerrainAttribute = false;
	TArray<int32> PointRaiseTerrains;
	
	/** Point bLowerTerrain */
	bool bHasPointLowerTerrainAttribute = false;
	TArray<int32> PointLowerTerrains;

	/** The StaticMesh ref per point. */
	bool bHasPointMeshRefAttribute = false;
	TArray<FString> PointMeshRefs;

	/**
	 * The Material Override refs of each point. The outer index material override index, the inner index
	 * is point index.
	 */
	TArray<TArray<FString>> PerMaterialOverridePointRefs;

	/** The static mesh scale of each point. */
	bool bHasPointMeshScaleAttribute = false;
	TArray<float> PointMeshScales;

	/** The names of the control points. */
	bool bHasPointNameAttribute = false;
	TArray<FString> PointNames;

	/** The point half-width. */
	bool bHasPointHalfWidthAttribute = false;
	TArray<float> PointHalfWidths;

	//
	// Although the following properties are named Vertex... they are point attributes in HAPI (but are intended to be
	// authored as vertex attributes in Houdini). When curves are extracted via HAPI points and vertices are the same.
	//
	
	/**
	 * The mesh socket names on the splines' vertices. The outer index is the near side (0) and far side (1) of the
	 * segment connection. The inner index is a vertex index.
	 */
	bool bHasVertexConnectionSocketNameAttribute[2] { false, false };
	TArray<FString> VertexConnectionSocketNames[2];

	/**
	 * Tangent length point attribute, for segment connections. The outer index is the near side (0) and far side (1)
	 * of the segment connection. The inner index is a vertex index.
	 */
	bool bHasVertexConnectionTangentLengthAttribute[2] { false, false };
	TArray<float> VertexConnectionTangentLengths[2];

	/** Vertex/segment paint layer name */
	bool bHasVertexPaintLayerNameAttribute = false;
	TArray<FString> VertexPaintLayerNames;

	/** Vertex/segment bRaiseTerrain */
	bool bHasVertexRaiseTerrainAttribute = false;
	TArray<int32> VertexRaiseTerrains;

	/** Vertex/segment bLowerTerrain */
	bool bHasVertexLowerTerrainAttribute = false;
	TArray<int32> VertexLowerTerrains;

	/** Static mesh attributes on vertices. Outer index is mesh 0, 1, 2 ... */
	TArray<FLandscapeSplineSegmentMeshAttributes> VertexPerMeshSegmentData;

	//
	// Primitive attributes
	//
	
	/**
	 * The mesh socket names on the splines' prims. The index is the near side (0) and far side (1) of the
	 * segment connection.
	 */
	bool bHasPrimConnectionSocketNameAttribute[2] { false, false };
	FString PrimConnectionSocketNames[2];

	/**
	 * Tangent length point attribute, for segment connections. The index is the near side (0) and far side (1)
	 * of the segment connection.
	 */
	bool bHasPrimConnectionTangentLengthAttribute[2] { false, false };
	float PrimConnectionTangentLengths[2];

	/** Prim/segment paint layer name */
	bool bHasPrimPaintLayerNameAttribute = false;
	FString PrimPaintLayerName;

	/** Prim/segment bRaiseTerrain */
	bool bHasPrimRaiseTerrainAttribute = false;
	int32 bPrimRaiseTerrain;

	/** Prim/segment bLowerTerrain */
	bool bHasPrimLowerTerrainAttribute = false;
	int32 bPrimLowerTerrain;

	/** Static mesh attribute from primitives, the index is mesh 0, 1, 2 ... */
	TArray<FLandscapeSplineSegmentMeshAttributes> PrimPerMeshSegmentData;
};

/**
 * Transient/transactional struct for processing landscape spline output. Used in
 * CreateOutputLandscapeSplinesFromHoudiniGeoPartObject(). This is not a UStruct / does n0t use UProperties. We do not
 * intend to store this struct anywhere, its entire lifecycle is during calls to
 * CreateOutputLandscapeSplinesFromHoudiniGeoPartObject().
 */
struct FLandscapeSplineInfo
{
	/**
	 * True for valid entries. Invalid entries would be those where Landscape, LandscapeInfo or SplinesComponent is
	 * null / invalid.
	 */
	bool bIsValid = false;

	/** Output object indentifier for this landscape splines component / actor. */
	FHoudiniOutputObjectIdentifier Identifier;

	/** True if we are going to reuse a previously create landscape spline component / actor. */
	bool bReusedPreviousOutput = false;
	
	/** The landscape that owns the spline. */
	ALandscapeProxy* Landscape = nullptr;

	/** The landscape info for the landscape */
	ULandscapeInfo* LandscapeInfo = nullptr;

	/** Custom output name, if applicable (WP only). */
	FName OutputName = NAME_None;
	
	/** The landscape spline actor, if applicable (WP only). */
	ALandscapeSplineActor* LandscapeSplineActor = nullptr;

	/** The splines component. */
	ULandscapeSplinesComponent* SplinesComponent = nullptr;

	/**
	 * Array of curve indices in the HGPO that will be used to create segments for this landscape spline. There can
	 * be more than one segment per curve.
	 */
	TArray<int32> CurveIndices;

	/**
	 * An array per-curve that stores the index of the first point (corresponding to the P attribute) for the curve
	 * info in the HGPO.
	 */
	TArray<int32> PerCurveFirstPointIndex;

	/** An array per-curve that stores the number of points for the curve in the HGPO. */
	TArray<int32> PerCurvePointCount;

	/** Curve prim and point attributes read from Houdini to apply to ULandscapeSplineControlPoint/Segment. */
	TArray<FLandscapeSplineCurveAttributes> CurveAttributes;

	/**
	 * Control points mapped by desired name that have been created for this splines component. Names can be
	 * auto-generated if no name attribute was present.
	 */
	TMap<FName, ULandscapeSplineControlPoint*> ControlPointMap;
};


FVector ConvertPositionToVector(const float* InPosition)
{
	// Swap Y/Z and convert meters to centimeters
	return {
		static_cast<double>(InPosition[0] * HAPI_UNREAL_SCALE_FACTOR_POSITION),
		static_cast<double>(InPosition[2] * HAPI_UNREAL_SCALE_FACTOR_POSITION),
		static_cast<double>(InPosition[1] * HAPI_UNREAL_SCALE_FACTOR_POSITION)
	};
}

bool
FHoudiniLandscapeSplineTranslator::DestroyLandscapeSplinesSegmentsAndControlPoints(ULandscapeSplinesComponent* const InSplinesComponent)
{
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	return false;
#else
	if (!IsValid(InSplinesComponent))
		return false;

	TArray<TObjectPtr<ULandscapeSplineSegment>>& Segments = InSplinesComponent->GetSegments();
	TArray<TObjectPtr<ULandscapeSplineControlPoint>>& ControlPoints = InSplinesComponent->GetControlPoints();

	for (ULandscapeSplineSegment* const Segment : Segments)
	{
		if (!IsValid(Segment))
			continue;
		
		Segment->DeleteSplinePoints();

		const bool bCP0Valid = IsValid(Segment->Connections[0].ControlPoint);
		const bool bCP1Valid = IsValid(Segment->Connections[1].ControlPoint);
		if (bCP0Valid)
			Segment->Connections[0].ControlPoint->ConnectedSegments.Remove(FLandscapeSplineConnection(Segment, 0));
		if (bCP1Valid)
			Segment->Connections[1].ControlPoint->ConnectedSegments.Remove(FLandscapeSplineConnection(Segment, 1));

		if (bCP0Valid)
			Segment->Connections[0].ControlPoint->UpdateSplinePoints();
		if (bCP1Valid)
			Segment->Connections[1].ControlPoint->UpdateSplinePoints();
	}
	Segments.Empty();

	for (ULandscapeSplineControlPoint* const ControlPoint : ControlPoints)
	{
		if (!IsValid(ControlPoint))
			continue;

		ControlPoint->DeleteSplinePoints();

		// There shouldn't really be any connected segments left...
		for (FLandscapeSplineConnection& Connection : ControlPoint->ConnectedSegments)
		{
			if (!IsValid(Connection.Segment))
				continue;
			
			Connection.Segment->DeleteSplinePoints();

			// Get the control point at the *other* end of the segment and remove it from it
			ULandscapeSplineControlPoint* OtherEnd = Connection.GetFarConnection().ControlPoint;
			if (!IsValid(OtherEnd))
				continue;
			
			OtherEnd->ConnectedSegments.Remove(FLandscapeSplineConnection(Connection.Segment, 1 - Connection.End));
			OtherEnd->UpdateSplinePoints();
		}

		ControlPoint->ConnectedSegments.Empty();
	}
	ControlPoints.Empty();

	return true;
#endif
}

bool
FHoudiniLandscapeSplineTranslator::ProcessLandscapeSplineOutput(
	UHoudiniOutput* const InOutput, UObject* const InOuterComponent)
{
	if (!IsValid(InOutput))
		return false;

	if (!IsValid(InOuterComponent))
		return false;

	// Only run on landscape spline inputs
	if (InOutput->GetType() != EHoudiniOutputType::LandscapeSpline)
		return false;

	// If InOuterComponent is a HAC, look for the first valid output landscape to use as a fallback if the spline does
	// not specify a landscape target
	ALandscapeProxy* FallbackLandscape = nullptr;
	UHoudiniAssetComponent const* const HAC = Cast<UHoudiniAssetComponent>(InOuterComponent);
	if (IsValid(HAC))
	{
		TArray<UHoudiniOutput*> Outputs;
		HAC->GetOutputs(Outputs);
		for (UHoudiniOutput const* const Output : Outputs)
		{
			if (!IsValid(Output) || Output->GetType() == EHoudiniOutputType::Landscape)
				continue;

			for (const auto& Entry : Output->GetOutputObjects())
			{
				const FHoudiniOutputObject& OutputObject = Entry.Value;
				if (!IsValid(OutputObject.OutputObject))
					continue;
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(OutputObject.OutputObject);
				if (IsValid(Proxy))
				{
					FallbackLandscape = Proxy;
					break;
				}
			}

			if (FallbackLandscape)
				break;
		}
	}

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> NewOutputObjects;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OldOutputObjects = InOutput->GetOutputObjects();
	// Iterate on all the output's HGPOs
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->GetHoudiniGeoPartObjects())
	{
		// Skip any HGPO that is not a landscape spline
		if (CurHGPO.Type != EHoudiniPartType::LandscapeSpline)
			continue;

		// Create / update landscape splines from this HGPO
		static constexpr bool bForceRebuild = false;
		CreateOutputLandscapeSplinesFromHoudiniGeoPartObject(
			CurHGPO,
			InOuterComponent,
			OldOutputObjects,
			bForceRebuild,
			FallbackLandscape,
			NewOutputObjects);
	}

	// The old map now only contains unused/stale output landscape splines: destroy them
	for (auto& OldPair : OldOutputObjects)
	{
		for (UObject* const Component : OldPair.Value.OutputComponents)
		{
			ULandscapeSplinesComponent* OldSplineComponent = Cast<ULandscapeSplinesComponent>(Component);
			if (!IsValid(OldSplineComponent))
				continue;
			
			// In non-WP the component is managed via the landscape, and it only has one splines component. In
			// WP we want to destroy the actor ... so we just clear the segments and control points here.
			DestroyLandscapeSplinesSegmentsAndControlPoints(OldSplineComponent);
		}
		OldPair.Value.OutputComponents.Empty();

		// If the output object used a landscape spline actor destroy it
		ALandscapeSplineActor* OldActor = Cast<ALandscapeSplineActor>(OldPair.Value.OutputObject);
		if (IsValid(OldActor))
		{
			ULandscapeInfo* const LandscapeInfo = OldActor->GetLandscapeInfo();
			if (IsValid(LandscapeInfo))
			{
				LandscapeInfo->UnregisterSplineActor(OldActor);
			}
			OldActor->Destroy();
		}
		OldPair.Value.OutputObject = nullptr;
	}
	OldOutputObjects.Empty();

	InOutput->SetOutputObjects(NewOutputObjects);

	FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::CreateOutputLandscapeSplinesFromHoudiniGeoPartObject(
	const FHoudiniGeoPartObject& InHGPO, 
	UObject* const InOuterComponent,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InCurrentSplines,
	const bool bInForceRebuild,
	ALandscapeProxy* InFallbackLandscape,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputSplines)
{
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	HOUDINI_LOG_WARNING(TEXT("Landscape Spline Output is only supported in UE5.1+"));
	return false;
#else
	// If we're not forcing the rebuild then only recreate if the HGPO is marked has changed.
	if (!bInForceRebuild && (!InHGPO.bHasGeoChanged || !InHGPO.bHasPartChanged))
	{
		// Simply reuse the existing splines
		OutputSplines = InCurrentSplines;
		return true;
	}

	if (!IsValid(InOuterComponent))
		return false;

	const int32 CurveNodeId = InHGPO.GeoId;
	const int32 CurvePartId = InHGPO.PartId;
	if (CurveNodeId < 0 || CurvePartId < 0)
		return false;

	// Find the fallback landscape to use, either InFallbackLandscape if valid, otherwise the first one we find in the
	// world
	UWorld* const World = InOuterComponent->GetWorld();
	const bool bIsUsingWorldPartition = IsValid(World->GetWorldPartition());
	ALandscapeProxy* FallbackLandscape = InFallbackLandscape;
	if (!IsValid(FallbackLandscape))
	{
		TActorIterator<ALandscapeProxy> LandscapeIt(World, ALandscapeProxy::StaticClass());
		if (LandscapeIt)
			FallbackLandscape = *LandscapeIt;
	}

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	if (!Session)
		return false;

	// Get the curve info from HAPI
	HAPI_CurveInfo CurveInfo;
	FHoudiniApi::CurveInfo_Init(&CurveInfo);
	FHoudiniApi::GetCurveInfo(Session, CurveNodeId, CurvePartId, &CurveInfo);

	// Get the point/vertex count for each curve primitive
	int32 NumCurves = CurveInfo.curveCount;
	TArray<int32> CurvePointCounts;
	CurvePointCounts.SetNumZeroed(NumCurves);
	FHoudiniApi::GetCurveCounts(Session, CurveNodeId, CurvePartId, CurvePointCounts.GetData(), 0, NumCurves);
	
	// Extract all target landscapes refs as prim attributes
	TArray<FString> LandscapeRefs;
	HAPI_AttributeInfo AttrLandscapeRefs;
	FHoudiniApi::AttributeInfo_Init(&AttrLandscapeRefs);
	FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		CurveNodeId, CurvePartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_TARGET_LANDSCAPE, AttrLandscapeRefs, LandscapeRefs, 1, HAPI_ATTROWNER_PRIM);

	// Extract all custom output name as prim attributes (used for landscape spline actor names in WP, not applicable to non-WP).
	TArray<FString> OutputNames;
	if (bIsUsingWorldPartition)
	{
		HAPI_AttributeInfo AttrOutputNames;
		FHoudiniApi::AttributeInfo_Init(&AttrOutputNames);
		FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			CurveNodeId, CurvePartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, AttrLandscapeRefs, LandscapeRefs, 1, HAPI_ATTROWNER_PRIM);
	}

	// Iterate over curves first, use prim attributes to find the landscape that the splines should be attached to,
	// and for world partition look at unreal_output_name to determine the landscape spline actor name.
	TMap<FName, FLandscapeSplineInfo> LandscapeSplineInfos;
	LandscapeSplineInfos.Reserve(NumCurves);
	for (int32 CurveIdx = 0, NextCurveStartPointIdx = 0; CurveIdx < NumCurves; ++CurveIdx)
	{
		const int32 NumPointsInCurve = CurvePointCounts[CurveIdx];
		NextCurveStartPointIdx += NumPointsInCurve;
		
		// Determine the name (or NAME_None in non-WP)
		FName OutputName = NAME_None;
		if (bIsUsingWorldPartition && OutputNames.IsValidIndex(CurveIdx))
		{
			OutputName = *OutputNames[CurveIdx];
		}

		// Get/create the FLandscapeSplineInfo entry that we use to manage the data for each
		// ULandscapeSplinesComponent / ALandscapeSplineActor that we will output to
		FLandscapeSplineInfo* SplineInfo = LandscapeSplineInfos.Find(OutputName);
		if (!SplineInfo)
		{
			const FString IdentifierName = FString::Printf(TEXT("%s-%s"), *InHGPO.PartName, *OutputName.ToString());
			FHoudiniOutputObjectIdentifier Identifier(InHGPO.ObjectId, InHGPO.GeoId, InHGPO.PartId, IdentifierName);

			SplineInfo = &LandscapeSplineInfos.Add(OutputName);
			SplineInfo->bIsValid = false;
			SplineInfo->Identifier = Identifier;
			SplineInfo->OutputName = OutputName;

			FHoudiniOutputObject* FoundOutputObject = InCurrentSplines.Find(Identifier);

			// Use the landscape specified with the landscape target attribute
			if (LandscapeRefs.IsValidIndex(CurveIdx))
			{
				const FString LandscapeRef = LandscapeRefs[CurveIdx];
				SplineInfo->Landscape = FindObjectFast<ALandscapeProxy>(nullptr, *LandscapeRef);
			}

			// Otherwise use the fallback landscape
			if (!SplineInfo->Landscape)
				SplineInfo->Landscape = FallbackLandscape;
			// Get the landscape info from our target landscape (if valid)
			const bool bIsLandscapeValid = IsValid(SplineInfo->Landscape);
			bool bIsLandscapeInfoValid = false;
			if (bIsLandscapeValid)
			{
				SplineInfo->LandscapeInfo = SplineInfo->Landscape->GetLandscapeInfo();
				if (IsValid(SplineInfo->LandscapeInfo))
					bIsLandscapeInfoValid = true;
			}

			// Depending if the world is using world partition we need to create a landscape spline actor, or manipulate
			// the landscape splines component on the landscape directly (non-world partition)
			if (bIsUsingWorldPartition)
			{
				if (bIsLandscapeInfoValid)
				{
					// Check if we found a matching Output Object, and check if it already has a landscape spline actor.
					// That actor must belong to SplineInfo->Landscape/Info. If it does not then we won't reuse that
					// output object
					if (FoundOutputObject && IsValid(FoundOutputObject->OutputObject) && FoundOutputObject->OutputObject->IsA<ALandscapeSplineActor>())
					{
						ALandscapeSplineActor* CurrentActor = Cast<ALandscapeSplineActor>(FoundOutputObject->OutputObject);
						if (CurrentActor->GetLandscapeInfo() == SplineInfo->LandscapeInfo)
						{
							SplineInfo->LandscapeSplineActor = CurrentActor;
							SplineInfo->bReusedPreviousOutput = true;
						}
					}
					
					if (!SplineInfo->LandscapeSplineActor)
						SplineInfo->LandscapeSplineActor = SplineInfo->LandscapeInfo->CreateSplineActor(FVector::ZeroVector);
					
					if (IsValid(SplineInfo->LandscapeSplineActor))
					{
						SplineInfo->SplinesComponent = SplineInfo->LandscapeSplineActor->GetSplinesComponent();
					}
				}
			}
			else if (bIsLandscapeValid)
			{
				SplineInfo->SplinesComponent = SplineInfo->Landscape->GetSplinesComponent();
				if (!IsValid(SplineInfo->SplinesComponent))
				{
					SplineInfo->Landscape->CreateSplineComponent();
					SplineInfo->SplinesComponent = SplineInfo->Landscape->GetSplinesComponent();
				}
				else
				{
					// Check if we are re-using the splines component 
					if (FoundOutputObject && !FoundOutputObject->OutputComponents.IsEmpty() && IsValid(FoundOutputObject->OutputComponents[0])
							&& FoundOutputObject->OutputComponents[0] == SplineInfo->SplinesComponent)
					{
						SplineInfo->bReusedPreviousOutput = true;
					}
				}
			}

			SplineInfo->bIsValid = IsValid(SplineInfo->SplinesComponent);

			if (!SplineInfo->bReusedPreviousOutput || !FoundOutputObject)
			{
				// If we are not re-using the previous output object with this identifier, record / create it as a new one.
				FHoudiniOutputObject OutputObject;
				OutputObject.OutputComponents.Add(SplineInfo->SplinesComponent);
				if (bIsUsingWorldPartition)
					OutputObject.OutputObject = SplineInfo->LandscapeSplineActor;
				else
					OutputObject.OutputObject = SplineInfo->SplinesComponent;

				OutputSplines.Add(SplineInfo->Identifier, OutputObject);
			}
			else
			{
				// Re-use the FoundOutputObject
				OutputSplines.Add(SplineInfo->Identifier, *FoundOutputObject);
			}
		}

		if (!SplineInfo->bIsValid)
			continue;

		// Add the primitive and point indices of this curve to the SplineInfo 
		SplineInfo->CurveIndices.Add(CurveIdx);
		SplineInfo->PerCurvePointCount.Add(CurvePointCounts[CurveIdx]);
		const int32 CurveFirstPointIndex = NextCurveStartPointIdx - NumPointsInCurve;
		SplineInfo->PerCurveFirstPointIndex.Add(CurveFirstPointIndex);

		// Copy the attributes for this curve primitive from Houdini / HAPI
		CopyCurveAttributesFromHoudini(
			CurveNodeId,
			CurvePartId,
			CurveIdx,
			CurveFirstPointIndex,
			NumPointsInCurve,
			SplineInfo->CurveAttributes.AddDefaulted_GetRef());
	}

	// Fetch generic attributes
	TArray<FHoudiniGenericAttribute> GenericPointAttributes;
	const bool bHasGenericPointAttributes = (FHoudiniEngineUtils::GetGenericAttributeList(
		InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, GenericPointAttributes, HAPI_ATTROWNER_POINT) > 0);
	TArray<FHoudiniGenericAttribute> GenericPrimAttributes;
	const bool bHasGenericPrimAttributes = (FHoudiniEngineUtils::GetGenericAttributeList(
		InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, GenericPrimAttributes, HAPI_ATTROWNER_PRIM) > 0);

	// Process each SplineInfo entry 
	for (auto& Entry : LandscapeSplineInfos)
	{
		FLandscapeSplineInfo& SplineInfo = Entry.Value;
		if (!SplineInfo.bIsValid)
			continue;

		// If we are reusing the spline component, clear all segments and control points first
		if (SplineInfo.bReusedPreviousOutput)
			DestroyLandscapeSplinesSegmentsAndControlPoints(SplineInfo.SplinesComponent);
		
		const FTransform WorldTransform = SplineInfo.SplinesComponent->GetComponentTransform();
		TArray<TObjectPtr<ULandscapeSplineControlPoint>>& ControlPoints = SplineInfo.SplinesComponent->GetControlPoints();
		TArray<TObjectPtr<ULandscapeSplineSegment>>& Segments = SplineInfo.SplinesComponent->GetSegments();

		// Process each curve primitive recorded in SplineInfo. Each curve primitive will be at least one segment (with
		// at least the first and last points of the primitive being control points).
		const int32 NumCurvesInSpline = SplineInfo.PerCurveFirstPointIndex.Num();
		for (int32 CurveEntryIdx = 0; CurveEntryIdx < NumCurvesInSpline; ++CurveEntryIdx)
		{
			const FLandscapeSplineCurveAttributes& Attributes = SplineInfo.CurveAttributes[CurveEntryIdx];
			ULandscapeSplineControlPoint* PreviousControlPoint = nullptr;
			int32 PreviousControlPointArrayIdx = INDEX_NONE;

			const int32 NumPointsInCurve = SplineInfo.PerCurvePointCount[CurveEntryIdx];
			for (int32 CurvePointArrayIdx = 0; CurvePointArrayIdx < NumPointsInCurve; ++CurvePointArrayIdx)
			{
				const int32 HGPOPointIndex = SplineInfo.PerCurveFirstPointIndex[CurveEntryIdx] + CurvePointArrayIdx;
				
				// Check if this is a control point: it has a control point name attribute, or is the first or last
				// point of the curve prim.
				FName ControlPointName = NAME_None;
				if (!Attributes.PointNames.IsValidIndex(CurvePointArrayIdx))
				{
					HOUDINI_LOG_WARNING(TEXT("Point index %d out of range for " HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_NAME " attribute."), CurvePointArrayIdx);
				}
				else
				{
					ControlPointName = *Attributes.PointNames[CurvePointArrayIdx]; 
				}

				bool bControlPointCreated = false;
				ULandscapeSplineControlPoint* ThisControlPoint = nullptr;
				// A point is a control point if:
				// 1. It is the first or last point of the curve, or
				// 2. It has non-blank control point name attribute
				if (!PreviousControlPoint || CurvePointArrayIdx == NumPointsInCurve - 1 || !ControlPointName.IsNone())
					ThisControlPoint = GetOrCreateControlPoint(SplineInfo, ControlPointName, bControlPointCreated);

				if (bControlPointCreated && IsValid(ThisControlPoint))
				{
					ControlPoints.Add(ThisControlPoint);
					ThisControlPoint->Location = WorldTransform.InverseTransformPosition(
						ConvertPositionToVector(&Attributes.PointPositions[CurvePointArrayIdx * 3]));

					// Update generic properties attributes on the control point
					if (bHasGenericPointAttributes)
						FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(ThisControlPoint, GenericPointAttributes, HGPOPointIndex);

					// Apply point attributes
					UpdateControlPointFromAttributes(ThisControlPoint, Attributes, WorldTransform, CurvePointArrayIdx);
				}

				// If we have two control points, create a segment
				if (PreviousControlPoint && ThisControlPoint)
				{
					// Create the segment
					ULandscapeSplineSegment* Segment = NewObject<ULandscapeSplineSegment>(
						SplineInfo.SplinesComponent, ULandscapeSplineSegment::StaticClass());
					Segment->Connections[0].ControlPoint = PreviousControlPoint;
					Segment->Connections[1].ControlPoint = ThisControlPoint;

					// Update generic properties attributes on the segment
					if (bHasGenericPointAttributes)
						FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(Segment, GenericPointAttributes, SplineInfo.PerCurveFirstPointIndex[CurveEntryIdx]);
					if (bHasGenericPrimAttributes)
						FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(Segment, GenericPrimAttributes, SplineInfo.CurveIndices[CurveEntryIdx]);

					// Apply attributes to segment
					UpdateSegmentFromAttributes(Segment, Attributes, CurvePointArrayIdx);

					// Apply attributes for connections
					UpdateConnectionFromAttributes(Segment->Connections[0], 0, Attributes, PreviousControlPointArrayIdx);
					UpdateConnectionFromAttributes(Segment->Connections[1], 1, Attributes, CurvePointArrayIdx);
					
					FVector StartLocation; FRotator StartRotation;
					PreviousControlPoint->GetConnectionLocationAndRotation(Segment->Connections[0].SocketName, StartLocation, StartRotation);
					FVector EndLocation; FRotator EndRotation;
					ThisControlPoint->GetConnectionLocationAndRotation(Segment->Connections[1].SocketName, EndLocation, EndRotation);

					// Set up tangent lengths if not in vertex/prim connection attributes
					if (!(Attributes.bHasVertexConnectionTangentLengthAttribute[0] && Attributes.VertexConnectionTangentLengths[0].IsValidIndex(PreviousControlPointArrayIdx))
						|| !(Attributes.bHasPrimConnectionTangentLengthAttribute[0] && Attributes.PrimConnectionTangentLengths[0]))
					{
						Segment->Connections[0].TangentLen = (EndLocation - StartLocation).Size();
					}
					if (!(Attributes.bHasVertexConnectionTangentLengthAttribute[1] && Attributes.VertexConnectionTangentLengths[1].IsValidIndex(CurvePointArrayIdx))
						|| !(Attributes.bHasPrimConnectionTangentLengthAttribute[1] && Attributes.PrimConnectionTangentLengths[1]))
					{
						Segment->Connections[1].TangentLen = Segment->Connections[0].TangentLen;
					}

					Segment->AutoFlipTangents();
					
					PreviousControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 0));
					ThisControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 1));

					// Auto-calculate rotation if we didn't receive rotation attributes
					if (!Attributes.bHasPointRotationAttribute || !Attributes.PointRotations.IsValidIndex(PreviousControlPointArrayIdx)) 
						PreviousControlPoint->AutoCalcRotation();
					if (!Attributes.bHasPointRotationAttribute || !Attributes.PointRotations.IsValidIndex(CurvePointArrayIdx)) 
						ThisControlPoint->AutoCalcRotation();
					
					Segments.Add(Segment);
				}

				// If we created a control point in this iteration, record that as the previous control point for the
				// next iteration
				if (ThisControlPoint)
				{
					PreviousControlPoint = ThisControlPoint;
					PreviousControlPointArrayIdx = CurvePointArrayIdx;
				}
			}
		}

		SplineInfo.SplinesComponent->RebuildAllSplines();
		
		FHoudiniOutputObject* const OutputObject = OutputSplines.Find(SplineInfo.Identifier);
		
		// Cache commonly supported Houdini attributes on the OutputAttributes
		TArray<FString> LevelPaths;
		if (OutputObject && FHoudiniEngineUtils::GetLevelPathAttribute(
			InHGPO.GeoId, InHGPO.PartId, LevelPaths, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (LevelPaths.Num() > 0 && !LevelPaths[0].IsEmpty())
			{
				// cache the level path attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_LEVEL_PATH, LevelPaths[0]);
			}
		}

		// cache the output name attribute on the output object
		OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, SplineInfo.OutputName.ToString());

		const int32 FirstCurvePrimIndex = SplineInfo.CurveIndices.Num() > 0 ? SplineInfo.CurveIndices[0] : INDEX_NONE;
		
		TArray<FString> BakeNames;
		if (OutputObject && FirstCurvePrimIndex != INDEX_NONE && FHoudiniEngineUtils::GetBakeNameAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeNames, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1))
		{
			if (BakeNames.Num() > 0 && !BakeNames[0].IsEmpty())
			{
				// cache the output name attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_NAME, BakeNames[0]);
			}
		}

		TArray<FString> BakeOutputActorNames;
		if (OutputObject && FirstCurvePrimIndex != INDEX_NONE && FHoudiniEngineUtils::GetBakeActorAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeOutputActorNames, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1))
		{
			if (BakeOutputActorNames.Num() > 0 && !BakeOutputActorNames[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR, BakeOutputActorNames[0]);
			}
		}

		TArray<FString> BakeOutputActorClassNames;
		if (OutputObject && FirstCurvePrimIndex != INDEX_NONE && FHoudiniEngineUtils::GetBakeActorClassAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeOutputActorClassNames, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1))
		{
			if (BakeOutputActorClassNames.Num() > 0 && !BakeOutputActorClassNames[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS, BakeOutputActorClassNames[0]);
			}
		}

		TArray<FString> BakeFolders;
		if (OutputObject && FHoudiniEngineUtils::GetBakeFolderAttribute(
			InHGPO.GeoId, BakeFolders, InHGPO.PartId, 0, 1))
		{
			if (BakeFolders.Num() > 0 && !BakeFolders[0].IsEmpty())
			{
				// cache the unreal_bake_folder attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_FOLDER, BakeFolders[0]);
			}
		}

		TArray<FString> BakeOutlinerFolders;
		if (OutputObject && FirstCurvePrimIndex != INDEX_NONE && FHoudiniEngineUtils::GetBakeOutlinerFolderAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeOutlinerFolders, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1))
		{
			if (BakeOutlinerFolders.Num() > 0 && !BakeOutlinerFolders[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, BakeOutlinerFolders[0]);
			}
		}

		if (SplineInfo.bReusedPreviousOutput)
		{
			// Remove the reused output object from the old map to avoid its deletion
			InCurrentSplines.Remove(SplineInfo.Identifier);
		}
	}

	return true;
#endif
}

ULandscapeSplineControlPoint*
FHoudiniLandscapeSplineTranslator::GetOrCreateControlPoint(FLandscapeSplineInfo& SplineInfo, const FName InDesiredName, bool& bOutCreated)
{
	ULandscapeSplineControlPoint* ControlPoint = nullptr;
	if (InDesiredName.IsNone() || !SplineInfo.ControlPointMap.Contains(InDesiredName))
	{
		// Point has not yet been created, so create it
		// Have to ensure the name is unique (using InDesiredName as a base).
		FName NewObjectName = InDesiredName;
		if (StaticFindObjectFast(ULandscapeSplineControlPoint::StaticClass(), SplineInfo.SplinesComponent, InDesiredName))
		{
			NewObjectName = MakeUniqueObjectName(SplineInfo.SplinesComponent, ULandscapeSplineControlPoint::StaticClass(), InDesiredName);
		}
		ControlPoint = NewObject<ULandscapeSplineControlPoint>(SplineInfo.SplinesComponent, ULandscapeSplineControlPoint::StaticClass(), NewObjectName);
		SplineInfo.ControlPointMap.Add(InDesiredName, ControlPoint);
		bOutCreated = true;
	}
	else
	{
		// Found the previously created point, just return it
		ControlPoint = SplineInfo.ControlPointMap[InDesiredName];
		bOutCreated = false;
	}

	return ControlPoint;
}


bool
FHoudiniLandscapeSplineTranslator::CopySegmentMeshAttributesFromHoudini(
	const HAPI_NodeId InNodeId,
	const HAPI_PartId InPartId,
	const HAPI_AttributeOwner InAttrOwner,
	const int32 InStartIndex,
	const int32 InCount,
	TArray<FLandscapeSplineSegmentMeshAttributes>& OutAttributes)
{
	OutAttributes.Reset();

	// Loop look for segment mesh attributes with MeshIndex as a suffix (when > 0). Break out of the loop as soon as
	// we cannot find any segment mesh attribute for the given MeshIndex.
	int32 MeshIndex = 0;
	while (true)
	{
		// If MeshIndex == 0 then don't add the numeric suffix
		const FString AttrNamePrefix = MeshIndex > 0
			? FString::Printf(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH "%d"), MeshIndex)
			: FString(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH));

		bool bFoundDataForMeshIndex = false;
		
		FLandscapeSplineSegmentMeshAttributes SegmentAttributes;
		
		// mesh ref
		static constexpr int32 TupleSizeOne = 1;
		HAPI_AttributeInfo MeshRefAttrInfo;
		SegmentAttributes.bHasMeshRefAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InNodeId,
			InPartId,
			TCHAR_TO_ANSI(*AttrNamePrefix),
			MeshRefAttrInfo,
			SegmentAttributes.MeshRef,
			TupleSizeOne,
			InAttrOwner,
			InStartIndex,
			InCount);
		if (SegmentAttributes.bHasMeshRefAttribute)
			bFoundDataForMeshIndex = true;

		// mesh scale
		static constexpr int32 MeshScaleTupleSize = 3;
		const FString MeshScaleAttrName = FString::Printf(
			TEXT("%s%s"), *AttrNamePrefix, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX));

		HAPI_AttributeInfo MeshScaleAttrInfo;
		SegmentAttributes.bHasMeshScaleAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			InNodeId,
			InPartId,
			TCHAR_TO_ANSI(*MeshScaleAttrName),
			MeshScaleAttrInfo,
			SegmentAttributes.MeshScale,
			MeshScaleTupleSize,
			InAttrOwner,
			InStartIndex,
			InCount);
		if (SegmentAttributes.bHasMeshScaleAttribute)
			bFoundDataForMeshIndex = true;

		// material overrides
		const FString MaterialAttrNamePrefix = AttrNamePrefix + TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX);
		SegmentAttributes.MeshMaterialOverrideRefs.Reset();

		// The same as with the MeshIndex above, loop until the first iteration where we cannot find a material
		// override attribute
		int32 MaterialOverrideIdx = 0;
		while (true)
		{
			TArray<FString> MaterialOverrides;

			// Add the MaterialOverrideIdx as a suffix to the attribute name when > 0
			const FString MaterialOverrideAttrName = MaterialOverrideIdx > 0
				? MaterialAttrNamePrefix + FString::Printf(TEXT("%d"), MaterialOverrideIdx)
				: MaterialAttrNamePrefix;

			HAPI_AttributeInfo MaterialOverrideAttrInfo;
			if (!FHoudiniEngineUtils::HapiGetAttributeDataAsString(
					InNodeId,
					InPartId,
					TCHAR_TO_ANSI(*MaterialOverrideAttrName),
					MaterialOverrideAttrInfo,
					MaterialOverrides,
					TupleSizeOne,
					InAttrOwner,
					InStartIndex,
					InCount))
			{
				break;
			}
			
			SegmentAttributes.MeshMaterialOverrideRefs.Emplace(MoveTemp(MaterialOverrides));
			bFoundDataForMeshIndex = true;
			MaterialOverrideIdx++;
		}
		SegmentAttributes.MeshMaterialOverrideRefs.Shrink();

		if (!bFoundDataForMeshIndex)
			break;

		OutAttributes.Emplace(MoveTemp(SegmentAttributes));

		MeshIndex++;
	}
	OutAttributes.Shrink();

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::CopyCurveAttributesFromHoudini(
	const HAPI_NodeId InNodeId,
	const HAPI_PartId InPartId,
	const int32 InPrimIndex,
	const int32 InFirstPointIndex,
	const int32 InNumPoints,
	FLandscapeSplineCurveAttributes& OutCurveAttributes)
{
	// Some constants that are reused
	// Tuple size of 1
	static constexpr int32 TupleSizeOne = 1;
	// Prim attribute data count of 1
	static constexpr int32 NumPrimsOne = 1;

	// point positions
	static constexpr int32 PositionTupleSize = 3;
	HAPI_AttributeInfo PositionAttrInfo;
	FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_POSITION,
		PositionAttrInfo,
		OutCurveAttributes.PointPositions,
		PositionTupleSize,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// rot attribute (quaternion) -- control point rotations
	static constexpr int32 RotationTupleSize = 4;
	HAPI_AttributeInfo RotationAttrInfo;
	OutCurveAttributes.bHasPointRotationAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_ROTATION,
		RotationAttrInfo,
		OutCurveAttributes.PointRotations,
		RotationTupleSize,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point paint layer names
	HAPI_AttributeInfo LayerNameAttrInfo;
	OutCurveAttributes.bHasPointPaintLayerNameAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_PAINT_LAYER_NAME,
		LayerNameAttrInfo,
		OutCurveAttributes.PointPaintLayerNames,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point raise terrains
	HAPI_AttributeInfo RaiseTerrainAttrInfo;
	OutCurveAttributes.bHasPointRaiseTerrainAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_RAISE_TERRAIN,
		RaiseTerrainAttrInfo,
		OutCurveAttributes.PointRaiseTerrains,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point lower terrains
	HAPI_AttributeInfo LowerTerrainAttrInfo;
	OutCurveAttributes.bHasPointLowerTerrainAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_LOWER_TERRAIN,
		LowerTerrainAttrInfo,
		OutCurveAttributes.PointLowerTerrains,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point meshes
	HAPI_AttributeInfo ControlPointMeshAttrInfo;
	OutCurveAttributes.bHasPointMeshRefAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH,
		ControlPointMeshAttrInfo,
		OutCurveAttributes.PointMeshRefs,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point material overrides
	OutCurveAttributes.PerMaterialOverridePointRefs.Reset();
	const FString ControlPointMaterialOverrideAttrNamePrefix = TEXT(
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX);

	// Loop until the first iteration where we don't find any material override attributes.
	int32 MaterialOverrideIdx = 0;
	while (true)
	{
		TArray<FString> MaterialOverrides;

		// If the index > 0 add it as a suffix to the attribute name
		const FString AttrName = MaterialOverrideIdx > 0
			? ControlPointMaterialOverrideAttrNamePrefix + FString::Printf(TEXT("%d"), MaterialOverrideIdx)
			: ControlPointMaterialOverrideAttrNamePrefix;

		HAPI_AttributeInfo AttrInfo;
		if (!FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InNodeId,
			InPartId,
			TCHAR_TO_ANSI(*AttrName),
			AttrInfo,
			MaterialOverrides,
			TupleSizeOne,
			HAPI_ATTROWNER_POINT,
			InFirstPointIndex,
			InNumPoints))
		{
			break;
		}
		
		OutCurveAttributes.PerMaterialOverridePointRefs.Emplace(MoveTemp(MaterialOverrides));
		MaterialOverrideIdx++;
	}

	// control point mesh scales
	static constexpr int32 MeshScaleTupleSize = 3;
	HAPI_AttributeInfo MeshScaleAttrInfo;
	OutCurveAttributes.bHasPointMeshScaleAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX,
		MeshScaleAttrInfo,
		OutCurveAttributes.PointMeshScales,
		MeshScaleTupleSize,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point names
	HAPI_AttributeInfo ControlPointNameAttrInfo;
	OutCurveAttributes.bHasPointNameAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_NAME,
		ControlPointNameAttrInfo,
		OutCurveAttributes.PointNames,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// point half-widths
	HAPI_AttributeInfo HalfWidthAttrInfo;
	OutCurveAttributes.bHasPointHalfWidthAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_HALF_WIDTH,
		HalfWidthAttrInfo,
		OutCurveAttributes.PointHalfWidths,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// Connection attributes -- there are separate attributes for the two ends of the connection
	static const char* ConnectionMeshSocketNameAttrNames[]
	{
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONNECTION0_MESH_SOCKET_NAME,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONNECTION1_MESH_SOCKET_NAME
	};
	static const char* ConnectionTangentLengthAttrNames[]
	{
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONNECTION0_TANGENT_LENGTH,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONNECTION1_TANGENT_LENGTH
	};
	for (int32 ConnectionIndex = 0; ConnectionIndex < 2; ++ConnectionIndex)
	{
		// segment connection[ConnectionIndex] socket names -- vertex/point attribute
		HAPI_AttributeInfo MeshSocketNameAttrInfo;
		OutCurveAttributes.bHasVertexConnectionSocketNameAttribute[ConnectionIndex] = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InNodeId,
			InPartId,
			ConnectionMeshSocketNameAttrNames[ConnectionIndex],
			MeshSocketNameAttrInfo,
			OutCurveAttributes.VertexConnectionSocketNames[ConnectionIndex],
			TupleSizeOne,
			HAPI_ATTROWNER_POINT,
			InFirstPointIndex,
			InNumPoints);

		// segment connection[ConnectionIndex] tangents -- vertex/point attribute
		HAPI_AttributeInfo TangentLengthAttrInfo;
		OutCurveAttributes.bHasVertexConnectionTangentLengthAttribute[ConnectionIndex] = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			InNodeId,
			InPartId,
			ConnectionTangentLengthAttrNames[ConnectionIndex],
			TangentLengthAttrInfo,
			OutCurveAttributes.VertexConnectionTangentLengths[ConnectionIndex],
			TupleSizeOne,
			HAPI_ATTROWNER_POINT,
			InFirstPointIndex,
			InNumPoints);

		// segment connection[ConnectionIndex] socket names -- prim attribute
		if (!OutCurveAttributes.bHasVertexConnectionSocketNameAttribute[ConnectionIndex])
		{
			TArray<FString> SocketNames;
			HAPI_AttributeInfo PrimMeshSocketNameAttrInfo;
			OutCurveAttributes.bHasPrimConnectionSocketNameAttribute[ConnectionIndex] = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				InNodeId,
				InPartId,
				ConnectionMeshSocketNameAttrNames[ConnectionIndex],
				PrimMeshSocketNameAttrInfo,
				SocketNames,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne);
			if (OutCurveAttributes.bHasPrimConnectionSocketNameAttribute[ConnectionIndex] && SocketNames.Num() > 0)
			{
				OutCurveAttributes.PrimConnectionSocketNames[ConnectionIndex] = SocketNames[0];
			}
		}
		else
		{
			OutCurveAttributes.bHasPrimConnectionSocketNameAttribute[ConnectionIndex] = false;
		}

		// segment connection[ConnectionIndex] tangents -- prim attribute
		if (!OutCurveAttributes.bHasVertexConnectionTangentLengthAttribute[ConnectionIndex])
		{
			TArray<float> Tangents;
			HAPI_AttributeInfo PrimTangentLengthAttrInfo;
			OutCurveAttributes.bHasPrimConnectionTangentLengthAttribute[ConnectionIndex] = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
				InNodeId,
				InPartId,
				ConnectionTangentLengthAttrNames[ConnectionIndex],
				PrimTangentLengthAttrInfo,
				Tangents,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne);
			if (OutCurveAttributes.bHasPrimConnectionTangentLengthAttribute[ConnectionIndex] && Tangents.Num() > 0)
			{
				OutCurveAttributes.PrimConnectionTangentLengths[ConnectionIndex] = Tangents[0];
			}
		}
		else
		{
			OutCurveAttributes.bHasPrimConnectionTangentLengthAttribute[ConnectionIndex] = false;
		}
	}

	// segment paint layer name -- vertex/point
	HAPI_AttributeInfo VertexLayerNameAttrInfo;
	OutCurveAttributes.bHasVertexPaintLayerNameAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_PAINT_LAYER_NAME,
		VertexLayerNameAttrInfo,
		OutCurveAttributes.VertexPaintLayerNames,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// segment raise terrains -- vertex/point
	HAPI_AttributeInfo VertexRaiseTerrainAttrInfo;
	OutCurveAttributes.bHasVertexRaiseTerrainAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_RAISE_TERRAIN,
		VertexRaiseTerrainAttrInfo,
		OutCurveAttributes.VertexRaiseTerrains,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// segment lower terrains -- vertex/point
	HAPI_AttributeInfo VertexLowerTerrainAttrInfo;
	OutCurveAttributes.bHasVertexLowerTerrainAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_LOWER_TERRAIN,
		VertexLowerTerrainAttrInfo,
		OutCurveAttributes.VertexLowerTerrains,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);
	
	// segment paint layer name
	if (!OutCurveAttributes.bHasVertexPaintLayerNameAttribute)
	{
		TArray<FString> SegmentPaintLayerName;
		HAPI_AttributeInfo PrimLayerNameAttrInfo;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				InNodeId,
				InPartId,
				HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_PAINT_LAYER_NAME,
				PrimLayerNameAttrInfo,
				SegmentPaintLayerName,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne) && SegmentPaintLayerName.Num() > 0)
		{
			OutCurveAttributes.PrimPaintLayerName = SegmentPaintLayerName[0];
			OutCurveAttributes.bHasPrimPaintLayerNameAttribute = true;
		}
		else
		{
			OutCurveAttributes.PrimPaintLayerName = FString();
			OutCurveAttributes.bHasPrimPaintLayerNameAttribute = false;
		}
	}
	else
	{
		OutCurveAttributes.bHasPrimPaintLayerNameAttribute = false;
	}

	// segment raise terrains
	if (!OutCurveAttributes.bHasVertexRaiseTerrainAttribute)
	{
		TArray<int32> RaiseTerrains;
		HAPI_AttributeInfo PrimRaiseTerrainAttrInfo;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
				InNodeId,
				InPartId,
				HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_RAISE_TERRAIN,
				PrimRaiseTerrainAttrInfo,
				RaiseTerrains,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne) && RaiseTerrains.Num() > 0)
		{
			OutCurveAttributes.bPrimRaiseTerrain = RaiseTerrains[0];
			OutCurveAttributes.bHasPrimRaiseTerrainAttribute = true;
		}
		else
		{
			OutCurveAttributes.bPrimRaiseTerrain = false;
			OutCurveAttributes.bHasPrimRaiseTerrainAttribute = false;
		}
	}
	else
	{
		OutCurveAttributes.bHasPrimRaiseTerrainAttribute = false;
	}

	// segment lower terrains
	if (!OutCurveAttributes.bHasVertexLowerTerrainAttribute)
	{
		TArray<int32> LowerTerrains;
		HAPI_AttributeInfo PrimLowerTerrainAttrInfo;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
				InNodeId,
				InPartId,
				HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_LOWER_TERRAIN,
				PrimLowerTerrainAttrInfo,
				LowerTerrains,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne) && LowerTerrains.Num() > 0)
		{
			OutCurveAttributes.bPrimLowerTerrain = LowerTerrains[0];
			OutCurveAttributes.bHasPrimLowerTerrainAttribute = true;
		}
		else
		{
			OutCurveAttributes.bPrimLowerTerrain = false;
			OutCurveAttributes.bHasPrimLowerTerrainAttribute = false;
		}
	}
	else
	{
		OutCurveAttributes.bHasPrimLowerTerrainAttribute = false;
	}

	// Copy segment mesh attributes from Houdini -- vertex/point attributes
	if (!CopySegmentMeshAttributesFromHoudini(
			InNodeId, InPartId, HAPI_ATTROWNER_POINT, InFirstPointIndex, InNumPoints, OutCurveAttributes.VertexPerMeshSegmentData))
	{
		return false;
	}

	// Copy segment mesh attributes from Houdini -- prim attributes
	if (!CopySegmentMeshAttributesFromHoudini(
			InNodeId, InPartId, HAPI_ATTROWNER_PRIM, InPrimIndex, 1, OutCurveAttributes.PrimPerMeshSegmentData))
	{
		return false;
	}

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::UpdateControlPointFromAttributes(
		ULandscapeSplineControlPoint* const InPoint,
		const FLandscapeSplineCurveAttributes& InAttributes,
		const FTransform& InWorldTransform,
		const int32 InPointIndex)
{
	if (!IsValid(InPoint))
		return false;

	// Apply the attributes from Houdini (InAttributes) to the control point InPoint

	// Rotation
	if (InAttributes.bHasPointRotationAttribute
			&& InAttributes.PointRotations.IsValidIndex(InPointIndex * 4) && InAttributes.PointRotations.IsValidIndex(InPointIndex * 4 + 3))
	{
		// Convert Houdini Y-up to UE Z-up and also Houdini -Z-forward to UE X-forward
		InPoint->Rotation = (InWorldTransform.InverseTransformRotation({
			InAttributes.PointRotations[InPointIndex * 4 + 0],
			InAttributes.PointRotations[InPointIndex * 4 + 2],
			InAttributes.PointRotations[InPointIndex * 4 + 1],
			-InAttributes.PointRotations[InPointIndex * 4 + 3]
		}) * FQuat(FVector::UpVector, FMath::DegreesToRadians(-90.0f))).Rotator();
	}

	// (Paint) layer name
	if (InAttributes.bHasPointPaintLayerNameAttribute && InAttributes.PointPaintLayerNames.IsValidIndex(InPointIndex))
	{
		InPoint->LayerName = *InAttributes.PointPaintLayerNames[InPointIndex];
	}

	// bRaiseTerrain
	if (InAttributes.bHasPointRaiseTerrainAttribute && InAttributes.PointRaiseTerrains.IsValidIndex(InPointIndex))
	{
		InPoint->bRaiseTerrain = InAttributes.PointRaiseTerrains[InPointIndex];
	}

	// bLowerTerrain
	if (InAttributes.bHasPointLowerTerrainAttribute && InAttributes.PointLowerTerrains.IsValidIndex(InPointIndex))
	{
		InPoint->bLowerTerrain = InAttributes.PointLowerTerrains[InPointIndex];
	}

	// Control point static mesh
	if (InAttributes.bHasPointMeshRefAttribute && InAttributes.PointMeshRefs.IsValidIndex(InPointIndex))
	{
		UObject* Mesh = StaticFindObject(UStaticMesh::StaticClass(), nullptr, *InAttributes.PointMeshRefs[InPointIndex]);
		if (!Mesh)
			Mesh = StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *InAttributes.PointMeshRefs[InPointIndex]);
		UStaticMesh* const SM = Mesh ? Cast<UStaticMesh>(Mesh) : nullptr;
		if (IsValid(SM))
			InPoint->Mesh = Cast<UStaticMesh>(Mesh);
		else
			InPoint->Mesh = nullptr;
	}

	// Control point static mesh material overrides
	if (InAttributes.PerMaterialOverridePointRefs.Num() > 0)
	{
		InPoint->MaterialOverrides.Reset(InAttributes.PerMaterialOverridePointRefs.Num());
		for (const TArray<FString>& PerPointMaterialOverrideX : InAttributes.PerMaterialOverridePointRefs)
		{
			if (!PerPointMaterialOverrideX.IsValidIndex(InPointIndex))
				continue;
			
			const FString& MaterialRef = PerPointMaterialOverrideX[InPointIndex];
			
			UObject* Material = StaticFindObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			if (!Material)
				Material = StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			UMaterialInterface* const MaterialInterface = Material ? Cast<UMaterialInterface>(Material) : nullptr;
			if (IsValid(MaterialInterface))
				InPoint->MaterialOverrides.Add(MaterialInterface);
			else
				InPoint->MaterialOverrides.Add(nullptr);
		}
		InPoint->MaterialOverrides.Shrink();
	}

	// Control point mesh scale
	if (InAttributes.bHasPointMeshScaleAttribute
			&& InAttributes.PointMeshScales.IsValidIndex(InPointIndex * 3) && InAttributes.PointMeshScales.IsValidIndex(InPointIndex * 3 + 2))
	{
		InPoint->MeshScale = FVector(
			InAttributes.PointMeshScales[InPointIndex * 3 + 0],
			InAttributes.PointMeshScales[InPointIndex * 3 + 2],
			InAttributes.PointMeshScales[InPointIndex * 3 + 1]);
	}

	// Control point half-width
	if (InAttributes.bHasPointHalfWidthAttribute && InAttributes.PointHalfWidths.IsValidIndex(InPointIndex))
	{
		// Convert from Houdini units to UE units
		InPoint->Width = InAttributes.PointHalfWidths[InPointIndex] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::UpdateSegmentFromAttributes(
	ULandscapeSplineSegment* const InSegment, const FLandscapeSplineCurveAttributes& InAttributes, const int32 InVertexIndex)
{
	if (!IsValid(InSegment))
		return false;

	// Update the segment (InSegment) with the attributes copied from Houdini (InAttributes)

	// Check the vertex/point attribute first, and if that is missing, try the prim attribute

	// (Paint) layer name
	if (InAttributes.bHasVertexPaintLayerNameAttribute && InAttributes.VertexPaintLayerNames.IsValidIndex(InVertexIndex))
	{
		InSegment->LayerName = *InAttributes.VertexPaintLayerNames[InVertexIndex];
	}
	else if (InAttributes.bHasPrimPaintLayerNameAttribute)
	{
		InSegment->LayerName = *InAttributes.PrimPaintLayerName;
	}

	// bRaiseTerrain
	if (InAttributes.bHasVertexRaiseTerrainAttribute && InAttributes.VertexRaiseTerrains.IsValidIndex(InVertexIndex))
	{
		InSegment->bRaiseTerrain = InAttributes.VertexRaiseTerrains[InVertexIndex];
	}
	else if (InAttributes.bHasPrimRaiseTerrainAttribute)
	{
		InSegment->bRaiseTerrain = InAttributes.bPrimRaiseTerrain;
	}

	// bLowerTerrain
	if (InAttributes.bHasVertexLowerTerrainAttribute && InAttributes.VertexLowerTerrains.IsValidIndex(InVertexIndex))
	{
		InSegment->bLowerTerrain = InAttributes.VertexLowerTerrains[InVertexIndex];
	}
	else if (InAttributes.bHasPrimLowerTerrainAttribute)
	{
		InSegment->bLowerTerrain = InAttributes.bPrimLowerTerrain;
	}

	// Segment static meshes
	const int32 MaxNumMeshAttrs = FMath::Max(InAttributes.VertexPerMeshSegmentData.Num(), InAttributes.PrimPerMeshSegmentData.Num());
	InSegment->SplineMeshes.Reset(MaxNumMeshAttrs);
	for (int32 MeshIdx = 0; MeshIdx < MaxNumMeshAttrs; ++MeshIdx)
	{
		FLandscapeSplineMeshEntry SplineMeshEntry;

		// For each index check the vertex/point attribute first, and if not set, check the prim attribute
		
		const FLandscapeSplineSegmentMeshAttributes* PerVertexAttributes = InAttributes.VertexPerMeshSegmentData.IsValidIndex(MeshIdx)
			? &InAttributes.VertexPerMeshSegmentData[MeshIdx] : nullptr;
		const FLandscapeSplineSegmentMeshAttributes* PerPrimAttributes = InAttributes.PrimPerMeshSegmentData.IsValidIndex(MeshIdx)
			? &InAttributes.PrimPerMeshSegmentData[MeshIdx] : nullptr;

		bool bHasMeshRef = false;
		FString MeshRef; 
		if (PerVertexAttributes && PerVertexAttributes->bHasMeshRefAttribute && PerVertexAttributes->MeshRef.IsValidIndex(InVertexIndex))
		{
			bHasMeshRef = true;
			MeshRef = PerVertexAttributes->MeshRef[InVertexIndex];
		}
		else if (PerPrimAttributes && PerPrimAttributes->bHasMeshRefAttribute && PerPrimAttributes->MeshRef.Num() > 0)
		{
			bHasMeshRef = true;
			MeshRef = PerPrimAttributes->MeshRef[0];
		}
		
		if (bHasMeshRef)
		{
			// We have a static mesh at this index, try to find / load it
			UObject* Mesh = StaticFindObject(UStaticMesh::StaticClass(), nullptr, *MeshRef);
			if (!Mesh)
				Mesh = StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MeshRef);
			UStaticMesh* const SM = Mesh ? Cast<UStaticMesh>(Mesh) : nullptr;
			if (IsValid(SM))
				SplineMeshEntry.Mesh = SM;
			else
				SplineMeshEntry.Mesh = nullptr;
		}

		// mesh scale
		if (PerVertexAttributes && PerVertexAttributes->bHasMeshScaleAttribute
				&& PerVertexAttributes->MeshScale.IsValidIndex(InVertexIndex * 3)
				&& PerVertexAttributes->MeshScale.IsValidIndex(InVertexIndex * 3 + 2))
		{
			const int32 ValueIdx = InVertexIndex * 3;
			SplineMeshEntry.Scale = FVector(
				PerVertexAttributes->MeshScale[ValueIdx + 0],
				PerVertexAttributes->MeshScale[ValueIdx + 2],
				PerVertexAttributes->MeshScale[ValueIdx + 1]);
		}
		else if (PerPrimAttributes && PerPrimAttributes->bHasMeshScaleAttribute && PerPrimAttributes->MeshScale.IsValidIndex(3))
		{
			SplineMeshEntry.Scale = FVector(
				PerVertexAttributes->MeshScale[0],
				PerVertexAttributes->MeshScale[2],
				PerVertexAttributes->MeshScale[1]);
		}

		// Each segment static mesh can have multiple material overrides
		// Determine the max from the number of vertex/point material override attributes and the number of prim
		// material override attributes
		const int32 MaxNumMaterialOverrides = FMath::Max(
			PerVertexAttributes ? PerVertexAttributes->MeshMaterialOverrideRefs.Num() : 0,
			PerPrimAttributes ? PerPrimAttributes->MeshMaterialOverrideRefs.Num() : 0);
		SplineMeshEntry.MaterialOverrides.Reset(MaxNumMaterialOverrides);
		for (int32 MaterialOverrideIdx = 0; MaterialOverrideIdx < MaxNumMaterialOverrides; ++MaterialOverrideIdx)
		{
			bool bHasMaterialRef = false;
			FString MaterialRef;

			// Check vertex/prim attribute first, if that is not set check the prim attribute.

			if (PerVertexAttributes && PerVertexAttributes->MeshMaterialOverrideRefs.IsValidIndex(MaterialOverrideIdx))
			{
				const TArray<FString>& PerVertexMaterialOverrides = PerVertexAttributes->MeshMaterialOverrideRefs[MaterialOverrideIdx];
				if (PerVertexMaterialOverrides.IsValidIndex(InVertexIndex))
				{
					bHasMaterialRef = true;
					MaterialRef = PerVertexMaterialOverrides[InVertexIndex];
				}
			}

			if (!bHasMaterialRef && PerPrimAttributes && PerPrimAttributes->MeshMaterialOverrideRefs.IsValidIndex(MaterialOverrideIdx))
			{
				const TArray<FString>& PerPrimMaterialOverrides = PerPrimAttributes->MeshMaterialOverrideRefs[MaterialOverrideIdx];
				if (PerPrimMaterialOverrides.Num() > 0)
				{
					bHasMaterialRef = true;
					MaterialRef = PerPrimMaterialOverrides[0];
				}
			}

			if (!bHasMaterialRef)
			{
				// No material override at this index, set it to null
				SplineMeshEntry.MaterialOverrides.Add(nullptr);
				continue;
			}

			// Found a material override at this index, try to find / load the it
			UObject* Material = StaticFindObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			if (!Material)
				Material = StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			UMaterialInterface* const MaterialInterface = Material ? Cast<UMaterialInterface>(Material) : nullptr;
			if (IsValid(MaterialInterface))
				SplineMeshEntry.MaterialOverrides.Add(MaterialInterface);
			else
				SplineMeshEntry.MaterialOverrides.Add(nullptr);
		}
		SplineMeshEntry.MaterialOverrides.Shrink();

		InSegment->SplineMeshes.Emplace(SplineMeshEntry);
	}
	InSegment->SplineMeshes.Shrink();

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::UpdateConnectionFromAttributes(
	FLandscapeSplineSegmentConnection& InConnection,
	const int32 InConnectionIndex,
	const FLandscapeSplineCurveAttributes& InAttributes,
	const int32 InPointIndex)
{
	// Update the InConnection's properties from the attributes copied from Houdini.
	// Check the vertex/point attribute first, if that is not set, use the prim attribute.

	// socket name
	if (InAttributes.bHasVertexConnectionSocketNameAttribute[InConnectionIndex] && InAttributes.VertexConnectionSocketNames[InConnectionIndex].IsValidIndex(InPointIndex))
	{
		InConnection.SocketName = *InAttributes.VertexConnectionSocketNames[InConnectionIndex][InPointIndex];
	}
	else if (InAttributes.bHasPrimConnectionSocketNameAttribute[InConnectionIndex])
	{
		InConnection.SocketName = *InAttributes.PrimConnectionSocketNames[InConnectionIndex];
	}

	// tangent length
	if (InAttributes.bHasVertexConnectionTangentLengthAttribute[InConnectionIndex] && InAttributes.VertexConnectionTangentLengths[InConnectionIndex].IsValidIndex(InPointIndex))
	{
		InConnection.TangentLen = InAttributes.VertexConnectionTangentLengths[InConnectionIndex][InPointIndex];
	}
	else if (InAttributes.bHasPrimConnectionTangentLengthAttribute[InConnectionIndex])
	{
		InConnection.TangentLen = InAttributes.PrimConnectionTangentLengths[InConnectionIndex];
	}

	return true;
}