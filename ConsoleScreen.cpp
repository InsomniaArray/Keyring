#include "ConsoleScreen.h"
#include "Constants.h"

ConsoleScreen::ConsoleScreen() {
	SECURITY_ATTRIBUTES attrib;
	attrib.nLength = sizeof(SECURITY_ATTRIBUTES);
	attrib.bInheritHandle = false;
	attrib.lpSecurityDescriptor = NULL;

	if ((m_hConIn = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, &attrib, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE) {
		throw std::runtime_error("Could not acquire the console input handle.");
	}

	if ((m_hOldScreenBuf = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, &attrib, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE) {
		throw std::runtime_error("Could not acquire the active screen buffer.");
	}

	if ((m_hConOut = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, &attrib, CONSOLE_TEXTMODE_BUFFER, NULL)) == INVALID_HANDLE_VALUE) {
		throw std::runtime_error("Could not create the console screen buffer.");
	}

	if ((m_hConWindow = GetConsoleWindow()) == NULL) {
		throw std::runtime_error("Could not acquire the console screen window.");
	}

	GetConsoleMode(m_hConIn, &m_oldInMode);
	SetConsoleMode(m_hConIn, ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT);
	SetConsoleMode(m_hConOut, 0);

	COORD size;
	size.X = ScreenWidth;
	size.Y = ScreenHeight;

	SMALL_RECT window;
	window.Bottom = ScreenHeight;
	window.Right = ScreenWidth;
	window.Top = 0;
	window.Left = 0;

	SetConsoleWindowInfo(m_hConOut, TRUE, &window);
	SetConsoleScreenBufferSize(m_hConOut, size);
	SetConsoleActiveScreenBuffer(m_hConOut);

	/* here we abuse the fact that the console window
	won't allow to be larger than it's underlying
	buffer allows, so it just sets it to the maximum
	possible size */
	MoveWindow(m_hConWindow, 0, 0, 1000, 1000, TRUE);

	ClearScreenHighlight();
	HideCursor();
}

ConsoleScreen::~ConsoleScreen() {
	SetConsoleMode(m_hConIn, m_oldInMode);
	SetConsoleActiveScreenBuffer(m_hOldScreenBuf);
	CloseHandle(m_hOldScreenBuf);
	CloseHandle(m_hConIn);
	CloseHandle(m_hConOut);
}

int ConsoleScreen::WriteRow(int row, const std::string& str) {
	int rows = str.length() / ScreenWidth + 1;
	
	for (int i = 0; i < rows; i++)
		ClearRow(row + i);

	DWORD written;
	COORD pos; pos.X = 0; pos.Y = row;
	FillConsoleOutputAttribute(m_hConOut, FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, rows * ScreenWidth, pos, &written);
	WriteConsoleOutputCharacter(m_hConOut, str.c_str(), str.length(), pos, &written);
	return rows;
}

void ConsoleScreen::SetHighlightRow(int row, bool b) {
	WORD flag = b ? (BACKGROUND_BLUE | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY) : (FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	DWORD written;
	COORD pos; pos.X = 0; pos.Y = row;
	FillConsoleOutputAttribute(m_hConOut, flag, ScreenWidth, pos, &written);
}

void ConsoleScreen::ClearRow(int row) {
	DWORD written;
	COORD pos; pos.X = 0; pos.Y = row;
	FillConsoleOutputCharacter(m_hConOut, ' ', ScreenWidth, pos, &written);
	SetHighlightRow(row, false);
}

void ConsoleScreen::ClearScreenHighlight() {
	DWORD written;
	COORD pos; pos.X = 0; pos.Y = 0;
	FillConsoleOutputAttribute(m_hConOut, FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, ScreenWidth * ScreenHeight, pos, &written);
}

void ConsoleScreen::HideCursor() {
	CONSOLE_CURSOR_INFO info;
	info.bVisible = false;
	info.dwSize = 1;

	SetConsoleCursorInfo(m_hConOut, &info);
}

bool ValidChar(unsigned short c) {
	return (c > 31 && c < 127);
}

void ConsoleScreen::ProcessInput(KeyPress& keyPress) {
	while (true) {
		INPUT_RECORD record;
		DWORD numRead;
		if (!ReadConsoleInput(m_hConIn, &record, 1, &numRead)) {
			throw std::runtime_error("Failed to read from console input.");
		}

		if (record.EventType == KEY_EVENT) {
			if (record.Event.KeyEvent.bKeyDown) {
				unsigned short c = record.Event.KeyEvent.uChar.AsciiChar;
				if (!ValidChar(c)) {
					keyPress.isVirtual = true;
					keyPress.key.virtualCode = record.Event.KeyEvent.wVirtualKeyCode;
				} else {
					keyPress.isVirtual = false;
					keyPress.key.charCode = c;
				}

				return;
			}
		}
	}

}