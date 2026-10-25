#include "platform/native_log.h"

#include <macros.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include "platform/native_win32.h"
#endif

global_variable FILE *s_logStream = NULL;
global_variable char s_logPath[512]; // TODO(aalhendi): yeah this is an issue waiting to happen. w/e

internal void Platform_LogWrite(FILE *consoleStream, const char *text)
{
	FILE *stream = (consoleStream != NULL) ? consoleStream : stdout;

#ifdef _WIN32
	OutputDebugStringA(text);
#endif

	fputs(text, stream);

	if (s_logStream != NULL)
	{
		fputs(text, s_logStream);
		fflush(s_logStream);
	}
}

internal void Platform_LogV(FILE *consoleStream, const char *fmt, va_list args)
{
	char text[4096];
	int written = vsnprintf(text, sizeof(text), fmt, args);

	if (written < 0)
	{
		return;
	}

	text[sizeof(text) - 1] = '\0';
	Platform_LogWrite(consoleStream, text);
}

int Platform_LogSetPath(const char *path)
{
	int written;

	if (s_logStream != NULL)
	{
		return 0;
	}

	if ((path == NULL) || (path[0] == '\0'))
	{
		s_logPath[0] = '\0';
		return 1;
	}

	written = snprintf(s_logPath, sizeof(s_logPath), "%s", path);
	if ((written < 0) || ((size_t)written >= sizeof(s_logPath)))
	{
		s_logPath[0] = '\0';
		fprintf(stderr, "[CTR Native] Error: log path is too long\n");
		return 0;
	}

	return 1;
}

const char *Platform_LogGetPath(void)
{
	return s_logPath;
}

void Platform_LogInit(const char *appName)
{
	if (s_logPath[0] == '\0')
	{
		char safeName[256];
		size_t i;
		for (i = 0; appName && appName[i] != '\0' && i < sizeof(safeName) - 1; i++)
		{
			char c = appName[i];
			if (c == ':' || c == '/' || c == '\\' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			{
				safeName[i] = '_';
			}
			else
			{
				safeName[i] = c;
			}
		}
		safeName[i] = '\0';

		unsigned long pid = 0;
#ifdef _WIN32
		pid = (unsigned long)GetCurrentProcessId();
#elif !defined(__vita__)
		pid = (unsigned long)getpid();
#endif

		int written = snprintf(s_logPath, sizeof(s_logPath), "%s_%lu.log", safeName, pid);

		if ((written < 0) || ((size_t)written >= sizeof(s_logPath)))
		{
			fprintf(stderr, "[CTR Native] Error: log filename is too long\n");
			s_logPath[0] = '\0';
			return;
		}
	}

	s_logStream = fopen(s_logPath, "wb");

	if (s_logStream == NULL)
	{
		fprintf(stderr, "[CTR Native] Error: cannot create log file '%s'\n", s_logPath);
	}
}

void Platform_LogShutdown(void)
{
	Platform_LogWarn("---- LOG CLOSED ----\n");

	if (s_logStream != NULL)
	{
		fclose(s_logStream);
	}

	s_logStream = NULL;
}

void Platform_LogFlush(void)
{
	if (s_logStream != NULL)
	{
		fflush(s_logStream);
	}
}

void Platform_Log(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	Platform_LogV(stdout, fmt, args);
	va_end(args);
}

void Platform_LogWarn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	Platform_LogV(stdout, fmt, args);
	va_end(args);
}

void Platform_LogError(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	Platform_LogV(stderr, fmt, args);
	va_end(args);
}
