#include "CLI.h"

#include "BuildInfo.h"

CLI parseCommandLine(QCommandLineParser & parser, QString & path, QString * errorMessage)
{
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    // trajectory
    parser.addPositionalArgument("Trajectory", "trajfile");

    const QCommandLineOption helpOption    = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();

    if(!parser.parse(QCoreApplication::arguments())) {
        *errorMessage = parser.errorText();
        return CLI::CommandLineError;
    }

    if(parser.isSet(versionOption))
        return CLI::CommandLineVersionRequested;

    if(parser.isSet(helpOption))
        return CLI::CommandLineHelpRequested;


    const QStringList positionalArguments = parser.positionalArguments();
    if(positionalArguments.isEmpty()) {
        *errorMessage = "trajectory file missing.";
        return CLI::CommandLineOk;
    }
    if(positionalArguments.size() > 1) {
        *errorMessage = "Several trajectory files specified.";
        return CLI::CommandLineError;
    }
    path = positionalArguments.first();
    return CLI::CommandLineOk;
}
