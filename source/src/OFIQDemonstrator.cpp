#include <filesystem>
#include <set>
#include <iostream>
#include <fstream>
#include <ofiq_lib.h>
#include <image_io.h>
#include <wx/wxprec.h>
#include <wx/wrapsizer.h>
#include <wx/gbsizer.h>
#include <wx/grid.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <OFIQPictureFrame.h>

#include <opencv2/opencv.hpp>

constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;

class OFIQDemoApp : public wxApp
{
public:
    virtual bool OnInit();
};

class OFIQDemoFrame : public wxFrame
{
public:
    OFIQDemoFrame();

protected:
    void OnMouseMoved(wxMouseEvent& event);
    void OnMouseClick(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);

private:
    bool IsKeyPressed(int keyCode);

    void OnLoadImage(wxCommandEvent& event);
    void OnSaveImage(wxCommandEvent& event);
    void OnSaveAssessment(wxCommandEvent& event);
    void OnSpecifyConfigPath(wxCommandEvent& event);
    void OnOfiqInit(wxCommandEvent& event);
    void OnOfiqAssess(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    void ZoomIn(wxCommandEvent& event = wxCommandEvent());
    void ZoomOut(wxCommandEvent& event = wxCommandEvent());
    void Zoom_1_4(wxCommandEvent& event);
    void Zoom_1_2(wxCommandEvent& event);
    void Zoom_1_1(wxCommandEvent& event);
    void Zoom_2_1(wxCommandEvent& event);
    void Zoom_4_1(wxCommandEvent& event);

    void OnShowOriginal(wxCommandEvent& event);
    void OnShowFaces(wxCommandEvent& event);
    void OnShowLandmarks(wxCommandEvent& event);
    void OnShowSegmentationMask(wxCommandEvent& event);
    void OnShowOcclusionMask(wxCommandEvent& event);
    void OnShowLandmarkedRegion(wxCommandEvent& event);

    bool DoLoadImage(const std::string& path);
    bool DoSaveImage(const std::string& path);
    bool DoSaveAssessment(const std::string& path);
    bool DoOfiqInit();

    void CreateCvImage();
    void UpdateOriginal();
    void UpdateFaces();
    void UpdateLandmarks();
    void UpdateSegmentationMask();
    void UpdateOcclusionMask();
    void UpdateLandmarkedRegion();
    void CreateWxImage();

    void DoUpdatePreferredScalingFactor();
    void DoInitImage();
    void DoUpdateImage();
    void DoClearAssessmentTable();
    void DoShowAssessmentTable();
    void DoClearPreprocessing();

    void LOG(const std::string& msg, const std::string& prefix = "");
    void LOG_INFO(const std::string& line);
    void LOG_ERROR(const std::string& line);

    double m_scaleFactor;
    double m_zoomFactor;
    std::set<int> m_pressedKeyCodes;
    wxFileDialog* m_configFileDialogPtr;
    wxFileDialog* m_imageFileDialogPtr;
    wxFileDialog* m_imageSaveFileDialogPtr;
    wxFileDialog* m_csvSaveFileDialogPtr;
    OFIQPictureFrame* m_pictureFramePtr;
    wxGrid* m_assessmentTablePtr;
    wxTextCtrl* m_logOutputPtr;

    wxSizer* m_assessmentTableSizerPtr;

    std::string m_ofiqConfigPath;
    std::shared_ptr<OFIQ::Interface> m_ofiqPtr;
    bool m_ofiqInitialized;

    std::string m_imagePath;
    OFIQ::Image m_ofiqImage;
    cv::Mat m_cvImage;
    wxImage m_wxImage;
    bool m_imageLoaded;

    bool m_showOriginal;
    bool m_showFaces;
    bool m_showLandmarks;
    bool m_showSegmentationMask;
    bool m_showOcclusionMask;
    bool m_showLandmarkedRegion;

    OFIQ::FaceImageQualityAssessment m_assessments;
    OFIQ::FaceImageQualityPreprocessingResult m_preprocessing;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(OFIQDemoFrame, wxFrame)
EVT_MOTION(OFIQDemoFrame::OnMouseMoved)
EVT_LEFT_DOWN(OFIQDemoFrame::OnMouseClick)
EVT_LEFT_UP(OFIQDemoFrame::OnMouseClick)
EVT_RIGHT_DOWN(OFIQDemoFrame::OnMouseClick)
EVT_RIGHT_UP(OFIQDemoFrame::OnMouseClick)
EVT_MOUSEWHEEL(OFIQDemoFrame::OnMouseWheel)
EVT_KEY_DOWN(OFIQDemoFrame::OnKeyDown)
EVT_KEY_UP(OFIQDemoFrame::OnKeyUp)
END_EVENT_TABLE()

enum
{
    ID_LoadImage = 1,
    ID_SaveImage,
    ID_SaveAssessment,
    ID_SpecifyConfigPath,
    ID_Initialize,
    ID_Assess,
    ID_ShowOriginal,
    ID_ShowFaces,
    ID_ShowLandmarks,
    ID_ShowSegmentationMask,
    ID_ShowOcclusionMask,
    ID_ShowLandmarkedRegion,
    ID_Log,
    ID_Zoom_1_4,
    ID_Zoom_1_2,
    ID_Zoom_1_1,
    ID_Zoom_2_1,
    ID_Zoom_4_1,
    ID_TopButton
};

const std::map<int, std::string> measurementMapping = {
    {0x41, "UnifiedQualityScore"},
    {0x42, "BackgroundUniformity"},
    {0x43, "IlluminationUniformity"},
    {-0x44, "Luminance"},
    {0x44, "LuminanceMean"},
    {0x45, "LuminanceVariance"},
    {0x46, "UnderExposurePrevention"},
    {0x47, "OverExposurePrevention"},
    {0x48, "DynamicRange"},
    {0x49, "Sharpness"},
    {0x4a, "CompressionArtifacts"},
    {0x4b, "NaturalColour"},
    {0x4c, "SingleFacePresent"},
    {0x4d, "EyesOpen"},
    {0x4e, "MouthClosed"},
    {0x4f, "EyesVisible"},
    {0x50, "MouthOcclusionPrevention"},
    {0x51, "FaceOcclusionPrevention"},
    {0x52, "InterEyeDistance"},
    {0x53, "HeadSize"},
    {-0x54, "CropOfTheFaceImage"},
    {0x54, "LeftwardCropOfTheFaceImage"},
    {0x55, "RightwardCropOfTheFaceImage"},
    {0x56, "DownwardCropOfTheFaceImage"},
    {0x57, "UpwardCropOfTheFaceImage"},
    {-0x58, "HeadPose"},
    {0x58, "HeadPoseYaw"},
    {0x59, "HeadPosePitch"},
    {0x5a, "HeadPoseRoll"},
    {0x5b, "ExpressionNeutrality"},
    {0x5c, "NoHeadCoverings"},
    {-1, "NotSet"}
};

wxIMPLEMENT_APP(OFIQDemoApp);

bool OFIQDemoApp::OnInit()
{
    OFIQDemoFrame* frame = new OFIQDemoFrame();
    frame->Show(true);
    return true;
}

OFIQDemoFrame::OFIQDemoFrame()
    : wxFrame(NULL, wxID_ANY, "OFIQ Demonstrator")
{
    wxMenu* menuFile = new wxMenu();
    menuFile->Append(ID_LoadImage, "&Load...\tCtrl-L",
        "Loads an image for OFIQ assessment");
    menuFile->AppendSeparator();
    menuFile->Append(ID_SaveImage, "&Save Image...\tCtrl-S",
        "Saves the visualized image");
    menuFile->Append(ID_SaveAssessment, "&Export Assesment...\tCtrl-E",
        "Exports the quality assessment in CSV format");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);

    wxMenu* menuOfiq = new wxMenu();
    menuOfiq->Append(ID_SpecifyConfigPath, "&Config path ...\tCtrl-P",
        "Specify path to OFIQ config file");
    menuOfiq->Append(ID_Initialize, "&Init...\tCtrl-I",
        "Initialize OFIQ using specified config file");
    menuOfiq->Append(ID_Assess, "&Assess...\tCtrl-A",
        "Assess loaded image using OFIQ");

    m_scaleFactor = 1.0;
    m_zoomFactor = 1.05;

    m_showOriginal = true;
    m_showFaces = true;
    m_showLandmarks = false;
    m_showSegmentationMask = false;
    m_showOcclusionMask = false;
    m_showLandmarkedRegion = false;

    wxMenu* menuView = new wxMenu();
    wxMenu* menuZoom = new wxMenu();
    wxMenuItem* zoom_1_4 = new wxMenuItem(menuZoom, ID_Zoom_1_4, wxT("1:4"), "");
    wxMenuItem* zoom_1_2 = new wxMenuItem(menuZoom, ID_Zoom_1_2, wxT("1:2"), "");
    wxMenuItem* zoom_1_1 = new wxMenuItem(menuZoom, ID_Zoom_1_1, wxT("1:1"), "");
    wxMenuItem* zoom_2_1 = new wxMenuItem(menuZoom, ID_Zoom_2_1, wxT("2:1"), "");
    wxMenuItem* zoom_4_1 = new wxMenuItem(menuZoom, ID_Zoom_4_1, wxT("4:1"), "");
    menuZoom->Append(zoom_1_4);
    menuZoom->Append(zoom_1_2);
    menuZoom->Append(zoom_1_1);
    menuZoom->Append(zoom_2_1);
    menuZoom->Append(zoom_4_1);
    menuView->AppendSubMenu(menuZoom, wxT("Zoom"));
    menuView->AppendSeparator();
    wxMenuItem* showOriginalItem = new wxMenuItem(menuView, ID_ShowOriginal, wxT("Show original"), "", wxITEM_CHECK);
    menuView->Append(showOriginalItem);
    showOriginalItem->SetCheckable(true);
    showOriginalItem->Check(m_showOriginal);
    wxMenuItem* showFacesItem = new wxMenuItem(menuView, ID_ShowFaces, wxT("Show faces"), "", wxITEM_CHECK);
    menuView->Append(showFacesItem);
    showFacesItem->SetCheckable(true);
    showFacesItem->Check(m_showFaces);
    wxMenuItem* showLandmarksItem = new wxMenuItem(menuView, ID_ShowLandmarks, wxT("Show landmarks"), "", wxITEM_CHECK);
    menuView->Append(showLandmarksItem);
    showLandmarksItem->SetCheckable(true);
    showLandmarksItem->Check(m_showLandmarks);
    wxMenuItem* showSegmentationMaskItem = new wxMenuItem(menuView, ID_ShowSegmentationMask, wxT("Show segmentation mask"), "", wxITEM_CHECK);
    menuView->Append(showSegmentationMaskItem);
    showSegmentationMaskItem->SetCheckable(true);
    showSegmentationMaskItem->Check(m_showSegmentationMask);
    wxMenuItem* showOcclusionMaskItem = new wxMenuItem(menuView, ID_ShowOcclusionMask, wxT("Show occlusion mask"), "", wxITEM_CHECK);
    menuView->Append(showOcclusionMaskItem);
    showOcclusionMaskItem->SetCheckable(true);
    showOcclusionMaskItem->Check(m_showOcclusionMask);
    wxMenuItem* showLandmarkedRegionItem = new wxMenuItem(menuView, ID_ShowLandmarkedRegion, wxT("Show landmarked region"), "", wxITEM_CHECK);
    menuView->Append(showLandmarkedRegionItem);
    showLandmarkedRegionItem->SetCheckable(true);
    showLandmarkedRegionItem->Check(m_showLandmarkedRegion);

    wxMenu* menuHelp = new wxMenu();
    menuHelp->Append(wxID_ABOUT);

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuOfiq, "&OFIQ");
    menuBar->Append(menuView, "&View");
    menuBar->Append(menuHelp, "&Help");

