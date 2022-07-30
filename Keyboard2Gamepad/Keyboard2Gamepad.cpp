// Keyboard2Gamepad.cpp : Defines the entry point for the application.
//

#include <ShellScalingApi.h>
#include <CommCtrl.h>

#pragma comment(lib,"Shcore.lib")
#pragma comment(lib,"Comctl32.lib")

#include "framework.h"
#include "Keyboard2Gamepad.h"

#include "yaml/Yaml.cpp"

#pragma comment(lib, "setupapi.lib")

#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")

#include "ViGEm/Client.h"
#pragma comment(lib, "ViGEmClient.lib")

#define MAX_LOADSTRING 100

#define IDC_LISTVIEW 1000
#define IDC_STATUS 1001

#define WM_TRAY_CLICK (WM_USER+1)

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HFONT hFont;                                    // system font

Yaml::Node macroList;
bool controllerAttached;
HANDLE threadHandle;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    AddDigital(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    AddAnalog(HWND, UINT, WPARAM, LPARAM);

BOOL CALLBACK       EnumChildProc(HWND, LPARAM);

HWND                DoCreateListView(HWND, HINSTANCE);
BOOL                InitListView(HWND);
BOOL                InsertListViewItem(HWND, const wchar_t*, int key, const wchar_t*, int = 0);
VOID                ResizeListView(HWND, HWND);

VOID                LoadMacrosFromFile(HWND, const char*);
VOID                SaveMacrosToFile(HWND, const char*);

DWORD WINAPI        ControllerThread(_In_  LPVOID);

INT                 ParseButtonMask(const char*);

VOID                ToTray(HWND);
VOID                DeleteTray(HWND);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Ensures that the common control DLL is loaded.
	InitCommonControls();
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_KEYBOARD2GAMEPAD, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_KEYBOARD2GAMEPAD));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_KEYBOARD2GAMEPAD));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_KEYBOARD2GAMEPAD);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}



//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 500, 300, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	// Get the window dpi
	UINT dpi = GetDpiForWindow(hWnd);

	NONCLIENTMETRICSW ncm = { 0 };
	ncm.cbSize = sizeof(ncm);

	SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, FALSE, dpi);
	hFont = CreateFontIndirectW(&ncm.lfMessageFont);

	ShowWindow(hWnd, nCmdShow);
	EnumChildWindows(hWnd, EnumChildProc, (LPARAM)&hFont);
	UpdateWindow(hWnd);

	return TRUE;
}

BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam)
{
	HFONT hfDefault = *(HFONT*)lParam;
	SendMessageW(hWnd, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(TRUE, 0));
	return TRUE;
}

HWND listView;
HWND groupBox;
HWND statusBar;

