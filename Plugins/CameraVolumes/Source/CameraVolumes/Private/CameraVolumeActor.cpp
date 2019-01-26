//Dmitriy Barannik aka redbox, 2019

#include "CameraVolumeActor.h"

ACameraVolumeActor::ACameraVolumeActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Default root
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	DefaultSceneRoot->Mobility = EComponentMobility::Static;
	DefaultSceneRoot->bVisible = false;
	RootComponent = DefaultSceneRoot;

	// Billboard
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("BillboardComponent"));
	BillboardComponent->SetupAttachment(RootComponent);
	BillboardComponent->bHiddenInGame = true;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TextureObj(TEXT("/CameraVolumes/Icons/CameraVolume"));
	if (TextureObj.Object)
		BillboardComponent->Sprite = TextureObj.Object;

	// BoxComponent
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetupAttachment(RootComponent);
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	BoxComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	// CameraComponent
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(RootComponent);

	// Default values
	Priority = 0;
	VolumeExtent = FVector(500.f, 500.f, 500.f);

	CameraMobility = ECameraMobility::ECM_Movable;

	bOverrideCameraLocation = false;
	CameraLocation = DefaultCameraLocation;
	bCameraLocationRelativeToVolume = false;

	bOverrideCameraFocalPoint = false;
	CameraFocalPoint = DefaultCameraFocalPoint;
	bIsCameraStatic = false;
	bFocalPointIsPlayer = true;

	bOverrideCameraFieldOfView = false;
	CameraFieldOfView = DefaultCameraFOV;

	CameraSmoothTransitionTime = 1.f;

	/**
	*	Sides indicators
	*	Priority [0]
	*	SideInfo [1-12]
	*/
	Text_Indicators.SetNum(13);
	FName ComponentName;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TextMaterialObj(TEXT("/CameraVolumes/Materials/UnlitText.UnlitText"));
	for (uint8 i = 0; i < 13; i++)
	{
		ComponentName = *(FString(TEXT("TextRenderComponent")) + FString::FromInt(i));
		Text_Indicators[i] = CreateDefaultSubobject<UTextRenderComponent>(ComponentName);
		Text_Indicators[i]->SetupAttachment(RootComponent);
		Text_Indicators[i]->bHiddenInGame = true;
		Text_Indicators[i]->bAlwaysRenderAsText = true;
		if (TextMaterialObj.Object)
			Text_Indicators[i]->SetTextMaterial(TextMaterialObj.Object);
		Text_Indicators[i]->HorizontalAlignment = EHorizTextAligment::EHTA_Center;
		Text_Indicators[i]->VerticalAlignment = EVerticalTextAligment::EVRTA_TextCenter;
	}

	UpdateVolume();
}

