#include <algorithm>
#include <atomic>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <fstream>
#include <iostream>
#include <mmdeviceapi.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <tlhelp32.h>
#include <unordered_set>
#include <vector>
#include <windows.h>

#define ID_TRAY_TOGGLE_CONSOLE 1002

using json = nlohmann::json;

// Struct to hold application configurations
struct ApplicationConfig {
	std::string				applicationName;
	std::unordered_set<int> volumeUpKeyCombination;
	std::unordered_set<int> volumeDownKeyCombination;
	float					volumePercentage; // Current volume percentage
	int						potNumber;		  // For serial input mapping
};

// Global variables
HHOOK						   hKeyboardHook = nullptr;
std::vector<ApplicationConfig> applications;
std::unordered_set<int>		   currentlyPressedKeys;

// Modifier keys set
std::unordered_set<int> modifierKeys = {VK_SHIFT, VK_LSHIFT, VK_RSHIFT, VK_CONTROL, VK_LCONTROL, VK_RCONTROL, VK_MENU, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN};

// Tray icon variables
#define WM_TRAYICON	 (WM_USER + 1)
#define ID_TRAY_EXIT 1001
NOTIFYICONDATA	  nid		= {};
HMENU			  hTrayMenu = nullptr;
HWND			  hWnd		= nullptr;

// Atomic flag to control the serial reading thread
std::atomic<bool> keepReading(true);

// Function to map key names to virtual key codes
int				  GetVirtualKeyCode(const std::string& keyName) {
	  // Modifier keys
	  if(_stricmp(keyName.c_str(), "Ctrl") == 0) return VK_CONTROL;
	  if(_stricmp(keyName.c_str(), "Alt") == 0) return VK_MENU;
	  if(_stricmp(keyName.c_str(), "Shift") == 0) return VK_SHIFT;
	  if(_stricmp(keyName.c_str(), "LWin") == 0) return VK_LWIN;
	  if(_stricmp(keyName.c_str(), "RWin") == 0) return VK_RWIN;

	  // Special keys
	  if(_stricmp(keyName.c_str(), "Up") == 0) return VK_UP;
	  if(_stricmp(keyName.c_str(), "Down") == 0) return VK_DOWN;
	  if(_stricmp(keyName.c_str(), "Left") == 0) return VK_LEFT;
	  if(_stricmp(keyName.c_str(), "Right") == 0) return VK_RIGHT;
	  if(_stricmp(keyName.c_str(), "Tab") == 0) return VK_TAB;
	  if(_stricmp(keyName.c_str(), "Enter") == 0) return VK_RETURN;
	  if(_stricmp(keyName.c_str(), "Esc") == 0 || _stricmp(keyName.c_str(), "Escape") == 0) return VK_ESCAPE;
	  if(_stricmp(keyName.c_str(), "Space") == 0) return VK_SPACE;
	  if(_stricmp(keyName.c_str(), "Backspace") == 0) return VK_BACK;
	  if(_stricmp(keyName.c_str(), "Delete") == 0 || _stricmp(keyName.c_str(), "Del") == 0) return VK_DELETE;
	  if(_stricmp(keyName.c_str(), "Insert") == 0 || _stricmp(keyName.c_str(), "Ins") == 0) return VK_INSERT;
	  if(_stricmp(keyName.c_str(), "Home") == 0) return VK_HOME;
	  if(_stricmp(keyName.c_str(), "End") == 0) return VK_END;
	  if(_stricmp(keyName.c_str(), "PageUp") == 0) return VK_PRIOR;
	  if(_stricmp(keyName.c_str(), "PageDown") == 0) return VK_NEXT;
	  if(_stricmp(keyName.c_str(), "CapsLock") == 0) return VK_CAPITAL;
	  if(_stricmp(keyName.c_str(), "NumLock") == 0) return VK_NUMLOCK;
	  if(_stricmp(keyName.c_str(), "ScrollLock") == 0) return VK_SCROLL;
	  if(_stricmp(keyName.c_str(), "PrintScreen") == 0) return VK_SNAPSHOT;
	  if(_stricmp(keyName.c_str(), "Pause") == 0) return VK_PAUSE;
	  if(_stricmp(keyName.c_str(), "Apps") == 0) return VK_APPS; // Context Menu key

	  // Function keys F1-F24
	  if(keyName.size() > 1 && (keyName[0] == 'F' || keyName[0] == 'f')) {
		  const int fn = std::stoi(keyName.substr(1));
		  if(fn >= 1 && fn <= 24) return VK_F1 + fn - 1;
	  }

	  // Alphanumeric and symbol keys
	  if(keyName.length() == 1) {
		  HKL		  hklLayout = GetKeyboardLayout(0);
		  const SHORT vk		= VkKeyScanExA(keyName[0], hklLayout);
		  if(vk != -1) return vk & 0xFF;
	  }

	  // Numpad keys
	  if(_stricmp(keyName.c_str(), "NumPad0") == 0) return VK_NUMPAD0;
	  if(_stricmp(keyName.c_str(), "NumPad1") == 0) return VK_NUMPAD1;
	  if(_stricmp(keyName.c_str(), "NumPad2") == 0) return VK_NUMPAD2;
	  if(_stricmp(keyName.c_str(), "NumPad3") == 0) return VK_NUMPAD3;
	  if(_stricmp(keyName.c_str(), "NumPad4") == 0) return VK_NUMPAD4;
	  if(_stricmp(keyName.c_str(), "NumPad5") == 0) return VK_NUMPAD5;
	  if(_stricmp(keyName.c_str(), "NumPad6") == 0) return VK_NUMPAD6;
	  if(_stricmp(keyName.c_str(), "NumPad7") == 0) return VK_NUMPAD7;
	  if(_stricmp(keyName.c_str(), "NumPad8") == 0) return VK_NUMPAD8;
	  if(_stricmp(keyName.c_str(), "NumPad9") == 0) return VK_NUMPAD9;

	  // Arrow keys
	  if(_stricmp(keyName.c_str(), "Up") == 0) return VK_UP;
	  if(_stricmp(keyName.c_str(), "Down") == 0) return VK_DOWN;
	  if(_stricmp(keyName.c_str(), "Left") == 0) return VK_LEFT;
	  if(_stricmp(keyName.c_str(), "Right") == 0) return VK_RIGHT;

	  // Media keys
	  if(_stricmp(keyName.c_str(), "VolumeUp") == 0) return VK_VOLUME_UP;
	  if(_stricmp(keyName.c_str(), "VolumeDown") == 0) return VK_VOLUME_DOWN;
	  if(_stricmp(keyName.c_str(), "VolumeMute") == 0) return VK_VOLUME_MUTE;

	  // If key not found, return 0
	  return 0;
}

