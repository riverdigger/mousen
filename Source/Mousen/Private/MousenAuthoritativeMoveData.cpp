// Copyright 2023 Anton Hesselbom. All Rights Reserved.

#include "MousenAuthoritativeMoveData.h"
#include "Components/PrimitiveComponent.h"

bool FMousenAuthoritativeMoveData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;

	// Location & Rotation
	Location.NetSerialize(Ar, Map, bOutSuccess);
	Rotation.SerializeCompressed(Ar);

	// Velocity - Skip serializing if velocity is zero to save bandwidth on characters standing still
	bool HasVelocity = !Velocity.IsNearlyZero();
	Ar.SerializeBits(&HasVelocity, 1);
	if (HasVelocity)
		Velocity.NetSerialize(Ar, Map, bOutSuccess);
	else
		Velocity = FVector_NetQuantize10(ForceInitToZero);

	// ControlRotation & MovementMode
	ControlRotation.SerializeCompressed(Ar);
	Ar << MovementMode;

	// BasedMovement
	Ar << BasedMovementBase;
	Ar << BasedMovementBaseBoneName;
	Ar.SerializeBits(&bBasedHasRelativeRotation, 1);
	Ar.SerializeBits(&bHasOverrideRootMotion, 1);

	// Crouching
	Ar.SerializeBits(&bCrouching, 1);

	// Timestamp
	Ar.SerializeBits(&bHasTimestamp, 1);
	if (bHasTimestamp)
		Ar << Timestamp;

	return true;
}
