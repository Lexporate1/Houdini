/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
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
#include "HoudiniSplineComponent.generated.h"

namespace EHoudiniSplineComponentType
{
    enum Enum
    {
        Polygon,
        Nurbs,
        Bezier
    };
}

namespace EHoudiniSplineComponentMethod
{
    enum Enum
    {
        CVs,
        Breakpoints,
        Freehand
    };
}

UCLASS( config = Engine )
class HOUDINIENGINERUNTIME_API UHoudiniSplineComponent : public USceneComponent
{
    public:
        friend class UHoudiniAssetComponent;

#if WITH_EDITOR

        friend class FHoudiniSplineComponentVisualizer;

#endif // WITH_EDITOR

        GENERATED_UCLASS_BODY()

        virtual ~UHoudiniSplineComponent();

    /** UObject methods. **/
    public:

        virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR

        virtual void PostEditUndo() override;

#endif // WITH_EDITOR

    public:

        /** Construct spline from given information. Resets any existing state. **/
        bool Construct(
            const FHoudiniGeoPartObject & InHoudiniGeoPartObject,
            const TArray< FTransform > & InCurvePoints,
            const TArray< FVector > & InCurveDisplayPoints,
            EHoudiniSplineComponentType::Enum InCurveType,
            EHoudiniSplineComponentMethod::Enum InCurveMethod,
            bool bInClosedCurve = false );


        /** Construct spline from given information. Resets any existing state. **/
        bool Construct(
            const FHoudiniGeoPartObject & InHoudiniGeoPartObject,
            const TArray< FVector > & InCurveDisplayPoints,
            EHoudiniSplineComponentType::Enum InCurveType,
            EHoudiniSplineComponentMethod::Enum InCurveMethod,
            bool bInClosedCurve = false);

	void SetHoudiniGeoPartObject(const FHoudiniGeoPartObject& InHoudiniGeoPartObject);


        /** Copies data from an another curve. Resets any existing state **/
        bool CopyFrom( UHoudiniSplineComponent* InSplineComponent );

        /** Return the type of this curve. **/
        EHoudiniSplineComponentType::Enum GetCurveType() const;

        /** Return method used by this curve. **/
        EHoudiniSplineComponentMethod::Enum GetCurveMethod() const;

        /** Return true if this curve is closed. **/
        bool IsClosedCurve() const;

        /** Return number of curve points. **/
        int32 GetCurvePointCount() const;

        /** Resets all points of this curve. **/
        void ResetCurvePoints();

        /** Reset display points of this curve. **/
        void ResetCurveDisplayPoints();

        /** Add a point to this curve. **/
        void AddPoint( const FTransform & Point );

        /** Add points to this curve. **/
        void AddPoints( const TArray< FTransform > & Points );

        /** Add display points to this curve. **/
        void AddDisplayPoints( const TArray< FVector > & Points );

        /** Return true if this spline is a valid spline. **/
        bool IsValidCurve() const;

        /** Update point at given index with new information. **/
        void UpdatePoint( int32 PointIndex, const FTransform & Point );

        /** Upload changed control points to HAPI. **/
        void UploadControlPoints();

        /** Remove point at a given index. **/
        void RemovePoint( int32 PointIndex );

        /** Add a point to this curve at given point index. **/
        void AddPoint( int32 PointIndex, const FTransform & Point );

        /** Return true if this is an input curve. **/
        bool IsInputCurve() const;

        /** Returns true if this Spline component is Active **/
        bool IsActive() const;

        /** Assign input parameter to this spline, if it is an input curve. **/
        void SetHoudiniAssetInput( class UHoudiniAssetInput * InHoudiniAssetInput );

        /** Return curve points. **/
        const TArray< FTransform > & GetCurvePoints() const;

        /** Extract Positions from the Transform Array **/
        void GetCurvePositions(TArray<FVector>& Positions) const;

        /** Extract Rotations from the Transform Array **/
        void GetCurveRotations(TArray<FQuat>& Roatations) const;

        /** Extract Scales from the Transform Array **/
        void GetCurveScales(TArray<FVector>& Scales) const;

	/** Updates self and notify parent component **/
	void UpdateHoudiniComponents();

    protected:

        /** Corresponding geo part object. **/
        FHoudiniGeoPartObject HoudiniGeoPartObject;

        /** List of points composing this curve. **/
        TArray< FTransform > CurvePoints;

        /** List of refined points used for drawing. **/
        TArray< FVector > CurveDisplayPoints;

        /** Corresponding asset input parameter if this is an input curve. **/
        class UHoudiniAssetInput * HoudiniAssetInput;

        /** Type of this curve. **/
        EHoudiniSplineComponentType::Enum CurveType;

        /** Method used for this curve. **/
        EHoudiniSplineComponentMethod::Enum CurveMethod;

        /** Whether this spline is closed. **/
        bool bClosedCurve;
};
