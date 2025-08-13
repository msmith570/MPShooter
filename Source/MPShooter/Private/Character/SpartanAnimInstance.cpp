// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/SpartanAnimInstance.h"
#include "Character/SpartanCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "MPShooter/Weapon/Weapon.h"

void USpartanAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	SpartanCharacter = Cast<ASpartanCharacter>(TryGetPawnOwner());
}

void USpartanAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);
	if (SpartanCharacter == nullptr)
	{
		SpartanCharacter = Cast<ASpartanCharacter>(TryGetPawnOwner());
	}
	if (SpartanCharacter == nullptr) return;
	{
	
		UpdateMovementState();
		UpdateWeaponState();
		UpdateCharacterLean(DeltaTime);
		CalculateYawOffset(DeltaTime);
		UpdateIKState();

		// Call Getters from SpartanCharacter to get Yaw and Pitch for use in ABP.
		AO_Yaw = SpartanCharacter->GetAO_Yaw();
		AO_Pitch = SpartanCharacter->GetAO_Pitch();

	}
}

void USpartanAnimInstance::UpdateIKState()
{

	if (bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetWeaponMesh() && SpartanCharacter->GetMesh())

	{
		LeftHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("LeftHandSocket"), ERelativeTransformSpace::RTS_World);
		FVector OutPosition;
		FRotator OutRotation;// these two variables will be populated with data when we call TrnasformToBoneSpace (since they are const references?)
		SpartanCharacter->GetMesh()->TransformToBoneSpace(FName("hand_r"), LeftHandTransform.GetLocation(), FRotator::ZeroRotator, OutPosition, OutRotation); // Gives us values for position of left hand relative to hand_r.
		LeftHandTransform.SetLocation(OutPosition); // Now that we have the correct position and rotation in bone space, we can set location and rotation of our left hand transform.
		LeftHandTransform.SetRotation(FQuat(OutRotation)); // after this logic, we create a state machine for FABRIK IK logic.
		
	}
	
}

void USpartanAnimInstance::UpdateMovementState()
{
	bIsInAir = SpartanCharacter->GetCharacterMovement()->IsFalling();
	bIsAccelerating = SpartanCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false; //turnary operator.
	bIsCrouched = SpartanCharacter->bIsCrouched;
	FVector Velocity = SpartanCharacter->GetVelocity();
	Velocity.Z = 0.f;
	Speed = Velocity.Size();
	TurningInPlace = SpartanCharacter->GetTurningInPlace(); // Call getter to update TurningInPlace variable in the Animinstance.
	
}

void USpartanAnimInstance::UpdateWeaponState()
{
	bWeaponEquipped = SpartanCharacter->IsWeaponEquipped();
	EquippedWeapon = SpartanCharacter->GetEquippedWeapon(); // using Getter from Character for Fabrik IK variable
	bAiming = SpartanCharacter->bIsAiming();

	if (EquippedWeapon == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("nullptr in equipepd weapon"));
	}
}

void USpartanAnimInstance::UpdateCharacterLean(float DeltaTime)
{
	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = SpartanCharacter->GetActorRotation();
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
	const float Target = Delta.Yaw / DeltaTime; // Scale Delta up (since its a small value), and tie it to DeltaTime.
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f); // Interp to remove jankiness in lean motion (rapidly moving from one anim to another (left/right strafe)
	Lean = FMath::Clamp(Interp, -90.f, 90.f);
}

void USpartanAnimInstance::CalculateYawOffset(float DeltaTime)
{
	FRotator AimRotation = SpartanCharacter->GetBaseAimRotation();
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(SpartanCharacter->GetVelocity());
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaTime, 6.f);
	YawOffset = DeltaRotation.Yaw;
	CorrectiveRate = (FMath::Abs(YawOffset) > 45.f) ? FMath::Clamp(45.f / FMath::Abs(YawOffset), 0.5f, 1.0f) : 1.0f;
	
}
