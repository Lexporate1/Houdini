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

#pragma once

/** Unreal Editor headers. **/
#if WITH_EDITOR
#include "UnrealEd.h"
#include "ObjectTools.h"
#include "AssetTypeActions_Base.h"
#include "ComponentAssetBroker.h"
#include "PackageTools.h"
#include "ThumbnailHelpers.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "LevelEditor.h"
#include "IMainFrameModule.h"
#include "ClassIconFinder.h"
#include "SAssetDropTarget.h"
#include "SAssetSearchBox.h"
#include "SColorBlock.h"
#include "SColorPicker.h"
#include "SNumericEntryBox.h"
#include "SRotatorInputBox.h"
#include "SVectorInputBox.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Editor/UnrealEd/Private/GeomFitUtils.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/UnrealEd/Public/BusyCursor.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "RawMesh.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "IPluginManager.h"
#endif

/** Other Unreal headers. **/
#include "CoreUObject.h"
#include "ModuleManager.h"
#include "EngineModule.h"
#include "Engine/TextureDefines.h"
#include "Engine/EngineTypes.h"
#include "MaterialShared.h"
#include "CollisionQueryParams.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "Core.h"
#include "AssetData.h"
#include "AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "HitProxies.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ComponentReregisterContext.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "StaticMeshResources.h"
#include "ISettingsModule.h"
#include "TargetPlatform.h"

/** Houdini Engine headers. **/
#include <vector>
#include <string>
#include <stdint.h>

#include "HAPI.h"
#include "HAPI_Version.h"

/** Whether to enable logging. **/
#define HOUDINI_ENGINE_LOGGING 1

/** Define module names. **/
#define HOUDINI_MODULE_EDITOR "HoudiniEngineEditor"
#define HOUDINI_MODULE_RUNTIME "HoudiniEngine"

/** See whether we are in Editor or Engine module. **/
#ifdef HOUDINI_ENGINE_EDITOR
#define LOCTEXT_NAMESPACE HOUDINI_MODULE_EDITOR
DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngineEditor, Log, All);
#else
#define LOCTEXT_NAMESPACE HOUDINI_MODULE_RUNTIME
DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngine, Log, All);
#endif

/** Definitions coming from UBT. **/
#ifndef HOUDINI_ENGINE_HFS_PATH_DEFINE
#define HOUDINI_ENGINE_HFS_PATH ""
#else
#define HOUDINI_ENGINE_STRINGIFY_HELPER(X) #X
#define HOUDINI_ENGINE_STRINGIFY(X) HOUDINI_ENGINE_STRINGIFY_HELPER(X)
#define HOUDINI_ENGINE_HFS_PATH HOUDINI_ENGINE_STRINGIFY(HOUDINI_ENGINE_HFS_PATH_DEFINE)
#endif

/** Some additional logging macros. **/
#ifdef HOUDINI_ENGINE_LOGGING

