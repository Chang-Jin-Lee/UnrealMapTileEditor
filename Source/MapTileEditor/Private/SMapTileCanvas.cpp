#include "SMapTileCanvas.h"

#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** 축 눈금을 몇 칸마다 찍을지입니다. */
	constexpr int32 AxisLabelStride = 2;

	/** 셀이 이보다 작아지면 눈금 숫자를 생략합니다. */
	constexpr float MinPixelsForAxisLabels = 12.0f;

	constexpr float MinZoom = 0.25f;
	constexpr float MaxZoom = 6.0f;
}

void SMapTileCanvas::Construct(const FArguments& InArgs)
{
	BrushMode = InArgs._BrushMode;
	OnGetCellVisual = InArgs._OnGetCellVisual;
	OnCellsPainted = InArgs._OnCellsPainted;
	OnCellsErased = InArgs._OnCellsErased;
	OnCellPicked = InArgs._OnCellPicked;
	OnCellFocused = InArgs._OnCellFocused;
	OnRotateBrush = InArgs._OnRotateBrush;
}

FIntPoint SMapTileCanvas::LocalToCell(const FVector2D& LocalPosition) const
{
	const float CellPixels = GetCellPixelSize();
	const FVector2D GridSpace = (LocalPosition + ViewOffset) / CellPixels;

	return FIntPoint(FMath::FloorToInt(GridSpace.X), FMath::FloorToInt(GridSpace.Y));
}

FVector2D SMapTileCanvas::CellToLocal(const FIntPoint& Cell) const
{
	const float CellPixels = GetCellPixelSize();

	return FVector2D(Cell.X * CellPixels, Cell.Y * CellPixels) - ViewOffset;
}

void SMapTileCanvas::BuildRectangle(const FIntPoint& A, const FIntPoint& B, TArray<FIntPoint>& OutCells)
{
	const int32 MinX = FMath::Min(A.X, B.X);
	const int32 MaxX = FMath::Max(A.X, B.X);
	const int32 MinY = FMath::Min(A.Y, B.Y);
	const int32 MaxY = FMath::Max(A.Y, B.Y);

	OutCells.Reset();
	OutCells.Reserve((MaxX - MinX + 1) * (MaxY - MinY + 1));

	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			OutCells.Emplace(X, Y);
		}
	}
}

void SMapTileCanvas::FocusOnCellRange(const FIntPoint& Min, const FIntPoint& Max)
{
	const FVector2D LocalSize = GetTickSpaceGeometry().GetLocalSize();
	if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		return;
	}

	// 가장자리에 한 칸씩 여백을 둡니다.
	const float SpanX = static_cast<float>(Max.X - Min.X + 3);
	const float SpanY = static_cast<float>(Max.Y - Min.Y + 3);

	const float ZoomX = LocalSize.X / (SpanX * BaseCellPixels);
	const float ZoomY = LocalSize.Y / (SpanY * BaseCellPixels);

	ZoomLevel = FMath::Clamp(FMath::Min(ZoomX, ZoomY), MinZoom, MaxZoom);

	const float CellPixels = GetCellPixelSize();
	const FVector2D ContentCenter(
		(Min.X + Max.X + 1) * 0.5f * CellPixels,
		(Min.Y + Max.Y + 1) * 0.5f * CellPixels);

	ViewOffset = ContentCenter - LocalSize * 0.5f;
}

