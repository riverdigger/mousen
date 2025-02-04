// Copyright 2023 Anton Hesselbom. All Rights Reserved.

#include "MousenAuthoritativeCharacterMC.h"
#include "MousenAuthoritativeCharacter.h"
#include "Engine/Player.h"
#include "Engine/World.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/GameStateBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ProfilingDebugging/CsvProfiler.h"

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
#include "Engine/ScopedMovementUpdate.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMousenAuthoritativeCharacterMovement, Log, All);

DECLARE_CYCLE_STAT(TEXT("ClientAuthChar Tick"), STAT_CharacterMovementTick, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("ClientAuthChar Simulated Time"), STAT_CharacterMovementSimulated, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("ClientAuthChar NonSimulated Time"), STAT_CharacterMovementNonSimulated, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("ClientAuthChar Physics Interation"), STAT_CharPhysicsInteraction, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("ClientAuthChar Update Acceleration"), STAT_CharUpdateAcceleration, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("ClientAuthChar ReplicateAutonomousMoveToServer"), STAT_CharacterMovementReplicateAutonomousMoveToServer, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("ClientAuthChar SmoothClientPosition"), STAT_CharacterMovementSmoothClientPosition, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("ClientAuthChar NetSmoothCorrection"), STAT_CharacterMovementSmoothCorrection, STATGROUP_Character);

UMousenAuthoritativeCharacterMC::UMousenAuthoritativeCharacterMC(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ListenServerNetworkSimulatedSmoothLocationTime = NetworkSimulatedSmoothLocationTime;
	ListenServerNetworkSimulatedSmoothRotationTime = NetworkSimulatedSmoothRotationTime;
}

void UMousenAuthoritativeCharacterMC::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPED_NAMED_EVENT(UMousenAuthoritativeCharacterMC_TickComponent, FColor::Yellow);
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementTick);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(CharacterMovement);

#if ENGINE_MAJOR_VERSION >= 5
	FVector InputVector = FVector::ZeroVector;
	bool bUsingAsyncTick = (CharacterMovementCVars::AsyncCharacterMovement == 1) && IsAsyncCallbackRegistered();
	if (!bUsingAsyncTick)
		InputVector = ConsumeInputVector(); // Do not consume input if simulating asynchronously, we will consume input when filling out async inputs.
#else
	const FVector InputVector = ConsumeInputVector();
#endif

	if (!HasValidData() || ShouldSkipUpdate(DeltaTime))
		return;

	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Super tick may destroy/invalidate CharacterOwner or UpdatedComponent, so we need to re-check.
	if (!HasValidData())
		return;

#if ENGINE_MAJOR_VERSION >= 5
	if (bUsingAsyncTick)
	{
		check(CharacterOwner && CharacterOwner->GetMesh());
		USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();
		if (CharacterMesh->ShouldTickPose())
		{
			const bool bWasPlayingRootMotion = CharacterOwner->IsPlayingRootMotion();

			CharacterMesh->TickPose(DeltaTime, true);
			// We are simulating character movement on physics thread, do not tick movement.
			const bool bIsPlayingRootMotion = CharacterOwner->IsPlayingRootMotion();
			if (bIsPlayingRootMotion || bWasPlayingRootMotion)
			{
				FRootMotionMovementParams RootMotion = CharacterMesh->ConsumeRootMotion();
				if (RootMotion.bHasRootMotion)
				{
					RootMotion.ScaleRootMotionTranslation(CharacterOwner->GetAnimRootMotionTranslationScale());
					RootMotionParams.Accumulate(RootMotion);
				}
			}
		}

		AccumulateRootMotionForAsync(DeltaTime, AsyncRootMotion);
		return;
	}
