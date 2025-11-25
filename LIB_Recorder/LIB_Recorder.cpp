// Fill out your copyright notice in the Description page of Project Settings.


#include "LIB_Recorder.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h" 
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "ImageUtils.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "RHICommandList.h"
#include "RHI.h"

// -- Static variables initialization --
TSharedPtr<FFrameWriter, ESPMode::ThreadSafe> ULIB_Recorder::FrameWriter = nullptr;
FRunnableThread* ULIB_Recorder::WriterThread = nullptr;
FThreadSafeCounter ULIB_Recorder::FrameCounter;
FThreadSafeBool ULIB_Recorder::bIsProcessing = false; // [신규] 초기화

// [성능 개선] 정적 변수 초기화
double ULIB_Recorder::LastCaptureTime = 0.0;
float ULIB_Recorder::MinimumFrameDelay = 0.0f;


// -- FFrameWriter implementation --
FFrameWriter::FFrameWriter()
    : bIsRunning(false)
    , WorkEvent(nullptr)
{
    TempImageDirectory = FPaths::ProjectSavedDir() / TEXT("TempRecording");
    // [개선] 이벤트 생성
    WorkEvent = FPlatformProcess::GetSynchEventFromPool(false);
}

FFrameWriter::~FFrameWriter()
{
    if (WorkEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(WorkEvent);
        WorkEvent = nullptr;
    }
}

bool FFrameWriter::Init()
{
    bIsRunning = true;
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (PlatformFile.DirectoryExists(*TempImageDirectory))
    {
        PlatformFile.DeleteDirectoryRecursively(*TempImageDirectory);
    }
    return PlatformFile.CreateDirectory(*TempImageDirectory);
}

uint32 FFrameWriter::Run()
{
    while (bIsRunning)
    {
        // 큐에 작업이 있는지 확인
        if (!WriteQueue.IsEmpty())
        {
            FFrameWriteTask Task;
            if (WriteQueue.Dequeue(Task))
            {
                // 작업 처리 시 카운터 감소
                QueueSizeCounter.Decrement();

                FString FrameFilename = FString::Printf(TEXT("Frame_%05d.bmp"), Task.FrameNumber);
                FString FullPath = TempImageDirectory / FrameFilename;
                FFileHelper::CreateBitmap(*FullPath, Task.Width, Task.Height, Task.PixelData.GetData());
            }
        }
        else
        {
            // 큐가 비었으면 이벤트 대기 (최대 1초 대기 후 다시 루프 체크)
            if (WorkEvent)
            {
                WorkEvent->Wait(1000);
            }
            else
            {
                FPlatformProcess::Sleep(0.01f);
            }
        }
    }

    // [개선] Stop() 호출 후 남은 큐 처리 (잔여 프레임 저장)
    while (!WriteQueue.IsEmpty())
    {
        FFrameWriteTask Task;
        if (WriteQueue.Dequeue(Task))
        {
            QueueSizeCounter.Decrement();
            FString FrameFilename = FString::Printf(TEXT("Frame_%05d.bmp"), Task.FrameNumber);
            FString FullPath = TempImageDirectory / FrameFilename;
            FFileHelper::CreateBitmap(*FullPath, Task.Width, Task.Height, Task.PixelData.GetData());
        }
    }

    return 0;
}

void FFrameWriter::Stop()
{
    bIsRunning = false;
    // 스레드가 Wait 상태일 수 있으므로 즉시 깨움
    if (WorkEvent)
    {
        WorkEvent->Trigger();
    }
}

void FFrameWriter::Exit()
{
}

bool FFrameWriter::EnqueueFrameToWrite(FFrameWriteTask Task)
{
    // [개선] 큐 크기 제한 체크
    if (QueueSizeCounter.GetValue() >= MaxQueueSize)
    {
        // 큐가 가득 찼으면 프레임 드랍 (메모리 보호)
        return false;
    }

    WriteQueue.Enqueue(Task);
    QueueSizeCounter.Increment();

    if (WorkEvent)
    {
        WorkEvent->Trigger();
    }
    return true;
}

int32 FFrameWriter::GetQueueSize() const
{
    return QueueSizeCounter.GetValue();
}

bool FFrameWriter::IsQueueFull() const
{
    return QueueSizeCounter.GetValue() >= MaxQueueSize;
}


