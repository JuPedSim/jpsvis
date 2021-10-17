#include "CLI.h"

CLI parseCommandLine(
    QCommandLineParser & parser,
    std::filesystem::path & path,
    QString * errorMessage)
{
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
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
    if(positionalArguments.size() > 1) {
        *errorMessage = "Several trajectory files specified.";
        return CLI::CommandLineError;
    }
    if(not positionalArguments.isEmpty())
        path = positionalArguments[0].toStdString();
    return CLI::CommandLineOk;
}
