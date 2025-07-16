// Fill out your copyright notice in the Description page of Project Settings.


#include "MousenGameUserSettings.h"

UMousenGameUserSettings::UMousenGameUserSettings()
{
	FriendlyTeamColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	EnemyTeamColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
	MouseSensitivity = 0.542f;
	ShowFPS = false;

	// Volume
	MasterVolume = 1.0f;
	MusicVolume = 1.0f;
	SFXVolume = 1.0f;
}

UMousenGameUserSettings* UMousenGameUserSettings::GetMousenGameUserSettings()
{
	return Cast<UMousenGameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

void UMousenGameUserSettings::SetFriendlyTeamColor(FLinearColor NewColor)
{
	FriendlyTeamColor = NewColor;
}

FLinearColor UMousenGameUserSettings::GetFriendlyTeamColor() const
{
	return FriendlyTeamColor;
}

void UMousenGameUserSettings::SetEnemyTeamColor(FLinearColor NewColor)
{
	EnemyTeamColor = NewColor;
}

void UMousenGameUserSettings::SetMouseSensitivity(float NewSensitivity)
{
	MouseSensitivity = NewSensitivity;
}

void UMousenGameUserSettings::SetShowFPS(bool Enable)
{
	ShowFPS = Enable;
}

void UMousenGameUserSettings::SetMasterVolume(float NewVolume)
{
	MasterVolume = NewVolume;
}

void UMousenGameUserSettings::SetMusicVolume(float NewVolume)
{
	MusicVolume = NewVolume;
}

void UMousenGameUserSettings::SetSFXVolume(float NewVolume)
{
	SFXVolume = NewVolume;
}

float UMousenGameUserSettings::GetMasterVolume() const
{
	return MasterVolume;
}

float UMousenGameUserSettings::GetMusicVolume() const
{
	return MusicVolume;
}

float UMousenGameUserSettings::GetSFXVolume() const
{
	return SFXVolume;
}

FLinearColor UMousenGameUserSettings::GetEnemyTeamColor() const
{
	return EnemyTeamColor;
}

float UMousenGameUserSettings::GetMouseSensitivity() const
{
	return MouseSensitivity;
}

bool UMousenGameUserSettings::GetShowFPS() const
{
	return ShowFPS;
}