VOID DeleteSelectedMacro() {
	int selIndex = ListView_GetNextItem(listView, -1, LVNI_SELECTED);
	if (selIndex >= 0) {
		ListView_DeleteItem(listView, selIndex);
	}
	else {
		MessageBox(NULL, _T("No macro selected!"), _T("Error"), MB_ICONERROR);
	}
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		statusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE, _T("Controller detached"), hWnd, IDC_STATUS);

		listView = DoCreateListView(hWnd, hInst);
		InitListView(listView);
	}
	break;
	case WM_TRAY_CLICK:
	{
		if (lParam == WM_LBUTTONDBLCLK) {
			SetForegroundWindow(hWnd);
			AnimateWindow(hWnd, 200, AW_BLEND | AW_ACTIVATE);
			ShowWindow(hWnd, SW_SHOWNORMAL);
		}
	}
	break;
	case WM_SIZE:
	{
		if (wParam == SIZE_MINIMIZED) {
			ToTray(hWnd);
			AnimateWindow(hWnd, 200, AW_BLEND | AW_HIDE);
			ShowWindow(hWnd, SW_HIDE);
		}
		else {
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);

			SendMessage(statusBar, WM_SIZE, 0, 0);
			ResizeListView(listView, hWnd);
		}
	}
	break;
	case WM_NOTIFY:
		if (((LPNMITEMACTIVATE)lParam)->hdr.code == LVN_KEYDOWN) {
			LPNMLVKEYDOWN key = (LPNMLVKEYDOWN)lParam;
			if (key->wVKey == VK_DELETE) {
				DeleteSelectedMacro();
			}
		}
		break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_CONTROLLER_INSTALL:
			ShellExecute(0, 0, _T("https://github.com/ViGEm/ViGEmBus/releases"), 0, 0, SW_SHOW);
			break;
		case IDM_MACRO_DELETE:
			DeleteSelectedMacro();
			break;
		case IDM_CONTROLLER_ATTACH:
			if (controllerAttached)
			{
				MessageBox(NULL, _T("Controller already attached!"), _T("Error"), MB_ICONERROR);
			}
			else
			{
				controllerAttached = true;
				threadHandle = CreateThread(NULL, 0, ControllerThread, NULL, 0, NULL);
				if (threadHandle == NULL)
				{
					DWORD errCode = GetLastError();
					TCHAR* lpBuffer;
					FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
						errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						(LPTSTR)&lpBuffer, 0, NULL);
					OutputDebugString(lpBuffer);
					MessageBox(NULL, _T("Unable to create thread!"), _T("Error"), MB_ICONERROR);
				}
				else
				{
					SendMessage(statusBar, SB_SETTEXT, 0, (LPARAM)_T("Controller attached"));
				}
			}
			break;
		case IDM_CONTROLLER_DETACH:
			if (!controllerAttached)
			{
				MessageBox(NULL, _T("Controller is currently not attached!"), _T("Error"), MB_ICONERROR);
			}
			else
			{
				controllerAttached = false;
				SendMessage(statusBar, SB_SETTEXT, 0, (LPARAM)_T("Controller detached"));
			}
			break;
		case IDM_FILE_SAVE:
			SaveMacrosToFile(listView, "config.yml");
			break;
		case IDM_ADD_DIGITAL:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ADD_DIGITAL), hWnd, AddDigital);
			break;
		case IDM_ADD_ANALOG:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ADD_ANALOG), hWnd, AddAnalog);
			break;
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		SelectObject(hdc, hFont);
		SetBkMode(hdc, TRANSPARENT);

		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		DeleteTray(hWnd);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

HWND gamepadComboBox;
HWND hotkey;

