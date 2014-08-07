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

#include "HAPI.h"

class FArchive;
class FReferenceCollector;
class FHoudiniMeshVertexBuffer;
class FHoudiniMeshVertexFactory;
class FHoudiniAssetObjectGeoPart;
class UHoudiniAssetMaterial;

class FHoudiniAssetObjectGeo
{
	friend class FHoudiniEngine;
	friend struct FHoudiniEngineUtils;
	friend class UHoudiniAssetComponent;
	friend class FHoudiniMeshSceneProxy;

public:

	/** Constructor. **/
	FHoudiniAssetObjectGeo();
	FHoudiniAssetObjectGeo(const FMatrix& InTransform, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);

	/** Destructor. **/
	virtual ~FHoudiniAssetObjectGeo();

public:

	/** Add a part to this asset geo. **/
	void AddGeoPart(FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart);

	/** Reference counting propagation. **/
	virtual void AddReferencedObjects(FReferenceCollector& Collector);

	/** Serialization. **/
	virtual void Serialize(FArchive& Ar);

	/** Retrieve list of vertices. **/
	TArray<FDynamicMeshVertex>& GetVertices();

	/** Add vertices of given triangle to list of vertices. **/
	void AddTriangleVertices(FHoudiniMeshTriangle& Triangle);

	/** Create rendering resources for this geo. **/
	void CreateRenderingResources();

	/** Release rendering resources used by this geo. **/
	void ReleaseRenderingResources();

	/** Return transform of this geo. **/
	const FMatrix& GetTransform() const;

	/** Compute whether this geo uses multiple materials. **/
	void ComputeMultipleMaterialUsage();

	/** Returns true if this geo uses multiple materials, false otherwise. **/
	bool UsesMultipleMaterials() const;

	/** Collect textures used by parts. **/
	void CollectTextures(TArray<UTexture2D*>& Textures);

	/** Retrieve single material. **/
	UHoudiniAssetMaterial* GetSingleMaterial() const;

	/** Replace material on al parts with given material. **/
	void ReplaceMaterial(UHoudiniAssetMaterial* Material);

	/** Return true if this geometry is Houdini logo geometry. **/
	bool IsHoudiniLogo() const;

protected:

	/** Set this geometry as Houdini logo geometry. **/
	void SetHoudiniLogo();

protected:

	/** List of geo parts (these correspond to submeshes). Will always have at least one. **/
	TArray<FHoudiniAssetObjectGeoPart*> HoudiniAssetObjectGeoParts;

	/** Vertices used by this geo. **/
	TArray<FDynamicMeshVertex> Vertices;

	/** Transform for this part. **/
	FMatrix Transform;

	/** Corresponding Vertex buffer used by proxy object. Owned by render thread. Kept here for indexing. **/
	FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer;

	/** Corresponding Vertex factory used by proxy object. Owned by render thread. Kept here for indexing. **/
	FHoudiniMeshVertexFactory* HoudiniMeshVertexFactory;

	/** HAPI Object Id for this geometry. **/
	HAPI_ObjectId ObjectId;

	/** HAPI Geo Id for this geometry. **/
	HAPI_GeoId GeoId;
	
	/** HAPI Part Id for this geometry. **/
	HAPI_PartId PartId;

	/** Is set to true when submeshes use different materials. **/
	bool bMultipleMaterials;
	
	/** Is set to true when this geometry is a Houdini logo geometry. **/
	bool bHoudiniLogo;
};