// -- BPL implementation --
bool ULIB_Recorder::StartRecording_ThreadSafe(int32 CaptureFPS)
{
    // [신규] 처리 중(인코딩 포함)이면 시작 불가
    if (FrameWriter.IsValid() || bIsProcessing)
    {
        UE_LOG(LogTemp, Warning, TEXT("Already recording or processing."));
        return false;
    }

    bIsProcessing = true; // 처리 시작 표시
    FrameCounter.Reset();
    LastCaptureTime = 0.0;

    if (CaptureFPS > 0)
    {
        MinimumFrameDelay = 1.0f / (float)CaptureFPS;
    }
    else
    {
        MinimumFrameDelay = 0.0f;
    }

    FrameWriter = MakeShared<FFrameWriter, ESPMode::ThreadSafe>();
    WriterThread = FRunnableThread::Create(FrameWriter.Get(), TEXT("RenderTargetWriterThread"), 0, TPri_BelowNormal);

    if (WriterThread)
    {
        UE_LOG(LogTemp, Log, TEXT("Started thread-safe recording at %d FPS target."), CaptureFPS);
        return true;
    }

    UE_LOG(LogTemp, Error, TEXT("Failed to create writer thread."));
    FrameWriter.Reset();
    bIsProcessing = false; // 실패 시 플래그 해제
    return false;
}

void ULIB_Recorder::CaptureFrame_ThreadSafe(UTextureRenderTarget2D* TargetRenderTarget)
{
    CaptureFrame_Cropped_ThreadSafe(TargetRenderTarget, 0, 0, 0, 0);
}

void ULIB_Recorder::CaptureFrame_Cropped_ThreadSafe(UTextureRenderTarget2D* TargetRenderTarget, int32 LeftPixel, int32 TopPixel, int32 CropWidth, int32 CropHeight)
{
    // [개선] 큐 확인 및 FPS 제한
    if (!FrameWriter.IsValid() || FrameWriter->IsQueueFull())
    {
        return;
    }

    double CurrentTime = FPlatformTime::Seconds();
    if ((CurrentTime - LastCaptureTime) < MinimumFrameDelay)
    {
        return;
    }

    LastCaptureTime = CurrentTime;

    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [TargetRenderTarget, LeftPixel, TopPixel, CropWidth, CropHeight]()
            {
                CaptureFrame_Internal(TargetRenderTarget, LeftPixel, TopPixel, CropWidth, CropHeight);
            });
    }
    else
    {
        CaptureFrame_Internal(TargetRenderTarget, LeftPixel, TopPixel, CropWidth, CropHeight);
    }
}

void ULIB_Recorder::CaptureFrame_Internal(UTextureRenderTarget2D* TargetRenderTarget, int32 LeftPixel, int32 TopPixel, int32 CropWidth, int32 CropHeight)
{
    if (!FrameWriter.IsValid() || !IsValid(TargetRenderTarget)) return;

    if (FrameWriter->IsQueueFull()) return;

    if (CropWidth <= 0 || CropHeight <= 0)
    {
        LeftPixel = 0;
        TopPixel = 0;
        CropWidth = TargetRenderTarget->SizeX;
        CropHeight = TargetRenderTarget->SizeY;
    }

    if ((LeftPixel + CropWidth) > TargetRenderTarget->SizeX ||
        (TopPixel + CropHeight) > TargetRenderTarget->SizeY)
    {
        return;
    }

    FTextureRenderTargetResource* RTResource = TargetRenderTarget->GameThread_GetRenderTargetResource();
    TSharedPtr<FFrameWriter, ESPMode::ThreadSafe> CurrentFrameWriter = FrameWriter;
    const int32 CurrentFrameNumber = FrameCounter.Increment();
    FIntRect CropRect(LeftPixel, TopPixel, LeftPixel + CropWidth, TopPixel + CropHeight);

    ENQUEUE_RENDER_COMMAND(ReadRenderTargetCommand)(
        [RTResource, CurrentFrameWriter, CurrentFrameNumber, CropRect](FRHICommandListImmediate& RHICmdList)
        {
            if (!CurrentFrameWriter.IsValid()) return;

            FTextureRHIRef SrcTexture = RTResource->GetTextureRHI();
            if (!SrcTexture) return;

            TArray<FColor> RawPixels;
            FReadSurfaceDataFlags ReadFlags;
            ReadFlags.SetLinearToGamma(true);

            RHICmdList.ReadSurfaceData(SrcTexture, CropRect, RawPixels, ReadFlags);

            if (RawPixels.Num() > 0)
            {
                FFrameWriteTask Task;
                Task.PixelData = MoveTemp(RawPixels);
                Task.Width = CropRect.Width();
                Task.Height = CropRect.Height();
                Task.FrameNumber = CurrentFrameNumber;
                CurrentFrameWriter->EnqueueFrameToWrite(MoveTemp(Task));
            }
        });
}