#endif

	// See if we fell out of the world.
	const bool bIsSimulatingPhysics = UpdatedComponent->IsSimulatingPhysics();
	if (CharacterOwner->GetLocalRole() == ROLE_Authority && (!bCheatFlying || bIsSimulatingPhysics) && !CharacterOwner->CheckStillInWorld())
		return;

	// We don't update if simulating physics (eg ragdolls).
	if (bIsSimulatingPhysics)
	{
		// Update camera to ensure client gets updates even when physics move him far away from point where simulation started
		if (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client))
			MarkForClientCameraUpdate();

		ClearAccumulatedForces();
		return;
	}

	AvoidanceLockTimer -= DeltaTime;

	if (CharacterOwner->GetLocalRole() > ROLE_SimulatedProxy)
	{
		SCOPE_CYCLE_COUNTER(STAT_CharacterMovementNonSimulated);

		// Allow root motion to move characters that have no controller.
		if (CharacterOwner->IsLocallyControlled() || (!CharacterOwner->Controller && bRunPhysicsWithNoController) || (!CharacterOwner->Controller && CharacterOwner->IsPlayingRootMotion()))
			ControlledCharacterMove(InputVector, DeltaTime);
		else if (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy)
		{
			if (bShrinkProxyCapsule)
				AdjustProxyCapsuleSize();
			SimulatedTick(DeltaTime);
		}
	}
	else if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		if (bShrinkProxyCapsule)
			AdjustProxyCapsuleSize();
		SimulatedTick(DeltaTime);
	}

	if (bUseRVOAvoidance)
		UpdateDefaultAvoidance();

	if (bEnablePhysicsInteraction)
	{
		SCOPE_CYCLE_COUNTER(STAT_CharPhysicsInteraction);
		ApplyDownwardForce(DeltaTime);
		ApplyRepulsionForce(DeltaTime);
	}
}

void UMousenAuthoritativeCharacterMC::SimulatedTick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSimulated);
	checkSlow(CharacterOwner != nullptr);

	if (CharacterOwner->IsReplicatingMovement() && UpdatedComponent && !Cast<AMousenAuthoritativeCharacter>(CharacterOwner)->bHasOverrideRootMotion)
	{
		USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
		const FVector SavedMeshRelativeLocation = Mesh ? Mesh->GetRelativeLocation() : FVector::ZeroVector;
		const FQuat SavedCapsuleRotation = UpdatedComponent->GetComponentQuat();
		const bool bPreventMeshMovement = !bNetworkSmoothingComplete;

		// Avoid moving the mesh during movement if SmoothClientPosition will take care of it.
		{
			const FScopedPreventAttachedComponentMove PreventMeshMovement(bPreventMeshMovement ? Mesh : nullptr);
			if (CharacterOwner->IsPlayingRootMotion())
				PerformMovement(DeltaSeconds);
			else
				SimulateMovement(DeltaSeconds);
		}

		// With Linear smoothing we need to know if the rotation changes, since the mesh should follow along with that (if it was prevented above).
		// This should be rare that rotation changes during simulation, but it can happen when ShouldRemainVertical() changes, or standing on a moving base.
		const bool bValidateRotation = bPreventMeshMovement && (NetworkSmoothingMode == ENetworkSmoothingMode::Linear);
		if (bValidateRotation && UpdatedComponent)
		{
			// Same mesh with different rotation?
			const FQuat NewCapsuleRotation = UpdatedComponent->GetComponentQuat();
			if (Mesh == CharacterOwner->GetMesh() && !NewCapsuleRotation.Equals(SavedCapsuleRotation, 1e-6f) && ClientPredictionData)
			{
				// Smoothing should lerp toward this new rotation target, otherwise it will just try to go back toward the old rotation.
				ClientPredictionData->MeshRotationTarget = NewCapsuleRotation;
				Mesh->SetRelativeLocationAndRotation(SavedMeshRelativeLocation, CharacterOwner->GetBaseRotationOffset());
			}
		}
	}

	// Smooth mesh location after moving the capsule above.
	if (!bNetworkSmoothingComplete)
	{
		const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);
		const bool bIsRemoteAutoProxy = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);
		if (bIsSimulatedProxy || bIsRemoteAutoProxy)
		{
			SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSmoothClientPosition);
			SmoothClientPosition(DeltaSeconds);
		}
	}
}

