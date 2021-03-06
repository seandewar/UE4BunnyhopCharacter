// Copyright© SD5 - Sean Dewar, 2015.

#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "SD5BunnyGunCharacterMovement.generated.h"

/**
 * The component that contains the movement logic for the Bunny Gun character.
 */
UCLASS()
class SD5BUNNYGUN_API USD5BunnyGunCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USD5BunnyGunCharacterMovement(const FObjectInitializer& ObjectInitializer);

	// Init the component.
	void InitializeComponent() override;

	// Tick the component logic.
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Performs a jump.
	bool DoJump(bool bReplayingMoves) override;

	// Applies trimping velocity.
	void ApplyTrimpingVelocity(const FFindFloorResult& Floor);

#ifdef BG_ENABLE_STAMINA
	// Update the current amount of stamina we have (we decrease it each tick).
	void UpdateStamina(float DeltaTime);
#endif // BG_ENABLE_STAMINA

	// Enables or disables slow walking mode.
	void SetSlowWalking(bool bNewIsSlowWalking);

	// Sets whether or not the character is walking on the server.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetSlowWalking(bool bNewIsSlowWalking);

	// Calc accel, new velo and apply friction.
	void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	// Calc and apply acceleration from movement input to the character.
	void ApplyAcceleration(float DeltaTime, float SurfaceFriction, const FVector& WishDirection, float WishSpeed, float Acceleration);

	// Calc and apply air acceleration from movement input to the character.
	void ApplyAirAcceleration(float DeltaTime, float SurfaceFriction, const FVector& WishDirection, float WishSpeed, float MaxAirWishSpeed, float Acceleration);

	// Calc and apply friction to the character from this frame.
	void ApplyFriction(float DeltaTime, float CharacterFriction, float SurfaceFriction, float StopSpeed);

	// Whether or not friction should be applied this frame.
	bool ShouldApplyGroundFriction();

	// Get the current max speed of the component depending on the current movement mode.
	float GetMaxSpeed() const override;

	// Get the current max accel of the component depending on the current movement mode.
	float GetMaxAcceleration() const override;

	// Whether or not slow walking is enabled for this character.
	UPROPERTY(Category = "Character Movement: Walking", Replicated, EditAnywhere, BlueprintReadWrite)
	uint32 bCanSlowWalk : 1;

	// When the character is slow walking, this number is multiplied by the max speed of the movement mode they are currently in.
	UPROPERTY(Category = "Character Movement: Walking", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bCanSlowWalk", ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float SlowWalkingMaxSpeedMultiplier;

	// Whether or not the character is walking [slowly] (instead of sprinting).
	UPROPERTY(Category = "Character Movement: Walking", VisibleAnywhere, BlueprintReadOnly)
	uint32 bIsSlowWalking : 1;

	// How much time (in seconds) that ground friction should be ignored after landing.
	UPROPERTY(Category = "Character Movement: Bunnyhopping", Replicated, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float NoFrictionAfterLandingTime;

	// Timestamp since character was last in the air.
	float LastAirTimestamp;

	// Whether or not to enforce an absolute maximum speed limit that the player cannot bypass through bunnyhopping or strafe-jumping.
	UPROPERTY(Category = "Character Movement: Bunnyhopping", Replicated, EditAnywhere, BlueprintReadWrite)
	uint32 bUseEnforcedMaxSpeed : 1;

	// If bUseEnforcedMaxSpeed is true, the value of this variable will be used as the enforced maximum speed limit for the player.
	UPROPERTY(Category = "Character Movement: Bunnyhopping", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bUseEnforcedMaxSpeed", ClampMin = "0", UIMin = "0"))
	float EnforcedMaxSpeed;

	// The maximum speed that a player can have while jumping or falling while in the air. Can be bypassed by bunnyhopping or strafe-jumping.
	UPROPERTY(Category = "Character Movement: Jumping / Falling", Replicated, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float MaxFallAirSpeed;

	// The max acceleration gained when jumping / falling in the air.
	UPROPERTY(Category = "Character Movement: Jumping / Falling", Replicated, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float MaxAirAcceleration;

	// If the character's speed is lower than this speed, then friction that is scaled with this speed will be applied instead. This allows for snappier stopping of movement.
	UPROPERTY(Category = "Character Movement: Walking", Replicated, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float StopSpeed;

	// Whether or not trimping should be enabled.
	// Trimping is when you get height gains on ramps facing upwards or speed gains on ramps facing downwards from the player.
	// The amount of speed / height you gain depends on the slope of the incline and the current speed.
	UPROPERTY(Category = "Character Movement: Trimping", Replicated, EditAnywhere, BlueprintReadWrite)
	uint32 bEnableTrimping : 1;

	// If trimping on a slope inclined downwards, this value is used in order to determine the lowest possible jump velocity. The higher this value, the less the min jump velocity.
	UPROPERTY(Category = "Character Movement: Trimping", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnableTrimping", ClampMin = "0", UIMin = "0"))
	float MaxTrimpJumpHeightReductionMultiplier;

	// If trimping on a slope inclined upwards, cap the max vertical velocity you can gain (in the Z direction).
	UPROPERTY(Category = "Character Movement: Trimping", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnableTrimping", ClampMin = "0", UIMin = "0"))
	float MaxTrimpVerticalVelocityBoost;

	// Multiplies the final velocity boost gained by trimping on a slope inclined upwards by this value.
	UPROPERTY(Category = "Character Movement: Trimping", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnableTrimping", ClampMin = "0", UIMin = "0"))
	float TrimpVerticalVelocityBoostMultiplier;

	// If trimping on a slope inclined downwards, cap the max horiz speed you can gain (in the X and Y direction).
	UPROPERTY(Category = "Character Movement: Trimping", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnableTrimping", ClampMin = "0", UIMin = "0"))
	float MaxTrimpHorizSpeedBoost;

	// Multiplies the final speed boost gained by trimping on a slope inclined downwards by this value.
	UPROPERTY(Category = "Character Movement: Trimping", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnableTrimping", ClampMin = "0", UIMin = "0"))
	float TrimpHorizSpeedBoostMultiplier;

#ifdef BG_ENABLE_STAMINA
	// Whether or not the CS-like stamina system should be enabled.
	UPROPERTY(Category = "Character Movement: Stamina", Replicated, EditAnywhere, BlueprintReadWrite)
	uint32 bEnableStamina : 1;

	// The player's current stamina.
	UPROPERTY(Category = "Character Movement: Stamina", Transient, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnableStamina", ClampMin = "0", UIMin = "0"))
	float Stamina;

	// The maximum amount of stamina the player has (the actual value of Stamina will not reflect this value but the calculations use it).
	UPROPERTY(Category = "Character Movement: Stamina", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnableStamina", ClampMin = "0", UIMin = "0"))
	float MaxStamina;

	// The amount of stamina that the player will lose when jumping (the actual value of Stamina will not reflect this value but the calculations use it).
	UPROPERTY(Category = "Character Movement: Stamina", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnableStamina", ClampMin = "0", UIMin = "0"))
	float StaminaJumpCost;

	// The rate in which stamina recovers over time (the actual value of Stamina will not reflect this value but the calculations use it).
	UPROPERTY(Category = "Character Movement: Stamina", Replicated, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnableStamina", ClampMin = "0", UIMin = "0"))
	float StaminaRecoveryRate;
#endif // BG_ENABLE_STAMINA
};