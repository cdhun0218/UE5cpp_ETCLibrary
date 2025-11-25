// Fill out your copyright notice in the Description page of Project Settings.
// 
// =================================================================================================
// !! 중요 !! - 사전 준비
// =================================================================================================
// 1. FFmpeg 다운로드: https://ffmpeg.org/download.html 에서 Windows 버전을 다운로드하세요.
// 2. FFmpeg 배치: 다운로드한 파일의 bin 폴더에서 ffmpeg.exe를 찾아
//    프로젝트의 [Content/ffmpeg/ffmpeg.exe] 경로에 복사해 넣어야 합니다.
// =================================================================================================

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Delegates/Delegate.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/Event.h"
#include "LIB_Recorder.generated.h"


class UTextureRenderTarget2D;

// 인코딩 완료 시 호출될 델리게이트
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRecordingEncodeComplete, bool, bSuccess);

// -- FFrameWriter 클래스와 관련 구조체를 UCLASS보다 먼저 정의합니다. --

// 작업 스레드에 전달될 데이터 구조
struct FFrameWriteTask
{
    TArray<FColor> PixelData;
    int32 Width;
    int32 Height;
    int32 FrameNumber;
};

// 파일 쓰기 작업을 전담하는 Runnable 클래스
class FFrameWriter : public FRunnable
{
public:
    FFrameWriter();
    virtual ~FFrameWriter();

    // FRunnable interface
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    /**
     * 큐에 작업을 추가합니다.
     * @return 큐가 가득 차서 실패하면 false 반환
     */
    bool EnqueueFrameToWrite(FFrameWriteTask Task);

    /** 현재 대기 중인 작업 수 반환 */
    int32 GetQueueSize() const;

    /** 큐가 처리를 감당할 수 있는지 확인 (Drop 프레임 결정용) */
    bool IsQueueFull() const;

private:
    FThreadSafeBool bIsRunning;
    FString TempImageDirectory;

    // 데이터 경합 방지를 위한 큐
    TQueue<FFrameWriteTask, EQueueMode::Mpsc> WriteQueue;

    // [개선] 큐 크기 추적용 카운터 (TQueue에는 Num()이 없거나 느림)
    FThreadSafeCounter QueueSizeCounter;

    // [개선] CPU 사용량을 줄이고 반응성을 높이기 위한 이벤트 트리거
    FEvent* WorkEvent;

    // [개선] 메모리 폭주 방지를 위한 최대 큐 크기 (예: 60프레임, 약 1~2초 분량 버퍼)
    static const int32 MaxQueueSize = 60;
};


UCLASS()
class TIUM_MEDIA_API ULIB_Recorder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 이미지 시퀀스 녹화를 시작합니다. 전용 파일 쓰기 스레드를 생성하고 실행합니다.
     * @param CaptureFPS 초당 캡처할 프레임 수입니다. (예: 30 또는 60). 0이면 제한 없이 캡처합니다.
     * @return 성공적으로 시작했는지 여부.
     */
    UFUNCTION(BlueprintCallable, Category = "Recording|ThreadSafe")
    static bool StartRecording_ThreadSafe(int32 CaptureFPS = 30);

    /**
     * 캡처할 렌더 타깃을 녹화 큐에 추가하는 요청을 보냅니다.
     */
    UFUNCTION(BlueprintCallable, Category = "Recording|ThreadSafe")
    static void CaptureFrame_ThreadSafe(UTextureRenderTarget2D* TargetRenderTarget);

    /**
     * 지정된 영역을 캡처할 렌더 타깃을 녹화 큐에 추가하는 요청을 보냅니다.
     */
    UFUNCTION(BlueprintCallable, Category = "Recording|ThreadSafe")
    static void CaptureFrame_Cropped_ThreadSafe(UTextureRenderTarget2D* TargetRenderTarget, int32 LeftPixel, int32 TopPixel, int32 CropWidth, int32 CropHeight);

    /**
     * 녹화 스레드를 중지하고, 캡처된 이미지 시퀀스를 사용하여 MP4로 인코딩을 시작합니다.
     * 비동기로 처리되므로 호출 즉시 리턴되며 게임이 멈추지 않습니다.
     * @param FilePath 저장할 MP4 파일의 전체 경로. (비어있으면 자동 생성)
     * @param FrameRate 인코딩할 영상의 프레임레이트. (StartRecording의 CaptureFPS와 맞추는 것이 좋습니다)
     * @param FFMpegParams FFmpeg 인코더 파라미터. (예: -c:v h264_nvenc -pix_fmt yuv420p)
     * @param OnComplete 인코딩이 완료되었을 때 호출될 이벤트.
     */
    UFUNCTION(BlueprintCallable, Category = "Recording|ThreadSafe", meta = (AutoCreateRefTerm = "OnComplete,FFMpegParams"))
    static void StopRecording_AndEncode_ThreadSafe(FString FilePath, int32 FrameRate, FString FFMpegParams, const FOnRecordingEncodeComplete& OnComplete);

    /**
     * 현재 녹화 중인지 여부를 반환합니다.
     */
    UFUNCTION(BlueprintPure, Category = "Recording|ThreadSafe")
    static bool IsRecording_ThreadSafe();

    /**
     * 현재 시스템이 처리 중(녹화 중이거나 인코딩 중)인지 확인합니다.
     */
    UFUNCTION(BlueprintPure, Category = "Recording|ThreadSafe")
    static bool IsProcessing();

private:
    static TSharedPtr<FFrameWriter, ESPMode::ThreadSafe> FrameWriter;
    static FRunnableThread* WriterThread;
    static FThreadSafeCounter FrameCounter;

    // [신규] 녹화부터 인코딩 완료까지 전체 과정을 보호하는 플래그
    static FThreadSafeBool bIsProcessing;

    // [성능 개선] 프레임 제한을 위한 변수들
    static double LastCaptureTime;
    static float MinimumFrameDelay; // 1.0 / CaptureFPS

    // 내부 캡처 로직
    static void CaptureFrame_Internal(UTextureRenderTarget2D* TargetRenderTarget, int32 LeftPixel, int32 TopPixel, int32 CropWidth, int32 CropHeight);
};