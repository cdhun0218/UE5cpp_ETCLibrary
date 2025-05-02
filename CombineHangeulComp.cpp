// Fill out your copyright notice in the Description page of Project Settings.


#include "CombineHangeulComp.h"


const int32 UCombineHangeulComp::BaseCode = 44032;
const int32 UCombineHangeulComp::InitialOffset = 21 * 28;
const int32 UCombineHangeulComp::MedialOffset = 28;

TArray<FString> UCombineHangeulComp::Chosungs =
{
    TEXT("ㄱ"), TEXT("ㄲ"), TEXT("ㄴ"), TEXT("ㄷ"), TEXT("ㄸ"), TEXT("ㄹ"), TEXT("ㅁ"), TEXT("ㅂ"), TEXT("ㅃ"),
    TEXT("ㅅ"), TEXT("ㅆ"), TEXT("ㅇ"), TEXT("ㅈ"), TEXT("ㅉ"), TEXT("ㅊ"), TEXT("ㅋ"), TEXT("ㅌ"), TEXT("ㅍ"),
    TEXT("ㅎ")
};

TArray<FString> UCombineHangeulComp::Jungsungs =
{
    TEXT("ㅏ"), TEXT("ㅐ"), TEXT("ㅑ"), TEXT("ㅒ"), TEXT("ㅓ"), TEXT("ㅔ"), TEXT("ㅕ"), TEXT("ㅖ"), TEXT("ㅗ"),
    TEXT("ㅘ"), TEXT("ㅙ"), TEXT("ㅚ"), TEXT("ㅛ"), TEXT("ㅜ"), TEXT("ㅝ"), TEXT("ㅞ"), TEXT("ㅟ"), TEXT("ㅠ"),
    TEXT("ㅡ"), TEXT("ㅢ"), TEXT("ㅣ")
};

TArray<FString> UCombineHangeulComp::Jongsungs =
{
    TEXT(""), TEXT("ㄱ"), TEXT("ㄲ"), TEXT("ㄳ"), TEXT("ㄴ"), TEXT("ㄵ"), TEXT("ㄶ"), TEXT("ㄷ"), TEXT("ㄹ"), TEXT("ㄺ"),
    TEXT("ㄻ"), TEXT("ㄼ"), TEXT("ㄽ"), TEXT("ㄾ"), TEXT("ㄿ"), TEXT("ㅀ"), TEXT("ㅁ"), TEXT("ㅂ"), TEXT("ㅄ"), TEXT("ㅅ"),
    TEXT("ㅆ"), TEXT("ㅇ"), TEXT("ㅈ"), TEXT("ㅊ"), TEXT("ㅋ"), TEXT("ㅌ"), TEXT("ㅍ"), TEXT("ㅎ")
};

void UCombineHangeulComp::ResetState()
{
    CurrentState = EHangulState::S0;
    CurrentInitial = "";
    CurrentMedial = "";
    CurrentFinal = "";
}

FString UCombineHangeulComp::CombineCurrentHangul() const
{
    if (CurrentInitial.IsEmpty() || CurrentMedial.IsEmpty())
    {
        return CurrentInitial + CurrentMedial + CurrentFinal;
    }

    int32 InitialIndex = Chosungs.IndexOfByKey(CurrentInitial);
    int32 MedialIndex = Jungsungs.IndexOfByKey(CurrentMedial);
    int32 FinalIndex = Jongsungs.IndexOfByKey(CurrentFinal);

    int32 CombinedCode = BaseCode + (InitialIndex * InitialOffset) + (MedialIndex * MedialOffset) + FinalIndex;
    return FString::Chr(CombinedCode);
}

bool UCombineHangeulComp::IsCompoundMedialPossible(const FString& BaseMedial, const FString& NextMedial, FString& CombinedMedial) const
{
    if (BaseMedial == TEXT("ㅗ"))
    {
        if (NextMedial == TEXT("ㅏ")) { CombinedMedial = TEXT("ㅘ"); return true; }
        if (NextMedial == TEXT("ㅐ")) { CombinedMedial = TEXT("ㅙ"); return true; }
        if (NextMedial == TEXT("ㅣ")) { CombinedMedial = TEXT("ㅚ"); return true; }
    }
    if (BaseMedial == TEXT("ㅜ"))
    {
        if (NextMedial == TEXT("ㅓ")) { CombinedMedial = TEXT("ㅝ"); return true; }
        if (NextMedial == TEXT("ㅔ")) { CombinedMedial = TEXT("ㅞ"); return true; }
        if (NextMedial == TEXT("ㅣ")) { CombinedMedial = TEXT("ㅟ"); return true; }
    }
    if (BaseMedial == TEXT("ㅡ"))
    {
        if (NextMedial == TEXT("ㅣ")) { CombinedMedial = TEXT("ㅢ"); return true; }
    }
    return false;
}