void UMousenAuthoritativeCharacterMC::ControlledCharacterMove(const FVector& InputVector, float DeltaSeconds)
{
	{
		SCOPE_CYCLE_COUNTER(STAT_CharUpdateAcceleration);

		// We need to check the jump state before adjusting input acceleration, to minimize latency
		// and to make sure acceleration respects our potentially new falling state.
		CharacterOwner->CheckJumpInput(DeltaSeconds);

		// apply input to acceleration
		Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector));
		AnalogInputModifier = ComputeAnalogInputModifier();
	}

	const FVector PreviousLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	const FQuat PreviousRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;

	PerformMovement(DeltaSeconds);

	if (CharacterOwner->HasAuthority() && GetWorld())
	{
		const bool bLocationChanged = (PreviousLocation != LastUpdateLocation);
		const bool bRotationChanged = (PreviousRotation != LastUpdateRotation);
		if (bLocationChanged || bRotationChanged)
			ServerLastTransformUpdateTimeStamp = GetWorld()->GetTimeSeconds();
	}

	Cast<AMousenAuthoritativeCharacter>(CharacterOwner)->bHasOverrideRootMotion = CharacterOwner->GetCharacterMovement()->CurrentRootMotion.HasOverrideVelocity();
	if (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client) && CharacterOwner->IsReplicatingMovement())
	{
		ReplicateAutonomousMoveToServer();
	}
}

void UMousenAuthoritativeCharacterMC::SimulateMovement(float DeltaTime)
{
	if (CharacterOwner->GetRemoteRole() != ROLE_AutonomousProxy)
	{
		Super::SimulateMovement(DeltaTime);
		return;
	}

	if (!HasValidData() || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
		return;

	// Workaround for replication not being updated initially
	if (ServerLatestMoveData.Location.IsZero() && ServerLatestMoveData.Rotation.IsZero() && ServerLatestMoveData.Velocity.IsZero())
		return;

	FVector OldVelocity;
	FVector OldLocation;

	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		bool bHandledNetUpdate = false;
		// Handle network changes
		if (bNetworkUpdateReceived)
		{
			bNetworkUpdateReceived = false;
			bHandledNetUpdate = true;
			UE_LOG(LogMousenAuthoritativeCharacterMovement, Verbose, TEXT("Proxy %s received net update"), *CharacterOwner->GetName());
			if (bNetworkMovementModeChanged)
			{
				ApplyNetworkMovementMode(ServerLatestMoveData.MovementMode);
				bNetworkMovementModeChanged = false;
			}
			else if (bJustTeleported || bForceNextFloorCheck)
			{
				// Make sure floor is current. We will continue using the replicated base, if there was one.
				bJustTeleported = false;
				UpdateFloorFromAdjustment();
			}
		}
		else if (bForceNextFloorCheck)
			UpdateFloorFromAdjustment();

		if (MovementMode != MOVE_None)
			HandlePendingLaunch();
		ClearAccumulatedForces();

		if (MovementMode == MOVE_None)
			return;

		const bool bSimGravityDisabled = (CharacterOwner->bSimGravityDisabled);
		const bool bZeroReplicatedGroundVelocity = (IsMovingOnGround() && ServerLatestMoveData.Velocity.IsZero());

		// bSimGravityDisabled means velocity was zero when replicated and we were stuck in something. Avoid external changes in velocity as well.
		// Being in ground movement with zero velocity, we cannot simulate proxy velocities safely because we might not get any further updates from the server.
		if (bSimGravityDisabled || bZeroReplicatedGroundVelocity)
			Velocity = FVector::ZeroVector;

		MaybeUpdateBasedMovement(DeltaTime);

		// simulated pawns predict location
		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();

		UpdateProxyAcceleration();

		// May only need to simulate forward on frames where we haven't just received a new position update.
		if (!bHandledNetUpdate || !bNetworkSkipProxyPredictionOnNetUpdate)
		{
			UE_LOG(LogMousenAuthoritativeCharacterMovement, Verbose, TEXT("Proxy %s simulating movement"), *GetNameSafe(CharacterOwner));
			FStepDownResult StepDownResult;
			MoveSmooth(Velocity, DeltaTime, &StepDownResult);

			// find floor and check if falling
			if (IsMovingOnGround() || MovementMode == MOVE_Falling)
			{
				if (StepDownResult.bComputedFloor)
					CurrentFloor = StepDownResult.FloorResult;
				else if (Velocity.Z <= 0.f)
					FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, Velocity.IsZero(), nullptr);
				else
					CurrentFloor.Clear();

				if (!CurrentFloor.IsWalkableFloor())
				{
					if (!bSimGravityDisabled)
					{
						// No floor, must fall.
						if (Velocity.Z <= 0.f || bApplyGravityWhileJumping || !CharacterOwner->IsJumpProvidingForce())
							Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaTime);
					}
					SetMovementMode(MOVE_Falling);
				}
				else
				{
					// Walkable floor
					if (IsMovingOnGround())
					{
						AdjustFloorHeight();
						SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
					}
					else if (MovementMode == MOVE_Falling)
					{
						if (CurrentFloor.FloorDist <= MIN_FLOOR_DIST || (bSimGravityDisabled && CurrentFloor.FloorDist <= MAX_FLOOR_DIST))
							SetPostLandedPhysics(CurrentFloor.HitResult); // Landed
						else
						{
							if (!bSimGravityDisabled)
								Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaTime); // Continue falling.
							CurrentFloor.Clear();
						}
					}
				}
			}
		}
		else
			UE_LOG(LogMousenAuthoritativeCharacterMovement, Verbose, TEXT("Proxy %s SKIPPING simulate movement"), *GetNameSafe(CharacterOwner));

		// consume path following requested velocity
		bHasRequestedVelocity = false;

		OnMovementUpdated(DeltaTime, OldLocation, OldVelocity);
	} // End scoped movement update

	// Call custom post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaTime, OldLocation, OldVelocity);

	MaybeSaveBaseLocation();

	UpdateComponentVelocity();
	bJustTeleported = false;

	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;
}

