// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/SpartanCharacter.h"
#include "Components/InputComponent.h"
#include "EnhancedInput/Public/EnhancedInputSubsystems.h"
#include "EnhancedInput/Public/InputAction.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "MPShooter/Weapon/Weapon.h"
#include "SpartanComponents/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Character/SpartanAnimInstance.h"

#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"


ASpartanCharacter::ASpartanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	TurningInPlace = ETurningInPlace::ETIP_NotTurning; //Set Default Value for ETIP
	NetUpdateFrequency = 66.f; // Sets the net update per second for the class
	MinNetUpdateFrequency = 33.f;  // sets the net update per second for less frequently updated things

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 750.f); //Sets Rotation Rate of character when orient rotation to movement is true (rate at which the character spins around)

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);
}

void ASpartanCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ASpartanCharacter, OverlappingWeapon, COND_OwnerOnly); // (B) Requires Net/UnrealNetwork.h header.  Overlapping is Null until we set it, in the Weapon class on overlap, which means we need a public setter.
																					// Owner only makes it replicate from server, only to client that owns the overlapping pawn.

}

void ASpartanCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(SpartanContext, 0);
		}
	}
	
}

void ASpartanCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AimOffset(DeltaTime);
	
}

void ASpartanCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASpartanCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASpartanCharacter::MouseLook);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ASpartanCharacter::Jump);
		EnhancedInputComponent->BindAction(EquipAction, ETriggerEvent::Triggered, this, &ASpartanCharacter::EquipButtonPressed);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ASpartanCharacter::CrouchButtonPressed);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &ASpartanCharacter::AimButtonPressed);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ASpartanCharacter::FireButtonPressed);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &ASpartanCharacter::FireButtonReleased);
	}
}

void ASpartanCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Combat)
	{
		Combat->Character = this;
	}
}

void ASpartanCharacter::PlayFireMontage(bool bAiming)
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName;
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ASpartanCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(ForwardDirection, MovementVector.Y);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void ASpartanCharacter::MouseLook(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerPitchInput(LookAxisVector.Y);
	AddControllerYawInput(LookAxisVector.X);
}

void ASpartanCharacter::EquipButtonPressed()
{

	if (Combat)
		if (HasAuthority())
		{
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else
		{
			ServerEquipButtomPressed();
		}
}

void ASpartanCharacter::ServerEquipButtomPressed_Implementation()
{
	if (Combat)
		Combat->EquipWeapon(OverlappingWeapon);
}

void ASpartanCharacter::CrouchButtonPressed()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
	
}

void ASpartanCharacter::AimButtonPressed()
{
	if (Combat && !Combat->bAiming && Combat->EquippedWeapon)
	{
		Combat->SetAiming(true);
	}
	else
	{
		Combat->SetAiming(false);
	}
}

void ASpartanCharacter::FireButtonPressed()
{
	if (Combat)
	{
		Combat->FireButtonPressed(true);// Calls function in CombatComponent.cpp
	}
}

void ASpartanCharacter::FireButtonReleased()
{
	if (Combat)
	{
		Combat->FireButtonPressed(false);// Calls function in CombatComponent.cpp
	}
}

void ASpartanCharacter::Jump()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void ASpartanCharacter::AimOffset(float DeltaTime) // Set Aim Offset Parameters and Params for TurningInPlace
{
	if (Combat && Combat->EquippedWeapon == nullptr) return; // early out if we dont have a weapon
	FVector Velocity = GetVelocity(); // Calculated Speed (we took code from Anim.cpp)
	Velocity.Z = 0.f;
	float Speed = Velocity.Size();

	bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir) // standing still, not jumping
	{
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); // Gets us the delta between Starting Im Rotation and current aim rotation
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning) // Used for TIP
		{
			Interp_AO_Yaw = AO_Yaw; // If we are not turning, InterpYaw = AO_Yaw.  See below for next step(XX)
		}
		bUseControllerRotationYaw = true; // Set true when standing still so that TurnInPlace functionality can happen.

		TurnInPlace(DeltaTime); // Set function here because we are already checking standing still/not jumping

	}
	if (Speed > 0.f || bIsInAir) // we use this to store our "starting base yaw" when we stop moving, so we can compare it to our rotation yaw while idle.
	{
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); // Stored every frame while we have a weapon or are in the air (not idle)
		AO_Yaw = 0.f; // keep AO_Yaw at 0 every frame while we are moving.
		bUseControllerRotationYaw = true;

		TurningInPlace = ETurningInPlace::ETIP_NotTurning; // Set TIP so we dont use TIP functionality

	}
	// set Pitch
	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled()) // correct for bitwise operations in pitch inhereint in charactermovementcomponent between server/clients
	{
		// Map pitch from {270, 360) to [-90, 0)  -  From lesson 59 in multiplayer tutorial "Pitch in Multiplayer"
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);

	}

} 

void ASpartanCharacter::TurnInPlace(float DeltaTime)
{
	if (AO_Yaw > 70.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		Interp_AO_Yaw = FMath::FInterpTo(Interp_AO_Yaw, 0.f, DeltaTime, 4.f); // 4.f (Interp Speed) controls the speed at which we turn.
		AO_Yaw = Interp_AO_Yaw; // (XX) If we are turning, we reset AO_Yaw to the interpolated value.
		if (FMath::Abs(AO_Yaw) < 15.f) //  check to see if we've turned enough, and if we have, set TIP to NotTurning.
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); // Now, we have turned enough and can reset our Starting Aim Rotation. 
		}
	}

}

void ASpartanCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}
	OverlappingWeapon = Weapon;
	if (IsLocallyControlled())  // Allows the server to show the widget
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true); 
		}
	}
}

void ASpartanCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{

	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);  // (E3) Functionality to hide widget.
	}

}

bool ASpartanCharacter::IsWeaponEquipped()
{
	return (Combat && Combat-> EquippedWeapon);
}

bool ASpartanCharacter::bIsAiming()
{
	return (Combat && Combat->bAiming);
}

AWeapon* ASpartanCharacter::GetEquippedWeapon()  // Getter for FABRIK IK in AnimInstance
{
	if (Combat == nullptr) return nullptr;

	return Combat->EquippedWeapon;
}

