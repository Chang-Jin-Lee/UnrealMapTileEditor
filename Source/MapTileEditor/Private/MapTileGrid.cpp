#include "MapTileGrid.h"

#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace MapTileTags
{
	const FName Owned(TEXT("MapTile"));
	const TCHAR* const TileIdPrefix = TEXT("MapTileId=");
}

float FMapTileGrid::GetCellSize() const
{
	const UMapTilePalette* Pal = Palette.Get();
	return (Pal && Pal->CellSize > KINDA_SMALL_NUMBER) ? Pal->CellSize : 800.0f;
}

FVector FMapTileGrid::GetGridOrigin() const
{
	const UMapTilePalette* Pal = Palette.Get();
	return Pal ? Pal->GridOrigin : FVector::ZeroVector;
}

float FMapTileGrid::GetGridYaw() const
{
	const UMapTilePalette* Pal = Palette.Get();
	return Pal ? Pal->GridYaw : 0.0f;
}

FVector2D FMapTileGrid::GridToWorldOffset(float GridX, float GridY) const
{
	// 격자 축을 GridYaw만큼 돌려 월드 오프셋으로 바꿉니다.
	const float Rad = FMath::DegreesToRadians(GetGridYaw());
	const float CosYaw = FMath::Cos(Rad);
	const float SinYaw = FMath::Sin(Rad);

	return FVector2D(
		GridX * CosYaw - GridY * SinYaw,
		GridX * SinYaw + GridY * CosYaw);
}

FVector2D FMapTileGrid::WorldToGridOffset(float WorldX, float WorldY) const
{
	// GridToWorldOffset의 역회전입니다.
	const float Rad = FMath::DegreesToRadians(-GetGridYaw());
	const float CosYaw = FMath::Cos(Rad);
	const float SinYaw = FMath::Sin(Rad);

	return FVector2D(
		WorldX * CosYaw - WorldY * SinYaw,
		WorldX * SinYaw + WorldY * CosYaw);
}

FVector FMapTileGrid::CellToWorld(const FIntPoint& Cell) const
{
	const float Size = GetCellSize();
	const FVector Origin = GetGridOrigin();
	const FVector2D Offset = GridToWorldOffset(Cell.X * Size, Cell.Y * Size);

	return FVector(Origin.X + Offset.X, Origin.Y + Offset.Y, Origin.Z);
}

FVector FMapTileGrid::FootprintCenterToWorld(const FIntPoint& Origin, const FIntPoint& Footprint) const
{
	const float Size = GetCellSize();
	const FVector GridOriginWorld = GetGridOrigin();

	// 격자 좌표계에서 점유 영역의 중심을 구한 뒤 한 번에 회전시킵니다.
	const float CenterX = (Origin.X + (Footprint.X - 1) * 0.5f) * Size;
	const float CenterY = (Origin.Y + (Footprint.Y - 1) * 0.5f) * Size;
	const FVector2D Offset = GridToWorldOffset(CenterX, CenterY);

	return FVector(GridOriginWorld.X + Offset.X, GridOriginWorld.Y + Offset.Y, GridOriginWorld.Z);
}

FIntPoint FMapTileGrid::WorldToCell(const FVector& WorldLocation) const
{
	const float Size = GetCellSize();
	const FVector Origin = GetGridOrigin();
	const FVector2D Local = WorldToGridOffset(WorldLocation.X - Origin.X, WorldLocation.Y - Origin.Y);

	return FIntPoint(FMath::RoundToInt(Local.X / Size), FMath::RoundToInt(Local.Y / Size));
}

void FMapTileGrid::EnumerateFootprint(const FIntPoint& Origin, const FIntPoint& Footprint, TArray<FIntPoint>& OutCells)
{
	const int32 SizeX = FMath::Max(1, Footprint.X);
	const int32 SizeY = FMath::Max(1, Footprint.Y);

	OutCells.Reset();
	OutCells.Reserve(SizeX * SizeY);

	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			OutCells.Emplace(Origin.X + X, Origin.Y + Y);
		}
	}
}

