//redbox, 2019

#include "CameraVolumesCameraManager.h"
#include "CameraVolumesCharacterInterface.h"
#include "CameraVolumeDynamicActor.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

ACameraVolumesCameraManager::ACameraVolumesCameraManager()
{
	bUpdateCamera = true;
	bCheckCameraVolumes = true;
	bPerformBlockingCalculations = true;
	PlayerPawnLocationOld = FVector::ZeroVector;
	PlayerPawnLocation = FVector::ZeroVector;
	CameraVolumePrevious = nullptr;
	CameraVolumeCurrent = nullptr;
	OldCameraLocation = FVector::ZeroVector;
	NewCameraLocation = FVector::ZeroVector;
	NewCameraFocalPoint = FVector::ZeroVector;
	OldCameraRotation = FQuat();
	NewCameraRotation = FQuat();
	OldCameraFOV_OW = 90.f;
	NewCameraFOV_OW = 90.f;
	bIsCameraStatic = false;
	bIsCameraOrthographic = false;
	bNeedsSmoothTransition = false;
	bNeedsCutTransition = false;
	bBroadcastOnCameraVolumeChanged = false;
	bUsePlayerPawnControlRotation = false;
}

void ACameraVolumesCameraManager::SetUpdateCamera(bool bNewUpdateCamera)
{
	bUpdateCamera = bNewUpdateCamera;
}

void ACameraVolumesCameraManager::SetCheckCameraVolumes(bool bNewCheck)
{
	if (bCheckCameraVolumes != bNewCheck)
	{
		bCheckCameraVolumes = bNewCheck;

		if (!bCheckCameraVolumes)
		{
			APawn* OwningPawn = GetOwningPlayerController()->GetPawn();
			if (OwningPawn)
			{
				ICameraVolumesCharacterInterface* PlayerCharacter = Cast<ICameraVolumesCharacterInterface>(PlayerPawn);
				if (PlayerCharacter)
				{
					UCameraVolumesCameraComponent* CamComp = PlayerCharacter->GetCameraComponent();
					CamComp->OverlappingCameraVolumes.Empty();
				}
			}
		}
	}
}

void ACameraVolumesCameraManager::SetPerformBlockingCalculations(bool bNewPerformBlockingCalculations)
{
	bPerformBlockingCalculations = bNewPerformBlockingCalculations;
}

