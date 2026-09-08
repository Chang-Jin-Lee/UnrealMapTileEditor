#include "MapTileEditor.h"

#include "SMapTileEditorWidget.h"

#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FMapTileEditorModule"

namespace
{
	/** 탭 식별자입니다. */
	const FName MapTileEditorTabName("MapTileEditor");
}

void FMapTileEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(
			MapTileEditorTabName, FOnSpawnTab::CreateRaw(this, &FMapTileEditorModule::OnSpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "맵 타일 편집기"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMapTileEditorModule::RegisterMenus));
}

void FMapTileEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MapTileEditorTabName);
}

TSharedRef<SDockTab> FMapTileEditorModule::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("TabTitle", "맵 타일 편집기"))
		[
			SNew(SMapTileEditorWidget)
		];
}

void FMapTileEditorModule::OpenMapTileEditorTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(MapTileEditorTabName);
}

void FMapTileEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	if (!ToolsMenu)
	{
		return;
	}

	// 레퍼런스 영상과 같은 위치입니다. Tools 메뉴 하단에 프로젝트 전용 섹션을 둡니다.
	FToolMenuSection& Section =
		ToolsMenu->FindOrAddSection("Metropia", LOCTEXT("MetropiaSection", "Metropia"));

	Section.AddMenuEntry(
		"OpenMapTileEditor",
		LOCTEXT("OpenMapTileEditor", "맵 타일 편집기"),
		LOCTEXT("OpenMapTileEditorTip", "규격 그리드 위에 타일과 블루프린트를 팔레트처럼 배치합니다."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FMapTileEditorModule::OpenMapTileEditorTab)));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMapTileEditorModule, MapTileEditor)
