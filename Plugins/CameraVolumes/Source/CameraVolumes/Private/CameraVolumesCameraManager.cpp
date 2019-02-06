//Dmitriy Barannik aka redbox, 2019

#include "CameraVolumesCameraManager.h"
#include "CameraVolumesCharacterInterface.h"
#include "GameFramework/PlayerController.h"

ACameraVolumesCameraManager::ACameraVolumesCameraManager()
{
	bUpdateCamera = true;
	bCheckCameraVolumes = true;
	CameraVolumePrevious = nullptr;
	CameraVolumeCurrent = nullptr;
	OldCameraLocation = FVector::ZeroVector;
	NewCameraLocation = FVector::ZeroVector;
	OldCameraRotation = FQuat();
	NewCameraRotation = FQuat();
	OldCameraFOV = 90.f;
	NewCameraFOV = 90.f;
	bNeedsSmoothTransition = false;
	bNeedsCutTransition = false;
}

void ACameraVolumesCameraManager::SetUpdateCamera(bool bNewUpdateCamera)
{
	bUpdateCamera = bNewUpdateCamera;
}

void ACameraVolumesCameraManager::SetCheckCameraVolumes(bool bNewCheck)
{
	bCheckCameraVolumes = bNewCheck;
}

void ACameraVolumesCameraManager::UpdateCamera(float DeltaTime)
{
	if (bUpdateCamera)
	{
		APawn* PlayerPawn = GetOwningPlayerController()->GetPawn();
		if (PlayerPawn)
		{
			ICameraVolumesCharacterInterface* PlayerCharacter = Cast<ICameraVolumesCharacterInterface>(PlayerPawn);
			if (PlayerCharacter)
			{
				CameraComponent = PlayerCharacter->GetCameraComponent();
				PlayerPawnLocation = PlayerPawn->GetActorLocation();

				// Prepare params
				CameraVolumePrevious = CameraVolumeCurrent;
				CameraVolumeCurrent = nullptr;

				OldCameraLocation = NewCameraLocation;
				OldCameraRotation = NewCameraRotation;
				OldCameraFOV = NewCameraFOV;
				NewCameraLocation = PlayerPawnLocation + CameraComponent->DefaultCameraLocation;
				NewCameraRotation = CameraComponent->DefaultCameraRotation;
				NewCameraFOV = CameraComponent->DefaultCameraFieldOfView;

				if (bCheckCameraVolumes)
				{
					// Try to get overlapping camera volumes stored in camera component
					if (CameraComponent->OverlappingCameraVolumes.Num() > 0)
						CameraVolumeCurrent = UCameraVolumesFunctionLibrary::GetCurrentCameraVolume(CameraComponent->OverlappingCameraVolumes, PlayerPawnLocation);
					// Try to get camera volumes from actors overlapping pawn
					else
					{
						OverlappingActors.Empty();
						PlayerPawn->GetOverlappingActors(OverlappingActors, ACameraVolumeActor::StaticClass());
						if (OverlappingActors.Num() > 0)
						{
							TArray<ACameraVolumeActor*> OverlappingCameraVolumes;
							for (AActor* Actor : OverlappingActors)
							{
								ACameraVolumeActor* CameraVolume = Cast<ACameraVolumeActor>(Actor);
								if (CameraVolume)
									OverlappingCameraVolumes.Add(CameraVolume);
							}
							CameraVolumeCurrent = UCameraVolumesFunctionLibrary::GetCurrentCameraVolume(OverlappingCameraVolumes, PlayerPawnLocation);
						}
						else
							bCheckCameraVolumes = false;
						// There are no camera volumes overlapping character at this time,
						// so we don't need this check until player pawn overlap some camera volume again.
					}

					if (CameraVolumeCurrent)
					{
						if (CameraVolumeCurrent != CameraVolumePrevious) // Do we changed to another volume?
						{
							// Check for dynamic camera volumes
							ACameraVolumeDynamicActor* CameraVolumeCurrentDynamic = Cast<ACameraVolumeDynamicActor>(CameraVolumeCurrent);
							if (CameraVolumeCurrentDynamic)
								CameraVolumeCurrentDynamic->SetActive(true);
							ACameraVolumeDynamicActor* CameraVolumePreviousDynamic = Cast<ACameraVolumeDynamicActor>(CameraVolumePrevious);
							if (CameraVolumePreviousDynamic)
								CameraVolumePreviousDynamic->SetActive(false);

							ESide PassedSideCurrent;
							PassedSideCurrent = CameraVolumeCurrent->GetNearestVolumeSide(PlayerPawnLocation);

							if (CameraVolumePrevious)
							{
								ESide PassedSidePrevious;
								PassedSidePrevious = CameraVolumePrevious->GetNearestVolumeSide(PlayerPawnLocation);

								if (UCameraVolumesFunctionLibrary::CompareSidesPairs(PassedSideCurrent, PassedSidePrevious, CameraVolumePrevious->bUse6DOFVolume))
									// We've passed to nearby volume
									// Use settings of side we have passed to
									SetTransitionBySideInfo(CameraVolumeCurrent, PassedSideCurrent);
								else
								{
									// we've passed to volume with another priority
									if (CameraVolumeCurrent->Priority > CameraVolumePrevious->Priority)
										// Use settings of side we have passed to
										SetTransitionBySideInfo(CameraVolumeCurrent, PassedSideCurrent);
									else
										// Use settings of side we have passed from
										SetTransitionBySideInfo(CameraVolumePrevious, PassedSidePrevious);
								}
							}
							else
								// We've passed from void to volume
								SetTransitionBySideInfo(CameraVolumeCurrent, PassedSideCurrent);
						}

						CalcNewCameraParams(CameraVolumeCurrent, DeltaTime);
					}
					else if (CameraVolumePrevious) // Do we passed from volume to void?
					{
						// Check for dynamic camera volumes
						ACameraVolumeDynamicActor* CameraVolumePreviousDynamic = Cast<ACameraVolumeDynamicActor>(CameraVolumePrevious);
						if (CameraVolumePreviousDynamic)
							CameraVolumePreviousDynamic->SetActive(false);

						// Use settings of side we've passed from
						ESide PassedSidePrevious = CameraVolumePrevious->GetNearestVolumeSide(PlayerPawnLocation);
						SetTransitionBySideInfo(CameraVolumePrevious, PassedSidePrevious);
						CalcNewCameraParams(nullptr, DeltaTime);
					}
					else
						CalcNewCameraParams(nullptr, DeltaTime);
				}
				else
					CalcNewCameraParams(nullptr, DeltaTime);

				CameraComponent->UpdateCamera(NewCameraLocation, NewCameraRotation, NewCameraFOV);
			}
		}
	}

	Super::UpdateCamera(DeltaTime);
}

