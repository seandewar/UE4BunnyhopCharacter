// Copyright© SD5 - Sean Dewar, 2015.

#include "SD5BunnyGun.h"
#include "SD5BunnyGunCharacterMovement.h"
#include "SD5BunnyGunCharacter.h"
#include "UnrealNetwork.h"

// !!! Copied from CharacterMovementComponent.cpp !!!
// * * * * *

// Version that does not use inverse sqrt estimate, for higher precision.
FORCEINLINE FVector GetSafeNormalPrecise(const FVector& V)
{
	const auto VSq = V.SizeSquared();
	if (VSq < SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}
	else
	{
		return V * (1.f / FMath::Sqrt(VSq));
	}
}

// Version that does not use inverse sqrt estimate, for higher precision.
FORCEINLINE FVector GetClampedToMaxSizePrecise(const FVector& V, float MaxSize)
{
	if (MaxSize < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const auto VSq = V.SizeSquared();
	if (VSq > FMath::Square(MaxSize))
	{
		return V * (MaxSize / FMath::Sqrt(VSq));
	}
	else
	{
		return V;
	}
}

// * * * * *

void USD5BunnyGunCharacterMovement::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USD5BunnyGunCharacterMovement, bCanSlowWalk);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, SlowWalkingMaxSpeedMultiplier);

	DOREPLIFETIME(USD5BunnyGunCharacterMovement, NoFrictionAfterLandingTime);

	DOREPLIFETIME(USD5BunnyGunCharacterMovement, bUseEnforcedMaxSpeed);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, EnforcedMaxSpeed);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, MaxFallAirSpeed);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, MaxAirAcceleration);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, StopSpeed);

	DOREPLIFETIME(USD5BunnyGunCharacterMovement, bEnableTrimping);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, MaxTrimpJumpHeightReductionMultiplier);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, MaxTrimpVerticalVelocityBoost);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, TrimpVerticalVelocityBoostMultiplier);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, MaxTrimpHorizSpeedBoost);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, TrimpHorizSpeedBoostMultiplier);

#ifdef BG_ENABLE_STAMINA
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, bEnableStamina);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, MaxStamina);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, StaminaJumpCost);
	DOREPLIFETIME(USD5BunnyGunCharacterMovement, StaminaRecoveryRate);
#endif // BG_ENABLE_STAMINA
}

USD5BunnyGunCharacterMovement::USD5BunnyGunCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	MaxFallAirSpeed(100.0f),
	MaxAirAcceleration(20.0f),
	StopSpeed(150.0f),
	bUseEnforcedMaxSpeed(true),
	EnforcedMaxSpeed(3500.0f),
	NoFrictionAfterLandingTime(0.0f),
	bCanSlowWalk(true),
	SlowWalkingMaxSpeedMultiplier(0.3f),
	bIsSlowWalking(false),
	bEnableTrimping(true),
	MaxTrimpJumpHeightReductionMultiplier(0.375f),
	MaxTrimpVerticalVelocityBoost(2000.0f),
	TrimpVerticalVelocityBoostMultiplier(1.25f),
	MaxTrimpHorizSpeedBoost(1000.0f),
	TrimpHorizSpeedBoostMultiplier(2.75f)
#ifdef BG_ENABLE_STAMINA
	,Stamina(0.0f),
	MaxStamina(100.0f),
	StaminaJumpCost(25.0f),
	StaminaRecoveryRate(20.0f),
	bEnableStamina(false) // TODO Predict & Replicate Stamina properly
#endif // BG_ENABLE_STAMINA
{
	// TODO SUPPORT SWIMMING
	GetNavAgentPropertiesRef().bCanSwim = false;

	// Ensure replication
	bReplicates = true;

	// Set the player's capsule crouched half height.
	CrouchedHalfHeight = 52.0f;

	// Allow full control of the character when looking while in the air.
	AirControl = 1.0f;
	AirControlBoostMultiplier = 1.0f;
	AirControlBoostVelocityThreshold = 0.0f;

	// This character can crouch.
	GetNavAgentPropertiesRef().bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;

	// Can walk on slopes inclined at a max angle of 45 deg
	SetWalkableFloorAngle(45.0f);

	// Use a flat base for floor checks so that the character cannot "hang" off ledges because of the
	// roundish base that the capsule uses.
	bUseFlatBaseForFloorChecks = true;

	// Ground accel.
	MaxAcceleration = 6.0f;

	MaxWalkSpeed = 700.0f;
	GroundFriction = 5.0f;
	JumpZVelocity = 380.0f;
}