void ACameraVolumeActor::UpdateVolume()
{
	//Reset actor rotation and scale
	SetActorRotation(FRotator::ZeroRotator);
	SetActorScale3D(FVector::OneVector);

	//Extents
	UpdateVolumeExtents();

	//Components
	BillboardComponent->SetRelativeLocation(FVector::ZeroVector);
	BillboardComponent->SetRelativeScale3D(FVector(5.f, 1.f, 1.f));
	BoxComponent->SetBoxExtent(VolumeExtent);

	if (!bOverrideCameraLocation)
	{
		CameraLocation = DefaultCameraLocation;
		bCameraLocationRelativeToVolume = false;
	}

	if (!bOverrideCameraFocalPoint)
		CameraFocalPoint = DefaultCameraFocalPoint;

	switch (CameraMobility)
	{
	case ECameraMobility::ECM_Movable:
		bIsCameraStatic = false;
		bFocalPointIsPlayer = true;
		break;
	case ECameraMobility::ECM_Static:
		bIsCameraStatic = true;
		break;
	}

	CameraRotation = FRotationMatrix::MakeFromX(CameraFocalPoint - CameraLocation).ToQuat();
	CameraComponent->SetRelativeLocationAndRotation(CameraLocation, CameraRotation);

	if (bOverrideCameraFieldOfView)
		CameraComponent->FieldOfView = CameraFieldOfView;
	else
	{
		CameraFieldOfView = DefaultCameraFOV;
		CameraComponent->FieldOfView = CameraFieldOfView;
	}
	//--------------------------------------------------

	//Indicators
	// Side-scroller screen should be aligned on CamVolWorldMax.X coordinate, so front side must be closed.
	//FrontSide.SideType = ESideType::EST_Closed;
	// Top-down screen should be aligned on CamVolWorldMax.Z coordinate, so top side must be closed.
	//TopSide.SideType = ESideType::EST_Closed;

	//Priority
	Text_Indicators[0]->SetText(FText::FromString(FString::FromInt(Priority)));
	Text_Indicators[0]->SetTextRenderColor(FColor::White);
	Text_Indicators[0]->SetWorldSize(2.f * Text_Size);
	Text_Indicators[0]->SetRelativeLocation(FVector(VolumeExtent.X, 0.f, 2.f * Text_Size)); // Side-scroller
	//Text_Indicators[0]->SetRelativeLocationAndRotation(FVector(-2.f * Text_Size, 0.f, VolumeExtent.Z), FRotator(90.f, 0.f, 0.f)); // Top-down

	//Sides
	for (uint8 i = 1; i <= 12; i = i + 2)
	{
		Text_Indicators[i]->SetWorldSize(Text_Size);
		Text_Indicators[i + 1]->SetWorldSize(Text_Size);

		FSideInfo SideInfo;
		if (i == 1)
		{
			SideInfo = FrontSide;
			Text_Indicators[i]->SetRelativeLocationAndRotation(FVector(VolumeExtent.X - 1.5f * Text_Size, 0.f, VolumeExtent.Z), FRotator(90.f, 0.f, 0.f));
			Text_Indicators[i + 1]->SetRelativeLocationAndRotation(FVector(VolumeExtent.X - 0.5f * Text_Size, 0.f, VolumeExtent.Z), FRotator(90.f, 0.f, 0.f));
		}
		else if (i == 3)
		{
			SideInfo = BackSide;
			Text_Indicators[i]->SetRelativeLocationAndRotation(FVector(-VolumeExtent.X + 0.5f * Text_Size, 0.f, VolumeExtent.Z), FRotator(90.f, 0.f, 0.f));
			Text_Indicators[i + 1]->SetRelativeLocationAndRotation(FVector(-VolumeExtent.X + 1.5f * Text_Size, 0.f, VolumeExtent.Z), FRotator(90.f, 0.f, 0.f));
		}
		else if (i == 5)
		{
			SideInfo = RightSide;
			Text_Indicators[i]->SetRelativeLocation(FVector(VolumeExtent.X, -VolumeExtent.Y + 2.f * Text_Size, 0.5f * Text_Size));
			Text_Indicators[i + 1]->SetRelativeLocation(FVector(VolumeExtent.X, -VolumeExtent.Y + 2.f * Text_Size, -0.5f * Text_Size));
		}
		else if (i == 7)
		{
			SideInfo = LeftSide;
			Text_Indicators[i]->SetRelativeLocation(FVector(VolumeExtent.X, VolumeExtent.Y - 2.f * Text_Size, 0.5f * Text_Size));
			Text_Indicators[i + 1]->SetRelativeLocation(FVector(VolumeExtent.X, VolumeExtent.Y - 2.f * Text_Size, -0.5f * Text_Size));
		}
		else if (i == 9)
		{
			SideInfo = TopSide;
			Text_Indicators[i]->SetRelativeLocation(FVector(VolumeExtent.X, 0.f, VolumeExtent.Z - 0.5f * Text_Size));
			Text_Indicators[i + 1]->SetRelativeLocation(FVector(VolumeExtent.X, 0.f, VolumeExtent.Z - 1.5f * Text_Size));
		}
		else if (i == 11)
		{
			SideInfo = BottomSide;
			Text_Indicators[i]->SetRelativeLocation(FVector(VolumeExtent.X, 0.f, -VolumeExtent.Z + 1.5f * Text_Size));
			Text_Indicators[i + 1]->SetRelativeLocation(FVector(VolumeExtent.X, 0.f, -VolumeExtent.Z + 0.5f * Text_Size));
		}

		if (SideInfo.SideType == ESideType::EST_Closed)
		{
			Text_Indicators[i]->SetText(Text_Closed);
			Text_Indicators[i]->SetTextRenderColor(FColor::Red);
		}
		else
		{
			Text_Indicators[i]->SetText(Text_Open);
			Text_Indicators[i]->SetTextRenderColor(FColor::Green);
		}

		switch (SideInfo.SideTransitionType)
		{
		case ESideTransitionType::ESTT_Normal:
			Text_Indicators[i + 1]->SetText(Text_Normal);
			Text_Indicators[i + 1]->SetTextRenderColor(FColor::White);
			break;
		case ESideTransitionType::ESTT_Smooth:
			Text_Indicators[i + 1]->SetText(Text_Smooth);
			Text_Indicators[i + 1]->SetTextRenderColor(FColor::Green);
			break;
		case ESideTransitionType::ESTT_Cut:
			Text_Indicators[i + 1]->SetText(Text_Cut);
			Text_Indicators[i + 1]->SetTextRenderColor(FColor::Red);
			break;
		}
	}
	//--------------------------------------------------

	this->Modify();
}