#ifdef HOUDINI_ENGINE_EDITOR
#define HOUDINI_LOG_HELPER(VERBOSITY, HOUDINI_LOG_TEXT, ...)							\
	do																					\
	{																					\
		UE_LOG(LogHoudiniEngineEditor, VERBOSITY, HOUDINI_LOG_TEXT, ##__VA_ARGS__);		\
	}																					\
	while(0)
#else
#define HOUDINI_LOG_HELPER(VERBOSITY, HOUDINI_LOG_TEXT, ...)							\
	do																					\
	{																					\
		UE_LOG(LogHoudiniEngine, VERBOSITY, HOUDINI_LOG_TEXT, ##__VA_ARGS__);			\
	}																					\
	while(0)
#endif

#define HOUDINI_LOG_MESSAGE(HOUDINI_LOG_TEXT, ...)										\
	HOUDINI_LOG_HELPER(Log, HOUDINI_LOG_TEXT, ##__VA_ARGS__)

#define HOUDINI_LOG_FATAL(HOUDINI_LOG_TEXT, ...)										\
	HOUDINI_LOG_HELPER(Fatal, HOUDINI_LOG_TEXT, ##__VA_ARGS__)

#define HOUDINI_LOG_ERROR(HOUDINI_LOG_TEXT, ...)										\
	HOUDINI_LOG_HELPER(Error, HOUDINI_LOG_TEXT, ##__VA_ARGS__)

#define HOUDINI_LOG_WARNING(HOUDINI_LOG_TEXT, ...)										\
	HOUDINI_LOG_HELPER(Warning, HOUDINI_LOG_TEXT, ##__VA_ARGS__)

#define HOUDINI_LOG_DISPLAY(HOUDINI_LOG_TEXT, ...)										\
	HOUDINI_LOG_HELPER(Display, HOUDINI_LOG_TEXT, ##__VA_ARGS__)

#else

#define HOUDINI_LOG_MESSAGE(HOUDINI_LOG_TEXT, ...)
#define HOUDINI_LOG_FATAL(HOUDINI_LOG_TEXT, ...)
#define HOUDINI_LOG_ERROR(HOUDINI_LOG_TEXT, ...)
#define HOUDINI_LOG_WARNING(HOUDINI_LOG_TEXT, ...)
#define HOUDINI_LOG_DISPLAY(HOUDINI_LOG_TEXT, ...)

#endif // HOUDINI_ENGINE_LOGGING


/** Error checking - this macro will check the status and return specified parameter. **/
#define HOUDINI_CHECK_ERROR_RETURN_HELPER(HAPI_PARAM_CALL, HAPI_PARAM_RETURN, HAPI_LOG_ROUTINE)						\
	do																												\
	{																												\
		HAPI_Result ResultVariable = HAPI_PARAM_CALL;																\
		if(HAPI_RESULT_SUCCESS != ResultVariable)																	\
		{																											\
			HAPI_LOG_ROUTINE(TEXT("Hapi failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());					\
			return HAPI_PARAM_RETURN;																				\
		}																											\
	}																												\
	while(0)

#define HOUDINI_CHECK_ERROR_RETURN(HAPI_PARAM_CALL, HAPI_PARAM_RETURN)												\
	HOUDINI_CHECK_ERROR_RETURN_HELPER(HAPI_PARAM_CALL, HAPI_PARAM_RETURN, HOUDINI_LOG_ERROR)


/** Error checking - this macro will check the status, execute specified parameter and return. **/
#define HOUDINI_CHECK_ERROR_EXECUTE_RETURN_HELPER(HAPI_PARAM_CALL, HAPI_PARAM_EXECUTE_RETURN, HAPI_LOG_ROUTINE)		\
	do																												\
	{																												\
		HAPI_Result ResultVariable = HAPI_PARAM_CALL;																\
		if(HAPI_RESULT_SUCCESS != ResultVariable)																	\
		{																											\
			HAPI_LOG_ROUTINE(TEXT("Hapi failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());					\
			HAPI_PARAM_EXECUTE_RETURN;																				\
			return;																									\
		}																											\
	}																												\
	while(0)

#define HOUDINI_CHECK_ERROR_EXECUTE_RETURN(HAPI_PARAM_CALL, HAPI_PARAM_EXECUTE_RETURN)								\
	HOUDINI_CHECK_ERROR_EXECUTE_RETURN_HELPER(HAPI_PARAM_CALL, HAPI_PARAM_EXECUTE_RETURN, HOUDINI_LOG_ERROR)


/* Error checking - this macro will check the status. **/
#define HOUDINI_CHECK_ERROR_HELPER(HAPI_PARAM_RESULT, HAPI_PARAM_CALL, HAPI_LOG_ROUTINE)							\
	do																												\
	{																												\
		*HAPI_PARAM_RESULT = HAPI_PARAM_CALL;																		\
		if(HAPI_RESULT_SUCCESS != *HAPI_PARAM_RESULT)																\
		{																											\
			HAPI_LOG_ROUTINE(TEXT("Hapi failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());					\
		}																											\
	}																												\
	while(0)

#define HOUDINI_CHECK_ERROR(HAPI_PARAM_RESULT, HAPI_PARAM_CALL)														\
	HOUDINI_CHECK_ERROR_HELPER(HAPI_PARAM_RESULT, HAPI_PARAM_CALL, HOUDINI_LOG_ERROR)

/** HAPI related attribute definitions. **/

/** Names of attributes used for marshalling Unreal data into Houdini Engine. **/
#define HAPI_UNREAL_ATTRIB_TANGENT						"unreal_tangent"
#define HAPI_UNREAL_ATTRIB_BINORMAL						"unreal_binormal"
#define HAPI_UNREAL_ATTRIB_MATERIAL						"unreal_face_material"
#define HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK			"unreal_face_smoothing_mask"

/** Names of other Houdini Engine attributes and parameters. **/
#define HAPI_UNREAL_ATTRIB_INSTANCE						"instance"
#define HAPI_UNREAL_ATTRIB_INSTANCE_ROTATION			"rot"
#define HAPI_UNREAL_ATTRIB_INSTANCE_SCALE				"scale"
#define HAPI_UNREAL_ATTRIB_INSTANCE_POSITION			"P"
#define HAPI_UNREAL_ATTRIB_POSITION						"P"

#define HAPI_UNREAL_PARAM_CURVE_TYPE					"type"
#define HAPI_UNREAL_PARAM_CURVE_METHOD					"method"
#define HAPI_UNREAL_PARAM_CURVE_COORDS					"coords"
#define HAPI_UNREAL_PARAM_CURVE_CLOSED					"close"

#define HAPI_UNREAL_PARAM_TRANSLATE						"t"
#define HAPI_UNREAL_PARAM_ROTATE						"r"
#define HAPI_UNREAL_PARAM_SCALE							"s"
#define HAPI_UNREAL_PARAM_PIVOT							"p"
#define HAPI_UNREAL_PARAM_UNIFORMSCALE					"scale"

#define HAPI_UNREAL_MATERIAL_TEXTURE_MAIN				"C A"
#define HAPI_UNREAL_MATERIAL_TEXTURE_NORMAL				"N"

#define HAPI_UNREAL_PARAM_MAP_DIFFUSE_0					"ogl_tex1"
#define HAPI_UNREAL_PARAM_MAP_DIFFUSE_1					"baseColorMap"
#define HAPI_UNREAL_PARAM_MAP_DIFFUSE_2					"map"

#define HAPI_UNREAL_PARAM_DIFFUSE_COLOR					"ogl_diff"

/** Default values for new curves. **/
#define HAPI_UNREAL_PARAM_INPUT_CURVE_COORDS_DEFAULT	"0.0, 0.0, 3.0 3.0, 0.0, 3.0"

/** Default values for certain UI min and max parameter values **/
#define HAPI_UNREAL_PARAM_INT_UI_MIN					0
#define HAPI_UNREAL_PARAM_INT_UI_MAX					10
#define HAPI_UNREAL_PARAM_FLOAT_UI_MIN					0.0f
#define HAPI_UNREAL_PARAM_FLOAT_UI_MAX					10.0f

/** Suffix for all Unreal materials which are generated from Houdini. **/
#define HAPI_UNREAL_GENERATED_MATERIAL_SUFFIX			TEXT("_houdini_material")

/** Group name prefix used for collision geometry generation. **/
#define HAPI_UNREAL_GROUP_GEOMETRY_COLLISION			"collision_geo"

/** Group name prefix used for rendered collision geometry generation. **/
#define HAPI_UNREAL_GROUP_GEOMETRY_RENDERED_COLLISION	"rendered_collision_geo"

/** Group name used to mark everything that is not a member of collision or rendered collision group. **/
#define HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION		"main_geo"

/** Details panel desired sizes. **/
#define HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH 				270
#define HAPI_UNREAL_DESIRED_ROW_FULL_WIDGET_WIDTH				310
#define HAPI_UNREAL_DESIRED_SETTINGS_ROW_VALUE_WIDGET_WIDTH		350
#define HAPI_UNREAL_DESIRED_SETTINGS_ROW_FULL_WIDGET_WIDTH		400

/** Various variable names used to store meta information in generated packages. **/
#define HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT			TEXT("HoudiniGeneratedObject")
#define HAPI_UNREAL_PACKAGE_META_GENERATED_NAME				TEXT("HoudiniGeneratedName")
#define HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE		TEXT("HoudiniGeneratedTextureType")
#define HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL	TEXT("N")
#define HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE	TEXT("C_A")

/** Various session related settings. **/
#define HAPI_UNREAL_SESSION_SERVER_HOST						TEXT("localhost")
#define HAPI_UNREAL_SESSION_SERVER_PORT						9090
#define HAPI_UNREAL_SESSION_SERVER_PIPENAME					TEXT("hapi")
#define HAPI_UNREAL_SESSION_SERVER_AUTOSTART				false
#define HAPI_UNREAL_SESSION_SERVER_TIMEOUT					3000.0f

/** Default material name. **/
#define HAPI_UNREAL_DEFAULT_MATERIAL_NAME					TEXT("default_material")

#define HAPI_UNREAL_ENABLE_LOADER

/** Names of HAPI libraries on different platforms. **/
#ifdef HAPI_UNREAL_ENABLE_LOADER
#define HAPI_LIB_OBJECT_WINDOWS			TEXT("libHAPIL.dll")
#define HAPI_LIB_OBJECT_MAC				TEXT("libHAPIL.dylib")
#define HAPI_LIB_OBJECT_LINUX			TEXT("libHAPIL.so")
#else
#define HAPI_LIB_OBJECT_WINDOWS			TEXT("libHAPI.dll")
#define HAPI_LIB_OBJECT_MAC				TEXT("libHAPI.dylib")
#define HAPI_LIB_OBJECT_LINUX			TEXT("libHAPI.so")
#endif

/** HFS subfolder containing HAPI lib. **/
#define HAPI_HFS_SUBFOLDER_WINDOWS		TEXT("bin")
#define HAPI_HFS_SUBFOLDER_MAC			TEXT("dsolib")
#define HAPI_HFS_SUBFOLDER_LINUX		TEXT("dsolib")

/** Helper function to serialize enumerations. **/
template <typename TEnum>
FORCEINLINE
FArchive&
SerializeEnumeration(FArchive& Ar, TEnum& E)
{
	uint8 B = (uint8) E;
	Ar << B;

	if(Ar.IsLoading())
	{
		E = (TEnum) B;
	}

	return Ar;
}

/** Struct to enable global silent flag - this will force dialogs to not show up. **/
struct FHoudiniScopedGlobalSilence
{
	FHoudiniScopedGlobalSilence()
	{
		bGlobalSilent = GIsSilent;
		GIsSilent = true;
	}

	~FHoudiniScopedGlobalSilence()
	{
		GIsSilent = bGlobalSilent;
	}

	bool bGlobalSilent;
};


/** Struct to disable transactional buffer serialization. This is used to avoid including undo reference count. **/
struct FHoudiniScopedGlobalTransactionDisable
{
	FHoudiniScopedGlobalTransactionDisable()
	{
#if WITH_EDITOR
		GEditor->Trans->DisableObjectSerialization();
#endif
	}

	~FHoudiniScopedGlobalTransactionDisable()
	{
#if WITH_EDITOR
		GEditor->Trans->EnableObjectSerialization();
#endif
	}
};


