// Copyright 2023 Anton Hesselbom. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "MousenAuthoritativeMoveData.generated.h"

// MoveData sent from the Client to the Server for MousenAuthoritative characters
USTRUCT()
struct FMousenAuthoritativeMoveData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector_NetQuantize10(ForceInitToZero);
	UPROPERTY()
	FRotator Rotation = FRotator(ForceInitToZero);
	UPROPERTY()
	FVector_NetQuantize10 Velocity = FVector_NetQuantize10(ForceInitToZero);
	UPROPERTY()
	FRotator ControlRotation = FRotator(ForceInitToZero);

	UPROPERTY()
	uint8 MovementMode = 0;

	UPROPERTY()
	UPrimitiveComponent* BasedMovementBase = nullptr;
	UPROPERTY()
	FName BasedMovementBaseBoneName = NAME_None;
	UPROPERTY()
	bool bBasedHasRelativeRotation = false;

	UPROPERTY()
	bool bHasOverrideRootMotion = false;

	UPROPERTY()
	bool bCrouching = false;

	UPROPERTY()
	bool bHasTimestamp = false;

	UPROPERTY()
	float Timestamp = 0;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template <>
struct TStructOpsTypeTraits<FMousenAuthoritativeMoveData> : public TStructOpsTypeTraitsBase2<FMousenAuthoritativeMoveData>
{
	enum
	{
		WithNetSerializer = true
	};
};
