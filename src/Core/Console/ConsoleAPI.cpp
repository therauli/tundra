/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   ConsoleAPI.cpp
 *  @brief  Console core API.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "ConsoleAPI.h"
#include "ConsoleWidget.h"
#include "ShellInputThread.h"
#include "Application.h"
#include "Profiler.h"
#include "Framework.h"
#include "InputAPI.h"
#include "UiAPI.h"
#include "UiGraphicsView.h"
#include "LoggingFunctions.h"
#include "FunctionInvoker.h"

#include <stdlib.h>

#include <QFile>
#include <QTextStream>

#include "MemoryLeakCheck.h"

ConsoleAPI::ConsoleAPI(Framework *fw) :
    QObject(fw),
    framework(fw),
    enabledLogChannels(LogLevelErrorWarnInfo),
    logFile(0),
    logFileText(0)
{
    if (!fw->IsHeadless())
        consoleWidget = new ConsoleWidget(framework);

    inputContext = framework->Input()->RegisterInputContext("Console", 100);
    inputContext->SetTakeKeyboardEventsOverQt(true);
    connect(inputContext.get(), SIGNAL(KeyEventReceived(KeyEvent *)), SLOT(HandleKeyEvent(KeyEvent *)));

    RegisterCommand("help", "Lists all registered commands.", this, SLOT(ListCommands()));
    RegisterCommand("clear", "Clears the console log.", this, SLOT(ClearLog()));
    RegisterCommand("loglevel", "Sets the current log level. Call with one of the parameters \"error\", \"warning\", \"info\", or \"debug\".",
        this, SLOT(SetLogLevel(const QString &)));

    shellInputThread = boost::shared_ptr<ShellInputThread>(new ShellInputThread);

    QStringList logLevel = fw->CommandLineParameters("--loglevel");
    if (logLevel.size() >= 1)
        SetLogLevel(logLevel[logLevel.size()-1]);
    if (logLevel.size() > 1)
        LogWarning("Ignoring multiple --loglevel command line parameters!");

    QStringList logFile = fw->CommandLineParameters("--logfile");
    if (logFile.size() >= 1)
        SetLogFile(logFile[logFile.size()-1]);
    if (logFile.size() > 1)
        LogWarning("Ignoring multiple --logfile command line parameters!");
}

ConsoleAPI::~ConsoleAPI()
{
    Reset();
}

void ConsoleAPI::Reset()
{
    commands.clear();
    inputContext.reset();
    SAFE_DELETE(consoleWidget);
    shellInputThread.reset();
    SAFE_DELETE(logFileText);
    SAFE_DELETE(logFile);
}

QVariant ConsoleCommand::Invoke(const QStringList &params)
{
    QVariant returnValue;

    // If we have a target QObject, invoke it.
    if (target)
    {
        FunctionInvoker fi;
        QString errorMessage;
        fi.Invoke(target, functionName, params, &returnValue, &errorMessage);
        if (!errorMessage.isEmpty())
            LogError("ConsoleCommand::Invoke returned an error: " + errorMessage);
    }

    // Also, there may exist a script-registered handler that implements this console command - invoke it.
    emit Invoked(params);

    return returnValue;
}

ConsoleCommand *ConsoleAPI::RegisterCommand(const QString &name, const QString &desc)
{
    if (commands.find(name) != commands.end())
    {
        LogWarning("ConsoleAPI: Command " + name + " is already registered.");
        return commands[name].get();
    }

    boost::shared_ptr<ConsoleCommand> command = boost::shared_ptr<ConsoleCommand>(new ConsoleCommand(name, desc, 0, ""));
    commands[name] = command;
    return command.get();
}

void ConsoleAPI::RegisterCommand(const QString &name, const QString &desc, QObject *receiver, const char *memberSlot)
{
    if (commands.find(name) != commands.end())
    {
        LogWarning("ConsoleAPI: Command " + name + " is already registered.");
        return;
    }

    boost::shared_ptr<ConsoleCommand> command = boost::shared_ptr<ConsoleCommand>(new ConsoleCommand(name, desc, receiver, memberSlot+1));
    commands[name] = command;
}

/// Splits a string of form "MyFunctionName(param1, param2, param3, ...)" into
/// a commandName = "MyFunctionName" and a list of parameters as a StringList.
void ParseCommand(QString command, QString &commandName, QStringList &parameterList)
{
    command = command.trimmed();
    if (command.isEmpty())
        return;

    int split = command.indexOf("(");
    if (split == -1)
    {
        commandName = command;
        return;
    }

    commandName = command.left(split).trimmed();
    // Take into account the possible ending ")" and strip it away from the parameter list.
    // Remove it only if it's the last character in the string, as f.ex. some code execution console
    // command could contain ')' in the syntax.
    int endOfSplit = command.lastIndexOf(")");
    if (endOfSplit != -1 && endOfSplit == command.length()-1)
        command.remove(endOfSplit, 1);
    parameterList = command.mid(split+1).split(",");
}

