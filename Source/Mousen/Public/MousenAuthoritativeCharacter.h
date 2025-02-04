// Copyright 2023 Anton Hesselbom. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MousenAuthoritativeMoveData.h"
#include "GameFramework/Character.h"
#include "MousenAuthoritativeCharacter.generated.h"

// Character baseclass that makes the clients be authoritative of their own movement.
//
// This works very similarly to how the Server replicates movement to SimulatedProxies, but now also from the AutonomousProxy on a Client to the Server.
//
// This has a few pros and cons:
// 1) Positional corrections from the server can happen for several reasons, lag, poor synchronization, etc, etc, and are very jarring for the player when they happen.
//    But since this Client is Authoritative, there will never be any corrections sent from the Server.
//    This way, any lag and synchronization issues will only be seen on the Character for other players, and not for the controlling player.
//    Which is significantly easier to hide with some smoothing; and since the ones that see the Character wont be controlling the Character themselves,
//    they likely wont even notice any discrepancies, as it can be very hard to tell the difference between what they saw happen and what the controlling Client saw.
// 
// 2) Since we now basically just replicate position and velocity, creating custom movement modes is *significantly* easier.
//    Normally we'd have to make sure that the Server simulates the character exactly the same way the Client did, which is not a trivial task.
//    It requires adding new data to replicate and save, as well as making sure that the new movement logic works with the rewind and replay system, etc.
//    All this requires deep knowledge about how Unreal's C++ movement replication works, and even then it takes a lot of tinkering and iterating to get everything working smoothly.
//    This is especially true for really unique and complex movement modes that are vastly different from the normal walking mode.
// 
//    With a Client Authoritative approach, we can essentially modify the movement however we want, simlar to how we could do it for a single player game.
//      Anything from building custom MovementComponents, to just directly setting the Characters location in Blueprints will work perfectly, without having to consider complicated replication issues.
// 
// 3) The drawback is of course that the server is no longer authoritative, which makes it a lot easier for Clients to cheat and so on.
//    Which is only really a problem if you're making a competitive game, in which case you probably shouldn't be using this plugin.
//
// Intended to be used together with UMousenAuthoritativeCharacterMovementComponent & AMousenAuthoritativePlayerController
UCLASS()
class MOUSEN_API AMousenAuthoritativeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMousenAuthoritativeCharacter(const FObjectInitializer& ObjectInitializer);
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;

	UFUNCTION(Server, Unreliable)
	void MousenAuthoritativeMove(const FMousenAuthoritativeMoveData& MoveData);

	void PostNetReceiveBasedMovement(const FMousenAuthoritativeMoveData& MoveData);
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void PostNetReceiveLocationAndRotation() override;
	virtual void OnRep_ReplicatedMovement() override;

	UPROPERTY(Replicated)
	bool bHasOverrideRootMotion;
};
