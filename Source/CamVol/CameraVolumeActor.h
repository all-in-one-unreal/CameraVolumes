// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "Components/BillboardComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "CameraVolumeActor.generated.h"

UENUM(BlueprintType)
enum class ESide : uint8
{
	ES_Unknown	UMETA(DisplayName = "Unknown"),
	ES_Front	UMETA(DisplayName = "Front"),
	ES_Back		UMETA(DisplayName = "Back"),
	ES_Right	UMETA(DisplayName = "Right"),
	ES_Left		UMETA(DisplayName = "Left"),
	ES_Top		UMETA(DisplayName = "Top"),
	ES_Bottom	UMETA(DisplayName = "Bottom")
};

//Side can be Open or Closed
UENUM(BlueprintType)
enum class ESideType : uint8
{
	EST_Open	UMETA(DisplayName = "Open"),
	EST_Closed	UMETA(DisplayName = "Closed")
};

//Side Transition can be Smooth or Cut
UENUM(BlueprintType)
enum class ESideTransitionType : uint8
{
	ESTT_Smooth	UMETA(DisplayName = "Smooth"),
	ESTT_Cut	UMETA(DisplayName = "Cut")
};

USTRUCT(BlueprintType)
struct FSideInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		ESideType SideType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		ESideTransitionType SideTransitionType;

	FSideInfo()
	{
		SideType = ESideType::EST_Open;
		SideTransitionType = ESideTransitionType::ESTT_Smooth;
	}

	FSideInfo(ESideType SideType, ESideTransitionType SideTransitionType)
	{
		this->SideType = SideType;
		this->SideTransitionType = SideTransitionType;
	}
};

UCLASS()
class ACameraVolumeActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACameraVolumeActor();

	//Components
	UPROPERTY()
		USceneComponent* DefaultSceneRoot;

	UPROPERTY()
		UBillboardComponent* BillboardComponent;

	UPROPERTY()
		UBoxComponent* BoxComponent;

	UPROPERTY()
		UCameraComponent* CameraComponent;
	//--------------------------------------------------

	//Parameters
	/** Use this to update volume after made changes in editor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume")
		bool bUpdate;

	/** Priority of camera volume in case of few overlapped volumes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume", Meta = (ClampMin = "-100", ClampMax = "100", UIMin = "-100", UIMax = "100"))
		int32 Priority;

	/** Volume extent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume", Meta = (MakeEditWidget = true))
		FVector VolumeExtent;

	/** Time of smooth camera transition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Transition", Meta = (ClampMin = "0.1", ClampMax = "10.0", UIMin = "0.1", UIMax = "10.0"))
		float CameraSmoothTransitionTime;

	/** Should camera FOV be overriden? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FieldOfView")
		bool bOverrideCameraFieldOfView;

	/** New camera FOV */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FieldOfView", Meta = (EditCondition = "bOverrideCameraFieldOfView", UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0", Units = deg))
		float CameraFieldOfView;

	/** Should camera offset be overriden? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|MovableCamera")
		bool bOverrideCameraOffset;

	/** New camera offset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|MovableCamera", Meta = (EditCondition = "bOverrideCameraOffset", UIMin = "10.0", UIMax = "10000.0", ClampMin = "10.0", ClampMax = "10000.0"))
		float CameraOffset;

	/** Should camera use fixed location? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FixedCamera")
		bool bFixedCamera;

	/** New camera fixed location */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FixedCamera", Meta = (EditCondition = "bFixedCamera", MakeEditWidget = true))
		FVector FixedCameraLocation;
	
	UPROPERTY()
		bool bFocalPointEditCond;

	/** Location to fixed camera look at */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FixedCamera", Meta = (EditCondition = "bFocalPointEditCond", MakeEditWidget = true))
		FVector FixedCameraFocalPoint;

	/** Should fixed camera look at player pawn? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FixedCamera", Meta = (EditCondition = "bFixedCamera"))
		bool bFocalPointIsPlayer;

	UPROPERTY()
		FRotator FixedCameraRotation;
	//--------------------------------------------------

	// Sides info
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SidesInfo")
		FSideInfo FrontSide;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SidesInfo")
		FSideInfo BackSide;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SidesInfo")
		FSideInfo RightSide;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SidesInfo")
		FSideInfo LeftSide;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SidesInfo")
		FSideInfo TopSide;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SidesInfo")
		FSideInfo BottomSide;
	//--------------------------------------------------

	//Sides indicators
	UPROPERTY()
		TArray<UTextRenderComponent*> Text_Indicators;
	const float Text_Size = 50.f;
	const FText Text_Open = FText::FromString("OPEN");
	const FText Text_Closed = FText::FromString("CLOSED");
	const FText Text_Smooth = FText::FromString("SMOOTH");
	const FText Text_Cut = FText::FromString("CUT");
	//--------------------------------------------------

	UPROPERTY()
		float CamVolAspectRatio;

	UPROPERTY()
		FVector CamVolWorldMin;

	UPROPERTY()
		FVector CamVolWorldMax;

	UPROPERTY()
		FVector CamVolWorldMinCorrected;

	UPROPERTY()
		FVector CamVolWorldMaxCorrected;

	UPROPERTY()
		FVector CamVolExtentCorrected;

	UFUNCTION()
		virtual void UpdateVolume();

	UFUNCTION()
		virtual FSideInfo GetSideInfo(ESide Side);

	const float OpenEdgeOffset = 10000.f;

	//Override PostEditChangeProperty
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};