int32 SMapTileCanvas::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float CellPixels = GetCellPixelSize();

	if (CellPixels <= 0.0f || LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		return LayerId;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateFontInfo AxisFont = FCoreStyle::GetDefaultFontStyle("Regular", 7);

	// 화면에 보이는 셀 범위만 계산합니다.
	const FIntPoint TopLeft = LocalToCell(FVector2D::ZeroVector);
	const FIntPoint BottomRight = LocalToCell(LocalSize);

	int32 CurrentLayer = LayerId;

	// 배경입니다.
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		CurrentLayer,
		AllottedGeometry.ToPaintGeometry(),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.06f, 0.07f, 0.09f, 1.0f));

	++CurrentLayer;

	// 채워진 셀입니다. 레이어 순서대로(바닥 → 프랍 → 액터) 겹쳐 그립니다.
	if (OnGetCellVisual.IsBound())
	{
		// 위 레이어일수록 안쪽으로 줄여 그려, 아래 레이어가 테두리로 드러나게 합니다.
		static constexpr float LayerInset[MapTileLayerCount] = { 1.0f, 0.62f, 0.38f };

		// 확장된 범위까지 훑어야 화면 밖에서 시작하는 다중 칸 배치도 그려집니다.
		constexpr int32 FootprintScanMargin = 8;

		for (int32 Y = TopLeft.Y - FootprintScanMargin; Y <= BottomRight.Y; ++Y)
		{
			for (int32 X = TopLeft.X - FootprintScanMargin; X <= BottomRight.X; ++X)
			{
				const FIntPoint Cell(X, Y);
				const FMapTileCellVisual Visual = OnGetCellVisual.Execute(Cell);
				if (!Visual.bFilled)
				{
					continue;
				}

				const FVector2D CellTopLeft = CellToLocal(Cell);

				for (int32 LayerIndex = 0; LayerIndex < MapTileLayerCount; ++LayerIndex)
				{
					const FMapTileLayerVisual& LayerVisual = Visual.Layers[LayerIndex];
					if (!LayerVisual.bDraw)
					{
						continue;
					}

					const FVector2D FootprintSize(
						FMath::Max(1, LayerVisual.Footprint.X) * CellPixels - 1.0f,
						FMath::Max(1, LayerVisual.Footprint.Y) * CellPixels - 1.0f);

					const float Inset = LayerInset[LayerIndex];
					const FVector2D DrawSize = FootprintSize * Inset;
					const FVector2D DrawOffset = CellTopLeft + (FootprintSize - DrawSize) * 0.5f;

					// 타일 텍스처가 있으면 그것으로, 없으면 단색으로 그립니다.
					const FSlateBrush* CellBrush = LayerVisual.Brush ? LayerVisual.Brush : WhiteBrush;
					const FLinearColor BaseTint = LayerVisual.Brush ? FLinearColor::White : LayerVisual.Color;

					FSlateDrawElement::MakeBox(
						OutDrawElements,
						CurrentLayer + LayerIndex,
						AllottedGeometry.ToPaintGeometry(
							FVector2f(DrawSize), FSlateLayoutTransform(FVector2f(DrawOffset))),
						CellBrush,
						ESlateDrawEffect::None,
						// 레벨에 원래 있던 "고정" 타일은 어둡게 그려 툴이 놓은 것과 구분합니다.
						LayerVisual.bFixed ? BaseTint * 0.45f : BaseTint);

					// 여러 칸을 덮는 배치는 점유 범위를 테두리로 표시합니다.
					if (LayerVisual.Footprint.X > 1 || LayerVisual.Footprint.Y > 1)
					{
						TArray<FVector2D> Outline;
						Outline.Add(CellTopLeft);
						Outline.Add(CellTopLeft + FVector2D(FootprintSize.X, 0.0f));
						Outline.Add(CellTopLeft + FootprintSize);
						Outline.Add(CellTopLeft + FVector2D(0.0f, FootprintSize.Y));
						Outline.Add(CellTopLeft);

						FSlateDrawElement::MakeLines(
							OutDrawElements,
							CurrentLayer + MapTileLayerCount,
							AllottedGeometry.ToPaintGeometry(),
							Outline,
							ESlateDrawEffect::None,
							FLinearColor(1.0f, 0.85f, 0.3f, 0.75f),
							true,
							1.5f);
					}
				}
			}
		}
	}

	CurrentLayer += MapTileLayerCount + 1;

	// 격자선입니다.
	const FLinearColor GridColor(1.0f, 1.0f, 1.0f, 0.08f);

	for (int32 X = TopLeft.X; X <= BottomRight.X + 1; ++X)
	{
		const float LocalX = CellToLocal(FIntPoint(X, 0)).X;

		TArray<FVector2D> Line;
		Line.Add(FVector2D(LocalX, 0.0f));
		Line.Add(FVector2D(LocalX, LocalSize.Y));

		FSlateDrawElement::MakeLines(
			OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridColor, false);
	}

	for (int32 Y = TopLeft.Y; Y <= BottomRight.Y + 1; ++Y)
	{
		const float LocalY = CellToLocal(FIntPoint(0, Y)).Y;

		TArray<FVector2D> Line;
		Line.Add(FVector2D(0.0f, LocalY));
		Line.Add(FVector2D(LocalSize.X, LocalY));

		FSlateDrawElement::MakeLines(
			OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridColor, false);
	}

	++CurrentLayer;

	// 축 눈금입니다. 셀이 충분히 클 때만 그립니다.
	if (CellPixels >= MinPixelsForAxisLabels)
	{
		const FLinearColor AxisColor(1.0f, 1.0f, 1.0f, 0.45f);

		for (int32 X = TopLeft.X; X <= BottomRight.X; ++X)
		{
			if (X % AxisLabelStride != 0)
			{
				continue;
			}

			FSlateDrawElement::MakeText(
				OutDrawElements,
				CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(CellPixels, 12.0f),
					FSlateLayoutTransform(FVector2f(CellToLocal(FIntPoint(X, 0)).X + 2.0f, 1.0f))),
				FString::FromInt(X),
				AxisFont,
				ESlateDrawEffect::None,
				AxisColor);
		}

		for (int32 Y = TopLeft.Y; Y <= BottomRight.Y; ++Y)
		{
			if (Y % AxisLabelStride != 0)
			{
				continue;
			}

			FSlateDrawElement::MakeText(
				OutDrawElements,
				CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(24.0f, CellPixels),
					FSlateLayoutTransform(FVector2f(1.0f, CellToLocal(FIntPoint(0, Y)).Y + 1.0f))),
				FString::FromInt(Y),
				AxisFont,
				ESlateDrawEffect::None,
				AxisColor);
		}

		++CurrentLayer;
	}

	// 직사각형 모드로 드래그하는 동안의 미리보기입니다.
	if (bPainting && bHasHoveredCell && BrushMode.Get() == EMapTileBrushMode::Rectangle)
	{
		const FIntPoint Min(FMath::Min(StrokeStartCell.X, HoveredCell.X), FMath::Min(StrokeStartCell.Y, HoveredCell.Y));
		const FIntPoint Max(FMath::Max(StrokeStartCell.X, HoveredCell.X), FMath::Max(StrokeStartCell.Y, HoveredCell.Y));

		const FVector2D PreviewTopLeft = CellToLocal(Min);
		const FVector2D PreviewSize(
			(Max.X - Min.X + 1) * CellPixels,
			(Max.Y - Min.Y + 1) * CellPixels);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			CurrentLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(PreviewSize), FSlateLayoutTransform(FVector2f(PreviewTopLeft))),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.2f, 0.7f, 1.0f, 0.25f));

		++CurrentLayer;
	}

	// 커서가 올라간 칸을 강조합니다.
	if (bHasHoveredCell)
	{
		const FVector2D HoverTopLeft = CellToLocal(HoveredCell);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			CurrentLayer,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(CellPixels, CellPixels), FSlateLayoutTransform(FVector2f(HoverTopLeft))),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.3f, 0.9f, 0.9f, 0.20f));

		++CurrentLayer;
	}

	return CurrentLayer;
}