    SetMenuBar(menuBar);

    CreateStatusBar(2);
    SetStatusText("", 0);

    Bind(wxEVT_MENU, &OFIQDemoFrame::OnLoadImage, this, ID_LoadImage);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnSaveImage, this, ID_SaveImage);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnSaveAssessment, this, ID_SaveAssessment);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnSpecifyConfigPath, this, ID_SpecifyConfigPath);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnOfiqInit, this, ID_Initialize);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnOfiqAssess, this, ID_Assess);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnExit, this, wxID_EXIT);

    Bind(wxEVT_MENU, &OFIQDemoFrame::Zoom_1_4, this, ID_Zoom_1_4);
    Bind(wxEVT_MENU, &OFIQDemoFrame::Zoom_1_2, this, ID_Zoom_1_2);
    Bind(wxEVT_MENU, &OFIQDemoFrame::Zoom_1_1, this, ID_Zoom_1_1);
    Bind(wxEVT_MENU, &OFIQDemoFrame::Zoom_2_1, this, ID_Zoom_2_1);
    Bind(wxEVT_MENU, &OFIQDemoFrame::Zoom_4_1, this, ID_Zoom_4_1);

    Bind(wxEVT_MENU, &OFIQDemoFrame::OnShowOriginal, this, ID_ShowOriginal);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnShowFaces, this, ID_ShowFaces);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnShowLandmarks, this, ID_ShowLandmarks);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnShowSegmentationMask, this, ID_ShowSegmentationMask);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnShowOcclusionMask, this, ID_ShowOcclusionMask);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnShowLandmarkedRegion, this, ID_ShowLandmarkedRegion);

    m_ofiqConfigPath = "ofiq_config.jaxn";
    m_ofiqInitialized = false;

    m_configFileDialogPtr = new wxFileDialog(this,
        "Open config file",
        "",
        "",
        "JAXN file (*.jaxn)|*.jaxn", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    m_imageFileDialogPtr = new wxFileDialog(this,
        "Open Image File",
        "",
        "",
        "Image Files (*.png;*.jpg;*.jpeg;*.bmp)|*.png;*.jpg;*.jpeg;*.bmp", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    m_imageSaveFileDialogPtr = new wxFileDialog(this,
        "Save Image File",
        "",
        "",
        "Image Files (*.png;*.jpg;*.jpeg;*.bmp)|*.png;*.jpg;*.jpeg;*.bmp", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    m_csvSaveFileDialogPtr = new wxFileDialog(this,
        "Save Quality Assesment",
        "",
        "",
        "CSV file (*.csv)|*.csv", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    wxSystemAppearance appearance = wxSystemSettings::GetAppearance();
    if (!appearance.IsDark())
    {
        SetBackgroundColour(*wxWHITE);
    }


    auto topSplitter = new wxSplitterWindow(this, wxID_ANY);
    auto verticalSplitterWindow = new wxSplitterWindow(topSplitter, wxID_ANY);
    topSplitter->SetMinimumPaneSize(64);
    verticalSplitterWindow->SetMinimumPaneSize(64);

    // Create the left panel
    auto leftPanel = new wxPanel(verticalSplitterWindow, wxID_ANY);
    m_pictureFramePtr = new OFIQPictureFrame();
    m_pictureFramePtr->Create(leftPanel);
    m_imageLoaded = false;
    auto leftPanelSizer = new wxBoxSizer(wxHORIZONTAL);
    leftPanelSizer->Add(m_pictureFramePtr, 1, wxEXPAND);
    leftPanel->SetSizer(leftPanelSizer);

    // Create the right panel
    auto rightPanel = new wxPanel(verticalSplitterWindow, wxID_ANY);
    m_assessmentTablePtr = new wxGrid(rightPanel, wxID_ANY);
    m_assessmentTablePtr->CreateGrid(0, 3);
    m_assessmentTablePtr->EnableEditing(false);
    m_assessmentTablePtr->EnableGridLines(true);
    m_assessmentTablePtr->HideRowLabels();
    m_assessmentTablePtr->SetColLabelValue(0, "component");
    m_assessmentTablePtr->SetColLabelValue(1, "native");
    m_assessmentTablePtr->SetColLabelValue(2, "value");
    m_assessmentTablePtr->Show(false);
    m_assessmentTableSizerPtr = new wxBoxSizer(wxHORIZONTAL);
    m_assessmentTableSizerPtr->Add(m_assessmentTablePtr, 1, wxEXPAND);
    rightPanel->SetSizer(m_assessmentTableSizerPtr);


    auto downPanel = new wxPanel(topSplitter, wxID_ANY);
    auto downPanelSizer = new wxBoxSizer(wxVERTICAL);
    m_logOutputPtr = new wxTextCtrl(downPanel, ID_Log, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    m_logOutputPtr->SetEditable(false);
    downPanelSizer->Add(m_logOutputPtr, 1, wxEXPAND);
    downPanel->SetSizer(downPanelSizer);

    // Split the window vertically and set the left and right panes
    verticalSplitterWindow->SplitVertically(leftPanel, rightPanel);
    topSplitter->SplitHorizontally(verticalSplitterWindow, downPanel);
    verticalSplitterWindow->SetSashGravity(1.0);
    topSplitter->SetSashGravity(1.0);

    SetSize(WIDTH, HEIGHT);
}

void OFIQDemoFrame::OnMouseMoved(wxMouseEvent& event)
{
    SetFocus();
}

void OFIQDemoFrame::OnMouseClick(wxMouseEvent& event)
{
    SetFocus();
}

void OFIQDemoFrame::OnMouseWheel(wxMouseEvent& event)
{
    if (m_pressedKeyCodes.find(WXK_CONTROL) != m_pressedKeyCodes.end())
    {
        int value = event.GetWheelRotation();
        if (value > 0)
        {
            ZoomIn();
        }
        else if (value < 0)
        {
            ZoomOut();
        }
    }
    else
    {
        m_pictureFramePtr->OnMouse(event);
    }
}

void OFIQDemoFrame::OnKeyDown(wxKeyEvent& event)
{
    auto keyCode = event.GetKeyCode();

    m_pressedKeyCodes.insert(keyCode);

    if (IsKeyPressed(WXK_CONTROL))
    {
        if (IsKeyPressed(WXK_NUMPAD_ADD) || IsKeyPressed('+'))
        {
            ZoomIn();
        }
        else if (IsKeyPressed(WXK_NUMPAD_SUBTRACT) || IsKeyPressed('-'))
        {
            ZoomOut();
        }
    }
}

void OFIQDemoFrame::OnKeyUp(wxKeyEvent& event)
{
    m_pressedKeyCodes.erase(event.GetKeyCode());
}

void OFIQDemoFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void OFIQDemoFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("This is a demonstrator for the Open Face Image Quality (OFIQ) library",
        "About OFIQ Demonstrator", wxOK | wxICON_INFORMATION);
}

void OFIQDemoFrame::ZoomIn(wxCommandEvent& event)
{
    m_scaleFactor *= m_zoomFactor;
    DoUpdateImage();
}

void OFIQDemoFrame::ZoomOut(wxCommandEvent& event)
{
    m_scaleFactor /= m_zoomFactor;
    DoUpdateImage();
}

void OFIQDemoFrame::Zoom_1_4(wxCommandEvent& event)
{
    m_scaleFactor = 0.25;
    DoUpdateImage();
}

void OFIQDemoFrame::Zoom_1_2(wxCommandEvent& event)
{
    m_scaleFactor = 0.5;
    DoUpdateImage();
}

void OFIQDemoFrame::Zoom_1_1(wxCommandEvent& event)
{
    m_scaleFactor = 1.0;
    DoUpdateImage();
}

void OFIQDemoFrame::Zoom_2_1(wxCommandEvent& event)
{
    m_scaleFactor = 2.0;
    DoUpdateImage();
}

void OFIQDemoFrame::Zoom_4_1(wxCommandEvent& event)
{
    m_scaleFactor = 4.0;
    DoUpdateImage();
}



void OFIQDemoFrame::OnShowOriginal(wxCommandEvent& event)
{
    m_showOriginal = event.IsChecked();
    DoUpdateImage();
}

void OFIQDemoFrame::OnShowFaces(wxCommandEvent& event)
{
    m_showFaces = event.IsChecked();
    DoUpdateImage();
}

void OFIQDemoFrame::OnShowLandmarks(wxCommandEvent& event)
{
    m_showLandmarks = event.IsChecked();
    DoUpdateImage();
}

void OFIQDemoFrame::OnShowSegmentationMask(wxCommandEvent& event)
{
    m_showSegmentationMask = event.IsChecked();
    DoUpdateImage();
}

void OFIQDemoFrame::OnShowOcclusionMask(wxCommandEvent& event)
{
    m_showOcclusionMask = event.IsChecked();
    DoUpdateImage();
}

void OFIQDemoFrame::OnShowLandmarkedRegion(wxCommandEvent& event)
{
    m_showLandmarkedRegion = event.IsChecked();
    DoUpdateImage();
}

bool OFIQDemoFrame::IsKeyPressed(int keyCode)
{
    return m_pressedKeyCodes.find(keyCode) != m_pressedKeyCodes.end();
}

void OFIQDemoFrame::OnLoadImage(wxCommandEvent& event)
{
    if (m_imageFileDialogPtr->ShowModal() == wxID_CANCEL)
    {
        return;
    }
    else
    {
        wxBusyCursor wait;
        std::string image_path = m_imageFileDialogPtr->GetPath();
        DoLoadImage(image_path);
    }
}

void OFIQDemoFrame::OnSaveImage(wxCommandEvent& event)
{
    if (m_imageSaveFileDialogPtr->ShowModal() == wxID_CANCEL)
    {
        return;
    }
    else
    {
        wxBusyCursor wait;
        std::string image_path = m_imageSaveFileDialogPtr->GetPath();
        DoSaveImage(image_path);
    }
}

void OFIQDemoFrame::OnSaveAssessment(wxCommandEvent& event)
{
    if (m_csvSaveFileDialogPtr->ShowModal() == wxID_CANCEL)
    {
        return;
    }
    else
    {
        wxBusyCursor wait;
        std::string csv_path = m_csvSaveFileDialogPtr->GetPath();
        DoSaveAssessment(csv_path);
    }
}

void OFIQDemoFrame::OnSpecifyConfigPath(wxCommandEvent& event)
{
    if (m_configFileDialogPtr->ShowModal() == wxID_CANCEL)
    {
        return;
    }
    else
    {
        m_ofiqConfigPath = m_configFileDialogPtr->GetPath();
        m_ofiqInitialized = false;
        LOG_INFO("OFIQ config path specified: " + m_ofiqConfigPath);
    }
}

void OFIQDemoFrame::OnOfiqInit(wxCommandEvent& event)
{
    wxBusyCursor wait; // Assumingly, the wait cursor is shown for the time the object is alive.
    m_ofiqInitialized = DoOfiqInit();
}

void OFIQDemoFrame::OnOfiqAssess(wxCommandEvent& event)
{
    wxBusyCursor wait;

    LOG_INFO("OFIQ assessment ...");

    if (!m_imageLoaded)
    {
        LOG_ERROR("No image loaded.");
        return;
    }

    if (!m_ofiqInitialized)
    {
        m_ofiqInitialized = DoOfiqInit();
        if (!m_ofiqInitialized) {
            return;
        }
    }

    uint32_t resultRequestsMask = static_cast<int>(OFIQ::PreprocessingResultType::All);
    auto result = m_ofiqPtr->vectorQualityWithPreprocessingResults(
        m_ofiqImage, m_assessments, m_preprocessing, resultRequestsMask);
    if (result.code != OFIQ::ReturnCode::Success)
    {
        LOG_ERROR("OFIQ assessment returned: " + result.info);
    }

    DoUpdateImage();
    DoShowAssessmentTable();

    LOG_INFO("OFIQ assessment done");
}

bool OFIQDemoFrame::DoLoadImage(const std::string& path)
{
    LOG_INFO("Loading image from '" + path + "' ...");

    OFIQ::ReturnStatus retStatus = OFIQ_LIB::readImage(path, m_ofiqImage);
    if (retStatus.code != OFIQ::ReturnCode::Success)
    {
        LOG_ERROR("Loading image returned: " + retStatus.info);
        return false;
    }

    this->m_imagePath = path;

    DoClearAssessmentTable();
    DoClearPreprocessing();

    DoUpdatePreferredScalingFactor();
    DoInitImage();

    LOG_INFO("Image loaded.");

    return m_imageLoaded;
}

bool OFIQDemoFrame::DoSaveImage(const std::string& path)
{
    LOG_INFO("Saving image to '" + path + "' ...");

    bool flag = cv::imwrite(path, m_cvImage);

    if (flag)
    {
        LOG_INFO("Image saved.");
    }
    else
    {
        LOG_ERROR("Saving image failed.");
    }

    return flag;
}

bool OFIQDemoFrame::DoSaveAssessment(const std::string& path)
{
    const static char sep = ';';

    LOG_INFO("Exporting assessment to '" + path + "' ...");

    std::ofstream csv_stream(path.c_str());
    if (csv_stream.is_open())
    {
        csv_stream << "Filename";

        for (auto q : m_assessments.qAssessments)
        {
            auto measureId = static_cast<int>(q.first);
            csv_stream << sep << measurementMapping.at(measureId);
        }

        for (auto q : m_assessments.qAssessments)
        {
            auto measureId = static_cast<int>(q.first);
            csv_stream << sep << measurementMapping.at(measureId) << ".scalar";
        }

        csv_stream << std::endl;

        csv_stream << m_imagePath;

        for (auto q : m_assessments.qAssessments)
        {
            csv_stream << sep << q.second.rawScore;
        }

        for (auto q : m_assessments.qAssessments)
        {
            csv_stream << sep << q.second.scalar;
        }

        csv_stream << std::endl;

        csv_stream.close();
        LOG_INFO("Assessment exported.");
    }
    else
    {
        LOG_ERROR("Failed to write assessment.");
    }


    return true;
}

bool OFIQDemoFrame::DoOfiqInit()
{
    LOG_INFO("OFIQ initialization ...");

    if (!std::filesystem::is_regular_file(m_ofiqConfigPath))
    {
        LOG_ERROR("Not an existing file: " + m_ofiqConfigPath);
        return false;
    }

    auto path = std::filesystem::absolute(m_ofiqConfigPath);
    auto configDir = path.parent_path().u8string();
    auto configFile = path.filename().u8string();

    m_ofiqPtr = OFIQ::Interface::getImplementation();
    auto ret = m_ofiqPtr->initialize(configDir, configFile);
    if (ret.code != OFIQ::ReturnCode::Success)
    {
        LOG_ERROR("Not an existing file: " + m_ofiqConfigPath);
        return false;
    }

    LOG_INFO("OFIQ initialization done");

    return true;
}

void OFIQDemoFrame::UpdateOriginal()
{
    if (m_showOriginal)
    {
        auto channels = m_ofiqImage.depth / 8;
        bool isRGB = (channels == 3);

        memcpy(m_cvImage.data, m_ofiqImage.data.get(), m_ofiqImage.size());
        if (!isRGB)
        {
            cv::cvtColor(m_cvImage, m_cvImage, cv::COLOR_GRAY2BGR);
        }
        else
        {
            cv::cvtColor(m_cvImage, m_cvImage, cv::COLOR_BGR2RGB);
        }
    }
    else
    {
        memset(m_cvImage.data, 255, m_ofiqImage.size());
    }
}

void OFIQDemoFrame::UpdateFaces()
{
    if (!m_showFaces)
    {
        return;
    }

    int width = m_ofiqImage.width;
    int height = m_ofiqImage.height;
    const cv::Scalar faceColour(0, 0, 255);
    int thickness = (int)std::ceil(0.01 * (height < width ? height : width));

    for (auto face : m_preprocessing.m_faces)
    {
        cv::Rect cvRect(face.xleft, face.ytop, face.width, face.height);
        cv::rectangle(m_cvImage, cvRect, faceColour, thickness);
    }
}

void OFIQDemoFrame::UpdateLandmarks()
{
    if (!m_showLandmarks)
    {
        return;
    }

    const cv::Vec3b FACE_CONTOUR_COLOR(255, 255, 0);
    const cv::Vec3b EYE_BROWS_COLOR(255, 0, 0);
    const cv::Vec3b NOSE_COLOR(0, 0, 0);
    const cv::Vec3b OUTER_BOUNDARY_OF_EYES_COLOR(128, 0, 128);
    const cv::Vec3b OUTER_BOUNDARY_OF_LIPS_COLOR(0, 0, 255);
    const cv::Vec3b INNER_BOUNDARY_OF_LIPS_COLOR(0, 255, 0);
    const cv::Vec3b PUPILS_COLOR(255, 255, 255);

    const int height = m_cvImage.rows;
    const int width = m_cvImage.cols;
    const int radius = (int)std::ceil(0.005 * (height < width ? height : width));
    const auto& landmarks = m_preprocessing.m_landmarks.landmarks;
    const auto& lmtypes = m_preprocessing.m_landmarks.type;

    for (size_t lm_label = 0; lm_label < landmarks.size(); lm_label++)
    {
        cv::Vec3b color;
        if (lm_label < 33)
        {
            color = FACE_CONTOUR_COLOR;
        }
        else if (lm_label < 47)
        {
            color = EYE_BROWS_COLOR;
        }
        else if (lm_label < 60)
        {
            color = NOSE_COLOR;
        }
        else if (lm_label < 76)
        {
            color = OUTER_BOUNDARY_OF_EYES_COLOR;
        }
        else if (lm_label < 88)
        {
            color = OUTER_BOUNDARY_OF_LIPS_COLOR;
        }
        else if (lm_label < 96)
        {
            color = INNER_BOUNDARY_OF_LIPS_COLOR;
        }
        else
        {
            color = PUPILS_COLOR;
        }
        auto& lm = landmarks[lm_label];
        cv::circle(m_cvImage, cv::Point(lm.x, lm.y), radius, color, -1);
    }
}

void OFIQDemoFrame::UpdateSegmentationMask()
{
    if (!m_showSegmentationMask || m_preprocessing.m_segmentationMaskPtr == nullptr)
    {
        return;
    }

    const static int labelCount = 24;
    const static cv::Vec3b colorMap[] = {
        cv::Vec3b(128, 128, 128), // 0: background
        cv::Vec3b(255, 85, 0), // 1: face_skin
        cv::Vec3b(255, 170, 0), // 2: left eye brow
        cv::Vec3b(255, 0, 85), // 3: right eye brow
        cv::Vec3b(255, 0, 170), // 4: left eye
        cv::Vec3b(0, 255, 0), // 5: right eye
        cv::Vec3b(0, 255, 255), // 6: eyeglasses
        cv::Vec3b(170, 255, 0), // 7: left ear
        cv::Vec3b(0, 255, 85), // 8: right ear
        cv::Vec3b(0, 255, 170), // 9: earring
        cv::Vec3b(0, 0, 255), // 10: nose
        cv::Vec3b(85, 0, 255), // 11: mouth
        cv::Vec3b(170, 0, 255), // 12: upper lip
        cv::Vec3b(0, 85, 255), // 13: lower lip
        cv::Vec3b(0, 170, 255), // 14: neck
        cv::Vec3b(255, 255, 0), // 15: necklace
        cv::Vec3b(255, 255, 85), // 16: clothing
        cv::Vec3b(255, 255, 170), // 17: hair
        cv::Vec3b(255, 0, 255), // 18: head covering
        cv::Vec3b(255, 85, 255), // 19:
        cv::Vec3b(255, 170, 255), // 20:
        cv::Vec3b(85, 255, 255), // 21:
        cv::Vec3b(170, 255, 255), // 22:
        cv::Vec3b(85, 255, 0) }; // 23:

    int height = m_cvImage.rows;
    int width = m_cvImage.cols;
    double alpha = 0.3;
    double beta = 1.0 - alpha;

    cv::Mat overlay(height, width, CV_8UC3);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            cv::Vec3b& color = overlay.at<cv::Vec3b>(cv::Point(x, y));
            auto val = m_preprocessing.m_segmentationMaskPtr.get()[y * width + x];
            if (val >= labelCount)
            {
                color[0] = 255;
                color[1] = 255;
                color[2] = 255;
            }
            else
            {
                color[0] = colorMap[val][0];
                color[1] = colorMap[val][1];
                color[2] = colorMap[val][2];
            }
        }
    }

    cv::addWeighted(overlay, alpha, m_cvImage, beta, 0.0, m_cvImage);
}

void OFIQDemoFrame::UpdateOcclusionMask()
{
    if (!m_showOcclusionMask || m_preprocessing.m_occlusionMaskPtr == nullptr)
    {
        return;
    }

    const cv::Vec3b foregroundColor(0, 0, 255);
    const cv::Vec3b backgroundColor(255, 255, 255);
    int height = m_cvImage.rows;
    int width = m_cvImage.cols;
    double alpha = 0.3;
    double beta = 1.0 - alpha;

    cv::Mat overlay(height, width, CV_8UC3);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            cv::Vec3b& color = overlay.at<cv::Vec3b>(cv::Point(x, y));
            auto val = m_preprocessing.m_occlusionMaskPtr.get()[y * width + x];
            if (val)
            {
                color = foregroundColor;
            }
            else
            {
                color = backgroundColor;
            }
        }
    }
    cv::addWeighted(overlay, alpha, m_cvImage, beta, 0.0, m_cvImage);
}

