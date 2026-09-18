#include "SMapTileEditorWidget.h"

#include "MapTilePalette.h"
#include "MapTilePaletteFactory.h"

#include "AssetRegistry/AssetData.h"
#include "AssetThumbnail.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "IAssetTools.h"
#include "LevelEditorViewport.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "MapTileEditor"

namespace
{
	/** 팔레트 썸네일 한 변의 픽셀 크기입니다. */
	constexpr int32 TileThumbnailSize = 40;

	/** 뷰포트를 셀로 옮길 때 카메라를 얼마나 뒤로 물릴지입니다. */
	constexpr float ViewportFocusDistance = 4000.0f;
}

void SMapTileEditorWidget::Construct(const FArguments& InArgs)
{
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(32);

	StatusText = LOCTEXT("NoPalette", "타일 팔레트를 선택하거나 새로 만드세요.");

	// 되돌리기로 레벨이 바뀌면 모델을 다시 읽어야 합니다.
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		// 상단 툴바
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			BuildToolbar()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			// 좌: 등록된 타일
			+ SSplitter::Slot()
			.Value(0.22f)
			[
				BuildPalettePanel()
			]

			// 중앙: 그리드 캔버스
			+ SSplitter::Slot()
			.Value(0.58f)
			[
				SNew(SBorder)
				.Padding(0.0f)
				[
					SAssignNew(Canvas, SMapTileCanvas)
					.BrushMode(this, &SMapTileEditorWidget::GetBrushModeForCanvas)
					.BrushFootprint(this, &SMapTileEditorWidget::GetBrushFootprintForCanvas)
					.CanPlaceRange(this, &SMapTileEditorWidget::CanPlaceRange)
					.OnGetCellVisual(this, &SMapTileEditorWidget::GetCellVisual)
					.OnCellsPainted(this, &SMapTileEditorWidget::HandleCellsPainted)
					.OnCellsErased(this, &SMapTileEditorWidget::HandleCellsErased)
					.OnCellPicked(this, &SMapTileEditorWidget::HandleCellPicked)
					.OnCellFocused(this, &SMapTileEditorWidget::HandleCellFocused)
					.OnRotateBrush(this, &SMapTileEditorWidget::HandleRotateBrush)
				]
			]

			// 우: 배치 설정
			+ SSplitter::Slot()
			.Value(0.20f)
			[
				BuildSettingsPanel()
			]
		]
	];
}

SMapTileEditorWidget::~SMapTileEditorWidget()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}

	ThumbnailPool.Reset();
}

void SMapTileEditorWidget::PostUndo(bool bSuccess)
{
	// 액터가 되살아나거나 사라졌으므로 그리드를 레벨에서 다시 읽습니다.
	RefreshFromLevel();
}

void SMapTileEditorWidget::PostRedo(bool bSuccess)
{
	RefreshFromLevel();
}

TSharedRef<SWidget> SMapTileEditorWidget::BuildToolbar()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("TilePaletteLabel", "타일 팔레트"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UMapTilePalette::StaticClass())
			.ObjectPath(this, &SMapTileEditorWidget::GetPalettePath)
			.OnObjectChanged(this, &SMapTileEditorWidget::OnPaletteChanged)
			.AllowClear(true)
			.DisplayUseSelected(false)
			// 레퍼런스와 동일하게 콤보만 둡니다. 썸네일을 켜면 라벨 영역과 겹칩니다.
			.DisplayThumbnail(false)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("NewPalette", "새 팔레트"))
			.ToolTipText(LOCTEXT("NewPaletteTip", "새 타일 팔레트 애셋을 만듭니다."))
			.OnClicked(this, &SMapTileEditorWidget::OnCreateNewPaletteClicked)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("RefreshLevel", "현재 레벨 새로고침"))
			.ToolTipText(LOCTEXT("RefreshLevelTip", "현재 레벨을 다시 훑어 그리드를 재구성합니다."))
			.OnClicked(this, &SMapTileEditorWidget::OnRefreshLevelClicked)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("ViewAll", "전체 보기"))
			.ToolTipText(LOCTEXT("ViewAllTip", "배치된 타일이 모두 보이도록 캔버스를 맞춥니다."))
			.OnClicked(this, &SMapTileEditorWidget::OnViewAllClicked)
		];
}

FString SMapTileEditorWidget::GetPalettePath() const
{
	const UMapTilePalette* Pal = Palette.Get();

	return Pal ? Pal->GetPathName() : FString();
}