void ACameraVolumesCameraManager::UpdateCamera(float DeltaTime)
{
	if (bUpdateCamera)
	{
		PlayerPawn = GetOwningPlayerController()->GetPawn();
		if (PlayerPawn)
		{
			ICameraVolumesCharacterInterface* PlayerCharacter = Cast<ICameraVolumesCharacterInterface>(PlayerPawn);
			if (PlayerCharacter)
			{
				// Prepare params
				CameraVolumePrevious = CameraVolumeCurrent;
				CameraVolumeCurrent = nullptr;
				CameraComponent = PlayerCharacter->GetCameraComponent();
				bIsCameraOrthographic = CameraComponent->GetIsCameraOrthographic();
				PlayerPawnLocationOld = PlayerPawnLocation;
				PlayerPawnLocation = PlayerPawn->GetActorLocation();
				OldCameraLocation = NewCameraLocation;
				OldCameraRotation = NewCameraRotation;
				NewCameraLocation = PlayerPawnLocation + CameraComponent->DefaultCameraLocation;
				NewCameraRotation = CameraComponent->DefaultCameraRotation;
				NewCameraFocalPoint = PlayerPawnLocation + CameraComponent->DefaultCameraFocalPoint;
				bIsCameraStatic = false;
				OldCameraFOV_OW = NewCameraFOV_OW;
				bBroadcastOnCameraVolumeChanged = false;
				bUsePlayerPawnControlRotation = false;
				NewCameraFOV_OW = bIsCameraOrthographic ? CameraComponent->DefaultCameraOrthoWidth : CameraComponent->DefaultCameraFieldOfView;

				if (bCheckCameraVolumes)
				{
					// Check if camera component not contains any overlapped camera volumes,
					// try to get them from actors overlapping character's primitive component
					// and put into camera component
					if (CameraComponent->OverlappingCameraVolumes.Num() == 0)
					{
						TArray<AActor*> OverlappingActors;
						PlayerCharacter->GetCollisionPrimitiveComponent()->GetOverlappingActors(OverlappingActors, ACameraVolumeActor::StaticClass());

						if (OverlappingActors.Num() > 0)
						{
							for (AActor* OverlappingActor : OverlappingActors)
							{
								if (ACameraVolumeActor* CameraVolume = Cast<ACameraVolumeActor>(OverlappingActor))
								{
									CameraComponent->OverlappingCameraVolumes.AddUnique(CameraVolume);
								}
							}

							CameraVolumeCurrent = UCameraVolumesFunctionLibrary::GetCurrentCameraVolume(CameraComponent->OverlappingCameraVolumes, PlayerPawnLocation);
						}
						else
						{
							// There are no camera volumes overlapping character at this time,
							// so we don't need this check until player pawn overlap some camera volume again.
							SetCheckCameraVolumes(false);
						}
					}
					else
					{
						CameraVolumeCurrent = UCameraVolumesFunctionLibrary::GetCurrentCameraVolume(CameraComponent->OverlappingCameraVolumes, PlayerPawnLocation);
					}

					if (CameraVolumeCurrent)
					{
						if (CameraVolumeCurrent != CameraVolumePrevious) // Do we changed to another volume?
						{
							const FSideInfo PassedSideInfoCurrent = CameraVolumeCurrent->GetNearestVolumeSideInfo(PlayerPawnLocation);

							if (CameraVolumePrevious)
							{
								const FSideInfo PassedSideInfoPrevious = CameraVolumePrevious->GetNearestVolumeSideInfo(PlayerPawnLocation);

								if (UCameraVolumesFunctionLibrary::CompareSidesPairs(PassedSideInfoCurrent.Side, PassedSideInfoPrevious.Side, CameraVolumePrevious->bUse6DOFVolume))
								{
									// We've passed to nearby volume
									// Use settings of side we have passed to
									SetTransitionBySideInfo(CameraVolumeCurrent, PassedSideInfoCurrent);
								}
								else
								{
									// We've passed to volume with another priority
									if (CameraVolumeCurrent->Priority > CameraVolumePrevious->Priority)
									{
										// Use settings of side we have passed to
										SetTransitionBySideInfo(CameraVolumeCurrent, PassedSideInfoCurrent);
									}
									else
									{
										// Use settings of side we have passed from
										SetTransitionBySideInfo(CameraVolumePrevious, PassedSideInfoPrevious);
									}
								}
							}
							else
							{
								// We've passed from void to volume
								SetTransitionBySideInfo(CameraVolumeCurrent, PassedSideInfoCurrent);
							}
						}

						//SelectPerformBlockingCalculations(CameraVolumeCurrent->bPerformCameraBlocking);
						bBlockingCalculations = bPerformBlockingCalculations ? CameraVolumeCurrent->bPerformCameraBlocking : bPerformBlockingCalculations;
						CalcNewCameraParams(CameraVolumeCurrent, DeltaTime);
					}
					else if (CameraVolumePrevious) // Do we passed from volume to void?
					{
						// Use settings of side we've passed from
						const FSideInfo PassedSideInfoPrevious = CameraVolumePrevious->GetNearestVolumeSideInfo(PlayerPawnLocation);
						SetTransitionBySideInfo(CameraVolumePrevious, PassedSideInfoPrevious);
						CalcNewCameraParams(nullptr, DeltaTime);
					}
					else
					{
						CalcNewCameraParams(nullptr, DeltaTime);
					}
				}
				else
				{
					CalcNewCameraParams(nullptr, DeltaTime);
				}

				CameraComponent->UpdateCamera(NewCameraLocationFinal, NewCameraFocalPoint, NewCameraRotationFinal, NewCameraFOV_OWFinal, bIsCameraStatic);
			}
		}
	}

	Super::UpdateCamera(DeltaTime);
}