void OFIQDemoFrame::UpdateLandmarkedRegion()
{
    if (!m_showLandmarkedRegion || m_preprocessing.m_landmarkedRegionPtr == nullptr)
    {
        return;
    }

    const cv::Vec3b foregroundColor(255, 0, 0);
    const cv::Vec3b backgroundColor(255, 255, 255);
    int height = m_cvImage.rows;
    int width = m_cvImage.cols;
    double alpha = 0.3;
    double beta = 1.0 - alpha;

    cv::Mat overlay(height, width, CV_8UC3);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            cv::Vec3b& color = overlay.at<cv::Vec3b>(cv::Point(x, y));
            auto val = m_preprocessing.m_landmarkedRegionPtr.get()[y * width + x];
            if (val)
            {
                color = foregroundColor;
            }
            else
            {
                color = backgroundColor;
            }
        }
    }
    cv::addWeighted(overlay, alpha, m_cvImage, beta, 0.0, m_cvImage);
}

void OFIQDemoFrame::CreateCvImage()
{
    auto width = m_ofiqImage.width;
    auto height = m_ofiqImage.height;
    auto channels = m_ofiqImage.depth / 8;
    bool isRGB = (channels == 3);

    m_cvImage = cv::Mat(height, width, isRGB ? CV_8UC3 : CV_8UC1);

    UpdateOriginal();
    UpdateFaces();
    UpdateLandmarks();
    UpdateSegmentationMask();
    UpdateOcclusionMask();
    UpdateLandmarkedRegion();
}