FReply SMapTileCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FIntPoint Cell = LocalToCell(LocalPosition);

	HoveredCell = Cell;
	bHasHoveredCell = true;

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bPanning = true;
		LastPanPosition = LocalPosition;

		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Alt+좌클릭은 스포이드입니다. 칠하지 않고 그 칸의 타일을 팔레트 선택으로 가져옵니다.
		if (MouseEvent.IsAltDown())
		{
			OnCellPicked.ExecuteIfBound(Cell);

			return FReply::Handled();
		}

		bPainting = true;
		StrokeStartCell = Cell;
		StrokeCells.Reset();

		if (BrushMode.Get() == EMapTileBrushMode::Single)
		{
			StrokeCells.Add(Cell);
		}

		OnCellFocused.ExecuteIfBound(Cell);

		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bErasing = true;
		StrokeStartCell = Cell;
		StrokeCells.Reset();

		if (BrushMode.Get() == EMapTileBrushMode::Single)
		{
			StrokeCells.Add(Cell);
		}

		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply SMapTileCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	if (bPanning)
	{
		ViewOffset -= (LocalPosition - LastPanPosition);
		LastPanPosition = LocalPosition;

		return FReply::Handled();
	}

	const FIntPoint Cell = LocalToCell(LocalPosition);
	const bool bCellChanged = !bHasHoveredCell || Cell != HoveredCell;

	HoveredCell = Cell;
	bHasHoveredCell = true;

	// 한 칸 브러시는 드래그하는 동안 지나간 칸을 계속 모읍니다.
	if (bCellChanged && (bPainting || bErasing) && BrushMode.Get() == EMapTileBrushMode::Single)
	{
		StrokeCells.AddUnique(Cell);
	}

	return FReply::Handled();
}

FReply SMapTileCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && bPanning)
	{
		bPanning = false;

		return FReply::Handled().ReleaseMouseCapture();
	}

	const bool bWasLeft = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPainting;
	const bool bWasRight = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bErasing;

	if (bWasLeft || bWasRight)
	{
		CommitStroke();

		bPainting = false;
		bErasing = false;

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void SMapTileCanvas::CommitStroke()
{
	TArray<FIntPoint> Cells;

	if (BrushMode.Get() == EMapTileBrushMode::Rectangle)
	{
		BuildRectangle(StrokeStartCell, HoveredCell, Cells);
	}
	else
	{
		Cells = StrokeCells;
	}

	if (Cells.IsEmpty())
	{
		return;
	}

	if (bErasing)
	{
		OnCellsErased.ExecuteIfBound(Cells);
	}
	else
	{
		OnCellsPainted.ExecuteIfBound(Cells);
	}

	StrokeCells.Reset();
}

FReply SMapTileCanvas::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// 커서 아래 지점이 고정되도록 확대합니다.
	const FVector2D GridPointBefore = (LocalPosition + ViewOffset) / GetCellPixelSize();

	const float ZoomStep = MouseEvent.GetWheelDelta() > 0.0f ? 1.15f : (1.0f / 1.15f);
	ZoomLevel = FMath::Clamp(ZoomLevel * ZoomStep, MinZoom, MaxZoom);

	ViewOffset = GridPointBefore * GetCellPixelSize() - LocalPosition;

	return FReply::Handled();
}

FReply SMapTileCanvas::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::R)
	{
		OnRotateBrush.ExecuteIfBound();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SMapTileCanvas::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	bHasHoveredCell = false;
}