bool FMapTileGrid::ParseTileIdFromActor(const AActor* Actor, FGuid& OutTileId)
{
	if (!Actor)
	{
		return false;
	}

	for (const FName& Tag : Actor->Tags)
	{
		const FString TagString = Tag.ToString();
		if (TagString.StartsWith(MapTileTags::TileIdPrefix))
		{
			const FString GuidPart = TagString.RightChop(FCString::Strlen(MapTileTags::TileIdPrefix));
			return FGuid::Parse(GuidPart, OutTileId);
		}
	}

	return false;
}

bool FMapTileGrid::IsScannableFloorActor(const AActor* Actor) const
{
	const UMapTilePalette* Pal = Palette.Get();
	if (!Pal || !Actor)
	{
		return false;
	}

	// 스태틱 메시 액터만 바닥 타일 후보로 본다.
	const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
	if (!MeshActor)
	{
		return false;
	}

	// 메시 이름 필터가 있으면 그것부터 통과해야 한다.
	// 이게 없으면 Z 범위 안의 장식물(유도블록 등)까지 바닥 타일로 잡혀 함께 지워진다.
	if (!Pal->ScanMeshNameFilters.IsEmpty())
	{
		const UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent();
		const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			return false;
		}

		const FString MeshName = Mesh->GetName();
		const bool bMatches = Pal->ScanMeshNameFilters.ContainsByPredicate(
			[&MeshName](const FString& Filter)
			{ return !Filter.IsEmpty() && MeshName.Contains(Filter); });

		if (!bMatches)
		{
			return false;
		}
	}

	const FVector Location = Actor->GetActorLocation();

	// 원점 높이에서 허용 범위를 벗어나면 바닥이 아니라고 본다.
	if (FMath::Abs(Location.Z - Pal->GridOrigin.Z) > Pal->ScanHeightTolerance)
	{
		return false;
	}

	// 격자에 떨어지는지 확인한다.
	// 오차는 반드시 격자 축 기준으로 재야 한다. 월드 축으로 재면 격자가 회전한 맵에서
	// 같은 셀 안의 타일이 최대 셀크기x0.707 만큼 벗어난 것으로 잡혀 스캔에서 빠진다.
	const float Size = GetCellSize();
	const FVector GridOriginWorld = GetGridOrigin();
	const FVector2D Local = WorldToGridOffset(Location.X - GridOriginWorld.X, Location.Y - GridOriginWorld.Y);

	const float ResidualU = FMath::Abs(Local.X - FMath::RoundToFloat(Local.X / Size) * Size);
	const float ResidualV = FMath::Abs(Local.Y - FMath::RoundToFloat(Local.Y / Size) * Size);

	// 허용 오차가 셀 크기의 절반이면 "셀 안에 있는 것은 모두" 라는 뜻이 된다.
	const float Tolerance = FMath::Max(0.0f, Pal->ScanSnapTolerance);

	return ResidualU <= Tolerance && ResidualV <= Tolerance;
}