void OFIQDemoFrame::CreateWxImage()
{
    int width = m_cvImage.cols;
    int height = m_cvImage.rows;
    int channels = m_cvImage.channels();
    uint8_t* data = (uint8_t*)m_cvImage.data;
    int depth = channels * 8;
    m_wxImage = wxImage(width, height, true);
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            int r, g, b;
            b = data[i * width * channels + j * channels + 0];
            g = data[i * width * channels + j * channels + 1];
            r = data[i * width * channels + j * channels + 2];
            m_wxImage.SetRGB(j, i, r, g, b);
        }
    }

    m_pictureFramePtr->LoadImage(m_wxImage);
}

void OFIQDemoFrame::DoUpdatePreferredScalingFactor()
{
    auto size = GetSize();
    double horizontal_scale = static_cast<double>(size.x) / static_cast<double>(m_ofiqImage.width);
    double vertical_scale = static_cast<double>(size.y) / static_cast<double>(m_ofiqImage.height);
    m_scaleFactor = horizontal_scale < vertical_scale ? horizontal_scale : vertical_scale;
}

void OFIQDemoFrame::DoInitImage()
{
    wxBusyCursor wait; // Assumingly, the wait cursor is shown for the time the object is alive.
    CreateCvImage();
    CreateWxImage();
    m_pictureFramePtr->LoadImage(m_wxImage, m_scaleFactor);
    m_imageLoaded = true;
    SetStatusText(std::to_string(static_cast<int>(std::round(m_scaleFactor * 100.0))) + "%", 0);
}

