/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include <QtGlobal>
#ifdef Q_OS_WIN
#include <QString>
#include <windows.h>

namespace Lemon::Windows {
	class Handle {
	  public:
		explicit Handle(HANDLE value = nullptr) : value(value) {}
		~Handle() { reset(); }
		Handle(const Handle &) = delete;
		Handle &operator=(const Handle &) = delete;
		HANDLE get() const { return value; }
		explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
		void reset(HANDLE replacement = nullptr) {
			if (*this)
				CloseHandle(value);
			value = replacement;
		}

	  private:
		HANDLE value;
	};

	inline const wchar_t *wide(const QString &text) {
		return reinterpret_cast<const wchar_t *>(text.utf16());
	}

	inline QString errorText(DWORD code) {
		wchar_t *buffer = nullptr;
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		                   FORMAT_MESSAGE_IGNORE_INSERTS,
		               nullptr, code, 0, reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
		const auto text = buffer ? QString::fromWCharArray(buffer).trimmed() : QString();
		LocalFree(buffer);
		return QString("%1 (Windows error %2)").arg(text).arg(code);
	}

	inline QString quoteArgument(const QString &argument) {
		QString result = "\"";
		int slashes = 0;
		for (QChar c : argument) {
			if (c == '\\') {
				++slashes;
				continue;
			}
			result += QString(c == '"' ? slashes * 2 + 1 : slashes, '\\');
			result += c;
			slashes = 0;
		}
		return result + QString(slashes * 2, '\\') + '"';
	}
} // namespace Lemon::Windows
#endif
