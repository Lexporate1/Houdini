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
*      Damien Pernuit
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once

#include "HoudiniGeoPartObject.h"
#include "Landscape.h"

struct HOUDINIENGINERUNTIME_API FHoudiniLandscapeUtils
{
    public:

        //--------------------------------------------------------------------------------------------------
        // Houdini to Unreal
        //--------------------------------------------------------------------------------------------------

        // Returns Heightfield contained in the GeoPartObject array
        static void GetHeightfieldsInArray(
            const TArray< FHoudiniGeoPartObject >& InArray,
            TArray< const FHoudiniGeoPartObject* >& OutHeightfields );

        // Returns the layers corresponding to a height field contained in the GeoPartObject array
        static void GetHeightfieldsLayersInArray(
            const TArray< FHoudiniGeoPartObject >& InArray,
            const FHoudiniGeoPartObject& Heightfield,
            TArray< const FHoudiniGeoPartObject* >& FoundLayers );

        // Returns the global ZMin/ZMax value of all heightfields contained in the array
        static void CalcHeightfieldsArrayGlobalZMinZMax(
            const TArray< const FHoudiniGeoPartObject* >& InHeightfieldArray,
            float& fGlobalMin, float& fGlobalMax );

        // Extract the float values of a given heightfield
        static bool GetHeightfieldData(
            const FHoudiniGeoPartObject& Heightfield,
            TArray< float >& FloatValues,
            HAPI_VolumeInfo& VolumeInfo,
            float& FloatMin, float& FloatMax );

        // Converts the Houdini Float height values to Unreal uint16
        static bool ConvertHeightfieldDataToLandscapeData(
            const TArray< float >& HeightfieldFloatValues,
            const HAPI_VolumeInfo& HeightfieldVolumeInfo,
            const float& FloatMin, const float& FloatMax,
            TArray< uint16 >& IntHeightData,
            FTransform& LandscapeTransform,
            int32& FinalXSize, int32& FinalYSize,
            int32& NumSectionPerLandscapeComponent,
            int32& NumQuadsPerLandscapeSection );

        // Converts the Houdini float layer values to Unreal uint8
        static bool ConvertHeightfieldLayerToLandscapeLayer(
            const TArray< float >& FloatLayerData,
            const int32& LayerXSize, const int32& LayerYSize,
            const float& LayerMin, const float& LayerMax,
            const int32& LandscapeXSize, const int32& LandscapeYSize,
            TArray< uint8 >& LayerData );

        // Resizes the HeightData so that it fits to UE4's size requirements.
        static bool ResizeHeightDataForLandscape(
            TArray< uint16 >& HeightData,
            int32& SizeX, int32& SizeY,
            int32& NumberOfSectionsPerComponent,
            int32& NumberOfQuadsPerSection,
            FVector& LandscapeResizeFactor );

        // Resizes LayerData so that it fits the Landscape size
        static bool ResizeLayerDataForLandscape(
            TArray< uint8 >& LayerData,
            const int32& SizeX, const int32& SizeY,
            const int32& NewSizeX, const int32& NewSizeY );

        //--------------------------------------------------------------------------------------------------
        // Unreal to Houdini
        //--------------------------------------------------------------------------------------------------
#if WITH_EDITOR
        // Temporary test function, creates a heightfield from a Landscape
        static bool CreateHeightfieldFromLandscape(
            ALandscape* Landscape, const HAPI_NodeId& AssetId );

        static bool CreateHeightfieldFromLandscapeComponent(
            ULandscapeComponent * LandscapeComponent, const HAPI_NodeId& AssetId );

        // Extracts the uint16 values of a given landscape
        static bool GetLandscapeData(
            ALandscape* Landscape,
            TArray<uint16>& HeightData,
            int32& XSize, int32& YSize,
            FVector& Min, FVector& Max );

        static bool GetLandscapeData(
            ULandscapeInfo* LandscapeInfo,
            const int32& MinX, const int32& MinY,
            const int32& MaxX, const int32& MaxY,
            TArray<uint16>& HeightData,
            int32& XSize, int32& YSize );

        static bool GetLandscapeLayerData(
            ULandscapeInfo* LandscapeInfo, const int32& LayerIndex,
            TArray<uint8>& LayerData, FLinearColor& LayerUsageDebugColor,
            FString& LayerName );

        static bool GetLandscapeLayerData(
            ULandscapeInfo* LandscapeInfo,
            const int32& LayerIndex,
            const int32& MinX, const int32& MinY,
            const int32& MaxX, const int32& MaxY,
            TArray<uint8>& LayerData,
            FLinearColor& LayerUsageDebugColor,
            FString& LayerName );
#endif

        // Converts Unreal uint16 values to Houdini Float
        static bool ConvertLandscapeDataToHeightfieldData(
            const TArray<uint16>& IntHeightData,
            const int32& XSize, const int32& YSize,
            FVector Min, FVector Max,
            const FTransform& LandscapeTransform,
            TArray<float>& HeightfieldFloatValues,
            HAPI_VolumeInfo& HeightfieldVolumeInfo );

        // Converts Unreal uint16 values to Houdini Float
        static bool ConvertLandscapeLayerDataToHeightfieldData(
            const TArray<uint8>& IntHeightData,
            const int32& XSize, const int32& YSize,
            const FLinearColor& LayerUsageDebugColor,
            TArray<float>& LayerFloatValues,
            HAPI_VolumeInfo& LayerVolumeInfo);

        // Set the volume float value for a heightfield
        static bool SetHeighfieldData(
            const HAPI_NodeId& AssetId, const HAPI_PartId& PartId,
            const TArray<float>& FloatValues,
            const HAPI_VolumeInfo& VolumeInfo,
            const FString& HeightfieldName );

        /*
        // Duplicate a given Landscape. This will create a new package for it. This will also create necessary
        // materials, textures, landscape layers and their corresponding packages.
        static ALandscape * DuplicateLandscapeAndCreatePackage(
        const ALandscape * Landscape, UHoudiniAssetComponent * Component,
        const FHoudiniGeoPartObject & HoudiniGeoPartObject, EBakeMode BakeMode );
        */
};
