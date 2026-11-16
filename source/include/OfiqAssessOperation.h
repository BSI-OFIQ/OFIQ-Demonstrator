#pragma once

#include <OFIQDemoOperation.h>

// Runs the OFIQ quality assessment on the loaded image (initializing OFIQ
// lazily if needed). The assessment runs on a worker thread (Do); the results
// are shown on the GUI thread (Finish).
class OfiqAssessOperation : public OFIQDemoOperation
{
public:
    explicit OfiqAssessOperation(OfiqOperationContext& context);

    wxString Title() const override;
    bool Init() override;
    bool Do(std::string& error) override;
    void Finish(bool success, const std::string& error) override;

private:
    OfiqOperationContext& m_context;
};