// Function to parse key combination strings
bool ParseKeyCombination(const std::string& keyCombinationStr, std::unordered_set<int>& keyCombination) {
	std::istringstream iss(keyCombinationStr);
	std::string		   key;
	while(std::getline(iss, key, '+')) {
		int vkCode = GetVirtualKeyCode(key);
		if(vkCode == 0) {
			std::cerr << "Invalid key in combination: " << key << std::endl;
			return false;
		}
		keyCombination.insert(vkCode);
	}
	return true;
}

// Function to read and parse the configuration file
bool ReadConfig(const std::string& configFile) {
	std::ifstream inFile(configFile);
	if(!inFile.is_open()) {
		std::cerr << "Unable to open config file." << std::endl;
		return false;
	}

	json j;
	inFile >> j;

	try {
		const auto& apps = j["applications"];
		for(const auto& app : apps) {
			ApplicationConfig appConfig;
			appConfig.applicationName = app["application_name"].get<std::string>();
			appConfig.potNumber		  = app["pot_number"].get<int>();

			if(app.contains("volume_up_key") && app.contains("volume_down_key")) {
				std::string volUpKeyStr	  = app["volume_up_key"].get<std::string>();
				std::string volDownKeyStr = app["volume_down_key"].get<std::string>();

				if(!ParseKeyCombination(volUpKeyStr, appConfig.volumeUpKeyCombination)) return false;
				if(!ParseKeyCombination(volDownKeyStr, appConfig.volumeDownKeyCombination)) return false;
			} else {
				std::cerr << "Volume up/down keys not set for application: " << appConfig.applicationName << std::endl;

				applications.push_back(appConfig);
				continue;
			}

			std::string volUpKeyStr	  = app["volume_up_key"].get<std::string>();
			std::string volDownKeyStr = app["volume_down_key"].get<std::string>();

			std::cout << volUpKeyStr << std::endl;

			if(!ParseKeyCombination(volUpKeyStr, appConfig.volumeUpKeyCombination)) return false;
			if(!ParseKeyCombination(volDownKeyStr, appConfig.volumeDownKeyCombination)) return false;

			applications.push_back(appConfig);
		}
	} catch(json::exception& e) {
		std::cerr << "Error parsing config file: " << e.what() << std::endl;
		return false;
	}

	return true;
}