int32 FMapTileGrid::RefreshFromLevel(UWorld* World)
{
	Cells.Reset();

	UMapTilePalette* Pal = Palette.Get();
	if (!World || !Pal)
	{
		return 0;
	}

	int32 PlacementCount = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		const bool bOwned = Actor->Tags.Contains(MapTileTags::Owned);
		if (!bOwned && !IsScannableFloorActor(Actor))
		{
			continue;
		}

		// 레이어와 점유 크기는 팔레트 정의에서 되찾습니다. 액터에 따로 기록하지 않습니다.
		EMapTileLayer Layer = EMapTileLayer::Floor;
		FIntPoint Footprint(1, 1);
		FGuid ParsedId;

		if (bOwned && ParseTileIdFromActor(Actor, ParsedId))
		{
			const int32 TileIndex = Pal->FindTileIndexById(ParsedId);
			if (Pal->Tiles.IsValidIndex(TileIndex))
			{
				Layer = Pal->Tiles[TileIndex].Layer;
				Footprint = Pal->Tiles[TileIndex].GetClampedFootprint();
			}
		}
		else
		{
			ParsedId = FGuid();
		}

		// 액터는 점유 영역의 중심에 있으므로, 격자 좌표계에서 좌상단 칸을 되계산합니다.
		const FVector ActorLocation = Actor->GetActorLocation();
		const float Size = GetCellSize();
		const FVector GridOriginWorld = GetGridOrigin();
		const FVector2D LocalCenter =
			WorldToGridOffset(ActorLocation.X - GridOriginWorld.X, ActorLocation.Y - GridOriginWorld.Y);

		const FIntPoint OriginCell(
			FMath::RoundToInt(LocalCenter.X / Size - (Footprint.X - 1) * 0.5f),
			FMath::RoundToInt(LocalCenter.Y / Size - (Footprint.Y - 1) * 0.5f));

		TArray<FIntPoint> Covered;
		EnumerateFootprint(OriginCell, Footprint, Covered);

		for (const FIntPoint& Covering : Covered)
		{
			FMapTileCellStack& Stack = Cells.FindOrAdd(Covering);
			FMapTileCell& Entry = Stack.Get(Layer);

			// 이미 이 칸·이 레이어를 누가 쓰고 있으면 겹침으로 모아둡니다.
			// 툴이 놓은 것을 대표로 삼고, 원래 있던 것은 겹침 목록으로 밀어냅니다.
			if (Entry.bOccupied && Entry.Actor.Get() != Actor)
			{
				if (bOwned && Entry.bFixed)
				{
					Entry.OverlappingActors.AddUnique(Entry.Actor);
				}
				else
				{
					Entry.OverlappingActors.AddUnique(TWeakObjectPtr<AActor>(Actor));
					continue;
				}
			}

			Entry.bOccupied = true;
			Entry.TileId = ParsedId;
			Entry.Actor = Actor;
			Entry.Yaw = Actor->GetActorRotation().Yaw;
			Entry.bFixed = !bOwned;
			Entry.Origin = OriginCell;
			Entry.Footprint = Footprint;
		}

		++PlacementCount;
	}

	return PlacementCount;
}

AActor* FMapTileGrid::SpawnTileActor(
	UWorld* World, const FMapTileDef& Def, const FVector& Location, float Yaw, FName FolderPath)
{
	if (!World || !Def.IsValidDef())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transactional;

	// 격자가 돌아가 있으면 배치되는 액터도 같은 방향을 기본으로 갖습니다.
	const FRotator Rotation(0.0f, GetGridYaw() + Yaw + Def.YawOffset, 0.0f);
	const FVector FinalLocation = Location + Def.PlacementOffset;

	AActor* Spawned = nullptr;

	if (!Def.ActorClass.IsNull())
	{
		// 블루프린트/네이티브 액터 배치 경로입니다. (스크린도어, 기둥, 적, 지하철 등)
		if (UClass* LoadedClass = Def.ActorClass.LoadSynchronous())
		{
			Spawned = World->SpawnActor<AActor>(LoadedClass, FinalLocation, Rotation, SpawnParams);
		}
	}
	else if (UStaticMesh* LoadedMesh = Def.Mesh.LoadSynchronous())
	{
		AStaticMeshActor* MeshActor =
			World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FinalLocation, Rotation, SpawnParams);

		if (MeshActor)
		{
			UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent();
			if (Component)
			{
				Component->SetMobility(EComponentMobility::Static);
				Component->SetStaticMesh(LoadedMesh);

				// 충돌 프로파일을 원본과 맞춥니다.
				// 이걸 빼면 원본이 BlockObjects여도 새 타일은 액터 기본값(BlockAll)이 되어 거동이 달라집니다.
				if (!Def.CollisionProfileName.IsNone())
				{
					Component->SetCollisionProfileName(Def.CollisionProfileName);
				}

				for (int32 SlotIndex = 0; SlotIndex < Def.MaterialOverrides.Num(); ++SlotIndex)
				{
					if (UMaterialInterface* Material = Def.MaterialOverrides[SlotIndex].LoadSynchronous())
					{
						Component->SetMaterial(SlotIndex, Material);
					}
				}
			}

			Spawned = MeshActor;
		}
	}

	if (!Spawned)
	{
		return nullptr;
	}

	Spawned->SetActorScale3D(Def.Scale);
	Spawned->Tags.Add(MapTileTags::Owned);
	Spawned->Tags.Add(FName(*FString::Printf(TEXT("%s%s"), MapTileTags::TileIdPrefix, *Def.TileId.ToString())));

	// 팔레트 이름을 액터 라벨에 반영해 아웃라이너에서 알아보기 쉽게 합니다.
	Spawned->SetActorLabel(FString::Printf(TEXT("Tile_%s"), *Def.GetDisplayName()));

	// 아웃라이너 폴더를 정합니다. 교체한 액터의 폴더가 있으면 그걸, 없으면 팔레트 기본값을 씁니다.
	FName TargetFolder = FolderPath;
	if (TargetFolder.IsNone())
	{
		if (const UMapTilePalette* Pal = Palette.Get())
		{
			TargetFolder = Pal->DefaultFolderPath;
		}
	}

	if (!TargetFolder.IsNone())
	{
		Spawned->SetFolderPath(TargetFolder);
	}

	return Spawned;
}