void UMousenAuthoritativeCharacterMC::SmoothClientPosition(float DeltaSeconds)
{
	if (!HasValidData() || NetworkSmoothingMode == ENetworkSmoothingMode::Disabled)
	{
		return;
	}

	// Only client proxies or remote clients on a listen server should run this code.
	const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);
	const bool bIsRemoteAutoProxy = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);
	if (!ensure(bIsSimulatedProxy || bIsRemoteAutoProxy))
	{
		return;
	}

	SmoothClientPosition_Interpolate(DeltaSeconds);
	SmoothClientPosition_UpdateVisuals();
}

void UMousenAuthoritativeCharacterMC::SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation)
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSmoothCorrection);
	if (!HasValidData())
	{
		return;
	}

	// Only client proxies or remote clients on a listen server should run this code.
	const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);
	const bool bIsRemoteAutoProxy = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);
	ensure(bIsSimulatedProxy || bIsRemoteAutoProxy);

	// Getting a correction means new data, so smoothing needs to run.
	bNetworkSmoothingComplete = false;

	// Handle selected smoothing mode.
	if (NetworkSmoothingMode == ENetworkSmoothingMode::Disabled || GetNetMode() == NM_Standalone)
	{
		UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
		bNetworkSmoothingComplete = true;
	}
	else if (FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character())
	{
		const UWorld* MyWorld = GetWorld();
		if (!ensure(MyWorld != nullptr))
		{
			return;
		}

		// The mesh doesn't move, but the capsule does so we have a new offset.
		FVector NewToOldVector = (OldLocation - NewLocation);
		if (bIsNavWalkingOnServer && FMath::Abs(NewToOldVector.Z) < NavWalkingFloorDistTolerance)
		{
			// ignore smoothing on Z axis
			// don't modify new location (local simulation result), since it's probably more accurate than server data
			// and shouldn't matter as long as difference is relatively small
			NewToOldVector.Z = 0;
		}

		const float DistSq = NewToOldVector.SizeSquared();
		if (DistSq > FMath::Square(ClientData->MaxSmoothNetUpdateDist))
		{
			ClientData->MeshTranslationOffset = (DistSq > FMath::Square(ClientData->NoSmoothNetUpdateDist))
				? FVector::ZeroVector
				: ClientData->MeshTranslationOffset + ClientData->MaxSmoothNetUpdateDist * NewToOldVector.GetSafeNormal();
		}
		else
		{
			ClientData->MeshTranslationOffset = ClientData->MeshTranslationOffset + NewToOldVector;
		}

		UE_LOG(LogMousenAuthoritativeCharacterMovement, Verbose, TEXT("Proxy %s SmoothCorrection(%.2f)"), *GetNameSafe(CharacterOwner), FMath::Sqrt(DistSq));
		if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
		{
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;

			// Remember the current and target rotation, we're going to lerp between them
			ClientData->OriginalMeshRotationOffset = OldRotation;
			ClientData->MeshRotationOffset = OldRotation;
			ClientData->MeshRotationTarget = NewRotation;

			// Move the capsule, but not the mesh.
			// Note: we don't change rotation, we lerp towards it in SmoothClientPosition.
			if (NewLocation != OldLocation)
			{
				const FScopedPreventAttachedComponentMove PreventMeshMove(CharacterOwner->GetMesh());
				UpdatedComponent->SetWorldLocation(NewLocation, false, nullptr, GetTeleportType());
			}
		}
		else
		{
			// Calc rotation needed to keep current world rotation after UpdatedComponent moves.
			// Take difference between where we were rotated before, and where we're going
			ClientData->MeshRotationOffset = (NewRotation.Inverse() * OldRotation) * ClientData->MeshRotationOffset;
			ClientData->MeshRotationTarget = FQuat::Identity;

			const FScopedPreventAttachedComponentMove PreventMeshMove(CharacterOwner->GetMesh());
			UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation, false, nullptr, GetTeleportType());
		}

		//////////////////////////////////////////////////////////////////////////
		// Update smoothing timestamps

		// If running ahead, pull back slightly. This will cause the next delta to seem slightly longer, and cause us to lerp to it slightly slower.
		if (ClientData->SmoothingClientTimeStamp > ClientData->SmoothingServerTimeStamp)
		{
			const double OldClientTimeStamp = ClientData->SmoothingClientTimeStamp;
			ClientData->SmoothingClientTimeStamp = FMath::LerpStable(ClientData->SmoothingServerTimeStamp, OldClientTimeStamp, 0.5);

			UE_LOG(LogMousenAuthoritativeCharacterMovement, VeryVerbose, TEXT("SmoothCorrection: Pull back client from ClientTimeStamp: %.6f to %.6f, ServerTimeStamp: %.6f for %s"),
				OldClientTimeStamp, ClientData->SmoothingClientTimeStamp, ClientData->SmoothingServerTimeStamp, *GetNameSafe(CharacterOwner));
		}

		// Using server timestamp lets us know how much time actually elapsed, regardless of packet lag variance.
		double OldServerTimeStamp = ClientData->SmoothingServerTimeStamp;
		if (bIsSimulatedProxy)
		{
			// This value is normally only updated on the server, however some code paths might try to read it instead of the replicated value so copy it for proxies as well.
			ServerLastTransformUpdateTimeStamp = CharacterOwner->GetReplicatedServerLastTransformUpdateTimeStamp();
		}
		ClientData->SmoothingServerTimeStamp = ServerLastTransformUpdateTimeStamp;

		// Initial update has no delta.
		if (ClientData->LastCorrectionTime == 0)
		{
			ClientData->SmoothingClientTimeStamp = ClientData->SmoothingServerTimeStamp;
			OldServerTimeStamp = ClientData->SmoothingServerTimeStamp;
		}

		// Don't let the client fall too far behind or run ahead of new server time.
		const double ServerDeltaTime = ClientData->SmoothingServerTimeStamp - OldServerTimeStamp;
		const double MaxOffset = ClientData->MaxClientSmoothingDeltaTime;
		const double MinOffset = FMath::Min(double(ClientData->SmoothNetUpdateTime), MaxOffset);

		// MaxDelta is the farthest behind we're allowed to be after receiving a new server time.
		const double MaxDelta = FMath::Clamp(ServerDeltaTime * 1.25, MinOffset, MaxOffset);
		ClientData->SmoothingClientTimeStamp = FMath::Clamp(ClientData->SmoothingClientTimeStamp, ClientData->SmoothingServerTimeStamp - MaxDelta, ClientData->SmoothingServerTimeStamp);

		// Compute actual delta between new server timestamp and client simulation.
		ClientData->LastCorrectionDelta = ClientData->SmoothingServerTimeStamp - ClientData->SmoothingClientTimeStamp;
		ClientData->LastCorrectionTime = MyWorld->GetTimeSeconds();

		UE_LOG(LogMousenAuthoritativeCharacterMovement, VeryVerbose, TEXT("SmoothCorrection: WorldTime: %.6f, ServerTimeStamp: %.6f, ClientTimeStamp: %.6f, Delta: %.6f for %s"),
			MyWorld->GetTimeSeconds(), ClientData->SmoothingServerTimeStamp, ClientData->SmoothingClientTimeStamp, ClientData->LastCorrectionDelta, *GetNameSafe(CharacterOwner));
	}
}