void USD5BunnyGunCharacterMovement::InitializeComponent()
{
	Super::InitializeComponent();
}

void USD5BunnyGunCharacterMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// If we are falling / jumping in the air, we need to update the LastAirTimestamp to this time.
	if (IsFalling())
	{
		LastAirTimestamp = GetWorld()->GetTimeSeconds();
	}

	// Check whether or not we should be able to slow walk...
	if (!bCanSlowWalk && bIsSlowWalking)
	{
		bIsSlowWalking = false;
	}

#ifdef BG_ENABLE_STAMINA
	// Update our stamina if necessary (function checks for bEnableStamina).
	UpdateStamina(DeltaTime);
#endif // BG_ENABLE_STAMINA

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USD5BunnyGunCharacterMovement::ApplyTrimpingVelocity(const FFindFloorResult& Floor)
{
	// Apply trimping if we were on an incline.
	if (bEnableTrimping && Floor.bBlockingHit)
	{
		const auto HorizVelocity = FVector(Velocity.X, Velocity.Y, 0.0f);
		const auto HorizSpeed = HorizVelocity.Size();

		// Check that we actually have some speed in horiz direction.
		if (HorizSpeed > SMALL_NUMBER)
		{
			const auto FloorNormal = Floor.HitResult.ImpactNormal;
			const auto HorizVeloDirection = GetSafeNormalPrecise(HorizVelocity);

			// Get the angle between the normal of the slope and the horizontal direction of our velocity.
			// If slope is inclined upwards from our horiz velo direction, angle will be > pi/2
			// If perpendicular, angle = pi/2 aka 90 deg (dot product = 0)
			// If slope is inclined downwards from our horiz velo direction, angle will be < pi/2
			// Range of acos is between 0 & pi. 
			const auto Angle = FMath::Acos(FVector::DotProduct(FloorNormal, HorizVeloDirection));

			// If slope is inclined upwards from us. Give a height boost!
			// If the slope is inclined downwards, we'll lose some speed instead.
			const auto VerticalBoostSlopeMultiplier = ((2.0f * Angle) / PI) - 1.0f;
			auto VerticalVelocityBoost = FMath::Min(HorizSpeed * VerticalBoostSlopeMultiplier * TrimpVerticalVelocityBoostMultiplier, MaxTrimpVerticalVelocityBoost);
			VerticalVelocityBoost = FMath::Max(JumpZVelocity * -MaxTrimpJumpHeightReductionMultiplier, VerticalVelocityBoost);

			Velocity.Z += VerticalVelocityBoost;

			// If on a slope inclined below us, apply a horizontal speed boost too.
			if (Angle < (PI * 0.5f))
			{
				// Slope is inclined below us. Start to give a horizontal speed boost.
				const auto HorizBoostSlopeMultiplier = 1.0f - ((2.0f * Angle) / PI);
				const auto HorizSpeedBoost = FMath::Min(HorizSpeed * HorizBoostSlopeMultiplier * TrimpHorizSpeedBoostMultiplier, MaxTrimpHorizSpeedBoost);
				const auto HorizVelocityBoost = HorizVeloDirection * HorizSpeedBoost;

				Velocity.X += HorizVelocityBoost.X;
				Velocity.Y += HorizVelocityBoost.Y;
			}
		}
	}
}

bool USD5BunnyGunCharacterMovement::DoJump(bool bReplayingMoves)
{
	const auto Floor = CurrentFloor;
	const auto bJumped = Super::DoJump(bReplayingMoves);

	// If we successfully jumped
	if (bJumped)
	{
		// This function will apply trimp velo if enabled.
		ApplyTrimpingVelocity(Floor);

#ifdef BG_ENABLE_STAMINA
		// Apply stamina if enabled.
		if (bEnableStamina)
		{
			if (Stamina > 0.0f)
			{
				// Mimic Counter-Strike's stamina logic for jumping & apply it to the vertical component of our velocity.
				const auto VerticalSpeedMultiplier = (MaxStamina - ((Stamina / 1000.0f) * StaminaRecoveryRate)) / MaxStamina;
				Velocity.Z *= VerticalSpeedMultiplier;
			}

			Stamina = (StaminaJumpCost * 1000.0f) / StaminaRecoveryRate;
		}
#endif // BG_ENABLE_STAMINA
	}

	return bJumped;
}