// Function to get process IDs by process name
std::vector<DWORD> GetProcessIdsByName(const std::string& processName) {
	std::vector<DWORD> processIds;

	HANDLE			   hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hSnapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if(Process32First(hSnapshot, &pe32)) {
			do {
				if(_stricmp(processName.c_str(), pe32.szExeFile) == 0) { processIds.push_back(pe32.th32ProcessID); }
			} while(Process32Next(hSnapshot, &pe32));
		}
		CloseHandle(hSnapshot);
	}
	return processIds;
}

// Function to adjust the application's volume by delta
void AdjustApplicationVolume(const std::string& applicationName, const float delta) {
	// Initialize COM in this thread
	CoInitialize(nullptr);

	// Get default audio endpoint
	IMMDeviceEnumerator* pDeviceEnumerator = nullptr;
	HRESULT				 hr =
		CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pDeviceEnumerator));
	if(FAILED(hr)) {
		std::cerr << "Failed to create MMDeviceEnumerator." << std::endl;
		CoUninitialize();
		return;
	}

	IMMDevice* pDevice = nullptr;
	hr				   = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	pDeviceEnumerator->Release();
	if(FAILED(hr)) {
		std::cerr << "Failed to get default audio endpoint." << std::endl;
		CoUninitialize();
		return;
	}

	IAudioSessionManager2* pAudioSessionManager = nullptr;
	hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&pAudioSessionManager));
	pDevice->Release();
	if(FAILED(hr)) {
		std::cerr << "Failed to get IAudioSessionManager2." << std::endl;
		CoUninitialize();
		return;
	}

	IAudioSessionEnumerator* pSessionEnumerator = nullptr;
	hr											= pAudioSessionManager->GetSessionEnumerator(&pSessionEnumerator);
	pAudioSessionManager->Release();
	if(FAILED(hr)) {
		std::cerr << "Failed to get session enumerator." << std::endl;
		CoUninitialize();
		return;
	}

	int sessionCount = 0;
	hr				 = pSessionEnumerator->GetCount(&sessionCount);
	if(FAILED(hr)) {
		std::cerr << "Failed to get session count." << std::endl;
		pSessionEnumerator->Release();
		CoUninitialize();
		return;
	}

	std::vector<DWORD> processIds = GetProcessIdsByName(applicationName);
	if(processIds.empty()) { std::cerr << "No running processes found with name: " << applicationName << std::endl; }

	bool volumeAdjusted = false;

	for(int i = 0; i < sessionCount; ++i) {
		IAudioSessionControl* pSessionControl = nullptr;
		hr									  = pSessionEnumerator->GetSession(i, &pSessionControl);
		if(SUCCEEDED(hr)) {
			IAudioSessionControl2* pSessionControl2 = nullptr;
			hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&pSessionControl2));
			if(SUCCEEDED(hr)) {
				DWORD sessionProcessId = 0;
				hr					   = pSessionControl2->GetProcessId(&sessionProcessId);
				if(SUCCEEDED(hr)) {
					if(std::find(processIds.begin(), processIds.end(), sessionProcessId) != processIds.end()) {
						ISimpleAudioVolume* pSimpleAudioVolume = nullptr;
						hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&pSimpleAudioVolume));
						if(SUCCEEDED(hr)) {
							float currentVolume = 0.0f;
							hr					= pSimpleAudioVolume->GetMasterVolume(&currentVolume);
							if(SUCCEEDED(hr)) {
								float newVolume = currentVolume + delta;
								if(newVolume != currentVolume) {

									if(newVolume > 1.0f) newVolume = 1.0f;
									if(newVolume < 0.0f) newVolume = 0.0f;

									hr = pSimpleAudioVolume->SetMasterVolume(newVolume, nullptr);
									if(SUCCEEDED(hr)) {
										std::cout << "Adjusted volume for " << applicationName << " to " << (newVolume * 100) << "%" << std::endl;
										volumeAdjusted = true;
									} else {
										std::cerr << "Failed to set volume." << std::endl;
									}
								}
							}
							pSimpleAudioVolume->Release();
						}
					}
				}
				pSessionControl2->Release();
			}
			pSessionControl->Release();
		}
	}

	pSessionEnumerator->Release();
	CoUninitialize();

	if(!volumeAdjusted) { std::cerr << "Volume adjustment failed. Process may not have an audio session." << std::endl; }
}

