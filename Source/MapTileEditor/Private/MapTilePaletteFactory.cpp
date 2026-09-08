#include "MapTilePaletteFactory.h"

#include "MapTilePalette.h"

UMapTilePaletteFactory::UMapTilePaletteFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMapTilePalette::StaticClass();
}

UObject* UMapTilePaletteFactory::FactoryCreateNew(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMapTilePalette>(InParent, InClass, InName, Flags);
}