void UMousenAuthoritativeCharacterMC::MousenAuthoritativeMove_ServerReceive(const FMousenAuthoritativeMoveData& MoveData)
{
	if (CharacterOwner->GetRemoteRole() != ROLE_AutonomousProxy || !CharacterOwner->IsReplicatingMovement())
		return;

	bool bHadOrHasBasedMovement = MoveData.BasedMovementBase != nullptr || ServerLatestMoveData.BasedMovementBase != nullptr;

	bNetworkMovementModeChanged |= ((ServerLatestMoveData.MovementMode != MoveData.MovementMode) || (PackNetworkMovementMode() != MoveData.MovementMode));
	bNetworkUpdateReceived |= bNetworkMovementModeChanged || bJustTeleported;

	if (MoveData.bHasTimestamp)
		ServerLastTransformUpdateTimeStamp = MoveData.Timestamp;

	ServerLatestMoveData = MoveData;

	auto Character = Cast<AMousenAuthoritativeCharacter>(CharacterOwner);
	Character->bHasOverrideRootMotion = MoveData.bHasOverrideRootMotion;
	if (Character->GetController())
		Character->GetController()->SetControlRotation(MoveData.ControlRotation);
	Character->PostNetReceiveVelocity(MoveData.Velocity);

	if (bHadOrHasBasedMovement)
		Character->PostNetReceiveBasedMovement(MoveData);

	Character->PostNetReceiveLocationAndRotation();

	if (CharacterOwner->bIsCrouched != MoveData.bCrouching)
	{
		if (MoveData.bCrouching)
		{
			bWantsToCrouch = true;
			Crouch(true);
			CharacterOwner->bIsCrouched = true;
		}
		else
		{
			bWantsToCrouch = false;
			UnCrouch(true);
			CharacterOwner->bIsCrouched = false;
		}
	}
}

