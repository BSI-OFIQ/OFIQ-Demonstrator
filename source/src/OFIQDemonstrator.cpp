#include <filesystem>
#include <iostream>
#include <fstream>
#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>
#include <map>
#include <set>
#include <vector>
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
#include <OFIQCameraEnumeration.h>
#include <OFIQDemoOperation.h>
#include <OfiqInitOperation.h>
#include <OfiqAssessOperation.h>

#include <opencv2/opencv.hpp>

constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;

// Bounds for the zoom scale factor, expressed as the longest edge (in pixels)
// the scaled image may reach. Prevents a held Ctrl++ from blowing the image up
// to an absurd size (and exhausting memory in wxImage::Scale).
constexpr int MAX_SCALED_DIMENSION = 8000;
constexpr int MIN_SCALED_DIMENSION = 32;

// Shared between the GUI and the webcam capture thread. Held by shared_ptr so it
// outlives the thread: the GUI can drop its reference (and detach the thread) at
// any time without waiting for a possibly-blocked read()/open().
struct WebcamStream
{
    std::mutex mutex;
    cv::Mat latestFrame;    // guarded by mutex
    bool hasFrame = false;  // guarded by mutex
    std::atomic<bool> stop{ false };
    std::atomic<bool> opened{ false };
    std::atomic<bool> failed{ false };
};

