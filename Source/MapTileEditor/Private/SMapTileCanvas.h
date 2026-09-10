/**
 *	@brief	2D 그리드 캔버스 위젯입니다. 칠하기·지우기·스포이드·줌/팬을 처리합니다.
 */

#pragma once

#include "CoreMinimal.h"
#include "MapTilePalette.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/** 브러시 모드입니다. 레퍼런스의 "한 칸 브러시" / "직사각형"에 대응합니다. */
enum class EMapTileBrushMode : uint8
{
	Single,
	Rectangle
};

/** 한 칸의 한 레이어를 그릴 때 필요한 정보입니다. */
struct FMapTileLayerVisual
{
	/**
	 * 이 칸에서 이 레이어를 그려야 하는지 여부입니다.
	 * 여러 칸을 덮는 배치는 대표 칸에서만 true가 되어 한 번만 그려집니다.
	 */
	bool bDraw = false;

	bool bFixed = false;

	FLinearColor Color = FLinearColor::Transparent;

	/**
	 * 셀에 그릴 타일 텍스처 브러시입니다.
	 * 유효하면 이걸로 그리고, 없으면 Color 단색으로 대체합니다.
	 */
	const FSlateBrush* Brush = nullptr;

	/** 이 배치가 덮는 칸 수입니다. 대표 칸에서 이 크기만큼 그립니다. */
	FIntPoint Footprint = FIntPoint(1, 1);
};

/** 셀 하나를 그릴 때 필요한 정보입니다. */
struct FMapTileCellVisual
{
	/** 어느 레이어든 점유되어 있는지 여부입니다. */
	bool bFilled = false;

	FMapTileLayerVisual Layers[MapTileLayerCount];
};

DECLARE_DELEGATE_OneParam(FOnMapTileCellsPainted, const TArray<FIntPoint>&);
DECLARE_DELEGATE_OneParam(FOnMapTileCellsErased, const TArray<FIntPoint>&);
DECLARE_DELEGATE_OneParam(FOnMapTileCellPicked, FIntPoint);
DECLARE_DELEGATE_OneParam(FOnMapTileCellFocused, FIntPoint);
DECLARE_DELEGATE(FOnMapTileRotateBrush);
DECLARE_DELEGATE_RetVal_OneParam(FMapTileCellVisual, FGetMapTileCellVisual, FIntPoint);

class SMapTileCanvas : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMapTileCanvas)
		: _BrushMode(EMapTileBrushMode::Single)
		, _BrushFootprint(FIntPoint(1, 1))
	{
	}
		SLATE_ATTRIBUTE(EMapTileBrushMode, BrushMode)
		SLATE_ATTRIBUTE(FIntPoint, BrushFootprint)
		SLATE_EVENT(FGetMapTileCellVisual, OnGetCellVisual)
		SLATE_EVENT(FOnMapTileCellsPainted, OnCellsPainted)
		SLATE_EVENT(FOnMapTileCellsErased, OnCellsErased)
		SLATE_EVENT(FOnMapTileCellPicked, OnCellPicked)
		SLATE_EVENT(FOnMapTileCellFocused, OnCellFocused)
		SLATE_EVENT(FOnMapTileRotateBrush, OnRotateBrush)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 지정한 셀 범위가 모두 보이도록 줌과 위치를 맞춥니다. (레퍼런스의 "전체 보기") */
	void FocusOnCellRange(const FIntPoint& Min, const FIntPoint& Max);

	//~ SWidget 인터페이스
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(640.0f, 480.0f); }
	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	//~ 끝

private:
	/** 위젯 로컬 좌표를 셀 좌표로 바꿉니다. */
	FIntPoint LocalToCell(const FVector2D& LocalPosition) const;

	/** 셀 좌표의 좌상단 로컬 좌표를 구합니다. */
	FVector2D CellToLocal(const FIntPoint& Cell) const;

	/** 현재 확대율에서의 셀 픽셀 크기입니다. */
	float GetCellPixelSize() const { return BaseCellPixels * ZoomLevel; }

	/** 드래그 중 누적된 대상 셀을 확정해 델리게이트로 넘깁니다. */
	void CommitStroke();

	/** 두 셀이 이루는 직사각형 범위를 채워 목록으로 돌려줍니다. */
	static void BuildRectangle(const FIntPoint& A, const FIntPoint& B, TArray<FIntPoint>& OutCells);

private:
	TAttribute<EMapTileBrushMode> BrushMode;

	/** 현재 선택된 타일의 점유 칸 크기입니다. 호버 하이라이트를 이 크기로 그립니다. */
	TAttribute<FIntPoint> BrushFootprint;

	FGetMapTileCellVisual OnGetCellVisual;
	FOnMapTileCellsPainted OnCellsPainted;
	FOnMapTileCellsErased OnCellsErased;
	FOnMapTileCellPicked OnCellPicked;
	FOnMapTileCellFocused OnCellFocused;
	FOnMapTileRotateBrush OnRotateBrush;

	/** 확대율 1.0에서의 셀 픽셀 크기입니다. */
	float BaseCellPixels = 18.0f;

	float ZoomLevel = 1.0f;

	/** 좌상단에 표시할 셀 기준 스크롤 위치(픽셀)입니다. */
	FVector2D ViewOffset = FVector2D::ZeroVector;

	/** 마우스가 올라가 있는 셀입니다. */
	FIntPoint HoveredCell = FIntPoint::ZeroValue;
	bool bHasHoveredCell = false;

	/** 진행 중인 스트로크 상태입니다. */
	bool bPainting = false;
	bool bErasing = false;
	bool bPanning = false;
	FIntPoint StrokeStartCell = FIntPoint::ZeroValue;
	TArray<FIntPoint> StrokeCells;
	FVector2D LastPanPosition = FVector2D::ZeroVector;
};
