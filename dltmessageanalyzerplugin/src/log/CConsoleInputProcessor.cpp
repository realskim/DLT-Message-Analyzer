#include "../common/Definitions.hpp"

#include "../dltmessageanalyzerplugin.hpp"
#include "CConsoleInputProcessor.hpp"
#include "CConsoleCtrl.hpp"

static void webLink()
{
    SEND_MSG("Web resource of the plugin: https://svlad-90.github.io/DLT-Message-Analyzer/");
}

static void supportedColors()
{
    SEND_MSG("Supported colors:");

    for(const auto& colorItem : sColorsMap)
    {
        SEND_MSG_COLORED(colorItem.first, colorItem.second);
    }
}

static void version()
{
    SEND_MSG(QString("%1. Version: %2. Author: %3")
             .arg(DLT_MESSAGE_ANALYZER_NAME)
             .arg(DLT_MESSAGE_ANALYZER_PLUGIN_VERSION)
             .arg(DLT_MESSAGE_ANALYZER_PLUGIN_AUTHOR));
}

static void support()
{
    SEND_MSG("In case of facing issues with the plugin, please, report here - https://github.com/svlad-90/DLT-Message-Analyzer/issues");
}

static void clear()
{
    CLEAR_CONSOLE_VIEW();
}

static CConsoleInputProcessor::tScenariosMap createScenariosMap()
{
    CConsoleInputProcessor::tScenariosMap result;

    result.insert(std::make_pair( QString("clear"), [](){clear();} ));
    result.insert(std::make_pair( QString("color-aliases"), [](){supportedColors();} ));
    result.insert(std::make_pair( QString("support"), [](){support();} ));
    result.insert(std::make_pair( QString("version"), [](){version();} ));
    result.insert(std::make_pair( QString("web-link"), [](){webLink();} ));

    return result;
}

CConsoleInputProcessor::CConsoleInputProcessor( QLineEdit* pTargetLineEdit ):
    mScenariosMap(createScenariosMap())
{
    if(nullptr != pTargetLineEdit)
    {
        connect(pTargetLineEdit, &QLineEdit::returnPressed,
                [this, pTargetLineEdit]()
        {
            if(nullptr != pTargetLineEdit)
            {
                SEND_MSG("========================================");

                QString text = pTargetLineEdit->text();

                pTargetLineEdit->clear();

                SEND_MSG(text);

                QString lowerCandidate = text.toLower();

                if(lowerCandidate == "help")
                {
                    SEND_MSG("Supported commands:");

                    for(const auto& scenarioItem : mScenariosMap)
                    {
                        SEND_MSG(QString("- ").append(scenarioItem.first));
                    }
                }
                else
                {
                    auto foundScenario = mScenariosMap.find(lowerCandidate);

                    if( foundScenario != mScenariosMap.end() )
                    {
                        foundScenario->second();
                    }
                    else
                    {
                        SEND_WRN(QString("Command \"%1\" not supported. Input \"help\" in order to get the list of supported commands.").arg(text));
                    }
                }

                if(lowerCandidate != "clear")
                {
                    SEND_MSG("========================================");
                }
            }
        });
    }
}

CConsoleInputProcessor::~CConsoleInputProcessor()
{}