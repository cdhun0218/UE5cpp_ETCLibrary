#pragma once

#include "LIB_Export.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "StaticMeshResources.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include <fstream>
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "UObject/UObjectGlobals.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

#include "IImageWrapperModule.h"


UStaticMesh* ULIB_Export::ConvertProcToStaticMesh(FProcMeshData MeshData, const bool RecalculateNormal)
{
    // 1. 유효성 검사 추가
    if (MeshData.Vertices.Num() == 0 || MeshData.Triangles.Num() % 3 != 0)
        return nullptr;

    // UV0가 없는 경우 UV를 자동 생성
    if (MeshData.UV0.Num() == 0)
    {
        MeshData.UV0.SetNum(MeshData.Vertices.Num());
        for (int32 i = 0; i < MeshData.Vertices.Num(); i++)
        {
            const FVector& Vert = MeshData.Vertices[i];
            MeshData.UV0[i] = FVector2D(Vert.X, Vert.Y); // 간단한 평면 투영
        }
    }

    // RecalculateNormal 이 true일때  Normals를 자동 생성
    if (RecalculateNormal)
    {
        MeshData.Normals.SetNum(MeshData.Vertices.Num());
        for (int32 i = 0; i < MeshData.Triangles.Num(); i += 3)
        {
            const int32 Index0 = MeshData.Triangles[i];
            const int32 Index1 = MeshData.Triangles[i + 1];
            const int32 Index2 = MeshData.Triangles[i + 2];

            if (MeshData.Vertices.IsValidIndex(Index0) && MeshData.Vertices.IsValidIndex(Index1) && MeshData.Vertices.IsValidIndex(Index2))
            {
                const FVector Edge1 = MeshData.Vertices[Index1] - MeshData.Vertices[Index0];
                const FVector Edge2 = MeshData.Vertices[Index2] - MeshData.Vertices[Index0];
                const FVector Normal = FVector::CrossProduct(Edge2, Edge1).GetSafeNormal();

                MeshData.Normals[Index0] = Normal;
                MeshData.Normals[Index1] = Normal;
                MeshData.Normals[Index2] = Normal;
            }
        }
    }

    // 2. 스태틱 메시 생성
    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);

    // 중요: 렌더 리소스를 올바르게 초기화
    StaticMesh->ReleaseResources();  // 기존 리소스 제거
    StaticMesh->InitResources();     // 새 리소스 초기화

    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attributes(MeshDesc);
    Attributes.Register();
    
    // 3. 정점 생성
    TArray<FVertexID> VertexIDs;
    for (const FVector& Vert : MeshData.Vertices)
    {
        FVertexID Vid = MeshDesc.CreateVertex();
        Attributes.GetVertexPositions()[Vid] = FVector3f(Vert);
        VertexIDs.Add(Vid);
    }

    // 4. 폴리곤 그룹 및 UV 채널 설정
    FPolygonGroupID Pgid = MeshDesc.CreatePolygonGroup();
    const int32 NumUvChannels = 1;
    Attributes.GetVertexInstanceUVs().SetNumChannels(NumUvChannels);

    // 5. 삼각형 및 정점 인스턴스 생성
    for (int32 i = 0; i < MeshData.Triangles.Num(); i += 3)
    {
        // 삼각형을 구성하는 세 개의 인덱스를 먼저 추출
        const int32 Index0 = MeshData.Triangles[i];
        const int32 Index1 = MeshData.Triangles[i + 1];
        const int32 Index2 = MeshData.Triangles[i + 2];

        // 모든 정점 인덱스가 유효한지 확인
        if (!VertexIDs.IsValidIndex(Index0) || !VertexIDs.IsValidIndex(Index1) || !VertexIDs.IsValidIndex(Index2))
        {
            // 유효하지 않은 인덱스가 있으면 해당 삼각형 전체를 건너뛰기
            continue;
        }

        // 유효한 경우, 정점 인스턴스를 생성하여 저장
        TArray<FVertexInstanceID> VertexInstances;
        const int32 Indices[3] = { Index0, Index1, Index2 };


        // 각 정점 인스턴스에 속성 할당
        for (int32 j = 0; j < 3; j++)
        {
            const int32 VertIndex = Indices[j];
            //if (!VertexIDs.IsValidIndex(VertIndex))
            //{
            //    // Invalid vertex index, skip this triangle
            //    continue;
            //}

            FVertexInstanceID InstanceID = MeshDesc.CreateVertexInstance(VertexIDs[VertIndex]);

            // 법선 설정
            if (MeshData.Normals.IsValidIndex(VertIndex))
                Attributes.GetVertexInstanceNormals()[InstanceID] = FVector3f(MeshData.Normals[VertIndex]);

            // UV 설정 (채널 0)
            if (MeshData.UV0.IsValidIndex(VertIndex))
                Attributes.GetVertexInstanceUVs().Set(InstanceID, 0, FVector2f(MeshData.UV0[VertIndex]));

            // 버텍스 컬러 설정
            if (MeshData.VertexColors.IsValidIndex(VertIndex))
                Attributes.GetVertexInstanceColors()[InstanceID] = FLinearColor(MeshData.VertexColors[VertIndex]);

            // 탄젠트 설정
            if (MeshData.Tangents.IsValidIndex(VertIndex))
            {
                const FProcMeshTangent& Tangent = MeshData.Tangents[VertIndex];
                Attributes.GetVertexInstanceTangents()[InstanceID] = FVector3f(Tangent.TangentX);
                Attributes.GetVertexInstanceBinormalSigns()[InstanceID] = Tangent.bFlipTangentY ? -1.0f : 1.0f;
            }
            VertexInstances.Add(InstanceID);
        }

        // 삼각형 생성
        if (VertexInstances.Num() == 3)
        {
            MeshDesc.CreateTriangle(Pgid, VertexInstances);
        }
    }

    // 6. 메시 빌드 설정
    StaticMesh->CreateBodySetup();
    
    UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
    BuildParams.bBuildSimpleCollision = false;
    BuildParams.bFastBuild = true;
    StaticMesh->bAllowCPUAccess = true;
    StaticMesh->BuildFromMeshDescriptions({ &MeshDesc }, BuildParams);
    StaticMesh->InitResources();
   
    return StaticMesh;
}