// Function to set the application's volume to a specific value
void SetApplicationVolume(const std::string& applicationName, float volume) {
	// Initialize COM in this thread
	CoInitialize(nullptr);

	// Get default audio endpoint
	IMMDeviceEnumerator* pDeviceEnumerator = nullptr;
	HRESULT				 hr =
		CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pDeviceEnumerator));
	if(FAILED(hr)) {
		std::cerr << "Failed to create MMDeviceEnumerator." << std::endl;
		CoUninitialize();
		return;
	}

	IMMDevice* pDevice = nullptr;
	hr				   = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	pDeviceEnumerator->Release();
	if(FAILED(hr)) {
		std::cerr << "Failed to get default audio endpoint." << std::endl;
		CoUninitialize();
		return;
	}

	IAudioSessionManager2* pAudioSessionManager = nullptr;
	hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&pAudioSessionManager));
	pDevice->Release();
	if(FAILED(hr)) {
		std::cerr << "Failed to get IAudioSessionManager2." << std::endl;
		CoUninitialize();
		return;
	}

	IAudioSessionEnumerator* pSessionEnumerator = nullptr;
	hr											= pAudioSessionManager->GetSessionEnumerator(&pSessionEnumerator);
	pAudioSessionManager->Release();
	if(FAILED(hr)) {
		std::cerr << "Failed to get session enumerator." << std::endl;
		CoUninitialize();
		return;
	}

	int sessionCount = 0;
	hr				 = pSessionEnumerator->GetCount(&sessionCount);
	if(FAILED(hr)) {
		std::cerr << "Failed to get session count." << std::endl;
		pSessionEnumerator->Release();
		CoUninitialize();
		return;
	}

	std::vector<DWORD> processIds = GetProcessIdsByName(applicationName);
	if(processIds.empty()) { std::cerr << "No running processes found with name: " << applicationName << std::endl; }

	bool volumeSet = false;

	for(int i = 0; i < sessionCount; ++i) {
		IAudioSessionControl* pSessionControl = nullptr;
		hr									  = pSessionEnumerator->GetSession(i, &pSessionControl);
		if(SUCCEEDED(hr)) {
			IAudioSessionControl2* pSessionControl2 = nullptr;
			hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&pSessionControl2));
			if(SUCCEEDED(hr)) {
				DWORD sessionProcessId = 0;
				hr					   = pSessionControl2->GetProcessId(&sessionProcessId);
				if(SUCCEEDED(hr)) {
					if(std::ranges::find(processIds, sessionProcessId) != processIds.end()) {
						ISimpleAudioVolume* pSimpleAudioVolume = nullptr;
						hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&pSimpleAudioVolume));
						if(SUCCEEDED(hr)) {
							if(volume > 1.0f) volume = 1.0f;
							if(volume < 0.0f) volume = 0.0f;

							float currentVolume = 0.0f;
							pSimpleAudioVolume->GetMasterVolume(&currentVolume);
							if(currentVolume != volume) {
								hr = pSimpleAudioVolume->SetMasterVolume(volume, nullptr);
								if(SUCCEEDED(hr)) {
									std::cout << "Set volume for " << applicationName << " to " << (volume * 100) << "%" << std::endl;
									volumeSet = true;
								} else {
									std::cerr << "Failed to set volume." << std::endl;
								}
							} else {
								volumeSet = true;
							}
							pSimpleAudioVolume->Release();
						}
					}
				}
				pSessionControl2->Release();
			}
			pSessionControl->Release();
		}
	}

	pSessionEnumerator->Release();
	CoUninitialize();

	if(!volumeSet) { std::cerr << "Volume adjustment failed. Process may not have an audio session." << std::endl; }
}