#ifdef BG_ENABLE_STAMINA
void USD5BunnyGunCharacterMovement::UpdateStamina(float DeltaTime)
{
	if (!bEnableStamina)
	{
		return;
	}

	if (Stamina > 0.0f)
	{
		Stamina -= 1000.0f * DeltaTime;
		Stamina = FMath::Max(0.0f, Stamina);
	}
}
#endif // BG_ENABLE_STAMINA

float USD5BunnyGunCharacterMovement::GetMaxSpeed() const
{
	auto Speed = 0.0f;

	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
	// NOTE Falling uses the walking speed, as MaxAirSpeed is used seperately by ApplyAirAcceleration()
	case MOVE_Falling:
		Speed = (IsCrouching() ? MaxWalkSpeedCrouched : MaxWalkSpeed);
		break;

	case MOVE_Swimming:
		Speed = MaxSwimSpeed;
		break;

	case MOVE_Flying:
		Speed = MaxFlySpeed;
		break;

	case MOVE_Custom:
		Speed = MaxCustomMovementSpeed;
		break;

	case MOVE_None:
	default:
		Speed = 0.f;
		break;
	}

	if (bIsSlowWalking)
	{
		Speed *= SlowWalkingMaxSpeedMultiplier;
	}

	return Speed;
}

void USD5BunnyGunCharacterMovement::SetSlowWalking(bool bNewIsSlowWalking)
{
	if (!bCanSlowWalk)
	{
		return;
	}

	// Make the character start slow walking on the client.
	bIsSlowWalking = bNewIsSlowWalking;

	// And then notify the server.
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerSetSlowWalking(bIsSlowWalking);
	}
}

void USD5BunnyGunCharacterMovement::ServerSetSlowWalking_Implementation(bool bNewIsSlowWalking)
{
	if (!bCanSlowWalk)
	{
		return;
	}

	bIsSlowWalking = bNewIsSlowWalking;
}

bool USD5BunnyGunCharacterMovement::ServerSetSlowWalking_Validate(bool bNewIsSlowWalking)
{
	return true;
}

float USD5BunnyGunCharacterMovement::GetMaxAcceleration() const
{
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
	case MOVE_Swimming:
	case MOVE_Custom:
		return MaxAcceleration;

	case MOVE_Falling:
	case MOVE_Flying:
		return MaxAirAcceleration;

	case MOVE_None:
	default:
		return 0.f;
	}
}

void USD5BunnyGunCharacterMovement::ApplyAcceleration(float DeltaTime, float SurfaceFriction, const FVector& WishDirection, float WishSpeed, float Acceleration)
{
	const auto VeloProj = FVector::DotProduct(Velocity, WishDirection);
	const auto AddSpeed = WishSpeed - VeloProj;
	if (AddSpeed <= 0.0f)
	{
		return;
	}

	auto AccelSpeed = Acceleration * WishSpeed * SurfaceFriction * DeltaTime;
	AccelSpeed = FMath::Min(AccelSpeed, AddSpeed);

	Velocity += AccelSpeed * WishDirection;
}

void USD5BunnyGunCharacterMovement::ApplyAirAcceleration(float DeltaTime, float SurfaceFriction, const FVector& WishDirection, float WishSpeed, float MaxAirWishSpeed, float Acceleration)
{
	const auto AirWishSpeed = FMath::Min(WishSpeed, MaxAirWishSpeed);

	const auto VeloProj = FVector::DotProduct(Velocity, WishDirection);
	const auto AddSpeed = AirWishSpeed - VeloProj;
	if (AddSpeed <= 0.0f)
	{
		return;
	}

	auto AccelSpeed = Acceleration * WishSpeed * SurfaceFriction * DeltaTime;
	AccelSpeed = FMath::Min(AccelSpeed, AddSpeed);

	Velocity += AccelSpeed * WishDirection;
}

void USD5BunnyGunCharacterMovement::ApplyFriction(float DeltaTime, float CharacterFriction, float SurfaceFriction, float StopSpeed)
{
	const auto Speed = Velocity.Size();
	// Check if the speed is too small to care about...
	if (Speed < 10.0f)
	{
		Velocity = FVector::ZeroVector;
		return;
	}

	auto ControlSpeed = FMath::Max(StopSpeed, Speed);
	auto SpeedDrop = ControlSpeed * CharacterFriction * SurfaceFriction * DeltaTime;

	Velocity *= FMath::Max(0.0f, Speed - SpeedDrop) / Speed;
}

