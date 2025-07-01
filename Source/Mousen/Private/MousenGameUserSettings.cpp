// Fill out your copyright notice in the Description page of Project Settings.


#include "MousenGameUserSettings.h"

UMousenGameUserSettings::UMousenGameUserSettings()
{
	AlphaTeamColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	BetaTeamColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
	MouseSensitivity = 0.542f;
}

UMousenGameUserSettings* UMousenGameUserSettings::GetMousenGameUserSettings()
{
	return Cast<UMousenGameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

void UMousenGameUserSettings::SetAlphaTeamColor(FLinearColor NewColor)
{
	AlphaTeamColor = NewColor;
}

FLinearColor UMousenGameUserSettings::GetAlphaTeamColor() const
{
	return AlphaTeamColor;
}

void UMousenGameUserSettings::SetBetaTeamColor(FLinearColor NewColor)
{
	BetaTeamColor = NewColor;
}

void UMousenGameUserSettings::SetMouseSensitivity(float NewSensitivity)
{
	MouseSensitivity = NewSensitivity;
}

FLinearColor UMousenGameUserSettings::GetBetaTeamColor() const
{
	return BetaTeamColor;
}

float UMousenGameUserSettings::GetMouseSensitivity() const
{
	return MouseSensitivity;
}