// Function to check if the current pressed keys match a key combination
bool IsKeyCombinationPressed(const std::unordered_set<int>& keyCombination) {
	for(int vkCode : keyCombination) {
		if(modifierKeys.contains(vkCode)) {
			// Check if modifier key is down
			const SHORT keyState = GetAsyncKeyState(vkCode);
			if(!(keyState & 0x8000)) { return false; }
		} else {
			// Check if non-modifier key is in currentlyPressedKeys
			if(!currentlyPressedKeys.contains(vkCode)) { return false; }
		}
	}
	return true;
}

// Low-level keyboard hook callback
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	// Check if nCode is HC_ACTION
	if(nCode == HC_ACTION) {
		const KBDLLHOOKSTRUCT* pKbdLLHookStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
		const DWORD			   vkCode			= pKbdLLHookStruct->vkCode;

		if(wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
			currentlyPressedKeys.insert(vkCode);

			for(const auto& app : applications) {
				// Check if volume keys are set
				if(app.volumeUpKeyCombination.empty() && app.volumeDownKeyCombination.empty()) {
					continue; // Skip volume adjustment if no keys are set
				}

				// Check if the current pressed keys match the volume up key combination
				if(IsKeyCombinationPressed(app.volumeUpKeyCombination)) {
					AdjustApplicationVolume(app.applicationName, 0.1f); // Increase volume by 10%
				}

				// Check if the current pressed keys match the volume down key combination
				if(IsKeyCombinationPressed(app.volumeDownKeyCombination)) {
					AdjustApplicationVolume(app.applicationName, -0.1f); // Decrease volume by 10%
				}
			}
		} else if(wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
			currentlyPressedKeys.erase(vkCode);
		}
	}
	return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

// Function to initialize the tray icon
void InitTrayIcon(const HWND hwnd) {
	nid.cbSize			 = sizeof(NOTIFYICONDATA);
	nid.hWnd			 = hwnd;
	nid.uID				 = 1; // Unique identifier
	nid.uFlags			 = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon			 = LoadIcon(nullptr, IDI_APPLICATION); // You can load a custom icon here
	strcpy_s(nid.szTip, "Audio Volume Controller");

	Shell_NotifyIcon(NIM_ADD, &nid);
}

// Function to clean up the tray icon
void CleanupTrayIcon() {
	Shell_NotifyIcon(NIM_DELETE, &nid);
	if(hTrayMenu) { DestroyMenu(hTrayMenu); }
}

