/**
 *	@brief	맵 타일 편집기 모듈입니다. Tools 메뉴 진입점과 도킹 탭을 등록합니다.
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class SDockTab;
class FSpawnTabArgs;

class FMapTileEditorModule : public IModuleInterface
{
public:
	//~ IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ 끝

	/** 편집기 탭을 띄웁니다. */
	static void OpenMapTileEditorTab();

private:
	void RegisterMenus();

	TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs);
};
