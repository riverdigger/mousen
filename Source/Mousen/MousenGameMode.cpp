// Copyright Epic Games, Inc. All Rights Reserved.

#include "MousenGameMode.h"
#include "MousenCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMousenGameMode::AMousenGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