// Message handler for about box.
INT_PTR CALLBACK AddDigital(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		gamepadComboBox = GetDlgItem(hDlg, IDC_COMBO_DIGITAL);
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("DPAD_UP"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("DPAD_DOWN"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("DPAD_LEFT"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("DPAD_RIGHT"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("START"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("BACK"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("LEFT_THUMB"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("RIGHT_THUMB"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("LEFT_SHOULDER"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("RIGHT_SHOULDER"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("A"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("B"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("X"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("Y"));
		ComboBox_SetCurSel(gamepadComboBox, 0);

		hotkey = GetDlgItem(hDlg, IDC_HOTKEY1);
	}
	return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			if (LOWORD(wParam) == IDOK)
			{
				int selection = ComboBox_GetCurSel(gamepadComboBox);
				if (selection != CB_ERR)
				{
					int hk = LOBYTE(LOWORD(SendMessage(hotkey, HKM_GETHOTKEY, 0, 0)));
					TCHAR buffer[20];
					ComboBox_GetText(gamepadComboBox, buffer, 20);
					InsertListViewItem(listView, L"DIGITAL", hk, buffer);
				}
			}
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

HWND analogSlider;
HWND sliderText;

// Message handler for about box.
INT_PTR CALLBACK AddAnalog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		gamepadComboBox = GetDlgItem(hDlg, IDC_COMBO_ANALOG);
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("LEFT_TRIGGER"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("RIGHT_TRIGGER"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("THUMB_LX"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("THUMB_LY"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("THUMB_RX"));
		ComboBox_AddString(gamepadComboBox, (LPARAM)_T("THUMB_RY"));
		ComboBox_SetCurSel(gamepadComboBox, 0);

		hotkey = GetDlgItem(hDlg, IDC_HOTKEY1);

		analogSlider = GetDlgItem(hDlg, IDC_SLIDER_VALUE);
		SendMessage(analogSlider, TBM_SETRANGE, (WPARAM)1, (LPARAM)MAKELONG(0, 256));

		sliderText = GetDlgItem(hDlg, IDC_SLIDER_LABEL);
	}
	return (INT_PTR)TRUE;
	case WM_HSCROLL:
	{
		if ((HWND)lParam == analogSlider)
		{
			int newPos = SendMessage(analogSlider, TBM_GETPOS, 0, 0);
			int max = SendMessage(analogSlider, TBM_GETRANGEMAX, 0, 0);
			TCHAR buffer[20];
			int percent = (newPos / (float)max) * 100;
			wsprintf(buffer, L"%d (%d %%)", newPos, percent);
			SetWindowText(sliderText, buffer);
		}
	}
	return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (HIWORD(wParam) == CBN_SELCHANGE)
		{
			int index = ComboBox_GetCurSel(gamepadComboBox);
			if (index == 0 || index == 1) {
				SendMessage(analogSlider, TBM_SETRANGE, (WPARAM)1, (LPARAM)MAKELONG(0, 255));
			}
			else {
				SendMessage(analogSlider, TBM_SETRANGE, (WPARAM)1, (LPARAM)MAKELONG(-32768, 32767));
			}
		}

		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			if (LOWORD(wParam) == IDOK)
			{
				int selection = ComboBox_GetCurSel(gamepadComboBox);
				if (selection != CB_ERR)
				{
					int analogValue = SendMessage(analogSlider, TBM_GETPOS, 0, 0);
					int hk = LOBYTE(LOWORD(SendMessage(hotkey, HKM_GETHOTKEY, 0, 0)));
					TCHAR buffer[20];
					ComboBox_GetText(gamepadComboBox, buffer, 20);
					InsertListViewItem(listView, L"ANALOG", hk, buffer, analogValue);
				}
			}
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

HWND DoCreateListView(HWND hwndParent, HINSTANCE hinst) {
	DWORD dwStyle;
	HWND hwndListView;
	HIMAGELIST himlSmall;
	HIMAGELIST himlLarge;
	BOOL bSuccess = TRUE;

	dwStyle = WS_TABSTOP |
		WS_CHILD |
		WS_VISIBLE |
		LVS_AUTOARRANGE |
		LVS_REPORT |
		LVS_SINGLESEL;

	hwndListView = CreateWindowEx(0,
		WC_LISTVIEW,               // class name - defined in commctrl.h
		_T(""),                        // dummy text
		dwStyle,                   // style
		0,                         // x position
		0,                         // y position
		0,                         // width
		0,                         // height
		hwndParent,                // parent
		(HMENU)IDC_LISTVIEW,        // ID
		hinst,                   // instance
		NULL);                     // no extra data

	ResizeListView(hwndListView, hwndParent);

	return hwndListView;
}

BOOL InitListView(HWND hwndListView)
{
	LV_COLUMN   lvColumn;
	int         i;
	TCHAR       szString[4][20] = { _T("Key"), _T("Gamepad"), _T("Type"), _T("Analog Value") };

	// empty the list
	ListView_DeleteAllItems(hwndListView);

	//initialize the columns
	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = 120;
	for (i = 0; i < 4; i++)
	{
		lvColumn.pszText = szString[i];
		ListView_InsertColumn(hwndListView, i, &lvColumn);
	}

	// load macros
	LoadMacrosFromFile(hwndListView, "config.yml");

	return TRUE;
}

VOID LoadMacrosFromFile(HWND hwndListView, const char* inFile) {
	Yaml::Node root;

	try {
		Yaml::Parse(root, inFile);
	}
	catch (const Yaml::Exception e) {
		OutputDebugStringA(e.what());
		return;
	}

	TCHAR gpBuffer[100];
	TCHAR typeBuffer[100];

	int index = 0;

	Yaml::Node& macros = root["macros"];
	for (auto it = macros.Begin(); it != macros.End(); it++) {
		Yaml::Node& current = (*it).second;

		int key = current["key"].As<int>();
		std::string type = current["type"].As<std::string>();
		std::string gamepad = current["gamepad"].As<std::string>();

		mbstowcs_s(NULL, gpBuffer, gamepad.c_str(), 100);
		mbstowcs_s(NULL, typeBuffer, type.c_str(), 100);

		int value = 0;
		if (type.compare("ANALOG") == 0)
		{
			value = current["value"].As<int>();
		}
		InsertListViewItem(hwndListView, typeBuffer, key, gpBuffer, value);

		index++;
	}
	macroList = root["macros"];
}

VOID SaveMacrosToFile(HWND hwndListView, const char* outFile) {
	Yaml::Node root;
	Yaml::Node& macros = root["macros"];

	LVITEM item;

	int count = ListView_GetItemCount(hwndListView);

	TCHAR wBuffer[100];
	memset(wBuffer, 100, '\0');
	char buffer[100];
	memset(buffer, 100, '\0');
	size_t pReturnValue = 0;

	item.mask = LVIF_TEXT;
	item.cchTextMax = 100;
	item.pszText = wBuffer;

	for (int i = 0; i < count; i++)
	{
		Yaml::Node& current = macros.PushBack();

		item.iItem = i;

		// key
		item.iSubItem = 0;
		ListView_GetItem(hwndListView, &item);
		wcstombs_s(&pReturnValue, buffer, sizeof(buffer), wBuffer, _TRUNCATE);
		current["key"] = buffer;

		// gamepad
		item.iSubItem = 1;
		ListView_GetItem(hwndListView, &item);
		wcstombs_s(&pReturnValue, buffer, sizeof(buffer), wBuffer, _TRUNCATE);
		current["gamepad"] = buffer;

		// type
		item.iSubItem = 2;
		ListView_GetItem(hwndListView, &item);
		wcstombs_s(&pReturnValue, buffer, sizeof(buffer), wBuffer, _TRUNCATE);
		current["type"] = buffer;

		if (strcmp(buffer, "ANALOG") == 0)
		{
			// value
			item.iSubItem = 3;
			ListView_GetItem(hwndListView, &item);
			wcstombs_s(&pReturnValue, buffer, sizeof(buffer), wBuffer, _TRUNCATE);
			current["value"] = buffer;
		}
	}

	Yaml::Serialize(root, outFile);
	macroList = root["macros"];
}

BOOL InsertListViewItem(HWND hwndListView, const wchar_t* type, int key, const wchar_t* gamepad, int value)
{
	LV_ITEM lvItem;

	lvItem.mask = LVIF_TEXT;
	lvItem.cchTextMax = 100;
	lvItem.iItem = ListView_GetItemCount(hwndListView); // insert in last position

	TCHAR buffer[100];

	// Key column
	lvItem.iSubItem = 0;
	wsprintf(buffer, L"%d", key);
	lvItem.pszText = buffer;
	ListView_InsertItem(hwndListView, &lvItem);

	// Gamepad column
	lvItem.iSubItem = 1;
	lvItem.pszText = const_cast<LPWSTR>(gamepad);
	ListView_SetItem(hwndListView, &lvItem);

	// Type column
	lvItem.iSubItem = 2;
	lvItem.pszText = const_cast<LPWSTR>(type);
	ListView_SetItem(hwndListView, &lvItem);

	// Value column
	if (wcscmp(type, _T("ANALOG")) == 0) {
		lvItem.iSubItem = 3;
		wsprintf(buffer, L"%d", value);
		lvItem.pszText = buffer;
		ListView_SetItem(hwndListView, &lvItem);
	}

	return TRUE;
}

VOID ResizeListView(HWND hwndListView, HWND hwndParent)
{
	RECT statusRc;
	GetWindowRect(statusBar, &statusRc);

	RECT windowRc;
	GetClientRect(hwndParent, &windowRc);

	RECT rc;
	rc.left = 0;
	rc.top = 0;
	rc.bottom = windowRc.bottom - (statusRc.bottom - statusRc.top);
	rc.right = windowRc.right;

	MoveWindow(hwndListView,
		rc.left,
		rc.top,
		rc.right,
		rc.bottom,
		TRUE);
}

DWORD WINAPI ControllerThread(_In_  LPVOID lpParameter) {
	const auto client = vigem_alloc();

	if (client == nullptr)
	{
		MessageBox(NULL, _T("Not enough memory to allocate controller!"), _T("Error"), MB_ICONERROR);
		return -1;
	}

	const auto retval = vigem_connect(client);

	if (!VIGEM_SUCCESS(retval))
	{
		MessageBox(NULL, _T("ViGEm Bus connection failed!"), _T("Error"), MB_ICONERROR);
		return -1;
	}

	// Allocate handle to identify new pad
	const auto pad = vigem_target_x360_alloc();

	// Add client to the bus, this equals a plug-in event
	const auto pir = vigem_target_add(client, pad);

	if (!VIGEM_SUCCESS(pir))
	{
		MessageBox(NULL, _T("Target plugin failed!"), _T("Error"), MB_ICONERROR);
		return -1;
	}

	XINPUT_STATE state;

	while (controllerAttached) {
		state.Gamepad.wButtons = 0;
		state.Gamepad.bLeftTrigger = 0;
		state.Gamepad.bRightTrigger = 0;
		state.Gamepad.sThumbLX = 0;
		state.Gamepad.sThumbRX = 0;
		state.Gamepad.sThumbRY = 0;
		state.Gamepad.sThumbLY = 0;

		for (auto it = macroList.Begin(); it != macroList.End(); it++) {
			Yaml::Node& current = (*it).second;

			int key = current["key"].As<int>();

			if (GetAsyncKeyState(key)) {
				std::string button = current["gamepad"].As<std::string>();
				std::string type = current["type"].As<std::string>();

				if (type.compare("DIGITAL") == 0) {
					int mask = ParseButtonMask(button.c_str());
					state.Gamepad.wButtons |= mask;
				}
				else if (type.compare("ANALOG") == 0) {
					int value = current["value"].As<int>();

					if (button.compare("LEFT_TRIGGER") == 0) {
						state.Gamepad.bLeftTrigger = value;
					}
					else if (button.compare("RIGHT_TRIGGER") == 0) {
						state.Gamepad.bRightTrigger = value;
					}
					else if (button.compare("THUMB_LX") == 0) {
						state.Gamepad.sThumbLX = value;
					}
					else if (button.compare("THUMB_LY") == 0) {
						state.Gamepad.sThumbLY = value;
					}
					else if (button.compare("THUMB_RX") == 0) {
						state.Gamepad.sThumbRX = value;
					}
					else if (button.compare("THUMB_RY") == 0) {
						state.Gamepad.sThumbRY = value;
					}
				}
			}
		}

		vigem_target_x360_update(client, pad, *reinterpret_cast<XUSB_REPORT*>(&state.Gamepad));

		Sleep(1);
	}

	vigem_target_remove(client, pad);
	vigem_target_free(pad);

	vigem_disconnect(client);
	vigem_free(client);

	return 0;
}

INT ParseButtonMask(const char* button) {
	if (strcmp(button, "DPAD_UP") == 0)
		return XINPUT_GAMEPAD_DPAD_UP;
	else if (strcmp(button, "DPAD_DOWN") == 0)
		return XINPUT_GAMEPAD_DPAD_DOWN;
	else if (strcmp(button, "DPAD_LEFT") == 0)
		return XINPUT_GAMEPAD_DPAD_LEFT;
	else if (strcmp(button, "DPAD_RIGHT") == 0)
		return XINPUT_GAMEPAD_DPAD_RIGHT;
	else if (strcmp(button, "START") == 0)
		return XINPUT_GAMEPAD_START;
	else if (strcmp(button, "BACK") == 0)
		return XINPUT_GAMEPAD_BACK;
	else if (strcmp(button, "LEFT_THUMB") == 0)
		return XINPUT_GAMEPAD_LEFT_THUMB;
	else if (strcmp(button, "RIGHT_THUMB") == 0)
		return XINPUT_GAMEPAD_RIGHT_THUMB;
	else if (strcmp(button, "LEFT_SHOULDER") == 0)
		return XINPUT_GAMEPAD_LEFT_SHOULDER;
	else if (strcmp(button, "RIGHT_SHOULDER") == 0)
		return XINPUT_GAMEPAD_RIGHT_SHOULDER;
	else if (strcmp(button, "A") == 0)
		return XINPUT_GAMEPAD_A;
	else if (strcmp(button, "B") == 0)
		return XINPUT_GAMEPAD_B;
	else if (strcmp(button, "X") == 0)
		return XINPUT_GAMEPAD_X;
	else if (strcmp(button, "Y") == 0)
		return XINPUT_GAMEPAD_Y;
}

// adapted from https://www.programmerall.com/article/8598814937/
VOID ToTray(HWND hWnd)
{
	NOTIFYICONDATA nid;
	nid.cbSize = (DWORD)sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = IDR_MAINFRAME;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAY_CLICK; // Customized message handling tray icon event
	nid.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_SMALL));
	wcscpy_s(nid.szTip, szTitle); // The text displayed when the mouse is placed on the tray icon
	Shell_NotifyIcon(NIM_ADD, &nid); // Add an icon in the tray area
}

VOID DeleteTray(HWND hWnd)
{
	NOTIFYICONDATA nid;
	nid.cbSize = (DWORD)sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = IDR_MAINFRAME;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAY_CLICK; // Customized message name Handling tray icon event
	nid.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_SMALL));
	wcscpy_s(nid.szTip, szTitle); // The text displayed when the mouse is placed on the tray icon
	Shell_NotifyIcon(NIM_DELETE, &nid); // Delete icon in tray
}