void SMapTileEditorWidget::OnPaletteChanged(const FAssetData& AssetData)
{
	Palette = Cast<UMapTilePalette>(AssetData.GetAsset());
	Grid.SetPalette(Palette.Get());

	SelectedTileIndex = Palette.IsValid() && Palette->Tiles.Num() > 0 ? 0 : INDEX_NONE;

	RebuildTileList();
	RefreshFromLevel();
}

FReply SMapTileEditorWidget::OnCreateNewPaletteClicked()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UMapTilePaletteFactory* Factory = NewObject<UMapTilePaletteFactory>();

	UObject* Created = AssetTools.CreateAssetWithDialog(UMapTilePalette::StaticClass(), Factory);
	if (UMapTilePalette* NewPalette = Cast<UMapTilePalette>(Created))
	{
		Palette = NewPalette;
		Grid.SetPalette(NewPalette);

		SelectedTileIndex = INDEX_NONE;

		RebuildTileList();
		RefreshFromLevel();
	}

	return FReply::Handled();
}

FReply SMapTileEditorWidget::OnRefreshLevelClicked()
{
	RefreshFromLevel();

	return FReply::Handled();
}

FReply SMapTileEditorWidget::OnViewAllClicked()
{
	FIntPoint Min;
	FIntPoint Max;

	if (Canvas.IsValid() && Grid.GetFilledBounds(Min, Max))
	{
		Canvas->FocusOnCellRange(Min, Max);
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SMapTileEditorWidget::BuildPalettePanel()
{
	return SNew(SBorder)
		.Padding(4.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 2.0f, 2.0f, 4.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("RegisteredTiles", "등록된 타일"))
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(TileListView, SListView<FMapTileListEntryPtr>)
				.ListItemsSource(&TileEntries)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SMapTileEditorWidget::OnGenerateTileRow)
				.OnSelectionChanged(this, &SMapTileEditorWidget::OnTileSelectionChanged)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 2.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("RegisterSelectedActor", "선택 액터 외형 등록"))
				.ToolTipText(LOCTEXT(
					"RegisterSelectedActorTip",
					"레벨에서 선택한 액터의 메시·머티리얼·스케일을 팔레트 항목으로 추가합니다.\n블루프린트 액터를 선택하면 그 클래스가 그대로 등록되어 그리드에 배치할 수 있습니다."))
				.OnClicked(this, &SMapTileEditorWidget::OnRegisterSelectedActorClicked)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("RemoveSelectedTile", "선택 타일 제거"))
				.OnClicked(this, &SMapTileEditorWidget::OnRemoveSelectedTileClicked)
			]
		];
}

TSharedRef<ITableRow> SMapTileEditorWidget::OnGenerateTileRow(
	FMapTileListEntryPtr Entry, const TSharedRef<STableViewBase>& OwnerTable)
{
	UMapTilePalette* Pal = Palette.Get();

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

	if (Pal && Entry.IsValid() && Pal->Tiles.IsValidIndex(Entry->TileIndex))
	{
		const FMapTileDef& Def = Pal->Tiles[Entry->TileIndex];

		// 썸네일: 액터 클래스가 있으면 그것을, 없으면 머티리얼 또는 메시를 보여줍니다.
		FAssetData ThumbnailAsset;
		if (!Def.ActorClass.IsNull())
		{
			ThumbnailAsset = FAssetData(Def.ActorClass.LoadSynchronous());
		}
		else if (!Def.MaterialOverrides.IsEmpty() && !Def.MaterialOverrides[0].IsNull())
		{
			ThumbnailAsset = FAssetData(Def.MaterialOverrides[0].LoadSynchronous());
		}
		else if (!Def.Mesh.IsNull())
		{
			ThumbnailAsset = FAssetData(Def.Mesh.LoadSynchronous());
		}

		TSharedPtr<FAssetThumbnail> Thumbnail =
			MakeShared<FAssetThumbnail>(ThumbnailAsset, TileThumbnailSize, TileThumbnailSize, ThumbnailPool);

		Row->AddSlot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SBox)
				.WidthOverride(static_cast<float>(TileThumbnailSize))
				.HeightOverride(static_cast<float>(TileThumbnailSize))
				[
					Thumbnail->MakeThumbnailWidget()
				]
			];

		const FIntPoint Footprint = Def.GetClampedFootprint();

		// 이름 아래에 레이어와 점유 크기를 함께 보여줍니다.
		const FText SubText = Def.IsMultiCell()
			? FText::Format(
				LOCTEXT("TileSubMulti", "{0} · {1}x{2}칸"),
				GetMapTileLayerText(Def.Layer),
				FText::AsNumber(Footprint.X),
				FText::AsNumber(Footprint.Y))
			: GetMapTileLayerText(Def.Layer);

		Row->AddSlot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(Def.GetDisplayName()))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(SubText)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.62f, 1.0f)))
				]
			];
	}

	return SNew(STableRow<FMapTileListEntryPtr>, OwnerTable)
		[
			Row
		];
}