void UMousenAuthoritativeCharacterMC::ReplicateAutonomousMoveToServer()
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementReplicateAutonomousMoveToServer);
	check(CharacterOwner != NULL);

	// Can only start sending moves if our controllers are synced up over the network, otherwise we flood the reliable buffer.
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (PC && PC->AcknowledgedPawn != CharacterOwner)
		return;

	// Bail out if our character's controller doesn't have a Player. This may be the case when the local player
	// has switched to another controller, such as a debug camera controller.
	if (PC && PC->Player == nullptr)
		return;

	// Decide whether to hold off on sending move
	const float NetMoveDelta = FMath::Clamp(GetMousenAuthoritativeNetSendDeltaTime(PC), 1.f / 120.f, 1.f / 5.f);

	if ((GetWorld()->RealTimeSeconds - LastClientUpdateTime) < NetMoveDelta)
		return; // Delay sending this move.

	LastClientUpdateTime = GetWorld()->RealTimeSeconds;

	// Send move to server
	SendMousenAuthoritativeMove();
}

float UMousenAuthoritativeCharacterMC::GetMousenAuthoritativeNetSendDeltaTime(const APlayerController* Controller) const
{
	const UPlayer* Player = Controller ? Controller->Player : nullptr;
	const AGameStateBase* const GameState = GetWorld()->GetGameState();
	const AGameNetworkManager* GameNetworkManager = (const AGameNetworkManager*)(AGameNetworkManager::StaticClass()->GetDefaultObject());
	float NetMoveDelta = GameNetworkManager->ClientNetSendMoveDeltaTime;

	if (Controller && Player)
	{
		// Send moves more frequently in small games where server isn't likely to be saturated
		if ((Player->CurrentNetSpeed > GameNetworkManager->ClientNetSendMoveThrottleAtNetSpeed) && (GameState != nullptr) && (GameState->PlayerArray.Num() <= GameNetworkManager->ClientNetSendMoveThrottleOverPlayerCount))
			NetMoveDelta = GameNetworkManager->ClientNetSendMoveDeltaTime;
		else
			NetMoveDelta = FMath::Max(GameNetworkManager->ClientNetSendMoveDeltaTimeThrottled, 2 * GameNetworkManager->MoveRepSize / Player->CurrentNetSpeed);

		// Lower frequency for standing still
		if (Acceleration.IsZero() && Velocity.IsZero())
			NetMoveDelta = FMath::Max(GameNetworkManager->ClientNetSendMoveDeltaTimeStationary, NetMoveDelta);
	}

	return NetMoveDelta;
}