void ACameraVolumeActor::UpdateVolumeExtents()
{
	VolumeExtent = VolumeExtent.GetAbs();
	CamVolAspectRatio = VolumeExtent.Y / VolumeExtent.Z; // YZ Side-scroller
	//CamVolAspectRatio = VolumeExtent.Y / VolumeExtent.X; // YX Top-down
	CamVolWorldMax = GetActorLocation() + VolumeExtent;
	CamVolWorldMin = CamVolWorldMax - VolumeExtent * 2.f;
	CamVolWorldMinCorrected = CamVolWorldMin;
	CamVolWorldMaxCorrected = CamVolWorldMax;

	if (FrontSide.SideType == ESideType::EST_Open)
		CamVolWorldMaxCorrected.X = CamVolWorldMaxCorrected.X + OpenEdgeOffset;

	if (BackSide.SideType == ESideType::EST_Open)
		CamVolWorldMinCorrected.X = CamVolWorldMinCorrected.X - OpenEdgeOffset;

	if (RightSide.SideType == ESideType::EST_Open)
		CamVolWorldMinCorrected.Y = CamVolWorldMinCorrected.Y - OpenEdgeOffset;

	if (LeftSide.SideType == ESideType::EST_Open)
		CamVolWorldMaxCorrected.Y = CamVolWorldMaxCorrected.Y + OpenEdgeOffset;

	if (TopSide.SideType == ESideType::EST_Open)
		CamVolWorldMaxCorrected.Z = CamVolWorldMaxCorrected.Z + OpenEdgeOffset;

	if (BottomSide.SideType == ESideType::EST_Open)
		CamVolWorldMinCorrected.Z = CamVolWorldMinCorrected.Z - OpenEdgeOffset;

	CamVolExtentCorrected = (CamVolWorldMaxCorrected - CamVolWorldMinCorrected) * 0.5f;
}

FSideInfo ACameraVolumeActor::GetSideInfo(ESide Side)
{
	switch (Side)
	{
	case ESide::ES_Front:
		return FrontSide;
		break;
	case ESide::ES_Back:
		return BackSide;
		break;
	case ESide::ES_Right:
		return RightSide;
		break;
	case ESide::ES_Left:
		return LeftSide;
		break;
	case ESide::ES_Top:
		return TopSide;
		break;
	case ESide::ES_Bottom:
		return BottomSide;
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("Unknown side type! Using SideInfo(Open, Normal)"))
			return FSideInfo();
		break;
	}
}

ESide ACameraVolumeActor::GetNearestVolumeSide(FVector& PlayerPawnLocation)
{
	ESide NearestSide = ESide::ES_Unknown;
	TMap<ESide, float> Sides;
	Sides.Add(ESide::ES_Front, FMath::Abs(PlayerPawnLocation.X - CamVolWorldMax.X));
	Sides.Add(ESide::ES_Back, FMath::Abs(PlayerPawnLocation.X - CamVolWorldMin.X));
	Sides.Add(ESide::ES_Right, FMath::Abs(PlayerPawnLocation.Y - CamVolWorldMin.Y));
	Sides.Add(ESide::ES_Left, FMath::Abs(PlayerPawnLocation.Y - CamVolWorldMax.Y));
	Sides.Add(ESide::ES_Top, FMath::Abs(PlayerPawnLocation.Z - CamVolWorldMax.Z));
	Sides.Add(ESide::ES_Bottom, FMath::Abs(PlayerPawnLocation.Z - CamVolWorldMin.Z));
	Sides.ValueSort([](float Min, float Max) { return Min < Max; });

	for (auto& Pair : Sides)
	{
		NearestSide = Pair.Key;
		break;
	}

	return NearestSide;
}

//Update with changed property
#if WITH_EDITOR
void ACameraVolumeActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == TEXT("CameraMobility")
		|| TEXT("Priority") || TEXT("VolumeExtent")
		|| TEXT("bOverrideCameraLocation") || TEXT("CameraLocation")
		|| TEXT("bOverrideCameraFocalPoint") || TEXT("CameraFocalPoint")
		|| TEXT("bOverrideCameraFieldOfView") || TEXT("CameraFieldOfView")
		|| TEXT("FrontSide") || TEXT("BackSide") || TEXT("RightSide") || TEXT("LeftSide") || TEXT("TopSide") || TEXT("BottomSide"))
		UpdateVolume();
}

void ACameraVolumeActor::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);
	UpdateVolume();
}

void ACameraVolumeActor::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);
	UpdateVolume();
}

void ACameraVolumeActor::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyScale(DeltaScale, PivotLocation, bAltDown, bShiftDown, bCtrlDown);
	UpdateVolume();
}
#endif