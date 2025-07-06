// Fill out your copyright notice in the Description page of Project Settings.


#include "MousenGameUserSettings.h"

UMousenGameUserSettings::UMousenGameUserSettings()
{
	FriendlyTeamColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	EnemyTeamColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
	MouseSensitivity = 0.542f;
	ShowFPS = false;
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
