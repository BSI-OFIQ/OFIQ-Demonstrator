#pragma once

#include <OFIQDemoOperation.h>

// Initializes OFIQ from the configured .jaxn file. The heavy work runs on a
// worker thread (Do); the result is applied on the GUI thread (Finish).
class OfiqInitOperation : public OFIQDemoOperation
{
public:
    explicit OfiqInitOperation(OfiqOperationContext& context);

    wxString Title() const override;
    bool Do(std::string& error) override;
    void Finish(bool success, const std::string& error) override;

private:
    OfiqOperationContext& m_context;
};
