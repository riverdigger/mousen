// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "MousenGameUserSettings.generated.h"

/**
 * 
 */
UCLASS()
class MOUSEN_API UMousenGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
public:
	UMousenGameUserSettings();

	UFUNCTION(BlueprintCallable)
	static UMousenGameUserSettings* GetMousenGameUserSettings();

	UFUNCTION(BlueprintCallable)
	void SetAlphaTeamColor(FLinearColor NewColor);
	
	UFUNCTION(BlueprintCallable)
	void SetBetaTeamColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable)
	void SetMouseSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintPure)
	FLinearColor GetAlphaTeamColor() const;

	UFUNCTION(BlueprintPure)
	FLinearColor GetBetaTeamColor() const;

	UFUNCTION(BlueprintPure)
	float GetMouseSensitivity() const;

protected:
	UPROPERTY(Config)
	FLinearColor AlphaTeamColor;
	
	UPROPERTY(Config)
	FLinearColor BetaTeamColor;

	UPROPERTY(Config)
	float MouseSensitivity;
};
