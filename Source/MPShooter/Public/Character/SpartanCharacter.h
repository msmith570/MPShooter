// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "MPShooter/SpartanTypes/TurningInPlace.h"
#include "SpartanCharacter.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class MPSHOOTER_API ASpartanCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	ASpartanCharacter();
	
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; // (B) This function needs to be called on any class using replication

	virtual void PostInitializeComponents() override;

	// ANIM MONTAGE
	void PlayFireMontage(bool bAiming);


protected:

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = Input)
	UInputMappingContext* SpartanContext;
	UPROPERTY(EditAnywhere, Category = Input)
	UInputAction* MoveAction;
	UPROPERTY(EditAnywhere, Category = Input)
	UInputAction* MouseLookAction;
	UPROPERTY(EditAnywhere, Category = Input)
	UInputAction* JumpAction;
	UPROPERTY(EditAnywhere, Category = Input)
	UInputAction* EquipAction;
	UPROPERTY(EditAnywhere, Category = Input)
	UInputAction* CrouchAction;
	UPROPERTY(EditAnywhere, Category = Input)
	UInputAction* AimAction;
	UPROPERTY(EditAnywhere, Category = Input)
	UInputAction* FireAction;


	void Move(const FInputActionValue& Value);
	void MouseLook(const FInputActionValue& Value);
	void EquipButtonPressed();
	void CrouchButtonPressed();
	void AimButtonPressed();
	void FireButtonPressed();
	void FireButtonReleased();
	virtual void Jump() override;  // Using to override jump function to allow us to stand up while crouched, since jumping doesnt work while crouched.

	// Used to set aim offset variables
	void AimOffset(float DeltaTime);

	



	

private:
	
	UPROPERTY(VisibleAnywhere)
	class UCameraComponent* FollowCamera;
	UPROPERTY(VisibleAnywhere)
	class USpringArmComponent* CameraBoom;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWidgetComponent* OverheadWidget;

	UPROPERTY(ReplicatedUsing = OnRep_OverlappingWeapon)
	class AWeapon* OverlappingWeapon; // (B) Makes this a replicated variable

	UFUNCTION()
	void OnRep_OverlappingWeapon(AWeapon* LastWeapon); // (C) OnRep doesnt replicate Client -> Server, so you wont see the widget on the server, only client, so we need to handle scenario where server is client
														// (E2) Adding input LastWeapon, and added it to our function call.
	UPROPERTY(VisibleAnywhere)
	class UCombatComponent* Combat;

	UFUNCTION(Server, Reliable)  // Remote Procedure Call (Allows client to request server to do an action like pick up weapon.  Server handles/allows the pickup. //Reliable RPC means that confirmation of the packet between server and client will occur.  If it doesnt occur, packet will be reset.  Use sparingly.
	void ServerEquipButtomPressed();

	// Used to set the AO inputs on the character class, which we will make a Getter for for use in the AnimInstance
	float AO_Yaw;
	float Interp_AO_Yaw;
	float AO_Pitch;
	FRotator StartingAimRotation;

	// Use TurningInPlace ENUM for TIP functionality
	ETurningInPlace TurningInPlace;
	void TurnInPlace(float DeltaTime); // we include delta time just incase we want to do some.. interpolating..?

	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* FireWeaponMontage;

public:

	void SetOverlappingWeapon(AWeapon* Weapon); // (B) Public Setter for Overlapping Weapon
	bool IsWeaponEquipped(); 
	bool bIsAiming();
	FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; } // Getter for AO YAW
	FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; } // Getter for Pitch
	AWeapon* GetEquippedWeapon(); // Getter for EquippedWeapon used in FABRIK IK.

	FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; } // Getter for use in AnimInstance



};
