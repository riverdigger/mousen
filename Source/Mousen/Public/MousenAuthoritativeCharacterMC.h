// Copyright 2023 Anton Hesselbom. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MousenAuthoritativeMoveData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MousenAuthoritativeCharacterMC.generated.h"

// Intended to be used together with AMousenAuthoritativeCharacter & AMousenAuthoritativePlayerController
UCLASS()
class MOUSEN_API UMousenAuthoritativeCharacterMC : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UMousenAuthoritativeCharacterMC(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void SimulatedTick(float DeltaSeconds) override;
	virtual void ControlledCharacterMove(const FVector& InputVector, float DeltaSeconds) override;
	virtual void SimulateMovement(float DeltaTime) override;
	virtual void SmoothClientPosition(float DeltaSeconds) override;
	virtual void SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation) override;

	void MousenAuthoritativeMove_ServerReceive(const FMousenAuthoritativeMoveData& MoveData);

	FORCEINLINE const FMousenAuthoritativeMoveData& GetServerLatestMoveData() const { return ServerLatestMoveData; }

private:
	void ReplicateAutonomousMoveToServer();
	float GetMousenAuthoritativeNetSendDeltaTime(const APlayerController* Controller) const;
	void SendMousenAuthoritativeMove();
	FMousenAuthoritativeMoveData GenerateMousenAuthoritativeMoveData();

	float LastClientUpdateTime;

	UPROPERTY()
	FMousenAuthoritativeMoveData ServerLatestMoveData;
};
