/**
 *	@brief	맵 타일 편집기의 메인 위젯입니다. 툴바·팔레트·캔버스·배치 설정을 조립합니다.
 */

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "MapTileGrid.h"
#include "SMapTileCanvas.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FAssetThumbnailPool;
class SMapTileCanvas;
class UMapTilePalette;

/** 좌측 목록의 한 행이 참조하는 항목입니다. */
struct FMapTileListEntry
{
	int32 TileIndex = INDEX_NONE;

	explicit FMapTileListEntry(int32 InIndex)
		: TileIndex(InIndex)
	{
	}
};

using FMapTileListEntryPtr = TSharedPtr<FMapTileListEntry>;

/**
 * 되돌리기/다시하기로 레벨이 바뀌면 그리드 모델이 낡으므로 FEditorUndoClient로 알림을 받습니다.
 * 모델은 트랜잭션 대상이 아니라서 이 알림이 없으면 캔버스가 사라진 액터를 계속 그립니다.
 */
class SMapTileEditorWidget : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SMapTileEditorWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual ~SMapTileEditorWidget() override;

	//~ FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ 끝

private:
	//~ 툴바
	TSharedRef<SWidget> BuildToolbar();
	FString GetPalettePath() const;
	void OnPaletteChanged(const FAssetData& AssetData);
	FReply OnCreateNewPaletteClicked();
	FReply OnRefreshLevelClicked();
	FReply OnViewAllClicked();

	//~ 좌측 팔레트 패널
	TSharedRef<SWidget> BuildPalettePanel();
	TSharedRef<ITableRow> OnGenerateTileRow(FMapTileListEntryPtr Entry, const TSharedRef<STableViewBase>& OwnerTable);
	void OnTileSelectionChanged(FMapTileListEntryPtr Entry, ESelectInfo::Type SelectInfo);
	FReply OnRegisterSelectedActorClicked();
	FReply OnRemoveSelectedTileClicked();
	void RebuildTileList();

	//~ 우측 배치 설정
	TSharedRef<SWidget> BuildSettingsPanel();

	/** 레이어 표시 토글 묶음입니다. */
	TSharedRef<SWidget> BuildLayerSection();

	/** 선택한 타일의 레이어와 점유 크기를 바로 고치는 묶음입니다. */
	TSharedRef<SWidget> BuildSelectedTileSection();

	ECheckBoxState IsLayerVisible(EMapTileLayer Layer) const;
	void OnLayerVisibilityChanged(ECheckBoxState NewState, EMapTileLayer Layer);

	ECheckBoxState IsSelectedTileLayer(EMapTileLayer Layer) const;
	void OnSelectedTileLayerChanged(ECheckBoxState NewState, EMapTileLayer Layer);

	TOptional<int32> GetSelectedFootprint(bool bAxisX) const;
	void OnSelectedFootprintChanged(int32 NewValue, bool bAxisX);

	/** 선택된 타일 정의를 돌려줍니다. 없으면 nullptr입니다. */
	FMapTileDef* GetSelectedTileDef() const;
	/** 캔버스에 현재 브러시 모드를 넘겨주는 어트리뷰트 게터입니다. */
	EMapTileBrushMode GetBrushModeForCanvas() const;
	/** 캔버스에 현재 선택된 타일의 점유 칸(Footprint)을 넘겨주는 어트리뷰트 게터입니다. */
	FIntPoint GetBrushFootprintForCanvas() const;
	/** 캔버스가 호버·드래그 범위 미리보기 색을 정할 때 묻는 콜백입니다. 범위가 비어 있으면 놓을 수 있습니다. */
	bool CanPlaceRange(FIntPoint Min, FIntPoint Max) const;
	ECheckBoxState IsBrushModeChecked(EMapTileBrushMode Mode) const;
	void OnBrushModeChanged(ECheckBoxState NewState, EMapTileBrushMode Mode);
	TOptional<float> GetCellSize() const;
	void OnCellSizeChanged(float NewValue);
	FText GetStatusText() const;

	//~ 캔버스 연동
	FMapTileCellVisual GetCellVisual(FIntPoint Cell) const;
	void HandleCellsPainted(const TArray<FIntPoint>& Cells);
	void HandleCellsErased(const TArray<FIntPoint>& Cells);
	void HandleCellPicked(FIntPoint Cell);
	void HandleCellFocused(FIntPoint Cell);
	void HandleRotateBrush();

	//~ 셀 미리보기 브러시
	/** 팔레트의 각 타일에 대해 셀에 그릴 텍스처 브러시를 다시 만듭니다. */
	void RebuildTileBrushes();

	/** 타일 정의에서 대표 텍스처를 찾습니다. 없으면 nullptr입니다. */
	static UTexture2D* FindPreviewTexture(const FMapTileDef& Def);

	//~ 공통
	/** 현재 편집 중인 에디터 월드를 돌려줍니다. */
	static UWorld* GetEditorWorld();

	/** 레벨을 다시 스캔하고 상태 문구를 갱신합니다. */
	void RefreshFromLevel();

	/** 지정한 셀이 화면 중앙에 오도록 레벨 뷰포트 카메라를 옮깁니다. 각도는 유지합니다. */
	void MoveViewportToCell(const FIntPoint& Cell) const;

private:
	FMapTileGrid Grid;

	TWeakObjectPtr<UMapTilePalette> Palette;

	TSharedPtr<SMapTileCanvas> Canvas;
	TSharedPtr<SListView<FMapTileListEntryPtr>> TileListView;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	TArray<FMapTileListEntryPtr> TileEntries;

	int32 SelectedTileIndex = INDEX_NONE;

	/** 셀에 그릴 타일 텍스처입니다. 텍스처를 살려두기 위해 강한 참조로 잡습니다. */
	struct FTileBrushEntry
	{
		TStrongObjectPtr<UTexture2D> Texture;
		TSharedPtr<FSlateBrush> Brush;
	};

	TMap<FGuid, FTileBrushEntry> TileBrushes;

	EMapTileBrushMode BrushMode = EMapTileBrushMode::Single;

	float BrushYaw = 0.0f;

	bool bLockViewportToSelection = false;

	/** 레이어별 표시 여부입니다. 숨긴 레이어는 그리지도, 지우지도 않습니다. */
	bool bLayerVisible[MapTileLayerCount] = { true, true, true };

	/** 우측 패널에 표시할 상태 문구입니다. */
	FText StatusText;
};