bool USD5BunnyGunCharacterMovement::ShouldApplyGroundFriction()
{
	return (IsMovingOnGround() && // If moving on the ground
		GetWorld()->TimeSeconds >= LastAirTimestamp + NoFrictionAfterLandingTime // If timeframe for ignoring friction after landing has expired.
		);
}

void USD5BunnyGunCharacterMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Do not update velocity when using root motion
	if (!HasValidData() || HasRootMotion() || DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	// NOTE Restore the last movement mode so that friction and accel uses falling / jumping values
	// for at least one extra frame so that bunnyhopping works without "jerking" as soon as the player
	// hits the ground and quickly jumps again.
	static auto LastCalcVeloMovementMode = MOVE_None;
	const auto CurrentMovementMode = MovementMode;
	if (LastCalcVeloMovementMode == MOVE_Falling)
	{
		// NOTE Not using SetMovementMode() so that other code doesn't get notified.
		MovementMode = LastCalcVeloMovementMode;
	}

	// Ensure that friction isn't negative.
	Friction = FMath::Max(0.f, Friction);
	const auto MaxAccel = GetMaxAcceleration();
	auto MaxSpeed = GetMaxSpeed();
	
	// Check if path following requested movement
	auto bZeroRequestedAcceleration = true;
	auto RequestedAcceleration = FVector::ZeroVector;
	auto RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		RequestedAcceleration = RequestedAcceleration.GetClampedToMaxSize(MaxAccel);
		bZeroRequestedAcceleration = false;
	}

#ifdef BG_ENABLE_STAMINA
	// If stamina is enabled...
	if (bEnableStamina)
	{
		// Walking stamina logic.
		if (IsMovingOnGround() && Stamina > 0.0f)
		{
			// Mimic Counter-Strike's stamina logic for walking.
			const auto HorizSpeedMultiplierExponent = 70.0f * DeltaTime;
			const auto HorizSpeedMultiplier = FMath::Pow((MaxStamina - ((Stamina / 1000.0f) * StaminaRecoveryRate)) / MaxStamina, HorizSpeedMultiplierExponent);

			// Apply to the horizontal axis of our velocity only.
			Velocity.X *= HorizSpeedMultiplier;
			Velocity.Y *= HorizSpeedMultiplier;
		}
	}
#endif // BG_ENABLE_STAMINA

	// Check if we are forcing the player to move at the max possible acceleration for the current move mode...
	// (aka walking automatically).
	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > SMALL_NUMBER)
		{
			Acceleration = GetSafeNormalPrecise(Acceleration) * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : GetSafeNormalPrecise(Velocity));
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	MaxSpeed = FMath::Max(RequestedSpeed, MaxSpeed * AnalogInputModifier);
	auto Speed = Velocity.Size();

	// Apply fluid friction if necessary
	if (bFluid)
	{
		Velocity *= (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}

	// Get size and direction of acceleration.
	const auto AccelDirection = GetSafeNormalPrecise(Acceleration);
	const auto AccelAmount = Acceleration.Size();

	// Apply ground friction first
	if (ShouldApplyGroundFriction())
	{
		ApplyFriction(DeltaTime, Friction, 1.0f, StopSpeed);
	}

	// Apply acceleration for movement.
	if (IsMovingOnGround())
	{
		ApplyAcceleration(DeltaTime, 1.0f, AccelDirection, MaxSpeed, AccelAmount);
	}
	else if (IsFalling())
	{
		ApplyAirAcceleration(DeltaTime, 1.0f, AccelDirection, MaxSpeed, MaxFallAirSpeed, AccelAmount);
	}

	// Apply path following acceleration & clamp speed to enforced speed limit if enabled (regardless if player is bhopping or strafing).
	Velocity += RequestedAcceleration * DeltaTime;
	if (bUseEnforcedMaxSpeed)
	{
		Velocity = GetClampedToMaxSizePrecise(Velocity, EnforcedMaxSpeed);
	}

	// Restore the current movement mode again.
	if (LastCalcVeloMovementMode == MOVE_Falling)
	{
		// NOTE Not using SetMovementMode() so that other code doesn't get notified.
		MovementMode = CurrentMovementMode;
	}
	LastCalcVeloMovementMode = MovementMode;

	// Check if RVO avoidance is enabled for AI movement & calculate the required avoidance velo from it.
	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}
}