void ConsoleAPI::ExecuteCommand(const QString &command)
{
    PROFILE(ConsoleAPI_ExecuteCommand);

    QString commandName;
    QStringList parameterList;
    ParseCommand(command, commandName, parameterList);
    if (commandName.isEmpty())
        return;

    CommandMap::iterator iter = commands.find(commandName);
    if (iter == commands.end())
    {
        LogError("Cannot find a console command \"" + commandName + "\"!");
        return;
    }

    iter->second->Invoke(parameterList);
}

void ConsoleAPI::Print(const QString &message)
{
    if (consoleWidget)
        consoleWidget->PrintToConsole(message);
    ///\todo Temporary hack which appends line ending in case it's not there (output of console commands in headless mode)
    if (!message.endsWith("\n"))
    {
        printf("%s\n", message.toStdString().c_str());
        if (logFileText)
        {
            (*logFileText) << message << "\n";
            /// \note If we want to guarantee that each message gets to the log even in the presence of a crash, we must flush()
            /// after each write. Tested that on Windows 7, if you kill the process using Ctrl-C on command line, or from 
            /// task manager, the log will not contain all the text, so this is required for correctness.
            /// But this flush() after each message also causes a *serious* performance drawback.
            /// One way to try avoiding this issue is to move to using C API for file writing, and at atexit() and other crash 
            /// handlers, close the file handle gracefully.
            logFileText->flush(); 
        }
    }
    else
    {
        printf("%s", message.toStdString().c_str());
        if (logFileText)
        {
            (*logFileText) << message;
            logFileText->flush(); // See comment about flush() above.
        }
    }
}

void ConsoleAPI::ListCommands()
{
    Print("Available console commands:");
    for(CommandMap::iterator iter = commands.begin(); iter != commands.end(); ++iter)
        Print(iter->first + " - " + iter->second->Description());
}

void ConsoleAPI::ClearLog()
{
    if (consoleWidget)
        consoleWidget->ClearLog();
#ifdef _WINDOWS
    system("cls");
#else
    system("clear");
#endif
}

void ConsoleAPI::SetLogLevel(const QString &level)
{
    if (level.compare("error", Qt::CaseInsensitive) == 0)
        SetEnabledLogChannels(LogLevelErrorsOnly);
    else if (level.compare("warning", Qt::CaseInsensitive) == 0)
        SetEnabledLogChannels(LogLevelErrorWarning);
    else if (level.compare("info", Qt::CaseInsensitive) == 0)
        SetEnabledLogChannels(LogLevelErrorWarnInfo);
    else if (level.compare("debug", Qt::CaseInsensitive) == 0)
        SetEnabledLogChannels(LogLevelErrorWarnInfoDebug);
    else
        LogError("Unknown parameter \"" + level + "\" specified to ConsoleAPI::SetLogLevel!");
}

void ConsoleAPI::SetLogFile(const QString &wildCardFilename)
{
    QString filename = Application::ParseWildCardFilename(wildCardFilename);
    
    // An empty log file closes the log output writing.
    if (filename.isEmpty())
    {
        SAFE_DELETE(logFileText);
        SAFE_DELETE(logFile);
        return;
    }
    logFile = new QFile(filename);
    bool isOpen = logFile->open(QIODevice::WriteOnly | QIODevice::Text);
    if (!isOpen)
    {
        LogError("Failed to open file \"" + filename + "\" for logging! (parsed from string \"" + wildCardFilename + "\")");
        SAFE_DELETE(logFile);
    }
    else
    {
        printf("Opened logging file \"%s\".\n", filename.toStdString().c_str());
        logFileText = new QTextStream(logFile);
    }
}

void ConsoleAPI::Update(f64 frametime)
{
    PROFILE(ConsoleAPI_Update);

    std::string input = shellInputThread->GetLine();
    if (input.length() > 0)
        ExecuteCommand(input.c_str());
}

void ConsoleAPI::ToggleConsole()
{
    if (consoleWidget)
        consoleWidget->ToggleConsole();
}

void ConsoleAPI::HandleKeyEvent(KeyEvent *e)
{
    const QKeySequence &toggleConsole = framework->Input()->KeyBinding("ToggleConsole", QKeySequence(Qt::Key_F1));
    if (e->sequence == toggleConsole)
        ToggleConsole();
}

void ConsoleAPI::LogInfo(const QString &message)
{
    ::LogInfo(message);
    Print(message);
}

void ConsoleAPI::LogWarning(const QString &message)
{
    ::LogWarning(message);
    Print("Warning: " + message);
}

void ConsoleAPI::LogError(const QString &message)
{
    ::LogError(message);
    Print("Error: " + message);
}

void ConsoleAPI::LogDebug(const QString &message)
{
#ifdef _DEBUG
    ::LogDebug(message);
    Print("Debug: " + message);
#endif
}

void ConsoleAPI::Log(u32 logChannel, const QString &message)
{
    if (!IsLogChannelEnabled(logChannel))
        return;

    Print(message);
}

void ConsoleAPI::SetEnabledLogChannels(u32 newChannels)
{
    enabledLogChannels = newChannels;
}

bool ConsoleAPI::IsLogChannelEnabled(u32 logChannel) const
{
    return (enabledLogChannels & logChannel) != 0;
}

u32 ConsoleAPI::EnabledLogChannels() const
{
    return enabledLogChannels;
}

