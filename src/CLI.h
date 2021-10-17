#pragma once
#include <QCommandLineParser>
#include <QStringList>
#include <filesystem>

enum class CLI {
    CommandLineOk,
    CommandLineError,
    CommandLineVersionRequested,
    CommandLineHelpRequested
};

CLI parseCommandLine(
    QCommandLineParser & parser,
    std::optional<std::filesystem::path> & path,
    QString * errorMessage);
