/**
 *	@brief	맵 타일 편집기가 사용하는 타일 팔레트 애셋입니다.
 *	@note	팔레트는 레벨이 아니라 애셋에 저장되므로 여러 레벨에서 재사용됩니다. (KAN-575)
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MapTilePalette.generated.h"

class UStaticMesh;
class UMaterialInterface;

/**
 * 타일이 놓이는 레이어입니다.
 * 같은 칸에 레이어마다 하나씩 놓을 수 있어, 바닥 위에 기둥을 얹는 식의 배치가 됩니다.
 */
UENUM(BlueprintType)
enum class EMapTileLayer : uint8
{
	/** 바닥 타일입니다. */
	Floor UMETA(DisplayName = "바닥"),

	/** 기둥·장애물·스크린도어처럼 바닥 위에 놓이는 프랍입니다. */
	Prop UMETA(DisplayName = "프랍"),

	/** 적·지하철처럼 게임플레이 요소가 되는 액터입니다. */
	Actor UMETA(DisplayName = "액터"),

	Count UMETA(Hidden)
};

/** 레이어 개수입니다. */
static constexpr int32 MapTileLayerCount = static_cast<int32>(EMapTileLayer::Count);

/** 레이어의 한글 표시 이름을 돌려줍니다. */
MAPTILEEDITOR_API FText GetMapTileLayerText(EMapTileLayer Layer);

/** 팔레트에 등록된 타일 하나의 정의입니다. */
USTRUCT(BlueprintType)
struct FMapTileDef
{
	GENERATED_BODY()

	/** 팔레트 목록에 표시할 이름입니다. 비어 있으면 메시/클래스 이름을 사용합니다. */
	UPROPERTY(EditAnywhere, Category = "Tile")
	FString DisplayName;

	/** 이 타일이 놓이는 레이어입니다. 레이어가 다르면 같은 칸에 함께 놓입니다. */
	UPROPERTY(EditAnywhere, Category = "Tile")
	EMapTileLayer Layer = EMapTileLayer::Floor;

	/**
	 * 이 타일이 차지하는 칸 수입니다. (X = 열, Y = 행)
	 * 지하철처럼 여러 칸을 덮는 것은 여기에 실제 크기를 넣습니다.
	 * 액터는 점유 영역의 중심에 하나만 놓입니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Tile", meta = (ClampMin = "1"))
	FIntPoint Footprint = FIntPoint(1, 1);

	/**
	 * 배치할 스태틱 메시입니다.
	 * ActorClass가 지정되어 있으면 무시되고 해당 액터가 배치됩니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Tile")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** 메시에 덮어씌울 머티리얼입니다. 슬롯 순서대로 적용됩니다. */
	UPROPERTY(EditAnywhere, Category = "Tile")
	TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

	/**
	 * 배치 시 적용할 충돌 프로파일입니다. 비어 있으면 액터 기본값을 씁니다.
	 * `선택 액터 외형 등록`이 원본 액터의 프로파일을 여기에 담습니다.
	 * 이 값을 비워두면 원본이 BlockObjects여도 새 타일은 BlockAll이 되어 충돌 거동이 바뀝니다.
	 * 액터 클래스(블루프린트) 경로에는 적용하지 않습니다. 그쪽은 블루프린트가 정한 값을 씁니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Tile")
	FName CollisionProfileName = NAME_None;

	/**
	 * 배치할 액터 클래스입니다. 스크린도어·기둥·적·지하철 같은 블루프린트를 그리드에 놓을 때 씁니다.
	 * 지정되면 Mesh 대신 이 클래스를 스폰합니다. (KAN-575의 "블루프린트 요소도 배치" 요구)
	 */
	UPROPERTY(EditAnywhere, Category = "Tile")
	TSoftClassPtr<AActor> ActorClass;

	/** 배치 시 적용할 스케일입니다. */
	UPROPERTY(EditAnywhere, Category = "Transform")
	FVector Scale = FVector::OneVector;

	/** 셀 중심으로부터의 오프셋입니다. 바닥이 아닌 프랍을 얹을 때 Z를 올리는 용도입니다. */
	UPROPERTY(EditAnywhere, Category = "Transform")
	FVector PlacementOffset = FVector::ZeroVector;

	/** 이 타일에만 추가로 더할 Yaw입니다. 브러시 Yaw와 합산됩니다. */
	UPROPERTY(EditAnywhere, Category = "Transform")
	float YawOffset = 0.0f;

	/** 캔버스 셀에 그릴 색입니다. 액터에서 등록할 때 머티리얼 기본색으로 자동 채워집니다. */
	UPROPERTY(EditAnywhere, Category = "Preview")
	FLinearColor PreviewColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