void ACameraVolumesCameraManager::CalcNewCameraParams(ACameraVolumeActor* CameraVolume, float DeltaTime)
{
	if (CameraVolume)
	{
		// FOV
		if (CameraVolume->bOverrideCameraFieldOfView)
			NewCameraFOV = CameraVolume->CameraFieldOfView;

		// Get screen (or at least player camera) aspect ratio for further calculations
		float PlayerCamFOVTangens = FMath::Tan(FMath::DegreesToRadians(NewCameraFOV * 0.5f));
		float ScreenAspectRatio;
		if (GEngine)
		{
			const FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
			ScreenAspectRatio = ViewportSize.X / ViewportSize.Y;
		}
		else
			ScreenAspectRatio = CameraComponent->AspectRatio;

		// Location and Rotation
		if (CameraVolume->GetIsCameraStatic())
		{
			NewCameraLocation = CameraVolume->GetActorLocation() + CameraVolume->CameraLocation;

			if (CameraVolume->bFocalPointIsPlayer)
				NewCameraRotation = UCameraVolumesFunctionLibrary::CalculateCameraRotationToCharacter(CameraVolume->CameraLocation, CameraVolume->CameraFocalPoint, CameraVolume->CameraRoll, PlayerPawnLocation, CameraVolume->GetActorLocation());
			else
				NewCameraRotation = CameraVolume->CameraRotation;
		}
		else
		{
			// Camera offset is always relative to CameraVolume->GetActorLocation().Y
			float CameraOffset;

			if (CameraVolume->bOverrideCameraLocation)
				NewCameraLocation = PlayerPawnLocation + CameraVolume->CameraLocation;

			if (CameraVolume->bCameraLocationRelativeToVolume)
				CameraOffset = CameraVolume->CameraLocation.Y; //Side-scroller
				//CameraOffset = CameraVolume->CameraLocation.Z; //Top-down
			else
				CameraOffset = NewCameraLocation.Y - CameraVolume->GetActorLocation().Y; //Side-scroller
				//CameraOffset = NewCameraLocation.Z - CameraVolume->GetActorLocation().Z; //Top-down

			FVector NewCamVolExtentCorrected = CameraVolume->CamVolExtentCorrected;
			FVector NewCamVolWorldMinCorrected = CameraVolume->CamVolWorldMinCorrected;
			FVector NewCamVolWorldMaxCorrected = CameraVolume->CamVolWorldMaxCorrected;
			// Calculate delta volume extent with +Y volume extent (+Z in top-down)
			if (!CameraVolume->bUseZeroDepthExtent)
			{
				FVector DeltaExtent = FVector::ZeroVector;
				//Side-scroller
				DeltaExtent.X = FMath::Abs((NewCamVolWorldMaxCorrected.Y - CameraVolume->GetActorLocation().Y) * PlayerCamFOVTangens);
				DeltaExtent = FVector(DeltaExtent.X, 0.f, DeltaExtent.X / ScreenAspectRatio);
				//Top-down
				//DeltaExtent.X = FMath::Abs((NewCamVolWorldMaxCorrected.Z - CameraVolume->GetActorLocation().Z) * PlayerCamFOVTangens);
				//DeltaExtent = FVector(DeltaExtent.X, DeltaExtent.X / ScreenAspectRatio, 0.f);

				NewCamVolExtentCorrected += DeltaExtent;
				NewCamVolWorldMinCorrected -= DeltaExtent;
				NewCamVolWorldMaxCorrected += DeltaExtent;
			}

			if (CameraVolume->CamVolAspectRatio >= ScreenAspectRatio) // Horizontal movement
			{
				CameraOffset = FMath::Clamp(CameraOffset, CameraOffset, NewCamVolExtentCorrected.Z * ScreenAspectRatio / PlayerCamFOVTangens); //Side-scroller
				//CameraOffset = FMath::Clamp(CameraOffset, CameraOffset, NewCamVolExtentCorrected.Y * ScreenAspectRatio / PlayerCamFOVTangens); //Top-down
			}
			else // Vertical movement
				CameraOffset = FMath::Clamp(CameraOffset, CameraOffset, NewCamVolExtentCorrected.X / PlayerCamFOVTangens);

			// Calculate screen world extent at depth (CameraOffset)
			FVector ScreenExtent = FVector::ZeroVector;
			ScreenExtent.X = FMath::Abs(CameraOffset * PlayerCamFOVTangens);
			ScreenExtent = FVector(ScreenExtent.X, 0.f, ScreenExtent.X / ScreenAspectRatio); //Side-scroller
			//ScreenExtent = FVector(ScreenExtent.X, ScreenExtent.X / ScreenAspectRatio, 0.f); //Top-down
			FVector ScreenWorldMin = NewCamVolWorldMinCorrected + ScreenExtent;
			FVector ScreenWorldMax = NewCamVolWorldMaxCorrected - ScreenExtent;

			// New camera location
			//Side-scroller
			NewCameraLocation = FVector(
				FMath::Clamp(NewCameraLocation.X, ScreenWorldMin.X, ScreenWorldMax.X),
				CameraOffset + CameraVolume->GetActorLocation().Y,
				FMath::Clamp(NewCameraLocation.Z, ScreenWorldMin.Z, ScreenWorldMax.Z));
			//Top-down
			//NewCameraLocation = FVector(
			//	FMath::Clamp(NewCameraLocation.X, ScreenWorldMin.X, ScreenWorldMax.X),
			//	FMath::Clamp(NewCameraLocation.Y, ScreenWorldMin.Y, ScreenWorldMax.Y),
			//	CameraOffset + CameraVolume->GetActorLocation().Z);

			// New camera rotation
			NewCameraRotation = UCameraVolumesFunctionLibrary::CalculateCameraRotation(CameraVolume->CameraLocation, CameraVolume->CameraFocalPoint, CameraVolume->CameraRoll);
		}
	}

	if (CameraComponent->bUseAdditionalCameraParams)
	{
		NewCameraLocation += CameraComponent->AdditionalCameraLocation;
		NewCameraRotation *= CameraComponent->AdditionalCameraRotation.Quaternion();
		NewCameraFOV += CameraComponent->AdditionalCameraFOV;
	}

	if (bNeedsSmoothTransition)
	{
		SmoothTransitionAlpha += DeltaTime * SmoothTransitionSpeed;
		if (SmoothTransitionAlpha <= 1.f)
		{
			NewCameraLocation = FMath::Lerp(OldCameraLocation, NewCameraLocation, SmoothTransitionAlpha);
			NewCameraRotation = FQuat::Slerp(OldCameraRotation, NewCameraRotation, SmoothTransitionAlpha);
			NewCameraFOV = FMath::Lerp(OldCameraFOV, NewCameraFOV, SmoothTransitionAlpha);
		}
		else
		{
			SmoothTransitionAlpha = 0.f;
			bNeedsSmoothTransition = false;
		}
	}
	else if (bNeedsCutTransition)
	{
		bNeedsSmoothTransition = false;
		bNeedsCutTransition = false;
	}
	else
	{
		if (CameraComponent->bEnableCameraLocationLag)
			NewCameraLocation = FMath::VInterpTo(OldCameraLocation, NewCameraLocation, DeltaTime, CameraComponent->CameraLocationLagSpeed);
		if (CameraComponent->bEnableCameraRotationLag)
			NewCameraRotation = FMath::QInterpTo(OldCameraRotation, NewCameraRotation, DeltaTime, CameraComponent->CameraRotationLagSpeed);
		if (CameraComponent->bEnableCameraFOVInterpolation)
			NewCameraFOV = FMath::FInterpTo(OldCameraFOV, NewCameraFOV, DeltaTime, CameraComponent->CameraFOVInterpolationSpeed);
	}
}

void ACameraVolumesCameraManager::SetTransitionBySideInfo(ACameraVolumeActor* CameraVolume, ESide Side)
{
	FSideInfo SideInfo = CameraVolume->GetSideInfo(Side);
	if (SideInfo.SideTransitionType == ESideTransitionType::ESTT_Smooth)
	{
		bNeedsSmoothTransition = true;
		SmoothTransitionAlpha = 0.f;
		SmoothTransitionSpeed = CameraVolume->CameraSmoothTransitionSpeed;
	}
	else if (SideInfo.SideTransitionType == ESideTransitionType::ESTT_Cut)
	{
		bNeedsCutTransition = true;
		bNeedsSmoothTransition = false;
		SmoothTransitionAlpha = 0.f;
	}
	else
	{
		bNeedsSmoothTransition = false;
		SmoothTransitionAlpha = 0.f;
		bNeedsCutTransition = false;
	}
}