// Function to handle serial reading in a separate thread
void SerialReader(const HANDLE hSerial) {
	uint8_t buffer[5]; // Expecting 5 bytes of data (one for each potentiometer)
	DWORD	bytesRead;

	while(keepReading) {
		// Read exactly 5 bytes of data (5 potentiometers percentages)
		if(ReadFile(hSerial, buffer, sizeof(buffer), &bytesRead, nullptr)) {
			if(bytesRead == sizeof(buffer)) { // We expect exactly 5 bytes
				// Process each potentiometer value (from 1 to 5)
				for(int potNumber = 0; potNumber < 5; potNumber++) {
					const int percentage = buffer[potNumber]; // Percentage (0-100)

					// Find all applications associated with this potentiometer
					for(ApplicationConfig& app : applications) {
						if(app.potNumber == potNumber) {
							// Adjust the volume
							const float volume = percentage / 100.0f;
							// std::cout << "Potentiometer " << potNumber << ": " << percentage << "%" << std::endl;

							if(app.volumePercentage != volume) {
								app.volumePercentage = volume;
								SetApplicationVolume(app.applicationName, volume);
								std::cout << "Set volume for " << app.applicationName << " from " << (app.volumePercentage * 100) << " to " << percentage << "%"
										  << std::endl;
							}
						}
					}
				}
			}
		} else {
			std::cerr << "Error reading from serial port." << std::endl;
			// break;
		}

		// Sleep to avoid excessive CPU usage
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	std::cout << "Serial reader thread exiting." << std::endl;
}

void ToggleConsoleVisibility() {
	if(const HWND consoleWindow = GetConsoleWindow()) {
		// Check if the console is currently visible
		ShowWindow(consoleWindow, IsWindowVisible(consoleWindow) ? SW_HIDE : SW_SHOW);
	}
}

LRESULT CALLBACK WindowProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
	switch(uMsg) {
		case WM_TRAYICON:
			if(lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
				POINT pt;
				GetCursorPos(&pt);
				SetForegroundWindow(hwnd);
				TrackPopupMenu(hTrayMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
			}
			break;
		case WM_COMMAND:
			if(LOWORD(wParam) == ID_TRAY_EXIT) {
				CleanupTrayIcon();
				PostQuitMessage(0);
			} else if(LOWORD(wParam) == ID_TRAY_TOGGLE_CONSOLE) {
				ToggleConsoleVisibility(); // Toggle the console window visibility
			}
			break;
		case WM_DESTROY:
			CleanupTrayIcon();
			PostQuitMessage(0);
			break;
		default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

// Main function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
	// Create a hidden window to receive messages
	constexpr char CLASS_NAME[] = "AudioVolumeControllerWindowClass";

	AllocConsole();
	freopen("CONOUT$", "w", stdout); // Redirect stdout to the console
	freopen("CONOUT$", "w", stderr); // Redirect stderr to the console
	freopen("CONIN$", "r", stdin);	 // Redirect stdin to the console

	WNDCLASS wc		 = {};
	wc.lpfnWndProc	 = WindowProc;
	wc.hInstance	 = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	hWnd	  = CreateWindowEx(0,						  // Optional window styles
							   CLASS_NAME,				  // Window class
							   "Audio Volume Controller", // Window text
							   WS_OVERLAPPEDWINDOW,		  // Window style
							   CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
							   nullptr,	  // Parent window
							   nullptr,	  // Menu
							   hInstance, // Instance handle
							   nullptr	  // Additional application data
		 );

	hTrayMenu = CreatePopupMenu();
	AppendMenu(hTrayMenu, MF_STRING, ID_TRAY_TOGGLE_CONSOLE, "Toggle Console");
	AppendMenu(hTrayMenu, MF_STRING, ID_TRAY_EXIT, "Exit");
	InitTrayIcon(hWnd);

	if(hWnd == nullptr) {
		std::cerr << "Failed to create hidden window. Error: " << GetLastError() << std::endl;
		return -1;
	}

	if(!ReadConfig(R"(C:\dev\audioMixer\audio_conf.json)")) { return -1; }

	hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
	if(!hKeyboardHook) {
		std::cerr << "Failed to install keyboard hook. Error: " << GetLastError() << std::endl;
		return -1;
	}

	std::cout << "Keyboard hook installed successfully." << std::endl;
	std::cout << "Application volume controller started." << std::endl;
	std::cout << "Monitoring shortcuts for applications:" << std::endl;
	for(const auto& app : applications) {
		std::cout << " - " << app.applicationName << std::endl;
	}

	// Open the serial port (replace "COM3" with your port if necessary)
	HANDLE hSerial = CreateFile("COM3", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if(hSerial == INVALID_HANDLE_VALUE) {
		std::cerr << "Error: Unable to open COM port." << std::endl;
		return 1;
	}

	// Set the serial port parameters
	DCB dcbSerialParams		  = {0};
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	if(!GetCommState(hSerial, &dcbSerialParams)) {
		std::cerr << "Error: Unable to get serial port state." << std::endl;
		CloseHandle(hSerial);
		return 1;
	}

	dcbSerialParams.BaudRate = CBR_115200;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity	 = NOPARITY;

	if(!SetCommState(hSerial, &dcbSerialParams)) {
		std::cerr << "Error: Unable to set serial port parameters." << std::endl;
		CloseHandle(hSerial);
		return 1;
	}

	// Set timeouts
	COMMTIMEOUTS timeouts				= {0};
	timeouts.ReadIntervalTimeout		= 50;
	timeouts.ReadTotalTimeoutConstant	= 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	SetCommTimeouts(hSerial, &timeouts);

	// Start the serial reader thread
	std::thread serialThread(SerialReader, hSerial);

	// Message loop
	MSG			msg;
	while(GetMessage(&msg, nullptr, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Signal the serial thread to stop and wait for it to finish
	std::cout << "Exiting..." << std::endl;
	keepReading = false;
	if(serialThread.joinable()) { serialThread.join(); }

	// Unhook the keyboard hook
	UnhookWindowsHookEx(hKeyboardHook);

	// Close the serial port
	CloseHandle(hSerial);
	std::cout << "Serial port closed." << std::endl;

	// Free the console on exit
	FreeConsole();

	return 0;
}