// Runs on the capture thread. Owns its VideoCapture completely (open, read,
// release) and only ever touches the shared stream — never the wxFrame. That is
// what makes a non-blocking stop safe: on stop the GUI just sets stream->stop
// and detaches this thread. A hung read()/open() can then leak this thread but
// can never freeze or crash the GUI.
static void CaptureThreadMain(std::shared_ptr<WebcamStream> stream, int deviceIndex, int backend)
{
    cv::VideoCapture capture;
    capture.open(deviceIndex, backend);
    if (!capture.isOpened())
    {
        stream->failed = true;
        return;
    }
    stream->opened = true;

    while (!stream->stop)
    {
        cv::Mat frame;
        if (!capture.read(frame) || frame.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        std::lock_guard<std::mutex> lock(stream->mutex);
        stream->latestFrame = frame;
        stream->hasFrame = true;
    }

    capture.release();
}

class OFIQDemoApp : public wxApp
{
public:
    virtual bool OnInit();
};

class OFIQDemoFrame : public wxFrame, public OfiqOperationContext
{
public:
    OFIQDemoFrame();

    // OfiqOperationContext — the asynchronous operations own the OFIQ logic and
    // reach the required state through these accessors. The state accessors may
    // be used from the worker thread; the rest are GUI-thread only.
    const std::string& OfiqConfigPath() const override { return m_ofiqConfigPath; }
    std::shared_ptr<OFIQ::Interface>& OfiqInterface() override { return m_ofiqPtr; }
    const OFIQ::Image& InputImage() const override { return m_ofiqImage; }
    OFIQ::FaceImageQualityAssessment& Assessments() override { return m_assessments; }
    OFIQ::FaceImageQualityPreprocessingResult& Preprocessing() override { return m_preprocessing; }
    std::map<OFIQ::QualityMeasure, std::vector<uint32_t>>& Visualizations() override { return m_visualizations; }
    std::set<OFIQ::QualityMeasure> RequestedVisualizations() const override;
    bool IsOfiqInitialized() const override { return m_ofiqInitialized; }
    void SetOfiqInitialized(bool initialized) override { m_ofiqInitialized = initialized; }
    bool IsImageLoaded() const override { return m_imageLoaded; }
    void ShowAssessmentResults() override { DoUpdateImage(); DoShowAssessmentTable(); }
    // Rebuilds the View-menu visualization check items from the currently
    // initialized OFIQ configuration. Called right after each initialization.
    void BuildVisualizationMenu() override { RefreshVisualizationMenu(); }
    void LogInfo(const std::string& message) override { LOG_INFO(message); }
    void LogError(const std::string& message) override { LOG_ERROR(message); }

protected:
    void OnMouseMoved(wxMouseEvent& event);
    void OnMouseClick(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);

private:
    void OnLoadImage(wxCommandEvent& event);
    void OnStartCapture(wxCommandEvent& event);
    void OnCaptureFrame(wxCommandEvent& event);
    void OnCancelCapture(wxCommandEvent& event);
    void OnPreviewTick(wxTimerEvent& event); // GUI: display the latest buffered frame
    int PreferredCaptureBackend() const;
    void OnClose(wxCloseEvent& event);
    void OnSaveImage(wxCommandEvent& event);
    void OnSaveAssessment(wxCommandEvent& event);
    void OnSpecifyConfigPath(wxCommandEvent& event);
    void OnOfiqInit(wxCommandEvent& event);
    void OnOfiqAssess(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    void ZoomIn();
    void ZoomOut();
    void OnZoomIn(wxCommandEvent& event);
    void OnZoomOut(wxCommandEvent& event);
    double ClampScaleFactor(double scale) const;
    void RequestZoomRedraw();
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
    void OnShowVisualization(wxCommandEvent& event);
    // Creates a "Show <Measure> Visualization" check item in the View menu for
    // every measure that implements a visualization and does not yet have one.
    void RefreshVisualizationMenu();

    bool DoLoadImage(const std::string& path);
    bool DoCaptureImage(const cv::Mat& frame);
    // OFIQ init/assess logic now lives in the operation classes; see
    // OfiqInitOperation / OfiqAssessOperation.
    void EnumerateCameraDevices();
    void UpdateCaptureControls();
    void StopCapture();
    void ShowLiveFrame(const cv::Mat& frame);
    bool DoSaveImage(const std::string& path);
    bool DoSaveAssessment(const std::string& path);
    void RunOperation(std::unique_ptr<OFIQDemoOperation> operation);

    void CreateCvImage();
    void UpdateOriginal();
    void UpdateFaces();
    void UpdateLandmarks();
    void UpdateSegmentationMask();
    void UpdateOcclusionMask();
    void UpdateLandmarkedRegion();
    void UpdateVisualizations();
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
    bool m_zoomRedrawPending;
    wxFileDialog* m_configFileDialogPtr;
    wxFileDialog* m_imageFileDialogPtr;
    wxFileDialog* m_imageSaveFileDialogPtr;
    wxFileDialog* m_csvSaveFileDialogPtr;
    OFIQPictureFrame* m_pictureFramePtr;
    wxGrid* m_assessmentTablePtr;
    wxTextCtrl* m_logOutputPtr;

    wxChoice* m_cameraChoicePtr;
    wxButton* m_startCaptureButtonPtr;
    wxButton* m_captureButtonPtr;
    wxButton* m_cancelCaptureButtonPtr;
    std::vector<CameraDevice> m_cameraDevices;
    std::thread m_captureThread;             // owns the camera; grabs frames off the GUI thread
    std::shared_ptr<WebcamStream> m_stream;  // shared hand-off buffer; nullptr when idle
    wxTimer m_previewTimer;                  // GUI-side consumer: displays buffered frames
    cv::Mat m_lastFrame;                     // GUI-side latest frame (for "Capture")
    std::string m_activeCameraName;
    bool m_captureMode;

    wxSizer* m_assessmentTableSizerPtr;

    std::string m_ofiqConfigPath;
    std::shared_ptr<OFIQ::Interface> m_ofiqPtr;
    bool m_ofiqInitialized;
    bool m_operationInProgress;
    std::thread m_operationThread;
    std::unique_ptr<wxWindowDisabler> m_windowDisabler;

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

    // Per-measure ARGB visualizations from the last assessment, keyed by measure.
    std::map<OFIQ::QualityMeasure, std::vector<uint32_t>> m_visualizations;
    // The View menu, kept so visualization check items can be appended lazily.
    wxMenu* m_menuViewPtr;
    // Menu-item id -> measure, for the dynamically created visualization items.
    std::map<int, OFIQ::QualityMeasure> m_visualizationMenuIds;
    // Measure -> whether its visualization is currently shown (checkbox state).
    std::map<OFIQ::QualityMeasure, bool> m_showVisualization;
    // The separator preceding the visualization items (nullptr if none), kept so
    // it can be removed when the visualization menu is rebuilt.
    wxMenuItem* m_visualizationSeparatorItem;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(OFIQDemoFrame, wxFrame)
EVT_MOTION(OFIQDemoFrame::OnMouseMoved)
EVT_LEFT_DOWN(OFIQDemoFrame::OnMouseClick)
EVT_LEFT_UP(OFIQDemoFrame::OnMouseClick)
EVT_RIGHT_DOWN(OFIQDemoFrame::OnMouseClick)
EVT_RIGHT_UP(OFIQDemoFrame::OnMouseClick)
EVT_MOUSEWHEEL(OFIQDemoFrame::OnMouseWheel)
END_EVENT_TABLE()

enum
{
    ID_LoadImage = 1,
    ID_CameraChoice,
    ID_StartCapture,
    ID_Capture,
    ID_CancelCapture,
    ID_PreviewTimer,
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
    ID_ZoomIn,
    ID_ZoomOut,
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
    m_zoomRedrawPending = false;

    m_showOriginal = true;
    m_showFaces = true;
    m_showLandmarks = false;
    m_showSegmentationMask = false;
    m_showOcclusionMask = false;
    m_showLandmarkedRegion = false;
    m_visualizationSeparatorItem = nullptr;

    wxMenu* menuView = new wxMenu();
    m_menuViewPtr = menuView;
    wxMenu* menuZoom = new wxMenu();
    menuZoom->Append(ID_ZoomIn, "Zoom In\tCtrl++");
    menuZoom->Append(ID_ZoomOut, "Zoom Out\tCtrl+-");
    menuZoom->AppendSeparator();
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

    // The "Zoom In/Out" menu items carry the Ctrl++/Ctrl+- accelerators for the
    // main keys. Cover the numpad +/- as well via an explicit accelerator table
    // (menu-item accelerator text cannot express the numpad keys). All are
    // focus-independent, unlike the old key-state tracking.
    wxAcceleratorEntry acceleratorEntries[2];
    acceleratorEntries[0].Set(wxACCEL_CTRL, WXK_NUMPAD_ADD, ID_ZoomIn);
    acceleratorEntries[1].Set(wxACCEL_CTRL, WXK_NUMPAD_SUBTRACT, ID_ZoomOut);
    SetAcceleratorTable(wxAcceleratorTable(2, acceleratorEntries));

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

    Bind(wxEVT_MENU, &OFIQDemoFrame::OnZoomIn, this, ID_ZoomIn);
    Bind(wxEVT_MENU, &OFIQDemoFrame::OnZoomOut, this, ID_ZoomOut);
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

    Bind(wxEVT_BUTTON, &OFIQDemoFrame::OnStartCapture, this, ID_StartCapture);
    Bind(wxEVT_BUTTON, &OFIQDemoFrame::OnCaptureFrame, this, ID_Capture);
    Bind(wxEVT_BUTTON, &OFIQDemoFrame::OnCancelCapture, this, ID_CancelCapture);
    Bind(wxEVT_TIMER, &OFIQDemoFrame::OnPreviewTick, this, ID_PreviewTimer);
    Bind(wxEVT_CLOSE_WINDOW, &OFIQDemoFrame::OnClose, this);

    m_captureMode = false;
    m_previewTimer.SetOwner(this, ID_PreviewTimer);

    m_ofiqConfigPath = "ofiq_config.jaxn";
    m_ofiqInitialized = false;
    m_operationInProgress = false;

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

    // Create the left panel: picture frame on top, then the webcam camera
    // selector, then the capture button row.
    auto leftPanel = new wxPanel(verticalSplitterWindow, wxID_ANY);
    m_pictureFramePtr = new OFIQPictureFrame();
    m_pictureFramePtr->Create(leftPanel);
    m_imageLoaded = false;

    m_cameraChoicePtr = new wxChoice(leftPanel, ID_CameraChoice);

    auto captureButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    m_startCaptureButtonPtr = new wxButton(leftPanel, ID_StartCapture, "Capture from Webcam");
    m_captureButtonPtr = new wxButton(leftPanel, ID_Capture, "Capture");
    m_cancelCaptureButtonPtr = new wxButton(leftPanel, ID_CancelCapture, "Cancel");
    captureButtonSizer->Add(m_startCaptureButtonPtr, 0, wxALL, 4);
    captureButtonSizer->Add(m_captureButtonPtr, 0, wxALL, 4);
    captureButtonSizer->Add(m_cancelCaptureButtonPtr, 0, wxALL, 4);

    auto leftPanelSizer = new wxBoxSizer(wxVERTICAL);
    leftPanelSizer->Add(m_pictureFramePtr, 1, wxEXPAND);
    leftPanelSizer->Add(m_cameraChoicePtr, 0, wxEXPAND | wxALL, 4);
    leftPanelSizer->Add(captureButtonSizer, 0, wxALIGN_CENTER);
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

    // Populate the camera dropdown and set the initial button states. Done last
    // because EnumerateCameraDevices() logs into m_logOutputPtr, which must
    // already exist.
    EnumerateCameraDevices();
    UpdateCaptureControls();

    // Log the OFIQ library version at startup. getVersion() returns compile-time
    // constants and needs no initialized interface, so a throwaway implementation
    // instance obtained via getImplementation() is enough (no config, no init).
    int major = 0;
    int minor = 0;
    int patch = 0;
    OFIQ::Interface::getImplementation()->getVersion(major, minor, patch);
    LOG_INFO(
        "OFIQ library version " + std::to_string(major) + "." + std::to_string(minor) + "." +
        std::to_string(patch));

    // Initialize OFIQ once at startup so the View-menu visualization items can be
    // built from the measures' visualization capability. Deferred via CallAfter
    // so the main window is shown first; initialization then runs as a normal
    // operation (busy cursor, worker thread) and builds the menu on completion
    // (see OfiqInitOperation::Finish). A later re-initialization rebuilds it.
    //
    // Only attempt this when the configured file actually exists: a missing
    // config at startup is not an error here (the user has not requested an
    // initialization yet), so it is skipped silently. An explicit Init or an
    // assessment still reports a missing config as before.
    CallAfter([this]()
    {
        if (std::filesystem::is_regular_file(m_ofiqConfigPath))
        {
            RunOperation(std::make_unique<OfiqInitOperation>(*this));
        }
        else
        {
            LOG_INFO("OFIQ config file not found at startup; skipping initial "
                     "initialization. Specify a config file and initialize via the "
                     "OFIQ menu.");
        }
    });
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
    if (event.ControlDown())
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

void OFIQDemoFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void OFIQDemoFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("This is a demonstrator for the Open Face Image Quality (OFIQ) library",
        "About OFIQ Demonstrator", wxOK | wxICON_INFORMATION);
}

void OFIQDemoFrame::ZoomIn()
{
    wxCommandEvent empty;
    OnZoomIn(empty);
}

void OFIQDemoFrame::ZoomOut()
{
    wxCommandEvent empty;
    OnZoomOut(empty);
}

void OFIQDemoFrame::OnZoomIn(wxCommandEvent& event)
{
    double newScale = ClampScaleFactor(m_scaleFactor * m_zoomFactor);
    if (newScale != m_scaleFactor)
    {
        m_scaleFactor = newScale;
        RequestZoomRedraw();
    }
}

void OFIQDemoFrame::OnZoomOut(wxCommandEvent& event)
{
    double newScale = ClampScaleFactor(m_scaleFactor / m_zoomFactor);
    if (newScale != m_scaleFactor)
    {
        m_scaleFactor = newScale;
        RequestZoomRedraw();
    }
}

// Clamps the scale factor so the scaled image's longest edge stays within
// [MIN_SCALED_DIMENSION, MAX_SCALED_DIMENSION]. 100% (scale 1.0) is always
// permitted, even for images larger/smaller than those bounds.
double OFIQDemoFrame::ClampScaleFactor(double scale) const
{
    int longestEdge = m_ofiqImage.width > m_ofiqImage.height
        ? m_ofiqImage.width : m_ofiqImage.height;
    if (longestEdge <= 0)
    {
        return scale;
    }

    double maxScale = static_cast<double>(MAX_SCALED_DIMENSION) / longestEdge;
    double minScale = static_cast<double>(MIN_SCALED_DIMENSION) / longestEdge;
    if (maxScale < 1.0) maxScale = 1.0;
    if (minScale > 1.0) minScale = 1.0;

    if (scale > maxScale) return maxScale;
    if (scale < minScale) return minScale;
    return scale;
}

// Coalesces the (expensive) redraw: while one is already queued, further zoom
// steps only accumulate m_scaleFactor. The queued redraw runs once the current
// batch of zoom events has drained, so a held Ctrl++ does not pile up redraws
// that keep firing after the key is released.
void OFIQDemoFrame::RequestZoomRedraw()
{
    if (m_zoomRedrawPending)
    {
        return;
    }
    m_zoomRedrawPending = true;
    CallAfter([this]()
    {
        m_zoomRedrawPending = false;
        DoUpdateImage();
    });
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

void OFIQDemoFrame::OnShowVisualization(wxCommandEvent& event)
{
    auto it = m_visualizationMenuIds.find(event.GetId());
    if (it != m_visualizationMenuIds.end())
    {
        m_showVisualization[it->second] = event.IsChecked();
        DoUpdateImage();
    }
}

std::set<OFIQ::QualityMeasure> OFIQDemoFrame::RequestedVisualizations() const
{
    // Request a visualization for every measure that implements one. OFIQ
    // ignores measures that do not, so passing them is harmless; requesting all
    // of them keeps the visualizations available for instant toggling in the
    // View menu without re-running the assessment.
    std::set<OFIQ::QualityMeasure> measures;
    if (!m_ofiqPtr)
    {
        return measures;
    }
    for (const auto& [id, name] : measurementMapping)
    {
        auto measure = static_cast<OFIQ::QualityMeasure>(id);
        if (m_ofiqPtr->ImplementsVisualization(measure))
        {
            measures.insert(measure);
        }
    }
    return measures;
}

void OFIQDemoFrame::RefreshVisualizationMenu()
{
    // Remove the visualization items created by a previous initialization so the
    // menu is rebuilt from scratch for the currently initialized configuration.
    for (const auto& [itemId, measure] : m_visualizationMenuIds)
    {
        Unbind(wxEVT_MENU, &OFIQDemoFrame::OnShowVisualization, this, itemId);
        m_menuViewPtr->Destroy(itemId);
    }
    m_visualizationMenuIds.clear();
    m_showVisualization.clear();
    if (m_visualizationSeparatorItem != nullptr)
    {
        m_menuViewPtr->Destroy(m_visualizationSeparatorItem);
        m_visualizationSeparatorItem = nullptr;
    }

    if (!m_ofiqPtr)
    {
        return;
    }

    for (const auto& [id, name] : measurementMapping)
    {
        auto measure = static_cast<OFIQ::QualityMeasure>(id);
        if (!m_ofiqPtr->ImplementsVisualization(measure))
        {
            continue;
        }

        if (m_visualizationSeparatorItem == nullptr)
        {
            m_visualizationSeparatorItem = m_menuViewPtr->AppendSeparator();
        }

        int itemId = wxWindow::NewControlId();
        wxMenuItem* item = new wxMenuItem(
            m_menuViewPtr, itemId, "Show " + name + " Visualization", "", wxITEM_CHECK);
        m_menuViewPtr->Append(item);
        item->Check(false);

        m_showVisualization[measure] = false;
        m_visualizationMenuIds[itemId] = measure;
        Bind(wxEVT_MENU, &OFIQDemoFrame::OnShowVisualization, this, itemId);
    }
}

void OFIQDemoFrame::OnLoadImage(wxCommandEvent& event)
{
    // Loading a file and a running live preview are mutually exclusive: stop the
    // webcam first so its capture thread does not keep overwriting the display.
    if (m_captureMode)
    {
        StopCapture();
    }

    if (m_imageFileDialogPtr->ShowModal() == wxID_CANCEL)
    {
        return;
    }
    else
    {
        wxBusyCursor wait;
        std::string image_path = m_imageFileDialogPtr->GetPath().ToStdString();
        DoLoadImage(image_path);
    }
}

void OFIQDemoFrame::OnStartCapture(wxCommandEvent& event)
{
    int selection = m_cameraChoicePtr->GetSelection();
    if (selection == wxNOT_FOUND ||
        selection >= static_cast<int>(m_cameraDevices.size()))
    {
        LOG_ERROR("No camera selected.");
        return;
    }

    const CameraDevice& device = m_cameraDevices[selection];
    LOG_INFO("Opening webcam '" + device.name + "' ...");
    m_activeCameraName = device.name;

    // Defensive: any previous session should already have been detached.
    if (m_captureThread.joinable())
    {
        m_captureThread.detach();
    }

    // The capture thread opens and reads the camera on its own; the GUI never
    // waits on it. The preview timer only displays whatever the thread buffered.
    m_stream = std::make_shared<WebcamStream>();
    m_captureMode = true;
    UpdateCaptureControls();

    m_captureThread = std::thread(CaptureThreadMain, m_stream, device.index, PreferredCaptureBackend());
    m_previewTimer.Start(33); // ~30 fps display refresh (no camera I/O here)
}

void OFIQDemoFrame::OnCaptureFrame(wxCommandEvent& event)
{
    if (!m_captureMode)
    {
        return;
    }
    if (m_lastFrame.empty())
    {
        LOG_ERROR("No frame available yet.");
        return;
    }

    cv::Mat frame = m_lastFrame.clone();
    StopCapture();

    wxBusyCursor wait;
    DoCaptureImage(frame);
}

void OFIQDemoFrame::OnCancelCapture(wxCommandEvent& event)
{
    if (!m_captureMode)
    {
        return;
    }

    LOG_INFO("Webcam capture cancelled.");
    StopCapture();

    // Restore the previously loaded still image; otherwise clear the view.
    if (m_imageLoaded)
    {
        DoUpdateImage();
    }
    else
    {
        wxImage blank(1, 1);
        m_pictureFramePtr->LoadImage(blank);
    }
}

int OFIQDemoFrame::PreferredCaptureBackend() const
{
    // Match how the cameras were enumerated (see OFIQCameraEnumeration.h):
    // DirectShow on Windows, V4L2 on Linux; avoids the slow default (MSMF).
#if defined(_WIN32)
    return cv::CAP_DSHOW;
#elif defined(__linux__)
    return cv::CAP_V4L2;
#else
    return cv::CAP_ANY;
#endif
}

// GUI thread: display the latest frame the capture thread has buffered. This
// does NO camera I/O, so it can never block the GUI.
void OFIQDemoFrame::OnPreviewTick(wxTimerEvent& event)
{
    if (!m_stream)
    {
        return;
    }
    if (m_stream->failed)
    {
        LOG_ERROR("Could not open camera '" + m_activeCameraName + "'.");
        StopCapture();
        return;
    }

    cv::Mat frame;
    {
        std::lock_guard<std::mutex> lock(m_stream->mutex);
        if (!m_stream->hasFrame)
        {
            return;
        }
        frame = m_stream->latestFrame.clone();
    }

    m_lastFrame = frame; // used by OnCaptureFrame ("Capture")
    ShowLiveFrame(frame);
}

void OFIQDemoFrame::OnClose(wxCloseEvent& event)
{
    StopCapture();
    if (m_operationThread.joinable())
    {
        m_operationThread.join();
    }
    event.Skip(); // continue with the normal close/destroy
}

void OFIQDemoFrame::StopCapture()
{
    if (m_previewTimer.IsRunning())
    {
        m_previewTimer.Stop();
    }

    // Never block the GUI on a possibly-hung camera: signal the thread and
    // detach it. The thread keeps its own shared_ptr to the stream, so it can
    // finish and release the camera on its own after read()/open() returns.
    if (m_stream)
    {
        m_stream->stop = true;
    }
    if (m_captureThread.joinable())
    {
        m_captureThread.detach();
    }
    m_stream.reset();

    m_captureMode = false;
    UpdateCaptureControls();
}

void OFIQDemoFrame::EnumerateCameraDevices()
{
    m_cameraDevices = EnumerateCameras();

    m_cameraChoicePtr->Clear();
    for (const auto& device : m_cameraDevices)
    {
        m_cameraChoicePtr->Append(device.name);
    }
    if (!m_cameraDevices.empty())
    {
        m_cameraChoicePtr->SetSelection(0);
    }

    LOG_INFO("Found " + std::to_string(m_cameraDevices.size()) + " camera(s).");
}

void OFIQDemoFrame::UpdateCaptureControls()
{
    bool hasCamera = !m_cameraDevices.empty();

    // Dropdown and "Capture from Webcam" only make sense when a camera is
    // available and we are not already streaming; "Capture"/"Cancel" only
    // while the capture mode is active.
    m_cameraChoicePtr->Enable(hasCamera && !m_captureMode);
    m_startCaptureButtonPtr->Enable(hasCamera && !m_captureMode);
    m_captureButtonPtr->Enable(m_captureMode);
    m_cancelCaptureButtonPtr->Enable(m_captureMode);
}

void OFIQDemoFrame::ShowLiveFrame(const cv::Mat& frame)
{
    if (frame.empty())
    {
        return;
    }

    cv::Mat bgr;
    if (frame.channels() == 1)
    {
        cv::cvtColor(frame, bgr, cv::COLOR_GRAY2BGR);
    }
    else if (frame.channels() == 4)
    {
        cv::cvtColor(frame, bgr, cv::COLOR_BGRA2BGR);
    }
    else
    {
        bgr = frame;
    }

    wxImage image(bgr.cols, bgr.rows);
    cv::Mat rgb(bgr.rows, bgr.cols, CV_8UC3, image.GetData());
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

    // Fit the live frame into the picture frame's current client area.
    double scale = 1.0;
    auto size = m_pictureFramePtr->GetClientSize();
    if (size.x > 0 && size.y > 0)
    {
        double horizontalScale = static_cast<double>(size.x) / bgr.cols;
        double verticalScale = static_cast<double>(size.y) / bgr.rows;
        scale = horizontalScale < verticalScale ? horizontalScale : verticalScale;
    }

    m_pictureFramePtr->LoadImage(image, scale);
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
        std::string image_path = m_imageSaveFileDialogPtr->GetPath().ToStdString();
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
        std::string csv_path = m_csvSaveFileDialogPtr->GetPath().ToStdString();
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

void OFIQDemoFrame::RunOperation(std::unique_ptr<OFIQDemoOperation> operation)
{
    if (m_operationInProgress)
    {
        return;
    }

    // GUI-thread pre-step; may abort before any work is done.
    if (!operation->Init())
    {
        return;
    }

    m_operationInProgress = true;
    LOG_INFO(operation->Title().ToStdString() + " ...");

    // Wait cursor + disable all windows/controls. wxWindowDisabler blocks user
    // input everywhere but keeps the GUI repainting, so it does not freeze.
    wxBeginBusyCursor();
    m_windowDisabler = std::make_unique<wxWindowDisabler>();

    // shared_ptr so both the worker lambda and the completion lambda keep the
    // operation alive until Finish() has run.
    std::shared_ptr<OFIQDemoOperation> shared = std::move(operation);
    m_operationThread = std::thread([this, shared]()
    {
        // Worker thread: no GUI access here.
        std::string error;
        bool success = shared->Do(error);

        // Marshal completion back to the GUI thread.
        CallAfter([this, shared, success, error]()
        {
            if (m_operationThread.joinable())
            {
                m_operationThread.join();
            }
            m_windowDisabler.reset();
            wxEndBusyCursor();

            shared->Finish(success, error);
            m_operationInProgress = false;
        });
    });
}

void OFIQDemoFrame::OnOfiqInit(wxCommandEvent& event)
{
    RunOperation(std::make_unique<OfiqInitOperation>(*this));
}

void OFIQDemoFrame::OnOfiqAssess(wxCommandEvent& event)
{
    RunOperation(std::make_unique<OfiqAssessOperation>(*this));
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

bool OFIQDemoFrame::DoCaptureImage(const cv::Mat& frame)
{
    LOG_INFO("Capturing image from webcam ...");

    if (frame.empty())
    {
        LOG_ERROR("Captured frame is empty.");
        return false;
    }

    // OFIQ::Image with depth 24 expects raster-scanned BGR data, which is
    // exactly what cv::VideoCapture delivers. Normalize to 3-channel BGR and
    // copy into a fresh buffer owned by the OFIQ image.
    cv::Mat bgr;
    if (frame.channels() == 1)
    {
        cv::cvtColor(frame, bgr, cv::COLOR_GRAY2BGR);
    }
    else if (frame.channels() == 4)
    {
        cv::cvtColor(frame, bgr, cv::COLOR_BGRA2BGR);
    }
    else
    {
        bgr = frame;
    }
    if (!bgr.isContinuous())
    {
        bgr = bgr.clone();
    }

    const uint16_t width = static_cast<uint16_t>(bgr.cols);
    const uint16_t height = static_cast<uint16_t>(bgr.rows);
    const uint8_t depth = 24;

    size_t size = static_cast<size_t>(width) * height * (depth / 8);
    std::shared_ptr<uint8_t[]> data(new uint8_t[size]);
    memcpy(data.get(), bgr.data, size);

    m_ofiqImage = OFIQ::Image(width, height, depth, data);

    this->m_imagePath = "webcam_capture";

    DoClearAssessmentTable();
    DoClearPreprocessing();

    DoUpdatePreferredScalingFactor();
    DoInitImage();

    LOG_INFO("Image captured.");

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

void OFIQDemoFrame::UpdateVisualizations()
{
    const int height = m_cvImage.rows;
    const int width = m_cvImage.cols;
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);

    for (const auto& [measure, argbImage] : m_visualizations)
    {
        auto shown = m_showVisualization.find(measure);
        if (shown == m_showVisualization.end() || !shown->second)
        {
            continue;
        }

        // The visualization is expected to be a row-major ARGB image (one
        // 0xAARRGGBB value per pixel) matching the input image's dimensions. If
        // its size does not match, it cannot be painted onto the display image,
        // so it is skipped rather than risking an out-of-bounds access.
        if (argbImage.size() != expected)
        {
            continue;
        }

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                uint32_t pixel = argbImage[static_cast<size_t>(y) * width + x];
                double alpha = ((pixel >> 24) & 0xFF) / 255.0;
                if (alpha <= 0.0)
                {
                    continue;
                }
                uint8_t r = (pixel >> 16) & 0xFF;
                uint8_t g = (pixel >> 8) & 0xFF;
                uint8_t b = pixel & 0xFF;

                // m_cvImage is BGR: channel 0 = B, 1 = G, 2 = R.
                cv::Vec3b& dst = m_cvImage.at<cv::Vec3b>(y, x);
                dst[0] = static_cast<uint8_t>((1.0 - alpha) * dst[0] + alpha * b);
                dst[1] = static_cast<uint8_t>((1.0 - alpha) * dst[1] + alpha * g);
                dst[2] = static_cast<uint8_t>((1.0 - alpha) * dst[2] + alpha * r);
            }
        }
    }
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
    UpdateVisualizations();
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
    m_visualizations.clear();
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
