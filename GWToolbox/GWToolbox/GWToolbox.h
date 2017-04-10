#pragma once

#include <Windows.h>

#include <vector>

#include "ToolboxModule.h"
#include "ToolboxUIElement.h"

DWORD __stdcall SafeThreadEntry(LPVOID mod);
DWORD __stdcall ThreadEntry(LPVOID dllmodule);

LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

class GWToolbox {
	GWToolbox() {};
	~GWToolbox() {};
public:
	static GWToolbox& Instance() {
		static GWToolbox instance;
		return instance;
	}

	static void Draw(IDirect3DDevice9* device);

	void Initialize();
	void Terminate();

	void LoadSettings();
	void SaveSettings();

	void StartSelfDestruct() { must_self_destruct = true; }
	bool must_self_destruct = false;	// is true when toolbox should quit

	void RegisterModule(ToolboxModule* m) { modules.push_back(m); }
	void RegisterUIElement(ToolboxUIElement* e) { uielements.push_back(e); }

	const std::vector<ToolboxModule*>& GetModules() const { return modules; }
	const std::vector<ToolboxUIElement*>& GetUIElements() const { return uielements; }

	void FlashGwTray() const;

private:
	std::vector<ToolboxModule*> modules;
	std::vector<ToolboxUIElement*> uielements;

	CSimpleIni* inifile = nullptr;
};
