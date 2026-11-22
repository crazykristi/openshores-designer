//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen designer - an MCP server, so that an AI assistant can help design.
//
// Started with --mcp, the designer opens its window as usual and also talks
// the Model Context Protocol (MCP) on its standard input and output: one
// JSON-RPC message a line, the way an MCP app (Claude Desktop, Claude Code
// and others) runs a local server. The AI gets the simple plan tools
// (plantools.h) and the designer's own: build, expected stats, save, open and
// a picture of the plan or the 3D view.
//
// Every change the AI makes goes through the designer's undo, and shows in
// the window straight away, so the player sees it work and can take any of
// it back. Files are read and written only in one folder (--mcp-folder, or
// "Shipgen designs" in the player's documents).
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_MCPSERVER_H
#define SHIPGEN_MCPSERVER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QThread>

class DesignerWindow;

// Reads standard input a line at a time on a thread of its own (a read
// blocks; on Windows a pipe cannot be watched with a socket notifier).
class McpStdinReader : public QThread
{
	Q_OBJECT
public:
	explicit McpStdinReader(QObject* parent = 0) : QThread(parent) {}
signals:
	void	LineRead(const QByteArray& line);
	void	Closed();
protected:
	void	run() override;
};

class McpServer : public QObject
{
	Q_OBJECT
public:
	// Keeps standard output for the protocol alone: anything else the program
	// prints (the engine, the build) goes to standard error from now on. Call
	// it first thing, before anything is printed.
	static void	ClaimStdout();

	McpServer(DesignerWindow* window, const QString& folder, QObject* parent = 0);
	void		Start();

	// Where the files go: --mcp-folder, or the default.
	static QString	DefaultFolder();

private slots:
	void		OnLine(const QByteArray& line);
	void		OnClosed();

private:
	// One message at a time, in order: a build or a picture keeps the window
	// drawn while it works, and messages that come in meanwhile wait here.
	void		HandleNext();
	void		Handle(const QByteArray& line);
	void		Send(const QJsonObject& message);
	void		Reply(const QJsonValue& id, const QJsonObject& result);
	void		ReplyError(const QJsonValue& id, int code, const QString& message);
	QJsonObject	CallTool(const QString& name, const QJsonObject& args);
	QJsonArray	ToolDefinitions() const;
	// A file name the AI gave, as a path inside the folder; empty (and 'error'
	// set) when it would be outside it or has the wrong suffix.
	QString		FilePath(const QString& given, const QString& suffix, QString& error) const;

	DesignerWindow*	m_window;
	QString			m_folder;
	McpStdinReader*	m_reader = 0;
	bool			m_initialised = false;
	QList<QByteArray>	m_queue;
	bool			m_busy = false;
	bool			m_closed = false;
};

#endif
