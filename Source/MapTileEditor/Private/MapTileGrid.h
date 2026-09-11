/**
 *	@brief	그리드 셀과 레벨 액터를 잇는 모델입니다.
 *	@note	툴은 별도 저장본을 두지 않습니다. 레벨 자체가 원본이고 이 모델은 그 투영입니다.
 *			한 칸은 레이어마다 하나씩 배치를 가질 수 있고, 배치 하나는 여러 칸을 덮을 수 있습니다.
 */

#pragma once

#include "CoreMinimal.h"
#include "MapTilePalette.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class UWorld;

/** 툴이 배치한 액터에 붙는 태그입니다. */
namespace MapTileTags
{
	/** 이 액터가 맵 타일 편집기 소유임을 나타냅니다. */
	extern const FName Owned;

	/** "MapTileId=<guid>" 형태로 어떤 팔레트 타일에서 나왔는지 기록합니다. */
	extern const TCHAR* const TileIdPrefix;
}

/**
 * 한 칸의 한 레이어가 담고 있는 배치입니다.
 * 여러 칸을 덮는 배치는 덮인 모든 칸이 같은 Actor와 Origin을 가리킵니다.
 */
struct FMapTileCell
{
	/** 이 레이어가 채워져 있는지 여부입니다. */
	bool bOccupied = false;

	/** 이 칸을 채운 팔레트 타일입니다. 유효하지 않으면 팔레트로 식별되지 않은 기존 액터입니다. */
	FGuid TileId;

	/** 배치된 액터입니다. 여러 칸을 덮어도 액터는 하나입니다. */
	TWeakObjectPtr<AActor> Actor;

	/** 배치 시 적용된 Yaw입니다. */
	float Yaw = 0.0f;

	/** 툴이 배치한 것이 아니라 레벨에 원래 있던 액터인지 여부입니다. */
	bool bFixed = false;

	/** 이 배치의 대표 칸(좌상단)입니다. 그리기는 이 칸에서만 합니다. */
	FIntPoint Origin = FIntPoint::ZeroValue;

	/** 이 배치가 덮는 칸 수입니다. */
	FIntPoint Footprint = FIntPoint(1, 1);

	/**
	 * 같은 칸·같은 레이어에 겹쳐 있던 다른 액터들입니다.
	 * 손으로 지은 맵은 타일이 한 칸 안에 두 개씩 들어가 있는 곳이 있습니다.
	 * 이걸 들고 있지 않으면 덮어 칠할 때 한 개만 교체되고 나머지가 남습니다.
	 */
	TArray<TWeakObjectPtr<AActor>> OverlappingActors;

	/** 이 칸이 배치의 대표 칸인지 여부입니다. */
	bool IsOrigin(const FIntPoint& Cell) const { return Origin == Cell; }
};

/** 한 칸의 레이어별 상태입니다. */
struct FMapTileCellStack
{
	FMapTileCell Layers[MapTileLayerCount];

	const FMapTileCell& Get(EMapTileLayer Layer) const { return Layers[static_cast<int32>(Layer)]; }
	FMapTileCell& Get(EMapTileLayer Layer) { return Layers[static_cast<int32>(Layer)]; }

	bool IsEmpty() const
	{
		for (const FMapTileCell& Cell : Layers)
		{
			if (Cell.bOccupied)
			{
				return false;
			}
		}

		return true;
	}
};

/**
 * 셀 좌표와 월드 좌표를 오가며 레벨 액터를 만들고 지우는 모델입니다.
 * 모든 쓰기 작업은 호출한 쪽에서 FScopedTransaction으로 감싸는 것을 전제로 합니다.
 */
class FMapTileGrid
{
public:
	/** 편집 대상 팔레트를 지정합니다. */
	void SetPalette(UMapTilePalette* InPalette) { Palette = InPalette; }

	UMapTilePalette* GetPalette() const { return Palette.Get(); }

	/** 셀 한 변의 월드 크기입니다. 팔레트가 없으면 기본값을 돌려줍니다. */
	float GetCellSize() const;

	/** 그리드 원점입니다. */
	FVector GetGridOrigin() const;

	/** 격자 전체의 Yaw 회전(도)입니다. */
	float GetGridYaw() const;

	/** 격자 좌표계의 오프셋을 월드 오프셋으로 회전시킵니다. */
	FVector2D GridToWorldOffset(float GridX, float GridY) const;

	/** 월드 오프셋을 격자 좌표계로 역회전시킵니다. */
	FVector2D WorldToGridOffset(float WorldX, float WorldY) const;

