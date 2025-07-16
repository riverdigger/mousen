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
	void SetFriendlyTeamColor(FLinearColor NewColor);
	
	UFUNCTION(BlueprintCallable)
	void SetEnemyTeamColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable)
	void SetMouseSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintCallable)
	void SetShowFPS(bool Enable);

	UFUNCTION(BlueprintCallable)
	void SetMasterVolume(float NewVolume);

	UFUNCTION(BlueprintCallable)
	void SetMusicVolume(float NewVolume);

	UFUNCTION(BlueprintCallable)
	void SetSFXVolume(float NewVolume);

	UFUNCTION(BlueprintPure)
	float GetMasterVolume() const;

	UFUNCTION(BlueprintPure)
	float GetMusicVolume() const;

	UFUNCTION(BlueprintPure)
	float GetSFXVolume() const;

	UFUNCTION(BlueprintPure)
	FLinearColor GetFriendlyTeamColor() const;

	UFUNCTION(BlueprintPure)
	FLinearColor GetEnemyTeamColor() const;

	UFUNCTION(BlueprintPure)
	float GetMouseSensitivity() const;

	UFUNCTION(BlueprintPure)
	bool GetShowFPS() const;

protected:
	UPROPERTY(Config)
	FLinearColor FriendlyTeamColor;
	
	UPROPERTY(Config)
	FLinearColor EnemyTeamColor;

	UPROPERTY(Config)
	float MouseSensitivity;

	UPROPERTY(Config)
	bool ShowFPS;

	UPROPERTY(Config)
	float MasterVolume;

	UPROPERTY(Config)
	float MusicVolume;

	UPROPERTY(Config)
	float SFXVolume;
};