void OFIQDemoFrame::DoUpdateImage()
{
    if (m_imageLoaded)
    {
        DoInitImage();
    }
}

void OFIQDemoFrame::DoClearAssessmentTable()
{
    if (m_assessmentTablePtr->GetNumberRows() != 0)
        m_assessmentTablePtr->DeleteRows(0, m_assessmentTablePtr->GetNumberRows(), false);
    m_assessmentTablePtr->Show(false);
}

void OFIQDemoFrame::DoClearPreprocessing()
{
    m_preprocessing = OFIQ::FaceImageQualityPreprocessingResult();
}

void OFIQDemoFrame::DoShowAssessmentTable()
{
    if (m_assessmentTablePtr->GetNumberRows() != 0)
    {
        m_assessmentTablePtr->DeleteRows(0, m_assessmentTablePtr->GetNumberRows(), false);
    }

    m_assessmentTablePtr->AppendRows(m_assessments.qAssessments.size());
    int row = 0;
    for (auto const& [measure, measure_result] : m_assessments.qAssessments)
    {
        const OFIQ::QualityMeasureResult& qaResult = measure_result;

        double nativeScore = -1.0;
        int qualityScore = -1;
        if (measure_result.code == OFIQ::QualityMeasureReturnCode::Success)
        {
            nativeScore = qaResult.rawScore;
            qualityScore = qaResult.scalar;
        }

        auto it = measurementMapping.find(static_cast<int>(measure));
        if (it != measurementMapping.end())
        {
            m_assessmentTablePtr->SetCellValue(row, 0, it->second);
        }
        else
        {
            m_assessmentTablePtr->SetCellValue(row, 0, "NotSet");
        }
        m_assessmentTablePtr->SetCellValue(row, 1, std::to_string(nativeScore));
        m_assessmentTablePtr->SetCellValue(row, 2, std::to_string(qualityScore));
        row++;
    }

    m_assessmentTablePtr->Show(true);
    m_assessmentTableSizerPtr->Layout();
}

void OFIQDemoFrame::LOG(const std::string& msg, const std::string& prefix)
{
    time_t timestamp;
    time(&timestamp);
    *m_logOutputPtr << strtok(ctime(&timestamp), "\n") << " " << prefix << ": " << msg.c_str() << "\n";
}

void OFIQDemoFrame::LOG_INFO(const std::string& info_message)
{
    LOG(info_message, "INFO");
}

void OFIQDemoFrame::LOG_ERROR(const std::string& error_message)
{
    LOG(error_message, "ERROR");
    wxMessageBox(error_message);
}