/////////////////////////////////////////////////////////////////////////////

void ULIB_Export::ConvertProcToStaticMeshAsync(
    FProcMeshData MeshData,
    const bool RecalculateNormal,
    FOnStaticMeshProgress OnProgress,
    FOnStaticMeshResult OnResult
)
{
    // 공유 리소스 접근이나 GC 충돌 방지를 위해 WeakObjectPtr과 StaticMesh outer 설정 변경
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [MeshData = MoveTemp(MeshData), RecalculateNormal, OnProgress, OnResult]() mutable
        {
            float Progress = 0.0f;
            int32 ErrorCode = 0;
            UStaticMesh* Result = nullptr;

            const int32 TotalSteps = 4;
            int32 CurrentStep = 0;

            // Step 1: Validate input
            if (MeshData.Vertices.Num() == 0 || MeshData.Triangles.Num() % 3 != 0)
            {
                ErrorCode = -1;
                AsyncTask(ENamedThreads::GameThread, [OnResult, ErrorCode]()
                    {
                        OnResult.ExecuteIfBound(ErrorCode, nullptr);
                    });
                return;
            }
            ++CurrentStep;
            Progress = static_cast<float>(CurrentStep) / TotalSteps;
            AsyncTask(ENamedThreads::GameThread, [OnProgress, Progress]()
                {
                    OnProgress.ExecuteIfBound(Progress);
                });

            // Step 2: Generate UVs if missing
            if (MeshData.UV0.Num() == 0)
            {
                MeshData.UV0.SetNum(MeshData.Vertices.Num());
                for (int32 i = 0; i < MeshData.Vertices.Num(); ++i)
                {
                    const FVector& Vert = MeshData.Vertices[i];
                    MeshData.UV0[i] = FVector2D(Vert.X, Vert.Y);
                }
            }
            ++CurrentStep;
            Progress = static_cast<float>(CurrentStep) / TotalSteps;
            AsyncTask(ENamedThreads::GameThread, [OnProgress, Progress]()
                {
                    OnProgress.ExecuteIfBound(Progress);
                });

            // Step 3: Recalculate normals if requested
            if (RecalculateNormal)
            {
                MeshData.Normals.SetNumZeroed(MeshData.Vertices.Num());
                for (int32 i = 0; i < MeshData.Triangles.Num(); i += 3)
                {
                    const int32 Index0 = MeshData.Triangles[i];
                    const int32 Index1 = MeshData.Triangles[i + 1];
                    const int32 Index2 = MeshData.Triangles[i + 2];

                    if (MeshData.Vertices.IsValidIndex(Index0) && MeshData.Vertices.IsValidIndex(Index1) && MeshData.Vertices.IsValidIndex(Index2))
                    {
                        const FVector Edge1 = MeshData.Vertices[Index1] - MeshData.Vertices[Index0];
                        const FVector Edge2 = MeshData.Vertices[Index2] - MeshData.Vertices[Index0];
                        const FVector Normal = FVector::CrossProduct(Edge2, Edge1).GetSafeNormal();

                        MeshData.Normals[Index0] = Normal;
                        MeshData.Normals[Index1] = Normal;
                        MeshData.Normals[Index2] = Normal;
                    }
                }
            }
            ++CurrentStep;
            Progress = static_cast<float>(CurrentStep) / TotalSteps;
            AsyncTask(ENamedThreads::GameThread, [OnProgress, Progress]()
                {
                    OnProgress.ExecuteIfBound(Progress);
                });

            // Step 4: Safe StaticMesh Creation
            AsyncTask(ENamedThreads::GameThread, [MeshData = MoveTemp(MeshData), RecalculateNormal, OnResult, OnProgress]()
                {
                    UStaticMesh* StaticMesh = ULIB_Export::ConvertProcToStaticMesh(MeshData, RecalculateNormal);
                    const int32 ErrorCode = (StaticMesh != nullptr) ? 0 : -1;
                    OnProgress.ExecuteIfBound(1.0f);
                    OnResult.ExecuteIfBound(ErrorCode, StaticMesh);
                });
        });
}

