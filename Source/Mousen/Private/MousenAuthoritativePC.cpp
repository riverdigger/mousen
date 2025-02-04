// Copyright 2023 Anton Hesselbom. All Rights Reserved.

#include "MousenAuthoritativePC.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/CheatManager.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "ProfilingDebugging/CsvProfiler.h"

DECLARE_CYCLE_STAT(TEXT("ClientAuthPC Tick"), STAT_ClientAuthPC_Tick, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("ClientAuthPC Tick Actor"), STAT_ClientAuthPC_TickActor, STATGROUP_PlayerController);

void AMousenAuthoritativePC::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(PlayerControllerTick);
	SCOPE_CYCLE_COUNTER(STAT_ClientAuthPC_Tick);
	SCOPE_CYCLE_COUNTER(STAT_ClientAuthPC_TickActor);

	if (TickType == LEVELTICK_PauseTick && !ShouldPerformFullTickWhenPaused())
	{
		if (PlayerInput)
			TickPlayerInput(DeltaTime, true);

		// Clear axis inputs from previous frame.
		RotationInput = FRotator::ZeroRotator;

		if (IsValid(this))
			Tick(DeltaTime); // Perform any tick functions unique to an actor subclass

		return; // Root of tick hierarchy
	}

	// Root of tick hierarchy

	const bool bIsClient = IsNetMode(NM_Client);
	const bool bIsLocallyControlled = IsLocalPlayerController();

	if ((GetRemoteRole() == ROLE_AutonomousProxy) && !bIsClient && !bIsLocallyControlled)
	{
		// Update viewtarget replicated info
		if (PlayerCameraManager != nullptr)
		{
			APawn* TargetPawn = PlayerCameraManager->GetViewTargetPawn();

			if ((TargetPawn != GetPawn()) && (TargetPawn != nullptr))
				TargetViewRotation = TargetPawn->GetViewRotation();
		}
	}
	else if (GetLocalRole() > ROLE_SimulatedProxy)
	{
		// Process PlayerTick with input.
		if (!PlayerInput && (Player == nullptr || Cast<ULocalPlayer>(Player) != nullptr))
			InitInputSystem();

		if (PlayerInput)
		{
			QUICK_SCOPE_CYCLE_COUNTER(PlayerTick);
			PlayerTick(DeltaTime);
		}

		if (!IsValid(this))
			return;

		// Update viewtarget replicated info
		if (PlayerCameraManager != nullptr)
		{
			APawn* TargetPawn = PlayerCameraManager->GetViewTargetPawn();
			if ((TargetPawn != GetPawn()) && (TargetPawn != nullptr))
				SmoothTargetViewRotation(TargetPawn, DeltaTime);

			// Send a camera update if necessary.
			// That position will be used as the base for replication
			// (i.e., the origin that will be used when calculating NetCullDistance for other Actors / Objects).
			// We only do this when the Pawn will move, to prevent spamming RPCs.
			if (bIsClient && bIsLocallyControlled && GetPawn() && PlayerCameraManager->bUseClientSideCameraUpdates)
			{
				UPawnMovementComponent* PawnMovement = GetPawn()->GetMovementComponent();
				if (PawnMovement != nullptr &&
					!PawnMovement->IsMoveInputIgnored() &&
					(PawnMovement->GetLastInputVector() != FVector::ZeroVector || PawnMovement->Velocity != FVector::ZeroVector))
				{
					PlayerCameraManager->bShouldSendClientSideCameraUpdate = true;
				}
			}
		}
	}

	if (IsValid(this))
	{
		QUICK_SCOPE_CYCLE_COUNTER(Tick);
		Tick(DeltaTime); // Perform any tick functions unique to an actor subclass
	}

	// Clear old axis inputs since we are done with them. 
	RotationInput = FRotator::ZeroRotator;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CheatManager != nullptr)
	{
		CheatManager->TickCollisionDebug();
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}