void FMapTileGrid::DestroyCellActors(UWorld* World, FMapTileCell& Entry)
{
	if (!World)
	{
		return;
	}

	// 대표 액터와 같은 칸에 겹쳐 있던 액터를 모두 없앱니다.
	TArray<TWeakObjectPtr<AActor>> ToDestroy = Entry.OverlappingActors;
	ToDestroy.AddUnique(Entry.Actor);

	for (const TWeakObjectPtr<AActor>& Weak : ToDestroy)
	{
		if (AActor* Target = Weak.Get())
		{
			Target->Modify();
			World->EditorDestroyActor(Target, /*bShouldModifyLevel=*/true);
		}
	}

	Entry.OverlappingActors.Reset();
}

void FMapTileGrid::RemovePlacementFromModel(const FIntPoint& AnyCell, EMapTileLayer Layer)
{
	const FMapTileCellStack* Stack = Cells.Find(AnyCell);
	if (!Stack)
	{
		return;
	}

	const FMapTileCell& Entry = Stack->Get(Layer);
	if (!Entry.bOccupied)
	{
		return;
	}

	// 덮고 있던 칸 전체에서 이 레이어를 비웁니다.
	TArray<FIntPoint> Covered;
	EnumerateFootprint(Entry.Origin, Entry.Footprint, Covered);

	for (const FIntPoint& Covering : Covered)
	{
		if (FMapTileCellStack* CoveredStack = Cells.Find(Covering))
		{
			CoveredStack->Get(Layer) = FMapTileCell();

			if (CoveredStack->IsEmpty())
			{
				Cells.Remove(Covering);
			}
		}
	}
}

