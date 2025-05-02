// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"
#include "Delegates/Delegate.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "LIB_Export.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FProcMeshData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LIB_Export")
	TArray<FVector> Vertices;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LIB_Export")
	TArray<int32> Triangles;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LIB_Export")
	TArray<FVector> Normals;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LIB_Export")
	TArray<FVector2D> UV0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LIB_Export")
	TArray<FLinearColor> VertexColors;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LIB_Export")
	TArray<FProcMeshTangent> Tangents;
};

UCLASS()
class SAMSUNGGLASSSIM_5_3_API ULIB_Export : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnStaticMeshConverted, UStaticMesh*, StaticMesh);
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnStaticMeshProgress, float, Progress);
	DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnStaticMeshResult, int32, ErrorCode, UStaticMesh*, StaticMesh);

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnStaticMeshConverted2, UStaticMesh*, StaticMesh);
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnStaticMeshProgress2, float, Progress);
	DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnStaticMeshResult2, int32, ErrorCode, UStaticMesh*, StaticMesh);

	UFUNCTION(BlueprintCallable, Category = "LIB_Export")
	static UStaticMesh* ConvertProcToStaticMesh(FProcMeshData MeshData, const bool RecalculateNormal);

	UFUNCTION(BlueprintCallable, Category = "LIB_Export")
	static bool SaveStaticMeshToStl(UStaticMesh* StaticMesh, const FString& FilePath);

	// Updated async version with progress and result handling
	UFUNCTION(BlueprintCallable, Category = "LIB_Export")
	static void ConvertProcToStaticMeshAsync(
		FProcMeshData MeshData,
		const bool RecalculateNormal,
		FOnStaticMeshProgress OnProgress,
		FOnStaticMeshResult OnResult
	);

	UFUNCTION(BlueprintCallable, Category = "LIB_Export")
	static void TakeScreenShot(const FString& FilePath, const FString& FileName, bool bCaptureUI, bool bAddSuffix, FString& FullFilePath);

	UFUNCTION(BlueprintCallable, Category = "LIB_Export")
	static void ConvetFileToTexture(const FString& FileFullPath, UTexture2D*& OutTexture);
};