void ACameraVolumesCameraManager::CalcNewCameraParams(ACameraVolumeActor* CameraVolume, float DeltaTime)
{
	if (CameraVolume)
	{
		if (bIsCameraOrthographic)
		{
			if (CameraVolume->bOverrideCameraOrthoWidth)
			{
				NewCameraFOV_OW = CameraVolume->CameraOrthoWidth;
			}
		}
		else
		{
			if (CameraVolume->bOverrideCameraFieldOfView)
			{
				NewCameraFOV_OW = CameraVolume->CameraFieldOfView;
			}
		}

		// Get screen (or at least player camera) aspect ratio for further calculations
		const float PlayerCamFOVTangens = FMath::Tan(FMath::DegreesToRadians(NewCameraFOV_OW * 0.5f));
		float ScreenAspectRatio = CameraComponent->AspectRatio;

		if (GEngine->GameViewport->Viewport)
		{
			const FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
			if (ViewportSize.Y != 0)
			{
				ScreenAspectRatio = ViewportSize.X / ViewportSize.Y;
			}
		}

		// Location and Rotation
		const FVector PlayerPawnLocationTransformed = CameraVolume->GetActorTransform().InverseTransformPositionNoScale(PlayerPawnLocation);
		bIsCameraStatic = CameraVolume->GetIsCameraStatic();

		if (bIsCameraStatic)
		{
			NewCameraLocation = CameraVolume->CameraLocation;

			if (CameraVolume->bFocalPointIsPlayer)
			{
				NewCameraFocalPoint = PlayerPawnLocationTransformed + CameraVolume->CameraFocalPoint;
				NewCameraRotation = UCameraVolumesFunctionLibrary::CalculateCameraRotation(CameraVolume->CameraLocation, NewCameraFocalPoint, CameraVolume->CameraRoll);
			}
			else
			{
				NewCameraRotation = CameraVolume->CameraRotation;
			}
		}
		else
		{
			if (CameraComponent->bUseDeadZone)
			{
				if (IsInDeadZone(CameraComponent->DeadZoneFocalPoint))
				{

				}
				else
				{

				}
			}
			else
			{
				if (CameraVolume->bUseCameraRotationAxis)
				{
					NewCameraLocation = CameraVolume->bOverrideCameraLocation
						? CameraVolume->CameraLocation
						: CameraComponent->DefaultCameraLocation;

					const FVector DirToAxis = PlayerPawnLocationTransformed.GetSafeNormal2D();
					const FQuat RotToAxis = FRotationMatrix::MakeFromX(DirToAxis).ToQuat();
					NewCameraLocation = RotToAxis.RotateVector(NewCameraLocation) + FVector(0.f, 0.f, PlayerPawnLocationTransformed.Z);

					if (!CameraVolume->bCameraLocationRelativeToVolume && CameraVolume->bOverrideCameraLocation)
					{
						NewCameraLocation += FVector(PlayerPawnLocationTransformed.X, PlayerPawnLocationTransformed.Y, 0.f);
					}

					NewCameraFocalPoint = PlayerPawnLocationTransformed;
					NewCameraFocalPoint += CameraVolume->bOverrideCameraRotation
						? RotToAxis.RotateVector(CameraVolume->CameraFocalPoint)
						: RotToAxis.RotateVector(CameraComponent->DefaultCameraFocalPoint);

					const float NewCameraRoll = CameraVolume->bOverrideCameraRotation
						? CameraVolume->CameraRoll
						: CameraComponent->DefaultCameraRoll;

					NewCameraRotation = UCameraVolumesFunctionLibrary::CalculateCameraRotation(NewCameraLocation, NewCameraFocalPoint, NewCameraRoll);
				}
				else
				{
					NewCameraLocation = PlayerPawnLocationTransformed;
					NewCameraFocalPoint = PlayerPawnLocationTransformed;

					NewCameraLocation += CameraVolume->bOverrideCameraLocation
						? CameraVolume->CameraLocation
						: CameraComponent->DefaultCameraLocation;

					NewCameraFocalPoint += CameraVolume->bOverrideCameraRotation
						? CameraVolume->CameraFocalPoint
						: CameraComponent->DefaultCameraFocalPoint;

					if (CameraVolume->bOverrideCameraRotation)
					{
						NewCameraRotation = CameraVolume->CameraRotation;
					}

					if (CameraVolume->bCameraLocationRelativeToVolume)
					{
						NewCameraLocation.Y = CameraVolume->bOverrideCameraLocation
							? CameraVolume->CameraLocation.Y
							: CameraComponent->DefaultCameraLocation.Y;

						NewCameraFocalPoint.Y = CameraVolume->bOverrideCameraRotation
							? CameraVolume->CameraFocalPoint.Y
							: CameraComponent->DefaultCameraFocalPoint.Y;
					}
				}
			}

			// Calculate camera blocking like it oriented to volume Front side
			if (bBlockingCalculations)
			{
				FVector NewCamVolExtentCorrected = CameraVolume->CamVolExtentCorrected;
				FVector NewCamVolMinCorrected = CameraVolume->CamVolMinCorrected;
				FVector NewCamVolMaxCorrected = CameraVolume->CamVolMaxCorrected;

				// Calculate delta volume extent with max +Y volume coordinate
				if (!CameraVolume->bUseZeroDepthExtent || !bIsCameraOrthographic)
				{
					FVector DeltaExtent = FVector::ZeroVector;
					DeltaExtent.X = FMath::Abs((NewCamVolMaxCorrected.Y) * PlayerCamFOVTangens);
					DeltaExtent = FVector(DeltaExtent.X, 0.f, DeltaExtent.X / ScreenAspectRatio);
					NewCamVolExtentCorrected += DeltaExtent;
					NewCamVolMinCorrected -= DeltaExtent;
					NewCamVolMaxCorrected += DeltaExtent;
				}

				// Camera offset is always relative to camera volume local Y axis
				float CameraOffset = CameraVolume->bUseCameraRotationAxis
					? NewCameraLocation.Size2D()
					: NewCameraLocation.Y;

				// Calculate new camera offset and screen world extent at depth (CameraOffset)
				FVector ScreenExtent = FVector::ZeroVector;

				if (bIsCameraOrthographic)
				{
					NewCameraFOV_OW = CameraVolume->CamVolAspectRatio >= ScreenAspectRatio
						? FMath::Clamp(NewCameraFOV_OW, NewCameraFOV_OW, 2.f * NewCamVolExtentCorrected.Z * ScreenAspectRatio)
						: FMath::Clamp(NewCameraFOV_OW, NewCameraFOV_OW, 2.f * NewCamVolExtentCorrected.X);

					ScreenExtent.X = NewCameraFOV_OW * 0.5f;
				}
				else
				{
					if (CameraVolume->bUseCameraRotationAxis)
					{
						CameraOffset = FMath::Clamp(CameraOffset, CameraOffset, NewCamVolExtentCorrected.Z * ScreenAspectRatio / PlayerCamFOVTangens);
					}
					else
					{
						CameraOffset = CameraVolume->CamVolAspectRatio >= ScreenAspectRatio
							? FMath::Clamp(CameraOffset, CameraOffset, NewCamVolExtentCorrected.Z * ScreenAspectRatio / PlayerCamFOVTangens)
							: FMath::Clamp(CameraOffset, CameraOffset, NewCamVolExtentCorrected.X / PlayerCamFOVTangens);
					}

					ScreenExtent.X = FMath::Abs(CameraOffset * PlayerCamFOVTangens);
				}

				ScreenExtent = FVector(ScreenExtent.X, 0.f, ScreenExtent.X / ScreenAspectRatio);
				const FVector ScreenMin = NewCamVolMinCorrected + ScreenExtent;
				const FVector ScreenMax = NewCamVolMaxCorrected - ScreenExtent;

				// Perform camera blocking only on top and bottom sides
				NewCameraLocation = CameraVolume->bUseCameraRotationAxis
					? FVector(
						NewCameraLocation.GetSafeNormal2D().X * CameraOffset,
						NewCameraLocation.GetSafeNormal2D().Y * CameraOffset,
						FMath::Clamp(NewCameraLocation.Z, ScreenMin.Z, ScreenMax.Z))
					: FVector(
						FMath::Clamp(NewCameraLocation.X, ScreenMin.X, ScreenMax.X),
						CameraOffset,
						FMath::Clamp(NewCameraLocation.Z, ScreenMin.Z, ScreenMax.Z));
			}
		}

		// Final world-space values
		NewCameraLocation = CameraVolume->GetActorTransform().TransformPositionNoScale(NewCameraLocation);
		NewCameraFocalPoint = CameraVolume->GetActorTransform().TransformPositionNoScale(NewCameraFocalPoint);
		NewCameraRotation = CameraVolume->GetActorTransform().TransformRotation(NewCameraRotation);

		bUsePlayerPawnControlRotation = false;
	}
	else if (CameraComponent->bUsePawnControlRotationCV)
	{
		const FRotator CamRot = CameraComponent->DefaultCameraRotation.Rotator();
		FRotator PawnViewRot = PlayerPawn->GetViewRotation();

		if (!CameraComponent->bInheritPitchCV)
		{
			PawnViewRot.Pitch = CamRot.Pitch;
		}

		if (!CameraComponent->bInheritYawCV)
		{
			PawnViewRot.Yaw = CamRot.Yaw;
		}

		if (!CameraComponent->bInheritRollCV)
		{
			PawnViewRot.Roll = CamRot.Roll;
		}

		NewCameraRotation = PawnViewRot.Quaternion();

		if (CameraComponent->bEnableCameraRotationLag)
		{
			NewCameraRotation = FMath::QInterpTo(OldCameraRotation, NewCameraRotation, DeltaTime, CameraComponent->CameraRotationLagSpeed);
		}

		NewCameraLocation = PlayerPawnLocation;

		if (CameraComponent->bEnableCameraLocationLag)
		{
			NewCameraLocation = FMath::VInterpTo(PlayerPawnLocationOld, NewCameraLocation, DeltaTime, CameraComponent->CameraLocationLagSpeed);
		}

		PlayerPawnLocationOld = NewCameraLocation;
		NewCameraFocalPoint = NewCameraLocation + PlayerPawn->GetActorQuat().RotateVector(CameraComponent->DefaultCameraFocalPoint);
		NewCameraLocation = NewCameraFocalPoint + NewCameraRotation.RotateVector(CameraComponent->DefaultCameraLocation - CameraComponent->DefaultCameraFocalPoint);

		bUsePlayerPawnControlRotation = true;
	}

	// Transitions and interpolation
	if (bNeedsSmoothTransition)
	{
		SmoothTransitionAlpha += DeltaTime * SmoothTransitionSpeed;

		if (SmoothTransitionAlpha <= 1.f)
		{
			NewCameraLocation = FMath::Lerp(OldCameraLocation, NewCameraLocation, SmoothTransitionAlpha);
			NewCameraRotation = FQuat::Slerp(OldCameraRotation, NewCameraRotation, SmoothTransitionAlpha);
			NewCameraFOV_OW = FMath::Lerp(OldCameraFOV_OW, NewCameraFOV_OW, SmoothTransitionAlpha);
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
		bGameCameraCutThisFrame = true;
	}
	else
	{
		if (CameraComponent->bEnableCameraLocationLag && !bUsePlayerPawnControlRotation)
		{
			NewCameraLocation = FMath::VInterpTo(OldCameraLocation, NewCameraLocation, DeltaTime, CameraComponent->CameraLocationLagSpeed);
		}

		if (CameraComponent->bEnableCameraRotationLag && !bUsePlayerPawnControlRotation)
		{
			NewCameraRotation = FMath::QInterpTo(OldCameraRotation, NewCameraRotation, DeltaTime, CameraComponent->CameraRotationLagSpeed);
		}

		if (bIsCameraOrthographic)
		{
			if (CameraComponent->bEnableCameraOrthoWidthInterp)
			{
				NewCameraFOV_OW = FMath::FInterpTo(OldCameraFOV_OW, NewCameraFOV_OW, DeltaTime, CameraComponent->CameraOrthoWidthInterpSpeed);
			}
		}
		else if (CameraComponent->bEnableCameraFOVInterp)
		{
			NewCameraFOV_OW = FMath::FInterpTo(OldCameraFOV_OW, NewCameraFOV_OW, DeltaTime, CameraComponent->CameraFOVInterpSpeed);
		}
	}

	NewCameraLocationFinal = NewCameraLocation;
	NewCameraRotationFinal = NewCameraRotation;
	NewCameraFOV_OWFinal = NewCameraFOV_OW;

	if (CameraComponent->bUseAdditionalCameraParams)
	{
		NewCameraLocationFinal += CameraComponent->AdditionalCameraLocation;
		NewCameraRotationFinal *= CameraComponent->AdditionalCameraRotation.Quaternion();
		NewCameraFOV_OWFinal += bIsCameraOrthographic
			? CameraComponent->AdditionalCameraOrthoWidth
			: CameraComponent->AdditionalCameraFOV;
	}

	// Broadcast volume changed event
	if (bBroadcastOnCameraVolumeChanged)
	{
		OnCameraVolumeChanged.Broadcast(BroadcastCameraVolume, BroadcastSideInfo);
	}
}

void ACameraVolumesCameraManager::SetTransitionBySideInfo(ACameraVolumeActor* CameraVolume, FSideInfo SideInfo)
{
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

	// Store OnCameraVolumeChanged broadcast params
	bBroadcastOnCameraVolumeChanged = true;

	if (bBroadcastOnCameraVolumeChanged)
	{
		BroadcastCameraVolume = CameraVolume;
		BroadcastSideInfo = SideInfo;
	}
}

bool ACameraVolumesCameraManager::IsInDeadZone(FVector WorldLocationToCheck)
{
	if (GetOwningPlayerController())
	{
		if (GEngine->GameViewport->Viewport)
		{
			if (CameraComponent)
			{
				FVector2D ScreenLocation;
				GetOwningPlayerController()->ProjectWorldLocationToScreen(WorldLocationToCheck, ScreenLocation);

				const FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
				const FVector2D ScreenCenter = ViewportSize / 2;
				const FVector2D DeadZoneCenter = ScreenCenter + CameraComponent->DeadZoneOffset;
				const FVector2D DeadZoneMin = DeadZoneCenter - CameraComponent->DeadZoneExtent;
				const FVector2D DeadZoneMax = DeadZoneCenter + CameraComponent->DeadZoneExtent;

				const float Depth = NewCameraLocationFinal.Y;
				const FVector2D Scr = CalculateScreenWorldExtentAtDepth(Depth);
				const FVector Ext = FVector(Scr.X * (CameraComponent->DeadZoneExtent.X / 100), 0.f, Scr.Y * (CameraComponent->DeadZoneExtent.Y / 100));

				DrawDebugBox(GetWorld(), FVector(NewCameraLocationFinal.X, 0.f, NewCameraLocationFinal.Z), Ext, FColor::Yellow, false, 0, 0, 2);

				return ScreenLocation >= DeadZoneMin && ScreenLocation <= DeadZoneMax;
			}
		}
	}

	return false;
}

FVector2D ACameraVolumesCameraManager::CalculateScreenWorldExtentAtDepth(float Depth)
{
	FVector2D ScreenExtentResult = FVector2D::ZeroVector;

	if (APawn* PlayerPawnRef = GetOwningPlayerController()->GetPawn())
	{
		ICameraVolumesCharacterInterface* PlayerCharacter = Cast<ICameraVolumesCharacterInterface>(PlayerPawnRef);
		if (PlayerCharacter)
		{
			UCameraVolumesCameraComponent* PlayerCameraComponent = PlayerCharacter->GetCameraComponent();
			float CameraFOV_OW;

			if (PlayerCameraComponent->GetIsCameraOrthographic())
			{
				CameraFOV_OW = PlayerCameraComponent->OrthoWidth;
				ScreenExtentResult.X = CameraFOV_OW * 0.5f;
			}
			else
			{
				CameraFOV_OW = PlayerCameraComponent->FieldOfView;
				ScreenExtentResult.X = FMath::Abs((GetCameraLocation().Y - Depth) * FMath::Tan(FMath::DegreesToRadians(NewCameraFOV_OW * 0.5f)));
			}

			float ScreenAspectRatio = CameraComponent->AspectRatio;

			if (GEngine->GameViewport->Viewport)
			{
				const FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
				if (ViewportSize.Y != 0)
				{
					ScreenAspectRatio = ViewportSize.X / ViewportSize.Y;
				}
			}

			ScreenExtentResult = FVector2D(ScreenExtentResult.X, ScreenExtentResult.X / ScreenAspectRatio);
		}
	}

	return ScreenExtentResult;
}