void UMousenAuthoritativeCharacterMC::SendMousenAuthoritativeMove()
{
	// Pass through RPC call to character, there is less RPC bandwidth overhead when used on an Actor rather than a Component.
	Cast<AMousenAuthoritativeCharacter>(CharacterOwner)->MousenAuthoritativeMove(GenerateMousenAuthoritativeMoveData());

	MarkForClientCameraUpdate();
}

FMousenAuthoritativeMoveData UMousenAuthoritativeCharacterMC::GenerateMousenAuthoritativeMoveData()
{
	FMousenAuthoritativeMoveData Data;
	Data.Location = CharacterOwner->GetActorLocation();
	Data.Rotation = CharacterOwner->GetActorRotation();
	Data.Velocity = CharacterOwner->GetVelocity();
	Data.ControlRotation = CharacterOwner->GetControlRotation();
	Data.MovementMode = CharacterOwner->GetCharacterMovement()->PackNetworkMovementMode();

	const auto& BasedMovement = CharacterOwner->GetBasedMovement();

	// Make sure the thing we're based on:
	// A) Supports networking (e.g. if a replicated actor has a non replicated component)
	// B) Does *not* have Authority, as Authority would mean it's a Client-spawned actor.
	// If any of these are not the case, just replicate our raw location and rotation, and ignore the base, as the Server wont know what it is.
	if (BasedMovement.MovementBase != nullptr && BasedMovement.MovementBase->IsSupportedForNetworking() && BasedMovement.MovementBase->GetOwner() != nullptr && !BasedMovement.MovementBase->GetOwner()->HasAuthority())
	{
		Data.BasedMovementBase = BasedMovement.MovementBase;
		Data.BasedMovementBaseBoneName = BasedMovement.BoneName;
		Data.bBasedHasRelativeRotation = BasedMovement.HasRelativeRotation();
		if (BasedMovement.HasRelativeLocation())
		{
			Data.Location = BasedMovement.Location;
			if (Data.bBasedHasRelativeRotation)
				Data.Rotation = BasedMovement.Rotation;
		}
	}
	else
	{
		Data.BasedMovementBase = nullptr;
		Data.BasedMovementBaseBoneName = NAME_None;
		Data.bBasedHasRelativeRotation = false;
	}

	Data.bCrouching = CharacterOwner->bIsCrouched;
	Data.bHasOverrideRootMotion = Cast<AMousenAuthoritativeCharacter>(CharacterOwner)->bHasOverrideRootMotion;
	if (auto World = GetWorld())
		if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
		{
			Data.bHasTimestamp = true;
			Data.Timestamp = World->GetTimeSeconds();
		}

	return Data;
}
