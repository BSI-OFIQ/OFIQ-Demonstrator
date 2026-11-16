#include <OfiqInitOperation.h>

#include <filesystem>

OfiqInitOperation::OfiqInitOperation(OfiqOperationContext& context)
    : m_context(context)
{
}

wxString OfiqInitOperation::Title() const
{
    return "OFIQ initialization";
}

// Worker thread: no GUI access. Reads the config path and OFIQ interface via
// the context.
bool OfiqInitOperation::Do(std::string& error)
{
    const std::string& configPath = m_context.OfiqConfigPath();
    if (!std::filesystem::is_regular_file(configPath))
    {
        error = "Not an existing file: " + configPath;
        return false;
    }

    auto path = std::filesystem::absolute(configPath);
    auto configDir = path.parent_path().u8string();
    auto configFile = path.filename().u8string();

    auto implementation = OFIQ::Interface::getImplementation();
    auto ret = implementation->initialize(configDir, configFile);
    if (ret.code != OFIQ::ReturnCode::Success)
    {
        error = "Failed to initialize OFIQ: " + ret.info;
        return false;
    }

    m_context.OfiqInterface() = implementation;
    return true;
}

void OfiqInitOperation::Finish(bool success, const std::string& error)
{
    m_context.SetOfiqInitialized(success);
    if (success)
    {
        m_context.LogInfo("OFIQ initialization done");
        // (Re)build the visualization menu from the now-initialized measures.
        m_context.BuildVisualizationMenu();
    }
    else
    {
        m_context.LogError(error);
    }
}