void ULIB_Recorder::StopRecording_AndEncode_ThreadSafe(FString FilePath, int32 FrameRate, FString FFMpegParams, const FOnRecordingEncodeComplete& OnComplete)
{
    // 녹화 중이 아니면 리턴
    if (!FrameWriter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Not currently recording."));
        OnComplete.ExecuteIfBound(false);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Stopping recording... Handing over to background thread."));

    // [핵심 변경] 비동기 처리를 위해 로컬 변수에 스레드 포인터 복사
    TSharedPtr<FFrameWriter, ESPMode::ThreadSafe> WriterToStop = FrameWriter;
    FRunnableThread* ThreadToWaitFor = WriterThread;

    // 즉시 전역 변수를 초기화하여 추가 캡처 방지 (bIsProcessing은 유지)
    FrameWriter.Reset();
    WriterThread = nullptr;

    // 파일 경로 및 인자 준비
    const FString TempImageDirectory = FPaths::ProjectSavedDir() / TEXT("TempRecording");
    const FString FFmpegPath = FPaths::ProjectContentDir() / TEXT("ffmpeg/ffmpeg.exe");

    if (FilePath.IsEmpty())
    {
        FilePath = FPaths::ProjectSavedDir() / TEXT("VideoCaptures") / FString::Printf(TEXT("Capture_%s.mp4"), *FDateTime::Now().ToString());
    }

    if (!FPaths::FileExists(FFmpegPath))
    {
        UE_LOG(LogTemp, Error, TEXT("ffmpeg.exe not found at %s"), *FFmpegPath);
        bIsProcessing = false;
        OnComplete.ExecuteIfBound(false);
        return;
    }

    // [핵심 변경] 대기 및 인코딩 로직을 완전히 백그라운드로 이동
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WriterToStop, ThreadToWaitFor, FFmpegPath, FrameRate, TempImageDirectory, FilePath, FFMpegParams, OnComplete]()
        {
            // 1. 스레드 종료 및 대기 (Game Thread가 아닌 여기서 대기하므로 프리징 없음)
            if (WriterToStop.IsValid())
            {
                WriterToStop->Stop();
            }

            if (ThreadToWaitFor)
            {
                ThreadToWaitFor->WaitForCompletion(); // 잔여 파일 쓰기 대기
                delete ThreadToWaitFor;
            }

            // 2. FFmpeg 인코딩 수행
            FString UserParams = FFMpegParams;
            if (UserParams.IsEmpty())
            {
                UserParams = TEXT("-c:v h264_nvenc -pix_fmt yuv420p");
            }

            const FString InputPattern = TempImageDirectory / TEXT("Frame_%05d.bmp");

            const FString Params = FString::Printf(
                TEXT("-loglevel error -framerate %d -i \"%s\" %s -y \"%s\""),
                FrameRate, *InputPattern, *UserParams, *FilePath
            );

            FProcHandle ProcHandle = FPlatformProcess::CreateProc(*FFmpegPath, *Params, false, true, true, nullptr, 0, nullptr, nullptr, nullptr);
            bool bSuccess = false;

            if (ProcHandle.IsValid())
            {
                FPlatformProcess::WaitForProc(ProcHandle);
                int32 ReturnCode;
                FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
                bSuccess = (ReturnCode == 0);
                FPlatformProcess::CloseProc(ProcHandle);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to launch FFmpeg process."));
            }

            // 3. 임시 파일 삭제
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
            PlatformFile.DeleteDirectoryRecursively(*TempImageDirectory);

            // 4. 완료 알림 (Game Thread로 복귀)
            AsyncTask(ENamedThreads::GameThread, [OnComplete, bSuccess, FilePath]()
                {
                    // 모든 처리가 끝났으므로 플래그 해제
                    bIsProcessing = false;

                    if (bSuccess)
                    {
                        UE_LOG(LogTemp, Log, TEXT("MP4 encoding successful: %s"), *FilePath);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("MP4 encoding failed."));
                    }
                    OnComplete.ExecuteIfBound(bSuccess);
                });
        });
}

bool ULIB_Recorder::IsRecording_ThreadSafe()
{
    return FrameWriter.IsValid();
}

bool ULIB_Recorder::IsProcessing()
{
    return bIsProcessing;
}
