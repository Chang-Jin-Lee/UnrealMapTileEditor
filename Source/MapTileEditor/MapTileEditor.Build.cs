using UnrealBuildTool;

public class MapTileEditor : ModuleRules
{
	public MapTileEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
				"LevelEditor",
				"ToolMenus",
				"EditorStyle",
				"PropertyEditor",
				"ContentBrowser",
				"AssetRegistry",
				"AssetTools",
				"EditorWidgets",
				"ToolWidgets",
			}
		);
	}
}