bool UCombineHangeulComp::IsCompoundFinalPossible(const FString& BaseFinal, const FString& NextFinal, FString& CombinedFinal) const
{
    if (BaseFinal == TEXT("ㄱ"))
    {
        if (NextFinal == TEXT("ㅅ")) { CombinedFinal = TEXT("ㄳ"); return true; }
    }
    if (BaseFinal == TEXT("ㄴ"))
    {
        if (NextFinal == TEXT("ㅈ")) { CombinedFinal = TEXT("ㄵ"); return true; }
        if (NextFinal == TEXT("ㅎ")) { CombinedFinal = TEXT("ㄶ"); return true; }
    }
    if (BaseFinal == TEXT("ㄹ"))
    {
        if (NextFinal == TEXT("ㄱ")) { CombinedFinal = TEXT("ㄺ"); return true; }
        if (NextFinal == TEXT("ㅁ")) { CombinedFinal = TEXT("ㄻ"); return true; }
        if (NextFinal == TEXT("ㅂ")) { CombinedFinal = TEXT("ㄼ"); return true; }
        if (NextFinal == TEXT("ㅅ")) { CombinedFinal = TEXT("ㄽ"); return true; }
        if (NextFinal == TEXT("ㅌ")) { CombinedFinal = TEXT("ㄾ"); return true; }
        if (NextFinal == TEXT("ㅍ")) { CombinedFinal = TEXT("ㄿ"); return true; }
        if (NextFinal == TEXT("ㅎ")) { CombinedFinal = TEXT("ㅀ"); return true; }
    }
    if (BaseFinal == TEXT("ㅂ"))
    {
        if (NextFinal == TEXT("ㅅ")) { CombinedFinal = TEXT("ㅄ"); return true; }
    }
    return false;
}

void UCombineHangeulComp::HandleStateTransition(FText Input)
{
    FString InputString = Input.ToString();

    switch (CurrentState)
    {
    case EHangulState::S0:
        if (Chosungs.Contains(InputString))
        {
            CurrentInitial = InputString;
            CurrentState = EHangulState::S10;
        }
        else if (Jungsungs.Contains(InputString))
        {
            CurrentMedial = InputString;
            CurrentState = EHangulState::S20;
        }
        break;

    case EHangulState::S10:
        if (Jungsungs.Contains(InputString))
        {
            CurrentMedial = InputString;
            CurrentState = EHangulState::S20;
        }
        else if (Chosungs.Contains(InputString))
        {
            CombinedString += CurrentInitial;
            CurrentInitial = InputString;
        }
        break;

    case EHangulState::S20:
        if (Jongsungs.Contains(InputString))
        {
            CurrentFinal = InputString;
            CurrentState = EHangulState::S30;
        }
        else if (Jungsungs.Contains(InputString))
        {
            FString CombinedMedial;
            if (IsCompoundMedialPossible(CurrentMedial, InputString, CombinedMedial))
            {
                CurrentMedial = CombinedMedial;
                CurrentState = EHangulState::S21;
            }
            else
            {
                CombinedString += CombineCurrentHangul();
                CurrentInitial = "";
                CurrentMedial = InputString;
                CurrentState = EHangulState::S20;
            }
        }
        break;

    case EHangulState::S21:
        if (Jongsungs.Contains(InputString))
        {
            CurrentFinal = InputString;
            CurrentState = EHangulState::S30;
        }
        else if (Jungsungs.Contains(InputString))
        {
            CombinedString += CombineCurrentHangul();
            CurrentInitial = "";
            CurrentMedial = InputString;
            CurrentState = EHangulState::S20;
        }
        break;

    case EHangulState::S30:
        if (Jongsungs.Contains(InputString))
        {
            FString CombinedFinal;
            if (IsCompoundFinalPossible(CurrentFinal, InputString, CombinedFinal))
            {
                ChrBuffer = InputString;   // 겹받침생성시 입력된 자음 백업
                ChrBuffer2 = CurrentFinal; // 겹받침생성시 기존의 자음 백업
                CurrentFinal = CombinedFinal;
                CurrentState = EHangulState::S31;
            }
            else
            {
                CombinedString += CombineCurrentHangul();
                ResetState();
                CurrentInitial = InputString;
                CurrentState = EHangulState::S10;
            }
        }
        else if (Jungsungs.Contains(InputString))
        {
            FString Buffer;
            Buffer = CurrentFinal;
            CurrentFinal = "";
            CombinedString += CombineCurrentHangul();
            CurrentInitial = Buffer;
            CurrentMedial = InputString;
            CurrentFinal = "";
            //CombinedString += CombineCurrentHangul();
            //ResetState();
            CurrentState = EHangulState::S20;
        }
        else if (Chosungs.Contains(InputString))
        {
            CombinedString += CombineCurrentHangul();
            ResetState();
            CurrentInitial = InputString;
            CurrentState = EHangulState::S10;
        }
        break;

    case EHangulState::S31:
        if (Jongsungs.Contains(InputString))
        {
            CombinedString += CombineCurrentHangul();
            ResetState();
            CurrentInitial = InputString;
            CurrentState = EHangulState::S10;
        }
        else if (Jungsungs.Contains(InputString))
        {
            CurrentFinal = ChrBuffer2;      // 백업해뒀던 전입력자음 가져오기
            CombinedString += CombineCurrentHangul();
            CurrentInitial = ChrBuffer;
            CurrentMedial = InputString;
            CurrentFinal = "";
            //CombinedString += CombineCurrentHangul();
            //ResetState();
            CurrentState = EHangulState::S20;
        }
        break;
    }
}

FString UCombineHangeulComp::ProcessHangulInput(FText Input)
{
    HandleStateTransition(Input);
    return CombinedString + CombineCurrentHangul();
}

void UCombineHangeulComp::ResetCombinedString()
{
    ResetState();
    CombinedString = "";
}


// Sets default values for this component's properties
UCombineHangeulComp::UCombineHangeulComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
    ResetState();

	// ...
}


// Called when the game starts
void UCombineHangeulComp::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UCombineHangeulComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

