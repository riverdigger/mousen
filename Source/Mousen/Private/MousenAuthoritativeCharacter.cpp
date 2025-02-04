// Copyright 2023 Anton Hesselbom. All Rights Reserved.

#include "MousenAuthoritativeCharacter.h"
#include "MousenAuthoritativeCharacterMC.h"
#include "Net/UnrealNetwork.h"
#include "Runtime/Launch/Resources/Version.h"

AMousenAuthoritativeCharacter::AMousenAuthoritativeCharacter(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer.SetDefaultSubobjectClass<UMousenAuthoritativeCharacterMC>(ACharacter::CharacterMovementComponentName))
{
}

void AMousenAuthoritativeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DISABLE_REPLICATED_PROPERTY(ACharacter, RepRootMotion);
	DOREPLIFETIME_CONDITION(AMousenAuthoritativeCharacter, bHasOverrideRootMotion, COND_SimulatedOnly);
}

void AMousenAuthoritativeCharacter::PossessedBy(AController* NewController)
{
	// Bypass ACharacter::PossessedBy to avoid setting bOnlyAllowAutonomousTickPose
	APawn::PossessedBy(NewController);
}

void AMousenAuthoritativeCharacter::MousenAuthoritativeMove_Implementation(const FMousenAuthoritativeMoveData& MoveData)
{
	Cast<UMousenAuthoritativeCharacterMC>(GetCharacterMovement())->MousenAuthoritativeMove_ServerReceive(MoveData);
}

void AMousenAuthoritativeCharacter::PostNetReceiveBasedMovement(const FMousenAuthoritativeMoveData& MoveData)
{
	// Skip base updates while playing root motion, it is handled elsewhere
	if (IsPlayingNetworkedRootMotionMontage())
	{
		return;
	}

	auto Movement = GetCharacterMovement();

	Movement->bNetworkUpdateReceived = true;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 5
	FGuardValue_Bitfield(bInBaseReplication, true);
#else
	TGuardValue<bool> bInBaseReplicationGuard(bInBaseReplication, true);
#endif

	const bool bBaseChanged = (BasedMovement.MovementBase != MoveData.BasedMovementBase || BasedMovement.BoneName != MoveData.BasedMovementBaseBoneName);
	if (bBaseChanged)
	{
		// Even though we will copy the replicated based movement info, we need to use SetBase() to set up tick dependencies and trigger notifications.
		SetBase(MoveData.BasedMovementBase, MoveData.BasedMovementBaseBoneName);
	}

	// Make sure to use the values of relative location/rotation etc from the client.
	BasedMovement.MovementBase = MoveData.BasedMovementBase;
	BasedMovement.BoneName = MoveData.BasedMovementBaseBoneName;
	BasedMovement.Location = MoveData.Location;
	BasedMovement.Rotation = MoveData.Rotation;
	BasedMovement.bRelativeRotation = MoveData.bBasedHasRelativeRotation;

	if (MovementBaseUtility::UseRelativeLocation(BasedMovement.MovementBase))
	{
		// Update transform relative to movement base
		const FVector OldLocation = GetActorLocation();
		const FQuat OldRotation = GetActorQuat();
		MovementBaseUtility::GetMovementBaseTransform(MoveData.BasedMovementBase, MoveData.BasedMovementBaseBoneName, Movement->OldBaseLocation, Movement->OldBaseQuat);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
		const FTransform BaseTransform(Movement->OldBaseQuat, Movement->OldBaseLocation);
		const FVector NewLocation = BaseTransform.TransformPositionNoScale(MoveData.Location);
#else
		const FVector NewLocation = Movement->OldBaseLocation + MoveData.Location;
#endif

		FRotator NewRotation;

		if (MoveData.bBasedHasRelativeRotation)
		{
			// Relative location, relative rotation
			NewRotation = (FRotationMatrix(MoveData.Rotation) * FQuatRotationMatrix(Movement->OldBaseQuat)).Rotator();

			if (Movement->ShouldRemainVertical())
			{
				NewRotation.Pitch = 0.f;
				NewRotation.Roll = 0.f;
			}
		}
		else
		{
			// Relative location, absolute rotation
			NewRotation = MoveData.Rotation;
		}

		// When position or base changes, movement mode will need to be updated. This assumes rotation changes don't affect that.
		Movement->bJustTeleported |= (bBaseChanged || NewLocation != OldLocation);
		Movement->bNetworkSmoothingComplete = false;
		Movement->SmoothCorrection(OldLocation, OldRotation, NewLocation, NewRotation.Quaternion());
		OnUpdateSimulatedPosition(OldLocation, OldRotation);
	}
}

void AMousenAuthoritativeCharacter::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	if (GetRemoteRole() == ROLE_AutonomousProxy)
	{
		if (UMovementComponent* const MoveComponent = GetMovementComponent())
			MoveComponent->Velocity = NewVelocity;
	}
	else
		Super::PostNetReceiveVelocity(NewVelocity);
}

void AMousenAuthoritativeCharacter::PostNetReceiveLocationAndRotation()
{
	if (GetRemoteRole() == ROLE_AutonomousProxy)
	{
		auto Movement = Cast<UMousenAuthoritativeCharacterMC>(GetCharacterMovement());

		// Don't change transform if using relative position (it should be nearly the same anyway, or base may be slightly out of sync)
		if (!MovementBaseUtility::UseRelativeLocation(Movement->GetServerLatestMoveData().BasedMovementBase))
		{
			const FMousenAuthoritativeMoveData& MovementData = Movement->GetServerLatestMoveData();
			const FVector OldLocation = GetActorLocation();
			const FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(MovementData.Location, this);
			const FQuat OldRotation = GetActorQuat();

			Movement->bNetworkSmoothingComplete = false;
			Movement->bJustTeleported |= (OldLocation != NewLocation);
			Movement->SmoothCorrection(OldLocation, OldRotation, NewLocation, MovementData.Rotation.Quaternion());
			OnUpdateSimulatedPosition(OldLocation, OldRotation);
		}
		Movement->bNetworkUpdateReceived = true;
	}
	else
		Super::PostNetReceiveLocationAndRotation();
}

void AMousenAuthoritativeCharacter::OnRep_ReplicatedMovement()
{
	// Clean up any PendingAddRootMotionSources on Non-Controlled clients. Only the Controlling client should have active RootMotionSources.
	if (!IsLocallyControlled())
		if (auto Movement = GetCharacterMovement())
			Movement->CurrentRootMotion.PendingAddRootMotionSources.Empty();

	AActor::OnRep_ReplicatedMovement();
}