void ULIB_Export::TakeScreenShot(const FString& FilePath, const FString& FileName, bool bCaptureUI, bool bAddSuffix, FString& FullFilePath)
{
    // 현재 날짜와 시간을 파일 이름에 추가
    FString CurrentDateTime = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"));
    FullFilePath = FilePath + "/" + CurrentDateTime + FileName;

    // 스크린샷 요청
    FScreenshotRequest::RequestScreenshot(FullFilePath, bCaptureUI, false);

    // 렌더링 명령 대기
    FlushRenderingCommands();
}

void ULIB_Export::ConvetFileToTexture(const FString& FullFilePath, UTexture2D*& OutTexture)
{ // 파일이 저장되었는지 확인
    if (!FPaths::FileExists(FullFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Screenshot file was not saved: %s"), *FullFilePath);
        OutTexture = nullptr;
        return;
    }

    // 파일 데이터를 읽어와 텍스처로 변환
    TArray<uint8> FileData;
    if (FFileHelper::LoadFileToArray(FileData, *FullFilePath))
    {
        // 이미지 데이터를 UTexture2D로 변환
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

        if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
        {
            TArray<uint8> RawData;
            if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
            {
                // 텍스처 생성
                OutTexture = UTexture2D::CreateTransient(
                    ImageWrapper->GetWidth(),
                    ImageWrapper->GetHeight(),
                    PF_B8G8R8A8
                );

                if (OutTexture)
                {
                    // 텍스처 데이터 설정
                    void* TextureData = OutTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
                    FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
                    OutTexture->GetPlatformData()->Mips[0].BulkData.Unlock();

                    // 텍스처 리소스 업데이트
                    OutTexture->UpdateResource();
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to extract raw data from image."));
                OutTexture = nullptr;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to initialize ImageWrapper or set compressed data."));
            OutTexture = nullptr;
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load file data from %s"), *FullFilePath);
        OutTexture = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////////////

bool ULIB_Export::SaveStaticMeshToStl(UStaticMesh* StaticMesh, const FString& FilePath)
{
    
    return true;
}