	/** 셀 중심의 월드 위치를 구합니다. */
	FVector CellToWorld(const FIntPoint& Cell) const;

	/** 점유 영역의 중심 월드 위치를 구합니다. 여러 칸을 덮는 배치의 액터 위치입니다. */
	FVector FootprintCenterToWorld(const FIntPoint& Origin, const FIntPoint& Footprint) const;

	/** 월드 위치가 속한 셀을 구합니다. */
	FIntPoint WorldToCell(const FVector& WorldLocation) const;

	/** 현재 레벨을 훑어 그리드를 다시 만듭니다. 읽어들인 배치 수를 돌려줍니다. */
	int32 RefreshFromLevel(UWorld* World);

	/**
	 * 한 배치를 놓습니다. Cell은 점유 영역의 좌상단이 됩니다.
	 * 같은 레이어에서 점유 영역이 기존 배치와 하나라도 겹치면 아무것도 바꾸지 않고 false를 돌려줍니다.
	 */
	bool PaintCell(UWorld* World, const FIntPoint& Cell, int32 TileIndex, float BrushYaw);

	/** 점유 영역이 해당 레이어의 기존 배치와 하나도 겹치지 않으면 true를 돌려줍니다. */
	bool IsFootprintClear(const FIntPoint& Origin, const FIntPoint& Footprint, EMapTileLayer Layer) const;

	/** 지정한 레이어의 배치를 지웁니다. 덮인 칸 전체가 함께 비워집니다. */
	bool EraseLayer(const FIntPoint& Cell, EMapTileLayer Layer);

	/**
	 * 보이는 레이어 중 가장 위(액터 → 프랍 → 바닥)의 배치를 지웁니다.
	 * @param bLayerVisible	레이어별 표시 여부입니다. 숨긴 레이어는 지우지 않습니다.
	 */
	bool EraseTopVisible(const FIntPoint& Cell, const bool bLayerVisible[MapTileLayerCount]);

	/** 칸의 레이어별 상태를 조회합니다. 없으면 nullptr입니다. */
	const FMapTileCellStack* FindStack(const FIntPoint& Cell) const { return Cells.Find(Cell); }

	const TMap<FIntPoint, FMapTileCellStack>& GetCells() const { return Cells; }

	/** 채워진 칸들의 경계입니다. 비어 있으면 false를 돌려줍니다. */
	bool GetFilledBounds(FIntPoint& OutMin, FIntPoint& OutMax) const;

	/** 모델만 비웁니다. 레벨 액터는 건드리지 않습니다. */
	void Clear() { Cells.Reset(); }

	/**
	 * 팔레트에 정의된 Footprint를 배치 Yaw만큼 돌린 값을 돌려줍니다.
	 * 90도 단위로만 의미가 있습니다. 90·270도(4로 나눈 나머지가 홀수)면 X·Y를 맞바꾸고,
	 * 0·180도면 그대로 둡니다. 90도 단위가 아닌 값은 가장 가까운 90도로 반올림해 처리합니다.
	 */
	static FIntPoint RotateFootprint(const FIntPoint& Footprint, float Yaw);

private:
	/** 액터 태그에서 팔레트 타일 식별자를 뽑아냅니다. */
	static bool ParseTileIdFromActor(const AActor* Actor, FGuid& OutTileId);

	/** 팔레트에 없는 기존 액터가 바닥 타일로 볼 만한지 판단합니다. */
	bool IsScannableFloorActor(const AActor* Actor) const;

	/**
	 * 정의대로 액터를 스폰하고 태그를 붙입니다.
	 * @param FolderPath	아웃라이너 폴더입니다. 비어 있으면 팔레트 기본 폴더를 씁니다.
	 */
	AActor* SpawnTileActor(
		UWorld* World, const FMapTileDef& Def, const FVector& Location, float Yaw, FName FolderPath);

	/** 점유 영역이 덮는 모든 칸을 돌려줍니다. */
	static void EnumerateFootprint(const FIntPoint& Origin, const FIntPoint& Footprint, TArray<FIntPoint>& OutCells);

	/** 한 배치를 모델에서만 제거합니다. 액터 파괴는 호출한 쪽에서 합니다. */
	void RemovePlacementFromModel(const FIntPoint& AnyCell, EMapTileLayer Layer);

	/** 한 칸의 배치가 들고 있는 액터를 전부(겹친 것까지) 파괴합니다. */
	static void DestroyCellActors(UWorld* World, FMapTileCell& Entry);

private:
	TWeakObjectPtr<UMapTilePalette> Palette;

	TMap<FIntPoint, FMapTileCellStack> Cells;
};
