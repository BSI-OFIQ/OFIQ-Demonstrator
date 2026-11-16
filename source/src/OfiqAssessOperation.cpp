#include <OfiqAssessOperation.h>
#include <OfiqInitOperation.h>

#include <cstdint>

OfiqAssessOperation::OfiqAssessOperation(OfiqOperationContext& context)
    : m_context(context)
{
}

wxString OfiqAssessOperation::Title() const
{
    return "OFIQ assessment";
}

bool OfiqAssessOperation::Init()
{
    if (!m_context.IsImageLoaded())
    {
        m_context.LogError("No image loaded.");
        return false;
    }
    return true;
}

// Worker thread: no GUI access. Initializes OFIQ lazily (by reusing
// OfiqInitOperation) and then runs the assessment via the context.
bool OfiqAssessOperation::Do(std::string& error)
{
    if (!m_context.IsOfiqInitialized())
    {
        OfiqInitOperation init(m_context);
        if (!init.Do(error))
        {
            return false;
        }
        m_context.SetOfiqInitialized(true);
    }

    // Recompute from scratch: drop any visualizations from a previous assessment
    // so stale entries cannot survive into the display.
    m_context.Visualizations().clear();

    uint32_t resultRequestsMask = static_cast<uint32_t>(OFIQ::PreprocessingResultType::All);
    auto result = m_context.OfiqInterface()->vectorQualityWithVisualization(
        m_context.InputImage(),
        m_context.Assessments(),
        m_context.Preprocessing(),
        m_context.Visualizations(),
        resultRequestsMask,
        m_context.RequestedVisualizations());
    if (result.code != OFIQ::ReturnCode::Success)
    {
        error = "OFIQ assessment returned: " + result.info;
        return false;
    }

    return true;
}

void OfiqAssessOperation::Finish(bool success, const std::string& error)
{
    if (success)
    {
        m_context.ShowAssessmentResults();
        m_context.LogInfo("OFIQ assessment done");
    }
    else
    {
        m_context.LogError(error);
    }
}
