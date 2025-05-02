// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombineHangeulComp.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable )
class HANGEULKEYBOARD_API UCombineHangeulComp : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCombineHangeulComp();

    UFUNCTION(BlueprintCallable, Category = "Hangeul")
    FString ProcessHangulInput(FText Input);

    UFUNCTION(BlueprintCallable, Category = "Hangeul")
    void ResetCombinedString();

private:
    void ResetState();
    FString CombineCurrentHangul() const;
    bool IsCompoundMedialPossible(const FString& BaseMedial, const FString& NextMedial, FString& CombinedMedial) const;
    bool IsCompoundFinalPossible(const FString& BaseFinal, const FString& NextFinal, FString& CombinedFinal) const;
    void HandleStateTransition(FText Input);

    static const int32 BaseCode; // 0xAC00
    static const int32 InitialOffset;
    static const int32 MedialOffset;
    static TArray<FString> Chosungs;
    static TArray<FString> Jungsungs;
    static TArray<FString> Jongsungs;

    enum class EHangulState
    {
        S0,  // 초기상태
        S10, // 초성이 입력된 상태
        S20, // 중성이 입력된 상태
        S21, // 조합 가능한 중성을 조합한 상태
        S30, // 종성이 입력된 상태
        S31  // 조합 가능한 종성을 조합한 상태
    };

    EHangulState CurrentState;
    FString CurrentInitial;
    FString CurrentMedial;
    FString CurrentFinal;
    FString CombinedString;
    FString ChrBuffer;
    FString ChrBuffer2;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