void SMapTileEditorWidget::OnTileSelectionChanged(FMapTileListEntryPtr Entry, ESelectInfo::Type SelectInfo)
{
	SelectedTileIndex = Entry.IsValid() ? Entry->TileIndex : INDEX_NONE;
}

void SMapTileEditorWidget::RebuildTileList()
{
	RebuildTileBrushes();

	TileEntries.Reset();

	if (const UMapTilePalette* Pal = Palette.Get())
	{
		for (int32 Index = 0; Index < Pal->Tiles.Num(); ++Index)
		{
			TileEntries.Add(MakeShared<FMapTileListEntry>(Index));
		}
	}

	if (TileListView.IsValid())
	{
		TileListView->RequestListRefresh();

		if (TileEntries.IsValidIndex(SelectedTileIndex))
		{
			TileListView->SetSelection(TileEntries[SelectedTileIndex], ESelectInfo::Direct);
		}
	}
}

FReply SMapTileEditorWidget::OnRegisterSelectedActorClicked()
{
	UMapTilePalette* Pal = Palette.Get();
	if (!Pal || !GEditor)
	{
		return FReply::Handled();
	}

	AActor* SelectedActor = GEditor->GetSelectedActors()
		? Cast<AActor>(GEditor->GetSelectedActors()->GetTop(AActor::StaticClass()))
		: nullptr;

	if (!SelectedActor)
	{
		StatusText = LOCTEXT("NoActorSelected", "레벨에서 액터를 먼저 선택하세요.");

		return FReply::Handled();
	}

	FMapTileDef Def;
	Def.Scale = SelectedActor->GetActorScale3D();

	const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(SelectedActor);
	const UStaticMeshComponent* MeshComponent = MeshActor ? MeshActor->GetStaticMeshComponent() : nullptr;

	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		// 순수 스태틱 메시 액터는 메시 + 머티리얼 조합으로 등록합니다.
		Def.Mesh = MeshComponent->GetStaticMesh();

		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 SlotIndex = 0; SlotIndex < MaterialCount; ++SlotIndex)
		{
			Def.MaterialOverrides.Add(MeshComponent->GetMaterial(SlotIndex));
		}

		// 충돌 프로파일도 함께 담습니다. 이게 없으면 배치된 타일이 액터 기본값을 써서
		// 원본과 충돌 거동이 달라집니다.
		Def.CollisionProfileName = MeshComponent->GetCollisionProfileName();

		if (const UMaterialInterface* FirstMaterial = MeshComponent->GetMaterial(0))
		{
			Def.DisplayName = FirstMaterial->GetName();
		}
		else
		{
			Def.DisplayName = MeshComponent->GetStaticMesh()->GetName();
		}
	}
	else
	{
		// 블루프린트를 포함한 그 밖의 액터는 클래스를 통째로 등록합니다. (KAN-575 요구)
		Def.ActorClass = SelectedActor->GetClass();
		Def.DisplayName = SelectedActor->GetActorLabel();

		// 스태틱 메시가 아닌 것은 바닥일 가능성이 낮으므로 프랍으로 둡니다.
		// 적·지하철처럼 액터 레이어가 맞으면 우측 "선택 타일"에서 바로 바꿀 수 있습니다.
		Def.Layer = EMapTileLayer::Prop;
	}

	// 캔버스에서 구분되도록 이름 해시로 미리보기 색을 만듭니다.
	const uint32 NameHash = GetTypeHash(Def.GetDisplayName());
	Def.PreviewColor = FLinearColor::MakeFromHSV8(
		static_cast<uint8>(NameHash % 256), 110, 210);

	Pal->Modify();
	Pal->Tiles.Add(Def);
	Pal->MarkPackageDirty();

	SelectedTileIndex = Pal->Tiles.Num() - 1;

	RebuildTileList();

	StatusText = FText::Format(
		LOCTEXT("TileRegistered", "'{0}' 을(를) 팔레트에 등록했습니다."), FText::FromString(Def.GetDisplayName()));

	return FReply::Handled();
}