bool FMapTileGrid::PaintCell(
	UWorld* World, const FIntPoint& Cell, int32 TileIndex, float BrushYaw, bool bKeepExistingHeight)
{
	UMapTilePalette* Pal = Palette.Get();
	if (!World || !Pal || !Pal->Tiles.IsValidIndex(TileIndex))
	{
		return false;
	}

	const FMapTileDef& Def = Pal->Tiles[TileIndex];
	if (!Def.IsValidDef())
	{
		return false;
	}

	const EMapTileLayer Layer = Def.Layer;
	const FIntPoint Footprint = Def.GetClampedFootprint();

	TArray<FIntPoint> Covered;
	EnumerateFootprint(Cell, Footprint, Covered);

	// 같은 레이어에서 겹치는 기존 배치를 먼저 걷어냅니다.
	// 겹친 것이 여러 칸짜리면 그 배치 전체가 사라집니다.
	float InheritedZ = TNumericLimits<float>::Lowest();

	// 지워지는 액터의 아웃라이너 폴더를 물려받아 레벨 정리를 유지합니다.
	FName InheritedFolder = NAME_None;

	for (const FIntPoint& Covering : Covered)
	{
		FMapTileCellStack* Stack = Cells.Find(Covering);
		if (!Stack)
		{
			continue;
		}

		FMapTileCell& Existing = Stack->Get(Layer);
		if (!Existing.bOccupied)
		{
			continue;
		}

		if (const AActor* ExistingActor = Existing.Actor.Get())
		{
			if (bKeepExistingHeight)
			{
				InheritedZ = FMath::Max(InheritedZ, ExistingActor->GetActorLocation().Z - Def.PlacementOffset.Z);
			}

			if (InheritedFolder.IsNone())
			{
				InheritedFolder = ExistingActor->GetFolderPath();
			}
		}

		DestroyCellActors(World, Existing);
		RemovePlacementFromModel(Covering, Layer);
	}

	FVector Location = FootprintCenterToWorld(Cell, Footprint);
	if (bKeepExistingHeight && InheritedZ > TNumericLimits<float>::Lowest())
	{
		Location.Z = InheritedZ;
	}

	AActor* Spawned = SpawnTileActor(World, Def, Location, BrushYaw, InheritedFolder);
	if (!Spawned)
	{
		return false;
	}

	for (const FIntPoint& Covering : Covered)
	{
		FMapTileCellStack& Stack = Cells.FindOrAdd(Covering);
		FMapTileCell& Entry = Stack.Get(Layer);

		Entry.bOccupied = true;
		Entry.TileId = Def.TileId;
		Entry.Actor = Spawned;
		Entry.Yaw = BrushYaw;
		Entry.bFixed = false;
		Entry.Origin = Cell;
		Entry.Footprint = Footprint;
	}

	return true;
}

bool FMapTileGrid::EraseLayer(const FIntPoint& Cell, EMapTileLayer Layer)
{
	FMapTileCellStack* Stack = Cells.Find(Cell);
	if (!Stack)
	{
		return false;
	}

	FMapTileCell& Entry = Stack->Get(Layer);
	if (!Entry.bOccupied)
	{
		return false;
	}

	if (const AActor* ExistingActor = Entry.Actor.Get())
	{
		DestroyCellActors(ExistingActor->GetWorld(), Entry);
	}

	RemovePlacementFromModel(Cell, Layer);

	return true;
}

bool FMapTileGrid::EraseTopVisible(const FIntPoint& Cell, const bool bLayerVisible[MapTileLayerCount])
{
	const FMapTileCellStack* Stack = Cells.Find(Cell);
	if (!Stack)
	{
		return false;
	}

	// 액터 → 프랍 → 바닥 순으로 위에서부터 찾습니다.
	for (int32 LayerIndex = MapTileLayerCount - 1; LayerIndex >= 0; --LayerIndex)
	{
		if (!bLayerVisible[LayerIndex])
		{
			continue;
		}

		if (Stack->Layers[LayerIndex].bOccupied)
		{
			return EraseLayer(Cell, static_cast<EMapTileLayer>(LayerIndex));
		}
	}

	return false;
}

bool FMapTileGrid::GetFilledBounds(FIntPoint& OutMin, FIntPoint& OutMax) const
{
	if (Cells.IsEmpty())
	{
		return false;
	}

	bool bFirst = true;

	for (const TPair<FIntPoint, FMapTileCellStack>& Pair : Cells)
	{
		if (bFirst)
		{
			OutMin = OutMax = Pair.Key;
			bFirst = false;
			continue;
		}

		OutMin.X = FMath::Min(OutMin.X, Pair.Key.X);
		OutMin.Y = FMath::Min(OutMin.Y, Pair.Key.Y);
		OutMax.X = FMath::Max(OutMax.X, Pair.Key.X);
		OutMax.Y = FMath::Max(OutMax.Y, Pair.Key.Y);
	}

	return true;
}
