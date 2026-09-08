#include "MapTilePalette.h"

#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "MapTilePalette"

FText GetMapTileLayerText(EMapTileLayer Layer)
{
	switch (Layer)
	{
	case EMapTileLayer::Floor:
		return LOCTEXT("LayerFloor", "바닥");
	case EMapTileLayer::Prop:
		return LOCTEXT("LayerProp", "프랍");
	case EMapTileLayer::Actor:
		return LOCTEXT("LayerActor", "액터");
	default:
		return LOCTEXT("LayerUnknown", "알 수 없음");
	}
}

#undef LOCTEXT_NAMESPACE

FString FMapTileDef::GetDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}

	if (!ActorClass.IsNull())
	{
		return FPaths::GetBaseFilename(ActorClass.ToString());
	}

	if (!MaterialOverrides.IsEmpty() && !MaterialOverrides[0].IsNull())
	{
		return FPaths::GetBaseFilename(MaterialOverrides[0].ToString());
	}

	if (!Mesh.IsNull())
	{
		return FPaths::GetBaseFilename(Mesh.ToString());
	}

	return TEXT("(빈 타일)");
}

int32 UMapTilePalette::FindTileIndexById(const FGuid& InTileId) const
{
	if (!InTileId.IsValid())
	{
		return INDEX_NONE;
	}

	return Tiles.IndexOfByPredicate(
		[&InTileId](const FMapTileDef& Def) { return Def.TileId == InTileId; });
}