FReply SMapTileEditorWidget::OnRemoveSelectedTileClicked()
{
	UMapTilePalette* Pal = Palette.Get();
	if (!Pal || !Pal->Tiles.IsValidIndex(SelectedTileIndex))
	{
		return FReply::Handled();
	}

	Pal->Modify();
	Pal->Tiles.RemoveAt(SelectedTileIndex);
	Pal->MarkPackageDirty();

	SelectedTileIndex = Pal->Tiles.Num() > 0 ? 0 : INDEX_NONE;

	RebuildTileList();

	return FReply::Handled();
}

TSharedRef<SWidget> SMapTileEditorWidget::BuildSettingsPanel()
{
	return SNew(SBorder)
		.Padding(6.0f)
		[
			SNew(SScrollBox)

			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("PlacementSettings", "배치 설정"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("PaintTool", "칠하기 도구"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f, 0.0f, 8.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.IsChecked(this, &SMapTileEditorWidget::IsBrushModeChecked, EMapTileBrushMode::Single)
						.OnCheckStateChanged(this, &SMapTileEditorWidget::OnBrushModeChanged, EMapTileBrushMode::Single)
						[
							SNew(STextBlock).Text(LOCTEXT("SingleBrush", "한 칸 브러시"))
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(10.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SMapTileEditorWidget::IsBrushModeChecked, EMapTileBrushMode::Rectangle)
						.OnCheckStateChanged(this, &SMapTileEditorWidget::OnBrushModeChanged, EMapTileBrushMode::Rectangle)
						[
							SNew(STextBlock).Text(LOCTEXT("RectangleBrush", "직사각형"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("CellSize", "셀 크기"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f, 0.0f, 8.0f)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(false)
					.Value(this, &SMapTileEditorWidget::GetCellSize)
					.OnValueCommitted_Lambda(
						[this](float NewValue, ETextCommit::Type) { OnCellSizeChanged(NewValue); })
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("RotationYaw", "회전 Yaw (도)"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f, 0.0f, 8.0f)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(false)
					.Value_Lambda([this]() { return TOptional<float>(BrushYaw); })
					.OnValueCommitted_Lambda(
						[this](float NewValue, ETextCommit::Type) { BrushYaw = NewValue; })
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda(
						[this]()
						{ return bLockViewportToSelection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda(
						[this](ECheckBoxState NewState)
						{ bLockViewportToSelection = (NewState == ECheckBoxState::Checked); })
					.ToolTipText(LOCTEXT(
						"LockViewportTip",
						"칠한 칸으로 레벨 뷰포트 카메라를 옮깁니다. 카메라 각도는 그대로 두므로 쿼터뷰를 유지한 채 작업할 수 있습니다."))
					[
						SNew(STextBlock).Text(LOCTEXT("LockViewport", "선택 위치에 뷰포트 고정"))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					BuildLayerSection()
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					BuildSelectedTileSection()
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(this, &SMapTileEditorWidget::GetStatusText)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT(
						"CanvasHelp",
						"한 칸 브러시: 클릭/연속 드래그\n"
						"직사각형: 드래그 범위 채우기\n"
						"우클릭: 지우기\n"
						"R: 시계 방향 90도 회전\n"
						"Alt+좌클릭: 스포이드\n"
						"휠: 확대/축소\n"
						"가운데 버튼 드래그: 이동\n"
						"Ctrl+Z: 되돌리기"))
				]
			]
		];
}

TSharedRef<SWidget> SMapTileEditorWidget::BuildLayerSection()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock).Text(LOCTEXT("LayerVisibility", "레이어 표시"))
		];

	for (int32 LayerIndex = 0; LayerIndex < MapTileLayerCount; ++LayerIndex)
	{
		const EMapTileLayer Layer = static_cast<EMapTileLayer>(LayerIndex);

		Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SMapTileEditorWidget::IsLayerVisible, Layer)
				.OnCheckStateChanged(this, &SMapTileEditorWidget::OnLayerVisibilityChanged, Layer)
				[
					SNew(STextBlock).Text(GetMapTileLayerText(Layer))
				]
			];
	}

	return Box;
}

ECheckBoxState SMapTileEditorWidget::IsLayerVisible(EMapTileLayer Layer) const
{
	return bLayerVisible[static_cast<int32>(Layer)] ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SMapTileEditorWidget::OnLayerVisibilityChanged(ECheckBoxState NewState, EMapTileLayer Layer)
{
	bLayerVisible[static_cast<int32>(Layer)] = (NewState == ECheckBoxState::Checked);
}

TSharedRef<SWidget> SMapTileEditorWidget::BuildSelectedTileSection()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock).Text(LOCTEXT("SelectedTile", "선택 타일"))
		];

	// 레이어 라디오입니다. 팔레트 애셋을 열지 않고 여기서 바로 바꿉니다.
	TSharedRef<SHorizontalBox> LayerRow = SNew(SHorizontalBox);

	for (int32 LayerIndex = 0; LayerIndex < MapTileLayerCount; ++LayerIndex)
	{
		const EMapTileLayer Layer = static_cast<EMapTileLayer>(LayerIndex);

		LayerRow->AddSlot()
			.AutoWidth()
			.Padding(LayerIndex == 0 ? 0.0f : 8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SMapTileEditorWidget::IsSelectedTileLayer, Layer)
				.OnCheckStateChanged(this, &SMapTileEditorWidget::OnSelectedTileLayerChanged, Layer)
				[
					SNew(STextBlock).Text(GetMapTileLayerText(Layer))
				]
			];
	}

	Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 6.0f)[LayerRow];

	Box->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock).Text(LOCTEXT("Footprint", "점유 칸 (가로 x 세로)"))
		];

	Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<int32>)
				.AllowSpin(false)
				.MinValue(1)
				.Value(this, &SMapTileEditorWidget::GetSelectedFootprint, true)
				.OnValueCommitted_Lambda(
					[this](int32 NewValue, ETextCommit::Type) { OnSelectedFootprintChanged(NewValue, true); })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SNumericEntryBox<int32>)
				.AllowSpin(false)
				.MinValue(1)
				.Value(this, &SMapTileEditorWidget::GetSelectedFootprint, false)
				.OnValueCommitted_Lambda(
					[this](int32 NewValue, ETextCommit::Type) { OnSelectedFootprintChanged(NewValue, false); })
			]
		];

	return Box;
}

