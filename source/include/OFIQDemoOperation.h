#pragma once

#include <memory>
#include <string>
#include <wx/string.h>
#include <ofiq_lib.h>

// Interface an operation uses to reach the application state and UI (implemented
// by OFIQDemoFrame). The concrete operations own the OFIQ logic themselves and
// only use this to read/write the shared OFIQ state and to report results.
//
// Threading: the state accessors may be used from a worker thread (from Do()).
// ShowAssessmentResults()/LogInfo()/LogError() touch the UI and must only be
// called from the GUI thread (i.e. from Init()/Finish()).
class OfiqOperationContext
{
public:
    virtual ~OfiqOperationContext() = default;

    // OFIQ state (worker-thread safe).
    virtual const std::string& OfiqConfigPath() const = 0;
    virtual std::shared_ptr<OFIQ::Interface>& OfiqInterface() = 0;
    virtual const OFIQ::Image& InputImage() const = 0;
    virtual OFIQ::FaceImageQualityAssessment& Assessments() = 0;
    virtual OFIQ::FaceImageQualityPreprocessingResult& Preprocessing() = 0;
    // Container receiving the per-measure ARGB visualizations produced by
    // Interface::vectorQualityWithVisualization.
    virtual std::map<OFIQ::QualityMeasure, std::vector<uint32_t>>& Visualizations() = 0;
    // The set of measures for which a visualization is requested during an
    // assessment (typically all measures that implement a visualization).
    virtual std::set<OFIQ::QualityMeasure> RequestedVisualizations() const = 0;
    virtual bool IsOfiqInitialized() const = 0;
    virtual void SetOfiqInitialized(bool initialized) = 0;

    // Query.
    virtual bool IsImageLoaded() const = 0;

    // GUI thread only.
    virtual void ShowAssessmentResults() = 0;
    // Builds/rebuilds the visualization check items in the View menu from the
    // currently initialized configuration.
    virtual void BuildVisualizationMenu() = 0;
    virtual void LogInfo(const std::string& message) = 0;
    virtual void LogError(const std::string& message) = 0;
};

// A single long-running, GUI-blocking operation, split into three phases so it
// can be driven by OFIQDemoFrame::RunOperation: the operation supplies the
// phases, the runner supplies the wait cursor, the window disabling, the worker
// thread and the marshalling back to the GUI thread.
//
// Threading contract:
//   Init()   - GUI thread, before the worker starts.
//   Do()     - WORKER thread. MUST NOT touch any wxWidgets object.
//   Finish() - GUI thread, after the worker finished (success or failure).
class OFIQDemoOperation
{
public:
    virtual ~OFIQDemoOperation() = default;

    // GUI thread. Operation-specific setup / validation before any work is
    // done. Return false to abort the operation (no worker is started, no
    // Finish() is called).
    virtual bool Init() { return true; }

    // Worker thread. The heavy lifting. MUST NOT call into wxWidgets. On
    // failure, fill `error` with a message for Finish() to report.
    virtual bool Do(std::string& error) = 0;

    // GUI thread. Apply results to the UI, log, show messages.
    virtual void Finish(bool success, const std::string& error) = 0;

    // Human-readable name, logged when the operation starts.
    virtual wxString Title() const = 0;
};
