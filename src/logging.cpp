//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - message handling shared by the command line and the tools.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "logging.h"

#include <QString>
#include <cstdio>

namespace {

QtMessageHandler	g_previous = 0;
bool				g_installed = false;

// The engine says this whenever the obstruction cache is refreshed without a
// scene client. There are four variants in auscparts.cpp, all with the same
// wording, and none of them means that anything is wrong here: building the
// cache is exactly what the report below needs.
static bool IsOfflineObstructionWarning(const QString& message)
{
	return message.contains(QLatin1String("must only be called on scene client"));
}

static void Handler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
	if(IsOfflineObstructionWarning(message))
		return;

	if(g_previous)
	{
		g_previous(type, context, message);
		return;
	}

	// Qt's default handler, near enough, for the case where no other handler
	// was installed before this one.
	const char* prefix = "debug";
	switch(type)
	{
	case QtInfoMsg:		prefix = "info"; break;
	case QtWarningMsg:	prefix = "warning"; break;
	case QtCriticalMsg:	prefix = "critical"; break;
	case QtFatalMsg:	prefix = "fatal"; break;
	default:			break;
	}
	fprintf(stderr, "%s: %s\n", prefix, qPrintable(message));
	fflush(stderr);
}

}

void InstallShipgenMessageHandler()
{
	if(g_installed)
		return;
	g_previous = qInstallMessageHandler(Handler);
	g_installed = true;
}