ECheckBoxState SMapTileEditorWidget::IsSelectedTileLayer(EMapTileLayer Layer) const
{
	const FMapTileDef* Def = GetSelectedTileDef();

	return (Def && Def->Layer == Layer) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SMapTileEditorWidget::OnSelectedTileLayerChanged(ECheckBoxState NewState, EMapTileLayer Layer)
{
	if (NewState != ECheckBoxState::Checked)
	{
		return;
	}

	UMapTilePalette* Pal = Palette.Get();
	FMapTileDef* Def = GetSelectedTileDef();
	if (!Pal || !Def || Def->Layer == Layer)
	{
		return;
	}

	Pal->Modify();
	Def->Layer = Layer;
	Pal->MarkPackageDirty();

	RebuildTileList();

	// 레이어가 바뀌면 기존 배치의 소속도 달라지므로 다시 읽습니다.
	RefreshFromLevel();
}

TOptional<int32> SMapTileEditorWidget::GetSelectedFootprint(bool bAxisX) const
{
	const FMapTileDef* Def = GetSelectedTileDef();
	if (!Def)
	{
		return TOptional<int32>(1);
	}

	const FIntPoint Clamped = Def->GetClampedFootprint();

	return TOptional<int32>(bAxisX ? Clamped.X : Clamped.Y);
}

void SMapTileEditorWidget::OnSelectedFootprintChanged(int32 NewValue, bool bAxisX)
{
	UMapTilePalette* Pal = Palette.Get();
	FMapTileDef* Def = GetSelectedTileDef();
	if (!Pal || !Def)
	{
		return;
	}

	const int32 Clamped = FMath::Max(1, NewValue);

	if ((bAxisX ? Def->Footprint.X : Def->Footprint.Y) == Clamped)
	{
		return;
	}

	Pal->Modify();

	if (bAxisX)
	{
		Def->Footprint.X = Clamped;
	}
	else
	{
		Def->Footprint.Y = Clamped;
	}

	Pal->MarkPackageDirty();

	RebuildTileList();
	RefreshFromLevel();
}

EMapTileBrushMode SMapTileEditorWidget::GetBrushModeForCanvas() const
{
	return BrushMode;
}

FIntPoint SMapTileEditorWidget::GetBrushFootprintForCanvas() const
{
	const FMapTileDef* Def = GetSelectedTileDef();

	return Def ? FMapTileGrid::RotateFootprint(Def->GetClampedFootprint(), BrushYaw) : FIntPoint(1, 1);
}

bool SMapTileEditorWidget::CanPlaceRange(FIntPoint Min, FIntPoint Max) const
{
	const FMapTileDef* Def = GetSelectedTileDef();
	if (!Def)
	{
		return true;
	}

	const FIntPoint Origin(FMath::Min(Min.X, Max.X), FMath::Min(Min.Y, Max.Y));
	const FIntPoint Footprint(FMath::Abs(Max.X - Min.X) + 1, FMath::Abs(Max.Y - Min.Y) + 1);

	return Grid.IsFootprintClear(Origin, Footprint, Def->Layer);
}

ECheckBoxState SMapTileEditorWidget::IsBrushModeChecked(EMapTileBrushMode Mode) const
{
	return BrushMode == Mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SMapTileEditorWidget::OnBrushModeChanged(ECheckBoxState NewState, EMapTileBrushMode Mode)
{
	// 두 체크박스는 라디오처럼 동작합니다. 체크된 쪽이 현재 모드가 됩니다.
	if (NewState == ECheckBoxState::Checked)
	{
		BrushMode = Mode;
	}
}

TOptional<float> SMapTileEditorWidget::GetCellSize() const
{
	const UMapTilePalette* Pal = Palette.Get();

	return TOptional<float>(Pal ? Pal->CellSize : 800.0f);
}

void SMapTileEditorWidget::OnCellSizeChanged(float NewValue)
{
	UMapTilePalette* Pal = Palette.Get();
	if (!Pal || NewValue <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	Pal->Modify();
	Pal->CellSize = NewValue;
	Pal->MarkPackageDirty();

	// 셀 크기가 바뀌면 그리드 해석이 통째로 달라지므로 다시 스캔합니다.
	RefreshFromLevel();
}

FText SMapTileEditorWidget::GetStatusText() const
{
	return StatusText;
}

FMapTileCellVisual SMapTileEditorWidget::GetCellVisual(FIntPoint Cell) const
{
	FMapTileCellVisual Visual;

	const FMapTileCellStack* Stack = Grid.FindStack(Cell);
	if (!Stack)
	{
		return Visual;
	}

	const UMapTilePalette* Pal = Palette.Get();

	for (int32 LayerIndex = 0; LayerIndex < MapTileLayerCount; ++LayerIndex)
	{
		const FMapTileCell& Entry = Stack->Layers[LayerIndex];
		if (!Entry.bOccupied || !bLayerVisible[LayerIndex])
		{
			continue;
		}

		// 툴 밖에서 액터가 지워졌으면 그리지 않습니다.
		if (!Entry.Actor.IsValid())
		{
			continue;
		}

		Visual.bFilled = true;

		// 여러 칸을 덮는 배치는 대표 칸에서 한 번만 그립니다.
		if (!Entry.IsOrigin(Cell))
		{
			continue;
		}

		FMapTileLayerVisual& LayerVisual = Visual.Layers[LayerIndex];
		LayerVisual.bDraw = true;
		LayerVisual.bFixed = Entry.bFixed;
		LayerVisual.Footprint = Entry.Footprint;

		const int32 TileIndex = Pal ? Pal->FindTileIndexById(Entry.TileId) : INDEX_NONE;

		LayerVisual.Color = (Pal && Pal->Tiles.IsValidIndex(TileIndex))
			? Pal->Tiles[TileIndex].PreviewColor
			// 팔레트로 식별되지 않은 기존 바닥 타일입니다.
			: FLinearColor(0.55f, 0.58f, 0.62f, 1.0f);

		if (Pal && Pal->Tiles.IsValidIndex(TileIndex))
		{
			if (const FTileBrushEntry* BrushEntry = TileBrushes.Find(Pal->Tiles[TileIndex].TileId))
			{
				LayerVisual.Brush = BrushEntry->Brush.Get();
			}
		}
	}

	return Visual;
}

FMapTileDef* SMapTileEditorWidget::GetSelectedTileDef() const
{
	UMapTilePalette* Pal = Palette.Get();

	return (Pal && Pal->Tiles.IsValidIndex(SelectedTileIndex)) ? &Pal->Tiles[SelectedTileIndex] : nullptr;
}

UTexture2D* SMapTileEditorWidget::FindPreviewTexture(const FMapTileDef& Def)
{
	// 머티리얼 오버라이드가 있으면 그것을, 없으면 메시의 첫 슬롯 머티리얼을 본다.
	UMaterialInterface* Material = nullptr;

	if (!Def.MaterialOverrides.IsEmpty())
	{
		Material = Def.MaterialOverrides[0].LoadSynchronous();
	}

	if (!Material && !Def.Mesh.IsNull())
	{
		if (UStaticMesh* Mesh = Def.Mesh.LoadSynchronous())
		{
			const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
			if (!Materials.IsEmpty())
			{
				Material = Materials[0].MaterialInterface;
			}
		}
	}

	if (!Material)
	{
		return nullptr;
	}

	// 베이스 컬러로 흔히 쓰는 파라미터 이름을 먼저 시도한다.
	static const TCHAR* const BaseColorParams[] = {
		TEXT("BaseColor"), TEXT("Base Color"), TEXT("Albedo"), TEXT("Diffuse"), TEXT("Texture")
	};

	for (const TCHAR* ParamName : BaseColorParams)
	{
		UTexture* ParamTexture = nullptr;
		if (Material->GetTextureParameterValue(FName(ParamName), ParamTexture) && ParamTexture)
		{
			if (UTexture2D* Texture2D = Cast<UTexture2D>(ParamTexture))
			{
				return Texture2D;
			}
		}
	}

	// 파라미터로 못 찾으면 머티리얼이 쓰는 텍스처 중 첫 2D를 쓴다. 미리보기 용도라 이 정도로 충분하다.
	TArray<UTexture*> UsedTextures;
	Material->GetUsedTextures(
		UsedTextures, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::Num, true);

	for (UTexture* Used : UsedTextures)
	{
		if (UTexture2D* Texture2D = Cast<UTexture2D>(Used))
		{
			return Texture2D;
		}
	}

	return nullptr;
}

void SMapTileEditorWidget::RebuildTileBrushes()
{
	TileBrushes.Reset();

	const UMapTilePalette* Pal = Palette.Get();
	if (!Pal)
	{
		return;
	}

	for (const FMapTileDef& Def : Pal->Tiles)
	{
		UTexture2D* Texture = FindPreviewTexture(Def);
		if (!Texture)
		{
			continue;
		}

		FTileBrushEntry Entry;
		Entry.Texture = TStrongObjectPtr<UTexture2D>(Texture);
		Entry.Brush = MakeShared<FSlateBrush>();
		Entry.Brush->SetResourceObject(Texture);
		Entry.Brush->ImageSize = FVector2D(64.0f, 64.0f);
		Entry.Brush->DrawAs = ESlateBrushDrawType::Image;

		TileBrushes.Add(Def.TileId, MoveTemp(Entry));
	}
}

void SMapTileEditorWidget::HandleCellsPainted(const TArray<FIntPoint>& Cells)
{
	UWorld* World = GetEditorWorld();
	UMapTilePalette* Pal = Palette.Get();

	if (!World || !Pal)
	{
		return;
	}

	if (!Pal->Tiles.IsValidIndex(SelectedTileIndex))
	{
		StatusText = LOCTEXT("NoTileSelected", "칠할 타일을 팔레트에서 먼저 고르세요.");

		return;
	}

	const FMapTileDef& Def = Pal->Tiles[SelectedTileIndex];
	const FIntPoint Footprint = FMapTileGrid::RotateFootprint(Def.GetClampedFootprint(), BrushYaw);

	// 범위 안 어디든 하나라도 기존 배치와 겹치면 스트로크 전체를 거부합니다.
	// 겹친 칸만 건너뛰면 여러 칸짜리 배치의 격자 크기가 어긋납니다.
	for (const FIntPoint& Cell : Cells)
	{
		if (!Grid.IsFootprintClear(Cell, Footprint, Def.Layer))
		{
			StatusText = LOCTEXT("PaintBlocked", "범위 안에 이미 채워진 칸이 있어 칠할 수 없습니다.");

			return;
		}
	}

	// 스트로크 하나가 트랜잭션 하나가 되어 Ctrl+Z 한 번에 통째로 되돌아갑니다.
	const FScopedTransaction Transaction(LOCTEXT("PaintTilesTransaction", "2D 맵 타일 칠하기"));

	int32 PaintedCount = 0;
	for (const FIntPoint& Cell : Cells)
	{
		if (Grid.PaintCell(World, Cell, SelectedTileIndex, BrushYaw))
		{
			++PaintedCount;
		}
	}

	StatusText = FText::Format(LOCTEXT("PaintedCells", "{0}칸을 칠했습니다."), FText::AsNumber(PaintedCount));
}

void SMapTileEditorWidget::HandleCellsErased(const TArray<FIntPoint>& Cells)
{
	if (!GetEditorWorld() || !Palette.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("EraseTilesTransaction", "2D 맵 타일 지우기"));

	int32 ErasedCount = 0;
	for (const FIntPoint& Cell : Cells)
	{
		// 보이는 레이어 중 가장 위의 것만 지웁니다. 숨긴 레이어는 건드리지 않습니다.
		if (Grid.EraseTopVisible(Cell, bLayerVisible))
		{
			++ErasedCount;
		}
	}

	StatusText = FText::Format(LOCTEXT("ErasedCells", "{0}칸을 지웠습니다."), FText::AsNumber(ErasedCount));
}

void SMapTileEditorWidget::HandleCellPicked(FIntPoint Cell)
{
	const UMapTilePalette* Pal = Palette.Get();
	const FMapTileCellStack* Stack = Grid.FindStack(Cell);

	if (!Pal || !Stack)
	{
		return;
	}

	// 보이는 레이어 중 가장 위의 것을 집습니다.
	const FMapTileCell* Found = nullptr;
	for (int32 LayerIndex = MapTileLayerCount - 1; LayerIndex >= 0; --LayerIndex)
	{
		if (bLayerVisible[LayerIndex] && Stack->Layers[LayerIndex].bOccupied)
		{
			Found = &Stack->Layers[LayerIndex];
			break;
		}
	}

	if (!Found)
	{
		return;
	}

	const int32 TileIndex = Pal->FindTileIndexById(Found->TileId);
	if (TileIndex == INDEX_NONE)
	{
		StatusText = LOCTEXT("PickUnknownTile", "이 칸은 팔레트에 등록되지 않은 타일입니다.");

		return;
	}

	SelectedTileIndex = TileIndex;
	BrushYaw = Found->Yaw;

	if (TileListView.IsValid() && TileEntries.IsValidIndex(TileIndex))
	{
		TileListView->SetSelection(TileEntries[TileIndex], ESelectInfo::Direct);
	}

	StatusText = FText::Format(
		LOCTEXT("PickedTile", "'{0}' 을(를) 집었습니다."), FText::FromString(Pal->Tiles[TileIndex].GetDisplayName()));
}

void SMapTileEditorWidget::HandleCellFocused(FIntPoint Cell)
{
	if (bLockViewportToSelection)
	{
		MoveViewportToCell(Cell);
	}
}

void SMapTileEditorWidget::HandleRotateBrush()
{
	BrushYaw = FMath::Fmod(BrushYaw + 90.0f, 360.0f);

	StatusText = FText::Format(LOCTEXT("BrushRotated", "브러시 회전: {0}도"), FText::AsNumber(BrushYaw));
}

UWorld* SMapTileEditorWidget::GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

void SMapTileEditorWidget::RefreshFromLevel()
{
	if (!Palette.IsValid())
	{
		Grid.Clear();
		StatusText = LOCTEXT("NoPaletteStatus", "타일 팔레트를 선택하거나 새로 만드세요.");

		return;
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Grid.Clear();
		StatusText = LOCTEXT("NoWorld", "편집 중인 레벨을 찾지 못했습니다.");

		return;
	}

	const int32 LoadedCount = Grid.RefreshFromLevel(World);

	StatusText = FText::Format(
		LOCTEXT("LoadedTiles", "현재 레벨의 고정 바닥 타일 {0}개를 불러왔습니다."), FText::AsNumber(LoadedCount));
}

void SMapTileEditorWidget::MoveViewportToCell(const FIntPoint& Cell) const
{
	if (!GEditor)
	{
		return;
	}

	const FVector CellWorld = Grid.CellToWorld(Cell);

	for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
	{
		if (!ViewportClient || !ViewportClient->IsPerspective())
		{
			continue;
		}

		// 카메라 각도는 건드리지 않고 위치만 옮깁니다.
		// KAN-575의 "현재 쿼터뷰 카메라 각도에 맞게 작업" 요구를 이렇게 만족시킵니다.
		const FRotator ViewRotation = ViewportClient->GetViewRotation();
		const FVector NewLocation = CellWorld - ViewRotation.Vector() * ViewportFocusDistance;

		ViewportClient->SetViewLocation(NewLocation);
		ViewportClient->Invalidate();
	}
}

#undef LOCTEXT_NAMESPACE
