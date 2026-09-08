/**
 *	@brief	콘텐츠 브라우저와 툴바의 "새 팔레트"에서 UMapTilePalette를 만들기 위한 팩토리입니다.
 */

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "MapTilePaletteFactory.generated.h"

UCLASS()
class MAPTILEEDITOR_API UMapTilePaletteFactory : public UFactory
{
	GENERATED_BODY()

public:
	UMapTilePaletteFactory();

	virtual UObject* FactoryCreateNew(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn) override;

	virtual bool ShouldShowInNewMenu() const override { return true; }
};