	/**
	 * 팔레트를 재정렬해도 레벨의 배치와 연결이 끊기지 않도록 하는 안정적 식별자입니다.
	 * 생성자에서 매번 새 GUID를 만들기 때문에 UE의 멤버 초기화 결정성 검사 대상에서 제외합니다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Tile", meta = (IgnoreForMemberInitializationTest))
	FGuid TileId;

	FMapTileDef()
		: TileId(FGuid::NewGuid())
	{
	}

	/** 목록에 표시할 최종 이름을 돌려줍니다. */
	FString GetDisplayName() const;

	/** 1칸을 넘게 차지하는지 여부입니다. */
	bool IsMultiCell() const { return Footprint.X > 1 || Footprint.Y > 1; }

	/** 음수·0 같은 잘못된 Footprint를 보정한 값을 돌려줍니다. */
	FIntPoint GetClampedFootprint() const
	{
		return FIntPoint(FMath::Max(1, Footprint.X), FMath::Max(1, Footprint.Y));
	}

	/** 메시도 액터 클래스도 없는 빈 정의인지 판단합니다. */
	bool IsValidDef() const
	{
		return !Mesh.IsNull() || !ActorClass.IsNull();
	}
};

/**
 * 맵 타일 편집기의 팔레트 애셋입니다.
 * 셀 크기와 그리드 원점을 함께 들고 있어, 같은 팔레트를 쓰는 레벨끼리 격자가 어긋나지 않습니다.
 */
UCLASS(BlueprintType)
class MAPTILEEDITOR_API UMapTilePalette : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 셀 한 변의 월드 크기(cm)입니다. 레퍼런스(LostSignal) 기준값은 800입니다. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = "1.0"))
	float CellSize = 800.0f;

	/** 그리드 (0,0) 셀의 중심이 놓이는 월드 위치입니다. */
	UPROPERTY(EditAnywhere, Category = "Grid")
	FVector GridOrigin = FVector::ZeroVector;

	/**
	 * 격자 전체의 Yaw 회전(도)입니다.
	 * 맵이 월드 축과 어긋나게 지어졌을 때 격자를 그 방향에 맞춥니다.
	 * 예: DungeonTest는 45도 돌아간 격자 위에 지어져 있어 -45를 씁니다.
	 * 배치되는 액터도 이 각도를 기본 회전으로 물려받습니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid")
	float GridYaw = 0.0f;

	/**
	 * 레벨 스캔 시 바닥 타일로 인정할 Z 허용 범위(cm)입니다.
	 * GridOrigin.Z 로부터 이 값 안에 있는 액터만 그리드로 읽어들입니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = "0.0"))
	float ScanHeightTolerance = 400.0f;

	/**
	 * 레벨 스캔 시 바닥 타일로 인정할 메시 이름 조각입니다.
	 * 하나라도 넣으면 스태틱 메시 이름에 그 조각이 포함된 액터만 스캔합니다.
	 * 비워두면 Z 범위·격자 오차 조건만 보므로, 바닥과 무관한 장식물(유도블록, 작은 프랍)까지
	 * 바닥 타일로 잡혀 덮어 칠할 때 함께 지워집니다. 손배치 맵에서는 반드시 채우는 것이 좋습니다.
	 * 예: DungeonTest는 "B_Tile" 하나만 넣으면 됩니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid")
	TArray<FString> ScanMeshNameFilters;

	/**
	 * 레벨 스캔 시 격자에 붙은 것으로 인정할 허용 오차(cm)입니다.
	 * 오차는 **격자 축 기준**으로 재므로, 이 값이 셀 크기의 절반이면
	 * "셀 안에 있는 것은 모두 인정"이라는 뜻이 됩니다.
	 * 손으로 배치한 맵은 셀 크기의 절반을 넣는 것이 안전합니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = "0.0"))
	float ScanSnapTolerance = 1.0f;

	/**
	 * 빈 칸에 새로 배치할 때 액터를 넣을 아웃라이너 폴더입니다. 비어 있으면 루트에 놓입니다.
	 * 이미 타일이 있던 칸을 덮어 칠하면 그 칸에 있던 액터의 폴더를 물려받으므로
	 * 이 값보다 기존 폴더가 우선합니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Placement")
	FName DefaultFolderPath = NAME_None;

	/** 등록된 타일 목록입니다. */
	UPROPERTY(EditAnywhere, Category = "Tiles")
	TArray<FMapTileDef> Tiles;

	/** TileId로 타일 인덱스를 찾습니다. 없으면 INDEX_NONE을 돌려줍니다. */
	int32 FindTileIndexById(const FGuid& InTileId) const;
};
